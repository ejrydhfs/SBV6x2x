/*
 * Copyright (C) 2007 Texas Instruments, Inc.  All rights reserved.
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
 *
 * Puma-5 Packet Processor initialization.
 * Contains Puma-5 specific initialization. The numbers (addresses etc) are
 * Puma-5 specific. The static structures are filled in with Puma-5 specific
 * data and the generic PPD init function gets called in the end
 * with this data.
 */

#include <linux/config.h>
#include <asm/arch/generic/pal.h>
#include <asm/arch/generic/pal_cppi41.h>
#include <asm/arch/generic/pal_cppi41pvt.h>
#include <../drivers/net/avalanche_cpgmac_f/ddc_cpgmac_f_Drv.h>

#ifdef EL
/* TODO: Include LE binaries and use corresponding structures */
#else
#include <asm/arch/puma5/c_bootcode_be_bin.h>
#include <asm/arch/puma5/c_srp5_be_bin.h>
#include <asm/arch/puma5/m_srp5_be_bin.h>
#include <asm/arch/puma5/q_srp5_be_bin.h>
#endif

#include <ti_ppd.h>
#include <asm/arch/puma5/puma5_pp.h>

#define TI_PP_NUM_FW    4


/* This is PPD global initialization structure. Contains static initialization
 * information for the whole PPD subsystem. 
 */
TI_PP_FIRMWARE ppFirmware_g [TI_PP_NUM_FW] = {
    {
        .id     = TI_PP_BOOTS_FW,
        .data   = (Uint32*)&PDSPcode_c_bootcode_be_0002[0],
        .size   = sizeof (PDSPcode_c_bootcode_be_0002)
    },

    {
        .id     = TI_PP_CPDSP_FW,
        .data   = (Uint32*)&PDSPcode_c_srp5_be_0002[0],
        .size   = sizeof (PDSPcode_c_srp5_be_0002)
    },

    {
        .id     = TI_PP_MPDSP_FW,
        .data   = (Uint32*)&PDSPcode_m_srp5_be_0002[0],
        .size   = sizeof (PDSPcode_m_srp5_be_0002)
    },

    {
        .id     = TI_PP_QPDSP_FW,
        .data   = (Uint32*)&PDSPcode_q_srp5_be_0002[0],
        .size   = sizeof (PDSPcode_q_srp5_be_0002)
    }
};

/* CHK: Forcing discard for packets not matching any installed PID */
TI_PPD_CONFIG ppCfg_g = {
    .dflt_host_rx_q         = 0 /* Discard */,
    .dflt_host_rx_dst_tag   = 0,
    .host_ev_queue          = PP_HOST_EVENTQ, 
    .host_q_mgr             = PP_HOST_EVENTQMGR,
#define SYNC_TIMEOUT_MICROSEC           1000
#define SYNC_TIMEOUT_MAXPACKET          48
    .sync_max_pkt           = SYNC_TIMEOUT_MAXPACKET,
    .sync_timeout_10us      = SYNC_TIMEOUT_MICROSEC/10,
    .buff_pool_indx         = 13 
};

/* On-Chip descriptor management
 * IMPORTANT NOTES:
 * ===============
 *  -   Following macros control how the on-chip descriptor memory is devided into
 *  different descriptors. All the base addresses must be varefully determined
 *  not to avoid overlap.
 *  -   Also note that all the On-cip descriptors *must* be of size equal to the
 *  specified in puma5_cppi.c cppi config structure for respective region.
 */
#define PREFETCH_DESC_BASEADDR      PPFW_CPPI4x_BD_BASE 
#define REPLICATOR_DESC_BASEADDR    ((Uint32)PREFETCH_DESC_BASEADDR \
                                    +(PREFETCH_DESC_COUNT*PREFETCH_FD_SIZE))
#define EVENT_DESC_BASEADDR         ((Uint32)REPLICATOR_DESC_BASEADDR \
                                    +(PPFW_BD_NUM*PREFETCH_FD_SIZE))

