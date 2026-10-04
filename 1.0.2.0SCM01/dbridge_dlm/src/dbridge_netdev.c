/*
 *
 * dbridge_netdev.c
 * Description:
 * DOCSIS bridge networking implementation
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

#define _DBRIDGE_NERDEV_C_

/*! \file dbridge_netdev.c
    \brief This module manage the docsis net device
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <asm/system.h>
#include "dbridge_netdev.h"
#include "dbridge_db.h"
#include "dbridge_main.h"
#include "dbridge_snif.h"
#include <linux/if_ether.h>
  
/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
#ifdef DBRIDGE_LOG
extern unsigned int dbridgeDbgLevel;
#endif
extern unsigned int ipv4WanInterface_enable;
extern unsigned int ipv6WanInterface_enable;
extern unsigned int ipv4LanInterface_enable;
extern unsigned int ipv6LanInterface_enable;

#include <linux/ti_hil.h>

struct net_device * wanDev =0;
struct net_device * lbr0Dev =0;
static struct net_device * lanDev =0;
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define QOS_INVALID_SF_INDEX -1

static int (*Dbridge_QosClassifierDataCB)(struct sk_buff *skb, int *sfIndex, int *phsIndex) = NULL;
int (*Dbridge_FilterCB)(struct sk_buff *skb, unsigned int destIfMask, unsigned int *acceptDestIfMask, unsigned int pktDirect) = NULL;

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/


/*! \fn int Dbridge_interconnectReceive(struct sk_buff *skb)                                     
 *  \brief Call back function called from the DOCSIS Bridge to push the packet
 *  \brief through the interconnect device back to the LOCAL Bridge.
 *  \param[in] pointer to skb.
 *  \return Always returns 0
 */

static int Dbridge_interconnectReceive(struct sk_buff *skb, unsigned int fc_flags);

/*! \fn int Dbridge_IpStackNetifReceive(struct sk_buff *skb)                                     
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the NET devices to IP stack.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 */
static int Dbridge_IpStackNetifReceive(struct sk_buff *skb, unsigned int fc_flags);

/*! \fn int Dbridge_netifReceive(struct sk_buff *skb)                                     
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the Cable NI
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 */
static int Dbridge_CableNetifReceive(struct sk_buff *skb,unsigned int fc_flags);

/*! \fn int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev)                                     
 *  \brief Registered Transmit function called when a packet is to be transmitted
 *  \brief on to the net device. 
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 */
static int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev);


/*! \fn struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev))                                     
 *  \brief et the network statistics
 *  \param[in] pointer to net_device.
 *  \return Pointer to the network statistics block
 */
static struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev);

/*! \fn int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
 *                                       unsigned int *sfIndex, unsigned int *phsIndex))
 *  \brief register the QOS data CB function
 *  \param[in] the QOS data CB function
 *  \return OK
 */
int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
                                               int *sfIndex, int *phsIndex));

/*! \fn static int Dbridge_DropFltrPacket(struct sk_buff *skb)
 *  \brief Drop the given packet as a result of filtering
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return none
 **************************************************************************/
static void Dbridge_DropFltrPacket(struct sk_buff *skb);

/*! \fn static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb)
 *  \brief Drop the given packet if InterfaceEnableStatus mib is down
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return 1 if packet dropt 
 **************************************************************************/
static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/


/**************************************************************************/
/*! \fn int Dbridge_NetDevInit(void)                                     
 **************************************************************************
 *  \brief DOCSIS net device initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0=OK or error status.
 **************************************************************************/

