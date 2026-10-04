/*
 *
 * avalanche_misc.c 
 * Description:
 * miscellaneous APIs (like clk helpers) implementation
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
#include <asm/arch/generic/pal.h>
#include <linux/module.h>
#include <linux/spinlock.h>


unsigned int avalanche_vbus_freq;

void avalanche_set_vbus_freq(unsigned int new_vbus_freq)
{
    avalanche_vbus_freq = new_vbus_freq;
}

unsigned int avalanche_get_vbus_freq(void)
{
    return(avalanche_vbus_freq);
}

unsigned int avalanche_get_chip_version_info(void)
{
    return(*(volatile unsigned int*)AVALANCHE_CVR);
}

int avalanche_is_cpmac_on_vbus(void)
{
    if(0x00b != (*((volatile unsigned int*)AVALANCHE_CVR) & 0xffff))
        return (1); /* The SoC which does not have cpmac on Vbus is Apex */
    else 
        return (0);
}




/* Reset API */
EXPORT_SYMBOL(PAL_sysResetCtrl);
EXPORT_SYMBOL(PAL_sysGetResetStatus);
EXPORT_SYMBOL(PAL_sysSystemReset);
EXPORT_SYMBOL(PAL_sysGetSysLastResetStatus);

/* Power API */
EXPORT_SYMBOL(PAL_sysPowerCtrl);
EXPORT_SYMBOL(PAL_sysGetPowerStatus);
EXPORT_SYMBOL(PAL_sysSetGlobalPowerMode);
EXPORT_SYMBOL(PAL_sysGetGlobalPowerMode);

/* Clock API */
EXPORT_SYMBOL(PAL_sysClkcSetFreq);
EXPORT_SYMBOL(PAL_sysClkcGetFreq);

/* Cache API */
EXPORT_SYMBOL(PAL_sysCacheInvalidate);
EXPORT_SYMBOL(PAL_sysCacheFlush);
EXPORT_SYMBOL(PAL_sysCacheFlushAndInvalidate);

/* Wakeup API */
/* EXPORT_SYMBOL(PAL_sysWakeupCtrl); */

/* Misc API */
EXPORT_SYMBOL(PAL_sysProbeAndPrep);
EXPORT_SYMBOL(PAL_sysGetChipVersionInfo);
EXPORT_SYMBOL(avalanche_get_cpu_type);
EXPORT_SYMBOL(avalanche_get_cpu_name);
EXPORT_SYMBOL(avalanche_set_mdix_on_chip);
EXPORT_SYMBOL(avalanche_is_mdix_on_chip);
EXPORT_SYMBOL(avalanche_set_vbus_freq);
EXPORT_SYMBOL(avalanche_get_vbus_freq);
EXPORT_SYMBOL(avalanche_is_cpmac_on_vbus);


#if defined (CONFIG_MACH_PUMA5)
/* cppi4.1 initialization */
extern int avalanche_cppi_init(void);
arch_initcall(avalanche_cppi_init);

/* cppi4.1 APIs */
EXPORT_SYMBOL(PAL_cppi4Init);
EXPORT_SYMBOL(PAL_cppi4AllocDesc);
EXPORT_SYMBOL(PAL_cppi4DeallocDesc);
EXPORT_SYMBOL(PAL_cppi4BufPoolInit);
EXPORT_SYMBOL(PAL_cppi4BufPoolDestroy);
EXPORT_SYMBOL(PAL_cppi4Control);
EXPORT_SYMBOL(PAL_cppi4Exit);
EXPORT_SYMBOL(PAL_cppi4TxChOpen);
EXPORT_SYMBOL(PAL_cppi4RxChOpen);
EXPORT_SYMBOL(PAL_cppi4EnableTxChannel);
EXPORT_SYMBOL(PAL_cppi4EnableRxChannel);
EXPORT_SYMBOL(PAL_cppi4TxChClose);
EXPORT_SYMBOL(PAL_cppi4RxChClose);
EXPORT_SYMBOL(PAL_cppi4DisableTxChannel);
EXPORT_SYMBOL(PAL_cppi4DisableRxChannel);
EXPORT_SYMBOL(PAL_cppi4QueueOpen);
EXPORT_SYMBOL(PAL_cppi4QueueClose);
EXPORT_SYMBOL(PAL_cppi4QueuePush);
EXPORT_SYMBOL(PAL_cppi4QueuePop);
EXPORT_SYMBOL(PAL_cppi4GetTdInfo);
EXPORT_SYMBOL(PAL_cppi4RxChDestroy);
EXPORT_SYMBOL(PAL_cppi4TxChDestroy);
EXPORT_SYMBOL(PAL_cppi4AccChGetNextList);
EXPORT_SYMBOL(PAL_cppi4AccChOpen);
EXPORT_SYMBOL(PAL_cppi4AccChClose);

/* avalanche I/O power control */
EXPORT_SYMBOL(avalanche_setIOPowerMode);
EXPORT_SYMBOL(avalanche_getIOPowerMode);

#endif

/* Power & Sleep Controlller APIs */
EXPORT_SYMBOL(PAL_sysGetDomainInfo);
EXPORT_SYMBOL(PAL_sysPscGetModuleInfo);
EXPORT_SYMBOL(PAL_sysPscSetDomainState);
EXPORT_SYMBOL(PAL_sysPscSetModuleState);
EXPORT_SYMBOL(PAL_sysPscClearRegister);
EXPORT_SYMBOL(PAL_sysPscSetMdctl);

#if defined(CONFIG_ARM_AVALANCHE_TIMER16)
EXPORT_SYMBOL(PAL_sysTimer16SetParams);    
EXPORT_SYMBOL(PAL_sysTimer16GetFreqRange);
EXPORT_SYMBOL(PAL_sysTimer16Ctrl);
#endif 

#if defined(CONFIG_MACH_PUMA5EVM)
EXPORT_SYMBOL(PAL_sysWdtimerInit);      
EXPORT_SYMBOL(PAL_sysWdtimerSetPeriod);
EXPORT_SYMBOL(PAL_sysWdtimerCtrl);      
EXPORT_SYMBOL(PAL_sysWdtimerKick);      
#endif 

EXPORT_SYMBOL(avalanche_intd_enable_interrupt);
EXPORT_SYMBOL(avalanche_intd_disable_interrupt);
EXPORT_SYMBOL(avalanche_intd_set_interrupt_status);
EXPORT_SYMBOL(avalanche_intd_clear_interrupt_status);
EXPORT_SYMBOL(avalanche_intd_is_status_set);
EXPORT_SYMBOL(avalanche_intd_is_status_cleared);
EXPORT_SYMBOL(avalanche_intd_read_eoi);
EXPORT_SYMBOL(avalanche_intd_write_eoi);
EXPORT_SYMBOL(avalanche_intd_get_interrupt_count);
EXPORT_SYMBOL(avalanche_intd_set_interrupt_count);

#if defined (CONFIG_AVALANCHE_GENERIC_GPIO)
EXPORT_SYMBOL(PAL_sysGpioInit);
EXPORT_SYMBOL(PAL_sysGpioCtrl);
EXPORT_SYMBOL(PAL_sysGpioOutBit);
EXPORT_SYMBOL(PAL_sysGpioInBit);
EXPORT_SYMBOL(PAL_sysGpioOutValue);
EXPORT_SYMBOL(PAL_sysGpioInValue);
#endif



