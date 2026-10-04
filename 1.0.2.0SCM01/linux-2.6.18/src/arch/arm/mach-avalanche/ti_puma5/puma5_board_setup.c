/*
 *
 * puma5_board_setup.c
 * Description:
 * puma5 board/soc initialization
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
#include <linux/config.h>
#include <asm/arch/generic/pal.h>
#include <asm/arch/hardware.h>
#include <asm/arch/generic/avalanche_vlynq_config.h>
#include <asm/arch/generic/pal_vlynq.h>

extern void prom_printf(char *fmt, ...);
extern char *prom_getenv(char *name);
void avalanche_set_vbus_freq(unsigned int new_vbus_freq);


unsigned int cpu_freq = AVALANCHE_ARM_FREQ_DEFAULT;

/** \enum AVL_NS_REMAP_SIZE_T
	\brief Port remap window size
*/
typedef enum AVL_NS_REMAP_SIZE_tag
{
	AVL_NS_REMAP_SIZE_0K    = 0,
	AVL_NS_REMAP_SIZE_INV1, /* this is not supported by arm */
	AVL_NS_REMAP_SIZE_INV2, /* this is not supported by arm */
	AVL_NS_REMAP_SIZE_4K,
	AVL_NS_REMAP_SIZE_8K,

	/* like this it can go upto 2GB, but let us keep 8k as max*/
	AVL_NS_REMAP_SIZE_MAX
}AVL_NS_REMAP_SIZE_T;

#define AVL_NS_REMAP_BASE_MASK 0xFFFFF000

/* This routine remaps the peripheral port remap register to the specified physical address*/
/* argument base should be aligned to the size of remapped region */
int avalanche_remap_non_shared(unsigned int phy_base, AVL_NS_REMAP_SIZE_T size)
{
	unsigned int region_size = 0;

	if((size >= AVL_NS_REMAP_SIZE_MAX) || (size == AVL_NS_REMAP_SIZE_INV1) || (size == AVL_NS_REMAP_SIZE_INV2))
		return (-1);

	/* calculate region_size from enum (second param) */
	region_size = ((1 << (size-1)) * (1024));

	/* align base address to the specified size */
	phy_base &= (~(region_size-1));

	/* mask out unwanted bits */
	phy_base &= AVL_NS_REMAP_BASE_MASK;

	/* set the size bit */
	phy_base |= size;

	/* update port remap h/w register */
	__asm__ ("mcr   p15, 0, %0, c15, c2, 4" : : "r" (phy_base): "cc");

	return (0);
}

#if defined(CONFIG_MACH_PUMA5EVM)

static void puma5_ext_phy_reset(void)
{
	int delay;
    /* Get the External phy out of reset
     * GPIO pin 14 is used to get it out of reset 
     */
    /* Set the pin as GPIO pin */   
    PAL_sysGpioCtrl(14, GPIO_PIN, GPIO_OUTPUT_PIN); 
    /* Assert the reset pin by bringing it low */
    PAL_sysGpioOutBit(14, 0);
    /* According to datasheet minimum delay required is 20us put 1ms to be on safer side */
    udelay(1000);
    /* De-assert the reset */
    PAL_sysGpioOutBit(14, 1);

	/* 5ms recommended - udelay cannot take values more than 1000.
       mdelay() will not work at this point 
     */
	for(delay = 0; delay < 5; delay++)
		udelay(1000); 
}

#endif

