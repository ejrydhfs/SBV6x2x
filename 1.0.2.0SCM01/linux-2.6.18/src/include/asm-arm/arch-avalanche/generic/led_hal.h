/*
 *
 * led_hal.h
 * Description:
 * see below
 *
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


/******************************************************************************   
 * FILE PURPOSE:     - LED kernel Header                                           
 ******************************************************************************   
 * FILE NAME:     led_hal.h                                                     
 *                                                                                
 * DESCRIPTION:   Header file defining HAL types and functions.
 *                                                                                
 *                                                                                
 *******************************************************************************/ 


#ifndef _LED_HAL_H_
#define _LED_HAL_H_

#include "_tistdtypes.h"

#define MAX_STATES_PER_MOD 25

/*---------------------------------------------------------------------------
 * Configuration Support.
 *-------------------------------------------------------------------------*/
typedef struct led_cfg
{
    Uint32 domain;
    Uint32 pos_map[2]; /* Indicates the position of LED bit; index 0 -> range of 0 - 31, index 1 -> range of 32 - 63 */
} LED_CFG_T;

typedef struct state_cfg
{
    Uint32 id;
    Uint32 mode;
    Uint32 param1;
    Uint32 param2;
    Uint32 led_val[2];
    LED_CFG_T    led_cfg;
} STATE_CFG_T;

typedef struct mod_cfg
{
    Int8          name[50];
    Uint32  instance;
    STATE_CFG_T   state_cfg;

} MOD_CFG_T;

typedef struct led_funcs
{
    Uint32 domain;
    Uint32 pos_map[2];  /* Indicates the position of LED bit; index 0 -> range of 0 - 31, index 1 -> range of 32 - 63 */
    Uint32 off_val[2]; /* Init values to be reflected on the LED(s). */
    Int32 (*outVal)(Uint32 led_val, Uint32 pos_map, Uint32 index);

} LED_FUNCS_T;

/*--------------------------------------------------------------------------
 * Public API(s)
 *------------------------------------------------------------------------*/
typedef void MOD_OBJ_HND;
typedef void LED_OBJ_HND;

MOD_OBJ_HND* led_hal_register(Int8 *name, Uint32 instance);
void led_hal_unregister(MOD_OBJ_HND *mod);
Int32 led_hal_action(MOD_OBJ_HND *mod, Uint32 state_id);
LED_OBJ_HND* led_hal_install_callbacks(LED_FUNCS_T *led_funcs);
Int32 led_hal_uninstall_callbacks(LED_OBJ_HND *led);
Int32 led_hal_configure_mod(MOD_CFG_T *cfg);
Int32 led_hal_dump_cfg_info(Int8 *buf, Int32 size);

Int32 led_hal_init( void );
Int32 led_hal_exit( void );

#endif

