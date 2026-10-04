/*
 * ppd_pvt.h
 *
 * Packet processor driver private header file.
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

/* Version 0.3 */

#ifndef _INCLUDE_PPD_PVT_H
#define _INCLUDE_PPD_PVT_H

#define PPD_DEBUG_ERR   (1<<0)
#define PPD_DEBUG_MSG   (1<<1)
#define PPD_DEBUG_CMD   (1<<2)
#define PPD_DEBUG_EVN   (1<<3)

#define PPD_DEBUG (PPD_DEBUG_ERR)

/* LUT L2-ETH Enable Flags */
#define TI_PP_L2_LUT_DST_MAC_EN    (0x5 << 0)
#define TI_PP_L2_LUT_SRC_MAC_EN    (0x5 << 1)
#define TI_PP_L2_LUT_ETH_TYPE_EN   (0x1 << 4)
#define TI_PP_L2_LUT_VLAN_ID_EN    (0x1 << 5)
#define TI_PP_L2_LUT_PKT_KEY_EN    (0x1 << 6)
#define TI_PP_L2_LUT_IN_PID_EN     (0x1 << 7)
#define TI_PP_L2_LUT_VLAN_PRI_EN   (0x1 << 8)
#define TI_PP_L2_LUT_DOCSIS_ID_EN  (0x1 << 9)

/* LUT L3-IPv4 Enable Flags */
#define TI_PP_L3_LUT_DST_IPV4_EN          (0x1 << 0)
#define TI_PP_L3_LUT_SRC_IPV4_EN          (0x1 << 1)
#define TI_PP_L3_LUT_TCP_UDP_DST_PORT_EN  (0x1 << 2)
#define TI_PP_L3_LUT_TCP_UDP_SRC_PORT_EN  (0x1 << 3)
#define TI_PP_L3_LUT_PPPOE_SES_ID_EN      	(0x1 << 4)
#define TI_PP_L3_LUT_PKT_KEY_EN    		(0x1 << 6)
#define TI_PP_L3_LUT_IPV4_FRAG_FLAG_EN    (0x1 << 7)
#define TI_PP_L3_LUT_IPV4_PROTO_EN        (0x1 << 8)
#define TI_PP_L3_LUT_IPV4_TOS_EN          (0x1 << 9)

#define TI_PP_L3_LUT_DST_IPV6_HASH_EN     (0x1 << 0)
#define TI_PP_L3_LUT_SRC_IPV6_HASH_EN     (0x1 << 1)
#define TI_PP_L3_LUT_IPV6_FLOW_LBL_EN     (0x5 << 5)
#define TI_PP_L3_LUT_IPV6_NEXT_HDR_EN     (0x1 << 8)
#define TI_PP_L3_LUT_IPV6_T_CLASS_EN      (0x1 << 9)

/* LUT Entry Type */
#define TI_PP_LUT_DATA_L2_ETH               (0x00)
#define TI_PP_LUT_DATA_L2_UNDEF             (0x07)
#define TI_PP_LUT_DATA_L3_IPV4              (0x08)
#define TI_PP_LUT_DATA_L3_IPV6              (0x09)
#define TI_PP_LUT_DATA_L3_UNDEF             (0x0F)


/* Valid flags in Modification Record */
#define TI_PP_MOD_IPSRC_VALID		(1<<0)
#define TI_PP_MOD_IPDST_VALID		(1<<1)
#define TI_PP_MOD_IPADR_VALID		(1<<2)
#define TI_PP_MOD_L3CHK_VALID		(1<<3)
#define TI_PP_MOD_SRCPORT_VALID		(1<<4)
#define TI_PP_MOD_DSTPORT_VALID		(1<<5)
#define TI_PP_MOD_PORTS_VALID		(1<<6)
#define TI_PP_MOD_L4CHK_VALID		(1<<7)
#define TI_PP_MOD_IPTOS_VALID		(1<<8)

