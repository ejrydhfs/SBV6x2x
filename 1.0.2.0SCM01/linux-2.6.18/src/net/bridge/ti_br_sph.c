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

#ifdef CONFIG_TI_L2_SELECTIVE_PACKET_HANDLING

#undef CONFIG_TI_L2_SELECTIVE_PACKET_HANDLING_DEBUG

/**************************************************************************
 * FUNCTION NAME : ti_selective_packet_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called from the bridge to selective handle packets.
 *  The function calls all registered packet handler in the order of priority.
 *  Currently only multicast and broadcast packets are handled selectively.
 *  
 * RETURNS       :
 *  0   -   No packet handlers. invoke default behavior 
 *  ret -   Returns the return code passed by the registered functions
 *
 * NOTES         :
 *  In the case of error; the packet memory is freed by the registered handlers.
 ***************************************************************************/
int ti_selective_packet_handler (struct sk_buff *skb)
{
	struct net_bridge   *br = skb->dev->br_port->br;
    struct l2_sph       *handle = br->selective_packet_handler;
    int                 ret = 0;

    /* Check if packet handler is installed. If no packet handler is installed 
     * return 0 to invoke default behavior i.e., flood the packet  */
    if (handle == NULL)
    {
#ifdef CONFIG_TI_L2_SELECTIVE_PACKET_HANDLING_DEBUG
        printk ("No selective packet handler installed. Flooding packet\n"); 
#endif /* CONFIG_TI_L2_SELECTIVE_PACKET_HANDLING_DEBUG */
        return ret;
    }
    /* Run all registered packet handlers. */
    while (handle != NULL)
    {
        if ((ret = handle->packet_handler(skb)) == -1)
        {
            /* Incase of error packet memory is freed by the registered handler. */
            printk ("Error running selective packet handler (%d)\n", ret);
            break;
        }
        handle = handle->next;
    }
    return ret;
}

/**************************************************************************
 * FUNCTION NAME : ti_register_selective_packet_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function registers a selective packet handler for a bridge.
 *  
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 ***************************************************************************/
int ti_register_selective_packet_handler (char *br_name, int (*packet_handler)(struct sk_buff *skb), int priority)
{
    struct l2_sph           *handle, *temp, *next_node;
	struct net_bridge       *br;
	struct net_device       *dev;
	struct net_bridge_port  *p;

    /* Check if device is valid. */ 
    if ((dev = dev_get_by_name (br_name)) == NULL)
    {
        printk ("Error: Device %s does not exist\n", br_name);
        return -1;
    }

    /* Check if bridge port. */ 
    if ((p = dev->br_port) == NULL) 
    {
        printk ("Error: Bridge port %s does not exist\n", br_name);
        return -1;
    }
    
    /* Check if bridge is valid. */ 
    if ((br = p->br) == NULL) 
    {
        printk ("Error: Bridge %s does not exist\n", br_name);
        return -1;
    }

    /* Allocate memory for packet handler. */
    handle = (struct l2_sph *) kmalloc(sizeof(struct l2_sph), GFP_KERNEL);
    if (handle == NULL)
    {
        printk ("Error: Failed to allocate memory for packet handler\n");
        return -1;
    }

    handle->packet_handler = packet_handler;
    handle->priority = priority;
    handle->next = NULL;
    handle->prev = NULL;

    /* Install the handler */    
    if (br->selective_packet_handler == NULL)
        br->selective_packet_handler = handle;
    else
    {
        temp = br->selective_packet_handler;
        while (temp != NULL)
        {  
            if (temp->priority == priority)
            {
                printk ("Error: Packet handler with priority %d already exists\n", priority); 
                kfree (handle);
                return -1;
            }
            else if (temp->priority > priority)
            {
                if (temp->prev == NULL)
                {
                    br->selective_packet_handler = handle;
                    temp->prev = handle;
                    handle->next = temp;
                }
                else
                {
                    next_node = temp->next;
                    if (next_node != NULL)
                    {
                        handle->next = next_node;
                        next_node->prev = handle;
                        handle->prev = temp;
                        temp->next = handle;
                    }
                    else
                    {
                        handle->prev = temp;
                        temp->next = handle;
                    }
                }
                break;
            }
            else if (temp->next == NULL)
            {
                handle->prev = temp;
                temp->next = handle;
                break;
            }
            temp = temp->next;
        }
    }   
    printk ("DEBUG: Register selective packet handler successful\n"); 
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_deregister_selective_packet_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function deregisters the selective packet handler for a bridge
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 ***************************************************************************/
int ti_unregister_selective_packet_handler (char *br_name, int (*packet_handler)(struct sk_buff *skb), int priority)
{
	struct net_bridge       *br;
	struct net_device       *dev;
    struct l2_sph           *temp;
   	struct net_bridge_port  *p;
 
    if ((dev = dev_get_by_name (br_name)) == NULL)
    {
        printk ("Error: Device %s does not exist\n", br_name);
        return -1;
    }

    if ((p = dev->br_port) == NULL) 
    {
        printk ("Error: Bridge port %s does not exist\n", br_name);
        return -1;
    }
    
    if ((br = p->br) == NULL) 
    {
        printk ("Error: Bridge %s does not exist\n", br_name);
        return -1;
    }

    if (br->selective_packet_handler == NULL)
    {
        printk ("Error: Bridge %s has no selective packet handler installed\n", br->dev->name);
        return -1;
    }

    /* Unregister the packet handler. */
    temp = br->selective_packet_handler;
    while (temp != NULL)
    { 
        if (temp->priority == priority)
        {
            if (temp->prev == NULL)
            {
                br->selective_packet_handler = temp->next;
                if (temp->next != NULL)
                    temp->next->prev = NULL;
            }
            else
            {
                if (temp->next == NULL)
                    temp->prev->next = NULL;
                else
                {
                    temp->prev->next = temp->next;
                    temp->next->prev = temp->prev;
                }
            }
            break;
        }
        temp = temp->next;
    }

    if (temp != NULL)
        kfree (temp);
    return 0;
}

EXPORT_SYMBOL(ti_register_selective_packet_handler); 
EXPORT_SYMBOL(ti_unregister_selective_packet_handler);
EXPORT_SYMBOL(ti_selective_packet_handler); 
#endif /* CONFIG_TI_L2_SELECTIVE_PACKET_HANDLING */

