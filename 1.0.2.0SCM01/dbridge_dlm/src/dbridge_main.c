/*
 *
 * dbridge_main.c
 * Description:
 * DOCSIS bridge main implementation
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

/***********************************************************************
 Portions Copyright (C) 2008 Motorola Incorporated - http://www.motorola.com
*********************************************************************/
#define _DBRIDGE_MAIN_C_

/*! \file dbridge_main.c
    \brief the docsis bridge main module
*/
/**************************************************************************/
/*      architecture overview                                             */
/**************************************************************************/


/*                                       ------------------------------
                                        |                              |
                                        |        Dual IP stack         |
                                        |                              |
                                         ------------------------------
                                           |           |            |
                                           |           |            |
                                         ------      ------      ------   
                                    	|      |  	|      |    |      |   
                                    	| WAN0 |	| LAN0 |    | MTA0 |   
	                                    |      |	|      |    |      |   
                                         ------      ------      ------    
                                           |           |            |
                                           |           |            |
    --------------     -------     ------------------------------   |
   |              |   |       |   |                              |  |
   | Local Bridgr |---| DBR00 |---|        Docsis Bridge         |  |
   |              |   |       |   |                              |  |
    --------------     -------     ------------------------------   |
       |       |                       |              |             |
       |       |                       |              |             |
    ------   ------                  ------   -------------------   |
   |      | |      |               	|      | |                   |  |
   | ETH0 |	| USB0 |	            | CNI0 | |   Esafe adaptor   |--
   |      |	|      |             	|      | |                   |
    ------   ------                  ------  |-------------------
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/if_arp.h>        
#include <linux/route.h>	  
#include <linux/udp.h>           
#include "dbridge_main.h"
#include "dbridge_db.h"
#include "dbridge_netdev.h"
#include "dbridge_igmp.h"
#include "dbridge_snif.h"
#include "dbridge_esafe.h"
#include "dbridge_mcast.h"
#include "dbridge_mdf_db.h"

#include "dbridge_frwd_rules.h"

/* work around for multicast counters */
#define IFINDEX_PRIMARY_CPE             1

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
#ifdef DBRIDGE_LOG
extern unsigned int dbridgeDbgLevel;
#endif
 
#ifdef CONFIG_TI_DEVICE_PROTOCOL_HANDLING
extern int ti_protocol_handler (struct net_device* dev, struct sk_buff *skb);
#endif

extern struct net_device * lbr0Dev;


/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
/*! \def MAC_ISBROADCAST( pa )
 *  \brief if broadcast return true , else false
 */
#define MAC_ISBROADCAST( pa, hw ) ( ~0xFF ==( (~(pa)->hw[0]) | \
                                              (~(pa)->hw[1]) | \
                                              (~(pa)->hw[2]) | \
                                              (~(pa)->hw[3]) | \
                                              (~(pa)->hw[4]) | \
                                              (~(pa)->hw[5]) ))

/*! \def MAC_ISMULTICAST( pa )
 *  \brief if the multicast macaddress return true , else false
 */
#define MULTICAST_MASK 0x01    
#define MAC_ISMULTICAST( pa, field )  ( ((pa)->field[0]) & MULTICAST_MASK )

#define FLOOD_TO_ALL_INTERFACE 0xffffffff
/* #define YAMUNA_MUSTER */

                      
/*! \var typedef enum McasrType_e
    \brief enum defines the Mcast type
*/
typedef enum McasrType_e
{
    NOT_MCAST,
    MCAST_IGMP,
    MCAST_DATA,
} McasrType_e;

#define CNID_DSID_INDEX_BIT_FIELD_OFFSET 24

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

static DbridgeCounters_t*  ptr_dbridge_counters;
 

/*! \fn void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,unsigned,
 *  \fn                           unsigned int filter_map,unsigned int classifier_map,
 *  \fn                           unsigned int alt_update_map)
 *  \brief floods the packet flowing the input parameters rules 
 *  \param[in] skb.
 *  \param[in] flood_map = map the destanation index the packet send to.
 *  \param[in] filter_map = map on which destanation index to perform filter.
 *  \param[in] filter_map = map on which destanation index to perform classifier.
 *  \return 0-Success, <0-Error.
 */
static void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,
								 unsigned int filter_map,unsigned int classifier_map);

/*! \fn void DBridge_registered_receive(struct sk_buff* skb)                                     
 *  \brief The function handle the packet according to Docsis spec 
 *  \brief for registered mode  
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 */
static void DBridge_registered_receive(struct sk_buff* skb);

/*! \fn void DDBridge_pre_registration_receive(struct sk_buff* skb)                                     
 *  \brief The function handle the packet according to Docsis spec 
 *  \brief for pre-registration and naco 0 mode.  
 *  \param[in] skb.
 *  \param[in] pointer to  Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_pre_registration_receive(struct sk_buff* skb);

/*! \fn void DBridge_standby_receive(struct sk_buff* skb)                                     
 *  \brief The function handle the packet according to Docsis spec 
 *  \brief for standby mode.  
 *  \param[in] skb.
 *  \param[in] pointer to  Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_standby_receive(struct sk_buff* skb);


/*! \fn void DBridge_regHandlePacketFromCmci(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 *  \brief The function handle the packet from CMCI according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromCMCI(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 *  \brief The function handle the packet from eSAFE according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 *  \brief The function handle the packet from cable according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 *  \brief The function handle the packet from Lan IP according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 *  \brief The function handle the packet from Wan IP according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromWanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev)                                    
 *  \brief   verify if this is Mcast handle the packet and returne the Mcast type 
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[in] mcast_filter_type.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 */
static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, 
										   unsigned int flood_map, unsigned int mcast_filter_type);

/*! \fn static int DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, 
                                       unsigned int flood_map)                                
 *  \brief verify if this is Mcast packet and handle the packet according to MDF logic.
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 */
static int  DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, 
                                unsigned int flood_map);

#ifdef CONFIG_TI_DOCSIS_EGRESS_HOOK
static int Dbridge_egress_hook(struct sk_buff* skb);
static struct net_device *usb0_netdev;
static struct net_device *eth0_netdev;

#endif /* CONFIG_TI_DOCSIS_EGRESS_HOOK */

static void mot_fixDestMACfromCMCI(struct sk_buff* skb);  
void mot_Dbridge_AddRoute(char *devName, unsigned int dest);  

void Dbridge_SendIgmpToCable(struct sk_buff* skb);
void Dbridge_SendIgmpToCpe(struct sk_buff* skb);

static int mot_DiscardCmTftpPktToCpeIP(struct sk_buff* skb);
int mot_CmTftpSrcPortNumInUse(unsigned int uiUdpSrcPort);
extern int mot_GetCmTftpPort();

extern int mot_GetCmFactoryMode(void);  
#define FACTORY_ACCESS_LEVEL 1


/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/



/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/
/* TODO Change to cfg*/
#define IGMP_DEFAULT_MQI_TIMEOUT        125    
#define IGMP_DEFAULT_QRI_TIMEOUT        100
#define IGMP_M1_MAX_TIMEOUT             3
#define IGMP_DOCSIS_ROBUSTNESS          2


/**************************************************************************/
/*! \fn int Dbridge_Init(void)                                     
 **************************************************************************
 *  \brief DOCSIS bridge dinitialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int Dbridge_Init(void)
{
    struct igmp_cfg igmp_cfg;

	if (DbridgeDb_Init())
		return -1; /* NOK*/
	
    if (DbridgeEsafe_Init())
		return -1; /* NOK*/

    if (Dbridge_NetDevInit())
		return -1; /* NOK*/

    if (DbridgeMdfDb_Init())
		return -1; /* NOK*/

	igmp_cfg.enabled=0;
	igmp_cfg.igmp_docsis_robustness = IGMP_DOCSIS_ROBUSTNESS;
	igmp_cfg.igmp_m1_max_timeout =IGMP_M1_MAX_TIMEOUT;
	igmp_cfg.igmp_mqi_timeout=IGMP_DEFAULT_MQI_TIMEOUT;
	igmp_cfg.igmp_qri_timeout=IGMP_DEFAULT_QRI_TIMEOUT;
    igmp_cfg.ti_igmp_send_packet=(void*)Dbridge_SendIgmpPacket;
    igmp_cfg.ti_igmp_message_processed= Dbridge_messageProcessed;
    if (ti_igmp_init(&igmp_cfg))
	    return -1; /* NOK*/

#ifdef CONFIG_TI_DOCSIS_EGRESS_HOOK
	/* Install the Ingress Hook. */
	{
		extern int ti_register_docsis_egress_hook_handler (struct net_device* dev, int (*docsis_egress_hook)(struct sk_buff *skb));

		eth0_netdev= dev_get_by_name (ETH_DEV_NAME);
		if(eth0_netdev)
		{
			if (ti_register_docsis_egress_hook_handler (eth0_netdev, Dbridge_egress_hook) == 0)
				printk ("Ingress Hook on  %s \n",eth0_netdev->name);
		}
		usb0_netdev= dev_get_by_name (USB_DEV_NAME);
		if(usb0_netdev)
		{
			if (ti_register_docsis_egress_hook_handler (usb0_netdev, Dbridge_egress_hook) == 0)
				printk ("Ingress Hook on  %s \n",eth0_netdev->name);
		}
	}
#endif /* CONFIG_TI_DOCSIS_EGRESS_HOOK */
	
	/* IGMP init */
    ptr_dbridge_counters= Dbridge_GetCounters();    
	return 0; /* OK*/
}




