/*
 * ppd_test.c
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

#include <linux/config.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include <pal_cppi41.h>
#include <ti_ppd.h>
#include <asm-arm/arch-avalanche/puma5/puma5_pp.h>

#if 0
static TI_PP_PID_RANGE  pid_range_eth;
static TI_PP_PID        pid_eth;
static TI_PP_VPID       ingress_vpid, egress_vpid;
#else
static TI_PP_PID        pid_eth[1];
static TI_PP_VPID       vpid_eth[2];    /* Multiple egress rec */
#endif

int avalanche_ppd_proc_init(void);

static void ppdt_event_handler (unsigned short event_id,
                                unsigned int param1,
                                unsigned int param2)
{
    static int counter = 1;
    printk ("ppdt_event_handler(%d): Got event %d, index %d, aux code %d\n",
                counter++, (int)event_id, (int)param1, (int)param2);
}


int ppdt_init(void)
{
    /* Replicate the PID and VPID configurations maintained in the driver to be
     * used for sessions. By doing this we are avoiding using driver data types
     * as 'extern' and setting non required fileds in driver.
     */
    pid_eth[0].type            = TI_PP_PID_TYPE_ETHERNET;
    pid_eth[0].ingress_framing   = 1 /*TI_PP_PID_IN_FRM_ETH*/;
    pid_eth[0].pri_mapping     = 1;    /* Num prio Qs for fwd */
    pid_eth[0].dflt_pri_drp    = 0;
    pid_eth[0].dflt_dst_tag    = 0x3FFF;
    pid_eth[0].dflt_fwd_q      = CPMAC_CPPI4x_ETH_TO_HOST_PRXY_QNUM(0);/* Fwd to inf0 by default */
    pid_eth[0].tx_pri_q_map[0] = CPMAC_CPPI4x_TX_QNUM(0);  /* Default Q used for egress rec */
    pid_eth[0].pid_handle      = PP_ETH_PID_BASE+0;

    vpid_eth[0].type               = TI_PP_ETHERNET;   /* No VLAN, PPP LUT*/
    vpid_eth[0].parent_pid_handle  = pid_eth[0].pid_handle;        /* Needed for LUT */
    vpid_eth[0].egress_mtu         = 0;        /* Don't care */
    vpid_eth[0].priv_tx_data_len   = 0;

    /* HACK: Since VPID is created and managed by PPM, anticipating value of
     * VPID here, assuming only ethernet is present.
     */
    vpid_eth[0].vpid_handle = 31;

    avalanche_ppd_proc_init ();

    ti_ppd_register_event_handler (ppdt_event_handler);
#ifdef USE_QOS
    setup_qos ();
#endif
    return 0;
}

/********************** Configure QPDSP ********************/
#ifdef USE_QOS
#define QM_QOSQ(num)            (64+num)        // QOS queues
// QOS Configuration
#define SROFF_QOSCLUSTER        0xC00
#define SROFF_QOSRESERVED       0xCC0
#define SROFF_QOSQUEUE          0xD00

//
// ** QOS Cluster Information **
//
#define QOSCLUST_PTR(chan)              ((AVALANCHE_NWSS_CPDSP_SCRATCH_RAM_BASE+SROFF_QOSCLUSTER)+chan*64)
#define QCLUST_GLOBALCREDIT(chan)       (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x00))
#define QCLUST_MAXCREDIT(chan)          (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x04))
#define QCLUST_W2(chan)                 (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x08))
#define   QCW2_QQCOUNT_SHIFT            0
#define   QCW2_QQCOUNT_MASK             (0xFFu<<QCW2_QQCOUNT_SHIFT)
#define   QCW2_QQ0_SHIFT                8
#define   QCW2_QQ0_MASK                 (0xFFu<<QCW2_QQ0_SHIFT)
#define   QCW2_QQ1_SHIFT                16
#define   QCW2_QQ1_MASK                 (0xFFu<<QCW2_QQ1_SHIFT)
#define   QCW2_QQ2_SHIFT                24
#define   QCW2_QQ2_MASK                 (0xFFu<<QCW2_QQ2_SHIFT)
#define QCLUST_W3(chan)                 (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x0C))
#define   QCW3_QQ3_SHIFT                0
#define   QCW3_QQ3_MASK                 (0xFFu<<QCW3_QQ3_SHIFT)
#define   QCW3_QQ4_SHIFT                8
#define   QCW3_QQ4_MASK                 (0xFFu<<QCW3_QQ4_SHIFT)
#define   QCW3_QQ5_SHIFT                16
#define   QCW3_QQ5_MASK                 (0xFFu<<QCW3_QQ5_SHIFT)
#define   QCW3_QQ6_SHIFT                24
#define   QCW3_QQ6_MASK                 (0xFFu<<QCW3_QQ6_SHIFT)
#define QCLUST_W4(chan)                 (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x10))
#define   QCW4_QQ7_SHIFT                0
#define   QCW4_QQ7_MASK                 (0xFFu<<QCW4_QQ7_SHIFT)
#define   QCW4_QQ8_SHIFT                8
#define   QCW4_QQ8_MASK                 (0xFFu<<QCW4_QQ8_SHIFT)
#define   QCW4_EQCOUNT_SHIFT            16
#define   QCW4_EQCOUNT_MASK             (0xFFu<<QCW4_EQCOUNT_SHIFT)
#define   QCW4_EQ0_SHIFT                24
#define   QCW4_EQ0_MASK                 (0xFFu<<QCW4_EQ0_SHIFT)
#define QCLUST_W5(chan)                 (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x14))
#define   QCW5_EQ1_SHIFT                0
#define   QCW5_EQ1_MASK                 (0xFFu<<QCW5_EQ1_SHIFT)
#define   QCW5_EQ2_SHIFT                8
#define   QCW5_EQ2_MASK                 (0xFFu<<QCW5_EQ2_SHIFT)
#define   QCW5_EQ3_SHIFT                16
#define   QCW5_EQ3_MASK                 (0xFFu<<QCW5_EQ3_SHIFT)
#define   QCW5_EQ4_SHIFT                24
#define   QCW5_EQ4_MASK                 (0xFFu<<QCW5_EQ4_SHIFT)
#define QCLUST_W6(chan)                 (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x18))
#define   QCW6_EQ5_SHIFT                0
#define   QCW6_EQ5_MASK                 (0xFFu<<QCW6_EQ5_SHIFT)
#define   QCW6_EQ6_SHIFT                8
#define   QCW6_EQ6_MASK                 (0xFFu<<QCW6_EQ6_SHIFT)
#define   QCW6_EQ7_SHIFT                16
#define   QCW6_EQ7_MASK                 (0xFFu<<QCW6_EQ7_SHIFT)
#define   QCW6_EQ8_SHIFT                24
#define   QCW6_EQ8_MASK                 (0xFFu<<QCW6_EQ8_SHIFT)
#define QCLUST_CONGESTLEVEL1(chan)      (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x1C))
#define QCLUST_CONGESTLEVEL2(chan)      (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x20))
#define QCLUST_CONGESTLEVEL3(chan)      (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x24))
#define QCLUST_CONGESTLEVEL4(chan)      (*(volatile UINT32 *)(QOSCLUST_PTR(chan)+0x28))

