/*
 *
 * puma5_cppi.h
 * Description:
 * File containing CPPI configurations for each driver.
 * Put into a single file to (hopefully) avoid configuration
 * clashes.
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


#ifndef __PUMA_CPPI_H__
#define __PUMA_CPPI_H__

#include "puma5_cppi_prv.h"


/* Global public */
#define PAL_CPPI_TOTAL_BD_NUM_INTERNAL      2048
#define PAL_CPPI_TOTAL_BD_NUM_64SIZE        4096
#define PAL_CPPI_TOTAL_BD_NUM_128SIZE          0
#define PAL_CPPI_TOTAL_BD_NUM_256SIZE          0


/* CPMAC public */
#define CPMAC_RX_EMBEDDED_BD_NUM            64
#define CPMAC_RX_HOST_BD_NUM                64
#define CPMAC_TX_HOST_BD_NUM                64
/* legacy definitions */
#define CPMAC_RX_BD_NUM                     CPMAC_RX_EMBEDDED_BD_NUM
#define CPMAC_TX_BD_NUM                     CPMAC_TX_HOST_BD_NUM                     

/* DMAC public */                                           
#define DMAC_US_BD_NUM                      32
#define DMAC_MNG_RX_HOST_BD_NUM             32
#define DMAC_MNG_TX_HOST_BD_NUM             32
#define DMAC_CNI_RX_EMBEDDED_BD_NUM_HI      64
#define DMAC_CNI_RX_EMBEDDED_BD_NUM_MD      16 /* 15 changed to 16 since right now cppi support 1 desc regions per BD allocation */
#define DMAC_CNI_RX_EMBEDDED_BD_NUM_LO     256
#define DMAC_CNI_RX_HOST_BD_NUM_HI          64
#define DMAC_CNI_RX_HOST_BD_NUM_MD          16  /* 17 changed to 16 since right now cppi support 1 desc regions per BD allocation */
#define DMAC_CNI_RX_HOST_BD_NUM_LO         512
#define DMAC_CNI_TX_HOST_BD_NUM_HI           0
#define DMAC_CNI_TX_HOST_BD_NUM_ML         256

#define BMGR0_POOL00_BUF_COUNT                 8192 
#define BMGR0_POOL01_BUF_COUNT                 8192
#define BMGR0_POOL02_BUF_COUNT                 8192
#define BMGR0_POOL03_BUF_COUNT                 8192
#define BMGR0_POOL04_BUF_COUNT                 2048
#define BMGR0_POOL05_BUF_COUNT                 2048
#define BMGR0_POOL06_BUF_COUNT                 2048
#define BMGR0_POOL07_BUF_COUNT                 2048
#define BMGR0_POOL08_BUF_COUNT                 2048
#define BMGR0_POOL09_BUF_COUNT                 2048
#define BMGR0_POOL10_BUF_COUNT                 1024
#define BMGR0_POOL11_BUF_COUNT                 512
#define BMGR0_POOL12_BUF_COUNT                 1024
#define BMGR0_POOL13_BUF_COUNT                 1024

/* PP-FW public */
#define PPFW_BD_NUM                         128
#define PPFW_BUF_BD_NUM                     24
#define PPFW_EVENT_DESC_NUM                 255 /*MOTO : cqvsd00016870 - thmq36 - TI fix for FTP throughput drop issue */

/* legacy definitions */
#define PREFETCH_DESC_COUNT                 PPFW_BD_NUM
#define PREFETCH_BUFF_COUNT                 PPFW_BUF_BD_NUM



/* CPPI platform abstractions. Ideally should have been PAL */
void* cppi_alloc_aligned_mem(unsigned int size);
void cppi_free_aligned_mem(void* addr, unsigned int size);

#define PAL_CPPI4_CACHE_INVALIDATE(addr, size)  dma_cache_inv ((unsigned long)(addr), (size))
#define PAL_CPPI4_CACHE_WRITEBACK(addr, size)   dma_cache_wback ((unsigned long)(addr), (size))
#define PAL_CPPI4_CACHE_WRITEBACK_INVALIDATE(addr, size)   dma_cache_wback_inv ((unsigned long)(addr), (size))

#endif /* __PUMA_CPPI_H__ */ 
