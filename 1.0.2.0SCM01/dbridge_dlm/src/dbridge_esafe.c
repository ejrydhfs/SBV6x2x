/*
 *
 * dbridge_esafe.c
 * Description:
 * DOCSIS bridge ESAFE implementation
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

#define _DBRIDGE_ESAFE_C_

/*! \file dbridge_esafe.c
    \brief the docsis bridge esafe support
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
#include "dbridge_esafe.h"
#include "dbridge_main.h"
#include "dbridge_db.h"
#include "dbridge_netdev.h"
#include "dbridge_snif.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>






/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
 
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/


#ifdef DBRIDGE_LOG
extern unsigned int dbridgeDbgLevel;
#endif


/*! \fn static struct net_device* DbridgeEsafe_CreateDev(unsigned int esafe_index)
 *  \brief creafe esafeX interface
 *  \param[in] esafe_index - the esafe index.
 *  \return ptr to esafeX dev interface.
 */
static struct net_device*  DbridgeEsafe_CreateDev(unsigned int esafe_index);

/*! \fn int DbridgeEsafe_NetifReceive(struct sk_buff *skb, unsigned int fc_flags)                                     
 *  \brief Call back function called from the DOCSIS Bridge to push the packet
 *  \brief through the esafe device.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return transfer status 0
 */
static int DbridgeEsafe_NetifReceive(struct sk_buff *skb, unsigned int fc_flags);

/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromEsafeDev(struct net_device* ptr_interface)
 *  \brief get meber from esafe table corresponding to esafe dev 
 *  \param[in] pointer to esafe dev.
 *  \return ptr  to esafe device table
 */
static DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromEsafeDev(struct net_device* ptr_interface);

/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromTargetDev(struct net_device* ptr_interface)
 *  \brief get meber from esafe table corresponding to target dev 
 *  \param[in] pointer to esafe dev.
 *  \return ptr  to esafe device table
 */
static DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromTargetDev(struct net_device* ptr_interface);


 
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/


/*  Structure which keeps track of all the information for the DOCSIS Bridge. */
static DbridgeEsafeDevice_t  DbridgeEsafeDeviceTable[DBR_MAX_ESAFE_INTERFACES];
static unsigned  int esafe_table_insert;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeEsafeDb_Init(void)                                     
 **************************************************************************
 *  \brief DOCSIS bridge esafe initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeEsafe_Init(void)
{
	memset ((void *)&DbridgeEsafeDeviceTable, 0, (sizeof(DbridgeEsafeDevice_t) * DBR_MAX_ESAFE_INTERFACES));
    esafe_table_insert =0;
	return 0; /* OK*/
}
/**************************************************************************/
/*! \fn int DbridgeEsafe_ReInit(void)                                     
 **************************************************************************
 *  \brief DOCSIS bridge esafe re-initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int DbridgeEsafe_ReInit(void)
{
	return 0;
}

/**************************************************************************/
/*! \fn int DbridgeEsafe_AddNetDev(struct net_device* ptr_net_dev,
 *                                  unsigned int esafe_type,int (*push)(struct sk_buff* skb))                                     
 **************************************************************************
 *  \brief Connect target device interface to esafe (EMTA,EPS,ESTB_IP,ESTB_DSG,ETEA)
 *  \param[in] ptr_net_dev - pointer to target  device interface.
 *  \param[in] esafe_type - EMTA -1, EPS -2, ESTB_IP-3, ESTB_DSG-4 ,ETEA-5.
 *  \param[in] push - push function to send packet from esafe to target device interface.
 *  \return OK=0 or error status.
 **************************************************************************/