// Individual Field definitions
#define QCW2_QQCOUNT(x)                 (((x)<<QCW2_QQCOUNT_SHIFT)&QCW2_QQCOUNT_MASK)
#define QCW2_QQ0(x)                     (((x)<<QCW2_QQ0_SHIFT)&QCW2_QQ0_MASK)
#define QCW2_QQ1(x)                     (((x)<<QCW2_QQ1_SHIFT)&QCW2_QQ1_MASK)
#define QCW2_QQ2(x)                     (((x)<<QCW2_QQ2_SHIFT)&QCW2_QQ2_MASK)
#define QCW3_QQ3(x)                     (((x)<<QCW3_QQ3_SHIFT)&QCW3_QQ3_MASK)
#define QCW3_QQ4(x)                     (((x)<<QCW3_QQ4_SHIFT)&QCW3_QQ4_MASK)
#define QCW3_QQ5(x)                     (((x)<<QCW3_QQ5_SHIFT)&QCW3_QQ5_MASK)
#define QCW3_QQ6(x)                     (((x)<<QCW3_QQ6_SHIFT)&QCW3_QQ6_MASK)
#define QCW4_QQ7(x)                     (((x)<<QCW4_QQ7_SHIFT)&QCW4_QQ7_MASK)
#define QCW4_QQ8(x)                     (((x)<<QCW4_QQ8_SHIFT)&QCW4_QQ8_MASK)
#define QCW4_EQCOUNT(x)                 (((x)<<QCW4_EQCOUNT_SHIFT)&QCW4_EQCOUNT_MASK)
#define QCW4_EQ0(x)                     (((x)<<QCW4_EQ0_SHIFT)&QCW4_EQ0_MASK)
#define QCW5_EQ1(x)                     (((x)<<QCW5_EQ1_SHIFT)&QCW5_EQ1_MASK)
#define QCW5_EQ2(x)                     (((x)<<QCW5_EQ2_SHIFT)&QCW5_EQ2_MASK)
#define QCW5_EQ3(x)                     (((x)<<QCW5_EQ3_SHIFT)&QCW5_EQ3_MASK)
#define QCW5_EQ4(x)                     (((x)<<QCW5_EQ4_SHIFT)&QCW5_EQ4_MASK)
#define QCW6_EQ5(x)                     (((x)<<QCW6_EQ5_SHIFT)&QCW6_EQ5_MASK)
#define QCW6_EQ6(x)                     (((x)<<QCW6_EQ6_SHIFT)&QCW6_EQ6_MASK)
#define QCW6_EQ7(x)                     (((x)<<QCW6_EQ7_SHIFT)&QCW6_EQ7_MASK)
#define QCW6_EQ8(x)                     (((x)<<QCW6_EQ8_SHIFT)&QCW6_EQ8_MASK)