#if defined(CONFIG_MACH_PUMA5EVM)
/* this function enables all the auxiliary GPIOs required for gmii */
static void puma5_gmii_enable(void)
{
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 19) /*aux_19*/,FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* MII_TD[2] */
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 20) /*aux_20*/,FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* MII_TD[3] */
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 21) /*aux_21*/,FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* MII_TEN   */
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 22) /*aux_22*/,FUNCTIONAL_PIN, GPIO_OUTPUT_PIN);  /* MII_RD[2] */
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 23) /*aux_23*/,FUNCTIONAL_PIN, GPIO_INPUT_PIN);  /* MII_RD[3] */
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 24) /*aux_24*/,FUNCTIONAL_PIN, GPIO_INPUT_PIN);  /* MII_RCLK  */
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 25) /*aux_25*/,FUNCTIONAL_PIN, GPIO_INPUT_PIN);  /* MII_TCLK  */
    PAL_sysGpioCtrl((AVALANCHE_MAX_PRIMARY_GPIOS + 26) /*aux_26*/,FUNCTIONAL_PIN, GPIO_INPUT_PIN);  /* MII_COL   */

    /* Set the Ethernet control register in the full duplex and giga bit mode enabled */
    /* TODO : Needs to confim this */
}
#endif

#if defined (CONFIG_ARM_AVALANCHE_VLYNQ)
extern AV_VL_GETENV vl_cfg_getenv;
extern AV_VL_PRINTF vl_cfg_printf;
extern void vlynq_delay_wait(Uint32 count);
void vlynq_dev_init(void)
{
/* TODO : Vlynq Initialization  - after BFT1 */
#if 0
    volatile unsigned int *reset_base = (unsigned int *) AVALANCHE_RESET_CONTROL_BASE;
    char *reset_pol_str;
    unsigned int reset_pol;

    /* Set the GPIO(40-43)/MII/VLYNQ pins to be used as non-MII.*/ 

    /* Set the following GPIOs as VLYNQ functional pins. */

    vl_cfg_printf = prom_printf;
    vl_cfg_getenv = prom_getenv;

	/* Put vlynq in reset and get it out of reset after a delay */

    reset_pol = 0;
    reset_pol_str = prom_getenv("wlan0_reset_pol");
    if(reset_pol_str)
    {
        prom_printf("Read the env var wlan0_reset_pol as %s.\n", reset_pol_str);
        reset_pol = simple_strtoul(reset_pol_str, NULL, 10)? 1:0;
    }

    avalanche_vlynq_set_hw_reset_info(AVALANCHE_LOW_VLYNQ_CONTROL_BASE, 0 /* hop */,
                                      25 /*reset bit.*/, reset_pol);

    PAL_sysVlynqInit( );

    avalanche_vlynq_enumerate(*(unsigned int *)AVALANCHE_CVR,
                                AVALANCHE_LOW_VLYNQ_CONTROL_BASE,
                                AVALANCHE_LOW_VLYNQ_MEM_MAP_BASE);

    vl_cfg_printf = (AV_VL_PRINTF)printk;
#endif
}
#endif



