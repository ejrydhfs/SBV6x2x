/*
 *
 * qos_classifier.c
 * Description:
 * QOS Classifiers implementation
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/*! \file qos_classifier.c
*   \brief QOS Classifiers implementation
*/
/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/list.h>
#include <linux/config.h>
#include <net/llc_pdu.h>
#include <net/ipv6.h>
#include "pal.h"
#include "qos_classifier.h"
#include "dfltr_class_utils.h"
#include "dfltr_class_ctrl.h"
#include "docsis_mng.h"
#include "us_phs_verify.h"

#ifdef DCLASS_DEBUG
/* note: prsints function name for you */
#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#pragma pack(1)
struct CM_mng_t 
{
    unsigned char da[6];
    unsigned char sa[6];
    unsigned short len;
    unsigned char dsap;
    unsigned char ssap;
    unsigned char control;
    unsigned char version;
    unsigned char type;
    unsigned char reserved;
    unsigned char data[];
};
#pragma pack()

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
extern int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierDataCB)
           (struct sk_buff *skb, int *sfIndex, int *phsIndex));
static void PrintClassData(QosClassifier_t *cl);

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

#define MAX_QOS_CLASSIFIERS 64 /*Max number of QoS classifiers*/
#define MAX_UDC_CLASSIFIERS 64 /*Max number of UDC classifiers*/    

/*Primary SF flow index*/
#define QOS_SF_PRIMARY_SF_INDEX 0

/*802.1Q header parsing macros*/
#define VLAN_TCI_GET_PRIO(X)(((X) & 0xE000 ) >> 13)
#define VLAN_TCI_GET_ID(X)  ((X) & VLAN_VID_MASK)

/*IPv6 header parsing macros*/
#define IPV6VER(h)    ((h->vcf[0]&0xF0)>>2)
#define IPV6CLASS(h)  (((h->vcf[0]&0x0F)<<4) | ((h->vcf[1]&0xF0)>>4))
#define IPV6FLABEL(h) (((h->vcf[1]&0x0F)<<16) | (h->vcf[2]<<8) | (h->vcf[3]))

/*Upstream Drop Classifier points to NULL Service Flow*/
#define IS_UPSTREAM_DROP_CLASSIFIER(cl) ((cl)->sfID == 0)

/*! \var    typedef ClassParamMatch_e
    \brief  classifier parameter match status for packet
*/
typedef enum
{
    CL_PARAM_NOMATCH,
    CL_PARAM_MATCH,
    CL_PARAM_UNRELEVANT,
}ClassParamMatch_e;

/*! \var    typedef struct ClassListEntry_t
    \brief  entry in classifiers list type
*/
typedef struct _ipFilterListEntry
{
    struct list_head list;
    QosClassifier_t  cl;
}ClassListEntry_t;

/*! \var    typedef struct _macAddrBytes
    \brief  integer view of MAC address
*/
#pragma pack(1)
typedef struct _macAddrBytes
{
    Uint32 mac4MSB; /*4 most significant bytes of MAC address*/
    Uint16 mac2LSB; /*2 least significant bytes of MAC address*/
}MacAddrBytes_t;
#pragma pack()

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static LIST_HEAD(classListHead);      /*Classifiers list (contains both QoS and UDC)*/
static Uint32    numUdcClassifiers;
static Uint32    numQoSClassifiers;
/**************************************************************************/
/*      LOCAL PROTOTYPES:                                                 */
/**************************************************************************/
static int AddClassifier(QosClassifier_t *newClass);
static int DelClassifier(Uint16 classId, Uint32 sfId);
static int DelAllClassifiersOfSf(Uint32 sfId);
static int UpdateClassifier(QosClassifier_t *cl);
static int GetClassifier(Uint16 classID, Uint32 sfId, QosClassifier_t *buf);
static int GetClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches);
static int GetPpClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches);
static int PrintPpClassMatches(void);
static int CreateNewClassListEntry(ClassListEntry_t **entry, QosClassifier_t *cl);
static int GetClassListEntryByIndex(Uint16 clID, Uint32 sfId, ClassListEntry_t **entry);
static ClassParamMatch_e MatchByLlc(struct ethhdr *ethHeader, Uint8 classEthType,
                                    Uint16 encProto, QosClassifier_t     *cl);
static ClassParamMatch_e MatchByVlan(Uint16 ethEncapsProto, Uint16 vlanPrio, 
                                     Uint16 vlanId, QosClassifier_t  *cl);
static ClassParamMatch_e MatchByIpv4(struct iphdr  *ipHdr, Bool udpTcpHdrPresent, Uint8 ipIpLayerProto, QosClassifier_t *cl);
static ClassParamMatch_e MatchByIpv6(struct Ipv6hdr_s  *ipHdr, Bool udpTcpHdrPresent, Uint8 ipUpLayerProto, QosClassifier_t *cl);
static ClassParamMatch_e MatchByUdpTcp(struct udphdr *udpTcpHdr, QosClassifier_t *cl);
static void ParseVlanTCI(struct vlan_hdr *vlanHdr, Uint16 *prio, Uint16 *id);
static void SetClassEntryData(ClassListEntry_t *entry, QosClassifier_t *cl);

/**************************************************************************/
/*      INTERFACE FUNCTIONS                                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int QosClass_ClassifyData(struct sk_buff *skb, Uint32 srcIfIndex, Int16 *sfIndex, Int16 *phsIndex)
 **************************************************************************
 *  \brief Classify MAC data packet
 *  \param[in] Uint8 *pkt - pointer to packet skb
 *  \param[in] Uint32 srcIfIndex - source interface index
 *  \param[out] Int16 *sfIndex - SF index
 *  \param[out] Int16 *phsIndex - PHS index
 *  \return  0 or error code
 */
