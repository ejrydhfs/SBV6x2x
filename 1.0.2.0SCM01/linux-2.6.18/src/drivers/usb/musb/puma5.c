/*
 *
 * puma5.c 
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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/memory.h>
//#include <asm/arch/gpio.h>
#include <asm/mach-types.h>

#include "musbdefs.h"
#include <asm/arch-avalanche/puma5/puma5.h>
#define AVALANCHE_USBPHY_RESET_BIT              26

#ifdef CONFIG_MACH_DAVINCI_EVM
#include <asm/arch/i2c-client.h>
#endif

#include <linux/platform_device.h>
#include "puma5.h"
#include "cppi41_dma.h"

/* this configuration is only of TNETC950N, where min-AB connect is provided 
   to test for host mode functionality 
*/
#ifdef CONFIG_USB_MUSB_HOST
	#define CONFIG_MACH_TNETC950N
#endif

/* REVISIT (PM) we should be able to keep the PHY in low power mode most
 * of the time (24 MHZ oscillator and PLL off, etc) by setting POWER.D0
 * and, when in host mode, autosuspending idle root ports... PHYPLLON
 * (overriding SUSPENDM?) then likely needs to stay off.
 */
/*
 * USB
 */
//#if defined(CONFIG_USB_MUSB_HDRC) || defined(CONFIG_USB_MUSB_HDRC_MODULE)

static struct musb_hdrc_platform_data usb_data = {
#if     defined(CONFIG_USB_MUSB_OTG)
        /* OTG requires a Mini-AB connector */
        .mode           = MUSB_OTG,
#elif   defined(CONFIG_USB_MUSB_PERIPHERAL)
        .mode           = MUSB_PERIPHERAL,
#elif   defined(CONFIG_USB_MUSB_HOST)
        .mode           = MUSB_HOST,
#endif
        /* irlml6401 switches 5V */
        .power          = 255,          /* sustains 3.0+ Amps (!) */
        .potpgt         = 4,            /* ~8 msec */

        /* REVISIT multipoint is a _chip_ capability; not board specific */
        .multipoint     = 1,
};
/* TODO IRQ and USBotg base address to be changed -
   refer 2.6.18.include/asm/arch-avalanche/puma5 */
static struct resource usb_resources [] = { {
        /* physical address */
        .start          = AVALANCHE_USB20_OTG_SLAVE_BASE,
        .end            = AVALANCHE_USB20_OTG_SLAVE_BASE + 0x5ff,
        .flags          = IORESOURCE_MEM,
}, {
        .start          = LNXINTNUM(AVALANCHE_SRTR_USB_INT),
        .flags          = IORESOURCE_IRQ,
},
};

static u64 usb_dmamask = DMA_32BIT_MASK;

struct platform_device usb_dev = {
        .name           = "musb_hdrc",
        .id             = -1,
        .dev = {
                .platform_data  = &usb_data,
                .dma_mask               = &usb_dmamask,
                .coherent_dma_mask      = DMA_32BIT_MASK,
        },
        .resource       = usb_resources,
        .num_resources  = ARRAY_SIZE(usb_resources),
};

void phy_on(void)
{
      /* start the on-chip PHY and its PLL */
        /* Enable entire phy including the  charg pump reset D1 bit */
        /* start the on-chip PHY and its PLL */
#ifdef CONFIG_MACH_PUMA5

        void    *__iomem tbase = (void *__iomem)AVALANCHE_DCL_BASE;
        u32 tmp,timeout = 100;

        /* unlock writes to bootcfg area */
        musb_writel(tbase,BOOTCFG_REG_KICK0_OFFS,BOOTCFG_KICK0_UNLOCK_PATTERN);
        musb_writel(tbase,BOOTCFG_REG_KICK1_OFFS,BOOTCFG_KICK1_UNLOCK_PATTERN);

        /* clear PDWN and OTGPDWN bits in USBPHY */
        tmp = musb_readl(tbase, USBPHY_CTRL_OFFS);
        tmp &= ~(USBPHY_CTRL_PDWN | USBPHY_CTRL_OTGPDWN | USBPHY_CTRL_PLLON);

#define PHYREFCLK_FROM_ONCHIP_PLL
#ifdef PHYREFCLK_FROM_ONCHIP_PLL
        tmp &= ~(USBPHY_CTRL_OSCSEL);
#endif
#ifdef PHYREFCLK_FROM_ONBOARD_PLL
        tmp |= USBPHY_CTRL_OSCSEL;
#endif
        /* enable session end comparator */
        tmp |= USBPHY_CTRL_SESNDEN;

        /* enable vbus comparator enable */
        tmp |= USBPHY_CTRL_VBDTCTEN;
        musb_writel(tbase,USBPHY_CTRL_OFFS,tmp);

        /* Check D6 bit if D6 =1 power is stable and PLL is locked */
        while ((musb_readl(tbase ,USBPHY_CTRL_OFFS)
                & USBPHY_CTRL_PWRCLKGD) == 0 ){
			if( timeout == 0 )
				break;
			timeout--;
                                cpu_relax();
        }
	if( timeout == 0 )
		printk("Warn : USB PLL not Locked UsbPhyCtrl(%x)\n",musb_readl(tbase ,USBPHY_CTRL_OFFS));
#endif

}

