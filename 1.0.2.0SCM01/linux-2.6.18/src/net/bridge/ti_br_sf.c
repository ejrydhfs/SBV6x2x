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
#include "br_private.h"

#ifdef CONFIG_TI_L2_SELECTIVE_FORWARDER

#undef CONFIG_TI_L2_SELECTIVE_FORWARDER_DEBUG

/**************************************************************************
 *************************** Extern Data Structures ***********************
 **************************************************************************/
extern struct net_device *ti_netdevice[TI_MAX_DEVICE_INDEX];

/**************************************************************************
 * FUNCTION NAME : get_enabled_bit_count
 **************************************************************************
 * DESCRIPTION   :
 *  The function counts the number of bits enabled. 
 *
 * RETURNS       :
 *  Number of enabled bits 0 to 32 (max)
 ***************************************************************************/
static int get_enabled_bit_count(unsigned long value)
{
    int count = 0;
    while (value != 0)
    {
        value &= value - 1;
        count ++;
    }
    return count;
}

/**************************************************************************
 * FUNCTION NAME : ti_selective_forward
 **************************************************************************
 * DESCRIPTION   :
 *  The function selectively forwards the packets on the interfaces that are 
 *  marked. If the mark is zero, packet is flooded.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 *
 ***************************************************************************/
int ti_selective_forward(struct sk_buff *skb)
{
    if (!skb->ti_selective_fwd_dev_info)
    {
        /* Mark is zero. Return zero to flood the packet */
        return 0;
    }
    else
    {
        unsigned int        interface_list = skb->ti_selective_fwd_dev_info, dev_index = 0, count;
        struct sk_buff      *skb2;
        struct net_device   *dev = skb->dev;
       
        /* Get number of bits that are marked */ 
        count = get_enabled_bit_count(interface_list);

        /* Iterate and forward the cloned packet on all interfaces that are marked */
        while (interface_list)
        {
            if (interface_list & 0x1)
            {
                if (((dev = ti_netdevice[dev_index]) != NULL) && (dev->br_port != NULL))
                {
                    if (count == 1)
                        break;

                    if ((skb2 = skb_clone(skb, GFP_ATOMIC)) != NULL)
                    {
#ifdef CONFIG_TI_L2_SELECTIVE_FORWARDER_DEBUG
                        printk ("DEBUG Local Bridge: Forwarding Cloned multicast packet from (%s) to (%s) index (%d)\n", skb2->dev->name, dev->br_port->dev->name, dev_index+1);
#endif /* CONFIG_TI_L2_SELECTIVE_FORWARDER_DEBUG */
        		        br_forward(dev->br_port, skb2);
                    }
                    else
                    {
                        printk ("DEBUG Local Bridge: SKB Clone failed. Freeing original packet\n");
                        kfree_skb(skb); 
                        return -1;
                    }
                }
                else
                    printk ("DEBUG Local Bridge: Device info not found for dev index (%d)\n", dev_index+1);
                count--;
            }
            dev_index++;
            interface_list >>= 1;
        }
        if ((dev != NULL) && (dev->br_port != NULL))
        {
#ifdef CONFIG_TI_L2_SELECTIVE_FORWARDER_DEBUG
            printk ("DEBUG Local Bridge: Forwarding Original multicast packet from (%s) to (%s) index (%d)\n", skb->dev->name, dev->br_port->dev->name, dev_index+1);
#endif /* CONFIG_TI_L2_SELECTIVE_FORWARDER_DEBUG */

            /* Send the original packet on the last interface */
            br_forward(dev->br_port, skb);
        }
        return 1;
    }
}

EXPORT_SYMBOL(ti_selective_forward); 

#endif /* CONFIG_TI_L2_SELECTIVE_FORWARDER */