int QosClass_ClassifyData(struct sk_buff *skb, int *sfIndex, int *phsIndex)
{
    struct list_head *pos;
    ClassListEntry_t *entry;
    QosClassifier_t  *cl;
    struct ethhdr *ethHeader;
    struct vlan_hdr *vlanHdr;
    void  *ipHdr;
    Uint8 *ipUpLayerHdr, *ethData;
    EthEncapsType_e ethEncapsType; /*Ethernet encapsulation type*/
    Uint8 ipUpLayerProto;
    Uint16 ethEncapsProto, vlanPrio, vlanId;
    Uint32 classEthType; /*Ethernet encapsulation type in classifier terms*/
    ClassParamMatch_e llcMatch, vlanMatch, ipv4Match, ipv6Match, udpTcpMatch,
                      cmimMatch, clMatch;
    Bool vlanHdrPresent, ipv4HdrPresent, ipv6HdrPresent, udpTcpHdrPresent;
    Uint32 lockKey, srcIfIndex;

    DPRINTK(KERN_DEBUG"%s -Start\n",__FUNCTION__);

    *sfIndex = QOS_SF_PRIMARY_SF_INDEX;
    *phsIndex = 0xFF;
    clMatch = False;
    cl = NULL;

    /*If no active classifiers don't search*/
    if (!numQoSClassifiers && !numUdcClassifiers)
    {
        DPRINTK(KERN_DEBUG"%s - No active classifiers\n",__FUNCTION__);
        return 0;
    }

    /*Parse packet network headers*/
    vlanHdrPresent = ipv4HdrPresent = ipv6HdrPresent = udpTcpHdrPresent = False;
    ethHeader = eth_hdr(skb);
    ethData = skb->data + ETH_HLEN;
#if 1
    DPRINTK(KERN_DEBUG"eth hdr: hdest=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",ethHeader->h_dest[0],
           ethHeader->h_dest[1],ethHeader->h_dest[2],ethHeader->h_dest[3],ethHeader->h_dest[4],
           ethHeader->h_dest[5]);
    DPRINTK(KERN_DEBUG"eth hdr: hsource=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",ethHeader->h_source[0],
           ethHeader->h_source[1],ethHeader->h_source[2],ethHeader->h_source[3],ethHeader->h_source[4],
           ethHeader->h_source[5]);

    DPRINTK(KERN_DEBUG"h_proto=0x%.4x\n",ethHeader->h_proto);
#endif
    FcUtils_ExtractEthEncapsType(ethHeader, ethData, &ethEncapsType, &ethEncapsProto, &vlanHdrPresent);
    DPRINTK(KERN_DEBUG"%s: ethEncapsType=%u, ethEncapsProto=0x%x, vlanHdrPresent=%u, skb data= \n",__FUNCTION__,
           ethEncapsType, ethEncapsProto, vlanHdrPresent);
#if 0
    { 
        int i;
        for (i=0;i<30;i++) {
            DPRINTK(KERN_DEBUG"%.2X ",ethData[i]);
        }
    }
    DPRINTK(KERN_DEBUG"\n");
#endif
    if (FcUtils_ExtractNetHdrs(ethData, ethEncapsType, ethEncapsProto, vlanHdrPresent, &ipHdr, &vlanHdr) != 0)
    {
        DPRINTK(KERN_INFO"%s: Unsupported packet format - no matching classifer!\n", __FUNCTION__);
        return 0;
    }
#if 0
    {
        struct iphdr *iph = (struct iphdr *)ipHdr;
        DPRINTK(KERN_DEBUG"Ip header: \n");
        DPRINTK(KERN_DEBUG"ver_len =%.2x, tos=%.2x, protocol=%.2x, saddr=%.8x",
           iph->version,iph->tos,iph->protocol,iph->saddr);
    }
#endif
    FcUtils_ExtractIpUpLayerHdr(ethEncapsProto,ipHdr, &ipUpLayerProto, &ipUpLayerHdr);
    DPRINTK(KERN_DEBUG"%s 0xipUpLayerProto=%.2x ipUpLayerHdr=%p\n",__FUNCTION__, ipUpLayerProto, ipUpLayerHdr);

    /*Translate the type of encapsulated protocol to classifiers terms*/
    if (ethEncapsType == ETH_ENCAP_V2 || ethEncapsType == ETH_ENCAP_802_2_3_SNAP)
    {
        classEthType = FC_LLC_PROTO_ETHERTYPE;
        if(ethEncapsProto == ETH_P_IP) 
            ipv4HdrPresent = True;
        else if (ethEncapsProto == ETH_P_IPV6) 
            ipv6HdrPresent = True;
    }
    else
        classEthType = FC_LLC_PROTO_DSAP;

    /*Parse IEEE 802.1Q header*/
    vlanPrio = vlanId = 0;
    if (vlanHdrPresent) 
    {
        ParseVlanTCI(vlanHdr, &vlanPrio, &vlanId);
    }

    if ((ipv4HdrPresent || ipv6HdrPresent) &&
       ((ipUpLayerProto == IPPROTO_UDP) || (ipUpLayerProto == IPPROTO_TCP)))
        udpTcpHdrPresent=True;

    /*Get source interface index from skb*/
    srcIfIndex = skb->input_dev->ifindex;
    DPRINTK(KERN_DEBUG"%s: packet from source interface index %u\n",__FUNCTION__, srcIfIndex);

    /* go over the classifier table and try to find a match */
    list_for_each(pos, &classListHead)
    {
        entry = list_entry(pos, ClassListEntry_t, list);
        cl = &(entry->cl);

        DPRINTK(KERN_DEBUG"%s Checking classifier %u of SF %u\n",__FUNCTION__, cl->classID, cl->sfID);

        llcMatch = vlanMatch = ipv4Match = ipv6Match = udpTcpMatch = cmimMatch = CL_PARAM_UNRELEVANT;
        
        /* Verify all relevant parameters*/
        if (cl->isLlcCriterion) /*If LLC matching criteria is used*/
        {
            DPRINTK(KERN_DEBUG"%s Using LLC criterion\n",__FUNCTION__);

            llcMatch = MatchByLlc(ethHeader, classEthType, ethEncapsProto, cl);
            if (llcMatch == CL_PARAM_NOMATCH) 
                continue;
            DPRINTK(KERN_DEBUG"Classifier matched by LLC criterion\n");
        }
        if  (cl->isVlanCriterion) 
        {
            DPRINTK(KERN_DEBUG"%s Using VLAN criterion\n",__FUNCTION__);

            /*If VLAN header is present attempt to find a match, otherwise return no match*/
            if (vlanHdrPresent)
                vlanMatch = MatchByVlan(ethEncapsProto, vlanPrio, vlanId, cl);
            else
                vlanMatch = CL_PARAM_NOMATCH;

            if (vlanMatch == CL_PARAM_NOMATCH) 
                continue;

            DPRINTK(KERN_DEBUG"Classifier matched by 802.1P/Q criterion\n");

        }
        /*If IPv4 matching criterion flag is set if at least one IPv4 parameter is present besides TCP/UDP*/ 
        if (cl->isIpv4Criterion)
        {
            DPRINTK(KERN_DEBUG"%s Using ipV4 criterion\n",__FUNCTION__);

            /*If IPv4 header is present attempt to find a match, otherwise return no match*/
            if (ipv4HdrPresent)
                ipv4Match = MatchByIpv4((struct iphdr *)ipHdr, udpTcpHdrPresent, ipUpLayerProto, cl);      
            else
                ipv4Match = CL_PARAM_NOMATCH;

            if (ipv4Match == CL_PARAM_NOMATCH) 
                    continue;

            DPRINTK(KERN_DEBUG"Classifier matched by IPv4 criterion\n");
        }
        else if (cl->isIpv6Criterion)
        {
            DPRINTK(KERN_DEBUG"%s Using ipV6 criterion\n",__FUNCTION__);

            /*If IPv6 header is present attempt to find a match, otherwise return no match*/
            if (ipv6HdrPresent)
                ipv6Match = MatchByIpv6((struct Ipv6hdr_s *)ipHdr, udpTcpHdrPresent, ipUpLayerProto, cl);
            else
                ipv6Match = CL_PARAM_NOMATCH;

            if (ipv6Match == CL_PARAM_NOMATCH) 
                continue;
            DPRINTK(KERN_DEBUG"Classifier matched by IPv6 criterion\n");
        }
        /*Verify UDP/TCP criterion independetly of IP protocol type*/
        /* If all of TCP/UDP parameters are omitted - default values are used which are
        all the possible range so no need to check if none of them specified*/
        if (cl->isTcpUdpCriterion) 
        {
            DPRINTK(KERN_DEBUG"%s Using TCP/UDP criterion\n",__FUNCTION__);

            if (udpTcpHdrPresent)
                udpTcpMatch = MatchByUdpTcp((struct udphdr *)ipUpLayerHdr, cl);
            else
                udpTcpMatch = CL_PARAM_NOMATCH;

            if (udpTcpMatch == CL_PARAM_NOMATCH)
                continue;
            DPRINTK(KERN_DEBUG"Classifier matched by TCP/UDP criterion\n");
        }

        /*CMIM classification*/
        if (cl->isCmimCriterion) 
        {
            DPRINTK(KERN_DEBUG"%s Using CMIM criterion\n",__FUNCTION__);
            if ((0x80000000 >> (srcIfIndex-1)) & cl->cmim) 
                cmimMatch = CL_PARAM_MATCH;
            else
                continue;
            DPRINTK(KERN_DEBUG"Classifier matched by CMIM criterion\n");

        }

        /*If we reached here all the parameters are matching or unrelevant*/
        /*If all the relevant parameters match - the classifier matches*/
        if ((llcMatch == CL_PARAM_MATCH) || (vlanMatch== CL_PARAM_MATCH) || 
            (ipv4Match == CL_PARAM_MATCH) || (ipv6Match==CL_PARAM_MATCH) || 
            (udpTcpMatch == CL_PARAM_MATCH) || (cmimMatch == CL_PARAM_MATCH) )
        {
            clMatch = True;
            break;
        }
    }
    if (clMatch) 
    {
        /*In case of UDC SF Index would be -1 */
        *sfIndex = cl->sfIndex;

        if (!IS_UPSTREAM_DROP_CLASSIFIER(cl))
        {
            DPRINTK(KERN_DEBUG"%s: QoS Classifier %u of SF %u matched, PHS=0x%x\n",__FUNCTION__,cl->classID, cl->sfID, cl->phsIndex);
            /* Check if there is valid PHS rule for this classifier */
            if( cl->phsIndex != 0xFF )
            {
                if (UsPhsVerify(skb, cl->phsIndex) == 0)
                {
                    *phsIndex = cl->phsIndex;
                    DPRINTK(KERN_DEBUG"%s:PHS verify is succefull, PHS index=0x%x!\n",__FUNCTION__, *phsIndex);
                }
            }
        }   
        else
        {
            DPRINTK(KERN_DEBUG"%s: Upstream Drop Classifier %u matched\n",__FUNCTION__,cl->classID);
        }

        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
        cl->pktCounter++;
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
        if (skb->pp_packet_info.ti_pp_flags & TI_PPM_SESSION_INGRESS_RECORDED)
        {
            (skb->pp_packet_info.ti_match_qos_classifier) = cl;
            DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_qos_classifier = %p\n", cl);
        }
#endif
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    }

    DPRINTK(KERN_DEBUG"%s - Finished, sfIndex=%d, phsIndex=%d\n",__FUNCTION__,
           *sfIndex, *phsIndex);
    
    return 0;
}