int Dbridge_NetDevInit(void)
{
    struct net_device *tmp_net_device;

    /******************************************/
    /* register Cable device to docsis bridge */
    /******************************************/


    tmp_net_device=dev_get_by_name (CABLE_DEV_NAME);
    if(tmp_net_device==NULL)
        {
        printk(KERN_INFO "unknown device %s\n",CABLE_DEV_NAME);
        return -1; 
    }
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_CableNetifReceive,DBR_CABLE_NET_DEV_TYPE);

    /**********************************************************************************************************/
    /* create and register lbr0 device to docsis bridge (the conaction between local bridge and docsis bridge)*/
    /**********************************************************************************************************/
    
    tmp_net_device=Dbridge_NetDevCreate(LBRIGE_CONNECTION_DEV_NAME,Dbridge_NetDevSetup);
    if(tmp_net_device==NULL)
        {
        printk(KERN_INFO "unknown device %s\n",LBRIGE_CONNECTION_DEV_NAME);
        return -1; 
    }
    
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_interconnectReceive,DBR_CMCI_NET_DEV_TYPE);
    
    lbr0Dev = tmp_net_device;
    /*****************************************************/
    /* create and register lan0 device to docsis bridge  */
    /*****************************************************/
    tmp_net_device=Dbridge_NetDevCreate(LAN_IP_DEV_NAME,Dbridge_NetDevSetup);

    if(tmp_net_device==NULL)
        {
        printk(KERN_INFO "unknown device %s\n",LAN_IP_DEV_NAME);
        return -1; 
    }
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_IpStackNetifReceive,DBR_LAN_IP_NET_DEV_TYPE);
	lanDev = tmp_net_device;


    /*****************************************************/
    /* create and register wan0 device to docsis bridge  */
    /*****************************************************/
    tmp_net_device=Dbridge_NetDevCreate(WAN_IP_DEV_NAME,Dbridge_NetDevSetup);
    if(tmp_net_device==NULL)
        {
        printk(KERN_INFO "unknown device %s\n",WAN_IP_DEV_NAME);
        return -1; 
    }
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_IpStackNetifReceive,DBR_WAN_IP_NET_DEV_TYPE);
    wanDev = tmp_net_device;

    return 0; /* OK*/
}

/**************************************************************************/
/*! \fn struct net_device* Dbridge_NetDevCreate(void)                                     
 **************************************************************************
 *  \brief alloc and register net device for docsis bridge.
 *  \param[in] device name.
 *  \param[in] device setup function.
 *  \return net_device.
 **************************************************************************/
struct net_device* Dbridge_NetDevCreate(char* name,void (*setup)(struct net_device *))
{
    struct net_device*       ptr_netdev;


    /* Allocate memory for the network device. */
    ptr_netdev = alloc_netdev(sizeof(DbridgeNetDev_t), name, setup);
    if(ptr_netdev == NULL)
    {
        printk (KERN_ERR "Unable to allocate memory for the DOCSIS to Network stack connectivity\n");
        return NULL;
    } 
    /* Register the network device */   
    if (register_netdev(ptr_netdev) < 0)
    {
        printk (KERN_ERR "Unable to register the DOCSIS to Network stack connectivity\n");
        return NULL;
    }

    /* Return the pointer to the network device. */
    return ptr_netdev;
}


/**************************************************************************/
/*! \fn void Dbridge_NetDevSetup(struct net_device *ptr_netdev)                                     
 **************************************************************************
 *  \brief Initialize and override the internal Connection Dev network structure.
 *  \param[in] pointer to Lbridge Connection Device.
 *  \return NONE.
 **************************************************************************/
void Dbridge_NetDevSetup(struct net_device *ptr_netdev)
{
    DbridgeNetDev_t* ptr_docsis_net_dev = ptr_netdev->priv;

    /* Initialize the Ethernet device. */
    ether_setup(ptr_netdev);

    /* Setup the pointers */
    ptr_docsis_net_dev->ptr_device  = ptr_netdev;
    ptr_netdev->hard_start_xmit     = Dbridge_netDeviceXmit;
    ptr_netdev->get_stats           = Dbridge_netDeviceGetStats;

   return;
}


/**************************************************************************/
/*! \fn int Dbridge_interconnectReceive(struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief ICall back function called from the DOCSIS Bridge to push the packet
 *  \brief through the interconnect device back to the LOCAL Bridge.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0
 **************************************************************************/
