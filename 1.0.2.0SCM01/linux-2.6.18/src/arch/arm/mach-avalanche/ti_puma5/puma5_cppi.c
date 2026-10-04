/*
 *
 * puma5_cppi.c
 * Description:
 * Puma-5 CPPI initialization.
 * Contains Puma-5 specific initialization. The numbers (addresses etc) are
 * Puma-5 specific. The static structures are filled in with Puma-5 specific
 * data and the generic CPPI4.1 init function gets called in the end
 * with this data.
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
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
 */

#include <pal.h>

#ifdef EL
#include <precum_le_bin.h>
#else
#include <precum_be_bin.h>
#endif

/* FIXME : uncomment line below to enable debug prints */
//#define DEBUG_PAL(fmt,arg...)  printk(KERN_EMERG fmt , ##arg);

#ifndef DEBUG_PAL
#define DEBUG_PAL(fmt,arg...)
#endif


/* This CPPI 4.1 global initialization structure. Contains static initialization
 * information for the whole CPPI subsystem. Details regarding structure fields
 * can be obtained from PAL documentation. Some comments on how the structure
 * can be populated/extended are thrown here as well.
 * 
 * *** IMPORTANT *** *** ALWAYS REMEMBER *** *** FOLLOW THESE RULES ***
 *
 * - Keep all base addresses of descriptor regions in ascending order.
 *   Where descriptor is allocated by CPPI from SDRAM, the base here is 
 *   specified as zero. Treat the zero as SDRAM base address while ensuring
 *   the ascending order. Keep all desc regiosns meant to be on SDRAM
 *   together. 
 *
 * - Sort the desc regions defined on SDRAM (base == 0) in descending order of
 *   desc size. This is required to ensure easy alignment (bit 14 aligned to 
 *   bit 15 and so on.)
 *
 * - Apply these rules to each queue manager in the system *separately*.
 *
 * - Read the PAL CPPI documentation (especially the Cppi4InitCfg structure doc)
 *   before starting to modify this structure.
 *
 */