/**************************************************************************/
/*! \fn int QosClass_ClassifyMacMng(Uint8 *buf, Int16 *sfIndex, Int16 *phsIndex) 
 **************************************************************************
 *  \brief Classify MAC management packet
 *  \param[in] Uint8 *buf - pointer to packet buffer
 *  \param[out] Int16 *sfIndex - SF index
 *  \param[out] Int16 *phsIndex - PHS index
 *  \return  0 or error code
 */
int QosClass_ClassifyMacMng(Uint8 *buf, Int16 *sfIndex, Int16 *phsIndex) 
{
   struct list_head *pos;
   ClassListEntry_t *entry;
   QosClassifier_t   *cl;
   Bool clMatch;
   struct CM_mng_t *macHdr;
   Uint32 lockKey;
   Uint8 prot1 = 0, prot2 = 0xFF;


   DPRINTK(KERN_DEBUG"%s - Start - \n",__FUNCTION__);

   *sfIndex = QOS_SF_PRIMARY_SF_INDEX;
   *phsIndex = -1;
   clMatch = False;

   macHdr = (struct CM_mng_t *)buf;

   /* go over the classifier table and try to find a match */
   list_for_each(pos, &classListHead)
   {
       entry = list_entry(pos, ClassListEntry_t, list);
       cl = &(entry->cl);

       if (!cl->isLlcCriterion) /*If LLC matching criteria is used*/
           continue;
        
       DPRINTK(KERN_DEBUG"%s Checking LLC classifier: enetProtocolType=%d \n",__FUNCTION__, cl->llc.enetProtocolType);

       if(cl->llc.enetProtocolType != FC_LLC_PROTO_MAC) 
           continue;

       /* MAC Msg - test type range */
       prot1 = (Uint8)(cl->llc.enetProtocol>>8);
       prot2 = (Uint8)(cl->llc.enetProtocol & 0x00FF);

       DPRINTK(KERN_DEBUG"%s Checking LLC-MAC classifier: MAC type=%d Prot1=%d  Prot2=%d  \n", 
              __FUNCTION__, macHdr->type, prot1, prot2);


       if((macHdr->type < prot1) || (macHdr->type > prot2))
       {
          continue; /* type out of range */
       }
       /* Classification succeeded - we have a match ! */
       clMatch = True;
       DPRINTK(KERN_DEBUG"%s Packet matches to LLC-MAC criteria; ClsfID=%d  SFID=0x%x  sfIndex=%d  phsIndex=%d \n",
              __FUNCTION__, cl->classID, cl->sfID, cl->sfIndex, cl->phsIndex);
       break;
   }

   if (clMatch) 
   {
      if (!IS_UPSTREAM_DROP_CLASSIFIER(cl))
      {
          DPRINTK(KERN_DEBUG"%s: QoS Classifier %u of SF %u matched\n",__FUNCTION__,cl->classID, cl->sfID);
      }
      else
      {
          DPRINTK(KERN_DEBUG"%s: Upstream Drop Classifier %u matched\n",__FUNCTION__,cl->classID);
      }
   
      /*In case of UDC both SF and PHS Index would be -1 */
      *sfIndex = cl->sfIndex;
      *phsIndex = cl->phsIndex;

      PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
      cl->pktCounter++;
      PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
   }

   return 0;
}

/**************************************************************************/
/*! \fn void QosClass_Init(void)
 **************************************************************************
 *  \brief Init classifiers module
 *  \return  NA
 */
void QosClass_Init(void)
{
    numQoSClassifiers = 0;
    numUdcClassifiers = 0;
}

/**************************************************************************/
/*! \fn void QosClass_Cleanup(void)
 **************************************************************************
 *  \brief Cleanup QOS classifiers module
 *  \return  OK
 */
int QosClass_Cleanup(void)
{
    struct list_head *pos;
    ClassListEntry_t *curr=NULL;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /* free classifiers list */
    pos = classListHead.next;
    while(pos != &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);
        pos = pos->next;
        PAL_osMemFree(0, curr, sizeof(ClassListEntry_t));
    }
    numQoSClassifiers = 0;
    numUdcClassifiers = 0;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}



/**************************************************************************/
/*! \fn int QosClass_Start(void)
 **************************************************************************
 *  \brief Start QOS classifiers module operation
 *  \return  OK/NOK
 */