void phy_off(void)
{
#ifdef CONFIG_MACH_PUMA5
         void    *__iomem tbase = (void *__iomem)AVALANCHE_DCL_BASE;
        u32 tmp;

        /* unlock writes to bootcfg area */
        musb_writel(tbase,BOOTCFG_REG_KICK0_OFFS,BOOTCFG_KICK0_UNLOCK_PATTERN);
        musb_writel(tbase,BOOTCFG_REG_KICK1_OFFS,BOOTCFG_KICK1_UNLOCK_PATTERN);

        /* powerdown the on-chip PHY and its oscillator */
        tmp = musb_readl(tbase, USBPHY_CTRL_OFFS);
        tmp |= (USBPHY_CTRL_PDWN | USBPHY_CTRL_OTGPDWN );
        musb_writel(tbase,USBPHYDISABLE,tmp);

         /* Check D6 bit if D6 =1 power is stable and PLL is locked */
        while ((musb_readl(tbase ,USBPHY_CTRL_OFFS)
                & USBPHY_CTRL_PWRCLKGD) == 1 )
                        cpu_relax();
#endif
}

//static int dma_off = 1;

void musb_platform_enable(struct musb *musb)
{
	u32	tmp, old, val;

   /* workaround:  setup irqs through both register sets */
        tmp = (musb->wEndMask & PUMA5_USB_TX_ENDPTS_MASK)
                        << PUMA5_USB_TXINT_SHIFT;
        musb_writel(musb->ctrl_base, PUMA5_USB_INT_MASK_SET_REG, tmp);
        old = tmp;
        tmp = (musb->wEndMask & (0xfffe & PUMA5_USB_RX_ENDPTS_MASK))
                        << PUMA5_USB_RXINT_SHIFT;
        musb_writel(musb->ctrl_base, PUMA5_USB_INT_MASK_SET_REG, tmp);
        tmp |= old;

        val = ~MGC_M_INTR_SOF;
        tmp |= ((val & 0x01ff) << PUMA5_USB_USBINT_SHIFT);
        musb_writel(musb->ctrl_base, PUMA5_USB_INT_MASK_SET_REG, tmp);

}

/*
 * Disable the HDRC and flush interrupts
 */
void musb_platform_disable(struct musb *musb)
{
    /* because we don't set CTRLR.UINT, "important" to:
         *  - not read/write INTRUSB/INTRUSBE
         *  - (except during initial setup, as workaround)
         *  - use INTSETR/INTCLRR instead
         */
        musb_writel(musb->ctrl_base, PUMA5_USB_INT_MASK_CLR_REG,
                          PUMA5_USB_USBINT_MASK
                        | PUMA5_USB_TXINT_MASK
                        | PUMA5_USB_RXINT_MASK);
        musb_writeb(musb->pRegs, MGC_O_HDRC_DEVCTL, 0);
        musb_writel(musb->ctrl_base, PUMA5_USB_EOI_REG, 0);

}


/* REVISIT this file shouldn't modify the OTG state machine ...
 *
 * The OTG infrastructure needs updating, to include things like
 * offchip DRVVBUS support and replacing MGC_OtgMachineInputs with
 * musb struct members (so e.g. vbus_state vanishes).
 */
static int vbus_state = -1;

#ifdef CONFIG_USB_MUSB_HDRC_HCD
#define	portstate(stmt)		stmt
#else
#define	portstate(stmt)
#endif