/**************************************************************************/
/*! \fn void DBridge_receive (struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief The function is the entry point by which packets are pushed into  
 *  \brief the DOCSIS Bridge. 
 *  \brief NOTE: Before calling this function ensure that the MAC RAW Pointer in the 
 *  \brief SKB is valid and points to the start of the MAC header.
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
void DBridge_receive (struct sk_buff *skb)
{
    struct ethhdr*     ptr_ethhdr;
    unsigned int dbridge_mode;

	/* Get the Ethernet header. */    
    ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
    if (ptr_ethhdr == NULL)
    {
        /* No Ethernet header; drop the packet and clean the memory. */
#ifdef DBRIDGE_DBG
        printk (KERN_INFO "Debuge : No Ethernet header; dropping packet\n");
#endif
		dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }

	/* in some cases the inpute_dev is null, docsis bridge use this information */
	/* therefore in these case we set the input_dev to be equal to dev */
	if(skb->input_dev == NULL)
		skb->input_dev = skb->dev;
    
#ifdef CONFIG_TI_DEVICE_PROTOCOL_HANDLING
    /* Pass the packet to the device specific protocol handler */
    if (ti_protocol_handler (skb->dev, skb) < 0)
    {
        /* Device Specific Protocol handler has "captured" the packet
         * and does not want to send it up the networking stack; so 
         * return immediately. */
        //cable_pp: don't do it, return NET_RX_SUCCESS;
    }
#endif /* CONFIG_TI_DEVICE_PROTOCOL_HANDLING */
	dbridge_mode = DbridgeDb_GetMode(); 

	switch (dbridge_mode)
	{
	case DBR_OFF_MODE:
		dbridgeAddDiscards(skb);
#ifdef DBRIDGE_DBG
		printk (KERN_INFO "Docsis bridge off - drop  Packet 0x%p\n",skb);
#endif
		kfree_skb (skb);
		return;
	case DBR_REGISTERED_MODE: 
		DBridge_registered_receive(skb);
		break;
    case DBR_PRE_REGISTRATION_MODE:
	case DBR_NACO_OFF_MODE: 
		DBridge_pre_registration_receive(skb);
		break;
	case DBR_STANDBY_MODE:
		DBridge_standby_receive(skb);
		break;
	default:
    	return;
	}
	return;
}





/**************************************************************************/
/*! \fn void Dbridge_CableRecive (struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief  
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
void Dbridge_CableRecive (struct sk_buff *skb)
{
	   /* This is the DOCSIS Interface; pass the packet to the DOCSIS Bridge; initialize the
		* MAC RAW Pointer before doing so. */


	   skb->mac.raw = skb->data;
	   DBridge_receive (skb);

}
/**************************************************************************/
/*! \fn void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,unsigned,
 *  \fn                            unsigned int filter_map,unsigned int classifier_map,
 *  \fn                            unsigned int alt_update_map, DbridgeDevice_t* ptr_dbridge_src_dev)
 **************************************************************************
 *  \brief floods the packet flowing the input parameters rules 
 *  \param[in] skb.
 *  \param[in] flood_map = map the destanation index the packet send to.
 *  \param[in] filter_map = map on which destanation index to perform filter.
 *  \param[in] filter_map = map on which destanation index to perform classifier.
 *  \return 0-Success, <0-Error.
 **************************************************************************/
static void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,
								 unsigned int filter_map, unsigned int classifier_map)
{
	struct sk_buff* skbclone;
    int    index = 0;
    int    num_of_device = 0;
    DbridgeDevice_t*   ptr_dbridge_dev;
	
	num_of_device=DbridgeDb_GetNumOfDevice();
    /* Cycle through all the entries. */
	for (index = 0; index < num_of_device; index++)
	{
		
		ptr_dbridge_dev = DbridgeDb_DevGetByIndex(index);
		/* Do we get the packet on this interface? */
		if (skb->dev == ptr_dbridge_dev->ptr_interface)
			continue;
        /* clone the packet onlt to map interface */
		if (ptr_dbridge_dev->net_dev_type & flood_map)
		{
    		unsigned int fc_flags=DBR_NO_FILTER_CLASS_FLAG;
			/* Clone the packet. */
		    skbclone = skb_clone(skb, GFP_ATOMIC);
			if (skbclone == NULL)
			{
				/* Memory Problems: Clean the original SKB before leaving. */
#ifdef DBRIDGE_DBG
				printk (KERN_ERR "Cloning Failed for Device %s\n", ptr_dbridge_dev->ptr_interface->name);
#endif
				dbridgeAddDiscards(skb);
				kfree_skb(skb);
				return;
			}
			/* Initialize the packet structure to indicate the device on which the packet will be transmitted */
			
			skbclone->dev = ptr_dbridge_dev->ptr_interface;

				/* we filter only packet from cable or CMCI */
			if(ptr_dbridge_dev->net_dev_type & filter_map)
			{
				if(ptr_dbridge_dev->net_dev_type == DBR_CMCI_NET_DEV_TYPE)
				{       
					/* the destination is CMCI */ 
					if(skbclone->ti_selective_fwd_dev_info ==0)
						skbclone->ti_selective_fwd_dev_info = DbridgeDB_GetlbridgeInterfaceMask();
				}
				else
				{
					/* the destination is Cable */
					 skbclone->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skbclone->dev->ifindex)-1));
				}
				fc_flags |= DBR_FILTER_FLAG;

			}

			if (ptr_dbridge_dev->net_dev_type & classifier_map)
			{
				fc_flags |= DBR_CLASSIFIER_FLAG;
			}
			
			
			/* Push the packet via the DOCSIS Device */
#ifdef DBRIDGE_LOG
			if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_FLOOD)
			{
				printk("\nDBRIDGE_DBG_REG_TX_FLOOD FROM %s TO %s",skb->dev->name, skbclone->dev->name);
				DBridge_logPacket(skbclone);
			}
#endif
			
			ptr_dbridge_dev->push(skbclone,fc_flags);
			
			/* update counters */
			
		}
	}
    /* Cleanup the original packet. */
    kfree_skb (skb);
    return;
}


/**************************************************************************/
/*! \fn void DBridge_registered_receive(struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet according to Docsis spec 
 *  \brief for registered mode  
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_registered_receive(struct sk_buff* skb)
{
	
	struct ethhdr*     ptr_ethhdr;
	DbridgeDevice_t*   ptr_dbridge_dev;
	
    unsigned int       src_dev_type;
	
#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RECIVE)
	{ 
		printk("\nDBRIDGE_DBG_REG_RECIVE");
		DBridge_logPacket(skb);
	}
#endif
	
	/* Get the Ethernet header. */    
	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
	
	/* Get the DOCSIS Device information. */
	ptr_dbridge_dev = DbridgeDb_DevGetByInterface (skb->dev);
	if (ptr_dbridge_dev == NULL)
	{
		/* Packet was passed from a device not attached to the DOCSIS Bridge. */ 
#ifdef DBRIDGE_DBG
		printk (KERN_WARNING "No DOCSIS Device; packet from %s\n", skb->dev->name);
#endif
		dbridgeAddDiscards(skb);
		kfree_skb (skb);
		return;
	}
	
	src_dev_type = ptr_dbridge_dev->net_dev_type;
	
	switch(src_dev_type)
	{
	case DBR_CABLE_NET_DEV_TYPE:
		DBridge_regHandlePacketFromCable(ptr_dbridge_dev,skb);
		break; 
	case DBR_CMCI_NET_DEV_TYPE:
		DBridge_regHandlePacketFromCMCI(ptr_dbridge_dev,skb);
		break;
	case DBR_ESAFE_NET_DEV_TYPE:
		DBridge_regHandlePacketFromESAFE(ptr_dbridge_dev,skb);
		break; 
		
	case DBR_WAN_IP_NET_DEV_TYPE:
		DBridge_regHandlePacketFromWanIP(ptr_dbridge_dev,skb);
		break; 
	case DBR_LAN_IP_NET_DEV_TYPE:
#ifndef YAMUNA_MUSTER
		DBridge_regHandlePacketFromLanIP(ptr_dbridge_dev,skb);
#else
		/* patch to transfer packet from lan to cable for master slave protocol */
		DBridge_regHandlePacketFromWanIP(ptr_dbridge_dev,skb);
#endif
		break; 
	default:
		break;
	}
	return;
}


/**************************************************************************/
/*! \fn void DDBridge_pre_registration_receive(struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet according to Docsis spec 
 *  \brief for pre-registration and naco0 mode  
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_pre_registration_receive(struct sk_buff* skb)
{
	struct ethhdr*     ptr_ethhdr;
	DbridgeDevice_t*   ptr_dbridge_dev;
	DbridgeAltEntry_t* ptr_dbridge_alt;
    unsigned int       src_dev_type  = 0;
	unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = DBR_WAN_IP_NET_DEV_TYPE;
	unsigned int       Mcast_type;
    unsigned int       mdfMode = 0;	
	unsigned int uiFactoryMode = 0; 

	/* Get the Ethernet header. */    
	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RECIVE)
	{ 
		printk("\n DBRIDGE_DBG_PRE_REG_RECIVE");
		DBridge_logPacket(skb);
	}