int QosClass_Start(void)
{
    if (numQoSClassifiers || numUdcClassifiers) 
    {
        /*Register Data classifer callback in the Docsis Bridge */
        if (Dbridge_register_QosClassifierDataCB(QosClass_ClassifyData))
            return -1;
    }

    if(HAL_DocsisRegisterQosClassifierMacMgmtCB(QosClass_ClassifyMacMng))
        return -1;

    
    return 0;
}

/**************************************************************************/
/*! \fn int QosClass_Stop(void)
 **************************************************************************
 *  \brief Stop QOS classifiers module operation
 *  \return  OK/NOK
 */
int QosClass_Stop(void)
{
    /*Unregister Data classifer callback in the Docsis Bridge */
    if (Dbridge_register_QosClassifierDataCB(NULL))
        return -1;
    
    return 0;
}

/**************************************************************************/
/*! \fn int QosClass_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
 **************************************************************************
 *  \brief Set Qos Classifier data
 *  \param[in] int subtype - set command sub - type
 *  \param[in]  Uint32 param1 - generic int parameter
 *  \param[in]  Uint32 param2 - generic int parameter
 *  \param[in] void *buf - input data buffer
 *  \return  0 or error code
 */
int QosClass_Set(int subtype, Uint32 param1, Uint32 param2, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case CLIOC_S_SUBTYPE_ADD:
        ret = AddClassifier((QosClassifier_t *)buf);
        break;
    case CLIOC_S_SUBTYPE_DEL:
        ret = DelClassifier(param1, param2);
        break;
    case CLIOC_S_SUBTYPE_UPD:
        ret = UpdateClassifier((QosClassifier_t *)buf);
        break;
    case CLIOC_S_SUBTYPE_PRINT:
        ret = PrintPpClassMatches();
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*! \fn int QosClass_Get(int subtype, Uint32 param1, Uint32 param2, void *buf)
 **************************************************************************
 *  \brief Get Qos Classifier data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] Uint32 param1 - generic input parameter
 *  \param[in] Uint32 param2 - generic input parameter
 *  \param[out] void *buf - output data buffer
 *  \return  0 or error code
 */
int QosClass_Get(int subtype, Uint32 param1, Uint32 param2, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case CLIOC_G_SUBTYPE_CLASS:
        ret = GetClassifier(param1, param2, (QosClassifier_t *)buf);
        break;
    case CLIOC_G_SUBTYPE_MATCHES:
        ret = GetClassMatches(param1, param2, (Uint64 *)buf);
        break;
    case CLIOC_G_SUBTYPE_PP_MATCHES:
        ret = GetPpClassMatches(param1, param2, (Uint64 *)buf);
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS                                                   */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int AddClassifier(QosClassifier_t *newClass)
 **************************************************************************
 *  \brief Add new Classifer to the classifers list
 *  \param[in] QosClassifier_t *newClass - classifier data
 *  \return  OK/NOK
 */
static int AddClassifier(QosClassifier_t *newClass)
{
    Uint32 lockKey;
    struct list_head *pos;
    ClassListEntry_t *curr=NULL, *new;

    DPRINTK(KERN_DEBUG"%s: class ID = %u, SF = %u\n",__FUNCTION__, newClass->classID, newClass->sfID);

    if (IS_UPSTREAM_DROP_CLASSIFIER(newClass)) 
    {
        if (numUdcClassifiers == MAX_UDC_CLASSIFIERS) {
            DPRINTK(KERN_WARNING"Failed to add UDC classifier - the classifer table is full\n");
            return -EINVAL;
        }
    }
    else
    {
        if (numQoSClassifiers == MAX_QOS_CLASSIFIERS) {
            DPRINTK(KERN_WARNING"Failed to add QOS classifier - the classifer table is full\n");
            return -EINVAL;
        }
    }

    /* find a place in the list according to the index */
    list_for_each(pos, &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);
        if (curr->cl.priority <= newClass->priority) 
            break;
    }

    /*Create new list entry*/
    if (CreateNewClassListEntry(&new, newClass)){
        DPRINTK(KERN_ERR"Failed to add Classifer - failed to create entry in the list\n");
        return -EINVAL;
    }

    /* insert before the current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_add(&new->list, pos->prev);
    if (IS_UPSTREAM_DROP_CLASSIFIER(newClass)) 
        numUdcClassifiers++;
    else
        numQoSClassifiers++;
    /*If it is the first classifier that is added register the class callback in the bridge*/
    if (numUdcClassifiers + numQoSClassifiers == 1) 
    {
        if (Dbridge_register_QosClassifierDataCB(QosClass_ClassifyData))
        {
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
            return -EFAULT;
        }
    }
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    DPRINTK(KERN_DEBUG"Successfully added classifier classId=0x%x of SFID=0x%x\n", newClass->classID, newClass->sfID);
    return 0;
}

/**************************************************************************/
/*! \fn static int DelClassifier(Uint16 classId, Uint32 sfId)
 **************************************************************************
 *  \brief Delete a classifier with the given index
 *  \param[in] Uint16 classId - classifier ID
 *  \param[in] Int16 sfId - SF ID
 *  \return  0 or error code
 */