//
// ** QOS Queue Information **
//
#define QOSQUEUE_PTR(chan)              ((SRSCRATCH_BASEADDR+SROFF_QOSQUEUE)+chan*24)
#define QQUEUE_W0(chan)                 (*(volatile UINT32 *)(QOSQUEUE_PTR(chan)+0x00))
#define   QQW0_FLAGS_SHIFT              0
#define   QQW0_FLAGS_MASK               (0xFFu<<QQW0_FLAGS_SHIFT)
#define   QQW0_EGRESS_SHIFT             8
#define   QQW0_EGRESS_MASK              (0xFFu<<QQW0_EGRESS_SHIFT)
#define   QQW0_ITERATION_SHIFT          16
#define   QQW0_ITERATION_MASK           (0xFFFFu<<QQW0_ITERATION_SHIFT)
#define QQUEUE_TOTALCEDIT(chan)         (*(volatile UINT32 *)(QOSQUEUE_PTR(chan)+0x04))
#define QQUEUE_MAXCREDIT(chan)          (*(volatile UINT32 *)(QOSQUEUE_PTR(chan)+0x08))
#define QQUEUE_CONGESTLEVEL(chan)       (*(volatile UINT32 *)(QOSQUEUE_PTR(chan)+0x0C))
#define QQUEUE_PKTFORWARD(chan)         (*(volatile UINT32 *)(QOSQUEUE_PTR(chan)+0x10))
#define QQUEUE_PKTDROP(chan)            (*(volatile UINT32 *)(QOSQUEUE_PTR(chan)+0x14))

// Individual Field definitions
#define QQW0_FLAGS(x)                   (((x)<<QQW0_FLAGS_SHIFT)&QQW0_FLAGS_MASK)
#define     QQW0FLAG_REALTIME               (1u<<0)
#define QQW0_EGRESS(x)                  (((x)<<QQW0_EGRESS_SHIFT)&QQW0_EGRESS_MASK)
#define QQW0_ITERATION(x)               (((x)<<QQW0_ITERATION_SHIFT)&QQW0_ITERATION_MASK)

int setup_qos ()
{
    int rc, i;

    //
    // Cluster 0
    //
    // Setup 2 QOS queues, one that gets 1Gb/s and then trickles down to the other.
    //

    // Queue 0
    QQUEUE_W0(0) = QQW0_FLAGS(0) | QQW0_ITERATION(3125) | QQW0_EGRESS(QM_EMAC0TXQ(0u));
    QQUEUE_TOTALCEDIT(0) = 0;
    QQUEUE_MAXCREDIT(0) = 1514;
    QQUEUE_CONGESTLEVEL(0) = 15140;
    QQUEUE_PKTFORWARD(0) = 0;
    QQUEUE_PKTDROP(0) = 0;

    // Queue 1
    QQUEUE_W0(1) = QQW0_FLAGS(0) | QQW0_ITERATION(0) | QQW0_EGRESS(QM_EMAC0TXQ(0u));
    QQUEUE_TOTALCEDIT(1) = 0;
    QQUEUE_MAXCREDIT(1) = 1514;
    QQUEUE_CONGESTLEVEL(1) = 15140;
    QQUEUE_PKTFORWARD(1) = 0;
    QQUEUE_PKTDROP(1) = 0;

    // Cluster 0
    QCLUST_GLOBALCREDIT(0) = 0;
    QCLUST_MAXCREDIT(0) = 15140;
    QCLUST_W2(0) = QCW2_QQCOUNT(2) | QCW2_QQ0(0) | QCW2_QQ1(1);
    QCLUST_W3(0) = 0;
    QCLUST_W4(0) = QCW4_EQCOUNT(1) | QCW4_EQ0(QM_EMAC0TXQ(0u));
    QCLUST_W5(0) = 0;
    QCLUST_W6(0) = 0;
    QCLUST_CONGESTLEVEL1(0) = 3096;
    QCLUST_CONGESTLEVEL2(0) = 6192;
    QCLUST_CONGESTLEVEL3(0) = 12384;
    QCLUST_CONGESTLEVEL4(0) = 24768;


    printk("Adding cluster 0\n");
    CPDSP_COMMAND = CCMD_COMMAND(SRPDSP_QOS_CLUSTER) |
                    CCMD_OPTION(1) |
                    CCMD_INDEX(0);

    // Wait for CPDSP command buffer to clear
    for( i=0; i<1000000 && CPDSP_COMMAND; i++);
    if( i==1000000 )
        { printk("Error - CPDSP firmware not responding!\n"); return(-1); }

    rc = CPDSP_PARAMETER0;
    printk("Return Value = %d\n",rc);


    //
    // Cluster 1
    //
    // Setup 1 QOS queue, one that gets 200Mb/s and forwards to the host.
    //

    // Queue 2
    QQUEUE_W0(2) = QQW0_FLAGS(0) | QQW0_ITERATION(625) | QQW0_EGRESS(QM_INF0TXQ(0u));
    QQUEUE_TOTALCEDIT(2) = 0;
    QQUEUE_MAXCREDIT(2) = 1518;
    QQUEUE_CONGESTLEVEL(2) = 3036;
    QQUEUE_PKTFORWARD(2) = 0;
    QQUEUE_PKTDROP(2) = 0;

    // Cluster 1
    QCLUST_GLOBALCREDIT(1) = 0;
    QCLUST_MAXCREDIT(1) = 1518;
    QCLUST_W2(1) = QCW2_QQCOUNT(1) | QCW2_QQ0(2);
    QCLUST_W3(1) = 0;
    QCLUST_W4(1) = QCW4_EQCOUNT(1) | QCW4_EQ0(QM_INF0TXQ(0u));
    QCLUST_W5(1) = 0;
    QCLUST_W6(1) = 0;
    QCLUST_CONGESTLEVEL1(1) = 0;
    QCLUST_CONGESTLEVEL2(1) = 6192;
    QCLUST_CONGESTLEVEL3(1) = 12384;
    QCLUST_CONGESTLEVEL4(1) = 24768;

    printk("Adding cluster 1\n");
    CPDSP_COMMAND = CCMD_COMMAND(SRPDSP_QOS_CLUSTER) |
                    CCMD_OPTION(1) |
                    CCMD_INDEX(1);

    // Wait for CPDSP command buffer to clear
    for( i=0; i<1000000 && CPDSP_COMMAND; i++);
    if( i==1000000 )
        { printk("Error - CPDSP firmware not responding!\n"); return(-1); }

    rc = CPDSP_PARAMETER0;
    printk("Return Value = %d\n",rc);

    return rc;
}
#endif /* USE_QOS */

