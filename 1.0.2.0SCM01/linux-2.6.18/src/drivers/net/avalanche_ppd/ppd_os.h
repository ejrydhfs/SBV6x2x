/*
 * ppd_pvt.h
 *
 * OS abstraction (non PAL OS) for PPD driver
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

#include "ppd_pvt.h"

#if ((PPD_DEBUG) & PPD_DEBUG_ERR)
#define printErr(fmt,args...) printk(fmt , ##args)
#else
#define printErr(x,...)
#endif

#if ((PPD_DEBUG) & PPD_DEBUG_MSG)
#define printMsg(fmt,args...) printk(fmt , ##args)
#else
#define printMsg(x,...)
#endif

#if ((PPD_DEBUG) & PPD_DEBUG_CMD)
#define printCmd(fmt,args...) printk(fmt , ##args)
#else
#define printCmd(x,...)
#endif

#if ((PPD_DEBUG) & PPD_DEBUG_EVN)
#define printEvn(fmt,args...) printk(fmt , ##args)
#else
#define printEvn(x,...)
#endif

#define ppd_os_get_io_virt(addr)    IO_ADDRESS(addr)
#define ppd_os_get_io_phys(addr)    IO_VIRT2PHY(addr)