#endif

	/* Get the DOCSIS Device information. */
	ptr_dbridge_dev = DbridgeDb_DevGetByInterface (skb->dev);
	if (ptr_dbridge_dev == NULL)
	{
		/* Packet was passed from a device not attached to the DOCSIS Bridge. */ 
#ifdef DBRIDGE_DBG
		printk (KERN_WARNING "1No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
		dbridgeAddDiscards(skb);
		kfree_skb (skb);
		return;
	}
	src_dev_type = ptr_dbridge_dev->net_dev_type;

	switch (src_dev_type)
	{
	case DBR_CABLE_NET_DEV_TYPE:
#ifdef DBRIDGE_LOG
	
	if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
	{
		printk("\nDBRIDGE_DBG_PREREG_RX_CABLE");
		DBridge_logPacket(skb);
	}
#endif
		 

	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
	if( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {   /* brodcast or Mcast packet */
        if (!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
		{
            /* if MDF is enabled */
            DbridgeMdfDb_GetMdfMode(&mdfMode);
            if (mdfMode == DBR_MDF_ENABLE_MODE)
            {
                Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_dev, flood_map);
    #ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk("\nDBridge_pre_registration_receive: DBRIDGE MDF Mode is enabled\n");
                }
    #endif    				
                if (Mcast_type == MCAST_DATA)
                {
                    return;
                }
            }

            /* if MDF is disabled */
            else
            {
    			/*     Mcast filtering */
    			if(DbridgeDb_GetMcastByMac(ptr_ethhdr->h_dest) == NULL)
    			{
    				/* Unknown destination or illegal destination */
    #ifdef DBRIDGE_LOG
    				if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                    {
    					printk (KERN_WARNING "No Filter Drop Mcast packet from %s\n", skb->dev->name);
    					DBridge_logPacket(skb);
    				}
    #endif
    				dbridgeAddDiscards(skb);
    				kfree_skb (skb);
    				return;
    			}
            }
		}
	}

    /* at this stage alt table can point to LAN_IP or WAN_IP only */
		ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
		if (ptr_dbridge_alt != NULL)
		{ 
			if(ptr_dbridge_alt->dbridgeDevice != NULL)
			{
				/* Match Found: Send the packet only on that interface */
				dbridgeAddInFrames(skb);
				dest_dev_type = ptr_dbridge_alt->dbridgeDevice->net_dev_type;
				skb->dev = ptr_dbridge_alt->dbridgeDevice->ptr_interface;

				ptr_dbridge_alt->dbridgeDevice->push(skb,DBR_NO_FILTER_CLASS_FLAG);
				/* update counter */
				return;
			}
		}
        /* No Match found; flood the packet to LAN and WAN ip interface */
		dbridgeAddInFrames(skb);
	    Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);        
		return;

        case DBR_CMCI_NET_DEV_TYPE:
/*  
  The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
   HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from the CMCI ports;
*/
    if( apply_fwrd_rule( NULL, TI_FWRD_RULE_FROM_CPE, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
     {
#ifdef DBRIDGE_DBG
		 if(dbridgeDbgLevel)
		 {
			 printk ("CM Pre-Operational: The CM MUST NOT accept this packet from the CMCI ports\n");
			 DBridge_logPacket(skb);
		 }
#endif
        dbridgeAddForwardDiscards(skb);
		dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;   
     }
        /* drop to the next case */
	case DBR_ESAFE_NET_DEV_TYPE:
		/* at this stage alt table can point to LAN_IP or WAN_IP only */
		ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
		if (ptr_dbridge_alt != NULL)
		{ 
			if(ptr_dbridge_alt->dbridgeDevice != NULL)
			{
				/* Match Found: Send the packet only on that interface */
				dbridgeAddInFrames(skb);
				dest_dev_type = ptr_dbridge_alt->dbridgeDevice->net_dev_type;
				skb->dev = ptr_dbridge_alt->dbridgeDevice->ptr_interface;

				ptr_dbridge_alt->dbridgeDevice->push(skb,DBR_NO_FILTER_CLASS_FLAG);
				/* update counter */
				return;
			}
		}
        /* No Match found; flood the packet to LAN and WAN ip interface */
		dbridgeAddInFrames(skb);
	    Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);        
		return;

	case DBR_WAN_IP_NET_DEV_TYPE:
		
		/* send the packet to cable interface only */
		ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
		dbridgeAddInFrames(skb);

		skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
		if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
		{
			printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","wan0",skb->dev->name);
			DBridge_logPacket(skb);
		}
#endif				
		ptr_dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);			
		
		return;

	case DBR_LAN_IP_NET_DEV_TYPE:
	  uiFactoryMode = mot_GetCmFactoryMode();
    	  if(uiFactoryMode == FACTORY_ACCESS_LEVEL)
    	  {
                /* CM operating in Factory Mode Do not apply any rules */
          }
          else
          {

   	/* send the packet to CMCI interface only */
/*  
   The CM MUST NOT send any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or Request,
   TFTP-RRQ, HTTP Request, Time Protocol Request, or IPv6 Router Solicitation messages to any interface
   except the RF Interface;
*/      
        if( apply_fwrd_rule( NULL, TI_FWRD_RULE_IPSTACK_TO_RF, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb) )
         {
#ifdef DBRIDGE_DBG
			 if(dbridgeDbgLevel)
			 {
				 printk ("CM Pre-Operational: The CM MUST NOT send this messages to any interface except the RF Interface\n");
				 DBridge_logPacket(skb);
			 }
#endif
            dbridgeAddForwardDiscards(skb);
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
         }
	} 
		ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
		dbridgeAddInFrames(skb);

		skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
		if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
		{
			printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","lan0",skb->dev->name);
			DBridge_logPacket(skb);
		}
#endif				
		ptr_dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);			


		return;

	default:
		return;
	} 
}


/**************************************************************************/
/*! \fn void DBridge_standby_receive(struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet according to Docsis spec 
 *  \brief for standby mode  
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_standby_receive(struct sk_buff* skb)
{
	struct ethhdr*     ptr_ethhdr;
	DbridgeDevice_t*   ptr_dbridge_dev;
	DbridgeAltEntry_t* ptr_dbridge_alt;
    unsigned int       src_dev_type  = 0;
	unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = DBR_WAN_IP_NET_DEV_TYPE;
	unsigned int       Mcast_type;
    unsigned int       mdfMode = 0;	

	/* Get the Ethernet header. */    
	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RECIVE)
	{ 
		printk("\n DBRIDGE_DBG_PRE_REG_RECIVE");
		DBridge_logPacket(skb);
	}
#endif

	/* Get the DOCSIS Device information. */
	ptr_dbridge_dev = DbridgeDb_DevGetByInterface (skb->dev);
	if (ptr_dbridge_dev == NULL)
	{
		/* Packet was passed from a device not attached to the DOCSIS Bridge. */ 
#ifdef DBRIDGE_DBG
		printk (KERN_WARNING "1No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
		dbridgeAddDiscards(skb);
		kfree_skb (skb);
		return;
	}
	src_dev_type = ptr_dbridge_dev->net_dev_type;

	switch (src_dev_type)
	{
	case DBR_CABLE_NET_DEV_TYPE:
#ifdef DBRIDGE_LOG
	
	if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
	{
		printk("\nDBRIDGE_DBG_PREREG_RX_CABLE");
		DBridge_logPacket(skb);
	}
#endif
		 

	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
	if( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {   /* brodcast or Mcast packet */
        if (!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
		{
            /* if MDF is enabled */
            DbridgeMdfDb_GetMdfMode(&mdfMode);
            if (mdfMode == DBR_MDF_ENABLE_MODE)
            {
                Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_dev, flood_map);
    #ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk("\nDBridge_pre_registration_receive: DBRIDGE MDF Mode is enabled\n");
                }
    #endif    				
                if (Mcast_type == MCAST_DATA)
                {
                    return;
                }
            }

            /* if MDF is disabled */
            else
            {
    			/*     Mcast filtering */
    			if(DbridgeDb_GetMcastByMac(ptr_ethhdr->h_dest) == NULL)
    			{
    				/* Unknown destination or illegal destination */
    #ifdef DBRIDGE_LOG
    				if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                    {
    					printk (KERN_WARNING "No Filter Drop Mcast packet from %s\n", skb->dev->name);
    					DBridge_logPacket(skb);
    				}
    #endif
    				dbridgeAddDiscards(skb);
    				kfree_skb (skb);
    				return;
    			}
            }
		}
	}

		ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
		if (ptr_dbridge_alt != NULL)
		{ 
			if(ptr_dbridge_alt->dbridgeDevice != NULL)
			{
				
				dest_dev_type = ptr_dbridge_alt->dbridgeDevice->net_dev_type;
			/* in standby mode we forword packet from cable IF to LAN or WAN IF only*/
				if((DBR_WAN_IP_NET_DEV_TYPE == dest_dev_type) || (DBR_LAN_IP_NET_DEV_TYPE == dest_dev_type))
				{
					dbridgeAddInFrames(skb);
					skb->dev = ptr_dbridge_alt->dbridgeDevice->ptr_interface;
					ptr_dbridge_alt->dbridgeDevice->push(skb,DBR_NO_FILTER_CLASS_FLAG);
				}
				else
				{
					dbridgeAddDiscards(skb);
					kfree_skb (skb);
				}
				return;
			}
		}
        /* No Match found; flood the packet to LAN and WAN ip interface */
		dbridgeAddInFrames(skb);
	    Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);        
		return;

        case DBR_CMCI_NET_DEV_TYPE:
/*  
  The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
   HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from the CMCI ports;
*/
    if( apply_fwrd_rule( NULL, TI_FWRD_RULE_FROM_CPE, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
     {
#ifdef DBRIDGE_DBG
		 if(dbridgeDbgLevel)
		 {
			 printk ("CM Pre-Operational: The CM MUST NOT accept this packet from the CMCI ports\n");
			 DBridge_logPacket(skb);
		 }
#endif
        dbridgeAddForwardDiscards(skb);
		dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;   
     }
        /* drop to the next case */
	case DBR_ESAFE_NET_DEV_TYPE:
		ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
		if (ptr_dbridge_alt != NULL)
		{ 
			if(ptr_dbridge_alt->dbridgeDevice != NULL)
			{
				dest_dev_type = ptr_dbridge_alt->dbridgeDevice->net_dev_type;
				/* in standby mode we forword packet from CMCI or ESAFE IF to LAN or WAN IF only*/
				if((DBR_WAN_IP_NET_DEV_TYPE == dest_dev_type) || (DBR_LAN_IP_NET_DEV_TYPE == dest_dev_type))
				{
					dbridgeAddInFrames(skb);
					skb->dev = ptr_dbridge_alt->dbridgeDevice->ptr_interface;

					ptr_dbridge_alt->dbridgeDevice->push(skb,DBR_NO_FILTER_CLASS_FLAG);
				}	
				else
				{
					dbridgeAddDiscards(skb);
					kfree_skb (skb);
				}
				return;
			}
		}
        /* No Match found; flood the packet to LAN and WAN ip interface */
		dbridgeAddInFrames(skb);
	    Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);        
		return;

	case DBR_WAN_IP_NET_DEV_TYPE:
		
		/* send the packet to cable interface only */
		ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
		dbridgeAddInFrames(skb);

		skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
		if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
		{
			printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","wan0",skb->dev->name);
			DBridge_logPacket(skb);
		}
#endif				
		ptr_dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);			
		
		return;

	case DBR_LAN_IP_NET_DEV_TYPE:
   	/* send the packet to CMCI interface only */
/*  
   The CM MUST NOT send any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or Request,
   TFTP-RRQ, HTTP Request, Time Protocol Request, or IPv6 Router Solicitation messages to any interface
   except the RF Interface;
*/      
        if( apply_fwrd_rule( NULL, TI_FWRD_RULE_IPSTACK_TO_RF, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb) )
         {
#ifdef DBRIDGE_DBG
			 if(dbridgeDbgLevel)
			 {
				 printk ("CM Pre-Operational: The CM MUST NOT send this messages to any interface except the RF Interface\n");
				 DBridge_logPacket(skb);
			 }
#endif
            dbridgeAddForwardDiscards(skb);
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
         }
		ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
		dbridgeAddInFrames(skb);

		skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
		if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
		{
			printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","lan0",skb->dev->name);
			DBridge_logPacket(skb);
		}
#endif				
		ptr_dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);			


		return;

	default:
		return;
	} 
}