static void session(struct musb *musb, int is_on)
{
//	void	*__iomem mregs = musb->pRegs;
    void    *__iomem mregs = musb->pRegs;

#if 0
        u8      devctl = musb_readb(mregs, MGC_O_HDRC_DEVCTL);

        /* NOTE: after drvvbus off the state _could_ be A_IDLE;
         * but the silicon seems to couple vbus to "ID grounded".
         */
        if (is_on)
                devctl |= MGC_M_DEVCTL_SESSION;
        else
                devctl &= ~MGC_M_DEVCTL_SESSION;

#if 0
        if (is_on)
                musb->xceiv.state = OTG_STATE_A_WAIT_BCON;
        else
                musb->xceiv.state = OTG_STATE_B_IDLE;
#endif
	printk("\n session devctl = %x ",devctl);
        musb_writeb(mregs, MGC_O_HDRC_DEVCTL, devctl);
#endif

#if 1
	if (musb->xceiv.default_a) {
		u8	devctl = musb_readb(mregs, MGC_O_HDRC_DEVCTL);

		if (is_on)
			devctl |= MGC_M_DEVCTL_SESSION;
		else
			devctl &= ~MGC_M_DEVCTL_SESSION;
		musb_writeb(mregs, MGC_O_HDRC_DEVCTL, devctl);
	} else
		is_on = 0;

	if (is_on) {
		/* NOTE: assumes VBUS already exceeds A-valid */
		musb->xceiv.state = OTG_STATE_A_WAIT_BCON;
		portstate(musb->port1_status |= USB_PORT_STAT_POWER);
		MUSB_HST_MODE(musb);
	} else {
		switch (musb->xceiv.state) {
		case OTG_STATE_UNDEFINED:
		case OTG_STATE_B_IDLE:
			MUSB_DEV_MODE(musb);
			musb->xceiv.state = OTG_STATE_B_IDLE;
			break;
		case OTG_STATE_A_IDLE:
			break;
		default:
			musb->xceiv.state = OTG_STATE_A_WAIT_VFALL;
			break;
		}
		portstate(musb->port1_status &= ~USB_PORT_STAT_POWER);
	}

//	DBG(2, "Default-%c, VBUS power %s, %s, devctl %02x, %s\n",
//		musb->xceiv.default_a ? 'A' : 'B',
//		is_on ? "on" : "off",
//		MUSB_MODE(musb),
//		musb_readb(musb->pRegs, MGC_O_HDRC_DEVCTL),
//		otg_state_string(musb));
#endif
}

/* VBUS SWITCHING IS BOARD-SPECIFIC */

#ifdef CONFIG_MACH_DAVINCI_EVM
#ifndef CONFIG_MACH_DAVINCI_EVM_OTG

/* I2C operations are always synchronous, and require a task context.
 * With unloaded systems, using the shared workqueue seems to suffice
 * to satisfy the 100msec A_WAIT_VRISE timeout...
 */
static void evm_deferred_drvvbus(void *_musb)
{
	struct musb	*musb = _musb;
	int		is_on = (musb->xceiv.state == OTG_STATE_A_IDLE);

	davinci_i2c_expander_op(0x3a, USB_DRVVBUS, !is_on);
	vbus_state = is_on;
	session(musb, is_on);
}
DECLARE_WORK(evm_vbus_work, evm_deferred_drvvbus, 0);

#endif	/* modified board */
#endif	/* EVM */

static void puma5_vbus_power(struct musb *musb, int is_on, int immediate)
{
      if (is_on)
         is_on = 1;

      if (vbus_state == is_on)
                return;


#ifdef CONFIG_MACH_DAVINCI_EVM
	if (machine_is_davinci_evm()) {
#ifdef CONFIG_MACH_DAVINCI_EVM_OTG
		/* modified EVM board switching VBUS with GPIO(6) not I2C
		 * NOTE:  PINMUX0.RGB888 (bit23) must be clear
		 */
		if (is_on)
			gpio_set(GPIO(6));
		else
			gpio_clear(GPIO(6));
		immediate = 1;
#else
		if (immediate)
			davinci_i2c_expander_op(0x3a, USB_DRVVBUS, !is_on);
		else
			schedule_work(&evm_vbus_work);
#endif
	}
#endif

#ifdef CONFIG_MACH_PUMA5
#ifdef CONFIG_MACH_TNETC950N
/* this configuration is only of TNETC950N, where min-AB connect is provided 
   to test for host mode functionality */ 
	PAL_sysGpioCtrl(14, GPIO_PIN, GPIO_OUTPUT_PIN);
        PAL_sysGpioOutBit(14,0);
#endif
#endif


	if (immediate) {
		vbus_state = is_on;
		session(musb, is_on);
	} else {
		/* REVISIT:  if is_on, start in A_WAIT_VRISE, then OTG timer
		 * should watch for session valid before calling session().
		 * EVM charges C133 VERY quickly (but discharge is sloooow).
		 */
	}
}


