/*
 * ti_ppd.c
 *
 * Packet processor driver file.
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

#include <pal_cppi41.h> /* Needed for header pointer ops like cache, phys */

#include <ti_ppd.h>
#include "ppd_pvt.h"
#include "ppd_os.h"

/* Flags used for validity checks in PPD */
static Uint32 g_ppd_init_done   = 0;
static Uint32 g_session_slots [TI_PP_MAX_ACCLERABLE_SESSIONS/32];
#define IS_SESSION_SLOT_FILLED(ses_id) \
            (g_session_slots[(Uint32)(ses_id)/32] & (1<<((Uint32)(ses_id)%32)))

/* Minimum buffer to be supplied to hold dump data */
#define PPD_DUMP_BUFF_BLK_LEN       1000

#define WSTR_W0(idx, w, w0_nm) \
            "w%02d: 0x%08x {%8s : 0x%08x}\n", \
            idx, w, w0_nm, ((w) >> 0) & 0xffffffff

#define WSTR_S1_0(idx, w, s1_nm, s0_nm) \
            "w%02d: 0x%08x {%8s : 0x%04x, %8s : 0x%04x}\n", \
            idx, w, s1_nm, ((w) >> 16) & 0xffff, s0_nm, ((w) >> 0) & 0xffff

#define WSTR_B3_0(idx, w, b3_nm, b2_nm, b1_nm, b0_nm) \
            "w%02d: 0x%08x {%8s :   0x%02x, %8s :   0x%02x, %8s :   0x%02x, \
%8s :   0x%02x}\n",\
            idx, w, b3_nm, ((w) >> 24) & 0xff, b2_nm, ((w) >> 16) & 0xff, \
            b1_nm, ((w) >> 8) & 0xff, b0_nm, ((w) >> 0) & 0xff

#define WSTR_S0_B1_0(idx, w, s0_nm, b1_nm, b0_nm) \
            "w%02d: 0x%08x {%8s : 0x%04x, %8s :   0x%02x, %8s :   0x%02x}\n", \
            idx, w, s0_nm, ((w) >> 16) & 0xffff, \
            b1_nm, ((w) >> 8) & 0xff, b0_nm, ((w) >> 0) & 0xff

#define WSTR_B1_0_S0(idx, w, b1_nm, b0_nm, s0_nm) \
            "w%02d: 0x%08x {%8s :   0x%02x, %8s :   0x%02x, %8s : 0x%04x}\n", \
            idx, w, b1_nm, ((w) >> 24) & 0xff, \
            b0_nm, ((w) >> 16) & 0xff, s0_nm, ((w) >> 0) & 0xffff

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
static char dump_buff[1000];
static Int32 ppd_dump_sr_open (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p, WSTR_S1_0(0, *ptr, "Dflt Tag", "Dflt RxQ"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(1, *(ptr+1), "Event QM", "Event Q"));
    len += sprintf(dump_buff_p + len, WSTR_W0(2, *(ptr+2), "SYNC Maxpkt"));
    len += sprintf(dump_buff_p + len, WSTR_W0(3, *(ptr+3), "SYNC Tmo 10us"));
    return len;
}

static Int32 ppd_dump_pid_install (Uint32* ptr, Uint8* dump_buff_p,
                                        Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p,
                    WSTR_B3_0(0, *ptr, "Port", "Type", "Num PID", "Base PID"));
    return len;
}

static Int32 ppd_dump_pid_add (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p,
            WSTR_B3_0(0, *ptr, "Pri Map", "Pri Drop", "Framing", "Flags"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(1, *(ptr+1), "Dst Tag", "Fwd Q"));
    return len;
}

static Int32 ppd_dump_vpid_add (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p,
                    WSTR_B3_0(0, *ptr, "byte3", "byte2", "byte1", "Flags"));
    return len;
}

static Int32 ppd_dump_eth_lut (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p, WSTR_W0(0, *ptr, "Dst LS"));
    len += sprintf(dump_buff_p + len, WSTR_W0(1, *(ptr+1), "Src LS"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(2, *(ptr+2), "Src MS", "Dst MS"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(3, *(ptr+3), "VLAN VID", "Eth Type"));
    len += sprintf(dump_buff_p + len,
                    WSTR_B3_0(4, *(ptr+4),
                                "DSID/TAG", "VLAN Pri", "In PID", "Pkt Key"));
    len += sprintf(dump_buff_p + len, WSTR_W0(5, *(ptr+5), "Enables"));
    return len;
}

static Int32 ppd_dump_ipv4_lut (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p, WSTR_W0(0, *ptr, "Dst IP"));
    len += sprintf(dump_buff_p + len, WSTR_W0(1, *(ptr+1), "Src IP"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(2, *(ptr+2), "Src Port", "Dst Port"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(3, *(ptr+3), "short1", "PPPoE Id"));
    len += sprintf(dump_buff_p + len,
            WSTR_B3_0(4, *(ptr+4), "TOS", "Protocol", "Frag", "Pkt Key"));
    len += sprintf(dump_buff_p + len, WSTR_W0(5, *(ptr+5), "Enables"));
    return len;
}

static Int32 ppd_dump_l2_hdr (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 cnt)
{
    Int32 len = 0, i;

    for (i = 0; i < l2_hdr_size; i=+4)
        len = sprintf(dump_buff_p + len, "%#x: %#x\n", &ptr[i/4], ptr[i/4]);

    return len;
}
#endif /* ((PPD_DEBUG) & PPD_DEBUG_MSG) */

static Int32 ppd_dump_ses_rec (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p,
                    WSTR_B1_0_S0(0, *ptr, "In VPID", "Status", "Flags"));
    len += sprintf(dump_buff_p + len, WSTR_W0(1, *(ptr+1), "IPv6 Ptr"));
    len += sprintf(dump_buff_p + len,
            WSTR_S0_B1_0(2, *(ptr+2), "Reserved", "Next Col", "Prev Col"));
    len += sprintf(dump_buff_p + len, WSTR_W0(3, *(ptr+3), "EFR Ptr"));
    len += sprintf(dump_buff_p + len, WSTR_W0(4, *(ptr+4), "Timeout"));
    len += sprintf(dump_buff_p + len, WSTR_W0(5, *(ptr+5), "Ref Time"));
    return len;
}

static Int32 ppd_dump_ef_rec (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p, WSTR_W0(0, *ptr, "Next EFR"));
    len += sprintf(dump_buff_p + len, WSTR_B3_0(1, *(ptr+1),
                                "Priority", "Out VPID", "Framing", "L2 Size"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(2, *(ptr+2), "Dst Tag", "Fwd Q"));
    len += sprintf(dump_buff_p + len,
            WSTR_S0_B1_0(3, *(ptr+3), "Reserved", "Egr Dev", "8023 Off"));
    len += sprintf(dump_buff_p + len, WSTR_W0(4, *(ptr+4), "TDox Ack"));
    len += sprintf(dump_buff_p + len, WSTR_W0(5, *(ptr+5), "L2 Ptr"));
    len += sprintf(dump_buff_p + len, WSTR_W0(6, *(ptr+6), "Mod Ptr"));
    len += sprintf(dump_buff_p + len, WSTR_W0(7, *(ptr+7), "ProtSpec"));
    return len;
}

static Int32 ppd_dump_mod_rec (Uint32* ptr, Uint8* dump_buff_p,
                                    Int32 buff_len)
{
    Int32 len;
    len = sprintf(dump_buff_p, WSTR_B1_0_S0(0, *ptr, "byte1", "TOS", "Flags"));
    len += sprintf(dump_buff_p + len, WSTR_W0(1, *(ptr+1), "IP Src"));
    len += sprintf(dump_buff_p + len, WSTR_W0(2, *(ptr+2), "IP Dst"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(3, *(ptr+3), "Src Port", "Dst Port"));
    len += sprintf(dump_buff_p + len,
                    WSTR_S1_0(4, *(ptr+4), "L3 Chk", "L2 Chk"));
    return len;
}

/** PPD register access and general defines **/

#define CPDSP                       0
#define MPDSP                       1
#define QPDSP                       2
#define APDSP                       3

typedef volatile struct
{
    Uint32    control;             /* 0x00 */
    Uint32    status;              /* 0x04 */
    Uint32    wakeup_en;           /* 0x08 */
    Uint32    cycle_count;         /* 0x0C */
    Uint32    stall_count;         /* 0x10 */
    Uint32    pad0[3];
    Uint32    table_block_index0;  /* 0x20 */
    Uint32    table_block_index1;  /* 0x24 */
    Uint32    table_program_ptr0;  /* 0x28 */
    Uint32    table_program_ptr1;  /* 0x2C */

} pdsp_regs_t;

#define PDSP_CTL_STATE_BIT          (1 << 15)
#define PDSP_CTL_BIG_EN_BIT         (1 << 14)
#define PDSP_CTL_EN_BIT             (1 <<  1)
#define PDSP_CTL_N_RST_BIT          (1 <<  0)

/* PDSP commands */
#define SRPDSP_SR_OPEN              0x80
#define SRPDSP_SR_CLOSE             0x81
#define SRPDSP_SR_STATS_GET         0x82
#define SRPDSP_SR_STATS_CLEAR       0x83
    #define STATS_TYPE_SRGLOBAL 0x01
    #define STATS_TYPE_VPID     0x02
    #define STATS_TYPE_SESSION  0x03
#define SRPDSP_PID_COMMAND          0x84
    #define PID_ADD             0x01
    #define PID_REMOVE          0x02
    #define PID_CHANGE_FLAGS    0x03
#define SRPDSP_VPID_COMMAND         0x85
    #define VPID_ADD            0x01
    #define VPID_REMOVE         0x02
    #define VPID_CHANGE_FLAGS   0x03
#define SRPDSP_SESSION_COMMAND      0x86
#define SRPDSP_SR_STATUS            0x87

/* Success Code */
#define SR_RETCODE_SUCCESS          1

/* PDSP error codes */
#define SRPDSP_ENORES                   -1
#define SRPDSP_EINVCMD                  -2
#define SRPDSP_EINVOPT                  -3
#define SRPDSP_EINVINDEX                -4
#define SRPDSP_EALREADYOPEN             -5
#define SRPDSP_ENOTOPEN                 -6
#define SRPDSP_EMAPERROR                -7
#define SRPDSP_EINVPORT                 -8
#define SRPDSP_EINVPID                  -9
#define SRPDSP_EPAUSELIMITEXCEED        -10
#define SRPDSP_ESESSIONNOTPAUSED        -11
#define SRPDSP_ESESSIONPAUSED           -12
#define SRPDSP_EREOPENINVALID           -13
#define SRPDSP_EINTERROR                -99

typedef struct
{
    Uint32 w0_next_egr_ptr;

    union
    {
        Uint8  l2_hdr_size;
        Uint8  frame_code;
        Uint8  egress_vpid;
        Uint8  priority;
        Uint32 frags;

    } w1;

    union
    {
        Uint16 fwd_q_index;    /* Fowarding queue manager and index */
        Uint16 fwd_dst_tag;    /* Destination tag for default forwarding */
        Uint32 frags;

    } w2;

    union
    {
        Uint8  offset_802_3_len;
        Uint8  egress_dev;
        Uint16 reserved;
        Uint32 frags;

    } w3;

    Uint32 w4_turbo_tcp_ack_num;
    Uint32 w5_l2_hdr_ptr;
    Uint32 w6_mod_rec_ptr;
    Uint32 w7_proto_spec;

} ppd_egress_rec_t;

#define AV_SR_NWSS_BASE_VIRT   (0xd0000000)
#define AV_SR_NWSS_BASE_PHYS   (0x00000000)
#define BASE_ADDR              (Uint32)gp_ses_blk /* AV_SR_NWSS_BASE_VIRT */

#define PPD_OFFSET_VIRT(o)     (AV_SR_NWSS_BASE_VIRT + (Uint32)(o))
#define PPD_OFFSET_PHYS(o)     (AV_SR_NWSS_BASE_PHYS + o)
#define PPD_PHYS_2_VIRT(p)     (PPD_OFFSET_VIRT((Uint32)(p) - AV_SR_NWSS_BASE_PHYS))
#define PPD_VIRT_2_PHYS(v)     (PPD_OFFSET_PHYS(v - AV_SR_NWSS_BASE_VIRT))

#define PPD_VIRT(p)             ((Uint32*)PPD_PHYS_2_VIRT((Uint32)(p)))
#define PPD_PHYS(v)             ((Uint32)(v) - 0xd0000000)
                                //((Uint32*)PPD_VIRT_2_PHYS(v))

typedef Uint32 pdsp_cmd_t;
typedef pdsp_cmd_t cpdsp_cmd_t;
typedef pdsp_cmd_t mpdsp_cmd_t;
typedef pdsp_cmd_t qpdsp_cmd_t;

typedef volatile struct pdsp_info pdsp_info_t;

typedef Int32 (*pdsp_trg_cmd_func_t)(pdsp_info_t*, pdsp_cmd_t);
typedef Int32 (*pdsp_rsp_sts_func_t)(pdsp_info_t*);

/* volatile */ struct pdsp_info
{
    Uint32              id;
    Uint32              *iram;
    Uint32              *sram;         /* scratch RAM */
    Uint32              *regs;
    Uint32              *cmd_rsp_ram;
    Uint32              *param_ram;
    Uint32              wait_count;

    pdsp_trg_cmd_func_t trg_cmd;
    pdsp_rsp_sts_func_t rsp_sts;

};

static inline Int32 pdsp_trigger_cmd(pdsp_info_t *pdsp, pdsp_cmd_t cmd)
{
    *pdsp->cmd_rsp_ram = cmd;
    return 0;
}

static inline Int32 pdsp_rsp_status(pdsp_info_t *pdsp)
{
    Uint32 wait_count = pdsp->wait_count;

    while((*pdsp->cmd_rsp_ram & 0xff) && wait_count)
        wait_count--;

    if(!wait_count) {
        printErr ("pdsp_rsp_status: Timeout waiting for PDSDP(%d).\n",
                    pdsp->id);
        return (-1);
    }

    return (*pdsp->param_ram);
}

static pdsp_info_t g_pdsp_info[ ] = {

    {   /* BOOTS - CPDSP */
        TI_PP_BOOTS_FW         /*  id  */, PPD_VIRT(0x03080000) /* iram */,
        PPD_VIRT(0x03100000)   /* sram */, PPD_VIRT(0x0300D000) /* regs */,
        PPD_VIRT(0x0300c000)/* cmd_rsp */, PPD_VIRT(0x03100004) /* param ram */,
        10000000         /* wait count */,
        pdsp_trigger_cmd,                    pdsp_rsp_status
    },

    {   /* CPDSP */
        TI_PP_CPDSP_FW          /*  id  */, PPD_VIRT(0x03080000) /* iram */,
        PPD_VIRT(0x030fffff)    /* sram */, PPD_VIRT(0x0300D000) /* regs */,
        PPD_VIRT(0x0300c000) /* cmd_rsp */, PPD_VIRT(0x03100004) /* param ram */,
        10000000          /* wait count */,
        pdsp_trigger_cmd,                    pdsp_rsp_status
    },

    {   /* MPDSP */
        TI_PP_MPDSP_FW          /*  id  */, PPD_VIRT(0x03088000) /* iram */,
        PPD_VIRT(0x03110000)    /* sram */, PPD_VIRT(0x0300D100) /* regs */,
        PPD_VIRT(0x0300C100) /* cmd_rsp */, PPD_VIRT(0x03110004)/* param ram */,
        10000000          /* wait count */,
        pdsp_trigger_cmd,                    pdsp_rsp_status
    },

    {   /* QPDSP */
        TI_PP_QPDSP_FW          /*  id  */, PPD_VIRT(0x03090000) /* iram */,
        PPD_VIRT(0x03120000)    /* sram */, PPD_VIRT(0x0300D200) /* regs */,
        PPD_VIRT(0x0300C200) /* cmd_rsp */, PPD_VIRT(0x03120004) /* param ram */,
        10000000          /* wait count */,
        pdsp_trigger_cmd,                    pdsp_rsp_status
    }
                                  };

#define SIZE_IN_WORD(p) ((sizeof(p) + 0x3) >> 2)

#define WORD_B3_0(b3, b2, b1, b0) (((Uint8)b3 << 24) | ((Uint8)b2 << 16) \
                                    | ((Uint8)b1 << 8) | ((Uint8)b0))
#define WORD_B1_0_S0(b1, b0, s0)  (((Uint8)b1 << 24) | ((Uint8)b0 << 16) \
                                    | ((Uint16)s0))