/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromCmci(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet from CMCI according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
**************************************************************************/
static void DBridge_regHandlePacketFromCMCI(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
	/* check packet ip and perform mac override */
	
	
	struct ethhdr*     ptr_ethhdr;
	DbridgeAltEntry_t* ptr_dbridge_alt;
    DbridgeDevice_t*   ptr_dbridge_dest_dev;
	unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = FLOOD_TO_ALL_INTERFACE;
    unsigned int       Mcast_type = NOT_MCAST;
    unsigned int       mdfMode = 0;
    bool               rf_only = false;

#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_CMCI)
	{
		printk("\nDBRIDGE_DBG_REG_RX_CMCI");
		DBridge_logPacket(skb);
	}
#endif


    mot_fixDestMACfromCMCI(skb);
	/********************************/
	/* Lookup destanation interface */
	/********************************/
	
	/* Get the Ethernet header. */    
	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
	if ( ptr_ethhdr->h_dest[0] & 0x1 )
	{
/*  
  The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
   HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
*/  
        rf_only = apply_fwrd_rule( NULL, TI_FWRD_RULE_IPSTACK_TO_RF, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb);
		/* Update ALT table  */
		if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->input_dev)!=0)
		{
      		/* CPE table is full don't flood the packet to cable interface */
#ifdef DBRIDGE_DBG
			printk (KERN_INFO "CPE table is full don't flood the packet to cable interface %s\n", skb->dev->name);
#endif
			if(!rf_only)
				flood_map ^= DBR_CABLE_NET_DEV_TYPE;
			else
			{
				if(dbridgeDbgLevel)
					{
					printk ("CM Operational: The CM MUST NOT send this messages to any interface except the RF Interface\n");
					DBridge_logPacket(skb);
				}

				dbridgeAddForwardDiscards(skb);
				dbridgeAddDiscards(skb);
				kfree_skb (skb);
				return;
			}
		}

        DbridgeMdfDb_GetMdfMode(&mdfMode);
        /* if MDF is enabled */
        if (mdfMode == DBR_MDF_ENABLE_MODE)
        {
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                printk("\nDBridge_regHandlePacketFromCMCI: DBRIDGE MDF Mode is enabled\n");
            }
#endif
            //Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_src_dev, flood_map);
            if(rf_only)
				flood_map = DBR_CABLE_NET_DEV_TYPE;
			else
				flood_map = DBR_CABLE_NET_DEV_TYPE | DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE;
			dbridgeAddInFrames(skb);
			Dbridge_floodPacket(skb, flood_map, flood_map ,flood_map);
            Mcast_type = MCAST_DATA;
        }

        /* if MDF is disabled */
        else
        {
            /* viserify if we need to handle this IGMP packet */    
            Mcast_type = DBridge_Mcast_handler(skb,ptr_dbridge_src_dev,flood_map, MCAST_FILTER_NOT_ACTIVE);
        }

		if (Mcast_type==NOT_MCAST)
		{
			/* broadcast packet */
            if(rf_only)
				flood_map = DBR_CABLE_NET_DEV_TYPE;
			dbridgeAddInFrames(skb);
			Dbridge_floodPacket(skb,flood_map,DBR_CABLE_NET_DEV_TYPE ,DBR_CABLE_NET_DEV_TYPE);
		}
		
		return;
		
	}

	ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
	if (ptr_dbridge_alt != NULL)
	{
		dest_dev_type = ptr_dbridge_alt->dbridgeDevice->net_dev_type;
		switch (dest_dev_type)
		{
		case DBR_CABLE_NET_DEV_TYPE:
			
			break; 
		case DBR_ESAFE_NET_DEV_TYPE:
			
			break; 
		case DBR_WAN_IP_NET_DEV_TYPE: 
        case DBR_LAN_IP_NET_DEV_TYPE:
			/*  
		    The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
		    HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
			*/  
			if ( apply_fwrd_rule( NULL, TI_FWRD_RULE_FROM_CPE, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
				{
				#ifdef DBRIDGE_DBG
	    		if(dbridgeDbgLevel)
				{
					printk ("CM Operational: The CM MUST NOT accept this packet from any of the CPE ports\n");
					DBridge_logPacket(skb);
				}
				#endif
				dbridgeAddForwardDiscards(skb);
				dbridgeAddDiscards(skb);
				kfree_skb (skb);
				return;
			}
    
			break; 
		default:
#ifdef DBRIDGE_LOG
			printk (KERN_WARNING "2No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
			dbridgeAddDiscards(skb);
			kfree_skb (skb);
			return;
		}
		
	} else
	{
		/* Unknown destination we send the packet to CABLE interface */
		
		/* performer filter - ToDo */
		/* performer classifier - ToDo */
		
		/* Update ALT table with src MAC address */
		if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->input_dev)!=0)
		{
#ifdef DBRIDGE_DBG
			printk (KERN_INFO "CPE table if full drop %s\n", skb->dev->name);
#endif
			dbridgeAddDiscards(skb);
			kfree_skb (skb);
			return;
		}
		
		ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
        dbridgeAddInFrames(skb);

		skb->dev = ptr_dbridge_dest_dev->ptr_interface;
		skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skb->dev->ifindex)-1)); 

#ifdef DBRIDGE_LOG
		if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
		{
			printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
			DBridge_logPacket(skb);
		}
#endif
		
		ptr_dbridge_dest_dev->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
		return;
	}
	
	dbridgeAddInFrames(skb);
	skb->dev = ptr_dbridge_alt->dbridgeDevice->ptr_interface;
	skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skb->dev->ifindex)-1)); 

#ifdef DBRIDGE_LOG	
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
    {
        printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif
    ptr_dbridge_alt->dbridgeDevice->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
    return;
}
/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet from eSAFE according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
	/* check packet ip and perform mac override */
	struct ethhdr*     ptr_ethhdr;
	DbridgeAltEntry_t* ptr_dbridge_alt;
    DbridgeDevice_t* ptr_dbridge_dest_dev;
	unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = FLOOD_TO_ALL_INTERFACE  ^ (DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE);
    unsigned int       Mcast_type;
    unsigned int       mdfMode = 0;

#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_CMCI)
	{
		printk("\nDBRIDGE_DBG_REG_RX_CMCI");
		DBridge_logPacket(skb);
	}
#endif
	
	/********************************/
	/* Lookup destanation interface */
	/********************************/

	/* Get the Ethernet header. */    
	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;

	if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
	{
		/* Update ALT table  */
		if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->input_dev)!=0)
		{
			/* CPE table is full don't flood the packet to cable interface */
			flood_map ^= DBR_CABLE_NET_DEV_TYPE;
#ifdef DBRIDGE_DBG
			printk (KERN_INFO "CPE table is full don't flood the packet to cable interface %s\n", skb->dev->name);
#endif			
		}

        DbridgeMdfDb_GetMdfMode(&mdfMode);
        /* if MDF is enabled */
        if (mdfMode == DBR_MDF_ENABLE_MODE)
        {
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                printk("\nDBridge_regHandlePacketFromESAFE: DBRIDGE MDF Mode is enabled\n");
            }