static void puma5_set_vbus(struct musb *musb, int is_on)
{
	WARN_ON(is_on && is_peripheral_active(musb));
	return puma5_vbus_power(musb, is_on, 0);
}
//#define MUSB_USE_TASKLET
//#undef MUSB_USE_TASKLET

#ifdef MUSB_USE_TASKLET
        static struct tasklet_struct musb_tasklet;
        static int musb_tasklet_scheduled = 0;
        static int taskInit = 0;
        void musb_tasklet_func(unsigned long);
#endif

//static DEFINE_SPINLOCK(musb_irq_lock);
static irqreturn_t puma5usb_direct_interrupt(int irq, void *__hci, struct pt_regs *r);

static irqreturn_t puma5usb_interrupt(int irq, void *__hci, struct pt_regs *r)
{
        irqreturn_t ret;

#ifndef MUSB_USE_TASKLET
        ret= puma5usb_direct_interrupt(irq,__hci,r);
#else
        /* use tasklet for processing here */
        spin_lock_irqsave(&musb_irq_lock, flags);
        if(musb_tasklet_scheduled)
        {
                /*printk("Dummy ret\n");*/
                ret = IRQ_HANDLED;
        }
        else
        {
                disable_irq(38); /* TODO */
                disable_irq(39);
                /* trigger tasklet action */
                p = (unsigned long) __hci;
                if(!taskInit)
                {
                        tasklet_init(&musb_tasklet,musb_tasklet_func,p);
                        taskInit = 1;
                }
                tasklet_schedule(&musb_tasklet);
                musb_tasklet_scheduled = 1;
                /*printk("Sched Task %d\n", irq);*/
                ret = IRQ_HANDLED;
        }/* end of else conditional block */

        spin_unlock_irqrestore(&musb_irq_lock, flags);
#endif

        return ret;
}

#ifdef MUSB_USE_TASKLET

void musb_tasklet_func(unsigned long param)
{
        void * __hci = param;
        unsigned long   flags;
        struct musb     *musb = __hci;
        void            *__iomem tibase = musb->ctrl_base;
        u32             tmp,cppi_tx,cppi_rx;

        spin_lock_irqsave(&musb->Lock, flags);

        /* NOTE: DaVinci shadows the Mentor IRQs; don't manage them through
         * the mentor registers (except for setup), use the TI ones and EOI.
         */

        /* ack and handle non-CPPI interrupts */
        tmp = musb_readl(tibase, PUMA5_USB_INT_SRC_MASKED_REG);
        musb_writel(tibase, PUMA5_USB_INT_SRC_CLR_REG, tmp);
       musb->int_rx = (tmp & PUMA5_USB_RXINT_MASK)
                        >> PUMA5_USB_RXINT_SHIFT;
        musb->int_tx = (tmp & PUMA5_USB_TXINT_MASK)
                        >> PUMA5_USB_TXINT_SHIFT;
        musb->int_usb = (tmp & PUMA5_USB_USBINT_MASK)
                        >> PUMA5_USB_USBINT_SHIFT;


        if (tmp & (1 << (8 + PUMA5_USB_USBINT_SHIFT))){

                #ifdef CONFIG_USB_MUSB_OTG
                int     drvvbus = musb_readl(tibase, PUMA5_USB_STAT_REG);
                #endif
                /* NOTE:  this must complete poweron within 100 msec */
                DBG(1,"drvvbus Interrupt\n");
                #ifdef CONFIG_USB_MUSB_OTG
                        puma5_vbus_power(musb, drvvbus, drvvbus);
                #endif
        }

        if ( (musb->int_tx & 1)  || musb->int_usb) {

                musb_interrupt(musb);
                #if defined (CONFIG_MIPS_YAMUNA) && defined (CONFIG_MIPS_AVALANCHE_COLORED_LED)
                if (musb->int_tx)
                        led_manager_led_action (musb->pledhandle, 3);
                else if (musb->int_rx)
                        led_manager_led_action (musb->pledhandle, 2);
                else if (musb->int_usb & MGC_M_INTR_CONNECT)
                        led_manager_led_action (musb->pledhandle, 1);
                else if (musb->int_usb & MGC_M_INTR_DISCONNECT)
                        led_manager_led_action (musb->pledhandle, 0);
                else if (musb->int_usb & MGC_M_INTR_RESET)
                        led_manager_led_action (musb->pledhandle, 1);
                #endif
        }


        tmp = musb_readl(tibase, DAVINCI_USB_INT_SRC_MASKED_REG);

        if(tmp)
        {
        /*      printk("T1\n"); */
                tasklet_schedule(&musb_tasklet);
        }
else
        {
        /*      printk("IE\n");*/
                musb_tasklet_scheduled = 0;
                enable_irq(39); /* TODO */
                enable_irq(38);
                /* Endpoint/USB Status irq stays asserted until EOI is written */
                musb_writel(tibase, PUMA5_USB_EOI_REG, 0);
        }

        spin_unlock_irqrestore(&musb->Lock, flags);
}
#endif