/* Egress Framing Record */
#define TI_PP_EGR_FRM_STRIP_L2		(1<<0)
#define TI_PP_EGR_FRM_PATCH_802_3	(1<<1)
#define TI_PP_EGR_FRM_TURBODOX_EN       (1<<2)
#define TI_PP_EGR_FRM_PPPOE_HDR         (1<<4)
#define TI_PP_EGR_FRM_REFRAME_IP	(1<<5)
#define TI_PP_EGR_FRM_TURBODOX_ADV_EN   (1<<7)


typedef Uint8 TI_PP_ses_id_t;

/* Base Session */
#define TI_PP_SES_FLAG_IDLE_TMOUT           (1 <<  0)
#define TI_PP_SES_FLAG_PASS_AFTER_TMOUT     (1 <<  1)
#define TI_PP_SES_FLAG_NO_INGRESS_STATS     (1 <<  2)
#define TI_PP_SES_FLAG_NO_EGRESS_STATS      (1 <<  3)
#define TI_PP_SES_FLAG_XLUDE_ETH_HDR_STATS  (1 <<  4)
#define TI_PP_SES_FLAG_UPDATE_TTL           (1 <<  5)
#define TI_PP_SES_FLAG_PROC_TTL_EXP         (1 <<  6)
#define TI_PP_SES_FLAG_IP_FRAG              (1 <<  7)
#define TI_PP_SES_FLAG_PROC_IP_OPTS         (1 <<  8)
#define TI_PP_SES_FLAG_TCP_CONTROL          (1 <<  9)
#define TI_PP_SES_FLAG_USE_FULL_SRC_IPV6    (1 <<  10)
#define TI_PP_SES_FLAG_USE_FULL_DST_IPV6    (1 <<  11)

/* Session Command Bits */
#define TI_PP_SES_CBIT_TABLEADD         7
#define TI_PP_SES_CBIT_TABLEREMOVE      6
#define TI_PP_SES_CBIT_SESSIONPAUSE     5
#define TI_PP_SES_CBIT_SESSIONRESUME    4
#define TI_PP_SES_CBIT_STATECHANGE      3

/* Session start state */
#define TI_PP_SES_STATE_IDLE            (0x00)
#define TI_PP_SES_STATE_NEEDSYNC        (0x01)
#define TI_PP_SES_STATE_QUEUE           (0x02)
#define TI_PP_SES_STATE_FWD             (0x03)
#define TI_PP_SES_STATE_DIVERT          (0x04)

/*
 * Session Commands
 */
#define SESSION_ADD_SYNC                ((1<<TI_PP_SES_CBIT_TABLEADD) \
                                            | (1<<TI_PP_SES_CBIT_STATECHANGE) \
                                            | TI_PP_SES_STATE_NEEDSYNC)
#define SESSION_ADD_FORWARD             ((1<<TI_PP_SES_CBIT_TABLEADD) \
                                            | (1<<TI_PP_SES_CBIT_STATECHANGE) \
                                            | TI_PP_SES_STATE_FWD)
#define SESSION_REMOVE                  ((1<<TI_PP_SES_CBIT_TABLEREMOVE) \
                                            | (1<<TI_PP_SES_CBIT_STATECHANGE) \
                                            | TI_PP_SES_STATE_IDLE)
#define SESSION_LUT_UPDATE              (1<<TI_PP_SES_CBIT_TABLEADD)

#define SESSION_STATE_SYNC              ((1<<TI_PP_SES_CBIT_STATECHANGE) \
                                            | TI_PP_SES_STATE_NEEDSYNC)
#define SESSION_STATE_FORWARD           ((1<<TI_PP_SES_CBIT_STATECHANGE) \
                                            | TI_PP_SES_STATE_FWD)
#define SESSION_STATE_DIVERT            ((1<<TI_PP_SES_CBIT_STATECHANGE) \
                                            | TI_PP_SES_STATE_DIVERT)


#define PP_HOST_EVENT_FDQMGR            0
#define PP_HOST_EVENT_FDQ               117
#define EVENT_POLLTIME_MSECS            100


typedef Uint8 TI_PP_pid_t;
typedef Uint8 TI_PP_vpid_t;

#endif /* !_INCLUDE_PPD_PVT_H */