static int DelClassifier(Uint16 classId, Uint32 sfId)
{
    Uint32 lockKey;
    ClassListEntry_t *entry;

    DPRINTK(KERN_DEBUG"%s: class ID = %u  SFID = %u\n",__FUNCTION__, classId, sfId);

    if (!numQoSClassifiers && !numUdcClassifiers) 
    {
        /* The classiifier list is empty; Do nothing. Return OK */
        return 0;
    }

    /* If Clsf ID is 0 - delete all classifiers of the specified Service Flow */
    if(classId == 0)
    {
        DelAllClassifiersOfSf(sfId);
        return 0;
    }

    /* Find the specified classifier and delete it from the list */
    /* If it is not found - do nothing, it is already deleted    */
    if (GetClassListEntryByIndex(classId, sfId, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Classifer to delete is not found in the list,  classId=0x%x SFID=0x%x\n", classId, sfId);
        return 0;
    }
    /* delete current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_del(&entry->list);
    if (IS_UPSTREAM_DROP_CLASSIFIER(&entry->cl))
        numUdcClassifiers --;
    else
        numQoSClassifiers --;
    /*If it was the last classifier unregister the class callback in the bridge*/
    if (!numQoSClassifiers && !numUdcClassifiers) 
    {
        if (Dbridge_register_QosClassifierDataCB(NULL))
            return -EFAULT;
    }
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    PAL_osMemFree(0, entry, sizeof(ClassListEntry_t));

    DPRINTK(KERN_DEBUG"Successfully deleted classifier classId=0x%x of SFID=0x%x\n", classId, sfId);
    
    return 0;
}

/**************************************************************************/
/*! \fn static int DelAllClassOfSf(Uint32 sfId)
 **************************************************************************
 *  \brief Delete all classifiers of the given SFID
 *  \param[in] Int16 sfId - SF ID
 *  \return  0 or error code
 */
static int DelAllClassifiersOfSf(Uint32 sfId)
{
    Uint32 lockKey;
    struct list_head *pos = NULL, *next=NULL;
    ClassListEntry_t *curr=NULL;

    DPRINTK(KERN_DEBUG"%s: SFID = %u\n",__FUNCTION__, sfId);

    /* go over the list and search for classifiers of the specified SF */
    pos = classListHead.next; /* the first element of the list */
    while(pos != &classListHead)
    {
        next = pos->next;  /* keep ptr to the next element in the list */

        curr = list_entry(pos, ClassListEntry_t, list);
        if(curr->cl.sfID == sfId)
        {
            DPRINTK(KERN_DEBUG"%s: Found Classifier for deleting; ClsfID = %u SFID = %u\n",
                   __FUNCTION__, curr->cl.classID  ,sfId);

            /* delete the found entry */
            PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
               list_del(&curr->list);
               numQoSClassifiers --; 
            PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

            PAL_osMemFree(0, curr, sizeof(ClassListEntry_t));
        }

        pos = next;
    } /* End of while */

    /* Always return OK */
    return 0;
}



/**************************************************************************/
/*! \fn static int UpdateClassifier(QosClassifier_t *cl)
 **************************************************************************
 *  \brief Update classifier with the given index
 *  \param[in] QosClassifier_t *cl - classifier data
 *  \return  0 or error code
 */
static int UpdateClassifier(QosClassifier_t *cl)
{
    Uint32 lockKey;
    ClassListEntry_t *entry;

    DPRINTK(KERN_DEBUG"%s: class ID = %u, SF = %u\n",__FUNCTION__, cl->classID, cl->sfID);

    if ((numQoSClassifiers || numUdcClassifiers) && 
        (GetClassListEntryByIndex(cl->classID, cl->sfID, &entry) == 0))
    {
       /* update current entry */
       PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
       SetClassEntryData(entry, cl);
       PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    }
    else
    {
        /* Classifier is not found in the list - add it */
        AddClassifier(cl);
    }

    DPRINTK(KERN_DEBUG"Successfully updated classifier classId=0x%x of SFID=0x%x\n", cl->classID, cl->sfID);
    
    return 0;
}

/**************************************************************************/
/*! \fn static int GetClassifier(Uint16 classID, Uint32 sfId, QosClassifier_t *buf)
 **************************************************************************
 *  \brief Get classifier by index
 *  \param[in] Uint16 classID - classifier ID
 *  \param[in]  Int16 sfId - SF ID
 *  \param[out] QosClassifier_t *buf - output data buffer
 *  \return  0 or error code
 */
static int GetClassifier(Uint16 classID, Uint32 sfId, QosClassifier_t *buf)
{
    ClassListEntry_t *entry;

    if (GetClassListEntryByIndex(classID, sfId, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"%s: Classifer %d, SFID 0x%x is not found\n", __FUNCTION__,classID, sfId);
        return -EINVAL;
    }
    PAL_osMemCopy(buf, &(entry->cl), sizeof(QosClassifier_t));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetClassMatches(Uint16 classID, Int16 sfIndex, Uint64 *matches)
 **************************************************************************
 *  \brief  Get number of matches for a classifier with a given ID
 *  \param[in]  Uint16 classID- classifier id
 *  \param[in]  Int32 sfId - SF ID
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches)
{
    Uint64 dummyPktCounter = 0;
    ClassListEntry_t *entry;


    if (GetClassListEntryByIndex(classID, sfId, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"%s: classifier %d, SFID 0x%x is not found\n", __FUNCTION__, classID, sfId);
        PAL_osMemCopy(matches, &dummyPktCounter, sizeof(Uint64));
        return 0;
    }
    PAL_osMemCopy(matches, &entry->cl.pktCounter, sizeof(Uint64));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetPpClassMatches(Uint16 classID, Int16 sfIndex, Uint64 *matches)
 **************************************************************************
 *  \brief  Get number of packet processor matches for a classifier with a given ID
 *  \param[in]  Uint16 classID- classifier id
 *  \param[in]  Int32 sfId - SF ID
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetPpClassMatches(Uint16 classID, Uint32 sfId, Uint64 *matches)
{
    Uint32 lockKey;
    Uint64 dummyPpPktCounter = 0;
    ClassListEntry_t *entry;
    TI_PP_SESSION_STATS sessionStats;
    Uint64 deadSessionsMatches = 0;
    Uint64 currSessionsMatches = 0;
    DppCountSessionEntry_t *currSessionEntry = NULL;

    memset(&sessionStats, 0, sizeof(sessionStats));

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* get matched from dead sessions */
    if (GetClassListEntryByIndex(classID, sfId, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"%s: classifier %d, SFID 0x%x is not found\n", __FUNCTION__, classID, sfId);
        PAL_osMemCopy(matches, &dummyPpPktCounter, sizeof(Uint64));
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return 0;
    }
    PAL_osMemCopy(&deadSessionsMatches, &entry->cl.ppCounters.ppPktCounter, sizeof(Uint64));

    /* get matched of current sessions */
    if (entry->cl.ppCounters.activeSessionsList != NULL)
    {
        currSessionEntry = entry->cl.ppCounters.activeSessionsList;

        while (currSessionEntry != NULL)
        {
            if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
            {
                DPRINTK(KERN_INFO"Failed to get session %d statistics for ClassID %d and SfID 0x%x\n", 
                        currSessionEntry->sessionHandle, classID, sfId);
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                return -EINVAL;
            }

            currSessionsMatches += sessionStats.packets_forwarded;
            currSessionEntry = currSessionEntry->qosClassInfo.nextSession;
        }
    }

    *matches =  deadSessionsMatches + currSessionsMatches;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	return 0;
}

/**************************************************************************/
/*! \fn static int GetPpClassMatches(Uint16 classID, Int16 sfIndex, Uint64 *matches)
 **************************************************************************
 *  \brief Print classifiers packet processor matches
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 or error code
 */
static int PrintPpClassMatches(void)
{
    Uint64 pktCnt = 0;
    Uint64 ppPktCnt = 0;
    struct list_head *pos = NULL;
    ClassListEntry_t *curr = NULL;

	printk("\n---------------------------------------");
	printk("\n Qos-Classifier match packets counters ");
	printk("\n---------------------------------------");
	printk("\nClassID\t SfID  \tHostPktCount\tPpPktCount");
	printk("\n-------\t-------\t------------\t----------");

    list_for_each(pos, &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);

        GetClassMatches(curr->cl.classID, curr->cl.sfID, &pktCnt);
        GetPpClassMatches(curr->cl.classID, curr->cl.sfID, &ppPktCnt);
        printk("\n0x%x\t0x%x\t%Lu\t\t%Lu", curr->cl.classID, curr->cl.sfID, 
               pktCnt, ppPktCnt);
    }
    
	printk("\n");
	printk("\n");
	return 0;
}

/**************************************************************************/
/*! \fn static int CreateNewClassListEntry(ClassListEntry_t **entry, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Create new entry in the classifiers list
 *  \param[out] ClassListEntry_t **entry - pointer to the allocated entry
 *  \param[in]  QosClassifier_t *cl - classifier data
 *  \return  OK/NOK
 */
static int CreateNewClassListEntry(ClassListEntry_t **entry, QosClassifier_t *cl)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(ClassListEntry_t), 0, (Ptr *)entry);
    if (res != PAL_SOK) {
        DPRINTK(KERN_ERR"%s: Failed to allocate dynamic memory\n",__FUNCTION__);
        return -1;
    }

    SetClassEntryData(*entry, cl);

    return 0;
}