/* Used fro modify session */
static TI_PP_SESSION       ses_cfg_bak;
static TI_PPD_IF           ingress_if_bak, egress_if_bak[2];

/***********************************************************/

/* Special Sessions :
 * Session 5 -> ETH classification based on src+dst MAC, !routable, replace L2
 * Session 7 -> Full ETH classification for IPoE, routable
 * Session 8 -> Full L2 classification for IPoE, !routable, priv vpid tx data,
 *              replace L2
 * Session 9 -> 2 Egress : ETH and PPPoE
 */

/* PPDT_PPPOE_TEST -->
 * Session 1 as IPoE -> PPPoE
 * Session 2 as PPPoE -> PPPoE bridged
 * Session 3 as PPPoE -> PPPoE with IP classification
 * session 4 as PPPoE -> IP
 */
#define PPDT_PPPOE_TEST

#ifdef PPDT_PPPOE_TEST
TI_PP_VPID vpid_pppoe;
#endif

/* VLAN --->
 * Session 10 as IpoE VLAN -> IPoE VLAN
 * Session 11 as IpoE -> IpoE VLAN
 * Session 12 as IPoE VLAN -> IpoE
 */
TI_PP_VPID vpid_vlan;

/* VLAN PPPoE --->
 * Session 20 as IpoE VLAN PPPoE -> IPoE VLAN PPPoE
 * Session 21 as IpoE PPPoE -> IpoE VLAN PPPoE
 * Session 22 as IPoE VLAN PPPoE -> IpoE PPPoE
 * Session 23 as IpoE -> IPoE VLAN PPPoE
 * Session 24 as IPoE VLAN PPPoE -> IpoE
 */
TI_PP_VPID vpid_vlan_pppoe;