#endif
            //Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_src_dev, flood_map);
            flood_map = DBR_CABLE_NET_DEV_TYPE;
			dbridgeAddInFrames(skb);
			Dbridge_floodPacket(skb, flood_map, DBR_CABLE_NET_DEV_TYPE ,DBR_CABLE_NET_DEV_TYPE);
            Mcast_type = MCAST_DATA;
        }

        /* if MDF is disabled */
        else
        {
            /* viserify if we need to handle this IGMP packet */
            Mcast_type = DBridge_Mcast_handler(skb,ptr_dbridge_src_dev,flood_map,MCAST_FILTER_NOT_ACTIVE);
        }

		if (Mcast_type==NOT_MCAST)
		{
			/* broadcast packet */
			dbridgeAddEsafeInFrames(skb);
			Dbridge_floodPacket(skb, flood_map, DBR_CABLE_NET_DEV_TYPE , DBR_CABLE_NET_DEV_TYPE);
		}
		
		return;
		
	}
   
	ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
	if (ptr_dbridge_alt != NULL)
	{
		dest_dev_type = ptr_dbridge_alt->dbridgeDevice->net_dev_type;
		switch (dest_dev_type)
		{
		case DBR_CMCI_NET_DEV_TYPE:	
		case DBR_CABLE_NET_DEV_TYPE:
			
			break; 
		case DBR_ESAFE_NET_DEV_TYPE:
			
			break; 
		case DBR_WAN_IP_NET_DEV_TYPE: 
		case DBR_LAN_IP_NET_DEV_TYPE:
			/*  
		    The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
		    HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
			*/  
			if ( apply_fwrd_rule( NULL, TI_FWRD_RULE_FROM_CPE, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
				{
				#ifdef DBRIDGE_DBG
	    		if(dbridgeDbgLevel)
				{
					printk ("CM Operational: The CM MUST NOT accept this packet from any of the CPE ports\n");
					DBridge_logPacket(skb);
				}
				#endif
				dbridgeAddForwardDiscards(skb);
				dbridgeAddDiscards(skb);
				kfree_skb (skb);
				return;
			}
   			break; 
		default:
#ifdef DBRIDGE_DBG
			printk (KERN_WARNING "2No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
			dbridgeAddDiscards(skb);
			kfree_skb (skb);
			return;
		}
		
	} else
	{
		/* Unknown destination we send the packet to CABLE interface */
		
		/* performer filter - ToDo */
		/* performer classifier - ToDo */
		
		/* Update ALT table with src MAC address */
		if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->input_dev)!=0)
		{
#ifdef DBRIDGE_DBG
			printk (KERN_INFO "CPE table if full drop %s\n", skb->dev->name);
#endif
			dbridgeAddDiscards(skb);
			kfree_skb (skb);
			return;
		}
		
		ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
        dbridgeAddEsafeInFrames(skb);
		skb->dev = ptr_dbridge_dest_dev->ptr_interface;
		skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skb->dev->ifindex)-1)); 
#ifdef DBRIDGE_LOG
		if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
		{
			printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
			DBridge_logPacket(skb);
		}
#endif
		
		ptr_dbridge_dest_dev->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
		return;
	}
	
	dbridgeAddEsafeInFrames(skb);
	skb->dev = ptr_dbridge_alt->dbridgeDevice->ptr_interface;
	if(ptr_dbridge_alt->sourceInterface)
	{
		skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((ptr_dbridge_alt->sourceInterface->ifindex)-1));
	}
	else
	{
		skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skb->dev->ifindex)-1));
	}
#ifdef DBRIDGE_LOG	
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
	{
		printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
		DBridge_logPacket(skb);
	}
#endif
	ptr_dbridge_alt->dbridgeDevice->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
	return;
}
/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet from cable according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
**************************************************************************/
static void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
	
	struct ethhdr*     ptr_ethhdr;
	DbridgeAltEntry_t* ptr_dbridge_alt;
    struct net_device*  ptr_dest_interface;
    unsigned int dest_dev_type;
	unsigned int Mcast_type;
	unsigned int  mcast_filter_type = MCAST_FILTER_NOT_ACTIVE;
	DbridgeMcastMac_t* mcast_filter_info;
	unsigned int fc_flags = DBR_NO_FILTER_CLASS_FLAG;
    unsigned int flood_map = FLOOD_TO_ALL_INTERFACE;
    unsigned int mdfMode = 0;
	int iRetValue = 0; 

#ifdef DBRIDGE_LOG
	
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_CABLE)
	{
		printk("\nDBRIDGE_DBG_REG_RX_CABLE");
		DBridge_logPacket(skb);
	}
#endif
	
	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
	if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
	{

        /* if MDF is enabled */
        DbridgeMdfDb_GetMdfMode(&mdfMode);
        if (mdfMode == DBR_MDF_ENABLE_MODE)
        {
            Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_src_dev, flood_map);
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                printk("\nDBridge_regHandlePacketFromCable: DBRIDGE MDF Mode is enabled\n");
            }
#endif
        }

        /* if MDF is disabled */
        else
        {
            if (!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
            {
                /*     Mcast filtering */
                mcast_filter_info = DbridgeDb_GetMcastByMac(ptr_ethhdr->h_dest);
                if(mcast_filter_info == NULL)
                {
                    /* Unknown destination or illegal destination */
				    #ifdef DBRIDGE_LOG
                    if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                    {				
                        printk (KERN_WARNING "No Filter Drop Mcast packet from %s\n", skb->dev->name);
                        DBridge_logPacket(skb);
                    }
				    #endif

                    dbridgeAddDiscards(skb);
                    kfree_skb (skb);
                    return;
                }
                else
                {
                    mcast_filter_type = mcast_filter_info->Type;
                }
            }
        
            /* if the packet is Mcase the rest of the handling done by DBridge_Mcast_handler */
            Mcast_type = DBridge_Mcast_handler(skb,ptr_dbridge_src_dev,flood_map,mcast_filter_type);
        }

		if (Mcast_type==NOT_MCAST)
		{
			/* Flood the packet to all interfaces and perform filter for CMCI and ESAFE . */
			dbridgeAddInFrames(skb);
			Dbridge_floodPacket(skb,FLOOD_TO_ALL_INTERFACE, DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE,
				DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE);		
		} 
		
		return;
	}
		iRetValue = mot_DiscardCmTftpPktToCpeIP(skb);
		if(0 == iRetValue)
        	{
            		dbridgeAddDiscards(skb);
	    		kfree_skb (skb);
            		return;

        	}

	/* Unicast Packets. Check the ALT Table for a match on the destination MAC Address */
	ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
	if (ptr_dbridge_alt != NULL)
		{
	        if(ptr_dbridge_alt->dbridgeDevice == NULL)
			{
			   /* flood the packet to all CPE (esafe and cmci) */
				dbridgeAddInFrames(skb);
				Dbridge_floodPacket(skb,DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE, DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE,
					DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE);	
				return;
			}
			ptr_dest_interface = ptr_dbridge_alt->dbridgeDevice->ptr_interface;
			dest_dev_type=ptr_dbridge_alt->dbridgeDevice->net_dev_type;
#ifndef YAMUNA_MUSTER
			if(dest_dev_type!=DBR_LAN_IP_NET_DEV_TYPE)
				{
#endif
				dbridgeAddInFrames(skb);

			/* the destanation is wan ip, cmci or esafe */
			/* Match Found: Send the packet only on that interface */
			if (dest_dev_type==DBR_CMCI_NET_DEV_TYPE || dest_dev_type==DBR_ESAFE_NET_DEV_TYPE)
			{
			   fc_flags |= DBR_FILTER_FLAG;
            }

			skb->dev = ptr_dest_interface;
			if(ptr_dbridge_alt->sourceInterface)
			{
				skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((ptr_dbridge_alt->sourceInterface->ifindex)-1));
			}
			else
			{
				skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skb->dev->ifindex)-1));
			}
			
#ifdef DBRIDGE_LOG
			if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
			{				
				printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
				DBridge_logPacket(skb);
			}
#endif

			
			
			ptr_dbridge_alt->dbridgeDevice->push(skb,fc_flags);
#ifndef YAMUNA_MUSTER
		}
#endif	
	} else
	{
		/* Unknown destination or illegal destination */
#ifdef DBRIDGE_DBG
		printk (KERN_WARNING "3No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
		dbridgeAddDiscards(skb);
		kfree_skb (skb);
		return;
	}
	
	
}
/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet from Lan IP according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
    DbridgeDevice_t* ptr_dbridge_dest_dev;
    unsigned int uiFactoryMode = 0; 	
#ifdef DBRIDGE_LOG
	
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_LANIP)
	{
		
		printk("\nDBRIDGE_DBG_REG_RX_LANIP");
		DBridge_logPacket(skb);
	}
#endif
    uiFactoryMode = mot_GetCmFactoryMode();  
    if(uiFactoryMode == FACTORY_ACCESS_LEVEL)
    {
		/* CM operating in Factory Mode Do not apply any rules */
        
    }
    else
    {
/*  
  		The CM MUST NOT forward any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or
		Request, TFTP-RRQ, HTTP Request, Time Protocol Request, or Router Solicitation messages
		to any ports except for Rf port 
	
/*  
  The CM MUST NOT forward any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or
    Request, TFTP-RRQ, HTTP Request, Time Protocol Request, or Router Solicitation messages to any ports
    except the RF port.
*/   
    if ( apply_fwrd_rule( NULL, TI_FWRD_RULE_IPSTACK_TO_RF, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb) )
    {
#ifdef DBRIDGE_DBG
		if(dbridgeDbgLevel)
		{
			printk (KERN_INFO "CM Operational: The CM MUST NOT forward this messages to any interface except the RF Interface\n");
			DBridge_logPacket(skb);
		}
#endif
		dbridgeAddForwardDiscards(skb);
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }
}	
	
	/* send the packet to CMCI interface only */
	ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
	dbridgeAddInFrames(skb);
	
	skb->dev = ptr_dbridge_dest_dev->ptr_interface;
#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
	{
		printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
		DBridge_logPacket(skb);
	}
#endif				
	ptr_dbridge_dest_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);			
}


/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)                                     
 **************************************************************************
 *  \brief The function handle the packet from Wan IP according to Docsis spec  
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_regHandlePacketFromWanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
	/* lookup destination interface */
	struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    DbridgeDevice_t*   ptr_dbridge_dest_dev;
    unsigned int       flood_map = DBR_CABLE_NET_DEV_TYPE | DBR_CMCI_NET_DEV_TYPE;
    unsigned int       mdfMode = 0;
    unsigned int       fwrd_map = FLOOD_TO_ALL_INTERFACE;
    bool               rf_only = false;
#ifdef DBRIDGE_LOG

   if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_WANIP)
   {
	   printk("\nDBRIDGE_DBG_REG_RX_WANIP");
	   DBridge_logPacket(skb);
   }