PrefchCfg prefCfg_g = {
    .pfDescCnt      = PREFETCH_DESC_COUNT,
    .pfDescBase     = (Ptr) PREFETCH_DESC_BASEADDR,
    .pfFQ.qMgr      = PPFW_CPPI4x_APDSP_FD_QMGR,
    .pfFQ.qNum      = PPFW_CPPI4x_APDSP_FD_QNUM,

    .repDescCnt     = PPFW_BD_NUM,
    .repDescBase    = (Ptr) REPLICATOR_DESC_BASEADDR,
    .repFQ.qMgr     = PPFW_CPPI4x_APDSP_PREBUF_QMGR,
    .repFQ.qNum     = 121,

    .pfBlkCnt       = PREFETCH_BUFF_COUNT,
    .pfBlkBase      = (Ptr) PPFW_CPPI4x_BUF_BD_BASE,
    .pfFBQ.qMgr     = PPFW_CPPI4x_APDSP_PREBUF_QMGR,
    .pfFBQ.qNum     = PPFW_CPPI4x_APDSP_PREBUF_QNUM,

    .pfDataSize     = PREFETCH_DATA_SIZE,
    .pfDataOffset   = PREFETCH_DATA_OFFSET,
    .pfCmdBase      = (Ptr) AVALANCHE_NWSS_APDSP_CMD_BASE
};

typedef struct {
    Uint32 numDesc;
    Uint32 qNum;

    Ptr firstDescPtr;
    Uint32 isAllocated;

    /* Desc config for the  group */
    Uint32 pktType;
} EmbBDBlkInfo;
/* Embedded descriptor table:
 *  - This table aggregates the embedded descriptors in the system. It contains
 *  information about the first descriptor and allocation status of each
 *  division of the region. Generally this division is based on per driver or
 *  per Q basis.
 */
typedef struct {
    Ptr embBDRgnPtr;
    Uint32 qMgr;
    Uint32 numDesc;
    Uint32 szDesc;
    Uint32 numBlks;
    EmbBDBlkInfo  embBDBlk [PAL_CPPI41_FD_Q_LAST - PAL_CPPI41_FD_Q_BASE];
} EmbBDCfg;