#define WORD_S0_B1_0(s0, b1, b0)  (((Uint16)s0 << 16) | ((Uint8)b1 <<  8) | ((Uint8)b0))
#define WORD_S1_0(s1, s0)         (((Uint16)s1 << 16) | ((Uint16)s0))
#define WORD_W0(w0)               ((Uint32)w0)

typedef Int32 (*pdsp_param_func_t)(volatile Uint32 *, void *ptr);

static inline Int32 pdsp_cmd_rsp_by_sz(pdsp_info_t *pdsp, pdsp_cmd_t cmd_word,
                                       void *wr_ptr, Uint32 wr_word,
                                       void *rd_ptr, Uint32 rd_word)
{
    volatile Uint32 *param_buf    = pdsp->param_ram;
    volatile Uint32 *cmd_rsp_reg  = pdsp->cmd_rsp_ram;
    unsigned int wait_count       = pdsp->wait_count;
    Uint32* wr_wptr = (Uint32*) wr_ptr;
    Uint32* rd_wptr = (Uint32*) rd_ptr;
    Uint32 cookie, ret_code;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    printCmd("COMMAND: %#x = %#x: ", (Uint32)cmd_rsp_reg, (Uint32)cmd_word);
    while(wr_word--) {
        printCmd ("%#x = %#x ", (Uint32)param_buf, *wr_wptr);
        *param_buf++ = *wr_wptr++;
    }
    printCmd ("\n");
    *cmd_rsp_reg = cmd_word;

    while((*cmd_rsp_reg & 0xff) && wait_count)
        wait_count--;

    if(!wait_count) {
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
        return (-1);
    }

    param_buf    = pdsp->param_ram + 1;
    while(rd_word--)
        *rd_wptr++ = *param_buf++;

    ret_code = *pdsp->param_ram;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);

    return ((ret_code == SR_RETCODE_SUCCESS)
            ? 0
            : -ret_code);
}

static Int32 cpdsp_cmd_rsp_by_sz(cpdsp_cmd_t cmd_word,
                                 void *wr_ptr, Uint32 wr_word,
                                 void *rd_ptr, Uint32 rd_word)
{
    pdsp_info_t *pdsp = g_pdsp_info + TI_PP_CPDSP_FW;
    return (pdsp_cmd_rsp_by_sz(pdsp, cmd_word,
                               wr_ptr, wr_word, rd_ptr, rd_word));
}

static inline Int32 pdsp_cmd_rsp_by_pm(pdsp_info_t *pdsp, pdsp_cmd_t cmd_word,
                                       pdsp_param_func_t wr_func, void *wr_obj,
                                       pdsp_param_func_t rd_func, void *rd_obj)
{
    volatile Uint32 *param_buf    = pdsp->param_ram;
    volatile Uint32 *cmd_rsp_reg  = pdsp->cmd_rsp_ram;
    unsigned int wait_count       = pdsp->wait_count;
    Uint32 cookie, ret_code;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    if(wr_func && wr_obj && wr_func(param_buf, wr_obj))
        goto pdsp_cmd_err;

    *cmd_rsp_reg = cmd_word;

    while((*cmd_rsp_reg & 0xff) && wait_count)
        wait_count--;

    if(!wait_count)
        goto pdsp_cmd_err;

    param_buf    = pdsp->param_ram + 1;;

    if(rd_func &&  rd_obj && rd_func(param_buf, rd_obj))
        goto pdsp_cmd_err;

    ret_code = *pdsp->param_ram;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);

    return ((ret_code == SR_RETCODE_SUCCESS)
            ? 0
            : -(ret_code));

pdsp_cmd_err:
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
    return (-1);
}

static Int32 cpdsp_cmd_rsp_by_pm(cpdsp_cmd_t cmd_word,
                                 pdsp_param_func_t wr_func, void *wr_obj,
                                 pdsp_param_func_t rd_func, void *rd_obj)
{
    pdsp_info_t *pdsp = g_pdsp_info + TI_PP_CPDSP_FW;
    return (pdsp_cmd_rsp_by_pm(pdsp, cmd_word,
                               wr_func, wr_obj, rd_func, rd_obj));
}

static inline Uint32* ppd_memset_word(Uint32 *s, Int32 v, Uint32 n_word)
{
    while(n_word--)
        *s++ = v;

    return (s);
}

static inline Uint32* ppd_memcpy_word(Uint32 *dest, Uint32 *src, Uint32 n_word)
{
    while(n_word--)
        *dest++ = *src++;

    return (dest);
}

Int32 ti_ppd_get_srl_pkt_stats(TI_PP_GLOBAL_STATS *stats)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0x00, STATS_TYPE_SRGLOBAL, 0x82);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, stats, SIZE_IN_WORD(*stats)));
}

Int32 ti_ppd_clear_srl_pkt_stats(void)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0x00, STATS_TYPE_SRGLOBAL,
                                        SRPDSP_SR_STATS_CLEAR);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0));
}

Int32 ti_ppd_get_n_clear_srl_pkt_stats(TI_PP_GLOBAL_STATS *stats)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0x00, STATS_TYPE_SRGLOBAL,
                                        SRPDSP_SR_STATS_CLEAR);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, stats, SIZE_IN_WORD(*stats)));
}

Int32 ti_ppd_get_vpid_stats(TI_PP_vpid_t vpid, TI_PP_VPID_STATS *stats)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(vpid, STATS_TYPE_VPID, 0x82);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, stats, SIZE_IN_WORD(*stats)));
}