#endif

/*  
  The CM MUST NOT forward any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or
    Request, TFTP-RRQ, HTTP Request, Time Protocol Request, or Router Solicitation messages to any ports
    except the RF port.
*/   
     if( (rf_only = apply_fwrd_rule( NULL, TI_FWRD_RULE_IPSTACK_TO_RF, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb)) )
         fwrd_map = DBR_CABLE_NET_DEV_TYPE;

	/* Get the Ethernet header. */    
	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;

	if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
	{

        DbridgeMdfDb_GetMdfMode(&mdfMode);
        /* if MDF is enabled */
        if (mdfMode == DBR_MDF_ENABLE_MODE)
        {
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                printk("\nDBridge_regHandlePacketFromWanIP: DBRIDGE MDF Mode is enabled\n");
            }
#endif
			dbridgeAddInFrames(skb);
			Dbridge_floodPacket(skb, flood_map, 0 ,DBR_CABLE_NET_DEV_TYPE);
        }

        /* if MDF is disabled */
        else
        {
    		/* broadcast or multicast, bit40 of MAC address is set */
    		/* Flood the packet on all allowed interfaces. */
    		dbridgeAddInFrames(skb);
    		Dbridge_floodPacket(skb, fwrd_map, 0, 0);
        }

		return;
	}

	/* Unicast Packets. Look up for destination interface */
	/* Check the ALT Table for a match on the destination MAC Address */
	ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
	if ( rf_only || ptr_dbridge_alt == NULL )
	{
		/* No Match found; Send the packet to Cable interface.*/
		ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
	} else
	{
		/* Match Found: Send the packet only on that interface */
		ptr_dbridge_dest_dev = ptr_dbridge_alt->dbridgeDevice;
	}
	dbridgeAddInFrames(skb);        
	
	skb->dev = ptr_dbridge_dest_dev->ptr_interface;
	
#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
	{  
		printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
		DBridge_logPacket(skb);
	}
#endif

    ptr_dbridge_dest_dev->push(skb,DBR_CLASSIFIER_FLAG);
	
}

/**************************************************************************/
/*! \fn void dbridgeAddInFrames(struct sk_buff* skb);                                    
 **************************************************************************
 *  \brief   increase input packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
 void dbridgeAddInFrames(struct sk_buff* skb)
 {
	 unsigned int dev_index;
	 struct ethhdr* ptr_ethhdr;

	 if(skb)
		 {
		 dev_index = skb->input_dev->ifindex;
		 ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
		 ptr_dbridge_counters->DevCounters[dev_index].In++;
		 if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
		 {   /*If this is unicast packet - increment unicast counter as well*/
			 ptr_dbridge_counters->DevCounters[dev_index].inUcast++;
		 } else
	     {   /* If this is broadcast/multicast packet - increment broadcast/multicast counter as well*/
			 if( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
				 ptr_dbridge_counters->DevCounters[dev_index].inBcast++ ;
             else
				 ptr_dbridge_counters->DevCounters[dev_index].inMcast++ ;
		 }
	 }
}
/**************************************************************************/
/*! \fn void dbridgeAddEsafeInFrames(struct sk_buff* skb);                                    
 **************************************************************************
 *  \brief   increase input packet from Esafe, in case of esafe we take 
 *           the ifindex from dev instead of input_dev;   
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
 void dbridgeAddEsafeInFrames(struct sk_buff* skb)
 {
	 unsigned int dev_index;
	 struct ethhdr* ptr_ethhdr;

	 if(skb)
	 {
		 dev_index = skb->dev->ifindex;
		 ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
		 ptr_dbridge_counters->DevCounters[dev_index].In++;
		 if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
		 {   /*If this is unicast packet - increment unicast counter as well*/
			 ptr_dbridge_counters->DevCounters[dev_index].inUcast++;
		 } else
		 {   /* If this is broadcast/multicast packet - increment broadcast/multicast counter as well*/
		   if( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
			  ptr_dbridge_counters->DevCounters[dev_index].inBcast++ ;
           else
              ptr_dbridge_counters->DevCounters[dev_index].inMcast++ ;    
		 }
	 }
 }

/**************************************************************************/
/*! \fn void dbridgeAddOutFrames(struct sk_buff* skb);                                    
 **************************************************************************
 *  \brief   increase output packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
 void dbridgeAddOutFrames(struct sk_buff* skb)  
 {
	 unsigned int dest_dev_index = 0;
	 unsigned int tmp_selective_fwd = 0;
	 struct ethhdr*     ptr_ethhdr;
	 DbridgeAltEntry_t* ptr_dbridge_alt;

	 if(skb)
		 {
		 dest_dev_index = skb->dev->ifindex;
		 /* if the dest is CMCI we need to find the  finale destanation device index, we have this information in ALT table */
		 ptr_ethhdr = (struct ethhdr *)skb->mac.raw;

		 ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
		 if (ptr_dbridge_alt) 
		 {
			 if(ptr_dbridge_alt->dbridgeDevice)
             {
				 if(ptr_dbridge_alt->dbridgeDevice->net_dev_type == DBR_CMCI_NET_DEV_TYPE)
				 {
					 if (ptr_dbridge_alt->sourceInterface)
						 {
						 dest_dev_index = ptr_dbridge_alt->sourceInterface->ifindex;
					 }
				 }
			 }
		 }
		 if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
		 {  /*If this is unicast packet - increment unicast counter as well*/
			 ptr_dbridge_counters->DevCounters[dest_dev_index].outUcast++;
			 ptr_dbridge_counters->DevCounters[dest_dev_index].Out++;
		 } 
		 else
		 {
			 if(skb->dev == lbr0Dev && skb->ti_selective_fwd_dev_info)
			 {
				 /* in case packet send to local bridge we have to update relevant interfaces 
				    only if the bit in ti_selective_fwd_dev_info is set */
				 dest_dev_index =0;
				 tmp_selective_fwd = skb->ti_selective_fwd_dev_info;
				 while(tmp_selective_fwd)
				 {
					 dest_dev_index++;
					 if(tmp_selective_fwd & 0x1)
					 {
						 ptr_dbridge_counters->DevCounters[dest_dev_index].Out++;
						 if( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
							 ptr_dbridge_counters->DevCounters[dest_dev_index].outBcast++ ;
						 else
							 ptr_dbridge_counters->DevCounters[dest_dev_index].outMcast++;
					 }
					 tmp_selective_fwd = tmp_selective_fwd >> 1;
				 }
			 }
			 else
			 {
				 if( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
					 ptr_dbridge_counters->DevCounters[dest_dev_index].outBcast++ ;
				 else
					 ptr_dbridge_counters->DevCounters[dest_dev_index].outMcast++;
				 ptr_dbridge_counters->DevCounters[dest_dev_index].Out++;
			 }
		 }
	 }
 }

/**************************************************************************/
/*! \fn void dbridgeAddDiscards(struct sk_buff* skb);                                    
 **************************************************************************
 *  \brief   increase discard  packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddDiscards(struct sk_buff* skb)   
{
	unsigned int dev_index;
	if(skb)
	{
		dev_index = skb->input_dev->ifindex;
	    ptr_dbridge_counters->DevCounters[dev_index].Discard++;
	}
}

/**************************************************************************/
/*! \fn void dbridgeAddForwardDiscards(struct sk_buff* skb);                                    
 **************************************************************************
 *  \brief   increase forward discard  packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddForwardDiscards(struct sk_buff* skb)   
{
	unsigned int dev_index;
	if(skb)
	{
		dev_index = skb->input_dev->ifindex;
	    ptr_dbridge_counters->DevCounters[dev_index].FwrdRuleDrop++;
	}
}

/**************************************************************************/
/*! \fn void dbridgeAddUnsupps(struct sk_buff* skb);                                    
 **************************************************************************
 *  \brief   increase Unsupps  packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddUnsupps(struct sk_buff* skb)
{
	unsigned int dev_index;
	if(skb)
	{
		dev_index = skb->input_dev->ifindex;
	    ptr_dbridge_counters->DevCounters[dev_index].UnSupport++;
	    ptr_dbridge_counters->DevCounters[dev_index].In--;
	}
}
/**************************************************************************/
/*! \fn void dbridgeAddFilterDropPacket(struct sk_buff* skb);                                    
 **************************************************************************
 *  \brief   increase filter drop packet    
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddFilterDropPacket(struct sk_buff* skb)
{
	unsigned int dev_index;
	if(skb)
	{
		dev_index = skb->input_dev->ifindex;
	    ptr_dbridge_counters->DevCounters[dev_index].FilterDrop++;
	}
}

/**************************************************************************/
/*! \fn static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev)                                    
 **************************************************************************
 *  \brief   viserify if this is Mcast handle the packet and returne the Mcast type 
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[in] mcast_filter_type.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 **************************************************************************/
static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, 
										   unsigned int flood_map, unsigned int mcast_filter_type)
{
   struct ethhdr*     ptr_ethhdr;
   DbridgeAltEntry_t* ptr_dbridge_alt; 
   unsigned int cpe_num =0;
   unsigned int fc_flags = DBR_FILTER_FLAG;

	ptr_ethhdr = (struct ethhdr *)skb->mac.raw;
    if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) ) 
	{
		if (!ptr_ethhdr->h_dest[1] && (ptr_ethhdr->h_dest[2] & 0x5e))
			{
			struct iphdr    *iph = (struct iphdr *) (((char *)skb->mac.raw) + ETH_HLEN);
			if (iph != NULL)
				{
				if (iph->protocol == IPPROTO_IGMP)
				{
#ifdef DBRIDGE_LOG
					if(dbridgeDbgLevel & DBRIDGE_DBG_REG_IGMP)
					{				
						printk ("\nIGMP Mcast packet from %s to igmp module\n", skb->dev->name);
						DBridge_logPacket(skb);
					}
#endif
					
					ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_source);
					if (ptr_dbridge_alt!=NULL)
					{
						cpe_num=ptr_dbridge_alt->cpe_number;
					}

					ti_igmp_packet_handler ((struct igmphdr *)(((char *)iph) + (iph->ihl << 2)), skb,
											ptr_dbridge_src_dev->net_dev_type, cpe_num);
					return MCAST_IGMP;
				}
				else
				{ /* Mcast data packet */
					
					DbridgeDevice_t* ptr_dbridge_dest_dev;
					unsigned int dev_map = ti_get_igmp_devices(iph->daddr);
					if((mcast_filter_type == DYNAMIC_FILTER)&&(dev_map == 0))
						/* The IGMP grop IP is not mach the packet IP - We drop the packet */
					{
#ifdef DBRIDGE_LOG
						if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
						{				
							printk ("Mcast Data packet from dev %s IP:0x%x Drop\n", skb->dev->name , iph->daddr);
						}
						#endif
						dbridgeAddDiscards(skb);
						kfree_skb (skb);
					}
					else
					{
#ifdef DBRIDGE_LOG
						if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
						{				
							printk ("Mcast Data packet from %s to dev map 0x%x\n", skb->dev->name , dev_map);
						}
#endif


						skb->ti_selective_fwd_dev_info=dev_map;

						/* Don't flood packet mcast from cable interface */
						if((skb->ti_selective_fwd_dev_info==0) && (ptr_dbridge_src_dev->net_dev_type !=DBR_CABLE_NET_DEV_TYPE ))
						{   
							dbridgeAddInFrames(skb);
							Dbridge_floodPacket(skb,flood_map, DBR_CABLE_NET_DEV_TYPE ,DBR_CABLE_NET_DEV_TYPE);		

						}
						else
						{
							ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
							if(!dev_map)
							{
								dev_map = DbridgeDB_GetlbridgeInterfaceMask();
							}
							skb->ti_selective_fwd_dev_info=dev_map;
							dbridgeAddInFrames(skb);
							skb->dev = ptr_dbridge_dest_dev->ptr_interface;
#ifdef DBRIDGE_LOG
							if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
							{
								printk("\n xx DBRIDGE_DBG_REG_TX Mcast FROM %s TO %s\n",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
								printk("dev type %x send Mcast data packet to %s with selective_fwd %x",ptr_dbridge_src_dev->net_dev_type, ptr_dbridge_dest_dev->ptr_interface->name, skb->ti_selective_fwd_dev_info);
								DBridge_logPacket(skb);
							}
#endif                  
							if (ptr_dbridge_dest_dev->net_dev_type == DBR_CABLE_NET_DEV_TYPE)
							{
								fc_flags |= DBR_CLASSIFIER_FLAG;
							}
							ptr_dbridge_dest_dev->push(skb,fc_flags);

						}
					}
					return MCAST_DATA;
                   
				}
			}
			
		}
	}
	return NOT_MCAST;
}