static int Dbridge_interconnectReceive(struct sk_buff *skb, unsigned int fc_flags)
{
    DbridgeNetDev_t* priv;

	if(fc_flags & DBR_FILTER_FLAG)
	{
		if(Dbridge_FilterCB)
		{
			unsigned int acceptPacket;
		    unsigned int acceptDestIfMask;
       		
			acceptPacket = Dbridge_FilterCB(skb,skb->ti_selective_fwd_dev_info,&acceptDestIfMask,DFLTR_FROM_CABLE);
		    if(acceptPacket)
			{
				skb->ti_selective_fwd_dev_info = acceptDestIfMask;
			}
			else
			{
                Dbridge_DropFltrPacket(skb);
				return 0;
			}

		}
	}

    dbridgeAddOutFrames(skb);
    /* Get the DOCSIS network device private area */
    priv = netdev_priv(skb->dev);

    if (!netif_running(skb->dev))
    {
        priv->stats.rx_dropped++;
        kfree_skb (skb);
        return 0;
    }

    /* Increment the statistics. */
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += skb->len;
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_IF)
    {
        printk("\nDBRIDGE_DBG_REG_TX_IF TO %s",skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif

    /* Pull the Ethernet MAC Address before passing it to the stack. */
    skb_pull(skb,ETH_HLEN);

    /* Push the packe to the networking stacks for the bridge to receive it */
    netif_receive_skb(skb);
    return 0;
}

/**************************************************************************/
/*! \fn int Dbridge_IpStackNetifReceive(struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the NET devices to IP stack.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 **************************************************************************/
static int Dbridge_IpStackNetifReceive(struct sk_buff *skb, unsigned int fc_flags)
{
    DbridgeNetDev_t* priv;
    struct ethhdr*  ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;

    /* Initialize the protocol. */

    ptr_ethhdr   = (struct ethhdr *)skb->mac.raw;
    skb->protocol = ptr_ethhdr->h_proto;
    skb->ti_meta_info = 0;

#ifdef CONFIG_TI_IP_PKTINFO_SOCKOPT
    /* packet dest is WAN0 */
    ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_source);
	if (ptr_dbridge_alt!=NULL)
    {
        if ( NULL != ptr_dbridge_alt->dbridgeDevice && 
             DBR_CMCI_NET_DEV_TYPE == ptr_dbridge_alt->dbridgeDevice->net_dev_type )
        {
            /* packet source  is CMCI */
            skb->ti_meta_info = 1;
        }
    }
#endif

    skb->input_dev = skb->dev;
	
	/* If the packet is not IP, IPv6, ARP or LLC increase unsupported counter and drop the packet */
	if(!((skb->protocol == ETH_P_IP) || (skb->protocol == ETH_P_IPV6) || 
		 (skb->protocol == ETH_P_ARP) || (skb->protocol <= ETH_DATA_LEN)))
	{
		/* If the packet is not IP, IPv6, ARP or LLC increase unsupported counter and drop the packet */
		dbridgeAddUnsupps(skb);
		kfree_skb (skb);
		return 0;
	}

    dbridgeAddOutFrames(skb);

    /* Get the DOCSIS network device private area */
    priv = netdev_priv(skb->dev);

    if (!netif_running(skb->dev))
    {
        priv->stats.rx_dropped++;
        kfree_skb (skb);
        return 0;
    }
    /* Increment the statistics. */
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += skb->len;

	if(1 == Dbridge_InterfaceEnableStatusHandler(skb))
	{
		return 0;
	}
    
    /* Set the flag if the packet was meant for us; this is called from the DOCSIS
     * Bridge and we know for sure that the skb->mac.raw points to the start of the
     * Ethernet frame. So we check the Destination MAC address of the packet with
     * the MAC Address of the network device and set the flag. This is required else
     * the packet will be dropped in the IP stack. This is the same that is done in the
     * eth_type_trans code. The reason why I did not call the function again is because
     * I want to preserve the original "input_dev" and not have it overwritten. */
    if (memcmp(priv->ptr_device->dev_addr, skb->mac.raw, ETH_ALEN) == 0)
        skb->pkt_type = PACKET_HOST;

    #ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_IF)
    {
        printk("\nDBRIDGE_DBG_REG_TX_IF TO %s",skb->dev->name);
        DBridge_logPacket(skb);
    }
    #endif
    /* Pull the Ethernet MAC Address before passing it to the stack. */
    skb_pull(skb,ETH_HLEN);

    /* Push the packet to the IP stack. */
    netif_receive_skb(skb);   

    return 0;
} 