Int32 ti_ppd_clear_vpid_stats(Uint8 vpid_handle)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(vpid_handle, STATS_TYPE_VPID,
                                        SRPDSP_SR_STATS_CLEAR);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0));
}

Int32 ti_ppd_get_n_clear_vpid_stats(TI_PP_vpid_t vpid, TI_PP_VPID_STATS *stats)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(vpid, STATS_TYPE_VPID,
                                        SRPDSP_SR_STATS_CLEAR);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, stats, SIZE_IN_WORD(*stats)));
}

Int32 ti_ppd_get_session_pkt_stats(TI_PP_ses_id_t ses_id,
                                    TI_PP_SESSION_STATS *stats)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(ses_id, STATS_TYPE_SESSION,
                                        SRPDSP_SR_STATS_GET);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, stats, SIZE_IN_WORD(*stats)));
}

Int32 ti_ppd_clear_session_pkt_stats(Uint8 session_handle)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(session_handle, STATS_TYPE_SESSION,
                                        SRPDSP_SR_STATS_CLEAR);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0));
}

Int32 ti_ppd_get_n_clear_session_pkt_stats(TI_PP_ses_id_t ses_id,
                                            TI_PP_SESSION_STATS *stats)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(ses_id, STATS_TYPE_SESSION,
                                        SRPDSP_SR_STATS_CLEAR);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, stats, SIZE_IN_WORD(*stats)));
}

typedef struct
{
    union
    {
        Uint8  flags;           /* Flags, Host provided */
        Uint8  in_framing;      /* Allowed ingress framing, Host provided */
        Uint8  dflt_pri_drp;    /* Drop priority, Host provided */
        Uint8  pri_map;         /* Priority mapping */
        Uint32 frags;

    } w0;

    union
    {
        Uint16 dflt_q_index;    /* Default fowarding queue manager and index */
        Uint16 dflt_dst_tag;    /* Destination tag for default forwarding */
        Uint32 frags;

    } w1;

} ppd_pid_info_blk_t;


Int32 ti_ppd_create_pid(TI_PP_PID *params)
{
    TI_PP_pid_t  pid         = params->pid_handle;
    cpdsp_cmd_t  cmd_word    = WORD_S0_B1_0(pid, 0x01, 0x84);
    ppd_pid_info_blk_t pd;

    params->priv_flags = 0;
    pd.w0.frags	= WORD_B3_0(params->pri_mapping, params->dflt_pri_drp,
				params->ingress_framing, params->priv_flags);

    pd.w1.frags	= WORD_S1_0(params->dflt_dst_tag, params->dflt_fwd_q);

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
    printMsg ("Parameter block for PID %d:\n", pid);
    ppd_dump_pid_add ((Uint32 *)&pd, dump_buff, sizeof(dump_buff));
    printMsg("%s\n",dump_buff);
#endif
    return (cpdsp_cmd_rsp_by_sz(cmd_word, &pd, SIZE_IN_WORD(pd), 0, 0));
}

Int32 ti_ppd_set_pid_flags(TI_PP_PID *pid_params, Uint32 new_flags)
{
    TI_PP_pid_t  pid         = pid_params->pid_handle;
    cpdsp_cmd_t  cmd_word    = WORD_S0_B1_0(pid, PID_CHANGE_FLAGS,
                                            SRPDSP_PID_COMMAND);

    pid_params->priv_flags = new_flags;
    printMsg ("New flags for PID(%d): %#x\n", pid, new_flags);
    new_flags = WORD_B3_0(0, 0, 0, pid_params->priv_flags);
    return (cpdsp_cmd_rsp_by_sz(cmd_word, &new_flags, 1, 0, 0));
}

Int32 ti_ppd_delete_pid(Uint8 pid_handle)
{
    cpdsp_cmd_t  cmd_word    = WORD_S0_B1_0(pid_handle, PID_REMOVE,
                                            SRPDSP_PID_COMMAND);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0));
}

typedef struct
{
    union
    {
        Uint8  flags;       /* Configuration flags, Host provided */
        Uint8  pad0[3];
        Uint32 frags;

    } w0;

} ppd_vpid_params_t;


Int32 ti_ppd_create_vpid(TI_PP_VPID *params)
{
    TI_PP_vpid_t        vpid = params->vpid_handle;
    ppd_vpid_params_t	vpid_params;
    cpdsp_cmd_t		cmd_word    = WORD_S0_B1_0(vpid, VPID_ADD,
                                                    SRPDSP_VPID_COMMAND);

    vpid_params.w0.frags = WORD_B3_0(0, 0, 0, 0);
    params->priv_vpid_flags = 0;

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
    printMsg ("Parameter block for VPID %d:\n", vpid);
    ppd_dump_vpid_add ((Uint32 *)&vpid_params, dump_buff, sizeof(dump_buff));
    printMsg("%s\n",dump_buff);
#endif
    return (cpdsp_cmd_rsp_by_sz(cmd_word, &vpid_params,
                                SIZE_IN_WORD(vpid_params), 0, 0));
}

Int32 ti_ppd_set_vpid_flags(TI_PP_VPID *params, Uint32 new_flags)
{
    TI_PP_vpid_t        vpid		= params->vpid_handle;
    cpdsp_cmd_t		cmd_word	= WORD_S0_B1_0(vpid, VPID_CHANGE_FLAGS,
                                                        SRPDSP_VPID_COMMAND);

    params->priv_vpid_flags = new_flags;

    printMsg ("New flags for VPID(%d): %#x\n", vpid, params->priv_vpid_flags);
    new_flags = WORD_B3_0(0, 0, 0, params->priv_vpid_flags);
    return (cpdsp_cmd_rsp_by_sz(cmd_word, &new_flags, 1, 0, 0));
}

Int32 ti_ppd_delete_vpid(Uint8 vpid_handle)
{
    cpdsp_cmd_t	    cmd_word    = WORD_S0_B1_0(vpid_handle, VPID_REMOVE,
                                                SRPDSP_VPID_COMMAND);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0));
}

typedef union block32
{
    union block32 *next;
    Uint8         data[32];

} ppd_block32_t;

static ppd_block32_t *gp_free_blk32_list = 0;

static Int32 ppd_init_blk32(ppd_block32_t *last_blk32_addr)
{
    ppd_block32_t *p_blk32 = gp_free_blk32_list;

    for(; p_blk32  < last_blk32_addr; p_blk32++)
    {
	p_blk32->next = p_blk32 + 1;
//	p_blk32       = p_blk32->next;
    }

    p_blk32->next     = 0;

    return (0);
}

static inline union block32* ppd_get_free_blk32(void)
{
    ppd_block32_t *p_blk32;
    Uint32 cookie;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    p_blk32 = gp_free_blk32_list;

    if(p_blk32)
        gp_free_blk32_list = p_blk32->next;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
    return (p_blk32);
}

static inline void ppd_free_blk32(union block32 *p_blk32)
{
    Uint32 cookie;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    p_blk32->next      = gp_free_blk32_list;
    gp_free_blk32_list = p_blk32;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
}

/* Note that size of ppd_l2_frame has to be aligned to 2^n, meaning that the
 * size of the following structure can not be anything but has to be one of
 * the 2, 4, 8, 16, 32, 64, 128, 256, .......
 *
 * Currently, the size of 64 B is being used.
 *
 * If the structure is not aligned this way, the implemented logic will not
 * work...
 */
typedef struct ppd_l2_frame
{
#define PPD_L2_HBLKSZ_MASK      (sizeof(ppd_l2_frame_t) - 1)
#define PPD_L2_FRM_MAX_HLEN     (52)
#define PPD_L2_FRM_HLEN(p)      ((Uint32)p & PPD_L2_HBLKSZ_MASK)

    union
    {
        Uint8  buf[PPD_L2_FRM_MAX_HLEN];
        struct ppd_l2_frame *next;
    } u;

    Uint8   *data;
    Uint8   *end;
    Uint8   len;
    Uint8   pad[3];

} ppd_l2_frame_t;

#define PPD_HBLK_POOL_SZ    (TI_PP_MAX_EGRESS_PROPERTY \
                                * TI_PP_MAX_ACCLERABLE_SESSIONS)
#define PPD_HBLK_LAST_BLK   ((Uint32)(gp_hblk_pool + PPD_HBLK_POOL_SZ - 1))

static Uint8 hblk_pool [(PPD_HBLK_POOL_SZ+1)*sizeof(ppd_l2_frame_t)];
static ppd_l2_frame_t* gp_hblk_pool = (ppd_l2_frame_t*)hblk_pool;

static Int32 ppd_init_hblk(void)
{
    ppd_l2_frame_t *p_hblk;

    /* Ensure that we start of with aligned memory location. */
    gp_hblk_pool = (ppd_l2_frame_t*)
                    ((Uint32)(gp_hblk_pool + 1) & (~PPD_L2_HBLKSZ_MASK));

    for(p_hblk = gp_hblk_pool;
            (Uint32)p_hblk  < PPD_HBLK_LAST_BLK; p_hblk++)
	    p_hblk->u.next = p_hblk + 1;

    printMsg ("%s: p_hblk = %#x, gp_hblk_pool = %#x\n",
                __FUNCTION__, (Uint32)p_hblk, (Uint32)gp_hblk_pool);

    p_hblk->u.next     = 0;

    return (0);
}

ppd_l2_frame_t* ppd_get_l2_buf(void)
{
    ppd_l2_frame_t *p_hblk;
    Uint32 cookie;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    p_hblk = gp_hblk_pool;

    if (!p_hblk) {
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
        return NULL;
    }

    gp_hblk_pool    = p_hblk->u.next;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
    printMsg ("%s: p_hblk = %#x, gp_hblk_pool = %#x\n",
                __FUNCTION__, (Uint32)p_hblk, (Uint32)gp_hblk_pool);
    p_hblk->data = (Uint8*) p_hblk;
    p_hblk->end  = (Uint8*) p_hblk;
    p_hblk->len  = 0;
    return p_hblk;
}

static Uint8 ppd_l2_buf_get_len (ppd_l2_frame_t *f)
{
    return (f->len);
}

static Int32 ppd_l2_buf_hdr_reserve(ppd_l2_frame_t *f, Uint8 len)
{
    f->data += len;
    f->end  += len;
    return (0);
}

static Int32 ppd_l2_buf_append(ppd_l2_frame_t *f, Uint8 *buf, Uint8 len)
{
    if(len > (PPD_L2_FRM_MAX_HLEN - PPD_L2_FRM_HLEN(f->data) - f->len))
        return (-1);

    printMsg ("%s: p_hblk = %#x\n", __FUNCTION__, (Uint32)f);
    memcpy(f->end, buf, len);
    f->len  += len;
    f->end += len;
    return (0);
}

static Int32 ppd_l2_buf_prepend(ppd_l2_frame_t *f, Uint8 *buf, Uint8 len)
{
    if(len > PPD_L2_FRM_HLEN(f->data))
        return (-1);

    printMsg ("%s: p_hblk = %#x\n", __FUNCTION__, (Uint32)f);
    f->data -= len;
    f->len  += len;
    memcpy(f->data, buf, len);
    return (0);
}

