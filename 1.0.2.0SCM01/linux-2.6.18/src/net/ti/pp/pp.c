/*
 * pp.c
 *
 *  The file contains the Packet Processor Simulation software based on the
 *  Linux Operating System. This is a pseudo-implementation for testing
 *  only.
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**************************************************************************
 *************************** Include Files ********************************
 **************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/if_pppox.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <net/tcp.h>
#include <linux/ppp_defs.h>
#include "ti_ppm.h"
#include "ti_ppd.h"

/**************************************************************************
 *************************** Local Definitions ****************************
 **************************************************************************/

/* Enable this to support packet dumping to debug L2 Framing. */
#define  TI_PP_DEBUG

#define INGRESS_MATCH                   0x1
#define EGRESS_MATCH                    0x2
#define SESSION_EXPIRATION_TIMEOUT      30

/**************************************************************************
 *************************** Local Structures *****************************
 **************************************************************************/

/**************************************************************************
 * STRUCTURE NAME : INTERNAL_PACKET_CLASS
 **************************************************************************
 * DESCRIPTION   :
 *  The enumeration defines the class of the session.
 **************************************************************************/
typedef enum INTERNAL_PACKET_CLASS
{
    INTERNAL_UNICAST   = 0,
    INTERNAL_MULTICAST = 1,
    INTERNAL_BROADCAST = 2,
}INTERNAL_PACKET_CLASS;

/**************************************************************************
 * STRUCTURE NAME : INTERNAL_SR_SESSION
 **************************************************************************
 * DESCRIPTION   :
 *  Internal structure used to store the session information.
 **************************************************************************/
typedef struct INTERNAL_SR_SESSION
{
    int                     status;
    struct timer_list       session_timer;
    TI_PP_SESSION_STATS     stats;
    TI_PP_SESSION           session;

    /* Scratchpad area */
    char*                   ptr_l3_header;
    INTERNAL_PACKET_CLASS   packet_class;
}INTERNAL_SR_SESSION;

/**************************************************************************
 * STRUCTURE NAME : INTERNAL_PPM_PID
 **************************************************************************
 * DESCRIPTION   :
 *  Internal structure used to store the session information.
 **************************************************************************/
typedef struct INTERNAL_PPM_PID
{
    int               status;
    TI_PP_PID         pid_info;
}INTERNAL_PPM_PID;

/**************************************************************************
 * STRUCTURE NAME : INTERNAL_PPM_VPID
 **************************************************************************
 * DESCRIPTION   :
 *  Internal structure used to store the session information.
 **************************************************************************/
typedef struct INTERNAL_PPM_VPID
{
    int                status;
    TI_PP_VPID_STATS   stats;
    TI_PP_VPID         vpid_info;
}INTERNAL_PPM_VPID;

/**************************************************************************
 ************************************ Globals *****************************
 **************************************************************************/

/* Global Statistics */
TI_PP_GLOBAL_STATS  pp_stats;

/* Database that keeps track of the PID. */
INTERNAL_PPM_PID    pid_table[TI_PP_MAX_PID + 1];

/* Database that keeps track of the VPID. */
INTERNAL_PPM_VPID   vpid_table[TI_PP_MAX_VPID + 1];

/* The simplicity of an array and linear searches to solve performance problems
 * because I dont have a LUT acclerator :(( */
INTERNAL_SR_SESSION   session_table[TI_PP_MAX_ACCLERABLE_SESSIONS];

/**************************************************************************
 ************************************ Externs *****************************
 **************************************************************************/
extern void ti_ppd_event_handler (unsigned short event_id, unsigned int param1, unsigned int param2);

static void ti_pp_dump_layer2_lut_entry (TI_PP_ETH_DESC* ptr_eth_desc);
static void ti_pp_dump_ipv4_lut_entry (TI_PP_IPV4_DESC* ptr_ipv4_desc);

#ifdef TI_PP_DEBUG

#define isprint(a) ((a >=' ')&&(a<= '~'))
/**************************************************************************
 * FUNCTION NAME : xdump
 **************************************************************************
 * DESCRIPTION   :
 *  Useful debugging utility function to display contents of memory at
 *  a specific location.
 **************************************************************************/
static void xdump(unsigned char *cp, int length, char *prefix)
{
    int col, count;
    u_char prntBuf[120];
    u_char *pBuf = prntBuf;
    count = 0;
    while (count < length) {
        pBuf += sprintf(pBuf, "%s", prefix);
        for (col = 0; count + col < length && col < 16; col++) {
            if (col != 0 && (col % 4) == 0)
                pBuf += sprintf(pBuf, " ");
            pBuf += sprintf(pBuf, "%02X ", cp[count + col]);
        }
        while (col++ < 16) {    /* pad end of buffer with blanks */
            if ((col % 4) == 0)
                sprintf(pBuf, " ");
            pBuf += sprintf(pBuf, "   ");
        }
        pBuf += sprintf(pBuf, "  ");
        for (col = 0; count + col < length && col < 16; col++) {
            if (isprint((int) cp[count + col]))
                pBuf += sprintf(pBuf, "%c", cp[count + col]);
            else
                pBuf += sprintf(pBuf, ".");
        }
        sprintf(pBuf, "\n");
        // SPrint(prntBuf);
        printk(prntBuf);
        count += col;
        pBuf = prntBuf;
    }
}

#endif /* TI_PP_DEBUG */

/**************************************************************************
 * FUNCTION NAME : ti_pp_event_generator
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the event generator which generates Packet Processor
 *  events.
 **************************************************************************/
