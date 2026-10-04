/*
 * TI DAVINCI I2C controller driver
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 Modifications:
 ver. 1.0: Feb 2005, Vinod/Sudhakar
 -
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/sysctl.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include "i2c-davinci.h"

/* ----- global defines ----------------------------------------------- */
static const char driver_name[] = "i2c_davinci";

#define DAVINCI_I2C_TIMEOUT (1*HZ)
#define MAX_MESSAGES    65536	/* max number of messages */
#define I2C_DAVINCI_INTR_ALL    (DAVINCI_I2C_ICIMR_AAS_MASK | \
				 DAVINCI_I2C_ICIMR_SCD_MASK | \
				 DAVINCI_I2C_ICIMR_ARDY_MASK | \
				 DAVINCI_I2C_ICIMR_NACK_MASK | \
				 DAVINCI_I2C_ICIMR_AL_MASK)

static int bus_freq = 400; /* Default: Fast Mode = 400 KHz, Standard Mode = 100 KHz */
static int own_addr = 0x1;	/* Randomly assigned own address */

#define PINMUX1		__REG(0x01c40004)
#define GPIO		__REG(0x01C67000)
#define GPIO23_DIR	__REG(0x01C67038)
#define GPIO23_SET	__REG(0x01C67040)
#define GPIO23_CLR	__REG(0x01C67044)

/*
 * This functions configures I2C and brings I2C out of reset.
 * This function is called during I2C init function. This function
 * also gets called if I2C encounetrs any errors. Clock calculation portion
 * of this function has been taken from some other driver.
 */
static int i2c_davinci_reset(struct i2c_davinci_device *dev)
{
	u16 psc;
	u32 clk;
	u32 input_clock = clk_get_rate(dev->clk);
	volatile struct davinci_i2cregs *r = dev->regs;

	/* put I2C into reset */
	r->icmdr &= ~DAVINCI_I2C_ICMDR_IRS_MASK;

        /* NOTE: I2C Clock divider programming info
	 * As per I2C specs the following formulas provide prescalar
         * and low/high divider values
	 *
	 * input clk --> PSC Div -----------> ICCL/H Div --> output clock
	 *                       module clk
	 *
	 * output clk = module clk / (PSC + 1) [ (ICCL + d) + (ICCH + d) ]
	 *
	 * Thus,
	 * (ICCL + ICCH) = clk = (input clk / ((psc +1) * output clk)) - 2d;
	 *
	 * where if PSC == 0, d = 7,
	 *       if PSC == 1, d = 6
	 *       if PSC > 1 , d = 5
	 */

	psc = 26; /* To get 1MHz clock */

        clk = ((input_clock/(psc + 1)) / (bus_freq * 1000)) - 10;

	r->icpsc = psc;
	r->icclkh = (50 * clk) / 100; /* duty cycle should be 27% */
	r->icclkl = (clk - r->icclkh);

	dev_dbg(dev->dev, "CLK  = %d\n", clk);
	dev_dbg(dev->dev, "PSC  = %d\n", r->icpsc);
	dev_dbg(dev->dev, "CLKL = %d\n", r->icclkl);
	dev_dbg(dev->dev, "CLKH = %d\n", r->icclkh);

	/* Set Own Address: */
	r->icoar = own_addr;

	/* Enable interrupts */
	r->icimr = I2C_DAVINCI_INTR_ALL;

	/* Take the I2C module out of reset: */
	r->icmdr |= DAVINCI_I2C_ICMDR_IRS_MASK;

	return 0;
}

/*
 * Waiting on Bus Busy
 */
static int i2c_davinci_wait_for_bb(struct i2c_davinci_device *dev,
				   char allow_sleep)
{
	unsigned long timeout;
	int i;
	static	char to_cnt = 0;
	volatile struct davinci_i2cregs *r = dev->regs;

	timeout = jiffies + DAVINCI_I2C_TIMEOUT;
	while ((r->icstr) & DAVINCI_I2C_ICSTR_BB_MASK) {
		if (to_cnt <= 2) {
			if (time_after(jiffies, timeout)) {
				dev_warn(dev->dev,
					 "timeout waiting for bus ready");
				to_cnt++;
				return -ETIMEDOUT;
			}
		}
		else {
			to_cnt = 0;
			/* Send the NACK to the slave */
			r->icmdr |= DAVINCI_I2C_ICMDR_NACKMOD_MASK;
			/* Disable I2C */
			PINMUX1 &= (~(1 << 7));

			/* Set the GPIO direction register */
			GPIO23_DIR &= ~0x0800;

			/* Send high and low on the SCL line */
			for (i = 0; i < 10; i++) {
				GPIO23_SET |= 0x0800;
				udelay(25);
				GPIO23_CLR |= 0x0800;
				udelay(25);
			}
			/* Re-enable I2C */
			PINMUX1 |= (1 << 7);

			i2c_davinci_reset(dev);
			init_completion(&dev->cmd_complete);
		}
		if (allow_sleep)
			schedule_timeout(1);
	}

	return 0;
}