void ppd_l2_buf_free(Uint8 *hdr)
{
    ppd_l2_frame_t *f = (ppd_l2_frame_t *)((Uint32)hdr & ~PPD_L2_HBLKSZ_MASK);
    Uint32 cookie;

    printMsg ("%s: p_hblk = %#x, gp_hblk_pool = %#x\n",
                __FUNCTION__, (Uint32)f, (Uint32)gp_hblk_pool);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    f->u.next = gp_hblk_pool;
    gp_hblk_pool = f;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
}

typedef struct
{
    union
    {
        Uint16 flags;		/* HST */
        Uint8 status;
        Uint8  ingress_vpid;    /* PPM */
        Uint32 frags;

    } w0;

    Uint32 w1_ipv6_rec;		/* HST */

    union
    {
        Uint8  prev_collision;  /* ??? */
        Uint8  next_collision;  /* ??? */
        Uint8  num_egress;      /* NRQ */
        Uint8  num_mod_rec;     /* NRQ */
        Uint32 frags;

    } w2;

    Uint32 w3_egress_ptr;
    Uint32 w4_ses_timeout;      /* HST */
    Uint32 w5_ses_ref_time;     /* NRQ */
    Uint32 w6_fwd_pkts_cnt;     /* NRQ */
    Uint32 w7_fwd_B_hi_cnt;     /* NRQ */
    Uint32 w8_fwd_B_lo_cnt;     /* NRQ */

} ppd_ses_info_blk_t;

typedef struct
{
    union
    {
        Uint16 flags;
        Uint8  tos;
        Uint8  reserved;
        Uint32 frags;

    } w0;

    Uint32  w1_ip_src;
    Uint32  w2_ip_dst;

    union
    {
        Uint16 port_dst;
        Uint16 port_src;
        Uint32 frags;

    } w3;

    union
    {
        Uint16 l4_chksum_delta;
        Uint16 l3_chksum_delta;
        Uint32 frags;

    } w4;

} ppd_pkt_mod_rec_t;

static ppd_ses_info_blk_t *gp_ses_blk = 0;

static inline void ppd_wr_word(volatile Uint32 *ram_ptr, Uint32 *word)
{
    *ram_ptr = *word;
}

static inline void ppd_rd_word(volatile Uint32 *ram_ptr, Uint32 *word)
{
    *word    = *ram_ptr;
}

/*
 * CHK: Returns '0' when invalid/unsupported protocol.
 */
static inline Uint16 ppd_get_eth_type_val (TI_PP_VPID_TYPE  vpid_type,
                                            TI_PP_PACKET_TYPE pkt_type)
{
    if ((vpid_type == TI_PP_ETHERNET) || ((vpid_type == TI_PP_VLAN)))
    {
        if (pkt_type == TI_PP_IPV4_TYPE)
            return 0x0800;
        else if (pkt_type == TI_PP_IPV6_TYPE)
            return 0x86DD;
    }
    else if ((vpid_type == TI_PP_PPPoE) || (vpid_type == TI_PP_VLAN_PPPoE))
        return 0x8864;

    return 0;
}
/*
 * CHK: Returns '0' when invalid/unsupported protocol.
 */
static inline Uint16 ppd_get_pkt_type_val (TI_PP_VPID_TYPE  vpid_type,
                                    TI_PP_PACKET_TYPE pkt_type)
{
    if ((vpid_type == TI_PP_PPPoE) || (vpid_type == TI_PP_VLAN_PPPoE))
    {
        /* Source: http://www.iana.org/assignments/ppp-numbers */
        if (pkt_type == TI_PP_IPV4_TYPE)
            return 0x0021;
        else if (pkt_type == TI_PP_IPV6_TYPE)
            return 0x0057;
    }
    else
    {
        if (pkt_type == TI_PP_IPV4_TYPE)
            return 0x0800;
        else if (pkt_type == TI_PP_IPV6_TYPE)
            return 0x86DD;
    }

    return 0;
}

typedef struct {
	TI_PP_SESSION*	ses_cfg;
	TI_PP_VPID*		ingress_vpid;
} ppd_lut_t;

#define IDX2PID(x)                  ((x)|0x40)
static Int32 ppd_wr_l2_lut(volatile Uint32 *ram_ptr, ppd_lut_t *lut_data)
{
    Int32 ret_val = 0;
    TI_PP_SESSION	*ses_cfg	= lut_data->ses_cfg;
    TI_PP_VPID		*ingress_vpid	= lut_data->ingress_vpid;
    TI_PP_PACKET_DESC	*pkt_desc	= &ses_cfg->ingress.l2_packet;
    TI_PP_PACKET_TYPE   pkt_type   = ses_cfg->ingress.l3l4_packet.packet_type;
    Uint32		enable_flags	    = TI_PP_L2_LUT_PKT_KEY_EN;

    switch(pkt_desc->packet_type)
    {
        case TI_PP_ETH_TYPE:
        {
            TI_PP_ETH_DESC* l2_desc = &pkt_desc->u.eth_desc;
            Uint16 eth_type_val;
            if (l2_desc->enables & TI_PP_SESSION_L2_DSTMAC_VALID)
                enable_flags |= TI_PP_L2_LUT_DST_MAC_EN;
            if (l2_desc->enables & TI_PP_SESSION_L2_SRCMAC_VALID)
                enable_flags |= TI_PP_L2_LUT_SRC_MAC_EN;

            if ((eth_type_val = ppd_get_eth_type_val (ingress_vpid->type,
                                                    pkt_type)))
                enable_flags |= TI_PP_L2_LUT_ETH_TYPE_EN;

            if (!eth_type_val && (pkt_type != TI_PP_NO_TYPE))
            {
                printErr ("%s: Unsupported payload type %d for vpid type %d\n",
                            __FUNCTION__, pkt_type, ingress_vpid->type);
                return -1;
            }

            if ((ingress_vpid->type == TI_PP_VLAN)
                    || (ingress_vpid->type == TI_PP_VLAN_PPPoE))
                enable_flags |= TI_PP_L2_LUT_VLAN_ID_EN;

            /* TODO: Handle TI_PP_L2_LUT_VLAN_PRI_EN */
            enable_flags |= TI_PP_L2_LUT_IN_PID_EN;

            *ram_ptr++ = WORD_B3_0(l2_desc->dstmac[2], l2_desc->dstmac[3],
                    l2_desc->dstmac[4], l2_desc->dstmac[5]);
            *ram_ptr++ = WORD_B3_0(l2_desc->srcmac[2], l2_desc->srcmac[3],
                    l2_desc->srcmac[4], l2_desc->srcmac[5]);
            *ram_ptr++ = WORD_B3_0(l2_desc->srcmac[0], l2_desc->srcmac[1],
                    l2_desc->dstmac[0], l2_desc->dstmac[1]);
            *ram_ptr++ = WORD_S1_0(ingress_vpid->vlan_identifier, eth_type_val);
            /* TODO: Get vlan pri info from somewhere! */
            {
                Uint32 docsis_DSID_Index = -1;

                if (ses_cfg->ingress.app_specific_data.u.app_desc.enables != 0)
                {
                    enable_flags |= TI_PP_L2_LUT_DOCSIS_ID_EN;
                    docsis_DSID_Index = ses_cfg->ingress.app_specific_data.u.app_desc.u.raw_app_info1;
                }

                *ram_ptr++ = WORD_B3_0( docsis_DSID_Index,
                                        0/*TODO:vlan_pri_bits*/,
                                        IDX2PID(ingress_vpid->parent_pid_handle), 
                                        TI_PP_LUT_DATA_L2_ETH );
            }
            *ram_ptr++ = WORD_W0(enable_flags);

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
            printMsg ("Eth LUT Dump:\n");
            ppd_dump_eth_lut((Uint32*)ram_ptr - 6, dump_buff,
                                sizeof(dump_buff));
            printMsg("%s\n",dump_buff);
#endif
        }
        break;

        default:
            printErr ("Non Ethernet 1st level lookup (L2) is NOT supported.\n");
            ret_val = -1;
    }

    return(ret_val);
}

static Int32 ppd_wr_l3_lut(volatile Uint32 *ram_ptr, ppd_lut_t *lut_data)
{
    Int32 ret_val = 0;
    TI_PP_SESSION		*ses_cfg		= lut_data->ses_cfg;
    TI_PP_VPID			*ingress_vpid	= lut_data->ingress_vpid;
    Uint32			enable_flags	= TI_PP_L3_LUT_PKT_KEY_EN;

    switch(ses_cfg->ingress.l3l4_packet.packet_type)
    {
        case TI_PP_IPV4_TYPE:
            {
                TI_PP_IPV4_DESC *dat =
                    &ses_cfg->ingress.l3l4_packet.u.ipv4_desc;
                Uint16 ppp_ses_id = 0xffff;

                if (dat->enables & TI_PP_SESSION_IPV4_DSTIP_VALID)
                    enable_flags |= TI_PP_L3_LUT_DST_IPV4_EN;
                if (dat->enables & TI_PP_SESSION_IPV4_SRCIP_VALID)
                    enable_flags |= TI_PP_L3_LUT_SRC_IPV4_EN;
                if (dat->enables & TI_PP_SESSION_IPV4_DST_PORT_VALID)
                    enable_flags |= TI_PP_L3_LUT_TCP_UDP_DST_PORT_EN;
                if (dat->enables & TI_PP_SESSION_IPV4_SRC_PORT_VALID)
                    enable_flags |= TI_PP_L3_LUT_TCP_UDP_SRC_PORT_EN;
                if (dat->enables & TI_PP_SESSION_IPV4_PROTOCOL_VALID)
                    enable_flags |= TI_PP_L3_LUT_IPV4_PROTO_EN;
                if (dat->enables & TI_PP_SESSION_IPV4_TOS_VALID)
                    enable_flags |= TI_PP_L3_LUT_IPV4_TOS_EN;
                if ((ingress_vpid->type == TI_PP_PPPoE)
                        || (ingress_vpid->type == TI_PP_VLAN_PPPoE))
                {
                    enable_flags |= TI_PP_L3_LUT_PPPOE_SES_ID_EN;
                    ppp_ses_id = ingress_vpid->ppp_session_id;
                }
                /* TODO: TI_PP_L3_LUT_IPV4_FRAG_FLAG_EN */

                *ram_ptr++ = WORD_W0(dat->dst_ip);
                *ram_ptr++ = WORD_W0(dat->src_ip);
                *ram_ptr++ = WORD_S1_0(dat->src_port,
                                        dat->dst_port);
                *ram_ptr++ = WORD_S1_0(0, ppp_ses_id);
                *ram_ptr++ = WORD_B3_0(dat->tos,  dat->protocol,
                                        0/*frag_flag*/, TI_PP_LUT_DATA_L3_IPV4);
                *ram_ptr++ = WORD_W0(enable_flags);

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
                printMsg ("IPv4 LUT Dump:\n");
                ppd_dump_ipv4_lut((Uint32*)ram_ptr - 6, dump_buff, sizeof(dump_buff));
                printMsg("%s\n",dump_buff);
#endif
                break;
            }

        case TI_PP_IPV6_TYPE:
            {
                printErr ("%s: WARNING: IPv6 LUT setup is not supported.\n",
                            __FUNCTION__);
                *(ram_ptr + 10) = WORD_B3_0(0, 0, 0, TI_PP_LUT_DATA_L3_IPV6);
                *ram_ptr++ = WORD_W0(enable_flags);
                break;
            }

        case TI_PP_NO_TYPE:
            {
                *(ram_ptr + 10) = WORD_B3_0(0, 0, 0, TI_PP_LUT_DATA_L3_UNDEF);
                *ram_ptr++ = WORD_W0(enable_flags);
                break;
            }

        default:
            ret_val = -1;
    }

    return(ret_val);
}