EmbBDCfg ppEmbBDCfg_g = {
    .qMgr       = PPFW_CPPI4x_FD_QMGR,
    .numDesc    = 512,  /* Must be same as in Cppi Cgf structure */
    .szDesc     = 64,   /* Must be same as for specific region */
    .numBlks    = 3,    /* Change for each block added/removed */
       
    /* Ethernet */
    .embBDBlk[0].numDesc   = CPMAC_RX_EMBEDDED_BD_NUM,
    .embBDBlk[0].qNum      = CPMAC_CPPI4x_FD_QNUM(0), //CPMAC_CPPI4x_FD_QMGR
    .embBDBlk[0].pktType   = PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH,

    /* Host2PP Infra 0 */
    .embBDBlk[1].numDesc   = 108,   /* !@@@ Use #def */
    .embBDBlk[1].qNum      = PPFW_CPPI4x_FD_QNUM(0), //PPFW_CPPI4x_FD_QMGR


    /* Host2PP Infra 1 */
    .embBDBlk[2].numDesc   = 64,   /* !@@@ Use #def */
    .embBDBlk[2].qNum      = PPFW_CPPI4x_FD_QNUM(1), //PPFW_CPPI4x_FD_QMGR

    /* TODO: Add USB, etc. */
};

  
void setup_host2pp_infra (PAL_Handle hnd, int infra_chan)
{
#define HOST2PP_INFRA_NUM   2
    Cppi4TxChInitCfg txCh;
    Cppi4RxChInitCfg rxCh;
    PAL_Cppi4TxChHnd cppi4TxChHnd;
    PAL_Cppi4RxChHnd cppi4RxChHnd;
    
    /* Set up Rx channel */
    rxCh.chNum              = HOST2PP_INFRA_CHN(infra_chan); 
    rxCh.dmaNum             = PAL_CPPI41_DMA_BLOCK1;
    rxCh.defDescType        = 0;    /* Embedded */
    rxCh.sopOffset          = 0;
    rxCh.rxCompQueue.qMgr   = PPFW_CPPI4x_RX_HOST_QMGR;
    rxCh.rxCompQueue.qNum   = PPFW_CPPI4x_RX_HOST_QNUM((HOST2PP_INFRA_NUM-infra_chan));
    rxCh.u.embeddedPktCfg.fdQueue.qMgr = PPFW_CPPI4x_FD_QMGR;
    rxCh.u.embeddedPktCfg.fdQueue.qNum = PPFW_CPPI4x_FD_QNUM(infra_chan); /* CHK: Assuming higher Q num = higher pri */
    rxCh.u.embeddedPktCfg.numBufSlot = (EMSLOTCNT);
    rxCh.u.embeddedPktCfg.sopSlotNum = 0;
    rxCh.u.embeddedPktCfg.fBufPool[0].bMgr = BUF_POOL_MGR0;
    rxCh.u.embeddedPktCfg.fBufPool[0].bPool = 13;
    rxCh.u.embeddedPktCfg.fBufPool[1].bMgr = BUF_POOL_MGR0;
    rxCh.u.embeddedPktCfg.fBufPool[1].bPool = 3;
    rxCh.u.embeddedPktCfg.fBufPool[2].bMgr = BUF_POOL_MGR0;
    rxCh.u.embeddedPktCfg.fBufPool[2].bPool = 3;
    rxCh.u.embeddedPktCfg.fBufPool[3].bMgr = BUF_POOL_MGR0;
    rxCh.u.embeddedPktCfg.fBufPool[3].bPool = 3;
    cppi4RxChHnd        = PAL_cppi4RxChOpen(hnd, &rxCh, NULL);
    
    /* Set up Tx channel
     * CHK: For now using CPGMAC Tx comp Q for teardown.
     */
    txCh.chNum          = HOST2PP_INFRA_CHN(infra_chan);
    txCh.dmaNum         = PAL_CPPI41_DMA_BLOCK1;
    txCh.tdQueue.qMgr   = CPMAC_CPPI4x_TX_COMP_QMGR;
    txCh.tdQueue.qNum   = CPMAC_CPPI4x_TX_COMP_QNUM(0);
    cppi4TxChHnd        = PAL_cppi4TxChOpen(hnd, &txCh, NULL);

    if (!cppi4TxChHnd || !cppi4RxChHnd)
    {
        printk ("%s: infra channel setup failed for channel %d\n",
                __FUNCTION__, infra_chan);   
        return;
    }
    
    /* Enable Tx-Rx channels */
    PAL_cppi4EnableRxChannel (cppi4RxChHnd, NULL);
    PAL_cppi4EnableTxChannel (cppi4TxChHnd, NULL);

    return;
}