/*
 * Low level master read/write transaction. This function is called
 * from i2c_davinci_xfer.
 */
static int
i2c_davinci_xfer_msg(struct i2c_adapter *adap, struct i2c_msg *msg, int stop)
{
	struct i2c_davinci_device *dev = i2c_get_adapdata(adap);
	u8 zero_byte = 0;
	volatile struct davinci_i2cregs *regs = dev->regs;
	int r;

	u32 flag = 0, stat = 0;
	int i;

	/* Introduce a 100musec delay.  Required for Davinci EVM board only */
	if (machine_is_davinci_evm())
		udelay(100);

	/* set the slave address */
	regs->icsar = msg->addr;

	/* Sigh, seems we can't do zero length transactions. Thus, we
	 * can't probe for devices w/o actually sending/receiving at least
	 * a single byte. So we'll set count to 1 for the zero length
	 * transaction case and hope we don't cause grief for some
	 * arbitrary device due to random byte write/read during
	 * probes.
	 */
	if (msg->len == 0) {
		dev->buf = &zero_byte;
		dev->buf_len = 1;
	} else {
		dev->buf = msg->buf;
		dev->buf_len = msg->len;
	}

	regs->iccnt = dev->buf_len;
	init_completion(&dev->cmd_complete);
	dev->cmd_err = 0;

	/* Clear any pending interrupts by reading the IVR */
	stat = regs->icivr;

	/* Take I2C out of reset, configure it as master and set the
	 * start bit */
	flag =
	    DAVINCI_I2C_ICMDR_IRS_MASK | DAVINCI_I2C_ICMDR_MST_MASK |
	    DAVINCI_I2C_ICMDR_STT_MASK;

	/* if the slave address is ten bit address, enable XA bit */
	if (msg->flags & I2C_M_TEN)
		flag |= DAVINCI_I2C_ICMDR_XA_MASK;
	if (!(msg->flags & I2C_M_RD))
		flag |= DAVINCI_I2C_ICMDR_TRX_MASK;
	if (stop)
		flag |= DAVINCI_I2C_ICMDR_STP_MASK;

	/* Enable receive and transmit interrupts */
	if (msg->flags & I2C_M_RD)
		regs->icimr |= DAVINCI_I2C_ICIMR_ICRRDY_MASK;
	else
		regs->icimr |= DAVINCI_I2C_ICIMR_ICXRDY_MASK;

	/* write the data into mode register */
	regs->icmdr = flag;

	r = wait_for_completion_interruptible_timeout(&dev->cmd_complete,
						      DAVINCI_I2C_TIMEOUT);
	dev->buf_len = 0;
	if (r < 0)
		return r;
	if (r == 0) {
		dev_err(dev->dev, "controller timed out\n");

		/* Send the NACK to the slave */
		regs->icmdr |= DAVINCI_I2C_ICMDR_NACKMOD_MASK;
		/* Disable I2C */
		PINMUX1 &= (~(1 << 7));

		/* Set the GPIO direction register */
		GPIO23_DIR &= ~0x0800;

		/* Send high and low on the SCL line */
		for (i = 0; i < 10; i++) {
			GPIO23_SET |= 0x0800;
			udelay(25);
			GPIO23_CLR |= 0x0800;
			udelay(25);
		}
		/* Re-enable I2C */
		PINMUX1 |= (1 << 7);

		i2c_davinci_reset(dev);
		return -ETIMEDOUT;
	}

	/* no error */
	if (!dev->cmd_err)
		return msg->len;

	/* We have an error */
	if (dev->cmd_err & DAVINCI_I2C_ICSTR_NACK_MASK) {
		if (msg->flags & I2C_M_IGNORE_NAK)
			return msg->len;
		if (stop)
			regs->icmdr |= DAVINCI_I2C_ICMDR_STP_MASK;
		return -EREMOTEIO;
	}
	if (dev->cmd_err & DAVINCI_I2C_ICSTR_AL_MASK) {
		i2c_davinci_reset(dev);
		return -EIO;
	}
	return msg->len;
}