void avalanche_soc_platform_init(void)
{
    PAL_SYS_Puma5Init clkc;

/***************************************************************
 * Power Up/Down required modules IOPDCR 
 **************************************************************/
#if defined(CONFIG_MACH_PUMA5EVM)
	unsigned int psc_pid = 0;

	/* Initialize PSC base  */
	#define AVL_PSC_NO_CCM_CONTROL 0
	if(PAL_sysPscInit(AVALANCHE_PSC_BASE, AVL_PSC_NO_CCM_CONTROL, &psc_pid) == 0)
		printk("Power & Sleep Controller @ 0x%08x Initialized [id-0x%08x]\n",AVALANCHE_PSC_BASE, psc_pid);
	else
		printk("Power & Sleep Controller @ 0x%08x Initialization failed\n", AVALANCHE_PSC_BASE);

    /* Disable the CLK_OUT0 as 25MHz clock to external phy is 
     * supplied by external oscillator
     */
	avalanche_setIOPowerMode(IOPM_CLKOUT0, AVALANCHE_IO_POWER_DOWN);  

	/* see chip level document for information in IOPDCR bits */ 
	avalanche_setIOPowerMode(IOPM_UART, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_TDM_CODEC, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_GPIO, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_I2C, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_GMII, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_OOB, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_EPGA, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_BBU, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_ASYNC_EMIF, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_AGC, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_TAGC, AVALANCHE_IO_POWER_UP);	
	avalanche_setIOPowerMode(IOPM_EXT_INT, AVALANCHE_IO_POWER_UP);	

/***************************************************************
 * Get entities on the board out of reset
 **************************************************************/

	PAL_sysResetCtrl((INT32)PSC_GROUP1, OUT_OF_RESET);
	PAL_sysResetCtrl((INT32)PSC_UART1, OUT_OF_RESET);
	PAL_sysResetCtrl((INT32)PSC_GPIO, OUT_OF_RESET);
	PAL_sysResetCtrl((INT32)PSC_TIMER0, OUT_OF_RESET);
	PAL_sysResetCtrl((INT32)PSC_TIMER1, OUT_OF_RESET);
	PAL_sysResetCtrl((INT32)PSC_TIMER2, OUT_OF_RESET);
	PAL_sysResetCtrl((INT32)PSC_MMAP_SPI, OUT_OF_RESET);
	PAL_sysResetCtrl((INT32)PSC_EMIF3E_VRST, OUT_OF_RESET);

    /* Bring CPI4 out of reset - for clean implementation, reset and then
     * unreset the module */
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK0, IN_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK1, IN_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK2, IN_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK3, IN_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK4, IN_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK5, IN_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK6, IN_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK7, IN_RESET);
    udelay (1000);  
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK0, OUT_OF_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK1, OUT_OF_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK2, OUT_OF_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK3, OUT_OF_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK4, OUT_OF_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK5, OUT_OF_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK6, OUT_OF_RESET);
    PAL_sysResetCtrl ((INT32)PSC_SR_CLK7, OUT_OF_RESET);
    /* bring USB-PHY out of reset */
    PAL_sysResetCtrl ((INT32)PSC_USB_PHY, OUT_OF_RESET);
    /* bring Performance monitor module out of reset */
    PAL_sysResetCtrl ((INT32)PSC_PERF_MON, OUT_OF_RESET);

    udelay (1000);
 

#else /* CONFIG_MACH_PUMA5_VOLCANO */	
    /* NWSS is already active because the bootloader was using it */
	/* so put it in reset mode and then bring it out of reset */
	PAL_sysResetCtrl( AVALANCHE_NWSS_RESET_BIT, IN_RESET );  
	udelay(1000);
    /* take everything out of reset */
	PAL_sysResetCtrl( AVALANCHE_NWSS_RESET_BIT, OUT_OF_RESET );  
    PAL_sysResetCtrl(AVALANCHE_CPMAC_ETH0_RESET_BIT, OUT_OF_RESET);
    PAL_sysResetCtrl(AVALANCHE_MDIO_RESET_BIT, OUT_OF_RESET);
    PAL_sysResetCtrl(AVALANCHE_GPIO_RESET_BIT, OUT_OF_RESET);
    PAL_sysResetCtrl(AVALANCHE_UART0_RESET_BIT, OUT_OF_RESET);
    PAL_sysResetCtrl(AVALANCHE_UART1_RESET_BIT, OUT_OF_RESET);
#endif /*CONFIG_MACH_PUMA5EVM */
 
/***************************************************************
 * Initialize clock controller
 **************************************************************/

	/* set the reference clock and initialize clock controller */
    clkc.refclk_inp = REFCLK_FREQ;
    PAL_sysClkcInit(&clkc);

/* initialize vbus frequency */
    avalanche_set_vbus_freq(PAL_sysClkcGetFreq(CLKC_VBUS));
	cpu_freq = PAL_sysClkcGetFreq(CLKC_ARM);

#if defined(CONFIG_MACH_PUMA5EVM)

   	/* set GMII clock to 125Mhz */
	if(PAL_sysClkcSetFreq(PAL_SYS_CLKC_GMII, AVALANCHE_GMII_CLOCK_SPEED) < 0)
		printk("%s:GMII Clock set failed\n", __FUNCTION__);

	/* set USB clock speed to 24 Mhz */
	if(PAL_sysClkcSetFreq(PAL_SYS_CLKC_USB, AVALANCHE_USB_CLOCK_SPEED) < 0)
		printk("%s:USB clock set failed\n", __FUNCTION__);