int DbridgeEsafe_AddNetDev(struct net_device* ptr_net_dev,unsigned int esafe_type,int (*push)(struct sk_buff* skb))
{
 if (esafe_table_insert < DBR_MAX_ESAFE_INTERFACES)
 {
	 if (ptr_net_dev)
	 {
		 /* create and register esafe interface */
		 struct net_device *esafe_net_device;
         esafe_net_device = DbridgeEsafe_CreateDev(esafe_table_insert);
		 if(esafe_net_device)
		 {
    		 DbridgeDb_DevAdd(esafe_net_device,DbridgeEsafe_NetifReceive,DBR_ESAFE_NET_DEV_TYPE);
             DbridgeEsafeDeviceTable[esafe_table_insert].ptr_esafeInterface = esafe_net_device;
			 DbridgeEsafeDeviceTable[esafe_table_insert].ptr_targetInterface = ptr_net_dev;
			 DbridgeEsafeDeviceTable[esafe_table_insert].eSafeType = esafe_type;
             DbridgeEsafeDeviceTable[esafe_table_insert].push = push;
			 esafe_table_insert++;
			 if(dev_open(esafe_net_device)==0)
				 return 0;
			 else
				 printk(KERN_WARNING "DUnable to open device %s\n",esafe_net_device->name);
		 }
		 else
		 {
			 printk(KERN_WARNING "DbridgeEsafe_CreateDev Fail\n");    
		 }
	 }
	 else
	 {
		 printk(KERN_WARNING "input ptr_net_dev is NULL\n");    
	 }
 }
 else
 {   
	 printk(KERN_WARNING "Device table is full\n");    
 }
 return -1;
}

/**************************************************************************/
/*! \fn static struct net_device* DbridgeEsafe_CreateDev(unsigned int esafe_index)
 **************************************************************************
 *  \brief creafe esafeX interface
 *  \param[in] esafe_index - the esafe index.
 *  \return ptr to esafeX dev interface.
 **************************************************************************/
static struct net_device* DbridgeEsafe_CreateDev(unsigned int esafe_index)
{
	struct net_device *tmp_net_device;
    char esafe_name[20];

	sprintf(esafe_name,"esafe%d",esafe_index);
    
	tmp_net_device=Dbridge_NetDevCreate(esafe_name,Dbridge_NetDevSetup);
	if(tmp_net_device==NULL)
		{
		printk(KERN_INFO "unknown device %s\n",LBRIGE_CONNECTION_DEV_NAME);
		return NULL; 
	}
	return tmp_net_device;
}


/**************************************************************************/
/*! \fn int DbridgeEsafe_NetifReceive(struct sk_buff *skb, unsigned int fc_flags)                                     
 **************************************************************************
 *  \brief Call back function called from the DOCSIS Bridge to push the packet
 *  \brief through the esafe device.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return transfer status 0
 **************************************************************************/
static int DbridgeEsafe_NetifReceive(struct sk_buff *skb, unsigned int fc_flags)
{
	DbridgeNetDev_t* priv;
    DbridgeEsafeDevice_t* ptr_esafe_table;
		
	if(fc_flags & DBR_FILTER_FLAG)
	{
		extern int (*Dbridge_FilterCB)(struct sk_buff *skb, unsigned int destIfMask, unsigned int *acceptDestIfMask, unsigned int pktDirect);
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
#ifdef DBRIDGE_LOG
				if(dbridgeDbgLevel &  DBRIDGE_DBG_FILTER)
				{
					printk("\nFilter drop packet");
					DBridge_logPacket(skb);
				}
#endif
				dbridgeAddFilterDropPacket(skb);
				kfree_skb (skb);
				return 0;
			}
		}
	}

#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_IF)
	{
		printk("\nDBRIDGE_DBG_REG_TX_IF TO %s",skb->dev->name);
		DBridge_logPacket(skb);
	}
#endif

	(void)dbridgeAddOutFrames(skb);


	/* Increment the statistics. */
	priv = netdev_priv(skb->dev);
	if (!netif_running(skb->dev))
	{
		priv->stats.rx_dropped++;
		kfree_skb (skb);
		return 0;

	}
	

	priv->stats.rx_packets++;
	priv->stats.rx_bytes += skb->len;
#ifdef DBRIDGE_LOG
	if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_IF)
	{
		printk("\nDBRIDGE_DBG_REG_TX_IF TO %s",skb->dev->name);
		DBridge_logPacket(skb);
	}