static irqreturn_t puma5usb_direct_interrupt(int irq, void *__hci, struct pt_regs *r)
{
	unsigned long	flags;
	irqreturn_t	retval = IRQ_NONE;
	struct musb	*musb = __hci;
	void		*__iomem tibase = musb->ctrl_base;
	u32		tmp;

	//printk("I");
	spin_lock_irqsave(&musb->Lock, flags);

	/* NOTE: DaVinci shadows the Mentor IRQs.  Don't manage them through
	 * the Mentor registers (except for setup), use the TI ones and EOI.
	 *
	 * Docs describe irq "vector" registers asociated with the CPPI and
	 * USB EOI registers.  These hold a bitmask corresponding to the
	 * current IRQ, not an irq handler address.  Would using those bits
	 * resolve some of the races observed in this dispatch code??
	 */

	/* CPPI interrupts share the same IRQ line, but have their own
	 * mask, state, "vector", and EOI registers.
	 */

        tmp = musb_readl(tibase, PUMA5_USB_INT_SRC_MASKED_REG);
        musb_writel(tibase, PUMA5_USB_INT_SRC_CLR_REG, tmp);

        musb->int_rx = (tmp & PUMA5_USB_RXINT_MASK)
                        >> PUMA5_USB_RXINT_SHIFT;
        musb->int_tx = (tmp & PUMA5_USB_TXINT_MASK)
                        >> PUMA5_USB_TXINT_SHIFT;
        musb->int_usb = (tmp & PUMA5_USB_USBINT_MASK)
                        >> PUMA5_USB_USBINT_SHIFT;
        musb->int_regs = r;
	
	/* treat DRVVBUS irq like an ID change IRQ (for now) */
	if (tmp & (1 << (8 + PUMA5_USB_USBINT_SHIFT))) {
		int	drvvbus = musb_readl(tibase, PUMA5_USB_STAT_REG);
#if 0
#ifdef CONFIG_USB_MUSB_OTG
                int     drvvbus = musb_readl(tibase, PUMA5_USB_STAT_REG);
#endif
                /* NOTE:  this must complete poweron within 100 msec */
                DBG(1,"drvvbus Interrupt\n");
#ifdef CONFIG_USB_MUSB_OTG

                davinci_vbus_power(musb, drvvbus, drvvbus);
#endif
#endif

#if 1
		if (drvvbus) {
			MUSB_HST_MODE(musb);
			musb->xceiv.default_a = 1;
			musb->xceiv.state = OTG_STATE_A_IDLE;
		} else {
			MUSB_DEV_MODE(musb);
			musb->xceiv.default_a = 0;
			musb->xceiv.state = OTG_STATE_B_IDLE;
		}

		/* NOTE:  this must complete poweron within 100 msec */
		puma5_vbus_power(musb, drvvbus, 0);
#endif
//		DBG(2, "DRVVBUS %d (%s)\n", drvvbus, otg_state_string(musb));
		retval = IRQ_HANDLED;
	}


	if ((musb->int_tx ) || musb->int_usb || musb->int_rx )
		retval |= musb_interrupt(musb);

	/* irq stays asserted until EOI is written */
	musb_writel(tibase, PUMA5_USB_EOI_REG, 0);

	musb->int_regs = NULL;
	spin_unlock_irqrestore(&musb->Lock, flags);

	/* REVISIT we sometimes get unhandled IRQs
	 * (e.g. ep0).  not clear why...
	 */
	if (retval != IRQ_HANDLED)
		DBG(5, "unhandled? %08x\n", tmp);
	return IRQ_HANDLED;
}

