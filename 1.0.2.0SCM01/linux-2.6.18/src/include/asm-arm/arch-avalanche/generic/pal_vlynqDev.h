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
**+--------------------------------------------------------------------------+**
*******************************************************************************/

/******************************************************************************
 * FILE PURPOSE:    PAL VLYNQ Device Header File
 ******************************************************************************
 * FILE NAME:       pal_vlynqDev.h
 *
 * DESCRIPTION:     PAL VLYNQ Device Header File
 *
 *******************************************************************************/

#ifndef __PAL_VLYNQ_DEV_H__
#define __PAL_VLYNQ_DEV_H__

/* The vlynq dev handle. */
typedef void PAL_VLYNQ_DEV_HND;

#include "pal_vlynq.h"

#define PAL_VLYNQ_EVENT_LOCAL_ERROR 0
#define PAL_VLYNQ_EVENT_PEER_ERROR  1

#define PAL_VLYNQ_DEV_ADD_IRQ       0
#define PAL_VLYNQ_DEV_REMOVE_IRQ    1

typedef Int32 (*PAL_VLYNQ_DEV_CB_FN)(void*, Uint32 condition, Uint32 value);

PAL_Result PAL_vlynqDevCbRegister(PAL_VLYNQ_DEV_HND   *vlynq_dev, 
	   	                     PAL_VLYNQ_DEV_CB_FN cb_fn,
				     void                *this_driver);

PAL_Result PAL_vlynqDevCbUnregister(PAL_VLYNQ_DEV_HND *vlynq_dev,
     		                       void              *this_driver);

PAL_VLYNQ_DEV_HND *PAL_vlynqDevFind(char *name, Uint8 instance);

PAL_VLYNQ_HND *PAL_vlynqDevGetVlynq(PAL_VLYNQ_DEV_HND *vlynq_dev);

PAL_Result PAL_vlynqDevFindIrq(PAL_VLYNQ_DEV_HND *vlynq_dev, Uint8 *irq, 
		                  Uint32 num_irq);

PAL_Result PAL_vlynqDevGetResetBit(PAL_VLYNQ_DEV_HND *vlynq_dev, 
                                   Uint32 *reset_bit);
           
PAL_VLYNQ_DEV_HND* PAL_vlynqDevCreate(PAL_VLYNQ_HND *vlynq, char *name,
					Uint32 instance, Int32 reset_bit,
					Bool peer);

/* Protected API(s) to be used only by pal_vlynq.c */
PAL_Result pal_vlynq_dev_init(void);

PAL_Result pal_vlynq_dev_handle_event(PAL_VLYNQ_DEV_HND *vlynq_dev,
                                       Uint32 cmd, Uint32 val);

PAL_Result pal_vlynq_dev_ioctl(PAL_VLYNQ_DEV_HND *vlynq_dev, Uint32 cmd, 
		               Uint32 cmd_val);

PAL_Result PAL_vlynqDevDestroy(PAL_VLYNQ_DEV_HND *vlynq_dev);

#endif
