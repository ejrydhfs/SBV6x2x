/*
 * hil_intrusive.c - HIL Intrusive Mode Profile
 *
 * Description:
 *  The file contains the implementation of the HIL Intrusive Mode Profile.
 *  The profile use the following kernel features
 *   - CONFIG_TI_DEVICE_PROTOCOL_HANDLING
 *   - CONFIG_TI_EGRESS_HOOK
 *  The profile install hooks into the Ingress and Egress Data Path and
 *  populates the session structure which is stored in the SKB for the LUT
 *  and Modification record configuration. These hooks are installed on
 *  networking devices which have an PID or VPID handle associated with it.
 *
 *  The profile is provided as is and can be used as a template for
 *  development of more system specific profiles.
 *
 * Copyright (C) <2008>, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

/**************************************************************************
 *************************** Include Files ********************************
 **************************************************************************/

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/if_pppox.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ppp_defs.h>
#ifdef CONFIG_NETFILTER
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_protocol.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#endif
#include <linux/ti_hil.h>
#include <linux/mroute.h>
#include <net/ip.h>
#include "ti_ppm.h"
#include "../../bridge/br_private.h"
#include <linux/proc_fs.h>

/**************************************************************************
 ************************************ Local Definitions *******************
 **************************************************************************/

/* Definitons required for the HIL Analysis. */
#define HIL_SESSION_STAT_BUCKET           64
#define HIL_MAX_NUM_BUCKETS               TI_PP_MAX_ACCLERABLE_SESSIONS / HIL_SESSION_STAT_BUCKET

/* PDSP Health Timer; to periodically check if the PDSP are executing properly or not. */
#define PDSP_HEALTH_TIMER                 60

/**************************************************************************
 *************************** Static Definitions ***************************
 **************************************************************************/

/* Debug Dumping Functions */
static void ti_hil_intrusive_display_l2         (TI_PP_ETH_DESC*    ptr_eth_desc);
static void ti_hil_intrusive_display_ipv4       (TI_PP_IPV4_DESC*   ptr_ipv4_lut_entry);
static void ti_hil_intrusive_display_session    (int session_handle);

/* Profile Initialization/Deinitialization and networking event handlers. */
static int ti_hil_intrusive_init (void);
static int ti_hil_netsubsystem_event_handler(unsigned int module_id, unsigned long event_id, void* ptr);
static int ti_hil_intrusive_deinit(void);

#ifdef CONFIG_IP_MULTICAST
extern struct vif_device vif_table[MAXVIFS];
static int hil_mfc_check_entry (unsigned int mcast_group);
static int hil_mfc_add_entry (unsigned int mcast_group, unsigned char session_handle);
static int hil_mfc_del_entry (unsigned int mcast_group);
static int hil_mfc_to_session(struct mfc_cache* ptr_mfc_cache, TI_PP_SESSION* ptrsession);
#endif /* CONFIG_IP_MULTICAST */

/**************************************************************************
 ********************************* Globals ********************************
 **************************************************************************/

/* Default Profile. */
TI_HIL_PROFILE      hil_intrusive_profile = {
    .name            = "intrusive",
    .profile_handler = ti_hil_netsubsystem_event_handler,
    .profile_init    = ti_hil_intrusive_init,
    .profile_deinit  = ti_hil_intrusive_deinit,
};

/* Packet Processor Subsystem Event Handler. */
unsigned int        ppsubsystem_event_handler;

/* PP PDSP Health Timer to verify if the PP is running correctly. */
struct timer_list   pdsp_health_timer;

/* Counters for keeping track of stats for the HIL Analysis. */
unsigned int        hil_num_total_sessions = 0;
unsigned int        hil_num_error = 0;
unsigned int        hil_session[HIL_MAX_NUM_BUCKETS];

#ifdef CONFIG_NETFILTER
/* This is a global data structure which maps the session handle to corresponding
 * connection tracking entries. */
struct ip_conntrack* hil_session_ct_mapper [TI_PP_MAX_ACCLERABLE_SESSIONS];
#endif /* CONFIG_NETFILTER */

#ifdef CONFIG_IP_MULTICAST
unsigned int hil_mfc_session_mapper[TI_PP_MAX_ACCLERABLE_SESSIONS];
#endif /* CONFIG_IP_MULTICAST */

/* This is the default session IDLE timeout in microseconds. */
int ti_session_timeout = 30 * HZ * 1000;

/* cable_pp: disable/enable capability */

struct
{
    int                 hil_disabled;
    int                 dbg_disabled;
    int                 tdox_disabled;

    unsigned int        num_bypassed_pkts;
    unsigned int        num_other_pkts;
    unsigned int        num_ingress_pkts;
    unsigned int        num_egress_pkts;
    unsigned int        num_null_drop_pkts;
}
global_ti_hil_db =
{
    .hil_disabled       =   0,
    .dbg_disabled       =   1,
    .tdox_disabled      =   0,
    .num_bypassed_pkts  =   0,
    .num_other_pkts     =   0,
    .num_ingress_pkts   =   0,
    .num_egress_pkts    =   0,
    .num_null_drop_pkts =   0
};


/* Packet terminating VPID handle */
static int  docsis_null_vpid_handle = -1;

/************************************************************************/
/*                                                                      */
/*                                                                      */
/*     TDOX stuff                                                       */
/*                                                                      */
/*                                                                      */
/************************************************************************/
#define TI_HIL_TDOX_MAX_SESSIONS        16

typedef struct tdox_db_entry_t
{
    struct tdox_db_entry_t *    next;
    int                         pp_session_handle;
    int                         tdox_session_handle;
}
tdox_db_entry_t;

static tdox_db_entry_t  tdox_db_repository[TI_HIL_TDOX_MAX_SESSIONS];

typedef struct
{
    struct tdox_db_entry_t *    activeList;
    int                         activeNum;
    struct tdox_db_entry_t *    freeList;
    int                         freeNum;
}
tdox_db_ctrl_t;

static tdox_db_ctrl_t   tdox_db;
/************************************************************************/


/**************************************************************************
 *************************** Extern Definitions ***************************
 **************************************************************************/

extern int ti_deregister_egress_hook_handler (struct net_device* dev);
extern int ti_register_egress_hook_handler (struct net_device* dev, int (*egress_hook)(struct sk_buff *skb));

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
/* DOCSIS Packet processor start session notification Callback */
extern TI_HIL_START_SESSION ti_hil_start_session_notification_cb;
/* DOCSIS Packet processor delete session notification Callback */
extern TI_HIL_DELETE_SESSION ti_hil_delete_session_notification_cb;
#endif /* CONFIG_TI_PACKET_PROCESSOR_STATS */

/**************************************************************************
 ******************************* Functions  *******************************
 **************************************************************************/

#define TDOX_LOCK(context)
#define TDOX_UNLOCK(context)

/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_alloc_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function allocates TDOX session handle.
 *
 * RETURNS:
 *   0  -  Session is allocated.
 *  -1  -  There are no TDOX IDs available at the time.
 **************************************************************************/
static int ti_hil_tdox_alloc_session (void)
{
    struct tdox_db_entry_t * tmp;

    TDOX_LOCK(os_context);

    if (0 == tdox_db.freeNum)
    {
        TDOX_UNLOCK(os_context);
        return -1;
    }

    tdox_db.freeNum--;

    tmp =                   tdox_db.freeList;
    tdox_db.freeList =      tdox_db.freeList->next;
    tmp->next =             tdox_db.activeList;
    tdox_db.activeList =    tmp;

    tdox_db.activeNum++;

    TDOX_UNLOCK(os_context);

#ifdef CONFIG_TI_HIL_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Allocate Session %d ====\n",tdox_db.activeList->tdox_session_handle);
    }
#endif

    return (tdox_db.activeList->tdox_session_handle);
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_associate_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function associates PP session to TDOX session handle.
 *
 * RETURNS:
 *   0  -  Session is associated.
 *  -1  -  Session parameters are invalid.
 **************************************************************************/
static int ti_hil_tdox_associate_session (unsigned char tdox_handle, int pp_session_handle)
{
    if (tdox_handle >= TI_HIL_TDOX_MAX_SESSIONS)
    {
        return -1;
    }

    if ((pp_session_handle < 0) || (pp_session_handle >=TI_PP_MAX_ACCLERABLE_SESSIONS))
    {
        return -1;
    }

#ifdef CONFIG_TI_HIL_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Associate tdox Session %d = pp %d ====\n", tdox_handle, pp_session_handle);
    }
#endif

    tdox_db_repository[tdox_handle].pp_session_handle = pp_session_handle;
    return 0;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_free_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function releases TDOX ID.
 *  The search can be done either per PP or TDOX session handle key.
 *  In case one of the search keys is unknown, the -1 should be specified.
 *
 * RETURNS:
 *   0  -  Session is freed.
 *  -1  -  Session is NOT found.
 **************************************************************************/
static int ti_hil_tdox_free_session (int pp_session_handle, int tdox_session_handle)
{
    tdox_db_entry_t *   curr;
    tdox_db_entry_t *   prev;
    tdox_db_entry_t *   tmp;

    TDOX_LOCK(os_context);

    if (0 == tdox_db.activeNum)
    {
        TDOX_UNLOCK(os_context);
        return -1;
    }

#ifdef CONFIG_TI_HIL_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Free Session [%d] START     ====\n",pp_session_handle);
    }
#endif

    curr = tdox_db.activeList;
    prev = NULL;

    do
    {
        if (
            ((curr->pp_session_handle != -1)   && (curr->pp_session_handle == pp_session_handle)) ||
            ((curr->tdox_session_handle != -1) && (curr->tdox_session_handle == tdox_session_handle))
           )
        {
            tdox_db.activeNum--;

            if (prev)
            {
                prev->next =         curr->next;
            }
            else
            {
                tdox_db.activeList = curr->next;
            }

            tmp =                    tdox_db.freeList;
            tdox_db.freeList =       curr;
            tdox_db.freeList->next = tmp;
            tdox_db.freeList->pp_session_handle = -1;

            tdox_db.freeNum++;
            TDOX_UNLOCK(os_context);

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk("\n==== TDOX: Free Session [%d] OK        ====\n",pp_session_handle);
            }
#endif
            return 0;
        }

        prev = curr;
        curr = curr->next;
    }
    while (NULL != curr);

    TDOX_UNLOCK(os_context);

#ifdef CONFIG_TI_HIL_DEBUG
    if (0 == global_ti_hil_db.dbg_disabled)
    {
        printk("\n==== TDOX: Free Session [%d] Not Found ====\n",pp_session_handle);
    }
#endif

    return -1;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_init
 **************************************************************************
 * DESCRIPTION   :
 *  The initialization function of TDOX driver internal database.
 *
 * RETURNS       :
 *  0   -   Success
 **************************************************************************/