static Int32 ppd_wr_lut(volatile Uint32 *ram_ptr, void *in_obj)

{
    Int32 ret_val;
    ppd_lut_t *lut_data = (ppd_lut_t*) in_obj;

    ret_val = ppd_wr_l2_lut(ram_ptr, lut_data);
    ret_val |= ppd_wr_l3_lut(ram_ptr + 6, lut_data); /* start at 25th B */

    return (ret_val);
}

void ppd_cleanup_egress_list (ppd_egress_rec_t *egress)
{
    ppd_egress_rec_t *next_egress;
    while (egress)
    {
        if (egress->w6_mod_rec_ptr)
            ppd_free_blk32((ppd_block32_t*) PPD_VIRT(egress->w6_mod_rec_ptr));

        next_egress = egress->w0_next_egr_ptr
                        ? (ppd_egress_rec_t*) PPD_VIRT(egress->w0_next_egr_ptr)
                        : 0;

        if (egress->w5_l2_hdr_ptr)
            ppd_l2_buf_free ((Uint8*)
                                PAL_osMemPhy2Virt(egress->w5_l2_hdr_ptr));

        ppd_free_blk32 ((ppd_block32_t*)(egress));

        printMsg ("%s: next-egr @%#x\n", __FUNCTION__, (unsigned)next_egress);
        egress = next_egress;
    }
}

Int32 ppd_destroy_session(ppd_ses_info_blk_t *ses)
{
    ppd_egress_rec_t *egress;
    egress = ses->w3_egress_ptr
                ? (ppd_egress_rec_t*)PPD_VIRT(ses->w3_egress_ptr)
                : 0;

    printMsg ("ppd_destroy_session: session @%#x, egress @%#x\n",
                (unsigned)ses, (unsigned)egress);

    ppd_cleanup_egress_list (egress);

    printMsg ("ppd_destroy_session: session destroyed\n");
    return (0);
}

Uint32 ppd_xcsum_u32(Uint32 csum_delta, Uint32 old_word, Uint32 new_word)
{
	csum_delta += (old_word & 0xFFFF) + ((old_word >> 16) & 0xFFFF);
	csum_delta -= (new_word & 0xFFFF) + ((new_word >> 16) & 0xFFFF);

	//CHK: Doesn't work when old_word < new_word
        //csum_delta  = (csum_delta & 0xFFFF) + ((csum_delta >> 16) & 0xFFFF);
	return (csum_delta);
}

Uint32 ppd_xcsum_u16(Uint32 csum_delta, Uint16 old_hword, Uint16 new_hword)
{
	csum_delta += old_hword - new_hword;
	csum_delta  = (csum_delta & 0xFFFF) + ((csum_delta >> 16) & 0xFFFF);
	return (csum_delta);
}

Int32 ppd_get_ipv4_mod_rec (TI_PP_IPV4_DESC *in_ipv4,
                            TI_PP_IPV4_DESC *out_ipv4,
                            ppd_pkt_mod_rec_t **pkt_mod_ptr
                            )
{
    ppd_pkt_mod_rec_t   *pkt_mod = NULL;
    Uint16              l3_mod_flags = 0, l4_mod_flags = 0;
    Uint32              l3_xcsum = 0;
    Uint16              l4_xcsum = 0;

    /* Make sure we have all the required ingress information for Header mod */
    if ((out_ipv4->enables & in_ipv4->enables) != out_ipv4->enables)
    {
        printErr ("ERROR: Ingress fields reqd for egress L3 chksum not provided\n");
        return -1;
    }

    if (!out_ipv4->enables)
    {
        *pkt_mod_ptr = NULL;
        return 0;
    }

    pkt_mod = (ppd_pkt_mod_rec_t*) ppd_get_free_blk32();

    if(!pkt_mod)
        return -1;

    if (out_ipv4->enables & TI_PP_SESSION_IPV4_DSTIP_VALID)
    {
        l3_mod_flags |= TI_PP_MOD_IPDST_VALID;
        l3_xcsum = ppd_xcsum_u32(l3_xcsum, in_ipv4->dst_ip, out_ipv4->dst_ip);
    }

    if (out_ipv4->enables & TI_PP_SESSION_IPV4_SRCIP_VALID)
    {
        l3_mod_flags |= TI_PP_MOD_IPSRC_VALID;
        l3_xcsum = ppd_xcsum_u32(l3_xcsum, in_ipv4->src_ip, out_ipv4->src_ip);
    }

    if (l3_mod_flags)
        l3_mod_flags |= TI_PP_MOD_L3CHK_VALID;

    if (out_ipv4->enables & TI_PP_SESSION_IPV4_TOS_VALID)
        l3_mod_flags |= TI_PP_MOD_IPTOS_VALID;

    l3_xcsum  = (l3_xcsum & 0xFFFF) + ((l3_xcsum >> 16) & 0xFFFF);
    l4_xcsum = l3_xcsum;

    if (out_ipv4->enables & TI_PP_SESSION_IPV4_DST_PORT_VALID)
    {
        l4_mod_flags |= TI_PP_MOD_DSTPORT_VALID;
        l4_xcsum = ppd_xcsum_u16(l4_xcsum, in_ipv4->dst_port,
                                    out_ipv4->dst_port);
    }

    if (out_ipv4->enables & TI_PP_SESSION_IPV4_SRC_PORT_VALID)
    {
        l4_mod_flags |= TI_PP_MOD_SRCPORT_VALID;
        l4_xcsum = ppd_xcsum_u16(l4_xcsum, in_ipv4->src_port,
                                    out_ipv4->src_port);
    }

    if (l4_mod_flags)
        l4_mod_flags |= TI_PP_MOD_L4CHK_VALID;

    pkt_mod->w0.frags   = WORD_B1_0_S0(0, out_ipv4->tos,
                                        (l3_mod_flags | l4_mod_flags));
    pkt_mod->w1_ip_src  = WORD_W0(out_ipv4->src_ip);
    pkt_mod->w2_ip_dst  = WORD_W0(out_ipv4->dst_ip);
    pkt_mod->w3.frags   = WORD_S1_0(out_ipv4->src_port, out_ipv4->dst_port);
    pkt_mod->w4.frags   = WORD_S1_0(l3_xcsum, l4_xcsum);
    printMsg ("L3 Checksum Delta = %#x L4 Checksum Delta = %#x\n",
                l3_xcsum, l4_xcsum);

   *pkt_mod_ptr = pkt_mod;
    return 0;
}

Int32 ppd_get_l3_mod_rec (TI_PP_PACKET_DESC *in_pkt,
                            TI_PP_PACKET_DESC *out_pkt,
                            ppd_pkt_mod_rec_t **pkt_mod_ptr
                            )
{
    /* Currently IPv4 < --- > IPv6 not supported */
    if(in_pkt->packet_type != out_pkt->packet_type)
    {
        printErr ("ERROR: Ingress-Egress L3/L4 Packet type mismatch (%d!=%d).\n",
                    in_pkt->packet_type, out_pkt->packet_type);
        return -1;
    }

    if(out_pkt->packet_type == TI_PP_IPV4_TYPE)
        return ppd_get_ipv4_mod_rec (&in_pkt->u.ipv4_desc,
                                    &out_pkt->u.ipv4_desc,
                                    pkt_mod_ptr);

    printErr ("ERROR: Unsupported egress L3/L4 packet type (%d).\n",
                out_pkt->packet_type);
    return -1;
}


#define TI_PP_ETH_HDR_MIN   (TI_PP_SESSION_L2_DSTMAC_VALID \
                            | TI_PP_SESSION_L2_SRCMAC_VALID)

typedef struct {
    ppd_l2_frame_t* hdr_blk;
    Uint8           offset_802_3_len;
    Uint8           l2_frame_code;
} ppd_l2_hdr_info_t;

Int32 ppd_get_802_3_hdr (TI_PP_ETH_DESC *out_eth,
                            TI_PP_VPID *out_vpid,
                            Uint8 priority,
                            Uint8 rsv_hdr_len,
                            Uint16 pkt_type,
                            ppd_l2_hdr_info_t *l2_hdr_info)
{
    Uint16 pkt_type_val;
    ppd_l2_frame_t* hdr_blk;
    ppd_memset_word ((Uint32*)l2_hdr_info, 0, SIZE_IN_WORD(*l2_hdr_info));
    /* Check if new header needs to be applied */
    if(!rsv_hdr_len && !out_eth->enables)
        return 0;

    if (out_eth->enables &&
            (out_eth->enables & TI_PP_ETH_HDR_MIN) != TI_PP_ETH_HDR_MIN)
    {
        printErr ("Minimum required egress header not provided "
                    "(%#x != %#x).\n", out_eth->enables,
                    TI_PP_ETH_HDR_MIN);
        return -1;
    }

    pkt_type_val = ppd_get_pkt_type_val (out_vpid->type, pkt_type);
    if (!pkt_type_val && (pkt_type != TI_PP_NO_TYPE))
    {
        printErr ("%s: ERROR - Unsupported payload type %d for vpid type %d\n",
                    __FUNCTION__, pkt_type, out_vpid->type);
        return -1;
    }

    hdr_blk = l2_hdr_info->hdr_blk = ppd_get_l2_buf ();
    if (!hdr_blk)
        return -1;

    printMsg ("Got l2 hdr block @%#x from pool @%#x\n",
                (unsigned)hdr_blk, (unsigned)hblk_pool);

    ppd_l2_buf_hdr_reserve (hdr_blk, rsv_hdr_len);
    if (!out_eth->enables)
        return 0;

    ppd_l2_buf_append(hdr_blk, out_eth->dstmac, 6);
    ppd_l2_buf_append(hdr_blk, out_eth->srcmac, 6);

    if((out_vpid->type == TI_PP_VLAN) || (out_vpid->type == TI_PP_VLAN_PPPoE))
    {
        Uint32 val  = (0x8100 << 16) | ((priority & 0x7) << 13)
                                        | (out_vpid->vlan_identifier & 0xFFF);
        /* CHK: Forcing CFI=0 for now */
        ppd_l2_buf_append(hdr_blk, (Uint8*)&val, sizeof(val));
    }

    if((out_vpid->type == TI_PP_PPPoE) || (out_vpid->type == TI_PP_VLAN_PPPoE))
    {
        /* Apply header as per RFC 2516 */
        Uint16 val;

        val = 0x8864; /* Ether type for PPPoE Session packets */
        ppd_l2_buf_append(hdr_blk, (Uint8*)&val, sizeof(val));

        val = (((1<<4) | 1) << 8) | 0; /* ver=1, type=1, code=0 */
        ppd_l2_buf_append(hdr_blk, (Uint8*)&val, sizeof(val));

        ppd_l2_buf_append(hdr_blk, (Uint8*)&out_vpid->ppp_session_id, 2);
        l2_hdr_info->l2_frame_code |= TI_PP_EGR_FRM_PPPOE_HDR;

    }

    /* This setting is actually only supported for PPPoE classified packets
     * (with IP as PPP type). But should be don't care for other packets.
     */
    if ((pkt_type == TI_PP_IPV4_TYPE) || (pkt_type == TI_PP_IPV6_TYPE))
        l2_hdr_info->l2_frame_code |= TI_PP_EGR_FRM_REFRAME_IP;

    /* TODO : Support for TI_PP_EGR_FRM_PATCH_802_3 */

    if (l2_hdr_info->l2_frame_code
            & (TI_PP_EGR_FRM_PPPOE_HDR | TI_PP_EGR_FRM_PATCH_802_3))
    {
        Uint16 val = 0;

        if (l2_hdr_info->l2_frame_code == TI_PP_EGR_FRM_PATCH_802_3)
            l2_hdr_info->offset_802_3_len = ppd_l2_buf_get_len (hdr_blk);

        ppd_l2_buf_append(hdr_blk, (Uint8*)&val, sizeof(val));
    }

    /* TODO: Handle other cases of 802.3 lenght patching. e.g., 802.3 -> VLAN
     * taga, 802.3 -> 802.3. May need to check ingress information for
     * determining this case.
     */

    if (pkt_type != TI_PP_NO_TYPE)
        ppd_l2_buf_append(hdr_blk, (Uint8*)&pkt_type_val, 2);

    return 0;
}