/*
 * Prepare controller for a transaction and call i2c_davinci_xfer_msg

 */
static int
i2c_davinci_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	struct i2c_davinci_device *dev = i2c_get_adapdata(adap);
	int count;
	int ret = 0;
	char retries = 5;

	dev_dbg(dev->dev, "%s: msgs: %d\n", __FUNCTION__, num);

	if (num < 1 || num > MAX_MESSAGES)
		return -EINVAL;

	/* Check for valid parameters in messages */
	for (count = 0; count < num; count++)
		if (msgs[count].buf == NULL)
			return -EINVAL;

	if ((ret = i2c_davinci_wait_for_bb(dev, 1)) < 0) {
		dev_warn(dev->dev, "timeout waiting for bus ready");
		return ret;
	}

	for (count = 0; count < num; count++) {
		dev_dbg(dev->dev,
			"%s: %d, addr: 0x%04x, len: %d, flags: 0x%x\n",
			__FUNCTION__,
			count, msgs[count].addr, msgs[count].len,
			msgs[count].flags);

		do {
			ret = i2c_davinci_xfer_msg(adap, &msgs[count],
						   (count == (num - 1)));

			if (ret < 0) {
				dev_dbg(dev->dev, "Retrying ...\n");
				mdelay (1);
				retries--;
			} else
				break;
		} while (retries);

		dev_dbg(dev->dev, "%s:%d ret: %d\n",
			__FUNCTION__, __LINE__, ret);

		if (ret != msgs[count].len)
			break;
	}

	if (ret >= 0 && num >= 1)
		ret = num;

	dev_dbg(dev->dev, "%s:%d ret: %d\n",
		__FUNCTION__, __LINE__, ret);

	return ret;
}

static u32 i2c_davinci_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

/*
 * This function marks a transaction as complete.
 */
static inline void i2c_davinci_complete_cmd(struct i2c_davinci_device *dev)
{
	complete(&dev->cmd_complete);
	wake_up(&dev->cmd_wait);
}

/*
 * Interrupt service routine. This gets called whenever an I2C interrupt
 * occurs.
 */
static irqreturn_t
i2c_davinci_isr(int this_irq, void *dev_id, struct pt_regs *reg)
{
	struct i2c_davinci_device *dev = dev_id;
	volatile struct davinci_i2cregs *regs = dev->regs;
	u32 stat;

	while ((stat = dev->regs->icivr) != 0) {
		dev_dbg(dev->dev, "%s: stat=0x%x\n", __FUNCTION__, stat);

		switch (stat) {
		case DAVINCI_I2C_ICIVR_INTCODE_AL:
			dev->cmd_err |= DAVINCI_I2C_ICSTR_AL_MASK;
			i2c_davinci_complete_cmd(dev);
			break;

		case DAVINCI_I2C_ICIVR_INTCODE_NACK:
			dev->cmd_err |= DAVINCI_I2C_ICSTR_NACK_MASK;
			i2c_davinci_complete_cmd(dev);
			break;

		case DAVINCI_I2C_ICIVR_INTCODE_RAR:
                        dev->regs->icstr |= DAVINCI_I2C_ICSTR_ARDY_MASK;
			i2c_davinci_complete_cmd(dev);
                        break;

		case DAVINCI_I2C_ICIVR_INTCODE_RDR:
			if (dev->buf_len) {
				*dev->buf++ = dev->regs->icdrr;
				dev->buf_len--;
				if (dev->buf_len) {
					continue;
				} else {
					regs->icimr &=
					    ~DAVINCI_I2C_ICIMR_ICRRDY_MASK;
				}
			}
			break;

		case DAVINCI_I2C_ICIVR_INTCODE_TDR:
			if (dev->buf_len) {
				regs->icdxr = *dev->buf++;
				dev->buf_len--;
				if (dev->buf_len)
					continue;
				else {
					regs->icimr &=
					    ~DAVINCI_I2C_ICIMR_ICXRDY_MASK;
				}
			}
			break;

		case DAVINCI_I2C_ICIVR_INTCODE_SCD:
                        regs->icstr |= DAVINCI_I2C_ICSTR_SCD_MASK;
                        i2c_davinci_complete_cmd(dev);
                        break;

		case DAVINCI_I2C_ICIVR_INTCODE_AAS:
			dev_warn(dev->dev, "Address as slave interrupt");
			break;

		default:
			break;
		}		/* switch */
	}			/* while */
	return IRQ_HANDLED;
}