int __devinit musb_platform_init(struct musb *musb)
{
	void	*__iomem tibase = musb->ctrl_base;
	u32	revision;

	musb->pRegs += PUMA5_BASE_OFFSET;

	/* returns zero if e.g. not clocked */
	revision = musb_readl(tibase, PUMA5_USB_VERSION_REG);

	if (revision == 0)
		return -ENODEV;

#ifdef CONFIG_MACH_DAVINCI_EVM
	if (machine_is_davinci_evm())
		evm_vbus_work.data = musb;
#endif

	musb->board_set_vbus = puma5_set_vbus;
	puma5_vbus_power(musb, 0, 1);

	/* soft reset the controller */
	musb_writel(tibase, PUMA5_USB_CTRL_REG, 0x1);

	/* write zero to EOI register, following the soft
	reset on USB, if EOI register is not written with zero
	followed by reset on USB, it may no longer possible to
	generate the USB interupts */
	musb_writel(tibase, PUMA5_USB_EOI_REG, 0);

	/* start the on-chip PHY and its PLL */
	phy_on();

	msleep(5);

	/* NOTE:  irqs are in mixed mode, not bypass to pure-musb */
	musb->isr = puma5usb_interrupt;
	return 0;
}

int musb_platform_exit(struct musb *musb)
{
#if 1
	puma5_vbus_power(musb, 0 /*off*/, 1);

	/* delay, to avoid problems with module reload */
	if (is_host_enabled(musb)) {
		int	maxdelay = 30;
		u8	devctl, warn = 0;

		/* if there's no peripheral connected, this can take a
		 * long time to fall, especially on EVM with huge C133.
		 */
		do {
			devctl = musb_readb(musb->pRegs, MGC_O_HDRC_DEVCTL);
			if (!(devctl & MGC_M_DEVCTL_VBUS))
				break;
			if ((devctl & MGC_M_DEVCTL_VBUS) != warn) {
				warn = devctl & MGC_M_DEVCTL_VBUS;
				DBG(1, "VBUS %d\n", warn >> MGC_S_DEVCTL_VBUS);
			}
			msleep(1000);
			maxdelay--;
		} while (maxdelay > 0);

		/* in OTG mode, another host might be connected */
		if (devctl & MGC_M_DEVCTL_VBUS)
			DBG(1, "VBUS off timeout (devctl %02x)\n", devctl);
	}
#endif
	phy_off();
 //	puma5_vbus_power(musb, 0 /*off*/, 1);

	return 0;
}
#ifdef CONFIG_PM
extern void musb_pullup(struct musb *musb, int is_on);
void puma5_usb_power_up(struct musb *musb)
{
        u32 *usb_pll ;
        u32 val;

  /* power up the USB_PLL */
        usb_pll = (u32 *)(AVALANCHE_USB_PLL_BASE + 8);
        val = *usb_pll;
        val |= 0xc;
        val &= ~0x3;
        *usb_pll = val;
        mdelay(500);

        /* enable the usb clock */
        PAL_sysPowerCtrl((INT32)PSC_SR_CLK5, POWER_CTRL_POWER_UP);
        PAL_sysPowerCtrl((INT32)PSC_USB_PHY, POWER_CTRL_POWER_UP);

         /* set USB clock speed to 24 Mhz */
        if(PAL_sysClkcSetFreq(PAL_SYS_CLKC_USB, AVALANCHE_USB_CLOCK_SPEED) < 0)
                DBG(1,"%s:USB clock set failed\n", __FUNCTION__);


        phy_on();
        musb_pullup(musb, 1);
}
EXPORT_SYMBOL(puma5_usb_power_up);
void puma5_usb_power_down(struct musb *musb)
{

        u32 *usb_pll;
        u32 val;

        musb_pullup(musb, 0);

#ifdef CONFIG_USB_GADGET_MUSB_HDRC
        if (musb->pGadgetDriver && musb->pGadgetDriver->disconnect) {
                spin_unlock(&musb->Lock);
                musb->pGadgetDriver->disconnect(&musb->g);
                spin_lock(&musb->Lock);
        }
#endif
        //* disable the usb pll clock */
        PAL_sysPowerCtrl((INT32)PSC_SR_CLK5, POWER_CTRL_POWER_DOWN);
        PAL_sysPowerCtrl((INT32)PSC_USB_PHY, POWER_CTRL_POWER_DOWN);

        /* power down usb phy */
        phy_off();
        udelay(1000);

        /* power down the USB_PLL */
        usb_pll = (u32 *)(AVALANCHE_USB_PLL_BASE + 0x8);
        val = *usb_pll;
        val |= 0x3;
        val &= ~0xc;
        *usb_pll = val;
}
EXPORT_SYMBOL(puma5_usb_power_down);
#endif
                