int ppdt_add_session (int ses_id)
{
    Int32 ret_val;
    TI_PP_SESSION       ses_cfg;
    TI_PPD_IF           ingress_if, egress_if[2];
    TI_PP_ETH_DESC      *in_eth, *out_eth;
    TI_PP_IPV4_DESC     *in_ipv4, *out_ipv4;
    static int is_pppoe_vpid_created = 0;
    static int is_vlan_pppoe_vpid_created = 0;
    static int is_vlan_vpid_created = 0;

#ifndef CONFIG_TI_PACKET_PROCESSOR
    static int is_eth_vpid_created = 0;

    if (!is_eth_vpid_created) {
        ret_val = ti_ppd_create_vpid (&vpid_eth[0]);
        printk ("Return code from ti_ppd_create_vpid for Ethernet VPID(%d) = %d.\n",
                    vpid_eth[0].vpid_handle, ret_val);
        is_eth_vpid_created = 1;
    }
#endif

    if (!is_vlan_pppoe_vpid_created) {
        vpid_vlan_pppoe.type               = TI_PP_VLAN_PPPoE;   /* No VLAN, PPP LUT*/
        vpid_vlan_pppoe.parent_pid_handle  = pid_eth[0].pid_handle;        /* Needed for LUT */
        vpid_vlan_pppoe.egress_mtu         = 0;        /* Don't care */
        vpid_vlan_pppoe.priv_tx_data_len   = 0;
        vpid_vlan_pppoe.vpid_handle = 28;
        vpid_vlan_pppoe.ppp_session_id = 0;
        vpid_vlan_pppoe.vlan_identifier = 0xff;

        ret_val = ti_ppd_create_vpid (&vpid_vlan_pppoe);
        printk ("Return code from ti_ppd_create_vpid for VLAN PPPoE VPID(%d) = %d.\n",
                    vpid_vlan_pppoe.vpid_handle, ret_val);
        is_vlan_pppoe_vpid_created = 1;
    }

    if (!is_vlan_vpid_created) {
        vpid_vlan.type               = TI_PP_VLAN;   /* No VLAN, PPP LUT*/
        vpid_vlan.parent_pid_handle  = pid_eth[0].pid_handle;        /* Needed for LUT */
        vpid_vlan.egress_mtu         = 0;        /* Don't care */
        vpid_vlan.priv_tx_data_len   = 0;
        vpid_vlan.vpid_handle = 29;
        vpid_vlan.vlan_identifier = 0xff;

        ret_val = ti_ppd_create_vpid (&vpid_vlan);
        printk ("Return code from ti_ppd_create_vpid for VLAN VPID(%d) = %d.\n",
                    vpid_vlan.vpid_handle, ret_val);
        is_vlan_vpid_created = 1;
    }

    if (!is_pppoe_vpid_created) {
        vpid_pppoe.type               = TI_PP_PPPoE;   /* No VLAN, PPP LUT*/
        vpid_pppoe.parent_pid_handle  = pid_eth[0].pid_handle;        /* Needed for LUT */
        vpid_pppoe.egress_mtu         = 0;        /* Don't care */
        vpid_pppoe.priv_tx_data_len   = 0;
        vpid_pppoe.vpid_handle = 30;
        vpid_pppoe.ppp_session_id = 0;

        ret_val = ti_ppd_create_vpid (&vpid_pppoe);
        printk ("Return code from ti_ppd_create_vpid for PPPoE VPID(%d) = %d.\n",
                    vpid_pppoe.vpid_handle, ret_val);
        is_pppoe_vpid_created = 1;
    }

    ses_cfg.session_handle  = ses_id;
    ses_cfg.session_timeout = 0;
#ifdef PPDT_PPPOE_TEST
    if (ses_id == 2)
        ses_cfg.is_routable_session = 0;
    else
#endif
    if (ses_id == 8) {
        vpid_eth[0].priv_tx_data_len = 6;
        vpid_eth[0].priv_tx_data[0] = 0x00;
        vpid_eth[0].priv_tx_data[1] = 0x0d;
        vpid_eth[0].priv_tx_data[2] = 0x56;
        vpid_eth[0].priv_tx_data[3] = 0x6d;
        vpid_eth[0].priv_tx_data[4] = 0x15;
        vpid_eth[0].priv_tx_data[5] = 0xd4;
        ses_cfg.is_routable_session = 0;
    } else if ((ses_id == 5))
        ses_cfg.is_routable_session = 0;
    else
        ses_cfg.is_routable_session = 1;

    ses_cfg.priority = 0;
    ses_cfg.docsis_id = 0xFF;

    //ses_prop.ses_timeout    = 60 * 100000;  // 60 sec

    ses_cfg.num_egress = 1;

    /* For LUT L2 */
    ses_cfg.ingress.l2_packet.packet_type  = TI_PP_ETH_TYPE;
    in_eth = &ses_cfg.ingress.l2_packet.u.eth_desc;
    in_eth->enables = 0;

    if ((ses_id == 7) || (ses_id == 5)) {
        in_eth->enables = TI_PP_SESSION_L2_DSTMAC_VALID
                            | TI_PP_SESSION_L2_SRCMAC_VALID;
        in_eth->dstmac[0] = 0x08;
        in_eth->dstmac[1] = 0x00;
        in_eth->dstmac[2] = 0x28;
        in_eth->dstmac[3] = 0x32;
        in_eth->dstmac[4] = 0x06;
        in_eth->dstmac[5] = 0x02;
    }

    in_eth->enables |= TI_PP_SESSION_L2_SRCMAC_VALID;

    in_eth->srcmac[0] = 0x00;
    in_eth->srcmac[1] = 0x0d;
    in_eth->srcmac[2] = 0x56;
    in_eth->srcmac[3] = 0x6d;
    in_eth->srcmac[4] = 0x15;
    in_eth->srcmac[5] = ses_id;

    ses_cfg.ingress.l3l4_packet.packet_type  = TI_PP_IPV4_TYPE;
    in_ipv4 = &ses_cfg.ingress.l3l4_packet.u.ipv4_desc;
    in_ipv4->dst_ip = 0xAC18BEA6;
    in_ipv4->src_ip = 0xAC18BEA7;
    in_ipv4->dst_port = 9320+ses_id;
    in_ipv4->src_port = 4660;
    in_ipv4->enables = 0;

    if ((ses_id != 2) && (ses_id != 5)) {
        in_ipv4->enables  = TI_PP_SESSION_IPV4_DSTIP_VALID
                            | TI_PP_SESSION_IPV4_SRCIP_VALID
                            | TI_PP_SESSION_IPV4_DST_PORT_VALID
                            | TI_PP_SESSION_IPV4_SRC_PORT_VALID;

        if (ses_id == 6) {
            in_ipv4->dst_port = 9320+0;
            in_ipv4->protocol   = 6;  /* TCP */
            in_ipv4->enables |= TI_PP_SESSION_IPV4_PROTOCOL_VALID;
        } else if (ses_id != 1) {
            in_ipv4->protocol   = 17;  /* UDP */
            in_ipv4->enables |= TI_PP_SESSION_IPV4_PROTOCOL_VALID;
        }
    }

#if 0
    ses_prop.egr_rec[0].frame_code  = TI_PP_EGR_FRM_ETH;
#endif

    ses_cfg.egress[0].l2_packet.packet_type  = TI_PP_ETH_TYPE;
    out_eth = &ses_cfg.egress[0].l2_packet.u.eth_desc;
    out_eth->enables = 0;
    out_eth->dstmac[0] = 0x00;
    out_eth->dstmac[1] = 0x0d;
    out_eth->dstmac[2] = 0x56;
    out_eth->dstmac[3] = 0x6d;
    out_eth->dstmac[4] = 0x15;
    out_eth->dstmac[5] = 0xd4;

    out_eth->srcmac[0] = 0x00;
    out_eth->srcmac[1] = 0x00;
    out_eth->srcmac[2] = 0x00;
    out_eth->srcmac[3] = 0x00;
    out_eth->srcmac[4] = 0x00;
    out_eth->srcmac[5] = 0x00;

    if (ses_id != 8)
        out_eth->enables = TI_PP_SESSION_L2_DSTMAC_VALID
                            | TI_PP_SESSION_L2_SRCMAC_VALID;                       ;


    ses_cfg.egress[0].l3l4_packet.packet_type  = TI_PP_IPV4_TYPE;
    out_ipv4 = &ses_cfg.egress[0].l3l4_packet.u.ipv4_desc;

    switch(ses_id)
    {
        case 0:
        out_ipv4->dst_ip = 0xAC18BEA5;
        break;
        case 1:
        out_ipv4->dst_ip = 0xAC18BEA8;
        break;
        case 2:
        out_ipv4->dst_ip = 0xAC18BEA7;
        break;
        case 3:
        out_ipv4->dst_ip = 0xAC18BEA6;
        break;
    }

    out_ipv4->src_ip = 0xAC18BEA6;
    out_ipv4->src_port = 9320+ses_id;
    out_ipv4->dst_port = 4660;
    out_ipv4->protocol   = 17;  /* UDP - Don't care */
    out_ipv4->enables = TI_PP_SESSION_IPV4_DSTIP_VALID
                        | TI_PP_SESSION_IPV4_SRCIP_VALID
                        | TI_PP_SESSION_IPV4_DST_PORT_VALID
                        | TI_PP_SESSION_IPV4_SRC_PORT_VALID;

#ifdef PPDT_PPPOE_TEST
    if ((ses_id == 2) || (ses_id == 3) || (ses_id == 4))
        ingress_if.vpid = &vpid_pppoe;
    else
#endif
    if ((ses_id == 10) || (ses_id == 12))
        ingress_if.vpid = &vpid_vlan;
    else if ((ses_id == 20) || (ses_id == 22) || (ses_id == 24))
        ingress_if.vpid = &vpid_vlan_pppoe;
    else if ((ses_id == 21))
        ingress_if.vpid = &vpid_pppoe;
    else
        ingress_if.vpid = &vpid_eth[0];

#ifdef PPDT_PPPOE_TEST
    if ((ses_id == 1) || (ses_id == 2) || (ses_id == 3))
        egress_if[0].vpid = &vpid_pppoe;
    else
#endif
    if ((ses_id == 10) || (ses_id == 11))
        egress_if[0].vpid = &vpid_vlan;
    else if ((ses_id == 20) || (ses_id == 21) || (ses_id == 23))
        egress_if[0].vpid = &vpid_vlan_pppoe;
    else if ((ses_id == 22))
        egress_if[0].vpid = &vpid_pppoe;
    else
        egress_if[0].vpid = &vpid_eth[0];

    ingress_if.pid = &pid_eth[0];
    egress_if[0].pid = &pid_eth[0];

    if (ses_id == 9)
    {
        ses_cfg.egress[1] = ses_cfg.egress[0];
        egress_if[1].vpid = &vpid_pppoe;
        egress_if[1].pid = &pid_eth[0];
        ses_cfg.num_egress = 2;
    }

    /* Back-up cfg to be able to modify */
    if (ses_id == 0) {
        ses_cfg_bak = ses_cfg;
        ingress_if_bak = ingress_if;
        egress_if_bak[0] = egress_if[0];
        egress_if_bak[1] = egress_if[1];
    }

    ret_val = ti_ppd_create_session (&ses_cfg, &ingress_if, &egress_if[0]);
    printk ("ppdt_add_session: Return code for Session creation = %d.\n", ret_val);
    return 0;
}

