/****************************************************************************************
#**+----------------------------------------------------------------------------------+**
#**|                            ****                                                  |**
#**|                            ****                                                  |**
#**|                            ******o***                                            |**
#**|                      ********_///_****                                           |**
#**|                      ***** /_//_/ ****                                           |**
#**|                       ** ** (__/ ****                                            |**
#**|                           *********                                              |**
#**|                            ****                                                  |**
#**|                            ***                                                   |**
#**|                                                                                  |**
#**|         Copyright (c) 1998-2007 Texas Instruments Incorporated                   |**
#**|                        ALL RIGHTS RESERVED                                       |**
#**|                                                                                  |**
#**| Permission is hereby granted to licensees of Texas Instruments                   |**
#**| Incorporated (TI) products to use this computer program for the sole             |**
#**| purpose of implementing a licensee product based on TI products.                 |**
#**| No other rights to reproduce, use, or disseminate this computer                  |**
#**| program, whether in part or in whole, are granted.                               |**
#**|                                                                                  |**
#**| TI makes no representation or warranties with respect to the                     |**
#**| performance of this computer program, and specifically disclaims                 |**
#**| any responsibility for any damages, special or consequential,                    |**
#**| connected with the use of this program.                                          |**
#**|                                                                                  |**
#**+----------------------------------------------------------------------------------+**
#***************************************************************************************/

#include <linux/bitops.h>
#include <linux/config.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>

#ifdef CONFIG_TI_UNMANAGED_BRIDGE

/**************************************************************************
 * FUNCTION NAME : ti_blackhole
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the registered blackhole routine which swallows all
 *  packets received from the bridge management interface before they reach 
 *  the networking stacks.
 *
 * RETURNS       :
 *  Always return -1 i.e. never pass the packet up the stack
 ***************************************************************************/
int ti_blackhole(struct sk_buff *skb)
{
    kfree_skb(skb);
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_unmanaged_bridge_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is registered with the device notifier and its function
 *  is to detect if a bridged management interface has been bought up. If 
 *  one is detected it blackholes the management interface
 *
 * RETURNS       :
 *  0   - Bridged Management Interface; event has been processed.
 *  1   - Process the event normally as Open Source...
 ***************************************************************************/
int ti_unmanaged_bridge_handler (struct net_device *dev, unsigned long event)
{
    /* Check if the interface is a bridged management interface? */
    if ((dev->priv_flags & IFF_EBRIDGE) == 0)
        return 1;

    /* Check if the bridged management interface is being bought up? */
    if (event == NETDEV_UP)
    {
        ti_register_protocol_handler (dev, ti_blackhole);
        return 0;
    }

    /* All other events are handled as open source. */
    return 1;
}

#endif /* CONFIG_TI_UNMANAGED_BRIDGE */