int avalanche_ppd_init(void)
{
    int i;
    PAL_Handle hnd, preFQHnd, preFBQHnd; 
    Uint32 pRcbMem;
    Cppi4EmbdDesc* currBD;
    APDSP_Command_Buffer_RegsOvly apdsp;
    Cppi4Queue tmpQ;    /* Just used for filling Q info for opening */
    Cppi4BufPool tmpBufPool; /* Used for Init calls */

#ifndef CONFIG_TI_PACKET_PROCESSOR
    if(ti_ppd_init(TI_PP_NUM_FW, &ppFirmware_g[0], &ppCfg_g) == 0) {
        printk("avalanche_ppd_init: PPD initialized successfully.\n");
    } else {
        printk("avalanche_ppd_init: ERROR: PPD failed to initialize!\n");
        return -1;
    }
#else
    extern int ti_pp_sys_initialize (unsigned int num_of_fw, 
                                        TI_PP_FIRMWARE *firmware, 
                                        TI_PPD_CONFIG *cfg);
    /* Initialize the PP Subsystem. */
    if (ti_pp_sys_initialize (TI_PP_NUM_FW, &ppFirmware_g[0], &ppCfg_g) < 0)
    {
        printk ("Error: Failed to initialize the PP Subsystem\n");
        return -1;
    }
    else
        printk("avalanche_ppd_init: PP system  initialized successfully.\n");
#endif

    hnd = PAL_cppi4Init (NULL, NULL);
  
    if (!hnd) {
        printk("avalanche_ppd_init: CPPI41 Library NOT initialized.\n");
        return -1;
    }

    /************************************************************************/
    /*********** Init buffer pools used in the system ***********************/
    /************************************************************************/
    /* Ethernet (& USB ?) */
    tmpBufPool.bMgr     = 0;
    tmpBufPool.bPool    = CPMAC_CPPI4x_POOL_NUM(0);
    if ((PAL_cppi4BufPoolInit(hnd, tmpBufPool,
                    BMGR0_POOL10_REF_CNT, BMGR0_POOL10_BUF_SIZE, BMGR0_POOL10_BUF_COUNT )) == NULL)
    {
        printk ("PAL_cppi4BufPoolInit for pool %d FAILED.\n",
                CPMAC_CPPI4x_POOL_NUM(0));
        return -1;
    }
    tmpBufPool.bPool    = CPMAC_CPPI4x_POOL_NUM(1);
    if ((PAL_cppi4BufPoolInit(hnd, tmpBufPool,
                    BMGR0_POOL11_REF_CNT, BMGR0_POOL11_BUF_SIZE, BMGR0_POOL11_BUF_COUNT )) == NULL)
    {
        printk ("PAL_cppi4BufPoolInit for pool %d FAILED.\n",
                CPMAC_CPPI4x_POOL_NUM(1));
        return -1;
    }

    /* Host2PP: Pool 13, 3 */
    tmpBufPool.bPool    = 13;
    if ((PAL_cppi4BufPoolInit(hnd, tmpBufPool, 
                              BMGR0_POOL13_REF_CNT, BMGR0_POOL13_BUF_SIZE, BMGR0_POOL13_BUF_COUNT )) == NULL)
    {
        printk ("PAL_cppi4BufPoolInit for pool 13 FAILED.\n");
        return -1;
    }

#ifdef TBD_HOST_TX_PROXY
    tmpBufPool.bPool    = 3;
    if ((PAL_cppi4BufPoolInit(hnd, tmpBufPool, 
                              BMGR0_POOL03_REF_CNT, BMGR0_POOL03_BUF_SIZE, BMGR0_POOL03_BUF_COUNT )) == NULL)
    {
        printk ("PAL_cppi4BufPoolInit for pool 3 FAILED.\n");
        return -1;
    }
#endif

    /* DOCSIS: Pool 4 */
    tmpBufPool.bMgr     = 0;
    tmpBufPool.bPool    = CNI_CPPI4x_POOL_NUM(0);
    if ((PAL_cppi4BufPoolInit(hnd, tmpBufPool,
                    BMGR0_POOL04_REF_CNT, BMGR0_POOL04_BUF_SIZE, BMGR0_POOL04_BUF_COUNT )) == NULL)
    {
        printk ("PAL_cppi4BufPoolInit for pool %d FAILED.\n", CNI_CPPI4x_POOL_NUM(0));
        return -1;
    }

    /******************** Buffer pool Init done *****************************/

    /************************************************************************/
    /*********** Setup Embedded descriptors  *************************/
    /************************************************************************/
    ppEmbBDCfg_g.embBDRgnPtr = PAL_cppi4AllocDesc (hnd, ppEmbBDCfg_g.qMgr, 
                                                    ppEmbBDCfg_g.numDesc, 
                                                    ppEmbBDCfg_g.szDesc);
    if (!ppEmbBDCfg_g.embBDRgnPtr) {
        printk ("Embedded descriptor region allocation FAILED.\n");
        return -1;
    }
    
    currBD = (Cppi4EmbdDesc*)ppEmbBDCfg_g.embBDRgnPtr;
    for (i = 0; i < ppEmbBDCfg_g.numBlks; i++)
    {
        EmbBDBlkInfo* embBDBlk = &ppEmbBDCfg_g.embBDBlk[i];
        int bd_cnt;
        PAL_Cppi4QueueHnd tmpQHnd; 
        
        tmpQ.qMgr = ppEmbBDCfg_g.qMgr;
        tmpQ.qNum = embBDBlk->qNum;
        tmpQHnd = PAL_cppi4QueueOpen (hnd, tmpQ);
        for (bd_cnt = 0; bd_cnt < embBDBlk->numDesc; bd_cnt++)
        {
            currBD->descInfo    = CPPI41_EM_DESCINFO_DTYPE_EMBEDDED 
                                    | CPPI41_EM_DESCINFO_SLOTCNT_MYCNT;
            currBD->tagInfo     = 0;
            currBD->pktInfo     = (embBDBlk->pktType<< CPPI41_EM_PKTINFO_PKTTYPE_SHIFT) 
                | (1 << CPPI41_EM_PKTINFO_RETPOLICY_SHIFT)
                | (1 << CPPI41_EM_PKTINFO_PROTSPEC_SHIFT)
                | (ppEmbBDCfg_g.qMgr << PAL_CPPI4_HOSTDESC_PKT_RETQMGR_SHIFT)
                | (embBDBlk->qNum << PAL_CPPI4_HOSTDESC_PKT_RETQNUM_SHIFT);
            PAL_CPPI4_CACHE_WRITEBACK(currBD, ppEmbBDCfg_g.szDesc);
            PAL_cppi4QueuePush(tmpQHnd, (Ptr)PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD), 
                                (ppEmbBDCfg_g.szDesc-24)/4, 
                                0/*!@@*/);
            currBD = (Cppi4EmbdDesc*)((Uint32)currBD + ppEmbBDCfg_g.szDesc);
        }
    }
    /********************** Embedded desc setup Done ******************/