/* TODO: Minimize number of args */
Int32 ppd_get_l2_hdr (TI_PP_PACKET_DESC *out_pkt,
                        TI_PP_VPID *out_vpid,
                        Uint8 priority,
                        Uint8 rsv_hdr_len,
                        TI_PP_PACKET_TYPE pkt_type,
                        ppd_l2_hdr_info_t* hdr_info)
{
    /* TODO: Add other L2 handling */

    if(out_pkt->packet_type == TI_PP_ETH_TYPE)
        return ppd_get_802_3_hdr (&out_pkt->u.eth_desc, out_vpid, priority,
                                    rsv_hdr_len, pkt_type, hdr_info);

    printErr ("ERROR: Unsupported egress L2 packet type (%d).\n",
            out_pkt->packet_type);
    return -1;
}

ppd_egress_rec_t* ppd_get_egress_rec ( Uint16 num_egress,
                                        TI_PPD_IF* egress_if,
                                        TI_PP_SESSION_PROPERTY* in_prop,
                                        TI_PP_SESSION_PROPERTY* out_prop,
                                        Uint8 is_routable,
                                        Uint8 priority )
{
    ppd_egress_rec_t	*first_egress, *egress, *next_egress;
    ppd_pkt_mod_rec_t	*pkt_mod = NULL;
    ppd_l2_hdr_info_t   hdr_info;
    ppd_l2_frame_t      *l2_hdr = NULL;
    Uint32  turbo_tcp_ack_num = 0;	        /* TODO: */

    first_egress = egress = (ppd_egress_rec_t*) ppd_get_free_blk32();

    while (num_egress--)
    {
        TI_PP_VPID *vpid    = egress_if->vpid;
        TI_PP_PID *pid      = egress_if->pid;
        Uint8   frame_code =0, l2_hdr_size = 0;

        if(!egress)
            return NULL;;

        egress->w0_next_egr_ptr  = WORD_W0(0);
        egress->w5_l2_hdr_ptr	 = WORD_W0(0);
        egress->w6_mod_rec_ptr   = WORD_W0(0);

        if (is_routable)
        {
            /* Note :- In case of multiple egress records, the current
             * implementation  only considers L3/L4 modification for the first
             * egress record. Thus, all subsequent records will have the same
             * modification as in the first record. __IMPORTANT__ This puts
             * restriction on caller to have only one modification allowed
             * across all applicable egress' as well as forces to have the
             * modification to be provided as part of first egress since the
             * succeeding mods will be skipped.
             */
            if((first_egress == egress)
                    && ppd_get_l3_mod_rec (&in_prop->l3l4_packet,
                                    &out_prop->l3l4_packet, &pkt_mod))
                goto lbl_mod_err;

            if(pkt_mod)
            {
                egress->w6_mod_rec_ptr   = PPD_PHYS(pkt_mod);
#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
                printMsg ("Modification record dump:\n");
                ppd_dump_mod_rec ((Uint32 *)(pkt_mod),
                                    dump_buff, sizeof(dump_buff));
                printMsg("%s\n",dump_buff);
#endif
            }
        }

        if(ppd_get_l2_hdr (&out_prop->l2_packet, vpid, 0 /*TODO: priority*/,
                            (vpid->priv_tx_data_len+pid->tx_hw_data_len),
                            out_prop->l3l4_packet.packet_type, &hdr_info))
            goto lbl_frm_err;

        l2_hdr = hdr_info.hdr_blk;
        if (is_routable && (!l2_hdr || !ppd_l2_buf_get_len(l2_hdr)))
        {
            printErr ("ERROR: Required L2 header information not provided "
                        "for routable session.\n");
            goto lbl_hdr_err;
        }

        if (l2_hdr)
        {
            /* Check if only L1 data needs to be applied with L2 intact */
            if (ppd_l2_buf_get_len(l2_hdr))
                frame_code = TI_PP_EGR_FRM_STRIP_L2;

            ppd_l2_buf_prepend(l2_hdr, &vpid->priv_tx_data[0],
                                vpid->priv_tx_data_len);
            ppd_l2_buf_prepend(l2_hdr, &pid->tx_hw_data[0],
                                pid->tx_hw_data_len);

            frame_code |= hdr_info.l2_frame_code;

            l2_hdr_size = ppd_l2_buf_get_len(l2_hdr);
#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
            printMsg ("L2 Header Dump:\n");
            ppd_dump_l2_hdr((Uint32*)l2_hdr->data, dump_buff, l2_hdr_size);
            printMsg("%s\n",dump_buff);
#endif
            PAL_CPPI4_CACHE_WRITEBACK_INVALIDATE((void*)l2_hdr->data,
                                                    PPD_L2_FRM_MAX_HLEN);
            l2_hdr->data = (Uint8*)PAL_osMemVirt2Phy(l2_hdr->data);
        }

        next_egress = 0;
        if(num_egress)
        {
            next_egress      = (ppd_egress_rec_t*) ppd_get_free_blk32();
            egress->w0_next_egr_ptr  = WORD_W0(PPD_PHYS((Uint32)next_egress));
            pkt_mod = NULL;
        }

		/* Support TurboDox */
		if (out_prop->app_specific_data.u.app_desc.enables & TI_PP_SESSION_APP_RAW_INFO1_B3_VALID)
		{
			frame_code |= TI_PP_EGR_FRM_TURBODOX_EN;
			turbo_tcp_ack_num = out_prop->app_specific_data.u.app_desc.raw_app_info2;

          /* Support advanced TurboDox option to allow suppression of 12 bytes length TCP option */
            if (out_prop->app_specific_data.u.app_desc.enables & TI_PP_SESSION_APP_RAW_INFO1_B3_PLUS_VALID)
            {
                frame_code |= TI_PP_EGR_FRM_TURBODOX_ADV_EN;
            }
		}
		
        /* TODO: Add support for Ingress Priority Mode */
        egress->w1.frags    = WORD_B3_0(/*CHK*/pid->dflt_pri_drp,
                                        vpid->vpid_handle,
                                        frame_code, l2_hdr_size);
        egress->w2.frags    = WORD_S1_0(/*CHK*/pid->dflt_dst_tag,
                                        /*CHK*/pid->tx_pri_q_map[priority]);
        egress->w3.frags    = WORD_S0_B1_0(0, pid->type,
                                            hdr_info.offset_802_3_len);
        egress->w4_turbo_tcp_ack_num	= WORD_W0(turbo_tcp_ack_num);
        egress->w5_l2_hdr_ptr	        = (Uint32)WORD_W0(l2_hdr->data);

        /* DOCSIS/TurboDox application specific handling */
        if (out_prop->app_specific_data.u.app_desc.enables != 0)
        {
            egress->w7_proto_spec = WORD_W0(out_prop->app_specific_data.u.app_desc.u.raw_app_info1);
        }
        else
        {
            egress->w7_proto_spec = WORD_W0(0);
        }


#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
        printMsg ("Egress record dump:\n");
        ppd_dump_ef_rec ((Uint32 *)(egress), dump_buff, sizeof(dump_buff));
        printMsg("%s\n",dump_buff);
#endif
        egress = next_egress;
        egress_if++;
        out_prop++;
    }

    return first_egress;

lbl_hdr_err:
lbl_frm_err:
lbl_mod_err:
    ppd_cleanup_egress_list (first_egress);
    return NULL;
}

Int32 ti_ppd_create_session(TI_PP_SESSION* ses_cfg, TI_PPD_IF *ingress_if,
                            TI_PPD_IF *egress_if)
{
    TI_PP_ses_id_t	ses_id  = (TI_PP_ses_id_t)ses_cfg->session_handle;
    TI_PP_VPID *ingress_vpid    = ingress_if->vpid;
    Int32   ret_val = -1;
    Uint32  cookie;
    /*
     * CHK: Always setting IDLE base timeout since there is no way to configure
     * otherwise in current data structure
     */
    Uint16  session_flags       = TI_PP_SES_FLAG_IDLE_TMOUT;
    ppd_ses_info_blk_t  *ses	= (ppd_ses_info_blk_t*)(gp_ses_blk + ses_id);
    ppd_egress_rec_t	*egress;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    if (IS_SESSION_SLOT_FILLED(ses_id)) {
        printErr ("%s: ***Error: session %d already exists or being created.\n",
                    __FUNCTION__, ses_id);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
        return -1;
    }
    g_session_slots[(Uint32)(ses_id)/32] |= (1<<((Uint32)(ses_id)%32));
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);

    /* Clears leftover stats */
    ppd_memset_word ((Uint32 *)ses, 0, SIZE_IN_WORD(ppd_ses_info_blk_t));

    egress = ppd_get_egress_rec (ses_cfg->num_egress, egress_if,
                                    &ses_cfg->ingress, &ses_cfg->egress[0],
                                    ses_cfg->is_routable_session,
                                    ses_cfg->priority);
    if (!egress)
        goto lbl_session_err;

    ses->w3_egress_ptr  = PPD_PHYS(egress);

    printMsg ("ti_ppd_create_session: session @%#x, egress = %#x, "
            "Phys = %#x\n", (unsigned)ses, (unsigned)egress,
            (unsigned)PPD_PHYS(egress)/*ses->w3_egress_ptr*/);

    if (ses_cfg->session_timeout == 0)
        session_flags |= TI_PP_SES_FLAG_PASS_AFTER_TMOUT;

    /* Check if this is a bridged or routed session? */
    if (ses_cfg->is_routable_session == 1)
    {
        /* All routable sessions will need their TTL Flags decremented by the
         * PDSP.
         */
        session_flags |= TI_PP_SES_FLAG_UPDATE_TTL;
    }
    else
    {
        /* All bridged sessions dont need to have their TTL Flags updated;
         * since this is layer2 operation. Besides for bridged sessions
         * Fragmented and TCP Control packets can be handled by the PDSP
         * firmware itself. The host bridging layer does not care about such
         * frames.
         */
        session_flags |= TI_PP_SES_FLAG_TCP_CONTROL | TI_PP_SES_FLAG_IP_FRAG;
    }

    ses->w0.frags = WORD_B1_0_S0(ingress_vpid->vpid_handle, 0, session_flags);
    /* TODO: Include information about IPv6 in the ses_prop */
    ses->w1_ipv6_rec = 0;
    ses->w2.frags	    = WORD_S0_B1_0(0, 0, 0);
    /* Convert provided usecs as 10^-5 seconds with mimnimum of 10us */
    ses->w4_ses_timeout	    = (ses_cfg->session_timeout/10)
                                + ((ses_cfg->session_timeout%10) ? 1 : 0);

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
    printMsg ("Session base dump for idx %d:\n", ses_id);
    ppd_dump_ses_rec ((Uint32 *)ses, dump_buff, sizeof(dump_buff));
    printMsg("%s\n",dump_buff);