int ppdt_modify_session (void)
{
    Int32 ret_val;

    ses_cfg_bak.ingress.l3l4_packet.u.ipv4_desc.src_port    = 4661;
    ses_cfg_bak.egress[0].l3l4_packet.u.ipv4_desc.src_ip    = 0xFFFFFFFF;
    ret_val = ti_ppd_modify_session (&ses_cfg_bak, &ingress_if_bak, &egress_if_bak[0]);
    printk ("ppdt_add_session: Return code for Session modification = %d.\n", ret_val);
    return 0;
}

static void print_vpid_stats (TI_PP_VPID_STATS vpid_stats)
{
    printk ("rx_unicast_pkt = %u\n", vpid_stats.rx_unicast_pkt);
    printk ("rx_broadcast_pkt = %u\n", vpid_stats.rx_broadcast_pkt);
    printk ("rx_multicast_pkt = %u\n", vpid_stats.rx_multicast_pkt);
    printk ("rx_byte_hi = %u\n", vpid_stats.rx_byte_hi);
    printk ("rx_byte_lo = %u\n", vpid_stats.rx_byte_lo);
    printk ("rx_discard = %u\n", vpid_stats.rx_discard);
    printk ("tx_unicast_pkt = %u\n", vpid_stats.tx_unicast_pkt);
    printk ("tx_broadcast_pkt = %u\n", vpid_stats.tx_broadcast_pkt);
    printk ("tx_multicast_pkt = %u\n", vpid_stats.tx_multicast_pkt);
    printk ("tx_byte_hi = %u\n", vpid_stats.tx_byte_hi);
    printk ("tx_byte_lo = %u\n", vpid_stats.tx_byte_lo);
    printk ("tx_error = %u\n", vpid_stats.tx_error);
    printk ("tx_discard = %u\n", vpid_stats.tx_discard);
}

static TI_PP_GLOBAL_STATS stats;
static TI_PP_VPID_STATS vpid_stats;
static TI_PP_SESSION_STATS ses_stats;