static struct i2c_algorithm i2c_davinci_algo = {
	.master_xfer = i2c_davinci_xfer,
	.functionality = i2c_davinci_func,
};

static int
davinci_i2c_probe(struct platform_device *pdev)
{
	struct i2c_davinci_device *dev;
	struct i2c_adapter	*adap;
	struct resource		*mem, *irq;
	int ret = 0;

	dev = kmalloc(sizeof (struct i2c_davinci_device), GFP_KERNEL);
	if (dev == NULL) {
		ret = -ENOMEM;
		goto out_kmalloc;
	}

	/* NOTE: driver uses the static register mapping */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		ret = -ENODEV;
		goto out_nodev;
	}
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq) {
		dev_err(&pdev->dev, "no irq resource?\n");
		ret = -ENODEV;
		goto out_nodev;
	}

	ret = (int) request_mem_region(mem->start, mem->end - mem->start + 1,
			driver_name);
	if (!ret) {
		dev_err(&pdev->dev, "I2C region already claimed\n");
		ret = -EBUSY;
		goto out_nodev;
	}

	memset(dev, 0, sizeof(struct i2c_davinci_device));
	init_waitqueue_head(&dev->cmd_wait);
	dev->dev = &pdev->dev;

	/*
	 * NOTE: On DaVinci EVM, the i2c bus frequency is set to 20kHz
	 *	 so that the MSP430, which is doing software i2c, has
	 *	 some extra processing time
	 */
	if (machine_is_davinci_evm())
		bus_freq = 20;

	dev->clk = clk_get (&pdev->dev, "I2CCLK");
	if (IS_ERR(dev->clk)) {
		ret = -EINVAL;
		goto out_clk;
	}
	clk_enable(dev->clk);

	dev->regs = (struct davinci_i2cregs *)mem->start;
	i2c_davinci_reset(dev);

	dev->irq = irq->start;
	platform_set_drvdata(pdev, dev);

	ret = request_irq(dev->irq, i2c_davinci_isr, 0, driver_name, dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failure requesting irq %i\n", dev->irq);
		goto out_irq;
	}

	adap = &dev->adapter;
	i2c_set_adapdata(adap, dev);
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_HWMON;
	strncpy(adap->name, "DaVinci I2C adapter", sizeof(adap->name));
	adap->algo = &i2c_davinci_algo;
	adap->dev.parent = &pdev->dev;
	adap->client_register = NULL;
	adap->client_unregister = NULL;
	adap->timeout = 1;
	adap->retries = 1;

	/* i2c device drivers may be active on return from add_adapter() */
	ret = i2c_add_adapter(adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "failure adding adapter\n");
		goto out_adap;
	}

	return 0;

out_adap:
	free_irq(dev->irq, dev);
out_irq:
	clk_disable(dev->clk);
	clk_put(dev->clk);
	dev->clk = NULL;
out_clk:
	release_mem_region(mem->start, mem->end - mem->start + 1);
out_nodev:
	kfree(dev);
out_kmalloc:
	return ret;
}

static int
davinci_i2c_remove(struct platform_device *pdev)
{
	struct i2c_davinci_device *dev = platform_get_drvdata(pdev);
	struct resource *mem;
	volatile struct davinci_i2cregs *regs = dev->regs;

        clk_disable(dev->clk);
	clk_put(dev->clk);
	dev->clk = NULL;

	regs->icmdr = 0;
	free_irq(dev->irq, dev);

	i2c_del_adapter(&dev->adapter);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, mem->end - mem->start + 1);
	return 0;
}

static struct platform_driver davinci_i2c_driver = {
	.probe		= davinci_i2c_probe,
	.remove		= davinci_i2c_remove,
	.driver		= {
		.name	= (char *)driver_name,
	},
};

/* I2C may be needed to bring up other drivers */
static int __init davinci_i2c_init_driver(void)
{
	return platform_driver_register(&davinci_i2c_driver);
}
subsys_initcall(davinci_i2c_init_driver);

static void __exit davinci_i2c_exit_driver(void)
{
	platform_driver_unregister(&davinci_i2c_driver);
}
module_exit(davinci_i2c_exit_driver);

MODULE_AUTHOR("Texas Instruments India");
MODULE_DESCRIPTION("TI DaVinci I2C bus adapter");
MODULE_LICENSE("GPL");