#endif
 
       ptr_esafe_table = DbridgeEsafe_GetMemberFromEsafeDev(skb->dev);

	if(ptr_esafe_table)
	{
		skb->dev = ptr_esafe_table->ptr_targetInterface;    
		/* push the packet through target device */
	    return ptr_esafe_table->push(skb);
	}
    else
	{
		printk(KERN_WARNING"\n [%s] ptr_esafe_table = null \n",__func__);
		kfree_skb (skb);
		return 0;
	}

}

/**************************************************************************/
/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromEsafeDev(struct net_device* ptr_interface)
 **************************************************************************
 *  \brief get meber from esafe table corresponding to esafe dev 
 *  \param[in] pointer to esafe dev.
 *  \return ptr  to esafe device table
 **************************************************************************/
static DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromEsafeDev(struct net_device* ptr_interface)
{
    int index;

    for (index = 0; index < esafe_table_insert; index++)
    {
        /* Compare the network interface for a match. */
        if (ptr_interface == DbridgeEsafeDeviceTable[index].ptr_esafeInterface)
            return &DbridgeEsafeDeviceTable[index];
    }

    /* No match found. */
    return NULL;
}



/**************************************************************************/
/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromTargetDev(struct net_device* ptr_interface)
 **************************************************************************
 *  \brief get meber from esafe table corresponding to target dev 
 *  \param[in] pointer to esafe dev.
 *  \return ptr  to esafe device table
 **************************************************************************/
static DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromTargetDev(struct net_device* ptr_interface)
{
    int index;

    for (index = 0; index < esafe_table_insert; index++)
    {
        /* Compare the network interface for a match. */
        if (ptr_interface == DbridgeEsafeDeviceTable[index].ptr_targetInterface)
			
            return &DbridgeEsafeDeviceTable[index];
    }

    /* No match found. */
    return NULL;
}

/**************************************************************************/
/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberByindex(unsigned int index)
 **************************************************************************
 *  \brief get meber from esafe table corresponding to target dev 
 *  \param[in] index.
 *  \return ptr  to esafe device table
 **************************************************************************/
DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberByindex(unsigned int index)
{
            return &DbridgeEsafeDeviceTable[index];
}

/**************************************************************************/
/*! \fn int DbridgeEsafe_GetNumOfEsaf(void)
 **************************************************************************
 *  \brief get the number of members in esafe dev table 
 *  \param[in] index.
 *  \return ptr  to esafe device table
 **************************************************************************/
int DbridgeEsafe_GetNumOfEsaf(void)
{
    return esafe_table_insert;
}


/**************************************************************************/
/*! \fn int  DBridgeEsafe_DeviceXmit(struct sk_buff *skb, struct net_device *dev)
 **************************************************************************
 *  \brief The function is the entry point by which packets from esafe are   
 *  \brief process and pushed into the DOCSIS Bridge. 
 *  \brief NOTE: Before calling this function ensure that the MAC RAW Pointer in the 
 *  \brief SKB is valid and points to the start of the MAC header.
 *  \param[in] skb.
 *  \param[in] dev.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int  DbridgeEsafe_DeviceXmit(struct sk_buff *skb, struct net_device *dev)
{


	DbridgeNetDev_t* priv;
    DbridgeEsafeDevice_t* ptr_esafe_table;

	skb->mac.raw = skb->data;

	if(skb->input_dev == NULL)
		skb->input_dev = skb->dev;

	/* replase the target device to esafe device */
	ptr_esafe_table= DbridgeEsafe_GetMemberFromTargetDev(skb->dev);
	if(ptr_esafe_table)
	{

		skb->dev=ptr_esafe_table->ptr_esafeInterface;
	}
	else
	{
		printk(KERN_WARNING"\n [%s] ptr_esafe_table = null \n",__func__);
		kfree_skb (skb);
		return 0;
	}
    /* Get the DOCSIS Local Interconnect Information*/
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


    /* Push the packet to the DOCSIS Bridge. */
	DBridge_receive (skb);

	return 0;
}


EXPORT_SYMBOL(DbridgeEsafe_DeviceXmit);
EXPORT_SYMBOL(DbridgeEsafe_AddNetDev);
