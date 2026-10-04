/*
 * This file contains Packet Processor configuration. All the configurations
 * here are system level and hence a complete knowledge of configurations of all
 * devices/drivers related to the Packet Processor is required before modifying
 * any value.
 *
 * The private data structures used by avalanche_ppd_init() are also present in
 * this file and should not be modified unless the function avalanche_ppd_init
 * in file puma5_pp.c is also required to be modified for PP configuration.
 * 
 * Copyright (C) Texas Instruments, Inc. 2007
 */

#ifndef _INCLUDE_PUMA5_PP_H
#define _INCLUDE_PUMA5_PP_H


/* Queue configuration for PP --> Host Envets */
#define PP_HOST_EVENTQMGR       0
#define PP_HOST_EVENTQ          116   
#define PP_HOST_EVENT_FDQMGR    0
#define PP_HOST_EVENT_FDQ       117
#define PPFW_EVENT_DESC_NUM     255 /*MOTO : cqvsd00016870 - thmq36 - TI fix for FTP throughput drop issue */

/* Prefetcher configuration 
 *  - Setup prefetch to fetch 96 bytes with a 24 byte offset
 */
#define PREFETCH_DATA_SIZE          96
#define PREFETCH_DATA_OFFSET        24

#define PREFETCH_FD_SIZE            64
#define PREFETCH_BUFF_SIZE          128

/*
 * PID Range configurations
 *  -   The following macros cover the complete PID range organized for PP
 *      system and needs to be obeyed by all the drivers in PP system.
 *  -   Currently only Ethernet configuration is present but new configurations
 *      (ranges) need to be added per newly added driver in PP system
 */
#define PP_ETH_PID_COUNT    1
#define PP_ETH_PID_BASE     31

#define PP_CNI_PID_COUNT    1
#define PP_CNI_PID_BASE     9


/* 
 * Data structures used to hold and perform various PP configurations 
 */
typedef volatile struct
{
    CSL_Reg32 Command;
    CSL_Reg32 Parameter0;
    CSL_Reg32 Parameter1;
    CSL_Reg32 Parameter2;
    CSL_Reg32 Parameter3;
} APDSP_Command_Buffer;

typedef APDSP_Command_Buffer *APDSP_Command_Buffer_RegsOvly;

/*
 * PrefchCfg -
 * Description:
 *  Prefetcher Configuration
 */
typedef struct
{
    Uint32 pfDescCnt;
    volatile Uint32* pfDescBase;
    Cppi4Queue pfFQ;
    
    Uint32 repDescCnt;
    volatile Uint32* repDescBase;
    Cppi4Queue repFQ;
    
    Uint32 pfBlkCnt;
    volatile Uint32* pfBlkBase;
    Cppi4Queue pfFBQ;

    Uint8 pfDataSize;
    Uint8 pfDataOffset;
    APDSP_Command_Buffer_RegsOvly  pfCmdBase;
} PrefchCfg;

#endif /* !_INCLUDE_PUMA5_PP_H */