static int _read_entry(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int i;
    i = ti_ppd_health_check ();
    if ( i >= -1) {
        printk ("PP status: ");
        if (i == 0) printk ("OK!\n");
        else if (i == 1) printk ("CPDSP in error\n");
        else if (i == 2) printk ("MPDSP in error\n");
        else if (i == 3) printk ("CPDSP & MPDSP in error\n");
        else if (i == -1) printk ("PP System Fail\n");
        else printk ("Unknown error from ti_ppd_health_check (%d)\n", i);
    } else
        printk ("ti_ppd_health_check failed\n");

    printk ("Prefetcher Stats:\n");
    printk ("Group A: Packets with pre-fetched buffers = %#x\n", *(volatile UINT32*)0xd3167C24);
    printk ("Group A: Packets with pre-fetched dsc and buffers = %#x\n", *(volatile UINT32*)0xd3167C28);
    printk ("Group A: Packets pushback = %#x\n", *(volatile UINT32*)0xd3167C2C);
    printk ("Group B: Packets with pre-fetched buffers = %#x\n", *(volatile UINT32*)0xd3167C34);
    printk ("Group B: Packets with pre-fetched dsc and buffers = %#x\n", *(volatile UINT32*)0xd3167C38);
    printk ("Group B: Packets pushback = %#x\n", *(volatile UINT32*)0xd3167C3C);
    printk ("Input Queue 0: Packet Congestion Discards = %#x\n", *(volatile UINT32*)0xd3167C40);
    printk ("Input Queue 1: Packet Congestion Discards = %#x\n", *(volatile UINT32*)0xd3167C48);
    printk ("Input Queue 2: Packet Congestion Discards = %#x\n", *(volatile UINT32*)0xd3167C50);
    printk ("Input Queue 3: Packet Congestion Discards = %#x\n", *(volatile UINT32*)0xd3167C58);
    printk ("Input Queue 4: Packet Congestion Discards = %#x\n", *(volatile UINT32*)0xd3167C60);
    printk ("Input Queue 5: Packet Congestion Discards = %#x\n", *(volatile UINT32*)0xd3167C68);

    if (!ti_ppd_get_srl_pkt_stats(&stats) == 1)
    {
        printk ("\nSR Stats:\n");
        printk ("in_pkts = %u\n", stats.packets_rxed);
        printk ("searched_pkts = %u\n", stats.packets_searched);
        printk ("search_matches = %u\n", stats.search_matched);
        printk ("in_sync_pkts = %u\n", stats.sync_delay);
        printk ("fwd_pkts = %u\n", stats.packets_fwd);
        printk ("ip_fwd_pkts = %u\n", stats.ipv4_packets_fwd);
        printk ("desc_starved = %u\n", stats.desc_starved);
        printk ("buffer_starved = %u\n", stats.buffer_starved);
    }
    else
        printk ("Error getting global stats.\n");

    if (!ti_ppd_get_n_clear_vpid_stats (vpid_eth[0].vpid_handle, &vpid_stats))
    {
        printk ("\nETH VPID Stats:\n");
        print_vpid_stats (vpid_stats);
    }
    else
        printk ("Error getting ETH vpid stats.\n");

    if (!ti_ppd_get_n_clear_vpid_stats (vpid_vlan.vpid_handle, &vpid_stats))
    {
        printk ("\nVLAN VPID Stats:\n");
        print_vpid_stats (vpid_stats);
    }
    else
        printk ("Error getting VLAN vpid stats.\n");

    if (!ti_ppd_get_n_clear_vpid_stats (vpid_pppoe.vpid_handle, &vpid_stats))
    {
        printk ("\nPPPoE VPID Stats:\n");
        print_vpid_stats (vpid_stats);
    }
    else
        printk ("Error getting VLAN PPPoE vpid stats.\n");

    if (!ti_ppd_get_n_clear_vpid_stats (vpid_vlan_pppoe.vpid_handle, &vpid_stats))
    {
        printk ("\nVLAN PPPoE VPID Stats:\n");
        print_vpid_stats (vpid_stats);
    }
    else
        printk ("Error getting VLAN PPPoE vpid stats.\n");

    if (!ti_ppd_get_session_pkt_stats (0, &ses_stats))
    {
        printk ("\nSession Stats:\n");
        printk ("alive_idle_time = %u\n", ses_stats.alive_idle_time);
        printk ("packets_forwarded = %u\n", ses_stats.packets_forwarded);
        printk ("bytes_forwarded_hi = %u\n", ses_stats.bytes_forwarded_hi);
        printk ("bytes_forwarded_lo = %u\n", ses_stats.bytes_forwarded_lo);
    }
    else
        printk ("Error getting session 0 stats.\n");


    for (i = 10; i < 23; i++)
    {
        if (!ti_ppd_get_session_pkt_stats (i, &ses_stats))
        {
            printk ("\nSession %d Stats:\n", i);
            printk ("alive_idle_time = %u\n", ses_stats.alive_idle_time);
            printk ("packets_forwarded = %u\n", ses_stats.packets_forwarded);
            printk ("bytes_forwarded_hi = %u\n", ses_stats.bytes_forwarded_hi);
            printk ("bytes_forwarded_lo = %u\n", ses_stats.bytes_forwarded_lo);
        }
        else
            printk ("Error getting session %d stats.\n", i);
    }

#define Cppi41QStatus(qnum)   *(volatile Uint32 *)(0xd3020000+(qnum)*16+0x00)
    printk ("\n\n%s(%d): Num entries in Q 144 = %d\n", __FUNCTION__, __LINE__, Cppi41QStatus(144));
    printk ("%s(%d): Num entries in Q 128 = %d\n", __FUNCTION__, __LINE__, Cppi41QStatus(128));

    *eof = 1; return 0;
}