static int ti_hil_tdox_init (void)
{
    int                 idx;
    tdox_db_entry_t *   prev = NULL;

    for (idx=0; idx < TI_HIL_TDOX_MAX_SESSIONS; idx++)
    {
        tdox_db_repository[idx].tdox_session_handle = idx;
        tdox_db_repository[idx].pp_session_handle = -1;
        tdox_db_repository[idx].next = prev;
        prev = &tdox_db_repository[idx];
    }

    tdox_db.freeList    = prev;
    tdox_db.freeNum     = TI_HIL_TDOX_MAX_SESSIONS;
    tdox_db.activeList  = NULL;
    tdox_db.activeNum   = 0;

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_tdox_print
 **************************************************************************
 * DESCRIPTION   :
 *  Utility function prints out the TDOX driver internal database.
 *
 * RETURNS       :
 *  0   -   Success
 **************************************************************************/
static int ti_hil_tdox_print (void)
{
    unsigned char       idx;

    printk ("\n==== TDOX DB ====\n");

    printk ("  Free [%08X], num [%d]\n", (unsigned int)tdox_db.freeList, tdox_db.freeNum);
    printk ("  Act  [%08X], num [%d]\n", (unsigned int)tdox_db.activeList, tdox_db.activeNum);

    printk ("\n==== TDOX RECORDS ====\n");
    printk ("+-----+------+------------+------------+------+\n");
    printk ("| idx | sess |    addr    |    next    |  PP  |\n");
    printk ("+-----+------+------------+------------+------+\n");

    for (idx=0; idx < TI_HIL_TDOX_MAX_SESSIONS; idx++)
    {
        printk("| %3d | %4d | 0x%08X | 0x%08X | %4d |\n",
                    idx,
                    tdox_db_repository[idx].tdox_session_handle,
                    (unsigned int)&tdox_db_repository[idx],
                    (unsigned int)tdox_db_repository[idx].next,
                    tdox_db_repository[idx].pp_session_handle
                    );
    }
    printk ("+-----+------+------------+------------+------+\n");

    return 0;
}


/************************************************************************/
/*  TCP Options Numbers                                                 */
/************************************************************************/
#define TCP_OPTION_CODE_EOL         0  // 0  End of Option List                     [RFC793]
#define TCP_OPTION_CODE_NOP         1  // 1  No-Operation                           [RFC793]
                                       // 2  Maximum Segment Size                   [RFC793]
                                       // 3  WSOPT - Window Scale                   [RFC1323]
                                       // 4  SACK Permitted                         [RFC2018]
                                       // 5  SACK                                   [RFC2018]
                                       // 6  Echo (obsoleted by option 8)           [RFC1072]
                                       // 7  Echo Reply (obsoleted by option 8)     [RFC1072]
#define TCP_OPTION_CODE_TIMESTAMP   8  // 8  TSOPT - Time Stamp Option              [RFC1323]
                                       // 9  Partial Order Connection Permitted     [RFC1693]
                                       // 10 Partial Order Service Profile          [RFC1693]
                                       // 11 CC                                     [RFC1644]
                                       // 12 CC.NEW                                 [RFC1644]
                                       // 13 CC.ECHO                                [RFC1644]
                                       // 14 TCP Alternate Checksum Request         [RFC1146]
                                       // 15 TCP Alternate Checksum Data            [RFC1146]
                                       // 16 Skeeter                                [Knowles]
                                       // 17 Bubba                                  [Knowles]
                                       // 18 Trailer Checksum Option                [Subbu & Monroe]
                                       // 19 MD5 Signature Option                   [RFC2385]
                                       // 20 SCPS Capabilities                      [Scott]
                                       // 21 Selective Negative Acknowledgements    [Scott]
                                       // 22 Record Boundaries                      [Scott]
                                       // 23 Corruption experienced                 [Scott]
                                       // 24 SNAP                                   [Sukonnik]
                                       // 25 Unassigned (released 2000-12-18)
                                       // 26 TCP Compression Filter                 [Bellovin]
                                       // 27 Quick-Start Response                   [RFC4782]
                                       // 28 User Timeout Option                    [RFC-ietf-tcpm-tcp-uto-11.txt]
/************************************************************************/


#if 0 /* TODO: Enable once router functionality is available */

/**************************************************************************
 * FUNCTION NAME : ti_hil_is_device_routed
 **************************************************************************
 * DESCRIPTION   :
 *  Utility function checks if the device is attached to the IP stack.
 *
 * RETURNS       :
 *  1   -   Device is attached to the IP stack.
 *  0   -   Device is not attached to the IP stack
 **************************************************************************/
static int ti_hil_is_device_routed (struct net_device* dev)
{
    struct in_device *in_dev = in_dev_get(dev);

    if ((in_dev == NULL) || (in_dev->ifa_list == NULL))
        return 0;

    return 1;
}

#endif

/**************************************************************************
 * FUNCTION NAME : ti_hil_extract_packet_desc
 **************************************************************************
 * DESCRIPTION   :
 *  The function is called to extract the various Layer2, Layer3 and
 *  Layer4 fields and populate the packet description structure.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_extract_packet_desc
(
    char*                   ptr_data,
    struct net_device*      dev,
    TI_PP_SESSION_PROPERTY* ptr_ses_property
)
{
    struct ethhdr*      ptr_ethhdr = NULL;
    struct iphdr*       ptr_iphdr = NULL;
    struct tcphdr*      ptr_tcphdr = NULL;
    TI_PP_PACKET_DESC*  ptr_pkt_desc;
    unsigned short      protocol_type;

    /* Check if the device is a PID or not? */
    if (dev->pid_handle != -1)
    {
        /* Valid PID Handle: Packet has been received at the lowest level; at
         * this point  in time we can extract all the L2/L3/L4 information from
         * the packet. */

        if( !ptr_data )
        {
            /* Without this data we cannot continue */
            return -1;
        }

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
            struct vlan_hdr* ptr_vlanheader = (struct vlan_hdr *)ptr_data;

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
                ptr_iphdr = (struct iphdr *)ptr_data;
                break;
            }
            case __constant_htons(ETH_P_IPV6):
            {
                /* Currently there is no support for IPv6. */
                return -1;
            }
            case __constant_htons(ETH_P_PPP_DISC):
            case __constant_htons(ETH_P_PPP_SES):
            {
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
                /* This is a default condition to handle any packet type which is not
                 * understood. These packets are currently not accelerated by the PP. Example
                 * ARP Packets etc. */
                return -1;
            }
        }

        /* Step4: Extract the Layer 4 information only if the packet was an IP Packet. */
        if (ptr_iphdr != NULL)
        {
            /* Currently we support only TCP & UDP. For both these protocols the PORT
             * information lies at the same location. */
            if ((ptr_iphdr->protocol == IPPROTO_TCP) || (ptr_iphdr->protocol == IPPROTO_UDP))
            {
                /* In case of IP Fragmentation - the first fragment has an offset of zero
                If this is the first fragment - the TCP/UDP header exists.
                The flags field uses one bit as the "more fragments" bit.
                This bit is turned on for each fragment comprising a datagram except the final fragment.
                frag_off field includes 3 bit flags and bit fragmant offset - Check if frag offset fiels set to zero
				this packet can be excelrated. Note that fragment offset field is 13 bits wide and we must mask the rest of the bits */
                if ( ( ptr_iphdr->frag_off & ( IP_OFFSET | IP_MF ) ) == 0 )
                {
                    ptr_tcphdr = (struct tcphdr *)((char *)ptr_iphdr + ptr_iphdr->ihl*4);
                }
                else
                {
                    return -1;
                }
            }
        }

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

                /* if TDOX is enabled */
                if (!global_ti_hil_db.tdox_disabled)
                {
                    /* extract ack number */
                    if (ptr_iphdr->protocol == IPPROTO_TCP)
                    {
                        if (ptr_tcphdr->ack)
                        {
                            int     tcp_opt_timestamp_found = 0;
                            char *  ptr_data_tail   = (char *)ptr_iphdr  + ptr_iphdr->tot_len;
                            char *  ptr_data_head   = (char *)ptr_tcphdr + ptr_tcphdr->doff*4;
                            char *  ptr_tcp_options = (char *)ptr_tcphdr + sizeof(struct tcphdr);

                            if (ptr_data_head > ptr_tcp_options)
                            {
                              /********************************************************/
                              /* Go over the options and look for timestamp option    */
                              /********************************************************/
                                while (ptr_tcp_options < ptr_data_head)
                                {
                                    char tcp_opt_type;
                                    char tcp_opt_len;

                                    tcp_opt_type = *(ptr_tcp_options);

                                    if (tcp_opt_type > TCP_OPTION_CODE_NOP)
                                    {
                                      /************************************************/
                                      /* T.L.V. styled option                         */
                                      /************************************************/
                                        tcp_opt_len =  *(ptr_tcp_options + 1);

                                        if (TCP_OPTION_CODE_TIMESTAMP == tcp_opt_type)
                                        {
                                            tcp_opt_timestamp_found = 1;
                                        }

                                        ptr_tcp_options += tcp_opt_len;
                                    }
                                    else
                                    {
                                      /************************************************/
                                      /* Options EOL and NOP are without parameters   */
                                      /************************************************/
                                        ptr_tcp_options++;
                                    }
                                }
                              /********************************************************/
                            }

                          /************************************************/
                          /* Options pointer ponts to the end of data -   */
                          /* means NO data present in this packet.        */
                          /* This packet is a potential TURBO candidate.  */
                          /************************************************/
                            if (ptr_tcp_options == ptr_data_tail)
                            {
                                ptr_ses_property->app_specific_data.u.app_desc.raw_app_info2 = ptr_tcphdr->ack_seq;
                                ptr_ses_property->app_specific_data.u.app_desc.enables |= TI_PP_SESSION_APP_RAW_INFO1_B3_VALID;

                                if (tcp_opt_timestamp_found)
                                {
                                    ptr_ses_property->app_specific_data.u.app_desc.enables |= TI_PP_SESSION_APP_RAW_INFO1_B3_PLUS_VALID;
                                }
                            }
                          /************************************************/
                        }
                        else
                        {
                            /**************************************************/
                            /* TCP with NO ACK flag set - do not accelerate   */
                            /**************************************************/
                            return -1;
                        }
                    }
                }
            }
        }
        else
        {
            /* No IPv4 header was present. Reset all the fields in the packet descriptor. */
            memset ((void *)ptr_pkt_desc, 0, sizeof(TI_PP_PACKET_DESC));
        }
    }

    /* Check if the device is a VPID handle or not? */
    if (dev->vpid_handle != -1)
    {
        /* The only missing information not available at the PID layer is the VPID handle
         * on which the packet was actually received/transmitted. */
        ptr_ses_property->vpid_handle = dev->vpid_handle;
    }

    /* All the fields have been extracted. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_ingress_hook
 **************************************************************************
 * DESCRIPTION   :
 *  The function is registered as the device specific protocol handler for
 *  all networking devices which exist in the system which have a valid
 *  VPID. The function extracts the information pertinent to creation of
 *  the session interface and stores it in the SKB.
 *
 * RETURNS:
 *  Always returns 0.
 **************************************************************************/
