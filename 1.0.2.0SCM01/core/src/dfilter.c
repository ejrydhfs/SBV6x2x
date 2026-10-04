/*
 *
 * dfilter.c
 * Description:
 * Docsis IP and LLC filters implementation
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

/*! \file dfilter.c
*   \brief Docsis IP and LLC filters implementation
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
#include <linux/udp.h>
#include <linux/list.h>
#include <linux/config.h>
#include <net/llc_pdu.h>
#include "_tistdtypes.h"
#include "pal.h"
#include "dfilter.h"
#include "dfltr_class_ctrl.h"
#include "dfltr_class_utils.h"


#ifdef DFILTER_DEBUG
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
extern int Dbridge_register_FilterCB(int (*Dbridge_Filter)(struct sk_buff *skb, unsigned int destIfMask, 
           unsigned int *acceptDestIfMask, unsigned int pktDirect));

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
/****************** IP/LLC Protocol filters definitions *******************/
#define MAX_NUM_LLC_FILTERS         16
#define MAX_NUM_IP_FILTERS          64

/*Definitions according to RFC*/
#define FLTR_CFG_TRUTH                  1
#define FLTR_CFG_FALSE                  2
#define FLTR_CFG_ACT_DISCARD    1
#define FLTR_CFG_ACT_ACCEPT     2


/*IP protocol types*/
#define FLTR_IP_PROTO_ANY            256

/*Traffic direction*/
#define FLTR_TRAFF_DIRECTION_INBOUND    (1<<0) /*inbound*/
#define FLTR_TRAFF_DIRECTION_OUTBOUND   (1<<1) /*outbound*/

/* SAP values */
#define SAP_SNAP    0xAA    /* SNAP encapsulated frames */
#define SAP_BPDU    0x42    /* BPDU */

/*Check if packet traffic direction matches the filter traffic direction*/
#define FLTR_IP_TRAFF_DIRECTION_MATCH(F,P)   (((F) & P) == P)
/*Check if MAC address is multicast*/
#define IS_MAC_MULTICAST(highByte) ((highByte) & 0x01)

/******************************************************************************************/
/* Entry in LLC filter list type */
typedef struct _llcFilterListEntry
{
    struct list_head list;
    FilterLLC_T      filter;
}LlcFilterListEntry_T;

/* Entry in IP filter list type */
typedef struct _ipFilterListEntry
{
    struct list_head list;
    FilterIP_T       filter;

}IpFilterListEntry_T;

/*Bridge data used for filtering*/
struct FltrBridgeData_s
{
    Uint32 srcIfIndex;          /*Source interface index*/
    Uint32 destIfIndex;         /*Destination interface index*/
    Uint32 destIntfBitmap;      /*Destination interfaces bitmap (one bit on for unicast packets)*/
    Uint32 acceptDestInfBitmap;/*Filter output: Accepted destination interfaces bitmap (after filtering)*/
};

struct PktData_s
{
    Uint8 *destMac;
    struct iphdr *ipHdr;
    struct udphdr *udpTcpHdr;
    Bool isUdpTcp;
    Uint16 sPort;
    Uint16 dPort;
};
/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
MODULE_LICENSE("GPL");

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static LIST_HEAD(llcFiltersListHead);/*LLC Filters list*/
static LIST_HEAD(ipFiltersListHead); /*IP Filters list*/

static Uint32 llcUnmatchedAction;    /*Action to perform on packed in case that no filter matched*/
static Uint32 numLLCFilters;         /*Number of active LLC filters*/
static Uint32 ipDefaultAction;       /*Action to perform on packed in case that no filter matched*/
static Uint32 numIPFilters;          /*Number of active IP filters*/

static StpControl_t   stpFilter;             /*Spanning Tree filtering control variable*/

static Bool ipFiltersEnabled;

/**************************************************************************/
/*      LOCAL PROTOTYPES:                                                 */
/**************************************************************************/
static int AddLLCFilter(FilterLLC_T *newFilter);
static int AddIpFilter(FilterIP_T *newFilter);
static int DelLLCFilter(Uint32 index);
static int DelIpFilter(Uint32 index);
static int UpdateLLCFilter(FilterLLC_T *filterData);
static int UpdateIpFilter(FilterIP_T *filterData);
static int SetLLCUnmatchAct(Uint32 action);
static int SetIpUnmatchAct(Uint32 action);
static int SetIpFiltersEnabled(Bool enabled);
static int GetLLCFilter(Uint32 index, FilterLLC_T *buf);
static int GetLLCMatches(Uint32 index, Uint32 *matches);
static int GetPpLLCMatches(Uint32 index, Uint32 *matches);
static int PrintPpLLCMatches(void);
static int GetLLCUnmatchAct(Uint8 *action);
static int GetIpFilter(Uint32 index, FilterIP_T *buf);
static int GetIpMatches(Uint32 index, Uint32 *matches);
static int GetPpIpMatches(Uint32 index, Uint32 *matches);
static int PrintPpIpMatches(void);
static int GetIpUnmatchAct(Uint8 *action);
static int CreateNewLLCEntry(LlcFilterListEntry_T **entry, FilterLLC_T *filter);
static int CreateNewIpEntry(IpFilterListEntry_T **entry, FilterIP_T *filter);
static DFltrAction_t ApplyLLCFilter(struct sk_buff *skb, Uint32 srcIfIndex, EthEncapsType_e enctype, Uint16 proto);
static inline DFltrAction_t ApplySTPFilter(EthEncapsType_e enctype, Uint16 proto);
static DFltrAction_t ApplyIPFilter(struct sk_buff *skb, struct PktData_s *pktData, struct FltrBridgeData_s *bridgeData) ;
static int FiltersIpInit(void);
static int FiltersLLCInit(void);
static int FiltersLLCCleanup(void);
static int FiltersIpCleanup(void);
static inline int IsBpduFrame(EthEncapsType_e enctype, Uint16 proto);
static int MatchIPAddr(FilterIP_T *ipFilter, Uint32 saddr, Uint32 daddr, Uint8 protocol);
static void SetIpFilterData(IpFilterListEntry_T *entry, FilterIP_T *filter);
static void SetLlcFilterData(LlcFilterListEntry_T *entry, FilterLLC_T *filter);
static void PrintIpFltrData(FilterIP_T *filter);
static void PrintLlcFltrData(FilterLLC_T *filter);

/**************************************************************************/
/*      INTERFACE FUNCTIONS                                               */
/**************************************************************************/
/**************************************************************************/
/*! \fn int StpControl_Set(Uint32 param1)
 **************************************************************************
 *  \brief Set spanning tree protocol control
 *  \param[in] int param1 - generic integer parameter
 *  \return  0 or error code
 */