/***************************************************************
 * Initialize Board GPIO mode and program GMII  
 * -pin muxing-
 **************************************************************/

#if (CONFIG_AVALANCHE_CONSOLE_PORT > 0)
	/* 	pin-mux for second uart, GPIOs 10,11,12,13 should be in functional mode  
	   	pin direction (last param) doesnt matter since we are in functional mode
	   	GPIO 12 is also used as Link Active LED, so if second UART is used then 
		Link Active LED cannot be used */
    PAL_sysGpioCtrl( 10, FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_TD */
    PAL_sysGpioCtrl( 11, FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_RD */
    PAL_sysGpioCtrl( 12, FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_CTS */
    PAL_sysGpioCtrl( 13, FUNCTIONAL_PIN, GPIO_OUTPUT_PIN); /* UART-B_RTS */
#endif

	/* Initialize GMII module */
    puma5_ext_phy_reset();
    puma5_gmii_enable();
#endif

/***************************************************************
 * Vlynq initialization  
 **************************************************************/
#if defined (CONFIG_ARM_AVALANCHE_VLYNQ)
	avalanche_setIOPowerMode(IOPM_VLYNQ, AVALANCHE_IO_POWER_UP);	
    vlynq_dev_init();
#endif
	
/***************************************************************
 * Programming ARM Peripheral Port remap register to map Interrupt controller
 * Userspace mmap on non-shared region will not work if this is not done
 ***************************************************************/
	if(avalanche_remap_non_shared(INTC_PHY, AVL_NS_REMAP_SIZE_4K) == 0)
		printk("Initialized Peripheral Port Remap Register to base : 0x%08x\n",  INTC_PHY);
	else
		printk("Peripheral Port Remap Register Initialization Failed at base : 0x%08x\n",  INTC_PHY);

	/* Power LED */
	PAL_sysGpioCtrl( 0, GPIO_PIN, GPIO_OUTPUT_PIN); 
}

#ifdef CONFIG_AVALANCHE_INTC_PACING
unsigned int avalanche_get_intc_input_freq(void)
{
    return PAL_sysClkcGetFreq(CLKC_SYS);
}
#endif

void avalanche_processor_idle(void)
{
    cpu_do_idle();	
    return ;
}


/* 
 * Puma5 uses watch dog for resetting the device
 * NOTE: parameter mode is not used in Puma5  
 */
void avalanche_system_reset(PAL_SYS_SYSTEM_RST_MODE_T mode)
{
/* Puma5 volcano RTLs doesnt have watchdog support */
#if defined(CONFIG_MACH_PUMA5EVM)

/* WDT PAL layer is initialized in watchdog character driver
 * if WDT character driver was compiled out then it should be 
 * initialized before triggering reset
 */
#if !defined(CONFIG_ARM_AVALANCHE_WDTIMER)
    /* get the Watchdog timer module out of reset */
    PAL_sysResetCtrl(AVALANCHE_WDT_RESET_BIT, OUT_OF_RESET);

    /* Initialize watchdog timer */
    PAL_sysWdtimerInit(AVALANCHE_WATCHDOG_TIMER_BASE,
                            PAL_sysClkcGetFreq(CLKC_VBUS));
#endif
    /* disable watchdog timer */
    if(PAL_sysWdtimerCtrl(AVALANCHE_WDT_DISABLE_VALUE))
       return; 

    /* set timer (in msecs) */
    if(PAL_sysWdtimerSetPeriod(AVALANCHE_WDT_RESET_MARGIN))
		return;

    /* enable watchdog timer */
    if(PAL_sysWdtimerCtrl(AVALANCHE_WDT_ENABLE_VALUE))
		return;

    /* kick is required to reload the new timeout margin */
    PAL_sysWdtimerKick();

/* volcano uses yamuna reset controller IP */
#else
	PAL_sysSystemReset(RESET_SOC_WITH_MEMCTRL);
#endif
    return ; 
}
