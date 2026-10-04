/*
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_MX2_H__
#define __ASM_ARCH_MX2_H__

#ifndef __ASM_ARCH_MXC_HARDWARE_H__
#error "Do not include directly."
#endif

/*!
 * defines the hardware clock tick rate
 */
#define CLOCK_TICK_RATE         13300000

/*
 *****************************************
 * GPT  Register definitions             *
 *****************************************
 */
#define GPT_BASE_ADDR(x)	(GPT ##x## _BASE_ADDR)
#define MXC_GPT_TCTL(x)		IO_ADDRESS(GPT_BASE_ADDR(x) + 0x00)
#define MXC_GPT_TPRER(x)	IO_ADDRESS(GPT_BASE_ADDR(x) + 0x04)
#define MXC_GPT_TCMP(x)		IO_ADDRESS(GPT_BASE_ADDR(x) + 0x08)
#define MXC_GPT_TCR(x)		IO_ADDRESS(GPT_BASE_ADDR(x) + 0x0C)
#define MXC_GPT_TCN(x)		IO_ADDRESS(GPT_BASE_ADDR(x) + 0x10)
#define MXC_GPT_TSTAT(x)	IO_ADDRESS(GPT_BASE_ADDR(x) + 0x14)
#define MXC_GPT_GPTCNT		MXC_GPT_TCN(MXC_TIMER_GPT1)
#define GPT_TSTAT_COMP		(1 << 0)
#define GPT_TSTAT_CAPT		(1 << 1)
#define GPT_TCTL_TEN		(1 << 0)
#define GPT_TCTL_SRC_PER1	(1 << 1)
#define GPT_TCTL_SRC_PER1_DIV4	(2 << 1)
#define GPT_TCTL_SRC_TIN	(3 << 1)
#define GPT_TCTL_SRC_32K	(4 << 1)
#define GPT_TCTL_COMPEN		(1 << 4)
#define GPT_TCTL_CAPTEN		(1 << 5)
#define GPT_TCTL_FRR		(1 << 8)
#define GPT_TCTL_OM		(1 << 9)
#define GPT_TCTL_CC		(1 << 10)
#define GPT_TCTL_SWR		(1 << 15)

#include <asm/arch/mx27.h>

#endif				/*  __ASM_ARCH_MX2_H__ */