#endif

    {
        ppd_lut_t lut_data = {ses_cfg, ingress_vpid};
        cpdsp_cmd_t cmd_word = WORD_S0_B1_0(ses_id, SESSION_ADD_SYNC, 0x86);

        ret_val = cpdsp_cmd_rsp_by_pm(cmd_word, ppd_wr_lut, &lut_data, 0, 0);
    }

    if (ret_val)
        goto lbl_session_err;

    return ret_val;

lbl_session_err:
    ppd_cleanup_egress_list (egress);
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    g_session_slots[(Uint32)(ses_id)/32] &= ~(1<<((Uint32)(ses_id)%32));
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
    return ret_val;
}

Int32 ti_ppd_modify_session(TI_PP_SESSION *ses_cfg,
                            TI_PPD_IF *ingress_if,
			    TI_PPD_IF *egress_if)
{
    TI_PP_ses_id_t  ses_id          = (TI_PP_ses_id_t)ses_cfg->session_handle;
    TI_PP_VPID      *ingress_vpid   = ingress_if->vpid;
    Int32           ret_val         = -1;
    Uint32          cookie;
    cpdsp_cmd_t     cmd_word;
    ppd_lut_t       lut_data        = {ses_cfg, ingress_vpid};


    ppd_ses_info_blk_t  *ses	= (ppd_ses_info_blk_t*)(gp_ses_blk + ses_id);
    ppd_egress_rec_t	*egress;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    if (!IS_SESSION_SLOT_FILLED(ses_id)) {
        printErr ("%s: ***Error: session %d does not exist.\n",
                    __FUNCTION__, ses_id);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
        return -1;
    }
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);

    /* Put the session in DIVERSION */
    cmd_word = WORD_S0_B1_0(ses_id, SESSION_STATE_DIVERT,
                            SRPDSP_SESSION_COMMAND);
    if ((ret_val = cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0)))
    {
        printErr ("%s: ***Error: Unable to put session %d in DIVERSION.\n",
                    __FUNCTION__, ses_id);

        return ret_val;
    }

    /* Note: NOT clearing session stats */

    /* Free up previously allocated egress, modification records and header
     * data. New data blocks will again be allocated as per session info.
     *
     * Note: We could lock the whole part from deallocation of data blocks till
     * reallocation to prevent situation where various session creates during
     * this API's execution would lead to resource failure error. By locking
     * till reallocation is done, we can ensure that this session will at least
     * get all the previously allocated blocks back.
     *
     * TODO: A better approach would be to reuse the existing blocks.
     */
    egress = ses->w3_egress_ptr
                ? (ppd_egress_rec_t*)PPD_VIRT(ses->w3_egress_ptr)
                : 0;

    ppd_cleanup_egress_list (egress);

    egress = ppd_get_egress_rec (ses_cfg->num_egress, egress_if,
                                    &ses_cfg->ingress, &ses_cfg->egress[0],
                                    ses_cfg->is_routable_session,
                                    ses_cfg->priority);
    if (!egress)
        goto lbl_session_mod_err;

    ses->w3_egress_ptr  = PPD_PHYS(egress);

    printMsg ("ti_ppd_modify_session: session @%#x, egress = %#x, "
            "Phys = %#x\n", (unsigned)ses, (unsigned)egress,
            (unsigned)PPD_PHYS(egress)/*ses->w3_egress_ptr*/);

    ses->w0.ingress_vpid = ingress_vpid->vpid_handle;

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
    printMsg ("Session base dump for idx %d:\n", ses_id);
    ppd_dump_ses_rec ((Uint32 *)ses, dump_buff, sizeof(dump_buff));
    printMsg("%s\n",dump_buff);
#endif

    cmd_word = WORD_S0_B1_0(ses_id, SESSION_LUT_UPDATE,
                            SRPDSP_SESSION_COMMAND);
    if ((ret_val = cpdsp_cmd_rsp_by_pm(cmd_word, ppd_wr_lut, &lut_data, 0, 0)))
    {
        printErr ("%s: ***Error: LUT update for session %d FAILED.\n",
                    __FUNCTION__, ses_id);
        goto lbl_session_mod_err;
    }

    /* SYNCHRONIZE the session */
    cmd_word = WORD_S0_B1_0(ses_id, SESSION_STATE_SYNC,
                            SRPDSP_SESSION_COMMAND);
    if ((ret_val = cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0)))
        goto lbl_session_mod_err;

    return ret_val;

lbl_session_mod_err:
    ti_ppd_delete_session (ses_id);
    return ret_val;
}

Int32 ti_ppd_delete_session(Uint8 session_handle)
{
    Int32 ret_val;
    Uint32 cookie;
    ppd_ses_info_blk_t *ses    = (ppd_ses_info_blk_t*)(gp_ses_blk
                                    + (TI_PP_ses_id_t)session_handle);
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(session_handle, SESSION_REMOVE, 0x86);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    if (!IS_SESSION_SLOT_FILLED(session_handle)) {
        printErr ("%s: ***Error: session %d does not exist.\n",
                    __FUNCTION__, session_handle);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
        return -1;
    }
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);

    ret_val = cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0);

    if (ret_val)
    {
        printErr ("Failed to destroy session @%#x\n", (Uint32)ses);
        return ret_val;
    }

    /* CHK: We are cleaning up after destroying the session. This will work if
     * the egress lists are not cleared by firmware on session remove.
     */
    ppd_destroy_session(ses);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    g_session_slots[(Uint32)(session_handle)/32]
                    &= ~(1<<((Uint32)(session_handle)%32));
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);

    return 0;
}

typedef struct
{
    union
    {
        Uint8  base_index;
        Uint8  count;
        Uint8  type;
        Uint8  port_num;            /* May be moved into input arg */
        Uint32 frags;

    } w0;

} ppd_cppi_pid_cfg_t;

Int32 ppd_wr_cppi_pid_cfg(volatile Uint32 *ram_ptr, void *in_obj)
{
    ppd_cppi_pid_cfg_t *cppi_cfg   = (ppd_cppi_pid_cfg_t*)ram_ptr;
    TI_PP_PID_RANGE *dat     = (TI_PP_PID_RANGE*)in_obj;

    cppi_cfg->w0.frags = WORD_B3_0(dat->port_num, dat->type, dat->count,
                                   dat->base_index);

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
    printMsg ("PID Range configuration:\n");
    ppd_dump_pid_install ((Uint32 *)ram_ptr, dump_buff, sizeof(dump_buff));
    printMsg("%s\n",dump_buff);
#endif
    return (0);
}

#define CPDSP_CMD(i, o, c) ((i << 16) | (i << 8) | c)

Int32 ti_ppd_config_pid_range(TI_PP_PID_RANGE *cfg)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0x00, 0x04, 0x84);

    return (cpdsp_cmd_rsp_by_pm(cmd_word, ppd_wr_cppi_pid_cfg, cfg, 0, 0));
}

Int32 ti_ppd_remove_pid_range(Uint32 port_num)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0x00, 0x05, 0x84);

    return (cpdsp_cmd_rsp_by_sz(cmd_word, &port_num, 1, 0, 0));
}

static Int32 ppd_test_n_dll_pdsp(Uint32 id, Uint32 *data, Uint32 size)
{
    pdsp_info_t *p_pdsp = &g_pdsp_info[id];
    pdsp_regs_t *p_regs = (pdsp_regs_t*)p_pdsp->regs;
    Uint32   wait_count = p_pdsp->wait_count;

    printMsg ("ENTRY: ppd_test_n_dll_pdsp");

    p_regs->control = 0;
    while(wait_count--)
        if(p_regs->control & (PDSP_CTL_N_RST_BIT))
            break;

    if(!wait_count) {
        printErr ("ppd_test_n_dll_pdsp: "
                    "Timeout waiting for reset complete for PDSDP(%d).\n", id);
        return (-1);
    }

    printMsg ("ppd_test_n_dll_pdsp: PDSDP(%d) reset done.\n", id);

    ppd_memcpy_word(p_pdsp->iram, data, (size + 0x3) >> 2);

    p_pdsp->trg_cmd(p_pdsp, 0xffffffff); /* 0xff << 16 | 0xff << 8 | 0xff */

    p_regs->control = PDSP_CTL_EN_BIT;

    printMsg ("EXIT: ppd_test_n_dll_pdsp");

    return(p_pdsp->rsp_sts(p_pdsp));
}

static Uint32 g_boot = 0;

static Int32 ppd_init_pdsp(TI_PP_FIRMWARE *fw)
{
    Int32 ret_val = -1;

    switch (fw->id)
    {
        case TI_PP_CPDSP_FW:
        case TI_PP_MPDSP_FW:
        case TI_PP_QPDSP_FW:
            if(!g_boot) {ret_val = -1; break;}

        case TI_PP_BOOTS_FW:
            ret_val = ppd_test_n_dll_pdsp(fw->id, fw->data, fw->size);

            printMsg ("ppd_init_pdsp: TI_PP_BOOTS_FW command status = %d\n",
                        ret_val);

            if(!ret_val)
                g_boot = 1;
            break;

        default:
            break;
    }

    return (ret_val);
}


typedef struct
{
    PAL_Cppi4QueueHnd       eventQHnd;
    PAL_Cppi4QueueHnd       eventFQHnd;

    TI_PPD_EVENT_HANDLER    pfnEventHdlr;
    PAL_OsTimerHandle       hTimer;
    Uint32                  polltime_msecs;
} ppd_event_hdlr_cfg_t;