static void ti_pp_event_generator (unsigned short event_id, unsigned int param1, unsigned int param2)
{
    /* Pass the event to the Host. */
    ti_ppd_event_handler (event_id, param1, param2);
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_create_pid
 **************************************************************************
 * DESCRIPTION   :
 *  The function creates a PID in the simulation code.
 *
 * RETURNS       :
 *  <0  - Error
 *  0   - Success
 **************************************************************************/
int ti_pp_create_pid (TI_PP_PID* ptr_pid)
{
    /* Basic Sanity Checking: Is the PID being used. */
    if (pid_table[ptr_pid->pid_handle].status == 1)
        return -1;

    /* Copy the PID Information. */
    memcpy ((void *)&pid_table[ptr_pid->pid_handle].pid_info, (void *)ptr_pid, sizeof(TI_PP_PID));

    /* The PID is now taken. */
    pid_table[ptr_pid->pid_handle].status = 1;

    /* PID has been successfully created. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_delete_pid
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes a PID from the simulation code.
 *
 * RETURNS       :
 *  < 0             - Error
 *  0               - Success
 **************************************************************************/
int ti_pp_delete_pid (int pid_handle)
{
    /* Basic Sanity Checking: Is the PID being used. */
    if (pid_table[pid_handle].status == 0)
        return -1;

    /* The PID is now taken. */
    pid_table[pid_handle].status = 0;

    /* Succesfully deleted the PID.*/
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_create_vpid
 **************************************************************************
 * DESCRIPTION   :
 *  The function creates a VPID in the simulation code.
 *
 * RETURNS       :
 *  <0  - Error
 *  0   - Success
 **************************************************************************/
int ti_pp_create_vpid (TI_PP_VPID* ptr_vpid)
{
    /* Basic Sanity Checking: Is the VPID being used. */
    if (vpid_table[ptr_vpid->vpid_handle].status == 1)
        return -1;

    /* Copy the VPID Information. */
    memcpy ((void *)&vpid_table[ptr_vpid->vpid_handle].vpid_info, (void *)ptr_vpid, sizeof(TI_PP_VPID));

    /* Reset the statistics block. */
    memset ((void *)&vpid_table[ptr_vpid->vpid_handle].stats, 0, sizeof(TI_PP_VPID_STATS));

    /* The VPID is now taken. */
    vpid_table[ptr_vpid->vpid_handle].status = 1;

#ifdef TI_PP_DEBUG
    /* DEBUG: Print the VPID Information. */
    printk ("---------------------------------------\n");
    printk ("VPID %d\n", ptr_vpid->vpid_handle);
    printk ("   Parent PID: %d\n", ptr_vpid->parent_pid_handle);
    printk ("   Egress MTU: %d\n", ptr_vpid->egress_mtu);
    switch (ptr_vpid->type)
    {
        case TI_PP_ETHERNET:
        {
            printk ("   Type      : Ethernet\n");
            break;
        }
        case TI_PP_VLAN:
        {
            printk ("   Type      : VLAN\n");
            printk ("   VLAN ID   : %d\n", ptr_vpid->vlan_identifier);
            break;
        }
        case TI_PP_PPPoE:
        {
            printk ("   Type      : PPPoE\n");
            printk ("   PPP ID    : %d\n", ptr_vpid->ppp_session_id);
            break;
        }
        case TI_PP_VLAN_PPPoE:
        {
            printk ("   Type      : VLAN+PPPoE\n");
            printk ("   PPP ID    : %d\n", ptr_vpid->ppp_session_id);
            printk ("   VLAN ID   : %d\n", ptr_vpid->vlan_identifier);
            break;
        }
        default:
        {
            printk ("Unknown VPID type %d\n", ptr_vpid->type);
            return -1;
        }
    }
    printk ("---------------------------------------\n");
#endif /* TI_PP_DEBUG */

    /* VPID has been successfully created */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_delete_vpid
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes a VPID in the simulation code.
 *
 * RETURNS       :
 *  < 0             - Error
 *  0               - Success
 **************************************************************************/
int ti_pp_delete_vpid (int vpid_handle)
{
    /* Basic Sanity Checking: Is the VPID being used. */
    if (vpid_table[vpid_handle].status == 0)
        return -1;

    /* The VPID is now taken. */
    vpid_table[vpid_handle].status = 0;

    /* Succesfully deleted the VPID.*/
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_set_pid_flags
 **************************************************************************
 * DESCRIPTION   :
 *  The function modifies PID flags in the simulation code.
 *
 * RETURNS       :
 *  < 0             - Error
 *  0               - Success
 **************************************************************************/
int ti_pp_set_pid_flags (TI_PP_PID* ptr_pid, unsigned int new_flags)
{
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_set_vpid_flags
 **************************************************************************
 * DESCRIPTION   :
 *  The function modifies VPID flags in the simulation code.
 *
 * RETURNS       :
 *  < 0             - Error
 *  0               - Success
 **************************************************************************/
int ti_pp_set_vpid_flags (TI_PP_VPID* ptr_vpid, unsigned int new_flags)
{
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_get_vpid
 **************************************************************************
 * DESCRIPTION   :
 *  The function searches the internal VPID table to get the corresponding
 *  VPID information.
 *
 * RETURNS       :
 *  Pointer to the VPID structure   - Success
 *  NULL                            - No match
 **************************************************************************/
static INTERNAL_PPM_VPID* ti_pp_get_vpid (int vpid_handle)
{
    /* Basic validations. */
    if (vpid_handle < 0 || vpid_handle > TI_PP_MAX_VPID)
        return NULL;

    /* Check if the VPID is active or not? */
    if (vpid_table[vpid_handle].status == 0)
        return NULL;

    /* Return the VPID Information */
    return &vpid_table[vpid_handle];
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_match_layer2_lut_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function compares the 2 Layer2 LUT entries passed to it as arguments
 *  and returns the result.
 *  Returns:
 *  1 - Success if the layer2 lut data matches
 *  0 - No match / error
 **************************************************************************/
static int ti_pp_match_layer2_lut_entry (TI_PP_ETH_DESC* ptr_eth_desc1, TI_PP_ETH_DESC* ptr_eth_desc2)
{
    /* Check if the Destination MAC Address has been specified. */
    if (ptr_eth_desc1->enables & TI_PP_SESSION_L2_DSTMAC_VALID)
    {
        /* Check if the Destination MAC addresses match */
        if(memcmp((void *)&ptr_eth_desc1->dstmac, (void *)&ptr_eth_desc2->dstmac, 6))
            return 0;
    }

    /* Check if the Source MAC Address has been specified. */
    if (ptr_eth_desc1->enables & TI_PP_SESSION_L2_SRCMAC_VALID)
    {
        /* Check if the Source MAC addresses match */
        if(memcmp((void *)&ptr_eth_desc1->srcmac, (void *)&ptr_eth_desc2->srcmac, 6))
            return 0;
    }

    /* Work is completed. */
    return 1;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_match_ipv4_lut_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function compares the 2 IPV4 LUT entries passed to it as arguments
 *  and returns the result.
 *  Returns:
 *  1 - Success if the ipv4 lut data matches
 *  0 - No match / error
 **************************************************************************/
static int ti_pp_match_ipv4_lut_entry (TI_PP_IPV4_DESC* ptr_ipv4_desc1,TI_PP_IPV4_DESC* ptr_ipv4_desc2)
{
    /* Check if the Destination IP has been specified. */
    if (ptr_ipv4_desc1->enables & TI_PP_SESSION_IPV4_DSTIP_VALID)
    {
        /* Check if the Destination IP matches */
        if(memcmp((void *)&ptr_ipv4_desc1->dst_ip,(void *)&ptr_ipv4_desc2->dst_ip, 4))
            return 0;
    }

    /* Check if the Source IP has been specified. */
    if (ptr_ipv4_desc1->enables & TI_PP_SESSION_IPV4_SRCIP_VALID)
    {
        /* Check if the Source IP matches */
        if(memcmp((void *)&ptr_ipv4_desc1->src_ip,(void *)&ptr_ipv4_desc2->src_ip, 4))
            return 0;
    }

    /* Check if the Destination port has been specified. */
    if (ptr_ipv4_desc1->enables & TI_PP_SESSION_IPV4_DST_PORT_VALID)
    {
        /* Check if the Destination ports match */
        if(memcmp((void *)&ptr_ipv4_desc1->dst_port,(void *)&ptr_ipv4_desc2->dst_port, sizeof(unsigned short)))
            return 0;
    }

    /* Check if the Source port has been specified. */
    if (ptr_ipv4_desc1->enables & TI_PP_SESSION_IPV4_SRC_PORT_VALID)
    {
        /* Check if the Source ports match */
        if(memcmp((void *)&ptr_ipv4_desc1->src_port,(void *)&ptr_ipv4_desc2->src_port, sizeof(unsigned short)))
            return 0;
    }

    /* Check if the protocol has been specified. */
    if (ptr_ipv4_desc1->enables & TI_PP_SESSION_IPV4_PROTOCOL_VALID)
    {
        /* Check if the Protocol field matches */
        if(memcmp((void *)&ptr_ipv4_desc1->protocol,(void *)&ptr_ipv4_desc2->protocol, sizeof(unsigned char)))
            return 0;
    }

    /* Check if the TOS has been specified. */
    if (ptr_ipv4_desc1->enables & TI_PP_SESSION_IPV4_TOS_VALID)
    {
        /* Check if the ToS byte matches */
        if(memcmp((void *)&ptr_ipv4_desc1->tos,(void *)&ptr_ipv4_desc2->tos, sizeof(unsigned char)))
            return 0;
    }

    /* Work is completed. */
    return 1;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_match_ipv6_lut_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function compares the 2 IPV6 LUT entries passed to it as arguments
 *  and returns the result.
 *  Returns:
 *  1 - Success if the ipv6 lut data matches
 *  0 - No match / error
 **************************************************************************/
static int ti_pp_match_ipv6_lut_entry (TI_PP_IPV6_DESC* ptr_ipv6_desc1, TI_PP_IPV6_DESC* ptr_ipv6_desc2)
{
    printk ("IPV6 Not Yet Supported\n");
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_lut_lookup
 **************************************************************************
 * DESCRIPTION   :
 *  The function searches the LUT Table for a match.
 *
 * RETURNS       :
 *  Handle to matched session - Success
 *  < 0                       - No match
 **************************************************************************/
static int ti_pp_lut_lookup (TI_PP_SESSION* ptr_receive_session)
{
    int             session_index;
    TI_PP_SESSION*  ptr_session = NULL;
    int             match_result;

    /* Cycle through all the Session that can exist in the PDSP for a match. */
    for (session_index = 0; session_index < TI_PP_MAX_ACCLERABLE_SESSIONS; session_index++)
    {
        /* Match only if the session was valid... */
        if (session_table[session_index].status)
        {
            /* Get the pointer to the session */
            ptr_session = &session_table[session_index].session;

            /* By default we have a perfect match until explicitly told so. */
            match_result = 1;

            /* Check if there was a Layer2 LUT entry specified? */
            if (ptr_session->ingress.l2_packet.packet_type == TI_PP_ETH_TYPE)
            {
               match_result = ti_pp_match_layer2_lut_entry ((void *)&ptr_session->ingress.l2_packet.u.eth_desc, (void *)&ptr_receive_session->ingress.l2_packet.u.eth_desc);

                /* If there is no match with a LUT abort! We dont want to cause performance degradation. */
                if (match_result == 0)
                    continue;
            }

            /* Check if there was a Layer3/Layer4 entry specified? */
            if (ptr_session->ingress.l3l4_packet.packet_type == TI_PP_IPV4_TYPE)
            {
                match_result = ti_pp_match_ipv4_lut_entry ((void *)&ptr_session->ingress.l3l4_packet.u.ipv4_desc, (void *)&ptr_receive_session->ingress.l3l4_packet.u.ipv4_desc);
            }
            if (ptr_session->ingress.l3l4_packet.packet_type == TI_PP_IPV6_TYPE)
            {
                match_result = ti_pp_match_ipv4_lut_entry ((void *)&ptr_session->ingress.l3l4_packet.u.ipv6_desc, (void *)&ptr_receive_session->ingress.l3l4_packet.u.ipv6_desc);
            }

            /* Check if we found a match for all configured LUT data */
            if(match_result == 1)
            {
                /* Match Found. */
                return session_index;
            }
        }
    }

    /* Control comes here indication that there was no match. */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_session_expired
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the timeout function which is called to indicate that
 *  no data has been received on the session for a particular period of time
 *  This will generate an event to the Packet Processor components
 **************************************************************************/
static void ti_pp_session_expired (unsigned long data)
{
    /* Session has timed-out. Generate an event for the PPM and PPD. */
    ti_pp_event_generator (TI_PP_SESSION_EXPIRATION, data, 0);
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_dump_layer2_lut_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function prints the Layer2 LUT entry
 **************************************************************************/
static void ti_pp_dump_layer2_lut_entry (TI_PP_ETH_DESC* ptr_eth_desc)
{
    /* Check if the Destination MAC Address has been specified. */
    if (ptr_eth_desc->enables & TI_PP_SESSION_L2_DSTMAC_VALID)
        printk ("Dst. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", ptr_eth_desc->dstmac[0], ptr_eth_desc->dstmac[1],
                ptr_eth_desc->dstmac[2], ptr_eth_desc->dstmac[3], ptr_eth_desc->dstmac[4], ptr_eth_desc->dstmac[5]);
    else
        printk ("Dst. MAC     = XXX\n");

    /* Check if the Source MAC Address has been specified. */
    if (ptr_eth_desc->enables & TI_PP_SESSION_L2_SRCMAC_VALID)
        printk ("Src. MAC     = %02x:%02x:%02x:%02x:%02x:%02x\n", ptr_eth_desc->srcmac[0], ptr_eth_desc->srcmac[1],
                ptr_eth_desc->srcmac[2], ptr_eth_desc->srcmac[3], ptr_eth_desc->srcmac[4], ptr_eth_desc->srcmac[5]);
    else
        printk ("Src. MAC     = XXX\n");

    /* Work is completed. */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_dump_ipv4_lut_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function prints the IPv4 LUT entry
 **************************************************************************/
static void ti_pp_dump_ipv4_lut_entry (TI_PP_IPV4_DESC* ptr_ipv4_desc)
{
    /* Check if the Destination IP has been specified. */
    if (ptr_ipv4_desc->enables & TI_PP_SESSION_IPV4_DSTIP_VALID)
        printk ("Dst IP       = 0x%x\n", ptr_ipv4_desc->dst_ip);
    else
        printk ("Dst IP       = XXX\n");

    /* Check if the Source IP has been specified. */
    if (ptr_ipv4_desc->enables & TI_PP_SESSION_IPV4_SRCIP_VALID)
        printk ("Src IP       = 0x%x\n", ptr_ipv4_desc->src_ip);
    else
        printk ("Src IP       = XXX\n");

    /* Check if the protocol has been specified. */
    if (ptr_ipv4_desc->enables & TI_PP_SESSION_IPV4_PROTOCOL_VALID)
        printk ("Protocol     = 0x%x\n", ptr_ipv4_desc->protocol);
    else
        printk ("Protocol     = XXX\n");

    /* Check if the TOS Byte has been specified. */
    if (ptr_ipv4_desc->enables & TI_PP_SESSION_IPV4_TOS_VALID)
        printk ("TOS          = 0x%x\n", ptr_ipv4_desc->tos);
    else
        printk ("TOS          = XXX\n");

    /* Check if the Destination Port has been specified. */
    if (ptr_ipv4_desc->enables & TI_PP_SESSION_IPV4_DST_PORT_VALID)
        printk ("Dst Port     = %d\n", ntohs(ptr_ipv4_desc->dst_port));
    else
        printk ("Dst Port     = XXX\n");

    /* Check if the Source Port has been specified. */
    if (ptr_ipv4_desc->enables & TI_PP_SESSION_IPV4_SRC_PORT_VALID)
        printk ("Src Port     = %d\n", ntohs(ptr_ipv4_desc->src_port));
    else
        printk ("Src Port     = XXX\n");

    /* Work is completed. */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_create_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to create a session.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_pp_create_session (TI_PP_SESSION *ptr_session, TI_PPD_IF *ingress_if, TI_PPD_IF *egress_if)
{
    /* Get the session handle. */
    int session_handle = ptr_session->session_handle;

    /* Check if the Session Handle passed is free or not? */
    if (session_table[ptr_session->session_handle].status == 0)
    {
        /* YES. Add the session to the internal list. */
        session_table[session_handle].status = 1;

        /* Initialize the statistics block: The firmware specification states that the statistics
         * block is cleared only on CREATE and the stats block is valid even after the session has
         * been deleted. */
        memset ((void *)&session_table[session_handle].stats, 0, sizeof(TI_PP_SESSION_STATS));

        /* Copy the session information to the PP database. */
        memcpy((void *)&session_table[session_handle].session, (void *)ptr_session, sizeof(TI_PP_SESSION));

#ifdef TI_PP_DEBUG
        {
            int index;

            /* Debug Message: Creating Sesson. */
            printk ("************************************************************\n");
            printk ("Creating Session %d\n", session_handle);

            /* Ingress Information is optional and might not be specified. */
            if ((ingress_if->vpid == NULL) || (ingress_if->pid == NULL))
            {
                /* Ingress Information not specified. */
                printk ("Ingress VPID = UNSPECIFIED\n");
                printk ("Ingress PID  = UNSPECIFIED\n");
            }
            else
            {
                /* Display the Ingress Information. */
                printk ("Ingress VPID = %d\n", ingress_if->vpid->vpid_handle);
                printk ("Ingress PID  = %d\n", ingress_if->pid->pid_handle);
            }
            /* Display the Ingress L2/L3 and L4 Information. */
            ti_pp_dump_layer2_lut_entry (&session_table[session_handle].session.ingress.l2_packet.u.eth_desc);
            ti_pp_dump_ipv4_lut_entry (&session_table[session_handle].session.ingress.l3l4_packet.u.ipv4_desc);

            /* Display all the Egress Records... */
            for (index = 0; index < ptr_session->num_egress; index++)
            {
                /* Print the Egress Session Properties; which include the L2/L3 and L4 properties. */
                printk ("Egress Record %d\n", index+1);
                printk ("Egress VPID  = %d\n", egress_if->vpid->vpid_handle);
                printk ("Egress PID   = %d\n", egress_if->pid->pid_handle);
                ti_pp_dump_layer2_lut_entry (&session_table[session_handle].session.egress[0].l2_packet.u.eth_desc);
                ti_pp_dump_ipv4_lut_entry (&session_table[session_handle].session.egress[0].l3l4_packet.u.ipv4_desc);

                /* Jump to the next egress record. */
                egress_if++;
            }
            printk ("************************************************************\n");
        }
#endif /* TI_PP_DEBUG */

        /* Check if a timeout has been specified. */
        if (ptr_session->session_timeout != 0)
        {
            /* Create a timer to check for inactivity. */
            init_timer(&session_table[session_handle].session_timer);
    	    session_table[session_handle].session_timer.function = ti_pp_session_expired;
        	session_table[session_handle].session_timer.data     = session_handle;

            /* Start the timer */
            session_table[session_handle].session_timer.expires  = jiffies + ptr_session->session_timeout;
            add_timer (&session_table[session_handle].session_timer);
        }

        /* Session has been succesfully created. */
        return 0;
    }

    /* Session Entry was already in use... PPM did not generate a correct handle. */
    printk ("Session Handle %d already in use; PPM failure detected\n", session_handle);
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_delete_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to delete a session.
 *
 * RETURNS       :
 *  0       -   Success
 *  < 0     -   Error
 **************************************************************************/
int ti_pp_delete_session(int session_handle)
{
    /* Ensure that the session is valid. */
    if (session_table[session_handle].status == 0)
        return -1;

    /* Delete the session timer */
    del_timer (&session_table[session_handle].session_timer);

    /* The session was valid; removing it is simply setting the "status" field as unused. */
    session_table[session_handle].status = 0;
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_modify_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to modify the session.
 *
 * RETURNS       :
 *  0       -   Success
 *  < 0     -   Error
 **************************************************************************/
int ti_pp_modify_session (TI_PP_SESSION *session_cfg, TI_PPD_IF *ingress_if, TI_PPD_IF *egress_if)
{
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_get_session_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to retreive the session stats
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_pp_get_session_stats (int session_handle, TI_PP_SESSION_STATS* ptr_stats)
{
    /* Populate the statistics. */
    memcpy ((void *)ptr_stats, (void *)&session_table[session_handle].stats, sizeof(TI_PP_SESSION_STATS));
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_clear_session_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to clear the session stats
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_pp_clear_session_stats (int session_handle)
{
    /* Check if the session exists or not? */
    if (session_table[session_handle].status == 0)
        return -1;

    /* Populate the statistics. */
    memset ((void *)&session_table[session_handle].stats, 0, sizeof(TI_PP_SESSION_STATS));
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_get_vpid_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to retreive the VPID stats
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_pp_get_vpid_stats (int vpid_handle, TI_PP_VPID_STATS* ptr_stats)
{
    /* Check if the session exists or not? */
    if (vpid_table[vpid_handle].status == 0)
        return -1;

    /* Populate the statistics. */
    memcpy ((void *)ptr_stats, (void *)&vpid_table[vpid_handle].stats, sizeof(TI_PP_VPID_STATS));
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_get_n_clear_vpid_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to clear the VPID stats
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_pp_get_n_clear_vpid_stats (unsigned char vpid, TI_PP_VPID_STATS *stats)
{
    /* Check if the session exists or not? */
    if (vpid_table[vpid].status == 0)
        return -1;

    /* Populate the statistics. */
    memset ((void *)&vpid_table[vpid].stats, 0, sizeof(TI_PP_VPID_STATS));
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_get_srl_pkt_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to retreive the Global Packet Processor stats
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_pp_get_srl_pkt_stats (TI_PP_GLOBAL_STATS* ptr_stats)
{
    /* Populate the statistics. */
    memcpy ((void *)ptr_stats, (void *)&pp_stats, sizeof(TI_PP_GLOBAL_STATS));
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_get_n_clear_srl_pkt_stats
 **************************************************************************
 * DESCRIPTION   :
 *  The function is used to get and clear the Global Packet Processor stats
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
int ti_pp_get_n_clear_srl_pkt_stats(TI_PP_GLOBAL_STATS *stats)
{
    /* Populate the statistics. */
    memset ((void *)&pp_stats, 0, sizeof(TI_PP_GLOBAL_STATS));
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_extract_pkt_fields
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to extract the L2/L3 and L4 fields from the
 *  packet and to populate the LUT entry.
 *
 * RETURNS       :
 *  Pointer to the L3 header   -   Success
 *  NULL                       -   Error
 **************************************************************************/
static char* ti_pp_extract_pkt_fields (char* ptr_data, TI_PP_SESSION* ptr_session, int *control_flags)
{
    struct ethhdr*          ptr_ethhdr = NULL;
    struct iphdr*           ptr_iphdr = NULL;
    struct tcphdr*          ptr_tcphdr = NULL;
    struct ipv6hdr*         ptr_ip6hdr  = NULL;
    struct vlan_hdr*        ptr_vlanheader = NULL;
    unsigned short          ppp_session_id = 0x0;
    unsigned short          protocol_type;
    TI_PP_PACKET_DESC*      ptr_pkt_desc;
    TI_PP_SESSION_PROPERTY* ptr_ses_property = &ptr_session->ingress;

    /* No control packets by default. */
    *control_flags = 0;

    /* Get the pointer to the Ethernet header */
    ptr_ethhdr = (struct ethhdr *)ptr_data;

    /* Skip the Ethernet header. */
    ptr_data = ptr_data + sizeof(struct ethhdr);

    /* Get the protocol type. */
    protocol_type = ptr_ethhdr->h_proto;

    /* Check the protocol field. If the protocol is VLAN or not?  */
    if (protocol_type == __constant_htons(ETH_P_8021Q))
    {
        /* Get the VLAN header. */
        ptr_vlanheader = (struct vlan_hdr *)ptr_data;

        /* The new protocol is encapsulated into the VLAN header. */
        protocol_type = ptr_vlanheader->h_vlan_encapsulated_proto;

        /* Skip the 4 bytes VLAN header. We have already accounted for the Ethernet header. */
        ptr_data = ptr_data + sizeof(struct vlan_hdr);
    }

    /* We have skipped the layer2 information; so try and get the layer3 information. */
    switch (protocol_type)
    {
        case __constant_htons(ETH_P_IP):
        {
            /* Get the pointer to the IPv4 Header. */
            ptr_iphdr   = (struct iphdr *)ptr_data;
            break;
        }
        case __constant_htons(ETH_P_IPV6):
        {
            /* Get the pointer to the IPv6 Header. */
            ptr_ip6hdr = (struct ipv6hdr *)ptr_data;
            break;
        }
        case __constant_htons(ETH_P_PPP_DISC):
        case __constant_htons(ETH_P_PPP_SES):
        {
            struct pppoe_hdr*   ptr_ppp_header;

            /* Get the pointer to the PPPoE Header. */
            ptr_ppp_header = (struct pppoe_hdr *)ptr_data;

            /* Extract the PPP Session ID. */
            ppp_session_id = ptr_ppp_header->sid;

            /* PPP Packet: Skip the PPP header. */
            ptr_data = ptr_data + 6;

            /* Get the PPP Protocol Information*/
            protocol_type = *((unsigned short *)ptr_data);
            if (protocol_type == __constant_htons(PPP_IP))
            {
                /* IP Packet. */
                ptr_iphdr = (struct iphdr *)(ptr_data + 2);
            }
            break;
        }
        default:
        {
            /* Unknown packets. */
            return NULL;
        }
    }

    /* Step4: Extract the Layer 4 information only if the packet was an IP Packet. */
    if (ptr_iphdr != NULL)
    {
        /* Currently we support only TCP & UDP. For both these protocols the PORT
         * information lies at the same location. */
        if ((ptr_iphdr->protocol == IPPROTO_TCP) || (ptr_iphdr->protocol == IPPROTO_UDP))
            ptr_tcphdr = (struct tcphdr *)((char *)ptr_iphdr + ptr_iphdr->ihl*4);
    }

    /* TODO: No IPv6 Support till now. */
    if (ptr_ip6hdr != NULL)
        return NULL;

    /* At this stage all the headers have been located and are pointing at the correct
     * locations. Time to start populating the Packet Properties.
     *
     * Step 1: Start with the Layer2 Header. */
    ptr_pkt_desc = &ptr_ses_property->l2_packet;

    /* Check did we have an Ethernet header. */
    if (ptr_ethhdr != NULL)
    {
        /* YES. Ethernet header was detected. Initialize the various fields. */
        ptr_pkt_desc->packet_type = TI_PP_ETH_TYPE;

        /* Populate the destination MAC address. */
        memcpy(ptr_pkt_desc->u.eth_desc.dstmac, (void *)&ptr_ethhdr->h_dest, 6);
        ptr_pkt_desc->u.eth_desc.enables  |= TI_PP_SESSION_L2_DSTMAC_VALID;

        /* Populate the source MAC address. */
        memcpy(ptr_pkt_desc->u.eth_desc.srcmac, (void *)&ptr_ethhdr->h_source, 6);
        ptr_pkt_desc->u.eth_desc.enables  |= TI_PP_SESSION_L2_SRCMAC_VALID;
    }
    else
    {
        /* No Ethernet header was present. Reset all the fields in the packet descriptor */
        memset ((void *)ptr_pkt_desc, 0, sizeof(TI_PP_PACKET_DESC));
    }

    /* Step 2: Start with the Layer3 header. */
    ptr_pkt_desc = &ptr_ses_property->l3l4_packet;

    /*  Check if an IPv4 header was detected or not? */
    if (ptr_iphdr != NULL)
    {
        /* Yes. IPv4 header was detected. Initialize the various fields. */
        ptr_pkt_desc->packet_type = TI_PP_IPV4_TYPE;

        /* Populate the Destination IP Address. */
        ptr_pkt_desc->u.ipv4_desc.dst_ip = ptr_iphdr->daddr;
        ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_DSTIP_VALID;

        /* Populate the Source IP Address. */
        ptr_pkt_desc->u.ipv4_desc.src_ip = ptr_iphdr->saddr;
        ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_SRCIP_VALID;

        /* Populate the TOS Byte */
        ptr_pkt_desc->u.ipv4_desc.tos = ptr_iphdr->tos;
        ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_TOS_VALID;

        /* Populate the Protocol */
        ptr_pkt_desc->u.ipv4_desc.protocol = ptr_iphdr->protocol;
        ptr_pkt_desc->u.ipv4_desc.enables  |= TI_PP_SESSION_IPV4_PROTOCOL_VALID;

        /* Step3: Check if a Layer4 header was detected or not? */
        if (ptr_tcphdr != NULL)
        {
            /* YES. Layer4 header was present. Populate the Dst Port */
            ptr_pkt_desc->u.ipv4_desc.dst_port = ptr_tcphdr->dest;
            ptr_pkt_desc->u.ipv4_desc.enables |= TI_PP_SESSION_IPV4_DST_PORT_VALID;

            /* Populate the Source Port. */
            ptr_pkt_desc->u.ipv4_desc.src_port = ptr_tcphdr->source;
            ptr_pkt_desc->u.ipv4_desc.enables |= TI_PP_SESSION_IPV4_SRC_PORT_VALID;
        }

        /* If the packet is a TCP packet; check the flags to see if it is a control packet or not? */
        if (ptr_iphdr->protocol == IPPROTO_TCP)
        {
	        if (ptr_tcphdr->rst | ptr_tcphdr->syn | ptr_tcphdr->fin)
                *control_flags = 1;
            if (ptr_tcphdr->ack == 0)
                *control_flags = 1;
        }
    }
    else
    {
        /* No IPv4 header was present. Reset all the fields in the packet descriptor. */
        memset ((void *)ptr_pkt_desc, 0, sizeof(TI_PP_PACKET_DESC));
    }

    /* Return the pointer to the L3 header. */
    return (char *)ptr_iphdr;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_classify_pdsp
 **************************************************************************
 * DESCRIPTION   :
 *  Simulation code for the Classify PDSP.
 *
 * RETURNS       :
 *      Non-Zero    -   Session Handle
 *      <0          -   Unable to classify the packet.
 **************************************************************************/
static int ti_pp_classify_pdsp(struct sk_buff* skb)
{
    int                 session_handle = -1;
    int                 control_flags = 0;
    INTERNAL_PPM_VPID*  ptr_vpid = NULL;
    char*               ptr_l3hdr;
    TI_PP_SESSION       session_info;

    /* Increment the global statistics. */
    pp_stats.packets_rxed++;

    /* Initialize the session information block */
    memset ((void *)&session_info, 0, sizeof (TI_PP_SESSION));

    /* Packets should only be received on the PID. Thus verify if this is the case. */
    if (skb->dev->pid_handle == -1)
    {
        printk ("FATAL Error: Packets should only be received by the PID but were received on %s\n", skb->dev->name);
        return -1;
    }

    /* Make sure the PID is up and running. */
    if (pid_table[skb->dev->pid_handle].status == 0)
    {
        printk ("FATAL Error: PID %d for device %s is DOWN\n", skb->dev->pid_handle, skb->dev->name);
        return -1;
    }

    /* Extract the fields from the Packet. */
    ptr_l3hdr = ti_pp_extract_pkt_fields (skb->mac.raw, &session_info, &control_flags);
    if (ptr_l3hdr == NULL)
        return -1;

    /* Lookup the session from the LUT Table; if there is no match then classification PDSP has failed
     * and the packet needs to be pushed up to the host. */
    session_handle = ti_pp_lut_lookup (&session_info);
    if (session_handle == -1)
        return -1;

    /* Retrieve the Ingress VPID handle */
    ptr_vpid = ti_pp_get_vpid(session_table[session_handle].session.ingress.vpid_handle);
    if(ptr_vpid == NULL)
    {
        printk ("FATAL Error: Ingress VPID %d does not exist\n", session_table[session_handle].session.ingress.vpid_handle);
        return -1;
    }

    /* Store the Layer3 header into the session information block. This is needed later on because the L2
     * framing is appended after this. */
    session_table[session_handle].ptr_l3_header = ptr_l3hdr;

#ifdef TI_PP_DEBUG
    /* Classification was a success. */
    printk ("Packet 0x%p with len %d bytes received on %s VPID: %d PID: %d Session %d\n",
            skb, skb->len, skb->dev->name, session_table[session_handle].session.ingress.vpid_handle,
            skb->dev->pid_handle, session_handle);
#endif /* TI_PP_DEBUG */

    /* Search Match; increment the statistics */
    pp_stats.search_matched++;

    /* Check if the destination MAC address has been specified while creating the session. */
    if ((session_info.ingress.l2_packet.u.eth_desc.enables & TI_PP_SESSION_L2_DSTMAC_VALID) &&
        (session_info.ingress.l2_packet.u.eth_desc.dstmac[0] & 1))
    {
        /* YES. The packet could be broadcast or multicast */
        if (session_info.ingress.l2_packet.u.eth_desc.dstmac[1] == 0xFF)
        {
            session_table[session_handle].packet_class = INTERNAL_BROADCAST;
            ptr_vpid->stats.rx_broadcast_pkt++;
        }
        else
        {
            session_table[session_handle].packet_class = INTERNAL_MULTICAST;
            ptr_vpid->stats.rx_multicast_pkt++;
        }
    }
    else
    {
        /* Unicast Packet. */
        session_table[session_handle].packet_class = INTERNAL_UNICAST;
        ptr_vpid->stats.rx_unicast_pkt++;
    }

    /* Increment the number of bytes received. */
    ptr_vpid->stats.rx_byte_lo += skb->len;

    /* There has been some activity; so reset the inactvity timeout if a timeout had been specified. */
    if(session_table[session_handle].session.session_timeout != 0)
        mod_timer(&session_table[session_handle].session_timer, jiffies + session_table[session_handle].session.session_timeout);

    /* If the sessions is a routable session then we will push the FTP Control packets up to the host. */
    if (session_table[session_handle].session.is_routable_session)
    {
        /* Check if the packet is a control packet and if so pass it to the host. */
        if (control_flags == 1)
        {
            printk ("Control Flag is SET; packet is passed to the host\n");
            return -1;
        }
    }

    /* Classification was successful; return the session handle */
    return session_handle;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_layer4_checksum
 **************************************************************************
 * DESCRIPTION   :
 *  The function calculates the Layer4 checksum.
 **************************************************************************/
static void ti_pp_layer4_checksum
(
    struct iphdr*  ptr_iphdr,
    char*          ptr_l4header,
    unsigned int   l4_protocol,
    unsigned short data_payload_length
)
{
    struct tcphdr*  ptr_tcphdr;
    struct udphdr*  ptr_udphdr;
    struct icmphdr* ptr_icmphdr;

    /* Dont proceed if there is no Layer4 header. */
    if (ptr_l4header == NULL)
        return;

    if (l4_protocol == IPPROTO_TCP)
    {
        unsigned int checksum;
        unsigned int checksum1;

        /* TCP Packet: */
        ptr_tcphdr = (struct tcphdr *)ptr_l4header;

        /* Initialize the checksum field to be 0. */
        ptr_tcphdr->check = 0;

        /* Compute the checksum of the TCP Pseudo Header and for the data payload. Dont fold it until later. */
        checksum = csum_tcpudp_nofold (ptr_iphdr->saddr, ptr_iphdr->daddr, data_payload_length, IPPROTO_TCP, 0);

        /* Compute the checksum over the entire data payload. */
        checksum1 = csum_partial(ptr_l4header, data_payload_length, checksum);

        /* Now combine the Pseudo-Header checksum and datapayload checksum and fold it. */
        ptr_tcphdr->check = csum_fold (checksum1);
        return;
    }

    if (l4_protocol == IPPROTO_UDP)
    {
        unsigned int checksum;
        unsigned int checksum1;

        /* UDP Packet: */
        ptr_udphdr = (struct udphdr *)ptr_l4header;

        /* Initialize the checksum field to be 0. */
        ptr_udphdr->check = 0;

        /* Compute the checksum of the UDP Pseudo Header and for the data payload. Dont fold it until later. */
        checksum = csum_tcpudp_nofold (ptr_iphdr->saddr, ptr_iphdr->daddr, data_payload_length, IPPROTO_UDP, 0);

        /* Compute the checksum over the entire data payload. */
        checksum1 = csum_partial(ptr_l4header, data_payload_length, checksum);

        /* Now combine the Pseudo-Header checksum and datapayload checksum and fold it. */
        ptr_udphdr->check = csum_fold (checksum1);
        return;
    }

    if (l4_protocol == IPPROTO_ICMP)
    {
        /* ICMP Packet: */
        ptr_icmphdr = (struct icmphdr *)ptr_l4header;
        ptr_icmphdr->checksum = 0;
        ptr_icmphdr->checksum = ip_compute_csum (ptr_l4header, data_payload_length);
        return;
    }

    printk ("Layer4 Protocol: %d Currently not supported\n", l4_protocol);
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_modify_pdsp
 **************************************************************************
 * DESCRIPTION   :
 *  Simulation code for the Modify PDSP. The function modifies only the L3
 *  and L4 header of the packet.
 *
 * RETURNS       :
 *      0    -   Success
 *      <0   -   Error
 **************************************************************************/
static int ti_pp_modify_pdsp(struct sk_buff* skb, int session_handle)
{
    TI_PP_SESSION*          ptr_session;
    struct iphdr*           ptr_iphdr;
    struct tcphdr*          ptr_l4header;
    TI_PP_SESSION_PROPERTY* ptr_egress;
    unsigned short          data_payload_length;
    unsigned short          layer4_checksum = 0;

    /* Get the pointer to the session. */
    ptr_session = &session_table[session_handle].session;

    /* Check what all needs to be modified if any? In the case of bridged sessions we dont do any
     * modifications at layer3 or layer 4. */
    if (ptr_session->is_routable_session == 0)
        return 0;

    /* Get the pointer to the Egress Session Property. */
    ptr_egress = &ptr_session->egress[0];

    /* Get the pointer to the Layer3 header. */
    ptr_iphdr = (struct iphdr *)session_table[session_handle].ptr_l3_header;

    /* We know for sure we are a routed session so decrement the TTL in the IP Header. */
    ptr_iphdr->ttl = ptr_iphdr->ttl - 1;
    if (ptr_iphdr->ttl == 0)
        return -1;

    /* Initialize the data payload length. */
    data_payload_length = ntohs(ptr_iphdr->tot_len) - (ptr_iphdr->ihl*4);

    /* Get the pointer to the Layer4 Information. */
    ptr_l4header = (struct tcphdr *)((char *)ptr_iphdr + ptr_iphdr->ihl*4);

    /* Check if the L3/L4 fields were described in the Egress? */
    if (ptr_egress->l3l4_packet.packet_type == TI_PP_IPV4_TYPE)
    {
        /* Was there a Destination IPv4 mangling specified? */
        if (ptr_egress->l3l4_packet.u.ipv4_desc.enables & TI_PP_SESSION_IPV4_DSTIP_VALID)
        {
            /* Optimizations: Dont mangle blindly do it only in required.  */
            if (ptr_iphdr->daddr != ptr_egress->l3l4_packet.u.ipv4_desc.dst_ip)
            {
                /* Modify the field; this change implies that Layer4 checksums need to be recalculated also. */
                ptr_iphdr->daddr = ptr_egress->l3l4_packet.u.ipv4_desc.dst_ip;
                layer4_checksum = 1;
            }
        }

        /* Was there a Source IPv4 mangling specified? */
        if (ptr_egress->l3l4_packet.u.ipv4_desc.enables & TI_PP_SESSION_IPV4_SRCIP_VALID)
        {
            /* Optimizations: Dont mangle blindly do it only in required.  */
            if (ptr_iphdr->saddr != ptr_egress->l3l4_packet.u.ipv4_desc.src_ip)
            {
                /* Modify the field; this change implies that Layer4 checksums need to be recalculated also. */
                ptr_iphdr->saddr = ptr_egress->l3l4_packet.u.ipv4_desc.src_ip;
                layer4_checksum = 1;
            }
        }

        /* Was there a Destination Port mangling specified? */
        if (ptr_egress->l3l4_packet.u.ipv4_desc.enables & TI_PP_SESSION_IPV4_DST_PORT_VALID)
        {
            /* Optimizations: Dont mangle blindly do it only in required.  */
            if (ptr_l4header->dest != ptr_egress->l3l4_packet.u.ipv4_desc.dst_port)
            {
                /* Modify the field; this change implies that Layer4 checksums need to be recalculated also. */
                ptr_l4header->dest = ptr_egress->l3l4_packet.u.ipv4_desc.dst_port;
                layer4_checksum = 1;
            }
        }

        /* Was there a Source Port mangling specified? */
        if (ptr_egress->l3l4_packet.u.ipv4_desc.enables & TI_PP_SESSION_IPV4_SRC_PORT_VALID)
        {
            /* Optimizations: Dont mangle blindly do it only in required.  */
            if (ptr_l4header->source != ptr_egress->l3l4_packet.u.ipv4_desc.src_port)
            {
                /* Modify the field; this change implies that Layer4 checksums need to be recalculated also. */
                ptr_l4header->source = ptr_egress->l3l4_packet.u.ipv4_desc.src_port;
                layer4_checksum = 1;
            }
        }
    }
    else
    {
        /* TherNo L4 Modificationse were no Egress L3/L4 packet characteristics specified. Send the original packet as is */
        printk ("Modify PDSP: No Egress L3/L4 info; no modifications done\n");
    }

    /* Always recompute the IP Header Checksum; since the TTL has changed if not anything else. */
	ptr_iphdr->check = 0;
	ptr_iphdr->check = ip_fast_csum((unsigned char *)ptr_iphdr, ptr_iphdr->ihl);

    /* Recompute the layer4 checksum only if detected so. */
    if (layer4_checksum)
        ti_pp_layer4_checksum (ptr_iphdr, (char *)ptr_l4header, ptr_iphdr->protocol, data_payload_length);

    /* Packet has been modified. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_frame_transmit_packet
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called from the QoS PDSP Simulation code to transmit a
 *  packet on a specific "Egress" Interface. The function is also responsible
 *  for doing the necessary L2 framing associated with the VPID.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
static int ti_pp_frame_transmit_packet
(
    struct sk_buff*         skb,
    TI_PP_SESSION_PROPERTY* ptr_egress_session_property,
    int                     session_handle
)
{
    INTERNAL_SR_SESSION* ptr_session;
    struct net_device*   pid_dev;
    INTERNAL_PPM_VPID*   ptr_egress_vpid;
    INTERNAL_PPM_VPID*   ptr_ingress_vpid;
    char*                ptr_data;
    unsigned short       protocol_type = 0;
    struct iphdr*        ptr_iphdr;

    /* Get the pointer to the Session. */
    ptr_session = &session_table[session_handle];

    /* Get the pointer to the Egress VPID. */
    ptr_egress_vpid = &vpid_table[ptr_egress_session_property->vpid_handle];
    if (ptr_egress_vpid == NULL)
    {
        printk ("FATAL Error: Egress VPID %d does not exist\n", ptr_egress_session_property->vpid_handle);
        return -1;
    }

    /* Get the pointer to the Ingress VPID. */
    ptr_ingress_vpid = &vpid_table[ptr_session->session.ingress.vpid_handle];
    if (ptr_ingress_vpid == NULL)
    {
        printk ("FATAL Error: Ingress VPID %d does not exist\n", ptr_session->session.ingress.vpid_handle);
        return -1;
    }

    /* Linux based semantics: We need to convert the PID handle to a network device object
     * so that we can transmit the packet. */
	for (pid_dev = dev_base; pid_dev; pid_dev = pid_dev->next)
    {
        /* Check the Parent PID in the VPID to the PID handle in the system. */
	    if (pid_dev->pid_handle == ptr_egress_vpid->vpid_info.parent_pid_handle)
			break;
    }

    /* If there is no matching network device object found! Abort since we cant pass the
     * packet to the driver. */
    if (pid_dev == NULL)
    {
        printk ("FATAL Error: No network device associated with PID %d\n",ptr_egress_vpid->vpid_info.parent_pid_handle);
        return -1;
    }

    /* Time to send the packet out on the interface. But before we can do this we need to add the appropriate
     * Layer2 Framing. Start from the IP header and work backwards. */
    ptr_data = ptr_session->ptr_l3_header;
    ptr_iphdr = (struct iphdr *)ptr_data;
    skb->len = ntohs(ptr_iphdr->tot_len);

    /* Decide the type of protocol which needs to be added.
     *  The type of L2 header being added depends on the networking endpoint.
     * NOTE: The "protocol_type" field will get initialized to what goes in the
     * outermost ethernet header. */
    switch (ptr_egress_vpid->vpid_info.type)
    {
        case TI_PP_ETHERNET:
        {
            /* The Egress VPID is vanilla ethernet. The protocol type which can be
             * abstracted here can only be IPv4 or IPv6. Currently we support only IPv4 */
            protocol_type = htons (ETH_P_IP);
            break;
        }
        case TI_PP_VLAN:
        {
            /* The Egress VPID is VLAN over Ethernet. */
            struct vlan_hdr*   ptr_vlan_header;

            /* The protocol type at this layer can only be 8021Q */
            protocol_type = htons (ETH_P_8021Q);

            /* Construct the VLAN header. */
            ptr_data = ptr_data - sizeof(struct vlan_hdr);
            ptr_vlan_header = (struct vlan_hdr *)ptr_data;

            /* Get the VLAN ID from the Egress VPID */
            ptr_vlan_header->h_vlan_TCI = ptr_egress_vpid->vpid_info.vlan_identifier;

            /* The encapsulated protocol can only be IPv4 or IPv6. Currently we only support IPv4. */
            ptr_vlan_header->h_vlan_encapsulated_proto = htons (ETH_P_IP);

            /* Once the VLAN Header has been added account for it in the length. */
            skb->len = skb->len + sizeof(struct vlan_hdr);
            break;
        }
        case TI_PP_PPPoE:
        {
            /* The Egress VPID PPPoE. */
            struct pppoe_hdr*   ptr_ppp_header;

            /* The protocol type at this layer can only be PPP Session. */
            protocol_type = htons (ETH_P_PPP_SES);

            /* Move back the data pointer to allow space to put the PPPoE Header. */
            ptr_data = ptr_data - sizeof(struct pppoe_hdr) - 2;

            /* Get the pointer to the PPPoE Header and fill it. */
            ptr_ppp_header = (struct pppoe_hdr *)ptr_data;
            ptr_ppp_header->ver    = 1;
            ptr_ppp_header->type   = 1;
            ptr_ppp_header->code   = 0;
        	ptr_ppp_header->length = htons(skb->len + 2);

            /* Get the Session ID from the modification record. */
            ptr_ppp_header->sid = ptr_egress_vpid->vpid_info.ppp_session_id;

            /* Populate the point to point protocol to be IP */
            *(ptr_data + sizeof(struct pppoe_hdr))     = 0;
            *(ptr_data + sizeof(struct pppoe_hdr) + 1) = 0x21;

            /* Once the PPP Header has been account for it in the length. */
            skb->len = skb->len + 2 + sizeof(struct pppoe_hdr);
            break;
        }
        case TI_PP_VLAN_PPPoE:
        {
            /* The Egress VPID is PPP executing over a VLAN connection. */
            struct pppoe_hdr*   ptr_ppp_header;
            struct vlan_hdr*    ptr_vlan_header;

            /* Move back the data pointer to allow space for the PPP header. */
            ptr_data = ptr_data - sizeof(struct pppoe_hdr) - 2;

            /* Get the pointer to the PPPoE Header and fill it. */
            ptr_ppp_header = (struct pppoe_hdr *)ptr_data;
            ptr_ppp_header->ver    = 1;
            ptr_ppp_header->type   = 1;
            ptr_ppp_header->code   = 0;
        	ptr_ppp_header->length = htons(skb->len + 2);

            /* Get the Session ID from the modification record. */
            ptr_ppp_header->sid = ptr_egress_vpid->vpid_info.ppp_session_id;

            /* Populate the point to point protocol to be IP */
            *(ptr_data + sizeof(struct pppoe_hdr))     = 0;
            *(ptr_data + sizeof(struct pppoe_hdr) + 1) = 0x21;

            /* Move back the data pointer to allow space for the VLAN header. */
            ptr_data = ptr_data - sizeof(struct vlan_hdr);
            ptr_vlan_header = (struct vlan_hdr *)ptr_data;

            /* Get the VLAN ID from the Egress VPID */
            ptr_vlan_header->h_vlan_TCI = ptr_egress_vpid->vpid_info.vlan_identifier;

            /* The encapsulated protocol can only be PPP Session */
            ptr_vlan_header->h_vlan_encapsulated_proto = htons (ETH_P_PPP_SES);

            /* The protocol type at this layer can only be 8021Q */
            protocol_type = htons (ETH_P_8021Q);

            /* Once the PPP Header has been account for it in the length. */
            skb->len = skb->len + 2 + sizeof(struct pppoe_hdr) + sizeof(struct vlan_hdr);
            break;
        }
        default:
        {
            /* Unknown Protocol Type present. This should have been detected by the PPM. */
            printk ("FATAL Error: PPM did not detect unknown VPID type %d\n", ptr_egress_session_property->vpid_handle);
            return -1;
        }
    }

    /* The outermost header is always an Ethernet header so move back the data pointer to allow
     * space for it. */
    ptr_data = ptr_data - sizeof(struct ethhdr);

    /* Copy the destination and source mac address from the Egress Session Property. But before
     * we proceed check if the L2 properites were specified or not? */
    if (ptr_egress_session_property->l2_packet.packet_type == TI_PP_ETH_TYPE)
    {
        /* L2 header has been specified; ensure that the destination and source mac fields
         * have been provided. */
        if (ptr_egress_session_property->l2_packet.u.eth_desc.enables &
            (TI_PP_SESSION_L2_DSTMAC_VALID|TI_PP_SESSION_L2_SRCMAC_VALID))
        {
            /* Copy the destination, source mac address and protocol type. */
            memcpy ((void *)ptr_data, (void *)&ptr_egress_session_property->l2_packet.u.eth_desc.dstmac, 6);
            memcpy ((void *)ptr_data+6, (void *)&ptr_egress_session_property->l2_packet.u.eth_desc.srcmac, 6);
            memcpy ((void *)(ptr_data+12), (void *)&protocol_type, 2);
        }
        else
        {
            /* Destination and Source MAC Address were not specified. So how do we frame the
             * ethernet header. If the session was a bridged session we could use the same L2
             * header as what was received to send the packet out. */
            if ((ptr_session->session.is_routable_session == 0) )
            {
                /* This was a bridged session we can reuse the same MAC header as what was received. */
                printk ("No Dst/Src MAC Address specified; but bridged session detected reusing....\n");
                ptr_data = skb->mac.raw;
            }
            else
            {
                /* This was a routable session and there was not enough information to add the new L2 header.
                 * We cannot recover from this situation; report the error and abort! */
                printk ("FATAL Error: Missing Dst. and Src. MAC Address and session was routable; framing failed\n");
                return -1;
            }
        }
    }
    else
    {
        /* The L2 properties were not specified. So how do we frame the ethernet header.
         * If the session was a bridged session we could use the same L2 header as
         * what was received to send the packet out. */
        if ((ptr_session->session.is_routable_session == 0) )
        {
            /* This was a bridged session */
            printk ("No L2 Packet Info specified; but bridged session detected reusing....\n");
            ptr_data = skb->mac.raw;
        }
        else
        {
            /* This is a routed session and so this condition cannot be handled. We just did not
             * have sufficient information to handle this. */
            printk ("FATAL Error: Session %d is routed but no L2 information is defined\n", session_handle);
            return -1;
        }
    }

    /* Increment the length to account for the new Ethernet header which was added. */
    skb->len = skb->len + sizeof(struct ethhdr);

    /* Increment the statistics based on the class of packet. */
    switch (ptr_session->packet_class)
    {
        case INTERNAL_UNICAST:
        {
            ptr_egress_vpid->stats.tx_unicast_pkt++;
            break;
        }
        case INTERNAL_MULTICAST:
        {
            ptr_egress_vpid->stats.tx_multicast_pkt++;
            break;
        }
        case INTERNAL_BROADCAST:
        {
            ptr_egress_vpid->stats.tx_broadcast_pkt++;
            break;
        }
        default:
        {
            printk ("FATAL Error: Unknown packet class 0x%x detected\n", ptr_session->packet_class);
            return -1;
        }
    }

    /* Increment the number of bytes transmitted. */
    ptr_egress_vpid->stats.tx_byte_lo += skb->len;

    /* Increment the Session statistics. */
    session_table[session_handle].stats.packets_forwarded++;
    session_table[session_handle].stats.bytes_forwarded_lo += skb->len;

    /* Increment the global statistics. */
    pp_stats.packets_fwd++;
    pp_stats.ipv4_packets_fwd++;

    /* Initialize the packet to the start after the headers have been added. */
    skb->data = ptr_data;
    skb->dev  = pid_dev;

    /* Before we send the packet out ensure that the packet meets the MTU requirements. */
    if (skb->len > ptr_egress_vpid->vpid_info.egress_mtu)
    {
        /* We will need to fragment the packet before it can be sent out. */
        printk ("Packet Length %d exceeds MTU %d; fragmentation required\n", skb->len, ptr_egress_vpid->vpid_info.egress_mtu);
        return 0;
    }

#ifdef TI_PP_DEBUG
    printk ("Packet 0x%p with len %d bytes transmitted on %s VPID: %d PID: %d Session %d\n",
            skb, skb->len, skb->dev->name, session_table[session_handle].session.ingress.vpid_handle,
            skb->dev->pid_handle, session_handle);
#endif /* TI_PP_DEBUG */

    /* Transmit the packet. */
    pid_dev->hard_start_xmit(skb, pid_dev);

    /* Packet has been successfully passed to the drivers for transmission. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_qos_pdsp
 **************************************************************************
 * DESCRIPTION   :
 *  Simulation code for the QoS PDSP.
 *
 * RETURNS       :
 *      0    -   Success
 *      <0   -   Error
 **************************************************************************/
static int ti_pp_qos_pdsp (struct sk_buff* skb, int session_handle)
{
    INTERNAL_SR_SESSION*    ptr_session;
    int                     egress_vpid_index = 0;
    TI_PP_SESSION_PROPERTY* ptr_egress_session_property;

    /* Get the pointer to the Session. */
    ptr_session = &session_table[session_handle];

    /* We need to cycle through all the Egress VPID for the packet. If the number of Egress
     * Interfaces are more than 1; that for each extra interface the packet needs to be copied. */
    while (egress_vpid_index < (ptr_session->session.num_egress - 1))
    {
        /* Create a duplicate packet. */
        struct sk_buff* duplicate_skb = skb_copy (skb, GFP_KERNEL);
        if (duplicate_skb == NULL)
        {
            printk ("FATAL Error: No memory available for copying the packet\n");
            return -1;
        }

        /* Get the Egress Session Property */
        ptr_egress_session_property = &ptr_session->session.egress[egress_vpid_index];

        /* Transmit the packet on the specified "Egress" VPID. */
        if (ti_pp_frame_transmit_packet (duplicate_skb, ptr_egress_session_property, session_handle) < 0)
            return -1;

        /* Goto the next VPID. */
        egress_vpid_index++;
    }

    /* Get the Egress Session Property */
    ptr_egress_session_property = &ptr_session->session.egress[egress_vpid_index];

    /* Packet has to be transmitted on the last (or the first if there was only one) VPID;In either case
     * send out the original packet here. */
    if (ti_pp_frame_transmit_packet (skb, ptr_egress_session_property, session_handle) < 0)
        return -1;

    /* Packet was succesfully transmitted. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_simulate_sr
 **************************************************************************
 * DESCRIPTION   :
 *  Simulation code for the Packet Processor.
 *
 * RETURNS       :
 *      1   -   Packet has been acclerated.
 *      0   -   Packet could not be acclerated.
 **************************************************************************/
int ti_pp_simulate_sr(struct sk_buff* skb)
{
    int session_handle;

    /**********************************************************************
     ****************************** PDSP1: Classify PDSP ******************
     **********************************************************************/
    session_handle = ti_pp_classify_pdsp(skb);
    if (session_handle < 0)
    {
        /* Classification failed: Pass the packet to the host. */
        return 0;
    }

    /**********************************************************************
     ****************************** PDSP2: Modify PDSP ********************
     **********************************************************************/
    if (ti_pp_modify_pdsp (skb, session_handle) < 0)
    {
        /* Modification failed: */
        printk ("Error: Modification PDSP Failed\n");
        return 0;
    }

    /**********************************************************************
     ****************************** PDSP3: QoS PDSP ***********************
     **********************************************************************/
    if (ti_pp_qos_pdsp (skb, session_handle) < 0)
    {
        /* QoS PDSP failed: */
        printk ("Error: QoS PDSP Failed\n");
        return 0;
    }
    /* If we have reached this stage; then the packet has succesfully passed through
     * all the stages. It does not need to be routed/bridged. */
    return 1;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_health_check
 **************************************************************************
 * DESCRIPTION   :
 *  The function returns the status of the PDSP
 *
 * RETURNS       :
 *      0   -   Success
 *      0x1 -   CPDSP Error
 *      0x2 -   MPDSP Error
 *      0x4 -   QPDSP Error
 *      0x8 -   Other Error
 **************************************************************************/
int ti_pp_health_check (void)
{
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_initialize
 **************************************************************************
 * DESCRIPTION   :
 *  Initialize the Simulation code for the Session Router.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_pp_initialize(unsigned int num_of_fw, TI_PP_FIRMWARE *firmware, TI_PPD_CONFIG *ptr_cfg)
{
    /* Initialize the database */
    memset ((void *)&session_table, 0, sizeof(session_table));

    /* Initialize the Global Statistics. */
    memset((void *)&pp_stats, 0 , sizeof(TI_PP_GLOBAL_STATS));

    /* Succesful Initialized the Simulation Code. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_pp_deinitialize
 **************************************************************************
 * DESCRIPTION   :
 *  De-initialize the Simulation code for the Session Router.
 *
 * RETURNS       :
 *      0   -   Success
 *      <0  -   Error
 **************************************************************************/
int ti_pp_deinitialize (void)
{
    return 0;
}