/**************************************************************************/
/*! \fn int Dbridge_netifReceive(struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the Cable NI
 *  \param[in] filter and classifier flags.
 *  \param[in] pointer to skb.
 *  \return Always returns 0.
 **************************************************************************/
static int Dbridge_CableNetifReceive(struct sk_buff *skb, unsigned int fc_flags)
{

    int sfIndex;
    int phsIndex;

    if(fc_flags & DBR_FILTER_FLAG)
    {
        if(Dbridge_FilterCB)
        {
            unsigned int acceptPacket;
            unsigned int acceptDestIfMask;

            acceptPacket = Dbridge_FilterCB(skb,skb->ti_selective_fwd_dev_info,&acceptDestIfMask,DFLTR_TO_CABLE);
            if(acceptPacket)
 			{
				skb->ti_selective_fwd_dev_info = acceptDestIfMask;
			}
			else
			{
                Dbridge_DropFltrPacket(skb);
                return 0;
            }
        }
    }

    skb->ti_meta_info |= 0x00FF0000;
    if(fc_flags&DBR_CLASSIFIER_FLAG)
    {
        if(Dbridge_QosClassifierDataCB)
        {   /* call QOS Classifier */
            int temp_meta_info =skb->ti_meta_info;
            Dbridge_QosClassifierDataCB(skb, &sfIndex, &phsIndex);
            
            if (sfIndex != QOS_INVALID_SF_INDEX) /*QoS classifier matched*/
            {
            /* set the meta data info */
            temp_meta_info &= 0x0000FFFF; 
            temp_meta_info |= ((sfIndex << 24) & 0xFF000000);
            temp_meta_info |= ((phsIndex << 16) & 0x00FF0000);
            skb->ti_meta_info = temp_meta_info;
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_CLASSIFIER)
            {
                printk("\nsfIndex = %x, phsIndex= %x, skb->ti_meta_info = %x\n",sfIndex, phsIndex,skb->ti_meta_info );
            }
#endif
        }
            else /*Upstream Drop Classifier matched*/
            {
                Dbridge_DropFltrPacket(skb);
                return 0;
            }
        }
    }

    dbridgeAddOutFrames(skb);

    dev_queue_xmit(skb);
    return 0;
}

/**************************************************************************/
/*! \fn int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev)                                     
 **************************************************************************
 *  \brief Registered Transmit function called when a packet is to be transmitted
 *  \brief on to the net device. 
 *  \param[in] pointer to skb.
 *  \return Always returns 0.
 **************************************************************************/
 int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev)
{
    DbridgeNetDev_t* priv;

	if(1 == Dbridge_InterfaceEnableStatusHandler(skb))
	{
		return 0;
    }

    /* Get the DOCSIS network device private area */

    priv = netdev_priv(skb->dev);

    if (!netif_running(skb->dev))
    {
        priv->stats.tx_dropped++;
        kfree_skb (skb);
        return 0;
    }

    /* Increment the statistics. */
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    /* Initialize the data pointers before passing the packet down to the 
     * DOCSIS Bridge. */
    skb->mac.raw = skb->data;

    /* Push the packet to the DOCSIS Bridge. */
    DBridge_receive (skb);

    return 0;
}

/**************************************************************************/
/*! \fn struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev))                                     
 **************************************************************************
 *  \brief get the network statistics
 *  \param[in] pointer to net_device.
 *  \return Pointer to the network statistics block
 **************************************************************************/
static struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev)
{
    DbridgeNetDev_t* ptr_docsis_net_dev;

    /* Get the DOCSIS Local Interconnect Information*/
    ptr_docsis_net_dev = (DbridgeNetDev_t *)ptr_netdev->priv;

    /* Return the network statistics. */
    return &ptr_docsis_net_dev->stats;
}

/**************************************************************************/
/*! \fn int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
 *                                       unsigned int *sfIndex, unsigned int *phsIndex))
 **************************************************************************
 *  \brief register the QOS data CB function
 *  \param[in] the QOS data CB function
 *  \return OK
 **************************************************************************/
int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
                                              int *sfIndex, int *phsIndex))
{   
    unsigned long flags;

    local_irq_save(flags);
    Dbridge_QosClassifierDataCB = Dbridge_QosClassifierData;
	local_irq_restore(flags);

    return 0;
}

/**************************************************************************/
/*! \fn int Dbridge_register_FilterCB(int (*Dbridge_Filter)(struct sk_buff *skb, unsigned int destIfMask, 
 *                                                  unsigned int acceptDestIfMask, unsigned int pktDirect))
 **************************************************************************
 *  \brief register the QOS data CB function
 *  \param[in] the filter CB function
 *  \return OK
 **************************************************************************/
int Dbridge_register_FilterCB(int (*Dbridge_Filter)(struct sk_buff *skb, unsigned int destIfMask, 
                                                    unsigned int *acceptDestIfMask, unsigned int pktDirect))
{   
    unsigned long flags;

    local_irq_save(flags);
    Dbridge_FilterCB = Dbridge_Filter;
    local_irq_restore(flags);

    return 0;
}

/**************************************************************************/
/*! \fn static int Dbridge_DropFltrPacket(struct sk_buff *skb)
 **************************************************************************
 *  \brief Drop the given packet as a result of filtering
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return none
 **************************************************************************/
static void Dbridge_DropFltrPacket(struct sk_buff *skb)
{

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel &  DBRIDGE_DBG_FILTER)
    {
        printk("\nFilter drop packet");
        DBridge_logPacket(skb);
    }
#endif

    ti_hil_pp_event (TI_DOCSIS_FLTR_DISCARD_PKT, (void *)skb);
    
    dbridgeAddFilterDropPacket(skb);
    kfree_skb (skb);
}

/**************************************************************************
 *! \fn static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb)
 **************************************************************************
 *  \brief Drop the given packet if InterfaceEnableStatus mib is down
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return 1 if packet dropt 
 **************************************************************************/
static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb)
{
	struct ethhdr*     ptr_ethhdr=NULL;

    int       interfaceEnableStatus =1;

	/* Get the Ethernet header. */    
    if(skb->mac.raw)
	{
		ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
	}

	if(ptr_ethhdr)
	{
			
		if((skb->dev == lanDev) && (ptr_ethhdr->h_proto == ETH_P_IP) && (!ipv4LanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
		else if((skb->dev == wanDev) && (ptr_ethhdr->h_proto == ETH_P_IP) && (!ipv4WanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
		else if((skb->dev == wanDev) && (ptr_ethhdr->h_proto == ETH_P_IPV6) && (!ipv6WanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
		else if((skb->dev == lanDev) && (ptr_ethhdr->h_proto == ETH_P_IPV6) && (!ipv6LanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
	}
	
	if(0 == interfaceEnableStatus)
	{
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel)
    {
        printk("\n Drop packet IPv4 or IPv6IP Interface %s Disable",skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif
		kfree_skb (skb);
		return 1;
	}
	return 0;
}

EXPORT_SYMBOL(Dbridge_register_QosClassifierDataCB); 
EXPORT_SYMBOL(Dbridge_register_FilterCB); 
EXPORT_SYMBOL(wanDev); 
EXPORT_SYMBOL(lbr0Dev); 