#ifdef TBD_HOST_TX_PROXY
    /*********** Open the Qs common for devices in PP system 
     * Actually the queues only need to be opened here if the handle is required
     * (for push/pop generally) and any driver depending on these Qs would do
     * so. Still opening here to provide idea of the system.
     * Also, opening these queue here would mean that any subsequent calls (from
     * drivers) to open these queues will just return the same handle to the
     * queue without resetting them
     */
    /************************************************************************/
    /* Open Tx Qs. These will be used by drivers to push for Tx. */
    /* Infra Qs */
    tmpQ.qMgr   = HOST2P_INFRA_RX_QMGR;
    tmpQ.qNum   = HOST2PP_INFRA_RX_Q(0);
    PAL_cppi4QueueOpen (hnd, tmpQ);
    setup_host2pp_infra (hnd, 0);
    tmpQ.qNum   = HOST2PP_INFRA_RX_Q(1);
    PAL_cppi4QueueOpen (hnd, tmpQ);
    setup_host2pp_infra (hnd, 1);
#endif

    /****************************************************************/
    /*  Set Up the PP Ingress queues                                */
    /****************************************************************/
    tmpQ.qMgr   = PPFW_CPPI4x_RX_HOST_QMGR;

    for (i = 0; PPFW_CPPI4x_RX_HOST_Q_COUNT > i; i++)
    {
        tmpQ.qNum   = PPFW_CPPI4x_RX_HOST_QNUM(i);
        PAL_cppi4QueueOpen (hnd, tmpQ);
    }

    /************************************************************************/
    
   /*****************************************************************
     ******************* Setup Prefetcher ***************************
     ****************************************************************/

    /* Push Prefetcher free desc */
    pRcbMem = (Uint32)prefCfg_g.pfDescBase;
    preFQHnd = PAL_cppi4QueueOpen (hnd, prefCfg_g.pfFQ);
    for( i=0; i<prefCfg_g.pfDescCnt; i++ ) {
        PAL_cppi4QueuePush (preFQHnd, (Ptr) pRcbMem, 0, 0);
        pRcbMem += PREFETCH_FD_SIZE;    // Next buffer descriptor
    }

    /* Push replicator free desc */
    pRcbMem = (Uint32)prefCfg_g.repDescBase;
    preFQHnd = PAL_cppi4QueueOpen (hnd, prefCfg_g.repFQ);
    for( i=0; i<prefCfg_g.repDescCnt; i++ ) {
        PAL_cppi4QueuePush (preFQHnd, (Ptr) pRcbMem, 0, 0);
        pRcbMem += PREFETCH_FD_SIZE;    // Next buffer descriptor
    }

    /* Push event free desc 
     * TODO: Currently using onchip, may move to offchip
     */
    if (ppCfg_g.host_ev_queue) {
        PAL_Cppi4QueueHnd tmpQHnd; 
        pRcbMem = (Uint32)EVENT_DESC_BASEADDR;
        tmpQ.qNum   = PP_HOST_EVENT_FDQ;
        tmpQ.qMgr    = PP_HOST_EVENT_FDQMGR;
        tmpQHnd = PAL_cppi4QueueOpen (hnd, tmpQ);
        for( i=0; i<PPFW_EVENT_DESC_NUM; i++ ) {
            PAL_CPPI4_CACHE_INVALIDATE(IO_PHY2VIRT(pRcbMem), sizeof(Cppi4EmbdDesc));
            PAL_cppi4QueuePush (tmpQHnd, (Ptr) pRcbMem, (sizeof(Cppi4EmbdDesc)-24)/4, 0);
            pRcbMem += 64;    // Next buffer descriptor
        }
    }

    /* Setup prefetch buffers */
    pRcbMem = (Uint32)prefCfg_g.pfBlkBase;
    preFBQHnd = PAL_cppi4QueueOpen (hnd, prefCfg_g.pfFBQ);
    for( i=0; i<prefCfg_g.pfBlkCnt; i++ )
    {
        PAL_cppi4QueuePush (preFBQHnd, (Ptr) pRcbMem, 0, 0);
        pRcbMem += 128;                  // Next buffer descriptor
    }