/**************************************************************************/
/*! \fn static int GetClassListEntryByIndex(Uint16 clId, Uint32 sfId, ClassListEntry_t **entry)
 **************************************************************************
 *  \brief Get classifiers list entry by class ID and SF index
 *  \param[in]  Uint16 clID - class ID
 *  \param[in]  Int16 sfId - SF ID
 *  \param[out] ClassListEntry_t **entry - pointer to the found entry
 *  \return  OK/NOK
 */
static int GetClassListEntryByIndex(Uint16 clId, Uint32 sfId, ClassListEntry_t **entry)
{
    struct list_head *pos;
    ClassListEntry_t *curr=NULL;

    /* find a place in the list according to the index */
    list_for_each(pos, &classListHead)
    {
        curr = list_entry(pos, ClassListEntry_t, list);
        if ((curr->cl.classID == clId) && (curr->cl.sfID == sfId)) 
            break;
    }
    if (pos != &classListHead) 
    {
        *entry = curr;
        return 0;
    }
    else
        return -1;
}

/**************************************************************************/
/*! \fn static void ParseVlanTCI(struct vlan_hdr *vlanHdr, Uint16 *prio, Uint16 *id)
 **************************************************************************
 *  \brief Parse the TCI field of VLAN header
 *  \param[in]  struct vlan_hdr *vlanHdr - vlan header 
 *  \param[out] Uint16 *prio - priority
 *  \param[out] Uint16 *id - id
 *  \return  NA
 */