Cppi4InitCfg cppi4InitCfg_g = {
    .resetLine              = 0,
    
    .dmaBlock[0].globalCtrlBase     = (Ptr) AVALANCHE_NWSS_DMA0_GBLCFG_BASE,//changed from DMA1 to DMA0
    .dmaBlock[0].chCtrlStatusBase   = (Ptr) AVALANCHE_NWSS_DMA0_CHNCFG_BASE,
    .dmaBlock[0].schedCtrlBase      = (Ptr) AVALANCHE_NWSS_DMA0_SCHEDCFG_BASE,
    .dmaBlock[0].schedTableBase     = (Ptr) AVALANCHE_NWSS_DMA0_SCHEDTBL_BASE,
    
    .dmaBlock[0].tdFQueue.qMgr      = DMA0_CPPI4x_FTD_QMGR,
    .dmaBlock[0].tdFQueue.qNum      = DMA0_CPPI4x_FTD_QNUM,
  
    .dmaBlock[1].globalCtrlBase     = (Ptr) AVALANCHE_NWSS_DMA1_GBLCFG_BASE,
    .dmaBlock[1].chCtrlStatusBase   = (Ptr) AVALANCHE_NWSS_DMA1_CHNCFG_BASE,
    .dmaBlock[1].schedCtrlBase      = (Ptr) AVALANCHE_NWSS_DMA1_SCHEDCFG_BASE,
    .dmaBlock[1].schedTableBase     = (Ptr) AVALANCHE_NWSS_DMA1_SCHEDTBL_BASE,

    .dmaBlock[1].tdFQueue.qMgr      = DMA1_CPPI4x_FTD_QMGR,
    .dmaBlock[1].tdFQueue.qNum      = DMA1_CPPI4x_FTD_QNUM,

 
    .queueMgrInfo[0].queueMgrRgnBase        = (Ptr) AVALANCHE_NWSS_QMGR_RGN_BASE, 
    .queueMgrInfo[0].descMemRgnBase         = (Ptr) AVALANCHE_NWSS_DESCMEM_RGN_BASE,     
    .queueMgrInfo[0].queueMgmtRgnBase       = (Ptr) AVALANCHE_NWSS_QMGMT_RGN_BASE,
    .queueMgrInfo[0].queueStatusRgnBase     = (Ptr) AVALANCHE_NWSS_QSTATUS_RGN_BASE,
    
    .queueMgrInfo[0].LinkingRAM0Base        = IO_VIRT2PHY(AVALANCHE_NWSS_PACKET_RAM_BASE), 
    .queueMgrInfo[0].LinkingRAM0Size        = 5000, /* Max pointers in this
                                                       region could be 7K
                                                       (assuming 32-bit
                                                       pointers), but currently
                                                       allocating 5k to be on
                                                       safer side, till
                                                       confirmed from h/w team.
                                                       */
    .queueMgrInfo[0].LinkingRAM1Base        = 0,

    .queueMgrInfo[0].descRegion[0].base     = (Ptr) 0x01119000,   /* Docsis MAC internal memory */
    .queueMgrInfo[0].descRegion[0].szDesc   = 128, 
    .queueMgrInfo[0].descRegion[0].numDesc  = 32,
    .queueMgrInfo[0].descRegion[0].isOnChip = 1,    

    .queueMgrInfo[0].descRegion[1].base     = (Ptr) PPFW_CPPI4x_BD_BASE,   /* Onchip */
    .queueMgrInfo[0].descRegion[1].szDesc   = PPFW_CPPI4x_BD_SIZE, 
    .queueMgrInfo[0].descRegion[1].numDesc  = PREFETCH_DESC_COUNT+PPFW_BD_NUM+PPFW_EVENT_DESC_NUM, /* Total BD Count: Prefetcher + Replication + Events */
    .queueMgrInfo[0].descRegion[1].isOnChip = 1,    
    
    .queueMgrInfo[0].descRegion[2].base     = (Ptr) PPFW_CPPI4x_BUF_BD_BASE,   /* Prefetch */
    .queueMgrInfo[0].descRegion[2].szDesc   = PPFW_CPPI4x_BUF_BD_SIZE, 
    .queueMgrInfo[0].descRegion[2].numDesc  = PREFETCH_BUFF_COUNT,
    .queueMgrInfo[0].descRegion[2].isOnChip = 1,

	/* Reserve region with following specifics for Embedded */
    .queueMgrInfo[0].descRegion[3].base     = 0,
    .queueMgrInfo[0].descRegion[3].szDesc   = 64,
    .queueMgrInfo[0].descRegion[3].numDesc  = 512 /* !@@ Use as per PP system req */,

    .queueMgrInfo[0].descRegion[4].base     = 0,
    .queueMgrInfo[0].descRegion[4].szDesc   = USB_CPPI4x_USB_BD_SIZE, 
    .queueMgrInfo[0].descRegion[4].numDesc  = USB_CPPI4x_MAX_USB_DESC, 

    .queueMgrInfo[0].descRegion[5].base     = 0,
    .queueMgrInfo[0].descRegion[5].szDesc   = CPMAC_CPPI4x_TX_HOST_BD_SIZE, /* sizeof (Cppi4HostDesc) + alignment */
    .queueMgrInfo[0].descRegion[5].numDesc  = CPMAC_TX_HOST_BD_NUM,

    .queueMgrInfo[0].descRegion[6].base     = 0,
    .queueMgrInfo[0].descRegion[6].szDesc   = CPMAC_CPPI4x_RX_HOST_BD_SIZE,
    .queueMgrInfo[0].descRegion[6].numDesc  = CPMAC_RX_HOST_BD_NUM,

    .queueMgrInfo[0].descRegion[7].base     = 0,
    .queueMgrInfo[0].descRegion[7].szDesc   = 64, /* 64 byte sized */
    .queueMgrInfo[0].descRegion[7].numDesc  = 64,    

    .queueMgrInfo[0].descRegion[8].base     = 0,
    .queueMgrInfo[0].descRegion[8].szDesc   = 64, /* 64 byte sized */
    .queueMgrInfo[0].descRegion[8].numDesc  = 64,    

    .queueMgrInfo[0].descRegion[9].base     = 0,
    .queueMgrInfo[0].descRegion[9].szDesc   = 64, /* 64 byte sized */
    .queueMgrInfo[0].descRegion[9].numDesc  = 256, //512,    

    .queueMgrInfo[0].descRegion[10].base     = 0,
    .queueMgrInfo[0].descRegion[10].szDesc   = sizeof(Cppi4TeardownDesc), /* 32 byte sized */
    .queueMgrInfo[0].descRegion[10].numDesc  = PAL_CPPI41_NUM_TD_DESC,

    .queueMgrInfo[0].descRegion[11].base     = 0,
    .queueMgrInfo[0].descRegion[11].szDesc   = sizeof(Cppi4TeardownDesc), /* 32 byte sized */
    .queueMgrInfo[0].descRegion[11].numDesc  = PAL_CPPI41_NUM_TD_DESC,

    .queueMgrInfo[0].descRegion[12].base     = 0,
    .queueMgrInfo[0].descRegion[12].szDesc   = 64,
    .queueMgrInfo[0].descRegion[12].numDesc  = DMAC_CNI_RX_HOST_BD_NUM_LO,  /* cable_pp should be LO+MD+HI */  

    .queueMgrInfo[0].descRegion[13].base     = 0,
    .queueMgrInfo[0].descRegion[13].szDesc   = 64,
    .queueMgrInfo[0].descRegion[13].numDesc  = DMAC_CNI_RX_EMBEDDED_BD_NUM_LO, /* cable_pp should be LO+MD+HI */ 

    .queueMgrInfo[0].basefdQNum             = PAL_CPPI41_FD_Q_BASE,
    .queueMgrInfo[0].basefdbQNum            = PAL_CPPI41_FBD_Q_BASE,    


    .bufMgrBase[0]                          = (CSL_BufMgr_RegsOvly) AVALANCHE_NWSS_BMGR_BASE,

    .dmaBlock[0].schedTable.numEntries  = 2,
    .dmaBlock[0].schedTable.entry	= { PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,8),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,8),
                                            },

    .dmaBlock[1].schedTable.numEntries  = 32,
    .dmaBlock[1].schedTable.entry       = { PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,21),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,21),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,4),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,4),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,5),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,5),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,6),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,6),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,7), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,7),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,9), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,9),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,10), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,10),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,11), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,11),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,12), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,12),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,13), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,13),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,14), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,14),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,15), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,15),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,16), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,16),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,17), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,17),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,18), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,18),
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_RX_CH,19), 
                                            PAL_CPPI41_DMA_CH_CONFIG(PAL_CPPI41_DMA_TX_CH,19),
                                            },

#ifdef EL
    .apdspInfo.firmwareBase     = (Uint32*) &PDSPcode_precum_le_0001[0],
    .apdspInfo.firmwareSize     = sizeof (PDSPcode_precum_le_0001),
#else
    .apdspInfo.firmwareBase     = (Uint32*) &PDSPcode_precum_be_0001[0],
    .apdspInfo.firmwareSize     = sizeof (PDSPcode_precum_be_0001),
#endif
    .apdspInfo.iRamBase         = (Uint32*) AVALANCHE_NWSS_APDSP_IRAM_RGN_BASE,
    .apdspInfo.pdspCtrlBase     = (Uint32*) AVALANCHE_NWSS_APDSP_CTRL_RGN_BASE,
    .apdspInfo.pdspCmdBase      = (Ptr) AVALANCHE_NWSS_APDSP_CMD_BASE, 
};

int avalanche_cppi_init(void)
{
    if(PAL_cppi4Init(&cppi4InitCfg_g, NULL) != NULL) {
        DEBUG_PAL("PAL_cppi4Init: CPPI 4.1 API initialized successfully.\n");
    } else {
        DEBUG_PAL("PAL_cppi4Init: ERROR: CPPI 4.1 API initialization failed!\n");
    }

    return 0;
}