#define ACMD_CONFIG_PREFETCH        0x84
#define ACMD_ENABLE_PREFETCH        0x83
#define ACMD_CONFIG_TDQ             0x87
#define ACMD_COMMAND_SHIFT          8
#define ACMD_COMMAND_MASK           (0xFFu<<ACMD_COMMAND_SHIFT)
#define ACMD_COMMAND(x)             (((x)<<ACMD_COMMAND_SHIFT)&ACMD_COMMAND_MASK)
#define ACMD_GET_COMMAND(x)         (((x)&ACMD_COMMAND_MASK)>>ACMD_COMMAND_SHIFT)
#define ACMD_INDEX_SHIFT            0
#define ACMD_INDEX_MASK             (0xFFu<<ACMD_INDEX_SHIFT)
#define ACMD_INDEX(x)               (((x)<<ACMD_INDEX_SHIFT)&ACMD_INDEX_MASK)
#define ACMD_RETCODE_SHIFT          24
#define ACMD_RETCODE_MASK           (0xFFu<<ACMD_RETCODE_SHIFT)
#define ACMD_GET_RETCODE(x)         (((x)&ACMD_RETCODE_MASK)>>ACMD_RETCODE_SHIFT)
    

    apdsp = prefCfg_g.pfCmdBase;

    //--------------------------------------------------------
    // Config Prefetcher
    //
    // Setup prefetch to fetch 96 bytes with a 24 byte offset
    //
    apdsp->Parameter0 = (24<<8) | 96;
    apdsp->Command = ACMD_COMMAND(ACMD_CONFIG_PREFETCH);

    for( i=0; i<1000000 && ACMD_GET_COMMAND(apdsp->Command); i++);
    if( i==1000000 ) { 
        printk("%s(%d): Error - APDSP firmware not responding!\n", 
                __FUNCTION__, __LINE__); 
        return -1; 
    }

    if (ACMD_GET_RETCODE(apdsp->Command) != 1)
        printk("%s(%d):return code 0x%02x\n", 
                __FUNCTION__, __LINE__, ACMD_GET_RETCODE(apdsp->Command));
    
    //--------------------------------------------------------
    //
    // Setup Teardown descriptor queue
    // Note:  The prefetcher is configured to route the teardown
    // descriptors back to free teardown desc queue. This avoids prefetcher
    // going into invalid state when TD is received. 
    // CHI: __IMPORTANT__ This configuration constraints ALL PP related Endpoint
    // Rx channels to be configured to use SAME Teardown Queue otherwise TDs
    // will be lost (since the queue number specified in DMA configuration to
    // pick the TD and Queue number for Prefetcher to queue the TD will be
    // different).
    //
    apdsp->Parameter0 = (DMA0_CPPI4x_FTD_QMGR<<16) | DMA0_CPPI4x_FTD_QNUM;
    apdsp->Command = ACMD_COMMAND(ACMD_CONFIG_TDQ);

    for( i=0; i<1000000 && ACMD_GET_COMMAND(apdsp->Command); i++);
    if( i==1000000 ) { 
        printk("%s(%d): Error - APDSP firmware not responding!\n", 
                __FUNCTION__, __LINE__); 
        return -1; 
    }

    if (ACMD_GET_RETCODE(apdsp->Command) != 1)
        printk("%s(%d):return code 0x%02x\n", 
                __FUNCTION__, __LINE__, ACMD_GET_RETCODE(apdsp->Command));


    //--------------------------------------------------------
    // Enable Prefetcher
    //
    // Enable prefetcher
    //
    apdsp->Command = ACMD_INDEX(1) |
        ACMD_COMMAND(ACMD_ENABLE_PREFETCH);

    for( i=0; i<1000000 && ACMD_GET_COMMAND(apdsp->Command); i++);
    if( i==1000000 ) {
        printk("%s(%d): Error - APDSP firmware not responding!\n", 
                __FUNCTION__, __LINE__); 
        return -1; 
    }

    if (ACMD_GET_RETCODE(apdsp->Command) != 1)
        printk("%s(%d):return code 0x%02x\n", 
                __FUNCTION__, __LINE__, ACMD_GET_RETCODE(apdsp->Command));
    /*********** Prefetcher setup done **************************/

    return 0;
}

int avalanche_ppd_deinit (void)
{
    PAL_Handle hnd;
    hnd = PAL_cppi4Init (NULL, NULL);
    if (!hnd) {
        printk("avalanche_ppd_deinit: CPPI41 Library NOT initialized.\n");
        return -1;
    }
    
    if (ppEmbBDCfg_g.embBDRgnPtr)
        PAL_cppi4DeallocDesc (hnd, ppEmbBDCfg_g.qMgr, ppEmbBDCfg_g.embBDRgnPtr);

#ifdef CONFIG_TI_PACKET_PROCESSOR
    ti_ppm_deinitialize ();
#else
    ti_ppd_exit ();
#endif

    return 0;
}

subsys_initcall(avalanche_ppd_init);
EXPORT_SYMBOL(ti_ppd_init);
EXPORT_SYMBOL(ti_ppd_exit);