int ti_hil_ingress_hook(struct sk_buff* skb)
{

    if (global_ti_hil_db.hil_disabled)
        return 0;

    /* Extract all the fields from the packet and populate the Ingress Packet Descriptor. */
    if (ti_hil_extract_packet_desc ((char *)skb->mac.raw, skb->dev, &skb->pp_packet_info.ti_session.ingress) < 0)
        return 0;

    global_ti_hil_db.num_ingress_pkts++;

    /* Packet has passed through the Ingress Hooks. */
    skb->pp_packet_info.ti_pp_flags = skb->pp_packet_info.ti_pp_flags | TI_PPM_SESSION_INGRESS_RECORDED;
    skb->pp_packet_info.ti_session.priority = 0;

    if ((skb->ti_meta_info & 0x7) > 3)
    {
        skb->pp_packet_info.ti_session.priority = 1;
    }

    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_session_intelligence
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the host intelligence layer which decides if the Session
 *  is worthy of accleration or not?
 *
 * RETURNS:
 *  0   -  Session is NOT accelerated
 *  1   -  Session is accelerated
 **************************************************************************/
static int ti_hil_session_intelligence (TI_PP_SESSION* ptr_session)
{
    /* Acclerate only IPv4 or IPv6 traffic. */
    if ((ptr_session->ingress.l3l4_packet.packet_type & (TI_PP_IPV4_TYPE | TI_PP_IPV6_TYPE)) == 0)
        return 0;

    /* Dont accelerate MAC Broadcast Packets.
     *  - Check the Ingress Properties and ensure that the DST MAC Valid Bit is set.
     *  - Destination MAC Address MSB has a non-zero value */
    if ((ptr_session->ingress.l2_packet.u.eth_desc.enables & TI_PP_SESSION_L2_DSTMAC_VALID) &&
        (ptr_session->ingress.l2_packet.u.eth_desc.dstmac[0] & 1))
        return 0;

    /* Accelerate only TCP and UDP. */
    if ((ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol != IPPROTO_UDP) &&
        (ptr_session->ingress.l3l4_packet.u.ipv4_desc.protocol != IPPROTO_TCP))
        return 0;

    /* Accelerate the session. */
    return 1;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_null_hook
 **************************************************************************
 * DESCRIPTION   :
 *  This function creates session to terminating VPID.
 *  All the packets like ingress one will be routed to it
 *  and dropped in the future.
 *  The function is very similar to ti_hil_egress_hook.
 * RETURNS       :
 *  Always returns 0.
 **************************************************************************/
int ti_hil_null_hook(struct sk_buff* skb, int null_vpid)
{
    TI_PP_SESSION*      ptr_session;
    int                 num_current_session;
    int                 bucket;
    int                 session_handle;

    if (global_ti_hil_db.hil_disabled)
        return 0;

    /* These checks have the following purpose:-
     *  a) If the Packet has not HIT the ingress hook there is no point in creating the session
     *     since the packet is locally generated and these sessions cannot be acclerated.
     *  b) The Host Intelligence layers have decided not to "acclerate" this session. */
    if ((skb->pp_packet_info.ti_pp_flags & TI_PPM_SESSION_INGRESS_RECORDED) == 0)
    {
        global_ti_hil_db.num_other_pkts++;
        return 0;
    }

    if  (skb->pp_packet_info.ti_pp_flags & TI_PPM_SESSION_BYPASS)
    {
        global_ti_hil_db.num_bypassed_pkts++;
        return 0;
    }

    global_ti_hil_db.num_null_drop_pkts++;

    /* Get the pointer to the session information block. */
    ptr_session = &skb->pp_packet_info.ti_session;

    /* There can be only one egress record at this stage. */
    ptr_session->num_egress = 1;

#ifdef CONFIG_TI_META_DATA
    /* DOCSIS DSID is not supported yet... */
    ptr_session->ingress.app_specific_data.u.app_desc.enables = 0;
 /* ptr_session->ingress.app_specific_data.u.app_desc.u.raw_app_info1 = -1; */
#endif

    /* Extract all the fields from the packet and populate the Egress Packet Descriptor. */
    if (ti_hil_extract_packet_desc ((char *)skb->data, skb->input_dev, &ptr_session->egress[0]) < 0)
        return 0;

    /* We can only proceed to the next step if all the information has been extracted. This implies
     * that the Egress Hook has hit the Egress Hook on both the PID and VPID. */
    if (skb->input_dev->pid_handle == -1)
        return 0;

    /* Override the VPID */
    ptr_session->egress[0].vpid_handle = null_vpid;

    /* Check if the session is ROUTABLE or not? One way of doing this is to compare the L2 destination
     * MAC Address at the Ingress and Egress and if they are not the same it is safe to assume that
     * the session was routed. There are other system wide optimizations that could be done here. For
     * example if the box is a layer2 bridge then this check is not required. Similarly if the box
     * operates only in ROUTED mode then the value can always be set. The check here is the most fail
     * proof as it handles conditions where both Bridging and Routing can coexist. */
    if (ptr_session->ingress.l2_packet.packet_type & TI_PP_ETH_TYPE)
    {
        /* OK. Ingress had recorded Layer2 properties. Does the Egress have the same */
        if (ptr_session->egress[0].l2_packet.packet_type & TI_PP_ETH_TYPE)
        {
            /* OK. Egress had also got recorded Layer2 properties. So compare the same. */
            if (memcmp ((void *)&ptr_session->ingress.l2_packet.u.eth_desc.dstmac,
                        (void *)&ptr_session->egress[0].l2_packet.u.eth_desc.dstmac, 6) != 0)
            {
                /* The destination MAC address are not the same; most definately a routed session. */
                ptr_session->is_routable_session = 1;
            }
            else
            {
                /* The destination MAC address are the same. This is bridged for sure. */
                ptr_session->is_routable_session = 0;
            }
        }
        else
        {
            /* No Egress information at layer2. We cannot take a decision now as all the information is not present.
             * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
            ptr_session->is_routable_session = 0;
        }
    }
    else
    {
        /* No Ingress information at layer2. We cannot take a decision now as all the information is not present.
         * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
        ptr_session->is_routable_session = 0;
    }

    /* Once all the fields have been extracted. Check if the session can be created or not? */
    if (ti_hil_session_intelligence (ptr_session) == 0)
        return 0;

    /* All sessions created here have a standard session timeout. */
    ptr_session->session_timeout = ti_session_timeout;

    /* Create the session in the Packet Processor. */
    session_handle = ti_ppm_create_session (ptr_session, (void*)skb);
    if (session_handle < 0)
    {
        /* Session Creation Failed. Increment the error counter. */
        hil_num_error++;
        return 0;
    }

    /* Do the necessary work for the HIL Analysis. */
    hil_num_total_sessions++;
    num_current_session = ti_ppm_get_session (-1, 0, NULL);
    bucket = num_current_session / HIL_SESSION_STAT_BUCKET;

    /* If the number of sessions is the maximum number of sessions, then the above equation results
     * in calculating a bucket one more than the expected number leading to erroneous calculations.
     * Thus decrement the bucket number so as to increment the session counter in the last bucket.
     * For e.g.,
     *  num_current_session = 256 then, bucket = 256 / 64 = 4 so we end up incrementing the
     *  counter in bucket[4] when the valid buckets are 0,1,2,and 3. */
    if(bucket == HIL_MAX_NUM_BUCKETS)
        bucket--;
    hil_session[bucket]++;

    return 0;
}


/**************************************************************************
 * FUNCTION NAME : ti_hil_egress_hook
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the egress SRM hook that is called when a packet is to
 *  be transmitted on a session interfaces. The function extracts the
 *  information pertinent to creation of the session interface. It then
 *  checks if this packet was "routed/bridged". If yes then control is
 *  passed to the Plugin Logic to determine creation of the session.
 *
 * RETURNS       :
 *  Always returns 0.
 **************************************************************************/
int ti_hil_egress_hook(struct sk_buff* skb)
{
    TI_PP_SESSION*      ptr_session;
    int                 num_current_session;
    int                 bucket;
    int                 session_handle;
    int                 i;
    int                 tdox_ID = -1;

    if (global_ti_hil_db.hil_disabled)
        return 0;

    /* These checks have the following purpose:-
     *  a) If the Packet has not HIT the ingress hook there is no point in creating the session
     *     since the packet is locally generated and these sessions cannot be acclerated.
     *  b) The Host Intelligence layers have decided not to "acclerate" this session. */
    if ((skb->pp_packet_info.ti_pp_flags & TI_PPM_SESSION_INGRESS_RECORDED) == 0)
    {
        global_ti_hil_db.num_other_pkts++;
        return 0;
    }

    if  (skb->pp_packet_info.ti_pp_flags & TI_PPM_SESSION_BYPASS)
    {
        global_ti_hil_db.num_bypassed_pkts++;
        return 0;
    }

    global_ti_hil_db.num_egress_pkts++;

    /* Get the pointer to the session information block. */
    ptr_session = &skb->pp_packet_info.ti_session;

    /* There can be only one egress record at this stage. */
    ptr_session->num_egress = 1;

#ifdef CONFIG_TI_META_DATA
    for (i=0; i<ptr_session->num_egress; i++)
    {
        ptr_session->egress[i].app_specific_data.u.app_desc.u.raw_app_info1 = skb->ti_meta_info;
        ptr_session->egress[i].app_specific_data.u.app_desc.enables = TI_PP_SESSION_APP_RAW_INFO1_VALID;
    }

    /* DOCSIS DSID is not supported yet... */
    ptr_session->ingress.app_specific_data.u.app_desc.enables = 0;
 /* ptr_session->ingress.app_specific_data.u.app_desc.u.raw_app_info1 = -1; */
#endif

    /* Extract all the fields from the packet and populate the Egress Packet Descriptor. */
    if (ti_hil_extract_packet_desc ((char *)skb->data, skb->dev, &ptr_session->egress[0]) < 0)
        return 0;

    /* We can only proceed to the next step if all the information has been extracted. This implies
     * that the Egress Hook has hit the Egress Hook on both the PID and VPID. */
    if (skb->dev->pid_handle == -1)
        return 0;

    /* Check if the session is ROUTABLE or not? One way of doing this is to compare the L2 destination
     * MAC Address at the Ingress and Egress and if they are not the same it is safe to assume that
     * the session was routed. There are other system wide optimizations that could be done here. For
     * example if the box is a layer2 bridge then this check is not required. Similarly if the box
     * operates only in ROUTED mode then the value can always be set. The check here is the most fail
     * proof as it handles conditions where both Bridging and Routing can coexist. */
    if (ptr_session->ingress.l2_packet.packet_type & TI_PP_ETH_TYPE)
    {
        /* OK. Ingress had recorded Layer2 properties. Does the Egress have the same */
        if (ptr_session->egress[0].l2_packet.packet_type & TI_PP_ETH_TYPE)
        {
            /* OK. Egress had also got recorded Layer2 properties. So compare the same. */
            if (memcmp ((void *)&ptr_session->ingress.l2_packet.u.eth_desc.dstmac,
                        (void *)&ptr_session->egress[0].l2_packet.u.eth_desc.dstmac, 6) != 0)
            {
                /* The destination MAC address are not the same; most definately a routed session. */
                ptr_session->is_routable_session = 1;
            }
            else
            {
                /* The destination MAC address are the same. This is bridged for sure. */
                ptr_session->is_routable_session = 0;
            }
        }
        else
        {
            /* No Egress information at layer2. We cannot take a decision now as all the information is not present.
             * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
            ptr_session->is_routable_session = 0;
        }
    }
    else
    {
        /* No Ingress information at layer2. We cannot take a decision now as all the information is not present.
         * Setting the flag is not correct because the framing in the PDSP will not work. So default to bridged */
        ptr_session->is_routable_session = 0;
    }

    /* Once all the fields have been extracted. Check if the session can be created or not? */
    if (ti_hil_session_intelligence (ptr_session) == 0)
        return 0;

    /* All sessions created here have a standard session timeout. */
    ptr_session->session_timeout = ti_session_timeout;

    /* Populate the ack supression */
    if (ptr_session->egress[0].l3l4_packet.u.ipv4_desc.protocol == IPPROTO_TCP)
    {
        TI_PP_VPID vpid;
        TI_PP_PID   pid;

        ti_ppm_get_vpid_info (skb->dev->vpid_handle, &vpid);
        ti_ppm_get_pid_info  (vpid.parent_pid_handle, &pid);

        if (TI_PP_PID_TYPE_DOCSIS == pid.type)
        {
          /************************************************************************/
          /* DOCSIS Upstream flow case only.                                      */
          /* The following code handles TURBO DOX feature case.                   */
          /************************************************************************/
            if (ptr_session->egress[0].app_specific_data.u.app_desc.enables & TI_PP_SESSION_APP_RAW_INFO1_B3_VALID)
            {
                if (ti_ppm_check_session(ptr_session) == -1)
                {
                    tdox_ID = ti_hil_tdox_alloc_session();

                    if (tdox_ID != -1)
                    {
                        ptr_session->egress[0].app_specific_data.u.app_desc.u.raw_app_info1_b3 = tdox_ID;
                    }
                    else
                    {
                        ptr_session->egress[0].app_specific_data.u.app_desc.enables &= ~TI_PP_SESSION_APP_RAW_INFO1_B3_VALID;
                    }
                }
            }
          /************************************************************************/
        }
    }


    /* Create the session in the Packet Processor. */
    session_handle = ti_ppm_create_session (ptr_session, (void*)skb);
    if (session_handle < 0)
    {
        if (tdox_ID != -1)
        {
            ti_hil_tdox_free_session( -1, tdox_ID );
        }

        /* Session Creation Failed. Increment the error counter. */
        hil_num_error++;
        return 0;
    }

    if (tdox_ID != -1)
    {
        ti_hil_tdox_associate_session(tdox_ID, session_handle);
    }

    /* Do the necessary work for the HIL Analysis. */
    hil_num_total_sessions++;
    num_current_session = ti_ppm_get_session (-1, 0, NULL);
    bucket = num_current_session / HIL_SESSION_STAT_BUCKET;

    /* If the number of sessions is the maximum number of sessions, then the above equation results
     * in calculating a bucket one more than the expected number leading to erroneous calculations.
     * Thus decrement the bucket number so as to increment the session counter in the last bucket.
     * For e.g.,
     *  num_current_session = 256 then, bucket = 256 / 64 = 4 so we end up incrementing the
     *  counter in bucket[4] when the valid buckets are 0,1,2,and 3. */
    if(bucket == HIL_MAX_NUM_BUCKETS)
        bucket--;
    hil_session[bucket]++;

#ifdef CONFIG_NETFILTER
    /* Once the session has been created; check if the packet had passed through the
     * connection tracking hooks and pass the session handle to that layer. */
    if (skb->nfct != NULL)
    {
        /* Session has passed through the connection tracking hooks.
         * Now get the connection tracking entry and set the hooks correctly. */
        struct ip_conntrack* conntrack  = (struct ip_conntrack *)skb->nfct;
        int*                 ct_session_handle;

        /* Check the direction of the connection tracking entry. */
        if (CTINFO2DIR(skb->nfctinfo) == IP_CT_DIR_REPLY)
            ct_session_handle = &conntrack->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle;
        else
            ct_session_handle = &conntrack->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle;

        /* Check if the current session handle is valid or not? */
        if (*ct_session_handle != 0xFFFF)
        {
            /* Handle was valid. Now we need to ensure that the current session handle matches the
             * one we just created. */
            if (*ct_session_handle != session_handle)
            {
                /* The existing session handle does not match the one we had.
                 *
                 * This should typically not happen because the PPM has an inbuilt duplicate session
                 * detection logic which will detect this and return the same session handle. (Example
                 * of this is the TCP Control Packets will have the same session handle passed to them
                 * at this stage so this code will never get executed)
                 *
                 * The fact that the control came here is that the PPM duplicate session detection logic
                 * failed or because there was something in the packet different which the conntrack
                 * did not care about. One of the known occurrences is the TOS Byte difference. Anyway
                 * in this case we need to handle the condition gracefully. So currently we ignore the
                 * new session handle and dont link it with the connection tracking entry.
                 *
                 * One more technique that can be used to solve this problem is to ignore the TOS byte in
                 * the LUT configuration. */
                printk ("INFO --> Existing session %d new session %d.\n", *ct_session_handle, session_handle);
                return 0;
            }
        }

        /* Remember the new session handle */
        *ct_session_handle = session_handle;

        /* Map the session handle and connection tracking entry together. */
        hil_session_ct_mapper[session_handle] = conntrack;
    }
#endif /* CONFIG_NETFILTER */
    return 0;
}

#if 0 /* TODO: Enable once router functionality is available */

/**************************************************************************
 * FUNCTION NAME : ti_hil_device_handler
 **************************************************************************
 * DESCRIPTION   :
 *  Default HIL Device Handler.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_device_handler(unsigned long event_id, void* ptr)
{
	struct net_device *dev = (struct net_device *)ptr;

    /* Dont do anything for Loopback devices; since the interface cannot participate
     * in the packet processor. */
    if (dev->flags & IFF_LOOPBACK)
    {
        /* YES. In that case there is no PID or VPID associated with the device */
        dev->pid_handle = -1;
        dev->vpid_handle= -1;
        return 0;
    }

    /* Processing is based on the Event. */
	switch (event_id)
    {
        case NETDEV_UP:
        {
            /* Network device is going UP. */
            printk ("Device %s is going UP!\n", dev->name);

            /* VPID needs to be created only if either of the following conditions are met:-
             *  a) Device is connected to the bridge.
             *     The bridge runs its internal state machine as ports move from LISTENING,
             *     LEARNING to FORWARDING state. There are events generated from the state
             *     machine which should handle the VPID creation/deletion. Thus this check
             *     is not explicitly seen here.
             *  b) Device is connected to the IP stack i.e has an IP Address. */
            if (ti_hil_is_device_routed(dev) == 1)
            {
                /* Check if the device had a valid VPID handle. */
                if (dev->vpid_handle == -1)
                {
                    /* NO. The device needs to be created as a VPID in the packet processor. */
                    dev->vpid_handle = ti_ppm_create_vpid (&dev->vpid_block);
                    if (dev->vpid_handle < 0)
                    {
                        printk ("Error: Unable to create VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                        return -1;
                    }
                    printk ("Succesfully created VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                }
            }

            /* Install an Ingress Hook on the networking device. This is done if either
             * of the following conditions are met:-
             *  (a) Device has valid PID Handle.
             *  (b) Device has valid VPID Handle. */
            if ((dev->vpid_handle != -1) || (dev->pid_handle != -1))
            {
                /* Install the Ingress Hook. */
                if (ti_register_protocol_handler (dev, ti_hil_ingress_hook) == 0)
                    printk ("Ingress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);

                /* Install the Egress Hook. */
                if (ti_register_egress_hook_handler(dev, ti_hil_egress_hook) == 0)
                    printk ("Egress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
            }
            break;
        }
	    case NETDEV_DOWN:
        {
            /* Network device is going DOWN. */
            printk ("Device %s with VPID: %d is going DOWN!\n", dev->name, dev->vpid_handle);

            /* Check if the device had a valid VPID handle. */
            if (dev->vpid_handle != -1)
            {
                /* YES. The device needs to be removed from the packet processor. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk ("Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }
                printk ("Succesfully deleted VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                dev->vpid_handle = -1;
            }

            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        default:
        {
            /* All other events are ignored. */
            return 0;
        }
    }

    /* Work has been successfully completed! */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_inet_handler
 **************************************************************************
 * DESCRIPTION   :
 *  Default HIL INET Handler.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_inet_handler(unsigned long event_id, void* ptr)
{
    struct in_ifaddr *ifa = (struct in_ifaddr*)ptr;
    struct in_device *in_dev = ifa->ifa_dev;
    struct net_device *dev = in_dev->dev;

    /* Dont do anything for Loopback devices; since the interface cannot participate
     * in the packet processor. */
    if (dev->flags & IFF_LOOPBACK)
    {
        /* YES. In that case there is no PID or VPID associated with the device */
        dev->pid_handle = -1;
        dev->vpid_handle= -1;
        return 0;
    }

    /* Processing is based on the Event. */
	switch (event_id)
    {
        case NETDEV_UP:
        {
            /* Network device is going UP. */
            printk ("Device %s is going UP!\n", dev->name);

            /* VPID needs to be created only if either of the following conditions are met:-
             *  a) Device is connected to the bridge.
             *     The bridge runs its internal state machine as ports move from LISTENING,
             *     LEARNING to FORWARDING state. There are events generated from the state
             *     machine which should handle the VPID creation/deletion. Thus this check
             *     is not explicitly seen here.
             *  b) Device is connected to the IP stack i.e has an IP Address. */
            if (ti_hil_is_device_routed(dev) == 1)
            {
                /* Check if the device had a valid VPID handle. */
                if (dev->vpid_handle == -1)
                {
                    /* NO. The device needs to be created as a VPID in the packet processor. */
                    dev->vpid_handle = ti_ppm_create_vpid (&dev->vpid_block);
                    if (dev->vpid_handle < 0)
                    {
                        printk ("Error: Unable to create VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                        return -1;
                    }
                    printk ("Succesfully created VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                }
            }

            /* Install an Ingress Hook on the networking device. This is done if either
             * of the following conditions are met:-
             *  (a) Device has valid PID Handle.
             *  (b) Device has valid VPID Handle. */
            if ((dev->vpid_handle != -1) || (dev->pid_handle != -1))
            {
                /* Install the Ingress Hook. */
                if (ti_register_protocol_handler (dev, ti_hil_ingress_hook) == 0)
                    printk ("Ingress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);

                /* Install the Egress Hook. */
                if (ti_register_egress_hook_handler(dev, ti_hil_egress_hook) == 0)
                    printk ("Egress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
            }
            break;
        }
	    case NETDEV_DOWN:
        {
            /* Network device is going DOWN. */
            printk ("Device %s with VPID: %d is going DOWN!\n", dev->name, dev->vpid_handle);

            /* Check if the device had a valid VPID handle. */
            if (dev->vpid_handle != -1)
            {
                /* YES. The device needs to be removed from the packet processor. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk ("Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }
                printk ("Succesfully deleted VPID %d PID %d Device %s\n",  dev->vpid_handle, dev->pid_handle, dev->name);
                dev->vpid_handle = -1;
            }

            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        default:
        {
            /* All other events are ignored. */
            return 0;
        }
    }

    /* Work has been successfully completed! */
    return 0;
}

#endif

/**************************************************************************
 * FUNCTION NAME : ti_hil_pp_handler
 **************************************************************************
 * DESCRIPTION   :
 *  Default HIL PP Handler.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_pp_handler(unsigned long event_id, void* ptr)
{
    struct net_device*           dev;
    struct net_bridge_fdb_entry* fdb;
    struct sk_buff*              skb;
#ifdef CONFIG_NETFILTER
    struct ip_conntrack*         conntrack;
#endif
#ifdef CONFIG_IP_MULTICAST
    struct mfc_cache*            ptr_mfc_cache;
#endif
#ifdef CONFIG_NETFILTER
    struct ipt_table*            t;
#endif

    /* Process the events. */
    switch (event_id)
    {
        case TI_DOCSIS_FLTR_DISCARD_PKT:
        {
            skb = (struct sk_buff*) ptr;

            if ((docsis_null_vpid_handle == -1) && (skb->input_dev->vpid_handle != -1))
            {
                docsis_null_vpid_handle = ti_ppm_create_vpid(&skb->input_dev->vpid_block);

                if (docsis_null_vpid_handle < 0)
                {
                    printk ("Error: Unable to create Null VPID %d PID %d Device %s\n", docsis_null_vpid_handle, 9, skb->input_dev->name);
                }
                else
                {
#ifdef CONFIG_TI_HIL_DEBUG
                    if (0 == global_ti_hil_db.dbg_disabled)
                    {
                        printk ("Succesfully created Null VPID %d PID %d Device %s\n",  docsis_null_vpid_handle, 9, skb->input_dev->name);
                    }
#endif /* CONFIG_TI_HIL_DEBUG */

                    ti_ppm_set_vpid_flags( docsis_null_vpid_handle,
                                           TI_PP_VPID_FLG_TX_DISBL | TI_PP_VPID_FLG_RX_DISBL );
                }
            }

            ti_hil_null_hook(skb, docsis_null_vpid_handle);

            break;
        }

        case TI_DOCSIS_FLTR_ADD:
        case TI_DOCSIS_FLTR_DEL:
        case TI_DOCSIS_FLTR_CHG:
        {
            /* Always flush ALL sessions since we never know the behavior of the filter.
             * It may discard or allow packets. In case filter is of "allow" kind it is
             * not different from any other regular session.
             * Thus it is required to flush everything. */
            ti_ppm_flush_sessions( -1 );
            break;
        }

        case TI_DOCSIS_CLASSIFY_ADD:
        case TI_DOCSIS_CLASSIFY_DEL:
        case TI_DOCSIS_CLASSIFY_CHG:
        {
            ti_ppm_flush_sessions(-1);
            break;
        }

        case TI_BRIDGE_PORT_DELETE:
        {
            /* Event indicates that a device has been removed from the bridge. This event is generated
             * when the user executes the brctl delif command to remove a port from the bridge. */
            dev = (struct net_device*)ptr;

            /* Do not touch VPIDs that do not have any parent PID defined */
            if (dev->vpid_block.parent_pid_handle == (unsigned char)(-1))
            {
                break;
            }

            /* This implies that the device is no longer capable of pariticipating
             * in the networking and should be removed from the packet processor.
             * But before doing so check if the device was attached to the Packet
             * processor or not i.e. valid VPID handle. */
            if (dev->vpid_handle != -1)
            {
                /* Delete the VPID. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk ("Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }
#ifdef CONFIG_TI_HIL_DEBUG
                if (0 == global_ti_hil_db.dbg_disabled)
                {
                    printk ("Succesfully deleted VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                }
#endif /* CONFIG_TI_HIL_DEBUG */
                dev->vpid_handle = -1;
            }
            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        case TI_BRIDGE_PORT_FORWARD:
        {
            /* Event indicates that a port attached to the bridge has moved to the FORWARDING state.
             * The host bridge will only forward packets in this state. Thus this is the time we
             * create the VPID.*/
            dev = (struct net_device*)ptr;

            /* Do not create any VPIDs if there is no parent PID defined */
            if (dev->vpid_block.parent_pid_handle == (unsigned char)(-1))
            {
                break;
            }

            if (dev->vpid_handle == -1)
            {
                /* Create the VPID. */
                dev->vpid_handle = ti_ppm_create_vpid (&dev->vpid_block);
                if (dev->vpid_handle < 0)
                {
                    printk ("Error: Unable to create VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }

#ifdef CONFIG_TI_HIL_DEBUG
                if (0 == global_ti_hil_db.dbg_disabled)
                {
                    printk ("Succesfully created VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                }
#endif /* CONFIG_TI_HIL_DEBUG */

            }
            /* Install an Ingress Hook on the networking device. This is done if either
             * of the following conditions are met:-
             *  (a) Device has valid PID Handle.
             *  (b) Device has valid VPID Handle. */
            if ((dev->vpid_handle != -1) || (dev->pid_handle != -1))
            {
                /* Install the Ingress Hook. */
                if (ti_register_protocol_handler (dev, ti_hil_ingress_hook) == 0)
                {
#ifdef CONFIG_TI_HIL_DEBUG
                    if (0 == global_ti_hil_db.dbg_disabled)
                    {
                        printk ("Ingress Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
                    }
#endif /* CONFIG_TI_HIL_DEBUG */
                }

                /* Install the Egress Hook. */
                if (ti_register_egress_hook_handler(dev, ti_hil_egress_hook) == 0)
                {
#ifdef CONFIG_TI_HIL_DEBUG
                    if (0 == global_ti_hil_db.dbg_disabled)
                    {
                        printk ("Egress  Hook on PID/VPID: %d/%d Name: %s\n", dev->pid_handle, dev->vpid_handle, dev->name);
                    }
#endif /* CONFIG_TI_HIL_DEBUG */
                }
            }
            break;
        }
        case TI_BRIDGE_PORT_DISABLED:
        {
            /* Event indicates that a port attached to the bridge has been moved to the disabled state.
             * This event if of concern only if the bridge is running the spanning tree protocol. The event
             * occurs if the spanning tree protocol detects a condition where the port is causing a loop and
             * disables the port. In such a scenario the VPID and thus all sessions related to the port need
             * to be removed from the packet processor. */
            dev = (struct net_device*)ptr;

            /* Do not touch VPIDs that do not have any parent PID defined */
            if (dev->vpid_block.parent_pid_handle == (unsigned char)(-1))
            {
                break;
            }

            /* If the device has a VPID handle; it needs to be removed from the packet processor. */
            if (dev->vpid_handle != -1)
            {
                /* Delete the VPID. */
                if (ti_ppm_delete_vpid (dev->vpid_handle) < 0)
                {
                    printk ("Error: Unable to delete VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                    return -1;
                }

#ifdef CONFIG_TI_HIL_DEBUG
                if (0 == global_ti_hil_db.dbg_disabled)
                {
                    printk ("Succesfully deleted VPID %d PID %d Device %s\n", dev->vpid_handle, dev->pid_handle, dev->name);
                }
#endif /* CONFIG_TI_HIL_DEBUG */
                dev->vpid_handle = -1;
            }
            /* Uninstall the Ingress Hook; since because if the device is no longer a VPID there is no
             * point to listen for packets on it. */
            ti_deregister_protocol_handler (dev);
            ti_deregister_egress_hook_handler (dev);
            break;
        }
        case TI_BRIDGE_FDB_CREATED:
        {
            /* Event indicates that an FDB entry is being created. This event is of concern if the single session
             * per interface mode is selected as an option for session creation. If this is not the case then this
             * event can be ignored. */
            fdb = (struct net_bridge_fdb_entry *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                /* Debug Message: To indicate how to access all the information? */
                printk ("Intrusive -> Create Session MAC=0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x on %s VPID:%d\n",
                    fdb->addr.addr[0], fdb->addr.addr[1], fdb->addr.addr[2], fdb->addr.addr[3], fdb->addr.addr[4],
                    fdb->addr.addr[5], fdb->dst->dev->name, fdb->dst->dev->vpid_handle);
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }
        case TI_BRIDGE_FDB_DELETED:
        {
            /* Event indicates that an FDB entry is being deleted. The event is of concern if the single session
             * per interface mode is selected as an option for session creation. If this is not the case then this
             * event can be ignored. */
            fdb = (struct net_bridge_fdb_entry *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                /* Debug Message: To indicate how to access all the information? */
                printk ("Intrusive -> Delete Session MAC=0x%02x-0x%02x-0x%02x-0x%02x-0x%02x-0x%02x on %s VPID:%d\n",
                    fdb->addr.addr[0], fdb->addr.addr[1], fdb->addr.addr[2], fdb->addr.addr[3], fdb->addr.addr[4],
                    fdb->addr.addr[5], fdb->dst->dev->name, fdb->dst->dev->vpid_handle);
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }
        case TI_BRIDGE_PACKET_FLOODED:
        {
            /* Event indicates that the packet will now be flooded onto all interfaces. This can happen in
             * any of the following cases:-
             *  a) Unicast packet but no matching FDB entry is found.
             *  b) Broadcast packet
             *  c) Multicast packet but no layer2 extensions eg IGMP snooping exists */
            skb = (struct sk_buff*) ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Packet %p is flooded on all interfaces\n", skb);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* In the intrusive mode profile these packets are not considered as candidates for acceleration.
             * So mark the packet BYPASS mode so that the egress hook is bypassed. */
            skb->pp_packet_info.ti_pp_flags |= TI_PPM_SESSION_BYPASS;
            break;
        }

        case TI_ROUTE_ADDED:
        {
            /*  Event indicates that a new route is being added. A route change
             *  could affect the existing sessions in the PP. So, the HIL profile
             *  could choose to delete all sessions that are affected by the route
             *  change / flush the entire session table to make sure that there
             *  is no inconsistency due to the route change.
             */
            break;
        }
        case TI_ROUTE_DELETED:
        {
            /*  Event indicates that an existing route is being deleted. A route change
             *  could affect the existing sessions in the PP. So, the HIL profile
             *  could choose to delete all sessions that are affected by the route
             *  change / flush the entire session table to make sure that there
             *  is no inconsistency due to the route change.
             */
            break;
        }

#ifdef CONFIG_NETFILTER
        case TI_CT_ENTRY_CREATED:
        {
            /* Event generated from the connection tracking layer to indicate that a connection tracking entry
             * has been created. This could be used by the system profile to analyze the connection and indicate
             * immediately if the connection is worthy of accleration or not? If the profile deems that the
             * connection can not be accelerated it sets the status flag in the connection tracking entry to BYPASS
             * mode. This will ensure that all packets matching the connection will also have the BYPASS mode
             * set. This can be used for performance optimizations as the Egress Hook and Session Intelligence
             * will be less burdened. */
            conntrack = (struct ip_conntrack *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Connection Tracking Entry %p has been created\n", conntrack);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* Check if the conntrack was associated with an ALG? In our profile we dont want these sessions
             * to be accelerated so set the connection tracking entry to operate in BYPASS mode. */
            if (conntrack->helper != NULL)
                conntrack->ti_pp_status_flag |= TI_PP_BYPASS;
            break;
        }
        case TI_CT_DEATH_BY_TIMEOUT:
        {
            /* Event indicates that the connection tracking entry has timed out. Use this event to
             * determine if the connection tracking entry needs to be deleted or not? If the profile
             * wants to prevent the death of the connection tracking entry it should ensure that the
             * PP staus flag does NOT set the TI_PP_KILL_CONNTRACK bit and if so be the case the
             * connection tracking entry is now owned by the System Profile and it has the repsonsibility
             * of cleaning it.
             * This event needs to be handled only if CONFIG_NETFILTER is enabled else this event can
             * be safetly ignored. */
            conntrack = (struct ip_conntrack *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Connection Tracking Entry %p has timed out.\n", conntrack);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            if ((conntrack->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle == 0xFFFF) &&
                (conntrack->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle == 0xFFFF))
            {
                /* Neither of the flows in the connection tracking entry are being accelerated.
                 * This implies that we should just go ahead and delete the entry? */
                conntrack->ti_pp_status_flag |= TI_PP_KILL_CONNTRACK;
            }
            else
            {
                /* The flows are still being accelerated; so keep the connection tracking timer alive */
        		conntrack->timeout.expires = ti_session_timeout + jiffies;
		        add_timer(&conntrack->timeout);
            }
            break;
        }
        case TI_CT_NETFILTER_TABLE_UPDATE:
        {
            /* Get the netfilter table */
            t = (struct ipt_table *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Netfilter Table %s has been modified\n", t->name);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* Flush all sessions only for NAT... No need to do anything for Mangle and Firewall */
            if (strcmp (t->name, "nat") == 0)
            {
                if (ti_ppm_flush_sessions(-1) < 0 )
                {
                    printk ("Error: Unable to flush all sessions\n");
                    return 0;
                }
                printk ("NAT Table update all sessions flushed\n");
            }
            break;
        }
#endif /* CONFIG_NETFILTER */
#ifdef CONFIG_IP_MULTICAST
        case TI_MFC_ENTRY_CREATED:
        {
            /* Event indicates that a Multicast Forwarding Cache Entry has been created. This is typically
             * the case when a proxy or multicast routing daemon has detected a multicast group to be active
             * and has created the entry which will allow multicast packets to flow through the box.
             * The parameter passed to the event is the Multicast Forwarding Cache Entry.
             *
             * This event can be ignored if the System does not support Multicast Routing. */
            TI_PP_SESSION    session;
            int              session_handle;

            /* Get the MFC Cache Entry. */
            ptr_mfc_cache = (struct mfc_cache *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Join Multicast Group %x\n", ptr_mfc_cache->mfc_mcastgrp);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* Convert the MFC Cache to a session structure */
            if (hil_mfc_to_session(ptr_mfc_cache, &session) == 0)
            {
                /* Conversion was successful; so lets try and create a session */
                session_handle = ti_ppm_create_session(&session, (void*)skb);
                if (session_handle < 0)
                {
                    printk ("FATAL Error: Multicast session creation failed\n");
                }
                else
                {
                    /* Map the session and MFC Entry together. */
                    printk ("Multicast Session %d created succesfully\n", session_handle);
                    hil_mfc_add_entry (ptr_mfc_cache->mfc_mcastgrp, session_handle);
                }
            }
            else
            {
                /* Conversion was not successful. */
                printk ("Error: MFC to session conversion failed\n");
            }
            break;
        }
        case TI_MFC_ENTRY_DELETED:
        {
            /* Event indicates that a Multicast Forwarding Cache Entry has been deleted. This is typically
             * the case when a proxy or multicast routing daemon has detected a multicast group to be inactive
             * and has deleted the Multicast Forwarding cavhe entry
             *
             * This event can be ignored if the System does not support Multicast Routing. */
            int session_handle;

            /* Get the MFC cache entry which needs to be deleted. */
            ptr_mfc_cache = (struct mfc_cache *)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> Leave Multicast Group %x\n", ptr_mfc_cache->mfc_mcastgrp);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* Get the session handle and delete the MFC and Session Handle mapping */
            session_handle = hil_mfc_del_entry(ptr_mfc_cache->mfc_mcastgrp);
            if (session_handle >= 0)
            {
                /* Timer has expired for a session; time to delete the session. */
                if (ti_ppm_delete_session (session_handle, NULL) < 0)
                    printk ("Error: Unable to delete session %d\n", session_handle);
            }
            break;
        }
#endif /* CONFIG_IP_MULTICAST */

#ifdef CONFIG_VLAN_8021Q
        case TI_VLAN_DEV_CREATED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> VLAN Device %s has been created\n", dev->name);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* We know for sure that this is attached to a VLAN interface.
             * So configure the VPID Information Block appropriately. The egress
             * MTU of the interface needs to be correctly handled to account for
             * the VLAN Header. */
            dev->vpid_block.type            = TI_PP_VLAN;
            dev->vpid_block.vlan_identifier = VLAN_DEV_INFO(dev)->vlan_id;
            dev->vpid_block.egress_mtu      = dev->vpid_block.egress_mtu - VLAN_HLEN;
            break;
        }
        case TI_VLAN_DEV_DELETED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> VLAN Device %s is being removed\n", dev->name);
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }
#endif /* CONFIG_VLAN_8021Q */

#ifdef CONFIG_PPPOE
        case TI_PPP_INTERFACE_CREATED:
        {
            /* Get the pointer to the network device. */
            dev = (struct net_device*)ptr;

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                printk ("Intrusive -> PPP Device %s has been created Session ID:0x%x\n", dev->name, dev->padded);
            }
#endif /* CONFIG_TI_HIL_DEBUG */

            /* PPPoE could be executed over a VLAN connection or on a vanilla Ethernet
             * connection. Configure the VPID device type appropriately. */
            if (dev->vpid_block.type == TI_PP_VLAN)
            {
                /* The PPP connection is initialized over a VLAN connection. Configure the Egress MTU
                 * appropriately accounting for the PPP and VLAN headers. */
                dev->vpid_block.type       = TI_PP_VLAN_PPPoE;
                dev->vpid_block.egress_mtu = dev->vpid_block.egress_mtu - sizeof(struct pppoe_hdr) - 2 - VLAN_HLEN;
            }
            else
            {
                /* The PPP Connection is being bought over a vanilla Ethernet connection. Configure
                 * the Egress MTU appropriately accounting only for the PPP header */
                dev->vpid_block.type        = TI_PP_PPPoE;
                dev->vpid_block.egress_mtu  = dev->vpid_block.egress_mtu - sizeof(struct pppoe_hdr) - 2;
            }

            /* Extract and configure the PPP Session ID. */
            dev->vpid_block.ppp_session_id  = dev->padded;
            break;
        }
#endif /* CONFIG_PPPOE */
        default:
        {
            printk ("Intrusive -> Does not handle event 0x%lx\n", event_id);
            break;
        }
    }

    /* Successfully handled the event. */
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_netsubsystem_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This is the HIL Intrusive event handler which will capture all events from
 *  the networking sub-system.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_netsubsystem_event_handler(unsigned int module_id, unsigned long event_id, void* ptr)
{
    /* Process based on the module identifier */
    switch (module_id)
    {
#if 0 /* TODO: Enable once router functionality is available */
        case TI_DEVICE:
        {
            /* HIL Device Handler. */
            ti_hil_device_handler (event_id, ptr);
            break;
        }
        case TI_INET:
        {
            /* HIL Inet Handler. */
            ti_hil_inet_handler (event_id, ptr);
            break;
        }
#endif
        case TI_PP:
        {
            /* HIL PP Handler. */
            ti_hil_pp_handler (event_id, ptr);
            break;
        }

        default:
            break;
    }
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_pp_ppm_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function handles all the events generated by the PPM part of the
 *  Packet Processor Subsystem.
 **************************************************************************/
static void ti_hil_pp_ppm_event_handler (int event, unsigned int param1, unsigned int param2)
{
    /* Process the event accordingly... */
    switch (event)
    {
        case TI_PPM_OUT_OF_MEMORY:
        {
            printk ("FATAL Error: PPM is OUT of memory\n");
            break;
        }
        case TI_PPM_INTERNAL_ERROR:
        {
            printk ("FATAL Error: PPM Internal Error\n");
            break;
        }
        case TI_PPM_CREATE_PID_FAILED:
        case TI_PPM_DELETE_PID_FAILED:
        case TI_PPM_CREATE_VPID_FAILED:
        case TI_PPM_DELETE_VPID_FAILED:
        case TI_PPM_CREATE_SESSION_FAILED:
        case TI_PPM_DELETE_SESSION_FAILED:
        case TI_PPM_MODIFY_SESSION_FAILED:
        {
            printk ("FATAL Error: PP Operation 0x%x Failed\n", event);
            break;
        }
        case TI_PPM_PID_CREATED:
        case TI_PPM_PID_DELETED:
        case TI_PPM_VPID_CREATED:
        case TI_PPM_VPID_DELETED:
        case TI_PPM_SESSION_MODIFIED:
        {
            break;
        }
        case TI_PPM_SESSION_CREATED:
        {
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
            if (ti_hil_start_session_notification_cb)
            {
                if (((struct sk_buff*)param2)->pp_packet_info.ti_session.egress[0].vpid_handle ==
                    docsis_null_vpid_handle)
                {
                    ti_hil_start_session_notification_cb(param1, TI_DOCSIS_PP_SESSION_TYPE_DISCARDING,
                                                         ((struct sk_buff*)param2));
                }
                else
                {
                    ti_hil_start_session_notification_cb(param1, TI_DOCSIS_PP_SESSION_TYPE_FORWARDING,
                                                         ((struct sk_buff*)param2));
                }
            }
#endif /* CONFIG_TI_PACKET_PROCESSOR_STATS */
            break;
        }
        case TI_PPM_SESSION_DELETED:
        {
            ti_hil_tdox_free_session(param1, -1);

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
            if (ti_hil_delete_session_notification_cb)
            {
                ti_hil_delete_session_notification_cb(param1, ((TI_PP_SESSION_STATS*)param2)->packets_forwarded);
            }
#endif /* CONFIG_TI_PACKET_PROCESSOR_STATS */

#ifdef CONFIG_NETFILTER
            struct ip_conntrack* ct = hil_session_ct_mapper[param1];

            /* Once the session has been removed; check if the session handle was present in the mapper
             * this implies that the session handle and connection tracking entry were connected to
             * each other. */
            if (ct != NULL)
            {
                if (ct->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle == param1)
                    ct->tuplehash[IP_CT_DIR_REPLY].ti_pp_session_handle = 0xFFFF;

                if (ct->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle == param1)
                    ct->tuplehash[IP_CT_DIR_ORIGINAL].ti_pp_session_handle = 0xFFFF;

                /* The mapper slot is free now! */
                hil_session_ct_mapper[param1] = NULL;
            }
#endif /* CONFIG_NETFILTER */
            break;
        }
    }

    /* Work is done! */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_pp_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function handles all the events generated by the PP PDSP.
 **************************************************************************/
static void ti_hil_pp_event_handler (int event, unsigned int param1, unsigned int param2)
{
    /* Process the event accordingly... */
    switch (event)
    {
        case TI_PP_SESSION_EXPIRATION:
        {
            /* Session has EXPIRED; param1 is the session handle that has expired. */
            TI_PP_SESSION_STATS    session_stats;

            /* Timer has expired for a session; time to delete the session. */
            if (ti_ppm_delete_session (param1, &session_stats) < 0)
            {
                printk ("Error: Unable to delete session %d\n", param1);
                return;
            }

#ifdef CONFIG_TI_HIL_DEBUG
            if (0 == global_ti_hil_db.dbg_disabled)
            {
                /* Print the session stats. */
                printk ("--------- Session Stats %d ---------\n", param1);
                printk ("Number of Packets Forwarded: %u\n", session_stats.packets_forwarded);
                printk ("Number of Bytes   Forwarded: %u\n", session_stats.bytes_forwarded_hi);
                printk ("Number of Bytes   Forwarded: %u\n", session_stats.bytes_forwarded_lo);
                printk ("------------------------------------\n");
            }
#endif /* CONFIG_TI_HIL_DEBUG */
            break;
        }
        default:
        {
            /* Unknown PP Event Generated! */
            printk ("PP Generated Event 0x%x which is UNHANDLED\n", event);
            break;
        }
    }
    /* Work is done! */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_ppsubsystem_event_handler
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the registered event handler which listens to all events
 *  that arise from the Packet Processor Subsystem.
 **************************************************************************/
static void ti_hil_ppsubsystem_event_handler(unsigned int event_id, unsigned int param1, unsigned int param2)
{
    int module;
    int subsystem;

    /* We should only get events generated by the PP Sub-System. */
    subsystem = event_id & TI_PP_SUBSYSTEM_BIT_MASK;
    if (subsystem != 0)
    {
        printk ("FATAL Error: Event 0x%x violates the specification\n", event_id);
        return;
    }

    /* Get information on the module which generated the event and the actual event identifier. */
    module = event_id & TI_PP_MODULE_BIT_MASK;

    /* Process each event based on the module which generated the event. */
    switch (module)
    {
        case TI_PP_MODULE:
        {
            /* Packet Processor Event */
            ti_hil_pp_event_handler (event_id, param1, param2);
            break;
        }
        case TI_PPM_MODULE:
        {
            /* Packet Processor Manager Event. */
            ti_hil_pp_ppm_event_handler (event_id, param1, param2);
            break;
        }
        case TI_PPD_MODULE:
        {
            /* Packet Processor Driver Event. */
            printk ("PPD Event 0x%x Param1: 0x%x Param2: 0x%x\n", event_id, param1, param2);
            break;
        }
        default:
        {
            printk ("FATAL Error: Event 0x%x violates the specification\n", event_id);
            break;
        }
    }

    /* Our work is done. */
    return;
}

#ifdef CONFIG_IP_MULTICAST

/**************************************************************************
 * FUNCTION NAME : hil_mfc_check_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function checks the HIL MFC Database for a "session id" match for
 *  a particular mcast_group. The database keeps track of all MFC Entries
 *  that have been accelerated.
 *
 * RETURNS       :
 *  Handle of the session     - Match.
 *  -1                        - No Match.
 ***************************************************************************/
static int hil_mfc_check_entry (unsigned int mcast_group)
{
    int session_handle = 0;

    /* Cycle through all the entries */
    for (session_handle = 0; session_handle < TI_PP_MAX_ACCLERABLE_SESSIONS; session_handle++)
    {
        /* Check if there is a valid entry or not? */
        if (hil_mfc_session_mapper[session_handle] != 0)
        {
            /* YES. Match the entry to the multicast group specified. */
            if (hil_mfc_session_mapper[session_handle] == mcast_group)
            {
                /* Perfect Match: Return the PP Session handle */
                return session_handle;
            }
        }
    }

    /* Control comes here implies that there was no match! */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : hil_mfc_add_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function adds an entry to the Session to MFC Database
 *
 * RETURNS       :
 *  0     - Success.
 *  <0    - Error.
 ***************************************************************************/
static int hil_mfc_add_entry (unsigned int mcast_group, unsigned char session_handle)
{
    /* Check if the slot is free! */
    if (hil_mfc_session_mapper[session_handle] != 0)
    {
        /* No; slot is not free!*/
        printk ("Session Handle %d already mapped to group: 0x%x\n", session_handle, hil_mfc_session_mapper[session_handle]);
        return -1;
    }

    /* Map and return. */
    hil_mfc_session_mapper[session_handle] = mcast_group;
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : hil_mfc_del_entry
 **************************************************************************
 * DESCRIPTION   :
 *  The function deletes an entry from the Session to MFC Database
 *
 * RETURNS       :
 *  Session Handle     - Success.
 *  <0                 - Error.
 ***************************************************************************/
static int hil_mfc_del_entry (unsigned int mcast_group)
{
    int session_handle = hil_mfc_check_entry(mcast_group);

    /* Check if the session handle exists? */
    if (session_handle == -1)
    {
        /* No Matching entry found. */
        printk ("Multicast group 0x%x not mapped\n", mcast_group);
        return -1;
    }

    /* Unmap and return. */
    hil_mfc_session_mapper[session_handle] = 0;
    return session_handle;
}

/**************************************************************************
 * FUNCTION NAME : hil_mfc_to_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function converts an MFC Entry used for multicast routing to a PP
 *  session structure.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int hil_mfc_to_session(struct mfc_cache* ptr_mfc_cache, TI_PP_SESSION* ptr_session)
{
    int downstream_vif_index;

    /* Initialize the session structure */
    memset ((void *)ptr_session, 0, sizeof(TI_PP_SESSION));

    /* Configure the session timeout. For MFC we can create a static session; this is added till we
     * get the new header file; since we need to configure the session flag correctly. */
    ptr_session->session_timeout = 0;

    /* This is for sure a ROUTED SESSION. */
    ptr_session->is_routable_session = 1;

    /* Configure the Ingress Session Properties. We need to know from which VPID the packet will
     * be arriving. This can be retrieved from the UPSTREAM VIF Index.But we would need to convert
     * the VIF Index to VPID before we can do this. */
    if (vif_table[ptr_mfc_cache->mfc_parent].dev != NULL)
    {
        /* Check if the VPID has a VPID handle or not? If not then we cannot create a session in the PP
         * since the device does not belong to the Packet processor subsystem. */
        if (vif_table[ptr_mfc_cache->mfc_parent].dev->vpid_handle == -1)
            return -1;
    }
    else
    {
        printk ("FATAL Error: VIF Index %d does not map to a device\n", ptr_mfc_cache->mfc_parent);
        return -1;
    }

    /* Configure the Ingress Session Properties. */
    ptr_session->ingress.vpid_handle           = vif_table[ptr_mfc_cache->mfc_parent].dev->vpid_handle;
    ptr_session->ingress.l2_packet.packet_type = TI_PP_ETH_TYPE;

    /* Configuration of the Destination MAC Address; we could take two approaches here!
     *  a) Have a wild carded destination MAC Address
     *  b) Convert the Multicast IPv4 address to a L2 address and configure it.
     * Approach (b) seems to be a more robust solution.
     * We make sure that the SOURCE MAC Address is WILDCARDED! Since at this time we dont
     * know from where the packet is arriving. */
    ip_eth_mc_map (ptr_mfc_cache->mfc_mcastgrp, &ptr_session->ingress.l2_packet.u.eth_desc.dstmac[0]);
    ptr_session->ingress.l2_packet.u.eth_desc.enables = TI_PP_SESSION_L2_DSTMAC_VALID;

    /* Configure the LUT Entry for Layer3 Classification.
     * The following fields are WILDCARDED @ Layer3
     *  a) Source IP Address
     *  b) Protocol
     *  c) TOS
     *  f) Source Port
     *  g) Destination Port
     * This information is not known @ the time the MFC entry is created. */
    ptr_session->ingress.l3l4_packet.packet_type        = TI_PP_IPV4_TYPE;
    ptr_session->ingress.l3l4_packet.u.ipv4_desc.dst_ip = ptr_mfc_cache->mfc_mcastgrp;
    ptr_session->ingress.l3l4_packet.u.eth_desc.enables = TI_PP_SESSION_IPV4_DSTIP_VALID;

    /* Configure the Egress Session Properties.
     *   - Multicast packets are not subjected to any NAT and so there are no modifications
     *     which need to be done at Layer3 or Layer4. Thus all we populate in the Egress
     *     Session Properties are the Egress VPID on which the packets need to be sent out. */
    for (downstream_vif_index=0; downstream_vif_index < MAXVIFS; downstream_vif_index++)
    {
        /* Check if a valid VIF is present or not? */
        if (ptr_mfc_cache->mfc_un.res.ttls[downstream_vif_index] != 0xFF)
        {
            /* Found it! Get the device information from the VIF. */
            if (vif_table[downstream_vif_index].dev != NULL)
            {
                /* Now we need to get the VPID handle. If the VPID handle does not exist then the packet
                 * is being transmitted on an interface which does not support the packet processor and
                 * thus this session is not capable of being accelerated! */
                if (vif_table[downstream_vif_index].dev->vpid_handle == -1)
                    return -1;

                /* Remember the Egress VPID handle. */
                ptr_session->egress[ptr_session->num_egress++].vpid_handle = vif_table[downstream_vif_index].dev->vpid_handle;
            }
            else
            {
                printk ("FATAL Error: VIF Index %d does not map to a device\n", downstream_vif_index);
                return -1;
            }
        }
    }

    /* Once we come out of the loop we should have at least one egress record. */
    if (ptr_session->num_egress == 0)
    {
        printk ("No downstream interface for group 0x%x\n", ptr_mfc_cache->mfc_mcastgrp);
        return -1;
    }

    /* Successfully translated. */
    return 0;
}

#endif /* CONFIG_IP_MULTICAST */

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_display_l2
 **************************************************************************
 * DESCRIPTION   :
 *  The function prints the Layer2 Information
 **************************************************************************/
static void ti_hil_intrusive_display_l2 (TI_PP_ETH_DESC* ptr_eth_desc)
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
 * FUNCTION NAME : ti_hil_intrusive_display_ipv4
 **************************************************************************
 * DESCRIPTION   :
 *  The function prints the IPv4 Information
 **************************************************************************/
static void ti_hil_intrusive_display_ipv4 (TI_PP_IPV4_DESC* ptr_ipv4_lut_entry)
{
    /* Check if the Destination IP has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_DSTIP_VALID)
    {
        unsigned char * ipPtr = (unsigned char *)&ptr_ipv4_lut_entry->dst_ip;
        printk ("Dst IP       = 0x%08x [ %d.%d.%d.%d ]\n", ptr_ipv4_lut_entry->dst_ip, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);
    }
    else
        printk ("Dst IP       = XXX\n");

    /* Check if the Source IP has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_SRCIP_VALID)
    {
        unsigned char * ipPtr = (unsigned char *)&ptr_ipv4_lut_entry->src_ip;
        printk ("Src IP       = 0x%08x [ %d.%d.%d.%d ]\n", ptr_ipv4_lut_entry->src_ip, ipPtr[0], ipPtr[1], ipPtr[2], ipPtr[3]);
    }
    else
        printk ("Src IP       = XXX\n");

    /* Check if the protocol has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_PROTOCOL_VALID)
        printk ("Protocol     = 0x%x\n", ptr_ipv4_lut_entry->protocol);
    else
        printk ("Protocol     = XXX\n");

    /* Check if the TOS Byte has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_TOS_VALID)
        printk ("TOS          = 0x%x\n", ptr_ipv4_lut_entry->tos);
    else
        printk ("TOS          = XXX\n");

    /* Check if the Destination Port has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_DST_PORT_VALID)
        printk ("Dst Port     = %d\n", ptr_ipv4_lut_entry->dst_port);
    else
        printk ("Dst Port     = XXX\n");

    /* Check if the Source Port has been specified. */
    if (ptr_ipv4_lut_entry->enables & TI_PP_SESSION_IPV4_SRC_PORT_VALID)
        printk ("Src Port     = %d\n", ptr_ipv4_lut_entry->src_port);
    else
        printk ("Src Port     = XXX\n");

    /* Work is completed. */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_display_session
 **************************************************************************
 * DESCRIPTION   :
 *  The function displays the session properties.
 **************************************************************************/
static void ti_hil_intrusive_display_session (int session_handle)
{
    TI_PP_SESSION    session;
    int              index;

    /* Get the pointer to the Session Information. */
    if (ti_ppm_get_session_info(session_handle, &session) < 0)
        return;

    /* Print the Session Parameters. */
    printk ("Property : %s\n", session.is_routable_session ? "Routed": "Bridged");
    printk ("Timeout  : %d\n", session.session_timeout);
    printk ("Priority : %d\n", session.priority);

    /* Print the Ingress Session Properties; which include the L2/L3 and L4 properties. */
    printk ("**** Ingress Properties ****\n");
    printk ("Ingress VPID = %d\n", session.ingress.vpid_handle);
    if (session.ingress.l2_packet.packet_type == TI_PP_ETH_TYPE)
        ti_hil_intrusive_display_l2 (&session.ingress.l2_packet.u.eth_desc);
    else
        printk ("No L2 Properties\n");
    if (session.ingress.l3l4_packet.packet_type == TI_PP_IPV4_TYPE)
        ti_hil_intrusive_display_ipv4 (&session.ingress.l3l4_packet.u.ipv4_desc);
    else
        printk ("No L3/L4 Properties\n");

    /* Cycle through all the Egress Session Properties. */
    for (index = 0; index < session.num_egress; index++)
    {
        printk ("**** Egress Properties %d ****\n", index+1);

        /* Print the Ingress Session Properties; which include the L2/L3 and L4 properties. */
        printk ("Egress VPID  = %d\n", session.egress[index].vpid_handle);
        if (session.egress[index].l2_packet.packet_type == TI_PP_ETH_TYPE)
            ti_hil_intrusive_display_l2 (&session.egress[index].l2_packet.u.eth_desc);
        else
            printk ("No L2 Properties\n");
        if (session.egress[index].l3l4_packet.packet_type == TI_PP_IPV4_TYPE)
            ti_hil_intrusive_display_ipv4 (&session.egress[index].l3l4_packet.u.ipv4_desc);
        else
            printk ("No L3/L4 Properties\n");
    }

    /* Work has been completed. */
    return;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_show_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor show command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int ti_hil_show_cmd_handler(int argc, char* argv[])
{
    /****************************** VALIDATIONS ***************************/

    /* Validate the number of arguments that have been passed. */
    if (argc != 2)
    {
        printk ("ERROR: Incorrect Number of parameters passed.\n");
        return -1;
    }

    /**************************** End of VALIDATIONS ***********************/

    if (strcmp(argv[1], "tdox") == 0)
    {
        ti_hil_tdox_print();
        return 0;
    }

    /* Check if the "Global" stats were requested. */
    if (strcmp(argv[1], "global") == 0)
    {
        TI_PP_GLOBAL_STATS  pp_stats;

        /* YES. Get the global stats through the PPM */
        ti_ppm_get_global_stats(&pp_stats);

        /* Print the stats on the console. */
        printk ("Packets received in the PP   : %u\n", pp_stats.packets_rxed);
        printk ("Number of search attemps     : %u\n", pp_stats.packets_searched);
        printk ("Number of matched searches   : %u\n", pp_stats.search_matched);
        printk ("Number of Synch Delays       : %u\n", pp_stats.sync_delay);
        printk ("Packet forwarded by the PP   : %u\n", pp_stats.packets_fwd);
        printk ("IPv4 Packets Forwarded       : %u\n", pp_stats.ipv4_packets_fwd);
        printk ("Descriptors Starved          : %u\n", pp_stats.desc_starved);
        printk ("Buffers Starved              : %u\n", pp_stats.buffer_starved);

        /* Work is completed. */
        return 0;
    }

    /* Check if VPID statistics were requested? */
    if (strcmp(argv[1], "vpid") == 0)
    {
        TI_PP_VPID  vpid[TI_PP_MAX_PID + 1];
        int         num_vpid;
        int         index = 0;

        /* Get a list of all VPID that exist in the System */
        num_vpid = ti_ppm_get_vpid (-1, TI_PP_MAX_PID, &vpid[0]);

            printk ("-----------------------------------------\n");
            printk ("    HIL State is  : %s\n", (global_ti_hil_db.hil_disabled)?"Disabled":"Enabled" );


        /* Cycle through all the VPID and get the stats */
        while (index < num_vpid)
        {
            TI_PP_VPID_STATS   vpid_stats;

            /* Get the VPID statistics. */
            ti_ppm_get_vpid_stats(vpid[index].vpid_handle, &vpid_stats);

            /* Print the statistics on the console. */
            printk ("---------------- VPID %d ----------------\n", vpid[index].vpid_handle);
            printk ("Rx Unicast   Packets: %u\n",     vpid_stats.rx_unicast_pkt);
            printk ("Rx Broadcast Packets: %u\n",     vpid_stats.rx_broadcast_pkt);
            printk ("Rx Multicast Packets: %u\n",     vpid_stats.rx_multicast_pkt);
            printk ("Rx Bytes            : 0x%08X%08X\n",  vpid_stats.rx_byte_hi, vpid_stats.rx_byte_lo);
            printk ("Rx Bytes - Low (dec): %u\n",     vpid_stats.rx_byte_lo);
            printk ("Rx Discard          : %u\n",     vpid_stats.rx_discard);
            printk ("Tx Unicast   Packets: %u\n",     vpid_stats.tx_unicast_pkt);
            printk ("Tx Broadcast Packets: %u\n",     vpid_stats.tx_broadcast_pkt);
            printk ("Tx Multicast Packets: %u\n",     vpid_stats.tx_multicast_pkt);
            printk ("Tx Bytes            : 0x%08X%08X\n",  vpid_stats.tx_byte_hi, vpid_stats.tx_byte_lo);
            printk ("Tx Bytes - Low (dec): %u\n",     vpid_stats.tx_byte_lo);
            printk ("Tx Errors           : %u\n",     vpid_stats.tx_error);
            printk ("Tx Discards         : %u\n",     vpid_stats.tx_discard);
            printk ("-----------------------------------------\n");

            /* Goto the next VPID. */
            index = index + 1;
        }
        /* Work is completed. */
        return 0;
    }

    /* Check if PID needs to be displayed? */
    if (strcmp(argv[1], "pid") == 0)
    {
        int         num_pid;
        TI_PP_PID   pid[TI_PP_MAX_PID+1];
        int         index = 0;

        /* Get the PID Information. */
        num_pid = ti_ppm_get_pid (TI_PP_MAX_PID, &pid[0]);

        /* Cycle through all the VPID and get the stats */
        while (index < num_pid)
        {
            /* Print the PID Information on the console. */
            printk ("PID %d\n", pid[index].pid_handle);

            /* Goto the next PID. */
            index = index + 1;
        }

        /* Work is completed. */
        return 0;
    }

    /* Check if Analysis Information needs to be displayed? */
    if (strcmp(argv[1], "stats") == 0)
    {
        int index;

        /* Print the HIL Analysis report. */
        printk (" HIL State is  : %s\n", (global_ti_hil_db.hil_disabled)? "Disabled":"Enabled" );
        printk (" DBG State is  : %s\n", (global_ti_hil_db.dbg_disabled)? "Disabled":"Enabled" );
        printk (" TDOX State is : %s\n", (global_ti_hil_db.tdox_disabled)? "Disabled":"Enabled" );
        printk (" Bypassed pkts : %u\n", global_ti_hil_db.num_bypassed_pkts );
        printk (" Other    pkts : %u\n", global_ti_hil_db.num_other_pkts    );
        printk (" Ingress  pkts : %u\n", global_ti_hil_db.num_ingress_pkts  );
        printk (" Egress   pkts : %u\n", global_ti_hil_db.num_egress_pkts   );
        printk (" Null     pkts : %u\n", global_ti_hil_db.num_null_drop_pkts );
        printk (" Total Sessions: %d\n", hil_num_total_sessions);
        printk (" Total Errors  : %d\n", hil_num_error);
        for(index = 0; index < HIL_MAX_NUM_BUCKETS; index++)
            printk("Bucket/No.Sessions %d / %d \n", index, hil_session[index]);

        /* Work is completed. */
        return 0;
    }

    /* Check if Session needs to be displayed? */
    if (strcmp(argv[1], "session") == 0)
    {
        int               num_session;
        unsigned char*    session;
        int               index = 0;

        /* Get the number of sessions that are available. */
        num_session = ti_ppm_get_session (-1, 0, NULL);
        printk ("Detected %d sessions in packet processor\n", num_session);

        /* Allocate memory for the sessions. */
        session = (unsigned char *)kmalloc(sizeof(unsigned char)*num_session, GFP_KERNEL);
        if (session == NULL)
            return -1;

        /* Get the Session Information - array of all the session indices */
        num_session = ti_ppm_get_session (-1, num_session, session);

        /* Cycle through all the VPID and get the stats */
        while (index < num_session)
        {
            /* Print the Session Information on the console. */
            printk ("---------------- Session %d ----------------\n", session[index]);
            ti_hil_intrusive_display_session(session[index]);

            /* Goto the next Session */
            index = index + 1;
        }

        /* Free the session buffer memory */
        kfree(session);

        /* Work is completed. */
        return 0;
    }

    /* cable_pp: add brief summary printing */
    if (strcmp(argv[1], "brief") == 0)
	{
		/* GLOBAL */
		{
			TI_PP_GLOBAL_STATS  pp_stats;

			/* YES. Get the global stats through the PPM */
			ti_ppm_get_global_stats(&pp_stats);

			/* Print the stats on the console. */
			printk ("Packets received in the PP   : %u\n", pp_stats.packets_rxed);
			printk ("Number of search attemps     : %u\n", pp_stats.packets_searched);
			printk ("Number of matched searches   : %u\n", pp_stats.search_matched);
			printk ("Number of Synch Delays       : %u\n", pp_stats.sync_delay);
			printk ("Packet forwarded by the PP   : %u\n", pp_stats.packets_fwd);
			printk ("IPv4 Packets Forwarded       : %u\n", pp_stats.ipv4_packets_fwd);
		}
		/* VPIDs */
		{
			TI_PP_VPID  vpid[TI_PP_MAX_PID + 1];
			int         num_vpid;
			int         index = 0;

			/* Get a list of all VPID that exist in the System */
			num_vpid = ti_ppm_get_vpid (-1, TI_PP_MAX_PID, &vpid[0]);

			/* Cycle through all the VPID and get the stats */
			while (index < num_vpid)
			{
				TI_PP_VPID_STATS   vpid_stats;

				/* Get the VPID statistics. */
				ti_ppm_get_vpid_stats(vpid[index].vpid_handle, &vpid_stats);
				printk ("VPID index=%02d, handle=%02d: Rx=%10d, Tx=%10d\n", index, vpid[index].vpid_handle,
					vpid_stats.rx_unicast_pkt+vpid_stats.rx_broadcast_pkt+vpid_stats.rx_multicast_pkt,
					vpid_stats.tx_unicast_pkt+vpid_stats.tx_broadcast_pkt+vpid_stats.tx_multicast_pkt);

				/* Goto the next VPID. */
				index = index + 1;
			}
		}
		/* SESSIONs */
		{
			int               num_session;
			unsigned char*    session;
			int               index = 0;

			/* Get the number of sessions that are available. */
			num_session = ti_ppm_get_session (-1, 0, NULL);

			/* Allocate memory for the sessions. */
			session = (unsigned char *)kmalloc(sizeof(unsigned char)*num_session, GFP_KERNEL);
			if (session == NULL)
				return -1;

			/* Get the Session Information - array of all the session indices */
			num_session = ti_ppm_get_session (-1, num_session, session);

			/* Cycle through all the VPID and get the stats */
			while (index < num_session)
			{

				TI_PP_SESSION_STATS   session_stats;

				/* Get the VPID statistics. */
				ti_ppm_get_session_stats(session[index], &session_stats);

				printk ("SESSION index=%02d, handle=%02d: Fwd=%10d\n", index, session[index],
					session_stats.packets_forwarded);
				/* Goto the next Session */
				index = index + 1;
			}

			/* Free the session buffer memory */
			kfree(session);
		}
        return 0;
	}

    /* Control comes here if the scope was not understood. */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_set_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor set command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int ti_hil_set_cmd_handler(int argc, char* argv[])
{
    /* Validate the number of arguments that have been passed. */
    if (argc != 3)
    {
        printk ("ERROR: Incorrect Number of parameters passed.\n");
        return -1;
    }

    /* Check if the "Global" stats were requested. */
    if (strcmp(argv[1], "timeout") == 0)
    {
        ti_session_timeout = (int) simple_strtol(argv[2], NULL, 0);
        if (ti_session_timeout < 0)
            return -1;

        /* Convert the session timeout to usec units */
        ti_session_timeout = ti_session_timeout * HZ * 1000;

        /* Work is completed. */
        return 0;
    }

    /* Control comes here if the command was not understood. */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_reset_cmd_handler
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the Packet Processor reset command handler.
 *
 * RETURNS       :
 *  -1      - Error.
 *  0       - Success.
 ***************************************************************************/
static int ti_hil_reset_cmd_handler(int argc, char* argv[])
{
    int i = 0;

    /* Validate the number of arguments that have been passed. */
    if (argc < 2)
    {
        printk ("ERROR: Incorrect Number of parameters passed.\n");
        return -1;
    }

    /* Reset the session timeout to default, i.e., 30 seconds. */
    if (strcmp(argv[1], "timeout") == 0)
    {
        ti_session_timeout = 30 * HZ * 1000;

        /* Work is completed. */
        return 0;
    }

    /* Reset the HIL Analysis stats. */
    if (strcmp(argv[1], "stats") == 0)
    {
        /* Initialize the counters for the HIL Analysis. */
        for(i = 0; i < HIL_MAX_NUM_BUCKETS; i++)
            hil_session[i] = 0;

        hil_num_total_sessions = 0;
        hil_num_error = 0;
        global_ti_hil_db.num_bypassed_pkts = 0;
        global_ti_hil_db.num_other_pkts = 0;
        global_ti_hil_db.num_ingress_pkts = 0;
        global_ti_hil_db.num_egress_pkts  = 0;
        global_ti_hil_db.num_null_drop_pkts = 0;

        /* Work is completed. */
        return 0;
    }

    /* Control comes here if the command was not understood. */
    return -1;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_write_cmds
 **************************************************************************
 * DESCRIPTION   :
 *  Interface for the Intrusive HIL. This is used to debug and display various
 *  packet processor entity information from the console.
 *
 * RETURNS       :
 *  -1              - Error.
 *  Non-Zero        - Success.
 ***************************************************************************/
static int ti_hil_write_cmds (struct file *file, const char *buffer, unsigned long count, void *data)
{
	char    pp_cmd[100];
	char*	argv[10];
	int		argc = 0;
	char*	ptr_cmd;
    char*   delimitters = " \n\t";
    char*   ptr_next_tok;

	/* Validate the length of data passed. */
	if (count > 100)
		count = 100;

    /* Initialize the buffer before using it. */
    memset ((void *)&pp_cmd[0], 0, sizeof(pp_cmd));
    memset ((void *)&argv[0], 0, sizeof(argv));

	/* Copy from user space. */
	if (copy_from_user (&pp_cmd, buffer, count))
		return -EFAULT;

    ptr_next_tok = &pp_cmd[0];

	/* Tokenize the command. Check if there was a NULL entry. If so be the case the
     * user did not know how to use the entry. Print the help screen. */
	ptr_cmd = strsep(&ptr_next_tok, delimitters);
    if (ptr_cmd == NULL)
        return -1;

    /* Parse all the commands typed. */
	do
	{
        /* Extract the first command. */
		argv[argc++] = ptr_cmd;

        /* Validate if the user entered more commands.*/
		if (argc >=10)
		{
			printk ("ERROR: Incorrect too many parameters dropping the command\n");
			return -EFAULT;
		}

        /* Get the next valid command. */
		ptr_cmd = strsep(&ptr_next_tok, delimitters);
	} while (ptr_cmd != NULL);

	/* We have an extra argument when strsep is used instead of strtok */
	argc--;

    /******************************* Command Handlers *******************************/

    /* Display Command Handlers */
    if (strncmp(argv[0], "show", strlen("show")) == 0)
    {
        /* Call the Show Command Handler. */
        if (ti_hil_show_cmd_handler (argc, argv) < 0)
            return -1;
    }

    /* Deinitialize Command Handlers */
    if (strncmp(argv[0], "deinit", strlen("deinit")) == 0)
    {
        /* Deinitialize all the packet processor components. */
        if (ti_ppm_deinitialize() < 0)
        {
            printk ("Error: Packet Processor Deinitialization Failed\n");
            return -1;
        }
    }

    /* Set Command Handlers */
    if (strncmp(argv[0], "set", strlen("set")) == 0)
    {
        /* Call the Set Command Handler. */
        if (ti_hil_set_cmd_handler (argc, argv) < 0)
            return -1;
    }

    /* Deinitialize Command Handlers */
    if (strncmp(argv[0], "reset", strlen("reset")) == 0)
    {
        /* Call the Reset Command Handler. */
        if (ti_hil_reset_cmd_handler (argc, argv) < 0)
            return -1;
    }

    /* cable_pp: disable/enable capability */
    if (strcmp(argv[0], "enable") == 0)
    {
	    global_ti_hil_db.hil_disabled = 0;
    }

    if (strcmp(argv[0], "disable") == 0)
    {
	    global_ti_hil_db.hil_disabled = 1;
	    ti_ppm_flush_sessions(-1);
    }

    if (strcmp(argv[0], "dbg") == 0)
    {
	    global_ti_hil_db.dbg_disabled = 0;
    }

    if (strcmp(argv[0], "nodbg") == 0)
    {
	    global_ti_hil_db.dbg_disabled = 1;
    }

    if (strcmp(argv[0], "tdox") == 0)
    {
	    global_ti_hil_db.tdox_disabled = 0;
    }

    if (strcmp(argv[0], "notdox") == 0)
    {
	    global_ti_hil_db.tdox_disabled = 1;
    }

    return count;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_health_timer_expired
 **************************************************************************
 * DESCRIPTION   :
 *  The function is the health timer expiration routine which is called by
 *  periodically by the HOST to verify the sanity of the PDSP.
 **************************************************************************/
static void ti_hil_health_timer_expired (unsigned long data)
{
    /* Use the PPM API to determine the health of the PDSP. */
    if (ti_ppm_health_check() < 0)
    {
        /* PDSP dont seem to be working correctly... This is a FATAL Condition and
         * needs to be handled... We cause the system to crash here as an indication
         * that this needs to be addressed. System Profiles need to handle this in
         * a more robust manner. */
        printk ("------- FATAL Error: Packet Processor PDSP is not healthy ------- \n");
        BUG();
        return;
    }

    /* The PDSP passed the health check; restart the timer */
    pdsp_health_timer.expires = jiffies + PDSP_HEALTH_TIMER*HZ;
    add_timer (&pdsp_health_timer);
    return;
}

int ti_hil_read_devs(char* buf, char **start, off_t offset, int count,
                 int *eof, void *data)
{
    unsigned int i;
    int len=0;
    unsigned int limit = count - 80;

#ifndef TI_MAX_DEVICE_INDEX
#define TI_MAX_DEVICE_INDEX 32
#endif

    for( i = 0; i < TI_MAX_DEVICE_INDEX; i++ )
    {
         struct net_device *cur_dev;
         TI_PP_VPID_STATS   vpid_stats;

         cur_dev = dev_get_by_index( i );

         if( !cur_dev )
              continue;

         /* Get the VPID statistics. */
         if( ti_ppm_get_vpid_stats( cur_dev->vpid_handle , &vpid_stats) < 0 )
              continue;

         /* Print the statistics on the console. */
         if( len < limit )
             len += sprintf(buf + len, "   /dev/%s:\n", cur_dev->name );
         if( len < limit )
             len += sprintf(buf + len, "-----------------------------------------\n");
         if( len < limit )
             len += sprintf(buf + len, "Rx Unicast   Packets: %u\n", vpid_stats.rx_unicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Rx Broadcast Packets: %u\n", vpid_stats.rx_broadcast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Rx Multicast Packets: %u\n", vpid_stats.rx_multicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Rx Bytes            : 0x%08X%08X\n", vpid_stats.rx_byte_hi, vpid_stats.rx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Rx Bytes - low      : %u\n", vpid_stats.rx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Rx Discard          : %u\n", vpid_stats.rx_discard);
         if( len < limit )
             len += sprintf(buf + len, "Tx Unicast   Packets: %u\n", vpid_stats.tx_unicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Broadcast Packets: %u\n", vpid_stats.tx_broadcast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Multicast Packets: %u\n", vpid_stats.tx_multicast_pkt);
         if( len < limit )
             len += sprintf(buf + len, "Tx Bytes            : 0x%08X%08X\n", vpid_stats.tx_byte_hi, vpid_stats.tx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Tx Bytes - low      : %u\n", vpid_stats.tx_byte_lo);
         if( len < limit )
             len += sprintf(buf + len, "Tx Errors           : %u\n", vpid_stats.tx_error);
         if( len < limit )
             len += sprintf(buf + len, "Tx Discards         : %u\n", vpid_stats.tx_discard);
    }

    return len;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_init
 **************************************************************************
 * DESCRIPTION   :
 *  Initialization function for the Intrusive mode profile.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_intrusive_init (void)
{
    struct proc_dir_entry*	ptr_dir_entry;
    struct proc_dir_entry*	ptr_dev_entry;
    int                     index = 0;

    /* Register an event handler to listen to events. */
    ppsubsystem_event_handler = ti_ppm_register_event_handler (ti_hil_ppsubsystem_event_handler);
    if (ppsubsystem_event_handler == 0)
    {
        printk ("Error: Event Handler register failed\n");
        return -1;
    }

    /* Create the PROC Entry used by the TCA Configuration Engine. */
    ptr_dir_entry = create_proc_entry("net/ti_pp" ,0644, NULL);
    ptr_dev_entry = create_proc_entry("net/ti_pp_dev" ,0644, NULL);

    if( (ptr_dir_entry == NULL) || (ptr_dev_entry == NULL ) )
	{
        printk ("Error: Unable to create Packet Processor proc entry.\n");
    	return -1;
    }
    ptr_dir_entry->data 	 = NULL;
	ptr_dir_entry->read_proc  = NULL;
    ptr_dir_entry->write_proc = ti_hil_write_cmds;
	ptr_dir_entry->owner 	 = THIS_MODULE;

    ptr_dev_entry->data 	 = NULL;
	ptr_dev_entry->read_proc  = ti_hil_read_devs;
    ptr_dev_entry->write_proc = NULL;;
	ptr_dev_entry->owner 	 = THIS_MODULE;

    /* Create a timer to poll for the health */
    init_timer (&pdsp_health_timer);
    pdsp_health_timer.function = ti_hil_health_timer_expired;
    pdsp_health_timer.data     = 0;

    /* Start the timer. */
    pdsp_health_timer.expires = jiffies + PDSP_HEALTH_TIMER*HZ;
    add_timer (&pdsp_health_timer);

    /* Initialize the counters for the HIL Analysis. */
    for(index = 0; index < HIL_MAX_NUM_BUCKETS; index++)
        hil_session[index] = 0;

#ifdef CONFIG_NETFILTER
    /* Initialize the HIL Session to Connection Tracking Mapper. */
    memset ((void *)&hil_session_ct_mapper[0], 0, sizeof(hil_session_ct_mapper));
#endif /* CONFIG_NETFILTER */

#ifdef CONFIG_IP_MULTICAST
    /* Initialize the MFC to Session Mapper. */
    memset ((void *)&hil_mfc_session_mapper[0], 0, sizeof(hil_mfc_session_mapper));;
#endif /* CONFIG_IP_MULTICAST */

    ti_hil_tdox_init();

    /* Profile has been successfully initialized. */
	return 0;
}

/**************************************************************************
 * FUNCTION NAME : ti_hil_intrusive_deinit
 **************************************************************************
 * DESCRIPTION   :
 *  Deinitialization function which deinitializes and unregisters the default
 *  profile with the HIL Core.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 **************************************************************************/
static int ti_hil_intrusive_deinit(void)
{
    return 0;
}