int StpControl_Set(Uint32 param1)
{
    switch ((StpControl_t)param1)
    {
        case FLTR_STP_ON:
        case FLTR_NO_STP_DISCARD:
        case FLTR_NO_STP_FORWARD:
            stpFilter = (StpControl_t)param1;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

/**************************************************************************/
/*! \fn int DFilter_SetLLCData(int subtype, void *buf)
 **************************************************************************
 *  \brief Set LLC filter data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] int param1 - generic integer parameter
 *  \param[in] void *buf - input data buffer
 *  \return  0 or error code
 */
int DFilter_SetLLCData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype)
    {
    case FLTRIOC_S_SUBTYPE_ADD:
        ret = AddLLCFilter((FilterLLC_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_DEL:
        ret = DelLLCFilter(param1);
        break;
    case FLTRIOC_S_SUBTYPE_UPD:
        ret = UpdateLLCFilter((FilterLLC_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_UNMATCH_ACT:
        ret = SetLLCUnmatchAct(param1);
        break;
    case FLTRIOC_S_SUBTYPE_PRINT:
        ret = PrintPpLLCMatches();
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*! \fn int DFilter_SetIpData(int subtype, void *buf)
 **************************************************************************
 *  \brief Set IP filter data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] int param1 - generic integer parameter
 *  \param[in] void *buf - input data buffer
 *  \return  0 or error code
 */
int DFilter_SetIpData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case FLTRIOC_S_SUBTYPE_ADD:
        ret = AddIpFilter((FilterIP_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_DEL:
        ret = DelIpFilter(param1);
        break;
    case FLTRIOC_S_SUBTYPE_UPD:
        ret = UpdateIpFilter((FilterIP_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_UNMATCH_ACT:
        ret = SetIpUnmatchAct(param1);
        break;
    case FLTRIOC_S_SUBTYPE_IPFLTR_ENABLED:
        ret = SetIpFiltersEnabled((Bool)param1);
        break;
    case FLTRIOC_S_SUBTYPE_PRINT:
        ret = PrintPpIpMatches();
        break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*! \fn int DFilter_GetLLCData(int subtype, Uint32 param1, void *buf)
 **************************************************************************
 *  \brief Get LLC filter data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] Uint32 param1 - generic input parameter
 *  \param[out] void *buf - output data buffer
 *  \return  0 or error code
 */

int DFilter_GetLLCData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case FLTRIOC_G_SUBTYPE_FLTR:
        ret = GetLLCFilter(param1,(FilterLLC_T *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_MATCHES:
        ret = GetLLCMatches(param1, (Uint32 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_UNMATCH_ACT:
        ret = GetLLCUnmatchAct((Uint8 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_PP_MATCHES:
        ret = GetPpLLCMatches(param1, (Uint32 *)buf);
        break;
    default:
        return -EINVAL;
    }
    return ret;

}

/**************************************************************************/
/*! \fn int int DFilter_GetIpData(int subtype, Uint32 param1, void *buf)
 **************************************************************************
 *  \brief Get IP filters data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] Uint32 param1 - generic input parameter
 *  \param[out] void *buf - output data buffer
 *  \return  0 or error code
 */

int DFilter_GetIpData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case FLTRIOC_G_SUBTYPE_FLTR:
        ret = GetIpFilter(param1,(FilterIP_T *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_MATCHES:
        ret = GetIpMatches(param1, (Uint32 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_UNMATCH_ACT:
        ret = GetIpUnmatchAct((Uint8 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_PP_MATCHES:
        ret = GetPpIpMatches(param1, (Uint32 *)buf);
        break;
    default:
        return -EINVAL;
    }
    return ret;

}

/**************************************************************************/
/*! \fn int DFilter_Apply(struct sk_buff *skb, Uint destIfMask, Uint *accDestIfMask, Uint pktDirect)
 **************************************************************************
 *  \brief Apply Protocol filters on a given packet. 
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in/out] struct FltrBridgeData_s *bridgeData - bridge data. Output accepted interfaces bitmap (used for non-unicast packet) 
 *  \return  action to perform on packet : accept/discard
 */
int DFilter_Apply(struct sk_buff *skb, Uint destIfMask, Uint *accDestIfMask, Uint pktDirect)
{
    EthEncapsType_e encapsType;
    Uint16 encapsProto;
    struct ethhdr *ethHeader;
    struct vlan_hdr *vlanHdr;
    struct PktData_s pktData;
    struct FltrBridgeData_s bridgeData;
    Bool withVlan;
    Uint8 upLayerProto, *ethData;
    DFltrAction_t ret;

    DPRINTK(KERN_DEBUG"%s: Start. destIfMask = 0x%.8x, pktDirect=%u\n", __FUNCTION__, destIfMask, pktDirect);
    /* All interfaces are accepted by default*/
    *accDestIfMask = destIfMask;

    /*Get source interface index from skb*/
    bridgeData.srcIfIndex = skb->input_dev->ifindex;

    /*Parse ethernet header*/
    ethHeader = eth_hdr(skb);
    ethData = skb->data + ETH_HLEN; 
    FcUtils_ExtractEthEncapsType(ethHeader, ethData, &encapsType, &encapsProto, &withVlan);

    /* Optimization - no filter is applied and default is DISCARD, no need to continue */
    if (numLLCFilters == 0) 
    {
       DPRINTK(KERN_DEBUG"%s: No active LLC filters\n", __FUNCTION__);
       if(llcUnmatchedAction == DFLTR_DISCARD)
       {
           DPRINTK(KERN_DEBUG"%s: Discarding packet by unmatched action\n", __FUNCTION__);
           return DFLTR_DISCARD;
       }
    }
    else
    {
        /* LLC inbound filtering if LLC exists */
        if (ApplyLLCFilter(skb, bridgeData.srcIfIndex, encapsType, encapsProto) != DFLTR_ACCEPT)
        {
            DPRINTK(KERN_DEBUG"%s: Discarding packet by LLC filters\n", __FUNCTION__);
            return DFLTR_DISCARD;
        }
    }

    /* Do not accept BPDU's by STP control filtering */
    if ( (stpFilter == FLTR_NO_STP_DISCARD) && ApplySTPFilter(encapsType, encapsProto) != DFLTR_ACCEPT )
    {
        DPRINTK(KERN_DEBUG"%s: Discarding packet by Bpdu filter\n", __FUNCTION__);
        return DFLTR_DISCARD;
    }
#if 0
    if (pktDirect == DFLTR_FROM_CABLE)
    {
        /* Multicast filtering layer 2 and 3 */
           if ((((*dest_addr) & 0x01) != 0) && ((*dest_addr) != 0xff) /* DEST_MULTICAST */) {
              if(multMacListSize && (cm_brgFilterMultStaticMac(dest_addr) == TRUE)) {
                 /* Packet accepted by static multicast filter */
              }
              else {
                 if(type == 0x800) { /* All-host always exist */
                    /* Only multicast packets managed by IGMP may be accepted through multicast IP filters */
                    if (multIpFilter(ntohl(IPHeader->DADDR)) == FALSE) /* All-host always present in table */
                       return CM_BRG_DFLTR_DISCARD;
                 }
                 else {
                    /* Discard all other type multicast packets */
                    return CM_BRG_DFLTR_DISCARD;
                 }
              }
           }
    }
#endif
    /* IP filtering */
    if (!ipFiltersEnabled) 
    {
        DPRINTK(KERN_DEBUG"%s: IP filters are disabled! Accepting packet\n", __FUNCTION__);
        return DFLTR_ACCEPT;
    }
    if (encapsProto == ETH_P_IP)
    {
        if (FcUtils_ExtractNetHdrs(ethData, encapsType, encapsProto, withVlan,
                                   (void **)(&(pktData.ipHdr)), &vlanHdr) != 0 )
        {
            DPRINTK(KERN_WARNING"%s: Unsupported packet format: failed to get network headers\n",__FUNCTION__);
            return DFLTR_DISCARD;
        }
        FcUtils_ExtractIpUpLayerHdr(encapsProto, (void *)pktData.ipHdr, &upLayerProto, (Uint8 **)(&(pktData.udpTcpHdr)));
#if 0
        if (pktDirect != DFLTR_FROM_CABLE)
        {
            /* CPE IP filters are called if enabled */
            if (cpeIpMax != -1) {
               udpHeader = (udp_hdr_t*)(((unsigned long)IPHeader) + ((unsigned long)((IPHeader->VERS_HLEN & 0x0f) << 2)));
               /* DHCP/BOOTP packets are not discarded nor learnt through IP spoofing filters */
               if (udpHeader->src_port != PORT_DHCP_CLIENT)   {
                  if (cpeIpFilter(ntohl(IPHeader->SADDR)) == FALSE)
                     return CM_BRG_DFLTR_DISCARD;
               }
            }
        }
#endif

       /* Optimization - no filter is applied and default is DISCARD, no need to call the filter */
       if (numIPFilters == 0)
       {
           DPRINTK(KERN_DEBUG"%s: No active IP filters\n", __FUNCTION__);
           if (ipDefaultAction == DFLTR_DISCARD)
           {
               DPRINTK(KERN_DEBUG"%s: Discarding packet by IP default action\n", __FUNCTION__);
               return DFLTR_DISCARD;
           }
           else
           {
               DPRINTK(KERN_DEBUG"%s: Accepting packet by IP default action. accDestIfMask=0x%.8x\n", __FUNCTION__, *accDestIfMask);
               return DFLTR_ACCEPT;
           }
       }
       bridgeData.destIntfBitmap = destIfMask;
       pktData.destMac = ethHeader->h_dest;
       ret = ApplyIPFilter(skb, &pktData, &bridgeData);
       *accDestIfMask = bridgeData.acceptDestInfBitmap;
       if (ret == DFLTR_DISCARD)
           DPRINTK(KERN_DEBUG"%s: Discarding packet by IP filter\n", __FUNCTION__);
       else
           DPRINTK(KERN_DEBUG"%s: Accepting packet by IP filter. accDestIfMask=0x%.8x\n", __FUNCTION__, *accDestIfMask);
       return ret;
    }

    DPRINTK(KERN_DEBUG"%s: Non-IP packet. Accepting by LLC filters. accDestIfMask=0x%.8x\n", __FUNCTION__, *accDestIfMask);
    return DFLTR_ACCEPT;

}

/**************************************************************************/
/*! \fn int DFilters_Init(void)
 **************************************************************************
 *  \brief Init Docsis Filters module 
 *  \return  OK/NOK
 */
int DFilters_Init(void)
{
    if (FiltersIpInit() != 0)
        return -1;
    if (FiltersLLCInit() != 0)
        return -1;
    stpFilter = FLTR_NO_STP_DISCARD;

    return 0;
}

/**************************************************************************/
/*! \fn int DFilters_Cleanup(void)
 **************************************************************************
 *  \brief Cleanup Docsis Filters module 
 *  \return  OK/NOK
 */
int DFilters_Cleanup(void)
{
    if (FiltersIpCleanup() != 0)
        return -1;
    if (FiltersLLCCleanup() != 0)
        return -1;

    return 0;
}

/**************************************************************************/
/*! \fn int DFilters_Start(void)
 **************************************************************************
 *  \brief Start Docsis Filters module operation
 *  \return  OK/NOK
 */
int DFilters_Start(void)
{
/*    if (numIPFilters || numLLCFilters) 
    {*/
        /* Register Filter callback in Docsis bridge*/
        if(Dbridge_register_FilterCB(DFilter_Apply))
            return -1;
/*    }*/
    return 0;
}

/**************************************************************************/
/*! \fn int DFilters_Stop(void)
 **************************************************************************
 *  \brief Stop Docsis Filters module operation
 *  \return  OK/NOK
 */
int DFilters_Stop(void)
{
    /*Unregister Filter callback in Docsis bridge*/
    if (Dbridge_register_FilterCB(NULL))
        return -1;
    return 0;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS                                                   */
/**************************************************************************/
/**************************************************************************/
/*! \fn static int FiltersLLCInit(void)
 **************************************************************************
 *  \brief Init LLC filters
 *  \return  OK/NOK
 */
static int FiltersLLCInit(void)
{
    numLLCFilters = 0;
    llcUnmatchedAction = DFLTR_ACCEPT;
    return 0;
}

/**************************************************************************/
/*! \fn static int FiltersLLCCleanup(void)
 **************************************************************************
 *  \brief Clean up LLC filters
 *  \return  OK/NOK
 */
static int FiltersLLCCleanup(void)
{
    struct list_head *pos;
    LlcFilterListEntry_T *curr=NULL;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /* free filters list */
    pos = llcFiltersListHead.next;
    while(pos != &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);
        pos = pos->next;
        PAL_osMemFree(0, curr, sizeof(LlcFilterListEntry_T));
    }
    numLLCFilters = 0;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int FiltersIpInit(void)
 **************************************************************************
 *  \brief Init IP filters
 *  \return  OK/NOK
 */
static int FiltersIpInit(void)
{
    numIPFilters = 0;
    ipDefaultAction = DFLTR_ACCEPT;
    ipFiltersEnabled = 1;
    return 0;
}

/**************************************************************************/
/*! \fn static int FiltersIpCleanup(void)
 **************************************************************************
 *  \brief Cleanup IP filters
 *  \return  OK/NOK
 */
static int FiltersIpCleanup(void)
{
    struct list_head *pos;
    IpFilterListEntry_T *curr=NULL;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /* free filters list */

    pos = ipFiltersListHead.next;
    while(pos != &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);
        pos = pos->next;
        PAL_osMemFree(0, curr, sizeof(IpFilterListEntry_T));
    }
    numIPFilters = 0;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int GetLlcListEntryByIndex(Uint32 index, LlcFilterListEntry_T **entry)
 **************************************************************************
 *  \brief Get LLC list entry by index
 *  \param[in]  Uint32 index - entry index
 *  \param[out] LlcFilterListEntry_T **entry - pointer to the found entry
 *  \return  OK/NOK
 */
static int GetLlcListEntryByIndex(Uint32 index, LlcFilterListEntry_T **entry)
{
    struct list_head *pos;
    LlcFilterListEntry_T *curr=NULL;

    /* find a place in the list according to the index */
    list_for_each(pos, &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);
        if (curr->filter.LLCIndex == index) 
            break;
    }
    if (pos != &llcFiltersListHead) 
    {
        *entry = curr;
        return 0;
    }
    else
        return -1;
}

/**************************************************************************/
/*! \fn static int GetIpListEntryByIndex(Uint32 index, IpFilterListEntry_T **entry)
 **************************************************************************
 *  \brief Get IP list entry by index
 *  \param[in]  Uint32 index - entry index
 *  \param[out] IpFilterListEntry_T **entry - pointer to the found entry
 *  \return  OK/NOK
 */
static int GetIpListEntryByIndex(Uint32 index, IpFilterListEntry_T **entry)
{
    struct list_head *pos;
    IpFilterListEntry_T *curr=NULL;

    /* find a place in the list according to the index */
    list_for_each(pos, &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);
        if (curr->filter.IPIndex == index) 
            break;
    }
    if (pos != &ipFiltersListHead) 
    {
        *entry = curr;
        return 0;
    }
    else
        return -1;
}

/**************************************************************************/
/*! \fn static int AddLLCFilter(FilterLLC_T *newFilter)
 **************************************************************************
 *  \brief Add new LLC filter to the LLC filters list
 *  \param[in] FilterLLC_T *newFilter - filter structure
 *  \return  0 or error code
 */
static int AddLLCFilter(FilterLLC_T *newFilter)
{
    Uint32 lockKey;
    struct list_head *pos;
    LlcFilterListEntry_T *curr=NULL, *new;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, newFilter->LLCIndex);

    if (numLLCFilters == MAX_NUM_LLC_FILTERS) {
        DPRINTK(KERN_WARNING"Failed to add LLC filter - the LLC table is full\n");
        return -EINVAL;
    }

    /* find a place in the list according to the index */
    list_for_each(pos, &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);
        if (curr->filter.LLCIndex >= newFilter->LLCIndex) 
            break;
    }

    /*if filter with such index already exists return error*/
    if (curr && (curr->filter.LLCIndex == newFilter->LLCIndex)) {
        DPRINTK(KERN_WARNING"Failed to add LLC filter - filter with such index already exists\n");
        return -EINVAL;
    }

    /*Create new list entry*/
    if (CreateNewLLCEntry(&new, newFilter)){
        DPRINTK(KERN_ERR"Failed to add LLC filter - failed to create entry in the list\n");
        return -EINVAL;
    }

    /* insert before the current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_add(&new->list, pos->prev);
    numLLCFilters ++;

    /*If it is the first filter that is added register the filter callback in the bridge*/
/*    if ((numLLCFilters == 1) && (numIPFilters == 0)) 
    {
        if (Dbridge_register_FilterCB(DFilter_Apply))
            return -EFAULT;
    }*/

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int AddIpFilter(FilterIP_T *newFilter)
 **************************************************************************
 *  \brief Add new IP filter to the IP filters list
 *  \param[in] FilterIP_T *newFilter - filter structure
 *  \return  OK/NOK
 */
static int AddIpFilter(FilterIP_T *newFilter)
{
    Uint32 lockKey;
    struct list_head *pos;
    IpFilterListEntry_T *curr=NULL, *new;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, newFilter->IPIndex);

    if (numIPFilters == MAX_NUM_IP_FILTERS) {
        DPRINTK(KERN_WARNING"Failed to add IP filter - the IP table is full\n");
        return -EINVAL;
    }

    /* find a place in the list according to the index */
    list_for_each(pos, &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);
        if (curr->filter.IPIndex >= newFilter->IPIndex) 
            break;
    }

    /*if filter with such index already exists return error*/
    if (curr && (curr->filter.IPIndex == newFilter->IPIndex)) {
        DPRINTK(KERN_WARNING"Failed to add IP filter - filter with such index already exists\n");
        return -EINVAL;
    }

    /*Create new list entry*/
    if (CreateNewIpEntry(&new, newFilter)){
        DPRINTK(KERN_ERR"Failed to add IP filter - failed to create entry in the list\n");
        return -EINVAL;
    }

    /* insert before the current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_add(&new->list, pos->prev);
    numIPFilters ++;

    /*If it is the first filter that is added register the filter callback in the bridge*/
/*    if ((numLLCFilters == 0) && (numIPFilters == 1)) 
    {
        if (Dbridge_register_FilterCB(DFilter_Apply))
            return -EFAULT;
    }*/

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int UpdateLlcFilter(FilterLLC_T *filterData)
 **************************************************************************
 *  \brief Update LLC filter with new data
 *  \param[in] FilterLLC_T *filterData - filter structure
 *  \return  OK/NOK
 */
static int UpdateLLCFilter(FilterLLC_T *filterData)
{
    Uint32 lockKey;
    LlcFilterListEntry_T *entry;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__,filterData->LLCIndex);

    /* find a filter with the same index */
    if (GetLlcListEntryByIndex(filterData->LLCIndex, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to update Llc filter - index %d not found\n", filterData->LLCIndex);
        return -EINVAL;
    }

    /* update the data */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    SetLlcFilterData(entry, filterData);    
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}
/**************************************************************************/
/*! \fn static int UpdateIpFilter(FilterIP_T *filterData)
 **************************************************************************
 *  \brief Update IP filter with new data
 *  \param[in] FilterIP_T *filterData - filter structure
 *  \return  OK/NOK
 */
static int UpdateIpFilter(FilterIP_T *filterData)
{
    Uint32 lockKey;
    IpFilterListEntry_T *entry;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, filterData->IPIndex);

    /* find a filter with the same index */
    if (GetIpListEntryByIndex(filterData->IPIndex, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to update IP filter - index %d not found\n", filterData->IPIndex);
        return -EINVAL;
    }

    /* update the data */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    SetIpFilterData(entry, filterData);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int DelLLCFilter(Uint32 index)
 **************************************************************************
 *  \brief Delete an LLC filter with the given index
 *  \param[in] Uint32 index - filter index
 *  \return  0 or error code
 */
static int DelLLCFilter(Uint32 index)
{
    Uint32 lockKey;
    LlcFilterListEntry_T *entry;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, index);

    if (!numLLCFilters) {
        DPRINTK(KERN_WARNING"Failed to delete LLC filter - the LLC table is empty\n");
        return -EINVAL;
    }
    if (GetLlcListEntryByIndex(index, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to delete LLC filter - index %d not found\n", index);
        return -EINVAL;
    }
    /* delete current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_del(&entry->list);
    numLLCFilters --;

    /*If it was the only filter that was active set filter callback to NULL in docsis bridge*/
/*    if ((numLLCFilters == 0) && (numIPFilters == 0)) 
    {
        if (Dbridge_register_FilterCB(NULL))
            return -EFAULT;
    }*/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    PAL_osMemFree(0, entry, sizeof(LlcFilterListEntry_T));

    return 0;
}

/**************************************************************************/
/*! \fn static int DelIpFilter(Uint32 index)
 **************************************************************************
 *  \brief Delete an IP filter with the given index
 *  \param[in] Uint32 index - filter index
 *  \return  0 or error code
 */
static int DelIpFilter(Uint32 index)
{
    Uint32 lockKey;
    IpFilterListEntry_T *entry;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, index);

    if (!numIPFilters) {
        DPRINTK(KERN_WARNING"Failed to delete IP filter - the IP table is empty\n");
        return -EINVAL;
    }
    if (GetIpListEntryByIndex(index, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to delete IP filter - index %d not found\n", index);
        return -EINVAL;
    }
    /* delete current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_del(&entry->list);
    numIPFilters --;

    /*If it was the only filter that was active set filter callback to NULL in docsis bridge*/
/*    if ((numLLCFilters == 0) && (numIPFilters == 0)) 
    {
        if (Dbridge_register_FilterCB(NULL))
            return -EFAULT;
    }*/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    PAL_osMemFree(0, entry, sizeof(IpFilterListEntry_T));

    return 0;
}

/**************************************************************************/
/*! \fn static int SetLLCUnmatchAct(Uint32 action)
 **************************************************************************
 *  \brief Set LLC unmatched action
 *  \param[in] Uint32 action - action
 *  \return  0 or error code
 */
static int SetLLCUnmatchAct(Uint32 action)
{
    Uint32 lockKey;

    DPRINTK(KERN_DEBUG"%s. Action = %u\n",__FUNCTION__, action);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /*translate from RFC numbering to local*/
    llcUnmatchedAction = (action == FLTR_CFG_ACT_DISCARD ? DFLTR_DISCARD : DFLTR_ACCEPT);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    DPRINTK(KERN_DEBUG"%s. llcUnmatchedAction = %u\n",__FUNCTION__, llcUnmatchedAction);

    return 0;
}

/**************************************************************************/
/*! \fn static int SetIpFiltersEnabled(Bool enabled)
 **************************************************************************
 *  \brief Set IP filters enabled flag
 *  \param[in] Uint32 action - action
 *  \return  0 or error code
 */
static int SetIpFiltersEnabled(Bool enabled)
{
    ipFiltersEnabled = enabled;

    DPRINTK(KERN_DEBUG"%s. ipFiltersEnabled = %d\n",__FUNCTION__, ipFiltersEnabled);

    return 0;
}

/**************************************************************************/
/*! \fn static int SetIpUnmatchAct(Uint32 action)
 **************************************************************************
 *  \brief Set IP unmatched action
 *  \param[in] Uint32 action - action
 *  \return  0 or error code
 */
static int SetIpUnmatchAct(Uint32 action)
{
    Uint32 lockKey;

    DPRINTK(KERN_DEBUG"%s. Action = %u\n",__FUNCTION__, action);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /*translate from RFC numbering to local*/
    ipDefaultAction = (action == FLTR_CFG_ACT_DISCARD ? DFLTR_DISCARD : DFLTR_ACCEPT);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    DPRINTK(KERN_DEBUG"%s. ipDefaultAction = %u\n",__FUNCTION__, ipDefaultAction);

    return 0;
}

/**************************************************************************/
/*! \fn static int GetLLCFilter(Uint32 index, FilterLLC_T *buf)
 **************************************************************************
 *  \brief Get LLC filter by index
 *  \param[in] Uint32 index - filter index
 *  \param[out] FilterLLC_T *buf - output data buffer
 *  \return  0 or error code
 */
static int GetLLCFilter(Uint32 index, FilterLLC_T *buf)
{
    LlcFilterListEntry_T *entry;

    if (GetLlcListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"LLC filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(buf, &(entry->filter), sizeof(FilterLLC_T));
    return 0;

}

/**************************************************************************/
/*! \fn static int GetIpFilter(Uint32 index, FilterIP_T *buf)
 **************************************************************************
 *  \brief Get IP filter by index
 *  \param[in] Uint32 index - filter index
 *  \param[in] FilterIP_T *buf - input data buffer
 *  \return  0 or error code
 */
static int GetIpFilter(Uint32 index, FilterIP_T *buf)
{
    IpFilterListEntry_T *entry;

    if (GetIpListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"IP filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(buf, &(entry->filter), sizeof(FilterIP_T));
    return 0;

}

/**************************************************************************/
/*! \fn static int GetLLCMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get number of matches for LLC filter
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetLLCMatches(Uint32 index, Uint32 *matches)
{
    LlcFilterListEntry_T *entry;
    if (GetLlcListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"LLC filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(matches, &entry->filter.LLCMatches, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetPpLLCMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get number of packet processor matches for LLC filter
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetPpLLCMatches(Uint32 index, Uint32 *matches)
{
    Uint32 lockKey;
    LlcFilterListEntry_T *entry;
    TI_PP_SESSION_STATS sessionStats;
    Uint32 deadSessionsMatches = 0;
    Uint32 currSessionsMatches = 0;
    DppCountSessionEntry_t *currSessionEntry = NULL;

    memset(&sessionStats, 0, sizeof(sessionStats));

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* get matched of dead sessions */
    if (GetLlcListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"LLC filter %d not found\n", index);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -EINVAL;
    }

    deadSessionsMatches = entry->filter.LLCPpCounters.ppMatches;

    /* get matched of current sessions */
    if (entry->filter.LLCPpCounters.activeSessionsList != NULL)
    {
        currSessionEntry = entry->filter.LLCPpCounters.activeSessionsList;

        while (currSessionEntry != NULL)
        {
            if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
            {
                DPRINTK(KERN_INFO"Failed to get session %d statistics for LLC filter index %d\n", 
                        currSessionEntry->sessionHandle, entry->filter.LLCIndex);
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                return -EINVAL;
            }

            currSessionsMatches += sessionStats.packets_forwarded;
            currSessionEntry = currSessionEntry->llcFilterInfo.nextSession;
        }
    }

    *matches = deadSessionsMatches + currSessionsMatches;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	return 0;
}

/**************************************************************************/
/*! \fn static int PrintPpLLCMatches(void)
 **************************************************************************
 *  \brief Print LLC filters packet processor matches
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 or error code
 */
static int PrintPpLLCMatches(void)
{
    Uint32 matches = 0;
    Uint32 ppMatches = 0;
    struct list_head *pos = NULL;
    LlcFilterListEntry_T *curr = NULL;

	printk("\n------------------------------------");
	printk("\n LLC Filters match packets counters ");
	printk("\n------------------------------------");
	printk("\nLLCIndex\tLLCIfIndex\tHostPktCount\tPpPktCount");
	printk("\n--------\t----------\t------------\t----------");

    list_for_each(pos, &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);

        GetLLCMatches(curr->filter.LLCIndex, &matches);
        GetPpLLCMatches(curr->filter.LLCIndex, &ppMatches);
        printk("\n0x%x\t0x%x\t%u\t%u", curr->filter.LLCIndex, curr->filter.LLCIfIndex, 
               matches, ppMatches);
    }

	printk("\n");
	printk("\n");
	return 0;
}

/**************************************************************************/
/*! \fn static int GetIpMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get IP filter by index
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetIpMatches(Uint32 index, Uint32 *matches)
{
    IpFilterListEntry_T *entry;
    if (GetIpListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"IP filter %d not found\n", index);
        return -EINVAL;
    }

    PAL_osMemCopy(matches, &entry->filter.IPMatches, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetPpIpMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get packet processor IP filter by index
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetPpIpMatches(Uint32 index, Uint32 *matches)
{
    Uint32 lockKey;
    IpFilterListEntry_T *entry;
    TI_PP_SESSION_STATS sessionStats;
    Uint32 inDeadSessionsMatches = 0;
    Uint32 inCurrSessionsMatches = 0;
    Uint32 outDeadSessionsMatches = 0;
    Uint32 outCurrSessionsMatches = 0;
    DppCountSessionEntry_t *currSessionEntry = NULL;

    memset(&sessionStats, 0, sizeof(sessionStats));

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* get matched from dead sessions */
    if (GetIpListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"IP filter %d not found\n", index);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -EINVAL;
    }

    /* Inbound */
    if (entry->filter.IPDirection & FLTR_TRAFF_DIRECTION_INBOUND)
    {
        /* get matched of dead sessions */
        inDeadSessionsMatches += entry->filter.IPInPpCounters.ppMatches;

        /* get matched of current sessions */
        if (entry->filter.IPInPpCounters.activeSessionsList != NULL)
        {
            currSessionEntry = entry->filter.IPInPpCounters.activeSessionsList;
            
            while (currSessionEntry != NULL)
            {
                if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
                {
                    DPRINTK(KERN_INFO"Failed to get session %d statistics for Inbound IP filter index %d\n", 
                            currSessionEntry->sessionHandle, entry->filter.IPIndex);
                    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                    return -EINVAL;
                }
    
                inCurrSessionsMatches += sessionStats.packets_forwarded;
                currSessionEntry = currSessionEntry->inIpFilterInfo.nextSession;
            }
        }
    }

    /* Outbond */
    if (entry->filter.IPDirection & FLTR_TRAFF_DIRECTION_OUTBOUND)
    {
        /* get matched of dead sessions */
        outDeadSessionsMatches = entry->filter.IPOutPpCounters.ppMatches;

        /* get matched of current sessions */
        if (entry->filter.IPOutPpCounters.activeSessionsList != NULL)
        {
            currSessionEntry = entry->filter.IPOutPpCounters.activeSessionsList;
            
            while (currSessionEntry != NULL)
            {
                if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
                {
                    DPRINTK(KERN_INFO"Failed to get session %d statistics for Outbond IP filter index %d\n", 
                            currSessionEntry->sessionHandle, entry->filter.IPIndex);
                    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                    return -EINVAL;
                }
    
                outCurrSessionsMatches += sessionStats.packets_forwarded;
                currSessionEntry = currSessionEntry->outIpFilterInfo.nextSession;
            }
        }
    }

    *matches = inDeadSessionsMatches + inCurrSessionsMatches + outDeadSessionsMatches + outCurrSessionsMatches;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	return 0;
}

/**************************************************************************/
/*! \fn static int PrintPpIpMatches(void)
 **************************************************************************
 *  \brief Print IP filters packet processor matches
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 or error code
 */
static int PrintPpIpMatches(void)
{
    Uint32 matches = 0;
    Uint32 ppMatches = 0;
    struct list_head *pos = NULL;
    IpFilterListEntry_T *curr = NULL;

	printk("\n-----------------------------------");
	printk("\n IP Filters match packets counters ");
	printk("\n-----------------------------------");
	printk("\nIPIndex\tIPIfIndex\tHostPktCount\tPpPktCount");
	printk("\n-------\t---------\t------------\t----------");

    list_for_each(pos, &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);

        GetIpMatches(curr->filter.IPIndex, &matches);
        GetPpIpMatches(curr->filter.IPIndex, &ppMatches);
        printk("\n0x%x\t0x%x\t%u\t%u", curr->filter.IPIndex, curr->filter.IPIfIndex, 
               matches, ppMatches);
    }

	printk("\n");
	printk("\n");
	return 0;
}

/**************************************************************************/
/*! \fn static int GetLLCUnmatchAct(Uint8 *action)
 **************************************************************************
 *  \brief Get LLC umatched action
 *  \param[out] Uint8 *action - output buffer
 *  \return  0 or error code
 */
static int GetLLCUnmatchAct(Uint8 *action)
{
    Uint32 actLocal = (llcUnmatchedAction == DFLTR_DISCARD ? FLTR_CFG_ACT_DISCARD : FLTR_CFG_ACT_ACCEPT);
    PAL_osMemCopy(action, &actLocal, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetIpUnmatchAct(Uint8 *action)
 **************************************************************************
 *  \brief Get number of matches for IP filter
 *  \param[out] Uint8 *action - output buffer
 *  \return  0 or error code
 */
static int GetIpUnmatchAct(Uint8 *action)
{
    Uint32 actLocal = (ipDefaultAction == DFLTR_DISCARD ? FLTR_CFG_ACT_DISCARD : FLTR_CFG_ACT_ACCEPT);
    PAL_osMemCopy(action, &actLocal, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int CreateNewLLCEntry(LlcFilterListEntry_T **entry, FilterLLC_T *filter)
 **************************************************************************
 *  \brief Create new entry in the LLC filters list
 *  \param[out] LlcFilterListEntry_T **entry - pointer to the allocated entry
 *  \param[in]  FilterLLC_T *filter - filter data
 *  \return  OK/NOK
 */
static int CreateNewLLCEntry(LlcFilterListEntry_T **entry, FilterLLC_T *filter)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(LlcFilterListEntry_T), 0, (Ptr *)entry);
    if (res != PAL_SOK) {
        DPRINTK(KERN_ERR"%s: Failed to allocate dynamic memory\n",__FUNCTION__);
        return -1;
    }

    SetLlcFilterData(*entry, filter);   

    return 0;
}

/**************************************************************************/
/*! \fn static int CreateNewIpEntry(IpFilterListEntry_T **entry, FilterIP_T *filter)
 **************************************************************************
 *  \brief Create new entry in the IP filters list
 *  \param[out] IpFilterListEntry_T **entry - pointer to the allocated entry
 *  \param[in]  FilterIP_T *filter - filter data
 *  \return  OK/NOK
 */
static int CreateNewIpEntry(IpFilterListEntry_T **entry, FilterIP_T *filter)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(IpFilterListEntry_T), 0, (Ptr *)entry);
    if (res != PAL_SOK) {
        DPRINTK(KERN_ERR"%s: Failed to allocate dynamic memory\n",__FUNCTION__);
        return -1;
    }

    SetIpFilterData(*entry, filter);

    return 0;
}
/**************************************************************************/
/*! \fn static void SetLlcFilterData(LlcFilterListEntry_T *entry, FilterLLC_T *filter)
 **************************************************************************
 *  \brief Set Llc filter data to the list entry
 *  \param[out] LlcFilterListEntry_T *entry - LLC filter entry in the list
 *  \param[in]  FilterLLC_T *filter - filter data
 *  \return  NA
 */
static void SetLlcFilterData(LlcFilterListEntry_T *entry, FilterLLC_T *filter)
{
    Uint32 sysIfIndex;

    if (filter->LLCIfIndex != DFLTR_IFACE_ALL) 
    {
        /*Translate interface index from docsis numbering to Kernel numbering */
        if (FcUtils_GetSysIfIndexByDocsis(filter->LLCIfIndex, &sysIfIndex) != 0) 
        {
            DPRINTK(KERN_ERR"%s: Failed translate docsis interface index %u to system one\n",__FUNCTION__,filter->LLCIfIndex );
            return;
        }

        filter->LLCIfIndex = sysIfIndex;
    }

    filter->LLCPpCounters.ppMatches = 0;
    filter->LLCPpCounters.activeSessionsList = NULL;

    PrintLlcFltrData(filter);

    PAL_osMemCopy(&(entry->filter), filter, sizeof(FilterLLC_T));
}

/**************************************************************************/
/*! \fn static int SetIpFilterData(IpFilterListEntry_T *entry, FilterIP_T *filter)
 **************************************************************************
 *  \brief Set IP filter data to the list entry
 *  \param[out] IpFilterListEntry_T *entry - IP filter entry in the list
 *  \param[in]  FilterIP_T *filter - filter data
 *  \return  NA
 */
static void SetIpFilterData(IpFilterListEntry_T *entry, FilterIP_T *filter)
{
    Uint32 sysIfIndex;

    switch (filter->IPIfIndex) 
    {
    case DFLTR_IFACE_ALL: /* all interfaces */
        filter->IPIfIndexMask = 0xffffffff;
        break;
    case DFLTR_IFACE_CUSTSIDE_ETH: /* All CMCI interfaces */
        if (FcUtils_GetSysCmciIfBitmap(&filter->IPIfIndexMask) != 0) 
        {
            DPRINTK(KERN_ERR"%s: Failed to get system CMCI interface bitmap\n",__FUNCTION__);
            return;
        }
        break;
    default:
        /*Translate interface index from docsis numbering to Kernel numbering */
        if (FcUtils_GetSysIfIndexByDocsis(filter->IPIfIndex, &sysIfIndex) != 0) 
        {
            DPRINTK(KERN_ERR"%s: Failed to translate docsis interface index %u to system one\n",__FUNCTION__,filter->IPIfIndex );
            return;
        }
        filter->IPIfIndex = sysIfIndex;
        filter->IPIfIndexMask = (1 << (sysIfIndex-1));
    
        break;
    }

    /*Translate action from RFC numbering to local. Treat "Policy(3)" as Accept*/
    filter->IPControl = (filter->IPControl == FLTR_CFG_ACT_DISCARD ? DFLTR_DISCARD : DFLTR_ACCEPT);
    /*Calculate dynamic parameters in advance to improve the performance*/
    filter->IPDAddrMask = filter->IPDaddr & filter->IPDmask;
    filter->IPSAddrMask = filter->IPSaddr & filter->IPSmask;

    filter->IPInPpCounters.ppMatches = 0;
    filter->IPInPpCounters.activeSessionsList = NULL;
    filter->IPOutPpCounters.ppMatches = 0;
    filter->IPOutPpCounters.activeSessionsList = NULL;

    PrintIpFltrData(filter);
    PAL_osMemCopy(&(entry->filter), filter, sizeof(FilterIP_T));
}

/**************************************************************************/
/*! \fn static inline FilterAction_e ApplySTPFilter(EthEncapsType_e enctype, Uint16 proto)
 **************************************************************************
 *  \brief Apply STP Filter
 *  \param[in] EthEncapsType_e enctype - ethernet encapsulation type
 *  \param[in] Uint16 proto - encapsulated protocol type
 *  \return  action to perform on packet : accept/discard
 */
static inline DFltrAction_t ApplySTPFilter(EthEncapsType_e enctype, Uint16 proto)
{
    if ( IsBpduFrame(enctype,proto) )
        return DFLTR_DISCARD;
    else
        return DFLTR_ACCEPT;
}
/*******************************************************************************************************/
/*! \fn static FilterAction_e ApplyLLCFilter(Uint32 srcIfIndex, EthEncapsType_e enctype, Uint16 proto) 
 *******************************************************************************************************
 *  \brief Apply LLC filter given packet LLC header
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in] Uint32 srcIfIndex - index of source interface
 *  \param[in] EthEncapsType_e enctype - ethernet encapsulation type
 *  \param[in] Uint16 proto - encapsulated protocol type
 *  \return  action to perform on packet : accept/discard
 */
static DFltrAction_t ApplyLLCFilter(struct sk_buff *skb, Uint32 srcIfIndex, EthEncapsType_e enctype, Uint16 proto) 
{
   struct list_head *pos;
   Uint32 lockKey;
   LlcFilterListEntry_T *entry;
   FilterLLC_T *llcFilter;


   /* go over the filter table and try to find a match */
   list_for_each(pos, &llcFiltersListHead)
   {
      entry = list_entry(pos, LlcFilterListEntry_T, list);
      llcFilter = &(entry->filter);

      DPRINTK(KERN_DEBUG"llcFilter->LLCIfIndex=%u, srcIfIndex=%u\n", llcFilter->LLCIfIndex, srcIfIndex);

      /* check if the source interface in the filter matches        */
      /* the source interface from which the packet came            */
      if ((llcFilter->LLCIfIndex != DFLTR_IFACE_ALL) &&
          (llcFilter->LLCIfIndex != srcIfIndex)) 
          continue;

      DPRINTK(KERN_DEBUG"llcFilter->LLCProtocolType= %u, enctype=%u\n", llcFilter->LLCProtocolType, enctype);

      /* Verify encapsulated protocol */
      if (enctype == ETH_ENCAP_802_2_3) 
      {
          if (llcFilter->LLCProtocolType != FC_LLC_PROTO_DSAP)
               continue;
      }
      else /*SNAP frames are handled like ethertype frames*/
      {
          if (llcFilter->LLCProtocolType != FC_LLC_PROTO_ETHERTYPE)
            continue;
      }

      DPRINTK(KERN_DEBUG"llcFilter->LLCProtocol= %u, proto=%u\n", llcFilter->LLCProtocol, proto);
    
      if(llcFilter->LLCProtocol != proto)
          continue;

      /* if we are here - we have a match */
      PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
      llcFilter->LLCMatches++;
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
      if (skb->pp_packet_info.ti_pp_flags & TI_PPM_SESSION_INGRESS_RECORDED)
      {
          skb->pp_packet_info.ti_match_llc_filter = llcFilter;   
          DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_llc_filter = %p\n", llcFilter);
      }
#endif
      PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

      DPRINTK(KERN_DEBUG"%s: LLC filter %u matched. Action=%s\n", __FUNCTION__, llcFilter->LLCIndex,
            (llcUnmatchedAction == DFLTR_DISCARD ? "Accept" : "Discard"));
      return (llcUnmatchedAction == DFLTR_DISCARD ? DFLTR_ACCEPT : DFLTR_DISCARD);

   } /* llc filter loop */

   /* no match */
   DPRINTK(KERN_DEBUG"%s: None of LLC filters matched. Action=%s\n", __FUNCTION__, 
         (llcUnmatchedAction == DFLTR_DISCARD ? "Discard" : "Accept"));
   return llcUnmatchedAction;
}

/**************************************************************************/
/*! \fn static FilterAction_e MatchIpFilter(struct iphdr *ipHdr, Uint8 *destMac,
                              FltrBridgeData_s *bridgeData,Uint16 sPort, Uint16 dPort)
 **************************************************************************
 *  \brief Find matching IP filter from a list of active IP filters
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in] struct iphdr *ipHdr - packet IP header
 *  \param[in] Uint8 *destMac - destination MAC
 *  \param[in/out] struct FltrBridgeData_s *bridgeData - bridge data
 *                 Output accepted interfaces bitmap (used for non-unicast packet)
 *  \param[in] Uint32 intfDirection - interface direction: inbound or outbound
 *  \param[in] Uint16 sPort, Uint16 dPort - source and destination ports
 *  \return  action to perform on packet : accept/discard or Uknown if no match found
 */
static DFltrAction_t MatchIpFilter(struct sk_buff *skb, struct PktData_s *pktData, struct FltrBridgeData_s *bridgeData,
                                    Uint32 intfDirection)
{
      Uint32 lockKey;
      struct list_head *pos;
      DFltrAction_t action;
      IpFilterListEntry_T   *entry;
      FilterIP_T *ipFilter;
      struct iphdr *ipHdr = pktData->ipHdr;

      action = DFLTR_UNKNOWN;

      DPRINTK(KERN_DEBUG"%s: Interface direction = 0x%x\n", __FUNCTION__, intfDirection);

      /* go over the filter table and try to find a match */
      list_for_each(pos, &ipFiltersListHead)
      {
         entry = list_entry(pos, IpFilterListEntry_T, list);
         ipFilter = &(entry->filter);

         /* check if the direction/interface in the filter matches the type of filters which is currently applied    */
         if (!FLTR_IP_TRAFF_DIRECTION_MATCH(ipFilter->IPDirection, intfDirection))
               continue;

         /* if this filter is only for broadcasts and multicasts skip if MAC is non-broadast/multicast */
         if ((ipFilter -> IPBroadcast == FLTR_CFG_TRUTH) && (!IS_MAC_MULTICAST(pktData->destMac[0])))
               continue;

         /*  check source + destinaion ip address and protocol type    */
         if (!MatchIPAddr(ipFilter,ntohl(ipHdr->saddr),ntohl(ipHdr->daddr), ipHdr->protocol))
            continue;

         /* check source and destination port numbers if packet is tcp or udp */
         if (pktData->isUdpTcp)
         {
            if (pktData->sPort < ipFilter->IPSourcePortLow)
               continue;
            if (pktData->sPort > ipFilter->IPSourcePortHigh)
               continue;
            if (pktData->dPort < ipFilter->IPDestPortLow)
               continue;
            if (pktData->dPort > ipFilter->IPDestPortHigh)
               continue;
         }

         /* Check TOS */
         if (ipFilter->IPTos != ( ipHdr->tos & ipFilter->IPTosMask) )
            continue;

         /* Verify CMCI interfaces */
         if(intfDirection & FLTR_TRAFF_DIRECTION_INBOUND) 
         {
             
             /*If filter applies to given source interface */
              if ((ipFilter->IPIfIndex != DFLTR_IFACE_ALL) &&
                  (ipFilter->IPIfIndex != DFLTR_IFACE_CUSTSIDE_ETH) &&
                  (ipFilter->IPIfIndex != bridgeData->srcIfIndex)) 
              {
                  continue;
              }
              else if (ipFilter->IPIfIndex == DFLTR_IFACE_CUSTSIDE_ETH) 
              {   /*match if source interface is one of CMCI interfaces*/
                  if ( (ipFilter->IPIfIndexMask & (1 << (bridgeData->srcIfIndex-1))) == 0)
                      continue;
              }
         }
         else /*outbound traffic direction*/
         { 
             /*If filter applies to one interface in the destination interface bitmap */
            if ( (ipFilter->IPIfIndexMask & bridgeData->destIntfBitmap) == 0)
               continue;
            /* Need to be the last operation */

            /* Filter out matched by this filter interface from destination interfaces bitmap if default action is Accept */
            if ((ipDefaultAction == DFLTR_ACCEPT) && (ipFilter->IPControl == DFLTR_DISCARD))
               bridgeData->acceptDestInfBitmap &= ~((ipFilter->IPIfIndexMask) & bridgeData->destIntfBitmap);
            /* Add matched by this filter interface from destination interfaces bitmap
             if default action is Discard */
            if ((ipDefaultAction == DFLTR_DISCARD) && (ipFilter->IPControl != DFLTR_DISCARD))
               bridgeData->acceptDestInfBitmap |= (ipFilter->IPIfIndexMask) & bridgeData->destIntfBitmap;
         }

         /* if we are here - we have a match */
         PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
         ipFilter->IPMatches++;
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
         if (skb->pp_packet_info.ti_pp_flags & TI_PPM_SESSION_INGRESS_RECORDED)
         {
             if(intfDirection & FLTR_TRAFF_DIRECTION_INBOUND) 
             {
                 skb->pp_packet_info.ti_match_inbound_ip_filter = ipFilter;
                 DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_inbound_ip_filter = %p\n", ipFilter);
             }
             else
             {
                 skb->pp_packet_info.ti_match_outbound_ip_filter = ipFilter;
                 DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_outbound_ip_filter = %p\n", ipFilter);
             }
         }
#endif
         PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

         DPRINTK(KERN_DEBUG"%s: IP filter %u matched\n", __FUNCTION__, ipFilter->IPIndex);

         if (ipFilter->IPControl == DFLTR_DISCARD) 
        {
            /* For outbound check if fully discarded for all interfaces, if there are no destination interfaces in bit mask */
            if ((intfDirection & FLTR_TRAFF_DIRECTION_INBOUND) || (bridgeData->acceptDestInfBitmap == 0)) 
            {
                DPRINTK(KERN_DEBUG"%s: Discarding packet for all interfaces\n", __FUNCTION__);
                action = DFLTR_DISCARD;
                break; /*matched*/
            }
            /* Partially discarded. Treat it as no match since it is not for all interfaces in the destination bitmap */
            else
            {
                DPRINTK(KERN_DEBUG"%s: Partially discarded packet for interfaces 0x%.8x. Continuing\n", __FUNCTION__,bridgeData->acceptDestInfBitmap);
                continue;
            }
         } 
         else 
         {
             DPRINTK(KERN_DEBUG"%s: Accepted packet by filter %u for if index %u\n", __FUNCTION__,ipFilter->IPIndex, ipFilter->IPIfIndex);
             action = DFLTR_ACCEPT; 
             /* should we continue */
             if (ipFilter->IPContinue == FLTR_CFG_FALSE) 
             {
                 DPRINTK(KERN_DEBUG"%s: Don't continue to other IP filters\n", __FUNCTION__);
                 break;
             }
             else
                 DPRINTK(KERN_DEBUG"%s: Continuing to other IP filters\n", __FUNCTION__);
         }
      }

      return action;
}

/**************************************************************************/
/*! \fn static int MatchIPAddr(FilterIP_T *ipFilter, Uint32 saddr, Uint32 daddr, Uint8 protocol) 
 **************************************************************************
 *  \brief Check if IP addresses and protocol of packet and filter of given index match
 *  \param[in] FilterIP_T *ipFilter - IP filter
 *  \param[in] Uint32 saddr - source IP address
 *  \param[in] Uint32 daddr - destination IP address
 *  \param[in] Uint8 protocol - IP protocol
 *  \return  action to perform on packet : accept/discard
 */
static int MatchIPAddr(FilterIP_T *ipFilter, Uint32 saddr, Uint32 daddr, Uint8 protocol) 
{
   if ( (ipFilter->IPSAddrMask == (saddr & ipFilter->IPSmask)) &&
        (ipFilter->IPDAddrMask == (daddr & ipFilter->IPDmask)) &&
        ( (ipFilter->IPProtocol == FLTR_IP_PROTO_ANY) || 
          (ipFilter->IPProtocol == protocol) ) ) 
   {
       return 1;
   }

   return 0;
}

/**************************************************************************/
/*! \fn static FilterAction_e ApplyIPFilter(struct iphdr *ipHdr, Uint8 *destMac,
                                            struct FltrBridgeData_s *bridgeData) 
 **************************************************************************
 *  \brief Apply IP filter given packet IP header
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in] struct iphdr *ipHdr - packet IP header
 *  \param[in] struct udphdr *udpTcpHdr - packet UDP/TCP header (first two bytes are the same)
 *  \param[in] Uint8 *destMac - destination MAC
 *  \param[in/out] struct FltrBridgeData_s *bridgeData - bridge data
 *                 Output accepted interfaces bitmap (used for non-unicast packet)
 *  \return  action to perform on packet : accept/discard
 */
static DFltrAction_t ApplyIPFilter(struct sk_buff *skb, struct PktData_s *pktData, struct FltrBridgeData_s *bridgeData)
{
    DFltrAction_t inboundAction, outboundAction;
    struct iphdr *ipHdr = pktData->ipHdr;
    struct udphdr *udpTcpHdr = pktData->udpTcpHdr;

   /* Extract port only if packet is TCP/UDP */
   if ((ipHdr->protocol == IPPROTO_TCP) || (ipHdr->protocol == IPPROTO_UDP))  
   {
       pktData->isUdpTcp = 1;
      /* Source Port */
      pktData->sPort = ntohs(udpTcpHdr->source);
      /* Destination Port */
      pktData->dPort   = ntohs(udpTcpHdr->dest);
   }
   else
   {
        pktData->isUdpTcp = False;
   }
  
   /*Intialize accepted interfaces bitmap in case no filter matches */
   if (ipDefaultAction == DFLTR_ACCEPT)
      bridgeData->acceptDestInfBitmap = bridgeData->destIntfBitmap;
   else
      bridgeData->acceptDestInfBitmap = 0;

   /* Apply first inbound then outbound IP filters */
   inboundAction = MatchIpFilter(skb, pktData, bridgeData, FLTR_TRAFF_DIRECTION_INBOUND);
   if (inboundAction == DFLTR_DISCARD) 
   {
       DPRINTK(KERN_DEBUG"%s: Discarding packet by inbound IP filters\n", __FUNCTION__);
       return DFLTR_DISCARD;
   }

   outboundAction = MatchIpFilter(skb, pktData, bridgeData, FLTR_TRAFF_DIRECTION_OUTBOUND);
   if (outboundAction == DFLTR_DISCARD) 
   {
       DPRINTK(KERN_DEBUG"%s: Discarding packet by outbound IP filters\n", __FUNCTION__);
       return DFLTR_DISCARD;
   }
                                                
   if ( ((inboundAction == DFLTR_UNKNOWN) || (outboundAction == DFLTR_UNKNOWN)) && (ipDefaultAction == DFLTR_DISCARD)) 
   {
       DPRINTK(KERN_DEBUG"%s: None of inbound and/or outbound IP filters matched. Discarding packet by default action\n", __FUNCTION__);
       return DFLTR_DISCARD;
   }
   return DFLTR_ACCEPT;
}
/**************************************************************************/
/*! \fn static inline Bool IsBpduFrame(EthEncapsType_e enctype, Uint16 proto)
 **************************************************************************
 *  \brief Determine if frame is BPDU
 *  \param[out]  EthEncapsType_e enctype - encapsulation type
 *  \param[out]  Uint16 proto - encapsulated protocol type
 *  \return  True/False
 */
static inline int IsBpduFrame(EthEncapsType_e enctype, Uint16 proto)
{
    return ( (enctype == ETH_ENCAP_802_2_3) && (proto == SAP_BPDU) );
}

/**************************************************************************/
/*! \fn static void PrintIpFltrData(FilterIP_T *filter)
 **************************************************************************
 *  \brief Print IP filter data
 *  \param[in] FilterIP_T *ipFilter - IP filter
 *  \return  NA
 */
static void PrintIpFltrData(FilterIP_T *filter)
{
    DPRINTK(KERN_DEBUG"Setting IP filter %u data:\n", filter->IPIndex);
    DPRINTK(KERN_DEBUG"IPControl = %u\n", filter->IPControl);
    DPRINTK(KERN_DEBUG"IPIfIndex = %u\n", filter->IPIfIndex);
    DPRINTK(KERN_DEBUG"IPIfIndexMask = 0x%.8x\n", filter->IPIfIndexMask);
    DPRINTK(KERN_DEBUG"IPDirection = %u\n", filter->IPDirection);
    DPRINTK(KERN_DEBUG"IPBroadcast = %u\n", filter->IPBroadcast);
    DPRINTK(KERN_DEBUG"IPSaddr = 0x%.8x\n", filter->IPSaddr);
    DPRINTK(KERN_DEBUG"IPSmask = 0x%.8x\n",  filter->IPSmask);
    DPRINTK(KERN_DEBUG"IPSAddrMask = 0x%.8x\n", filter->IPSAddrMask);
    DPRINTK(KERN_DEBUG"IPDaddr = 0x%.8x\n", filter->IPDaddr);
    DPRINTK(KERN_DEBUG"IPDmask = 0x%.8x\n", filter->IPDmask);
    DPRINTK(KERN_DEBUG"IPDAddrMask = 0x%.8x\n", filter->IPDAddrMask);
    DPRINTK(KERN_DEBUG"IPProtocol = %u\n", filter->IPProtocol);
    DPRINTK(KERN_DEBUG"IPSourcePortLow = %u\n", filter->IPSourcePortLow);
    DPRINTK(KERN_DEBUG"IPSourcePortHigh = %u\n", filter->IPSourcePortHigh);
    DPRINTK(KERN_DEBUG"IPDestPortLow = %u\n", filter->IPDestPortLow);
    DPRINTK(KERN_DEBUG"IPDestPortHigh= %u\n", filter->IPDestPortHigh);
    DPRINTK(KERN_DEBUG"IPMatches = %u\n", filter->IPMatches);
    DPRINTK(KERN_DEBUG"IPInPpCounters IPPpMatches = %u\n", filter->IPInPpCounters.ppMatches);
    DPRINTK(KERN_DEBUG"IPOutPpCounters IPPpMatches = %u\n", filter->IPOutPpCounters.ppMatches);
    DPRINTK(KERN_DEBUG"IPTos = %u\n", filter->IPTos);
    DPRINTK(KERN_DEBUG"IPTosMask = 0x%x\n", filter->IPTosMask);
    /*padding*/
    DPRINTK(KERN_DEBUG"IPContinue = %u\n", filter->IPContinue);
    DPRINTK(KERN_DEBUG"IPPolicyId = %u\n", filter->IPPolicyId); 
}

/**************************************************************************/
/*! \fn static void PrintLlcFltrData(FilterLLC_T *filter)
 **************************************************************************
 *  \brief Print LLC filter data
 *  \param[in] FilterLLC_T *filter - LLC filter
 *  \return  NA
 */
static void PrintLlcFltrData(FilterLLC_T *filter)
{
    DPRINTK(KERN_DEBUG"Setting LLC filter %u data:\n", filter->LLCIndex);
    DPRINTK(KERN_DEBUG"LLCIfIndex = %u\n", filter->LLCIfIndex);
    DPRINTK(KERN_DEBUG"LLCProtocolType = %u\n", filter->LLCProtocolType);
    DPRINTK(KERN_DEBUG"LLCProtocol = %u\n", filter->LLCProtocol);
    DPRINTK(KERN_DEBUG"LLCMatches = %u\n", filter->LLCMatches);
    DPRINTK(KERN_DEBUG"LLCPpMatches = %u\n", filter->LLCPpCounters.ppMatches);
}