/**************************************************************************/
/*! \fn static int  DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, 
                                        unsigned int flood_map)                                
 **************************************************************************
 *  \brief verify if this is Mcast packet and handle the packet according to MDF logic.
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 **************************************************************************/
static int DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, 
                               unsigned int flood_map)
{   
    struct ethhdr* ptr_ethhdr;
    unsigned int dsid_index = 0;
    unsigned int mcast_linux_cmim = 0;
    unsigned int mcast_flood_map = 0;	

    ptr_ethhdr = (struct ethhdr *)skb->mac.raw;

    /* if the packet is Multicast  */
	if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
	{
        if(!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
		{
            /* get DSID index from the packet - bits 24-31 */
            dsid_index = skb->ti_meta_info >> CNID_DSID_INDEX_BIT_FIELD_OFFSET & 0xff;

            /* The value from the HW is a multiple of 4 */
            if (dsid_index != 0xFF)
            {
                dsid_index = dsid_index >> 2;
            }

            /* if DSID index value not valid - drop the packet */
            if (dsid_index >= DBR_MDF_MAX_DSIDS)
            {
                printk("\nHandle MDF packet from dev %s Failed - DSID index value %u not valid!!!\n", 
                       skb->dev->name, dsid_index);
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {				
                    printk ("\nMcast Data packet from dev %s Drop\n", skb->dev->name);
                }
                #endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
            }

            #ifdef DBRIDGE_LOG
            if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {				
                printk("\nHandle MDF packet from dev %s with DSID index %u\n", skb->dev->name, dsid_index);
            }
            #endif
            
            /* if DSID index not found - drop the packet */
            if (DbridgeMdfDb_GetMcastBitmaps(dsid_index, &mcast_linux_cmim, &mcast_flood_map) == -1)
            {
                printk("\nHandle MDF packet from dev %s Failed - DSID index %u not found!!!\n", 
                       skb->dev->name, dsid_index);
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {				
                    printk ("\nMcast Data packet from dev %s Drop\n", skb->dev->name);
                }
                #endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
            }
    
            /* The flood map is 0 - drop the packet */
            else if (mcast_flood_map == 0)
            {
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {				
                    printk ("\nMcast Data packet from dev %s to dev map 0x%x and flood map 0x%x Drop\n", 
                            skb->dev->name, mcast_linux_cmim, mcast_flood_map);
                }
                #endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
            }

            /* forward the packet */
            else
            {
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {				
                    printk ("\nMcast Data packet from %s to dev map 0x%x and flood map 0x%x\n", 
                            skb->dev->name , mcast_linux_cmim, mcast_flood_map);
                }
                #endif
                    
                /* Flood the packet to all interfaces and perform filter for CMCI, ESAFE and WAN_IP. */
                dbridgeAddInFrames(skb);
                skb->ti_selective_fwd_dev_info = mcast_linux_cmim;
                    
                #ifdef DBRIDGE_LOG
				if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    /* Print the information before flooding, because the flood function frees the skb */
					printk("\n xx DBRIDGE_DBG_MDF Mcast FROM %s TO %s\n",
						   ptr_dbridge_src_dev->ptr_interface->name, skb->dev->name);
					printk("dev type %x send Mcast date packet with selective_fwd %x",
						   ptr_dbridge_src_dev->net_dev_type, 
						   skb->ti_selective_fwd_dev_info);
					DBridge_logPacket(skb);
				}
                #endif                                              

		        Dbridge_floodPacket(skb, mcast_flood_map, 
                                    DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE | DBR_WAN_IP_NET_DEV_TYPE, 0);
            }

            return MCAST_DATA;
        }
    }

	return NOT_MCAST;
}

/**************************************************************************/
/*! \fn void Dbridge_SendIgmpPacket(struct sk_buff *skb, unsigned long device_map, unsigned long device_type)
 **************************************************************************
 *  \brief   Send Mcasr packet, this function call from igmp module 
 *  \param[in] skb.
 *  \param[in] device_map, The devices that conect to the Mcast grope.
 *  \param[in] device_type, the docsis bridge dev type (cable or CPE)
 *  \param[out] none
 *  \return none
 **************************************************************************/
 void Dbridge_SendIgmpPacket(struct sk_buff *skb, unsigned long device_map, unsigned long device_type)
 {
	 DbridgeDevice_t* ptr_dbridge_dest_dev;
	 unsigned int fc_flags = DBR_FILTER_FLAG;

	 if (device_type & DBR_CMCI_NET_DEV_TYPE)
     {	/* send the packet to CPCI interface only */
		 ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
	 } 
	 else if (device_type & DBR_CABLE_NET_DEV_TYPE)
	 {
		 ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
		 fc_flags |= DBR_CLASSIFIER_FLAG;
	 } 
	 else
	 {
#ifdef DBRIDGE_DBG
		 printk (KERN_WARNING "Dbridge Mcast unknown device type %x\n", (unsigned int)device_type);
#endif
		 kfree_skb (skb);
		 return;
	 }

     if (skb == NULL)
     {
        printk (KERN_WARNING "Dbridge Mcast skb is NULL !!\n");
        return;
     }

	 skb->ti_selective_fwd_dev_info=device_map;
	 dbridgeAddInFrames(skb);
	 skb->dev = ptr_dbridge_dest_dev->ptr_interface;
	 
#ifdef DBRIDGE_LOG
	 if(dbridgeDbgLevel & DBRIDGE_DBG_REG_IGMP)
	 {				
		 printk (KERN_WARNING "IGMP packet from igmp module to %s\n", skb->dev->name);
		 DBridge_logPacket(skb);
	 }
#endif
	 
	 ptr_dbridge_dest_dev->push(skb,fc_flags);
}

/**************************************************************************/
/*! \fn int Dbridge_messageProcessed(char *mac_address, int msg_type)
 **************************************************************************
 *  \brief   
 *  \param[out] mac_address.
 *  \param[msg_type] device_map, The devices that conect to the Mcast grope.
 *  \return 
 **************************************************************************/
int Dbridge_messageProcessed(char *mac_address, int msg_type)
{
#ifdef DBRIDGE_DBG
    printk (KERN_DEBUG "IGMP message from %.2x:%.2x:%.2x:%.2x:%.2x:%.2x processsed for type (%d) interface\n",
			mac_address[0],mac_address[1],mac_address[2],
			mac_address[3],mac_address[4],mac_address[5], msg_type);
#endif    
	if (msg_type == JOIN_STATE)
	{
		(void)DbridgeDb_AddMcastMac(mac_address,DYNAMIC_FILTER);
	}else
	{
		DbridgeDb_DelMcastMac(mac_address);
	}
    return 0;
}


#ifdef CONFIG_TI_DOCSIS_EGRESS_HOOK
/**************************************************************************/
/*! \fn int Dbridge_egress_hook(struct sk_buff *skb)
 **************************************************************************
 *  \brief   In
 *  \param[in] SKB.
 *  \return 
 **************************************************************************/
