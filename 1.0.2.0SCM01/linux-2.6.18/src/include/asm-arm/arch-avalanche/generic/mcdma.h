/*******************************************************************************
**+--------------------------------------------------------------------------+**
**|                            ****                                          |**
**|                            ****                                          |**
**|                            ******o***                                    |**
**|                      ********_///_****                                   |**
**|                      ***** /_//_/ ****                                   |**
**|                       ** ** (__/ ****                                    |**
**|                           *********                                      |**
**|                            ****                                          |**
**|                            ***                                           |**
**|                                                                          |**
**|         Copyright (c) 1998-2005 Texas Instruments Incorporated           |**
**|                        ALL RIGHTS RESERVED                               |**
**|                                                                          |**
**|                                                                          |**
**+--------------------------------------------------------------------------+**
*******************************************************************************/

/******************************************************************************
 * FILE PURPOSE:    Multi Channel Direct Memory Access (MC-DMA) Header
 ********************************************************************************
 * FILE NAME:       mcdma.h
 *
 * DESCRIPTION:     Platform and OS independent API for MCDMA Controller
 *
 * REVISION HISTORY:
 * 27 Nov 02 - PSP TII  
 *
 *******************************************************************************/

#ifndef __MCDMA_H__
#define __MCDMA_H__


typedef enum MCDMA_CHANNEL_tag
{
    MCDMA_CHANNEL_0 = 0,
    MCDMA_CHANNEL_1,
    MCDMA_CHANNEL_2,
    MCDMA_CHANNEL_3,
    
} MCDMA_CHANNEL_T;

typedef enum MCDMA_CTRL_tag
{
    MCDMA_STOP = 0,
    MCDMA_START
    
} MCDMA_CTRL_T;

typedef enum MCDMA_ADDR_MODE_tag
{
    MCDMA_INCREMENTING = 0,
    MCDMA_FIXED = 2
    
} MCDMA_ADDR_MODE_T;

typedef enum MCDMA_BURST_MODE_tag
{
    MCDMA_1_WORD_BURST = 0,
    MCDMA_2_WORD_BURST = 1,
    MCDMA_4_WORD_BURST = 2,
    
} MCDMA_BURST_MODE_T;


void mcdma_init(UINT32 base_addr);
void mcdma_control(MCDMA_CHANNEL_T mcdma_ch, MCDMA_CTRL_T mcdma_ctrl);
INT32 mcdma_setdmaparams (MCDMA_CHANNEL_T mcdma_ch,
                          UINT32 src_addr, 
                          UINT32 dst_addr, 
                          UINT32 length, 
                          MCDMA_BURST_MODE_T burst_mode, 
                          MCDMA_ADDR_MODE_T src_addr_mode,
                          MCDMA_ADDR_MODE_T dst_addr_mode);

#endif /* __MCDMA_H__ */