static void ParseVlanTCI(struct vlan_hdr *vlanHdr, Uint16 *prio, Uint16 *id)
{
    *prio = VLAN_TCI_GET_PRIO(ntohs(vlanHdr->h_vlan_TCI));
    *id = VLAN_TCI_GET_ID(ntohs(vlanHdr->h_vlan_TCI));
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByLlc(struct ethhdr *ethHeader, Uint8 classEthType,
                       Uint16 encProto, QosClassifier_t  *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by LLC criterias
 *  \param[in]  struct ethhdr *ethHeader - packet ethernet header
 *  \param[in]  Uint8 classEthType - ethernet type in terms of classifiers
 *  \param[in]  Uint16 encProto - Etherenet encapsulated protocol
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No Match
 */
static ClassParamMatch_e MatchByLlc(struct ethhdr *ethHeader, Uint8 classEthType,
                       Uint16 encProto, QosClassifier_t  *cl)
{
    MacAddrBytes_t *destMac, *srcMac;

   /*If LLC Ethernet protocol type is specified*/
   if (cl->clParamsBitmap & CL_BIT_ETHER_TYPE)
   {
        /* Zero or MAC Mng classifier */
        if ((cl->llc.enetProtocolType == FC_LLC_PROTO_TYPE_NONE) ||
            (cl->llc.enetProtocolType == FC_LLC_PROTO_MAC)) {
            return CL_PARAM_NOMATCH;
        }
        /* Not a catch-all classifier */
        if (cl->llc.enetProtocolType != FC_LLC_PROTO_ALL_PDU) 
        {
            /*Check encapsulation type*/
            if(cl->llc.enetProtocolType != classEthType)
                return CL_PARAM_NOMATCH;

            /*Check encapsulation protocol value*/
            if ( (classEthType == FC_LLC_PROTO_ETHERTYPE) && 
                 (encProto != cl->llc.enetProtocol))
            {
                return CL_PARAM_NOMATCH;/* Ethernet protocol isn't matched */
            }
 
            if ( (classEthType == FC_LLC_PROTO_DSAP) &&
                 (encProto != PAL_UINT16_LOW8(cl->llc.enetProtocol)))
            {
                return CL_PARAM_NOMATCH;/* DSAP byte isn't matched */
            }
        }
   }
   /*If destination MAC is specified*/
   if (cl->clParamsBitmap & CL_BIT_DEST_MAC) 
   {
       /*The structure is packed so there won't be unaligned address exception*/
       destMac = (MacAddrBytes_t *)(ethHeader->h_dest);
       if (((destMac->mac4MSB & cl->llc.dstMacMask4bytes) != cl->llc.dstMacAddr4bytes) ||
           ((destMac->mac2LSB & cl->llc.dstMacMask2bytes) != cl->llc.dstMacAddr2bytes)) {
           return CL_PARAM_NOMATCH; /* MAC addresses do not match */
       }

   }
   /*If source MAC is specified*/
   if (cl->clParamsBitmap & CL_BIT_SOUR_MAC)
   {
       /*The structure is packed so there won't be unaligned address exception*/
       srcMac =  (MacAddrBytes_t *)(ethHeader->h_source);
       if ((srcMac->mac4MSB != cl->llc.srcMacAddr4bytes) || 
           (srcMac->mac2LSB != cl->llc.srcMacAddr2bytes)) {
           return CL_PARAM_NOMATCH; /* MAC addresses do not match */
       }
    }
    return CL_PARAM_MATCH; /*the classifier matches*/
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByVlan(Uint16 ethEncapsProto, Uint16 vlanPrio, 
                        Uint16 vlanId, QosClassifier_t   *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by 802.1q criterias
 *  \param[in]  Uint16 vlanPrio - vlan priority
 *  \param[in]  Uint16 vlanId - vlan ID
 *  \param[in]  Uint16 ethEncapsProto - Etherenet encapsulated protocol
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No match
 */
static ClassParamMatch_e MatchByVlan(Uint16 ethEncapsProto, Uint16 vlanPrio, 
                        Uint16 vlanId, QosClassifier_t   *cl)
{
    /*If priority parameter specified*/
    if (cl->clParamsBitmap & CL_BIT_PRIORITY)
    {
        if ( (vlanPrio < cl->vlan.prioLow )||
             (vlanPrio > cl->vlan.prioHigh) ){
            return CL_PARAM_NOMATCH; 
        }
    }
    if (cl->clParamsBitmap & CL_BIT_VLAN_ID) 
    {
        if (vlanId != cl->vlan.vlanId) {
             return CL_PARAM_NOMATCH;  
        }
    }
    return CL_PARAM_MATCH;
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByIpv4(struct iphdr  *ipHdr, struct udphdr *udpTcpHdr, 
                                Uint8 ipUpLayerProto,QosClassifier_t *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by IPv4 criterias
 *  \param[in]  struct iphdr  *ipHdr - packet IPv4 header
 *  \param[in]  Bool udpTcpHdrPresent - If packet contains UDP/TCP header (is UDP/TCP)
 *  \param[in]  Uint8 ipUpLayerProto - IP encapsulated protocol
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No match
 */
static ClassParamMatch_e MatchByIpv4(struct iphdr  *ipHdr, Bool udpTcpHdrPresent,
                                     Uint8 ipUpLayerProto, QosClassifier_t *cl)
{
   /*Check IP protocol*/
   if (cl->clParamsBitmap & CL_BIT_IP_PROTOCOL) 
   {
       if ((cl->ip.v4.protocol != FC_IP_PROTOCOL_ALL) &&
           (cl->ip.v4.protocol != ipUpLayerProto) &&
           ((cl->ip.v4.protocol != FC_IP_PROTOCOL_UDP_TCP) || !udpTcpHdrPresent)) 
       {
           return CL_PARAM_NOMATCH;
       }
   }
   /*Check addresses */
   if (cl->clParamsBitmap & CL_BIT_IP_SOUR_ADDR)  
   {
       if ((ntohl(ipHdr->saddr) & cl->ip.v4.srcMask) != (cl->ip.v4.srcAddr & cl->ip.v4.srcMask))
       {
             return CL_PARAM_NOMATCH;
       }
   }
   if (cl->clParamsBitmap & CL_BIT_IP_DEST_ADDR)  
   {
       if ((ntohl(ipHdr->daddr) & cl->ip.v4.dstMask) != (cl->ip.v4.dstAddr  & cl->ip.v4.dstMask)) 
       {
             return CL_PARAM_NOMATCH;
       }
   }
   /*Check ToS*/
   if (cl->clParamsBitmap & CL_BIT_IP_TOS) 
   {
       if ((((ipHdr->tos & cl->ip.v4.tosMask) > cl->ip.v4.tosHigh) ||
           ((ipHdr->tos & cl->ip.v4.tosMask) < cl->ip.v4.tosLow )))
       {
           return CL_PARAM_NOMATCH;
       }
   }
   return CL_PARAM_MATCH;
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByIpv6(struct Ipv6hdr_s  *ipHdr, struct udphdr *udpTcpHdr,
                               Uint8 ipUpLayerProto, Uint8 classEthType, Uint16 ethEncapsProto, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by IPv6 criterias
 *  \param[in]  sstruct Ipv6hdr_s  *ipHdr - packet IPv6 header
 *  \param[in]  Bool udpTcpHdrPresent - If packet contains UDP/TCP header (is UDP/TCP)
 *  \param[in]  Uint8 ipUpLayerProto - IP encapsulated protocol
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return Match/No match
 */
static ClassParamMatch_e MatchByIpv6(struct Ipv6hdr_s  *ipHdr, Bool udpTcpHdrPresent,
                                     Uint8 ipUpLayerProto, QosClassifier_t *cl)
{
    int i;

    /*If TOS parameter is specified*/
    if (cl->clParamsBitmap & CL_BIT_IP_TOS) 
    {
        if (((IPV6CLASS(ipHdr) & cl->ip.v6.tcMask) > cl->ip.v6.tcHigh) ||
            ((IPV6CLASS(ipHdr) & cl->ip.v6.tcMask) < cl->ip.v6.tcLow)){
            return CL_PARAM_NOMATCH; /* no match */
        }
    }
    /*If Flow Label parameter is specified*/
    if (cl->clParamsBitmap & CL_BIT_FLOW_LABEL) 
    {
        if (IPV6FLABEL(ipHdr) != (cl->ip.v6.flowLabel & 0xfffff)){
            return CL_PARAM_NOMATCH; /* no match */
        }
    }
    /*If Next Header parameter is specified*/
    if (cl->clParamsBitmap & CL_BIT_IP_PROTOCOL) 
    {
        if(cl->ip.v6.nextHdr != FC_IP_PROTOCOL_ALL) 
        {
            /*Don't match a packet with ESP header*/
            if (ipUpLayerProto == NEXTHDR_ESP){
                return CL_PARAM_NOMATCH;
            }

            if((cl->ip.v6.nextHdr != ipUpLayerProto) &&
               ((cl->ip.v6.nextHdr != FC_IP_PROTOCOL_UDP_TCP) || !udpTcpHdrPresent)) {
                return CL_PARAM_NOMATCH;
            }

        }
    }
    /*Verify address match if this criteria is specified*/
    if (cl->clParamsBitmap & CL_BIT_IP_SOUR_ADDR)
    {
        for (i=0; i<4; i++) 
        {
            if((ipHdr->saddr.in6_u.u6_addr32[i] & cl->ip.v6.srcMask[i]) != (cl->ip.v6.srcAddr[i] & cl->ip.v6.srcMask[i]))
                return CL_PARAM_NOMATCH;
        }
    }
    if (cl->clParamsBitmap & CL_BIT_IP_DEST_ADDR)
    {
        for (i=0; i<4; i++) 
        {
            if((ipHdr->daddr.in6_u.u6_addr32[i] & cl->ip.v6.dstMask[i]) != (cl->ip.v6.dstAddr[i] & cl->ip.v6.dstMask[i]))
                return CL_PARAM_NOMATCH;
        }
    }

    return CL_PARAM_MATCH;
}

/**************************************************************************/
/*! \fn static ClassParamMatch_e MatchByUdpTcp(struct udphdr *udpTcpHdr, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Verify if the classifier matches packet by UDP/TCP criterias
 *  \param[in]  struct udphdr *udpTcpHdr - packet UDP/TCP header
 *  \param[in]  QosClassifier_t  *cl - classifier
 *  \return  Match/No match
 */
static ClassParamMatch_e MatchByUdpTcp(struct udphdr *udpTcpHdr, QosClassifier_t *cl)
{
    /* TCP/UDP - test ports range */
    if (((ntohs(udpTcpHdr->source) < cl->udpTcp.srcPortStart) ||
         (ntohs(udpTcpHdr->source) > cl->udpTcp.srcPortEnd) ||
         (ntohs(udpTcpHdr->dest) < cl->udpTcp.dstPortStart) ||
         (ntohs(udpTcpHdr->dest) > cl->udpTcp.dstPortEnd))) 
    {
        return CL_PARAM_NOMATCH; /* no match */
    }
    return  CL_PARAM_MATCH; 
}

/**************************************************************************/
/*! \fn static void SetClassEntryData(ClassListEntry_t *entry, QosClassifier_t *cl)
 **************************************************************************
 *  \brief Set Classifier data into the list entry
 *  \param[out] ClassListEntry_t *entry - Classifier entry in the list
 *  \param[in]  QosClassifier_t *cl - classifier data
 *  \return  NA
 */
static void SetClassEntryData(ClassListEntry_t *entry, QosClassifier_t *cl)
{
    Uint32 sysIfIndex, cmim;
    int i;

    /*Translate the CMIM to system interface numbering*/
    cmim = cl->cmim;

    cl->cmim = 0;
    for (i=DOCSIS_MIN_IF_INDEX; i<=DOCSIS_MAX_IF_INDEX; i++) 
    {
        if ((0x80000000 >> i) & cmim) 
        {
            DPRINTK(KERN_DEBUG"Docsis CMIM bit %d is on\n", 32-i-1);

            /*Translate interface index from docsis numbering to Kernel numbering */
            if (FcUtils_GetSysIfIndexByDocsis(i, &sysIfIndex) != 0) 
            {
                DPRINTK(KERN_ERR"%s: Failed translate docsis interface index %u to system one\n",__FUNCTION__, i );
                return;
            }
            cl->cmim |= (0x80000000 >> (sysIfIndex-1));
        }
    }
    DPRINTK(KERN_DEBUG"Setting Classifier %u data:\n", cl->classID);
    DPRINTK(KERN_DEBUG"Docsis CMIM=0x%x, system CMIM=0x%x\n", cmim, cl->cmim);

    PrintClassData(cl);

    PAL_osMemCopy(&(entry->cl), cl, sizeof(QosClassifier_t));
}

/**************************************************************************/
/*! \fn static void PrintClassData(QosClassifier_t *cl)
 **************************************************************************
 *  \brief Print Classifier data
 *  \param[in]  QosClassifier_t *cl - classifier data
 *  \return  NA
 */
static void PrintClassData(QosClassifier_t *cl)
{
    int i;

    DPRINTK(KERN_DEBUG"classID = %u\n", cl->classID);
    DPRINTK(KERN_DEBUG"sfIndex = %u\n", cl->sfIndex);
    DPRINTK(KERN_DEBUG"phsIndex = %u\n", cl->phsIndex);
    DPRINTK(KERN_DEBUG"priority = %u\n", cl->priority);
    DPRINTK(KERN_DEBUG"pktCounter = %u\n", cl->pktCounter);
    DPRINTK(KERN_DEBUG"ppPktCounter = %u\n", cl->ppCounters.ppPktCounter);

    /*Which classification criterias are used in this classifier*/
    DPRINTK(KERN_DEBUG"isLlcCriterion = %u\n", cl->isLlcCriterion);
    DPRINTK(KERN_DEBUG"isVlanCriterion = %u\n", cl->isVlanCriterion);
    DPRINTK(KERN_DEBUG"isIpv4Criterion = %u\n", cl->isIpv4Criterion);
    DPRINTK(KERN_DEBUG"isIpv6Criterion = %u\n", cl->isIpv6Criterion);
    DPRINTK(KERN_DEBUG"isTcpUdpCriterion = %u\n", cl->isTcpUdpCriterion);
    DPRINTK(KERN_DEBUG"isCmimCriterion = %u\n", cl->isCmimCriterion);

    /* Bitmap of parameters which are relevant */
    DPRINTK(KERN_DEBUG"clParamsBitmap = %08X\n", cl->clParamsBitmap);

    /* Classifier parameters */
    if (cl->isIpv4Criterion) 
    {
        DPRINTK(KERN_DEBUG"IPv4 parameters:\n");
        DPRINTK(KERN_DEBUG"srcAddr = 0x%.8x\n",cl->ip.v4.srcAddr);
        DPRINTK(KERN_DEBUG"srcMask = 0x%.8x\n",cl->ip.v4.srcMask);
        DPRINTK(KERN_DEBUG"dstAddr = 0x%.8x\n",cl->ip.v4.dstAddr);
        DPRINTK(KERN_DEBUG"dstMask = 0x%.8x\n",cl->ip.v4.dstMask);
        DPRINTK(KERN_DEBUG"protocol = %u\n",cl->ip.v4.protocol);
        DPRINTK(KERN_DEBUG"tosLow = %u\n",cl->ip.v4.tosLow);
        DPRINTK(KERN_DEBUG"tosHigh = %u\n",cl->ip.v4.tosHigh);
        DPRINTK(KERN_DEBUG"tosMask = 0x%x\n",cl->ip.v4.tosMask);
    }
    if (cl->isIpv6Criterion) 
    {
        DPRINTK(KERN_DEBUG"IPv6 parameters:\n");
        DPRINTK(KERN_DEBUG"flowLabel = %u\n", cl->ip.v6.flowLabel); 
        DPRINTK(KERN_DEBUG"srcAddr = 0x"); 
        for (i=0; i<4; i++) 
            DPRINTK(KERN_DEBUG"%.8x ",cl->ip.v6.srcAddr[i]);
        DPRINTK(KERN_DEBUG"\nsrcMask = 0x");
        for (i=0; i<4; i++) 
            DPRINTK(KERN_DEBUG"%.8x ",cl->ip.v6.srcMask[i]);
        DPRINTK(KERN_DEBUG"\ndstAddr = 0x");
        for (i=0; i<4; i++) 
            DPRINTK(KERN_DEBUG"%.8x ",cl->ip.v6.dstAddr[i]);
        DPRINTK(KERN_DEBUG"\ndstMask = 0x");
        for (i=0; i<4; i++) 
            DPRINTK(KERN_DEBUG"%.8x ",cl->ip.v6.dstMask[i]);
        DPRINTK(KERN_DEBUG"\n"); 
        DPRINTK(KERN_DEBUG"nextHdr = %u\n", cl->ip.v6.nextHdr);
        DPRINTK(KERN_DEBUG"tosLow = %u\n",cl->ip.v6.tcLow);
        DPRINTK(KERN_DEBUG"tosHigh = %u\n",cl->ip.v6.tcHigh);
        DPRINTK(KERN_DEBUG"tosMask = 0x%x\n",cl->ip.v6.tcMask);
    }

    if (cl->isTcpUdpCriterion) 
    {
        DPRINTK(KERN_DEBUG"udpTcp parameters:\n");
        DPRINTK(KERN_DEBUG"srcPortStart = %u\n", cl->udpTcp.srcPortStart);
        DPRINTK(KERN_DEBUG"srcPortEnd = %u\n", cl->udpTcp.srcPortEnd);
        DPRINTK(KERN_DEBUG"dstPortStart = %u\n", cl->udpTcp.dstPortStart);
        DPRINTK(KERN_DEBUG"dstPortEnd = %u\n", cl->udpTcp.dstPortEnd);
    }

    if (cl->isLlcCriterion) 
    {
        Uint8 *fourByteBuf, *twoByteBuf;
        DPRINTK(KERN_DEBUG"llc parameters:\n");
        fourByteBuf = (Uint8 *)&cl->llc.dstMacAddr4bytes;
        twoByteBuf = (Uint8 *)&cl->llc.dstMacAddr2bytes;
        DPRINTK(KERN_DEBUG"dstMacAddr = %02X:%02X:%02X:%02X:%02X:%02X\n",
               fourByteBuf[0], fourByteBuf[1],fourByteBuf[2],fourByteBuf[3],
               twoByteBuf[0],twoByteBuf[1]);
        fourByteBuf = (Uint8 *)&cl->llc.dstMacMask4bytes;
        twoByteBuf = (Uint8 *)&cl->llc.dstMacMask2bytes;
        DPRINTK(KERN_DEBUG"dstMacMask = %02X:%02X:%02X:%02X:%02X:%02X\n",
               fourByteBuf[0], fourByteBuf[1],fourByteBuf[2],fourByteBuf[3],
               twoByteBuf[0],twoByteBuf[1]);

        fourByteBuf = (Uint8 *)&cl->llc.srcMacAddr4bytes;
        twoByteBuf = (Uint8 *)&cl->llc.srcMacAddr2bytes;
        DPRINTK(KERN_DEBUG"srcMacAddr = %02X:%02X:%02X:%02X:%02X:%02X\n",
               fourByteBuf[0], fourByteBuf[1],fourByteBuf[2],fourByteBuf[3],
               twoByteBuf[0],twoByteBuf[1]);

        DPRINTK(KERN_DEBUG"enetProtocol = %u\n", cl->llc.enetProtocol);
        DPRINTK(KERN_DEBUG"enetProtocolType = %u\n", cl->llc.enetProtocolType);
    }

    if (cl->isVlanCriterion) 
    {
        DPRINTK(KERN_DEBUG"vlan parameters:\n");
        DPRINTK(KERN_DEBUG"vlanId = %u\n", cl->vlan.vlanId);
        DPRINTK(KERN_DEBUG"prioLow = %u\n", cl->vlan.prioLow);
        DPRINTK(KERN_DEBUG"prioHigh = %u\n", cl->vlan.prioHigh);
    }

    if (cl->isCmimCriterion) 
    {
        DPRINTK(KERN_DEBUG"CMIM = %08X\n", cl->cmim);
    }

}