static int Dbridge_egress_hook(struct sk_buff *skb)
{
    struct ethhdr* ptr_ethhdr;

    if(skb)
    {
        ptr_ethhdr = (struct ethhdr *)skb->mac.raw;

        if((skb->input_dev == eth0_netdev) || (skb->input_dev == usb0_netdev))
        {
            ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].In++;
            ptr_dbridge_counters->DevCounters[skb->dev->ifindex].Out++;
            if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
            {/*If this is unicast packet - increment unicast counters as well*/
                ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].inUcast++;
                ptr_dbridge_counters->DevCounters[skb->dev->ifindex].outUcast++;
            } else
            {  /*If this is broadcast packet - increment broadcast counter as well*/
                if ( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
                {
                    ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].inBcast++;
                    ptr_dbridge_counters->DevCounters[skb->dev->ifindex].outBcast++;
                } else
                {   /*If this is multicast packet - increment multicast counter as well*/
                    ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].inMcast++;
                    ptr_dbridge_counters->DevCounters[skb->dev->ifindex].outMcast++;
                }
            }
        }
    }
    return 0;
}
#endif /* CONFIG_TI_DOCSIS_EGRESS_HOOK */


#define LAN_IP 0xC0A86401  /* 192.168.100.1 */

static void mot_fixDestMACfromCMCI(struct sk_buff* skb)
{
    struct ethhdr*    ptr_ethhdr;
    struct iphdr*     ptr_iphdr;
    unsigned char dst_mac[MAC_ADDRESS_LEN] = {0xff,0xff,0xff,0xff,0xff,0xff};  
    char dev_name[] = LAN_IP_DEV_NAME;
    __be32 cpe_cached_ip = 0;   
    unsigned char* cpe_mac[MAC_ADDRESS_LEN]={0x0,0x0,0x0,0x0,0x0,0x0};    
    int exist=1;

    if(skb->dev->hard_header_len == ETH_HLEN)
    {
    	ptr_iphdr = (skb->data)+ETH_HLEN;
        ptr_ethhdr = (struct ethhdr *)skb->mac.raw;

        if ( ptr_ethhdr->h_proto == ETH_P_IP ) 
        {
/* IP packets */
            if ( ptr_iphdr->daddr == LAN_IP) 
/* destination is LAN */
            {
                if ( ptr_iphdr->version == 0x04 )
                {
                    memcpy (&cpe_mac[0], &ptr_ethhdr->h_source[0], ETH_ALEN);
                    cpe_cached_ip = ptr_iphdr->saddr;  
                    if ((192 != (0xff & cpe_cached_ip>>24)) || (168 != (0xff & cpe_cached_ip>>16)) || (100 != (0xff & cpe_cached_ip>>8)))
                    {
                        exist=mot_dbridgeDb_IsCpeExisting(cpe_cached_ip);									
                        if(!exist)
                        {                    	
			    	mot_Dbridge_AddRoute(dev_name, cpe_cached_ip); 	/* Add the route to the routing table if nt added earlier*/	
                        }
/* CPE not on the local net */                         
                        mot_DbridgeDb_SaveCpeMacIp(cpe_cached_ip, cpe_mac);    
/* Get the LAN MAC from the alt table and swap the dst MAC to LAN mac */
                        mot_DbridgeDb_AltGetDstMac(dev_name, dst_mac);    
                        memcpy ((void *)&ptr_ethhdr->h_dest[0], (void *)dst_mac, ETH_ALEN);					
                    }			
                } 
                if ( ptr_iphdr->version == 0x06 )
                {
                    /* do nothing yet */
                }
#if 0
                printk("XY: mot_fixDestMACfromCMCI: source_ip= %x\n", ptr_iphdr->saddr);
                printk("XY: mot_fixDestMACfromCMCI: source_mac= %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", 
                    ptr_ethhdr->h_source[0], ptr_ethhdr->h_source[1], ptr_ethhdr->h_source[2],
                    ptr_ethhdr->h_source[3], ptr_ethhdr->h_source[4], ptr_ethhdr->h_source[5]);
                printk("XY: mot_fixDestMACfromCMCI: dest_mac= %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", 
                    ptr_ethhdr->h_dest[0], ptr_ethhdr->h_dest[1], ptr_ethhdr->h_dest[2],
                    ptr_ethhdr->h_dest[3], ptr_ethhdr->h_dest[4], ptr_ethhdr->h_dest[5]);
#endif
            }  /* endof if( ptr_iphdr->daddr == DIAG_IP) */
        } /* endof if ( ptr_ethhdr->h_proto == ETH_P_IP ) */
    }   

}
static inline void
set_sockaddr(struct sockaddr_in *sin, u32 addr, u16 port)
{
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = addr;
	sin->sin_port = port;
}

/* \fn mot_Dbridge_AddRoute
 *  \brief Add a route to the kernel routing table
 *  \param[in]  devName - Network interface name to add route through
 *  \param[in]  dest - Destination address
 */
void mot_Dbridge_AddRoute(char *devName, unsigned int dest)
{

    struct  rtentry rm; 					/* Structure that will hold the new route information */
    struct socket *sock;				/* Socket structure thats really not used */
    int     err;
    u32 ic_netmask = INADDR_NONE;	/* Netmask for local subnet */

    if((sock=(struct socket *)kmalloc(sizeof(struct socket),GFP_KERNEL))==NULL)
    {
        printk (KERN_WARNING "Unable to allocate memory for socket structure");
        return; 
    }		

    /* Set up request */
    memset(&rm, 0, sizeof(rm));
    rm.rt_dev = devName;

    set_sockaddr((struct sockaddr_in *) &rm.rt_dst, dest, 0);
    set_sockaddr((struct sockaddr_in *) &rm.rt_genmask, ic_netmask, 0);
    set_sockaddr((struct sockaddr_in *) &rm.rt_gateway, 0, 0);	

    rm.rt_flags = RTF_UP | RTF_HOST;		
    /* Add route */
    err =inet_ioctl(sock,SIOCADDRT, (unsigned long )&rm);

    #ifdef DBRIDGE_DBG
	if(err!=0)
	{
		printk (KERN_ERR "Unable to add the Route, Error Code= %d\n", err);
	}
    #endif   
	kfree(sock);	
}

/**************************************************************************/
/*! \fn static int mot_DiscardCmTftpPktToCpeIP
 **************************************************************************
 *  \brief checks whether the udp src port belongs to cm tftp client .
 *  \check the IP packet if the destination IP is CPE IP (getting from CPE table)
 *  \then get the UDP header and UDP src port compare it with the current tftp port
 *  \ in use by the CM tftp client if there is a match return 0
 *  \param[in] ptr to sk_buff structure
 *  \return 0 on port match  -1 otherwise.
 **************************************************************************/

static int mot_DiscardCmTftpPktToCpeIP(struct sk_buff* skb)
{
    struct ethhdr*    ptr_ethhdr;
    struct iphdr*     ptr_iphdr;    
    unsigned char *   ptr_arpHdr;
    unsigned char* cpe_mac[MAC_ADDRESS_LEN]={0x0,0x0,0x0,0x0,0x0,0x0};            
    __be32 cpe_cached_ip = 0;  
    struct udphdr *   ptr_udpHdr;          
    __u16              src_port;
    unsigned int uiIsPortInUse = 0;
    int iRetValue = 0;

    /* check if port value in proc file is 0 then return no need to check further */
    uiIsPortInUse = mot_GetCmTftpPort();
    if(0 == uiIsPortInUse)
    {
	return -1;
    }
    if(skb->dev->hard_header_len == ETH_HLEN)
    {
    	ptr_iphdr = (skb->data)+ETH_HLEN;
        ptr_ethhdr = (struct ethhdr *)skb->mac.raw;

        /* Check for IP packets */
        if ( ptr_ethhdr->h_proto == ETH_P_IP ) 
        {
                 /* IPv4 check */ 
                if ( ptr_iphdr->version == 0x04 )  
                {
                    cpe_cached_ip = ptr_iphdr->daddr;
                    iRetValue = mot_DbridgeDb_IsDestCpe(ptr_iphdr->daddr, cpe_mac); 
                    if (0 == iRetValue) 
                    {
                         /* if it is cpe ip then look for UDP and  destination port */
                            if(ptr_iphdr->protocol == IPPROTO_UDP)
                             {
				 ptr_udpHdr = (struct udphdr *)(((unsigned char *)ptr_iphdr) + (ptr_iphdr->ihl<<2));
				 src_port = ptr_udpHdr->source;
                                 uiIsPortInUse = mot_CmTftpSrcPortNumInUse(src_port);
			         
				 if(0 == uiIsPortInUse)
                                 {
					return 0;
				 }
                             }
                    }
                   else
                   {
                      /* not cpe ip*/
                    }
                } 
                else if ( ptr_iphdr->version == 0x06 )
                {
                        /* do nothing yet */
                }
                else
		{
		  /* incorrect version */	
		}
        }
        else
 	{
            /* not an IP packet */
        }
    }  /* end if(skb->dev->hard_header_len == ETH_HLEN) */ 
    return -1;
}

/**************************************************************************/
/*! \fn int mot_CmTftpSrcPortNumInUse(unsigned int uiUdpSrcPort)
 **************************************************************************
 *  \brief compares the port value passed with the value in guiCmTftpPort 
 *  \param[in] Port Number
 *  \return 0 or -1.
 **************************************************************************/
int mot_CmTftpSrcPortNumInUse(unsigned int uiUdpSrcPort)
{
	unsigned int uiPort = 0;
	uiPort = mot_GetCmTftpPort();
	if(uiUdpSrcPort == uiPort)
       	{
		return 0;	
 	}
       	else
 	{
 		return -1;
 	}
}


EXPORT_SYMBOL(DBridge_receive);