typedef struct
{
    union
    {
        Uint16  dflt_host_rx_q;          /* Default Host Rx Queue */
        Uint16  dflt_host_rx_dst_tag;    /* Default Host Rx Destination Tag */
        Uint32  frags;
    } w0;

    union
    {
        Uint16  host_ev_queue;           /* Index of host event queue */
        Uint16  dflt_host_rx_dst_tag;    /* Q Manager of host event queue */
        Uint32  frags;
    } w1;

    Uint32  w2_sync_max_pkt;        /* Max pkts in syncrnzn queue */
    Uint32  w3_sync_timeout_10us;   /* Max session sync time multiple of 10us */
    Uint32  w4_buff_pool_indx;      /* Pool for PP internal 128 byte buffers */

} ppd_sr_open_params_t;

static ppd_event_hdlr_cfg_t g_ppd_event_hdlr_cfg;

Int32 ti_ppd_init(Uint32 num_of_fw, TI_PP_FIRMWARE *firmware,
                  TI_PPD_CONFIG *cfg)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0x00, cfg->buff_pool_indx, 0x80);

    struct srl_ret_init_param
    {
        Uint32 ses_tbl_phy_mem;
        Uint32 ses_byte_size;
        Uint32 aux_ses_phy_mem;
        Uint32 aux_ses_byte_size;

    } ret_param;

    Int32 i = 0, ret_val = 0;
    ppd_sr_open_params_t sr_open_params;

    /* CHK: Not locking the init sequence here since this function is expected
     * to be called as part of system initialization. May need to use locks
     * otherwise.
     */
    if (g_ppd_init_done)
        return 0;

    for(i = 0; i < num_of_fw; i++)
    {
        if(ppd_init_pdsp(firmware + i))
        {
            ret_val = -1;
            return (ret_val);
        }
    }

    {
        PAL_Handle hnd;
        Cppi4Queue tmpQ;    /* Just used for filling Q info for opening */

        /* Get PAL CPPI4 Handle to use its APIs */
        hnd = PAL_cppi4Init (NULL, NULL);

        /* Open and store handles to event queuse for use by event timer */
        tmpQ.qMgr   = cfg->host_q_mgr;
        tmpQ.qNum   = cfg->host_ev_queue;
        g_ppd_event_hdlr_cfg.eventQHnd = PAL_cppi4QueueOpen (hnd, tmpQ);
        tmpQ.qMgr   = PP_HOST_EVENT_FDQMGR;
        tmpQ.qNum   = PP_HOST_EVENT_FDQ;
        g_ppd_event_hdlr_cfg.eventFQHnd = PAL_cppi4QueueOpen (hnd, tmpQ);
        g_ppd_event_hdlr_cfg.polltime_msecs = EVENT_POLLTIME_MSECS;
    }

    /* Open command */
    sr_open_params.w0.frags             = WORD_S1_0(cfg->dflt_host_rx_dst_tag,
                                                    cfg->dflt_host_rx_q);
    sr_open_params.w1.frags             = WORD_S1_0(cfg->host_q_mgr,
                                                    cfg->host_ev_queue);
    sr_open_params.w2_sync_max_pkt      = cfg->sync_max_pkt;
    sr_open_params.w3_sync_timeout_10us = cfg->sync_timeout_10us;
    sr_open_params.w4_buff_pool_indx    = cfg->buff_pool_indx;

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
    printMsg ("ti_ppd_init: SR_OPEN dump:\n");
    ppd_dump_sr_open ((Uint32*)&sr_open_params, dump_buff, sizeof(dump_buff));
    printMsg("%s\n",dump_buff);
#endif
    ret_val = cpdsp_cmd_rsp_by_sz(cmd_word, &sr_open_params,
                                    SIZE_IN_WORD(sr_open_params),
                                    &ret_param, SIZE_IN_WORD(ret_param));
    printMsg ("ti_ppd_init: SR_OPEN returned %d.\n", ret_val);

    if (ret_val)
        return ret_val;

    gp_ses_blk         =  (ppd_ses_info_blk_t *)
                            PPD_VIRT(ret_param.ses_tbl_phy_mem);
    gp_free_blk32_list =  (ppd_block32_t *) (((Uint8*)gp_ses_blk)
                                            + 256*sizeof(ppd_ses_info_blk_t));

    printMsg ("ti_ppd_init: gp_ses_blk = %#x\n", (unsigned)gp_ses_blk);

    /* todo: we are not reading all the return params */

    {
         Uint32 last_free_blk32 =
                             (Uint32)gp_ses_blk + ret_param.ses_byte_size - 32;
         ppd_init_blk32((ppd_block32_t*)last_free_blk32);
         ppd_init_hblk();
         ppd_memset_word (g_session_slots, 0, SIZE_IN_WORD(g_session_slots));
    }

    g_ppd_init_done = 1;
    return 0;
}

Int32 ti_ppd_exit(void)
{
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0x00, 0x00, 0x81);
    g_ppd_init_done = 0;
    return (cpdsp_cmd_rsp_by_sz(cmd_word, 0, 0, 0, 0));
}

static void  ppd_event_timer(unsigned long data)
{
    ppd_event_hdlr_cfg_t* event_cfg = (ppd_event_hdlr_cfg_t*)data;
    Cppi4EmbdDesc* emDesc = NULL;
    int pacing_iteration = 16;

    do
    {
        if ((emDesc = (Cppi4EmbdDesc*)PAL_cppi4QueuePop (event_cfg->eventQHnd)))
        {
            emDesc = (Cppi4EmbdDesc*)PPD_VIRT(emDesc); 
            printEvn ("ppd_event_timer: Got event -> EPI0 = %x\n", emDesc->EPI[0]);
            
            event_cfg->pfnEventHdlr ((emDesc->EPI[0] & 0xff), 
                ((emDesc->EPI[0]>>16) & 0xffff), 
                ((emDesc->EPI[0]>>8) & 0xff));
            
            PAL_CPPI4_CACHE_INVALIDATE(emDesc, sizeof(Cppi4EmbdDesc));
            PAL_cppi4QueuePush (event_cfg->eventFQHnd, (Ptr)PPD_PHYS(emDesc), 
                (sizeof(Cppi4EmbdDesc)-24)/4, 0); 
        }

        pacing_iteration--;
    }
    while ((emDesc) && (pacing_iteration));

    PAL_osTimerStart (event_cfg->hTimer, event_cfg->polltime_msecs);
}

Int32 ti_ppd_register_event_handler (TI_PPD_EVENT_HANDLER ptr_event_handler)
{
    if (!g_ppd_init_done) return -1;
    if (PAL_osTimerCreate (ppd_event_timer, (Uint32)&g_ppd_event_hdlr_cfg,
                            &g_ppd_event_hdlr_cfg.hTimer) != PAL_SOK)
    {
        printErr ("%s: Event timer installation failed\n", __FUNCTION__);
        return -1;
    }
    g_ppd_event_hdlr_cfg.pfnEventHdlr = ptr_event_handler;

    PAL_osTimerStart (g_ppd_event_hdlr_cfg.hTimer,
                        g_ppd_event_hdlr_cfg.polltime_msecs);

    return 0;
}

Int32 ti_ppd_deregister_event_handler ()
{
    if (g_ppd_event_hdlr_cfg.pfnEventHdlr)
    {
        PAL_osTimerDestroy (g_ppd_event_hdlr_cfg.hTimer);
        g_ppd_event_hdlr_cfg.pfnEventHdlr = NULL;
    }
    return 0;
}

Int32 ppd_health_check (volatile Uint32 *ram_ptr, void *in_obj)
{
    *(Int32*) in_obj = 0;

    /* Check for Error */
    if (ram_ptr[0]) *(Int32*) in_obj |= (1<<0); /* CPDSP */
    if (ram_ptr[1]) *(Int32*) in_obj |= (1<<1); /* MPDSP */

    return (0);
}

Int32 ti_ppd_health_check ()
{
    Int32 ret_val, status_code;
    cpdsp_cmd_t cmd_word = WORD_S0_B1_0(0, 0, SRPDSP_SR_STATUS);
    if ((ret_val = cpdsp_cmd_rsp_by_pm(cmd_word, 0, 0,
                                        ppd_health_check, &status_code)) == 0)
        return status_code;
    else
        return ret_val;
}

Int32 ti_ppd_get_session_dump (char *buf, char **start, Uint32 offset, int count, int *peof, void *data)
{
    Uint32 len = 0, cookie, blk_num;
    int egr_cnt = 0, mod_cnt = 0;
    ppd_egress_rec_t *egress, *next_egress;
    Uint32 mod_ptr = 0;
    Uint8 session_handle = *(Uint8*)data;
    ppd_ses_info_blk_t *ses     = (ppd_ses_info_blk_t*)(gp_ses_blk
                                    + (TI_PP_ses_id_t)session_handle);

    if ((PPD_DUMP_BUFF_BLK_LEN > count) || !buf)
        return -1;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &cookie);
    if (!IS_SESSION_SLOT_FILLED(session_handle)) {
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);
        return -1;
    }
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, cookie);

    /* Get the block number to dump */
    blk_num = offset / PPD_DUMP_BUFF_BLK_LEN;

    egress = ses->w3_egress_ptr
                ? (ppd_egress_rec_t*) PPD_VIRT(ses->w3_egress_ptr)
                : 0;

    /* We will provide the session dump as a series of 'blocks'. Set 'start' as
     * block size so that subsequent call will move to the next block (if
     * applicable).
     */
    *(Uint32*)start = PPD_DUMP_BUFF_BLK_LEN;

    /* This logic won't work for the case when PPD_DUMP_BUFF_BLK_LEN > buf.
     * Return error in such case.
     */
    if (*start >= buf) return -1;

    if (blk_num == 0) {
        len = snprintf (buf, count,
                "Session record dump for session id %d:\n",
                (Int32) session_handle);
        len += ppd_dump_ses_rec ((Uint32 *)ses, (buf+len), (count-len));
        /* There will be atleast one egress record present in a valid session.
         * So we are not checking for setting eof here.
         */
        return len;
    }

    while (egress) {
        next_egress = egress->w0_next_egr_ptr
                        ? (ppd_egress_rec_t*) PPD_VIRT(egress->w0_next_egr_ptr)
                        : 0;
        mod_ptr = egress->w6_mod_rec_ptr;
        egr_cnt++;
        if (blk_num == (egr_cnt + mod_cnt)) {
            len += snprintf ((buf+len), (count-len),
                    "Egress record(%d) dump:\n", egr_cnt);
            len += ppd_dump_ef_rec ((Uint32 *)egress,
                    (buf+len), (count-len));
            if (!next_egress && !mod_ptr)
                *peof = 1;
            return len;
        }
        if (mod_ptr) {
            mod_cnt++;
            if (blk_num == (egr_cnt + mod_cnt)) {
                len += snprintf ((buf+len), (count-len),
                        "Mod record(%d) dump:\n", egr_cnt);
                len += ppd_dump_mod_rec ((Uint32 *)PPD_VIRT(mod_ptr),
                        (buf+len), (count-len));
                if (!next_egress)
                    *peof = 1;
                return len;
            }
        }
        egress = next_egress;
    }

    return len;
}