static inline int make_ses_id_from_buff (unsigned long count, const char *buf)
{
    return (((count > 1) ? ((buf[0] - '0')*10) : 0)
            + ((count > 0) ? ((count > 1) ? (buf[1] - '0') : (buf[0] - '0'))
                            : -1));
}

static int _write_entry(struct file *fp, const char *buf, unsigned long count, void *data)
{
    printk ("_write_entry: count = %lu\n", count);
    printk ("DATA: %c %c\n", buf[0], buf[1]);

    if (buf[0] == 'c') ppdt_add_session (make_ses_id_from_buff(count-2, &buf[1]));
    if (buf[0] == 'd') ti_ppd_delete_session (make_ses_id_from_buff(count-2, &buf[1]));
    if (buf[0] == 'm') ppdt_modify_session ();

#if 0
    if ((count > 2) && (buf[0] == 's')) {
        if(ti_ppd_set_session_state (buf[1] - '0', buf[2] - '0'))
            printk ("State change for session %d failed\n", (buf[1] - '0'));
        else
            printk ("Session  %d state changed to %s\n",
                    (buf[1] - '0'),
                    (((buf[2] - '0') == TI_PP_SESSION_STATE_FORWARD)
                        ? "FORWARD" : "DIVERSION"));
    }
#endif

    if((count != 3) && (buf[0] == 'a')) {
        printk ("0: Enable VPID 31\n"
                "1: Disable VPID 31\n"
                "2: Enable PID 31\n"
                "3: Discard all pkts on PID 31\n"
                "4: Fwd all Rx on PID 31\n"
                "5: Fwd all Rx on VPID 31\n"
                "6: Delete PID 31\n"
                "7: Delete VPID 31\n"
                "8: Create PID 31\n"
                "9: Create VPID 31\n");
    }

    if (buf[0] == 'a') {
        Uint32 new_flags;
        switch (buf[1] - '0') {
            case 0:
                new_flags = 0;
                ti_ppd_set_vpid_flags (&vpid_eth[0], new_flags);
                break;

            case 1:
                new_flags = TI_PP_VPID_FLG_TX_DISBL | TI_PP_VPID_FLG_RX_DISBL;
                ti_ppd_set_vpid_flags (&vpid_eth[0], new_flags);
                break;

            case 2:
                ti_ppd_set_pid_flags (&pid_eth[0], 0);
                break;

            case 3:
                ti_ppd_set_pid_flags (&pid_eth[0], (1 << 6)/*TI_PP_PID_FLG_RX_DISBL*/);
                break;

            case 4:
                ti_ppd_set_pid_flags (&pid_eth[0], (1<<3)/*TI_PP_PID_FLG_RX_FWD_DFLT_PID*/);
                break;

            case 5:
                new_flags = TI_PP_VPID_FLG_RX_DFLT_FWD;
                ti_ppd_set_vpid_flags (&vpid_eth[0], new_flags);
                break;

            case 6:
                ti_ppd_delete_pid (pid_eth[0].pid_handle);
                break;

            case 7:
                ti_ppd_delete_vpid (vpid_eth[0].vpid_handle);
                break;

            case 8:
                ti_ppd_create_pid (&pid_eth[0]);
                break;

            case 9:
                ti_ppd_create_vpid (&vpid_eth[0]);
                break;

            default:
                ;
        }
    }

    {
#define TI_PP_NUM_FW    4
        extern int avalanche_ppd_init(void);
        extern int avalanche_ppd_deinit(void);

        if (buf[0] == 's') {
#if 0
            if(ti_ppd_init(TI_PP_NUM_FW, &ppFirmware_g[0], &ppCfg_g) == 0) {
                printk("ppdt: PPD initialized successfully.\n");
            } else {
                printk("ppdt: ERROR: PPD failed to initialize!\n");
            }
#endif
            avalanche_ppd_init ();
        }

        if (buf[0] == 'x') avalanche_ppd_deinit();
    }

    return count;
}

static int _write_info(struct file *fp, const char *buf, unsigned long count, void *data)
{
    //printk ("_write_entry: count = %lu\n", count);
    //printk ("DATA: %c %c\n", buf[0], buf[1]);

    *(Uint8*)data = make_ses_id_from_buff(count-1, buf);
    //printk ("Setting Session to dump = %d\n", *(Uint32*)data);

    return count;
}

static int _read_info(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    return (ti_ppd_get_session_dump (buf, start, offset, count, eof, data));
}

int avalanche_ppd_proc_init(void)
{
    static Uint8 ses_id;
    static struct proc_dir_entry *gp_stats_file = NULL, *gp_info_file;
    gp_stats_file =
            create_proc_entry("avalanche/ppdt", 0644, NULL);
    if (gp_stats_file) {
        gp_stats_file->read_proc = _read_entry;
        gp_stats_file->write_proc = _write_entry;
    }

    gp_info_file =
        create_proc_entry("avalanche/ppdt_info", 0644, NULL);
    if (gp_info_file) {
        gp_info_file->read_proc     = _read_info;
        gp_info_file->write_proc    = _write_info;
        gp_info_file->data          = &ses_id;
    }

    return 0;
}

fs_initcall(ppdt_init);

