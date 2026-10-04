/*
 * drivers/usb/host/ehci-arc.c
 *
 * Copyright 2005-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup USB ARC OTG USB Driver
 */
/*!
 * @file ehci-arc.c
 * @brief platform related part of usb host driver.
 * @ingroup USB
 */

/*!
 * Include files
 */

/* Note: this file is #included by ehci-hcd.c */

#include <linux/platform_device.h>
#include <linux/usb_otg.h>

#include <asm/arch/arc_otg.h>

#undef dbg
#define dbg(fmt, ...) do {} while (0)
#undef vdbg
#define vdbg(fmt, ...) do {} while (0)

static inline void set_host_mode(struct usb_hcd *hcd)
{
	u32 tmp = readl(hcd->regs + 0x1a8);
	writel(tmp | USBMODE_CM_HC, hcd->regs + 0x1a8);
}

/**
 * usb_hcd_arc_probe - initialize ARC HCDs
 * @drvier: Driver to be used for this HCD
 * @pdev: USB Host Controller being probed
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller.
 *
 */
static int usb_hcd_arc_probe(const struct hc_driver *driver,
			     struct platform_device *pdev)
{
	struct arc_usb_config *pdata;
	struct usb_hcd *hcd;
	struct resource *res;
	int irq;
	int retval;

	pr_debug("initializing ARC USB Controller\n");

	/* Need platform data for setup */
	pdata = (struct arc_usb_config *)pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev,
			"No platform data for %s.\n", pdev->dev.bus_id);
		return -ENODEV;
	}

	/*
	 * do platform specific init: check the clock, grab/config pins, etc.
	 */
	if (pdata->platform_init && pdata->platform_init()) {
		retval = -ENODEV;
		goto err1;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no IRQ. Check %s setup!\n",
			pdev->dev.bus_id);
		return -ENODEV;
	}
	irq = res->start;

	if (pdata->set_vbus_power)
		pdata->set_vbus_power(1);

	hcd = usb_create_hcd(driver, &pdev->dev, pdev->dev.bus_id);
	if (!hcd) {
		retval = -ENOMEM;
		goto err1;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no register addr. Check %s setup!\n",
			pdev->dev.bus_id);
		retval = -ENODEV;
		goto err2;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1;

	vdbg("start=0x%x   end=0x%x    rsrc_start=0x%llx  rsrc_len=0x%llx\n",
	     res->start, res->end, hcd->rsrc_start, hcd->rsrc_len);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
				driver->description)) {
		dev_dbg(&pdev->dev, "request_mem_region failed\n");
		retval = -EBUSY;
		goto err2;
	}
	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);

	if (hcd->regs == NULL) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		retval = -EFAULT;
		goto err3;
	}

	hcd->power_budget = pdata->power_budget;

	/* DDD
	 * the following must be done by this point, otherwise the OTG
	 * host port doesn't make it thru initializtion.
	 * ehci_halt(), called by ehci_arc_setup() returns -ETIMEDOUT
	 */
	set_host_mode(hcd);

	retval = usb_add_hcd(hcd, irq, SA_SHIRQ);
	if (retval != 0)
		goto err4;

	return retval;

err4:
	iounmap(hcd->regs);
err3:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err2:
	usb_put_hcd(hcd);
err1:
	dev_err(&pdev->dev, "init %s fail, %d\n", pdev->dev.bus_id, retval);
	if (pdata->platform_uninit)
		pdata->platform_uninit();
	return retval;
}

static void usb_hcd_arc_remove(struct usb_hcd *hcd,
			       struct platform_device *pdev)
{
	struct arc_usb_config *pdata;

	pdata = (struct arc_usb_config *)pdev->dev.platform_data;

	dbg("%s  hcd=0x%p\n", __FUNCTION__, hcd);

	usb_remove_hcd(hcd);
	if (pdata->set_vbus_power)
		pdata->set_vbus_power(0);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);

	/*
	 * do platform specific un-initialization:
	 * release iomux pins, etc.
	 */
	if (pdata->platform_uninit)
		pdata->platform_uninit();
}

/* called during probe() after chip reset completes */
static int ehci_arc_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval;

	/* EHCI registers start at offset 0x00 */
	ehci->caps = hcd->regs + 0x100;
	ehci->regs = hcd->regs + 0x100 +
	    HC_LENGTH(readl(&ehci->caps->hc_capbase));

	vdbg("%s(): ehci->caps=0x%p  ehci->regs=0x%p\n", __FUNCTION__,
	     ehci->caps, ehci->regs);

	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

	retval = ehci_halt(ehci);
	if (retval)
		return retval;

	/* data structure init */
	retval = ehci_init(hcd);
	if (retval)
		return retval;

	ehci->is_tdi_rh_tt = 1;

	ehci->sbrn = 0x20;

	ehci_reset(ehci);
	ehci_port_power(ehci, 0);

	return 0;
}

static const struct hc_driver ehci_arc_hc_driver = {
	.description	= hcd_name,
	.product_desc	= "Freescale On-Chip EHCI Host Controller",
	.hcd_priv_size	= sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq		= ehci_irq,
	.flags		= HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	.reset		= ehci_arc_setup,
	.start		= ehci_run,
	.stop		= ehci_stop,
	.shutdown	= ehci_shutdown,
#ifdef CONFIG_PM
	.suspend	= ehci_bus_suspend,
	.resume		= ehci_bus_resume,
#endif

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ehci_hub_control,
	.bus_suspend		= ehci_bus_suspend,
	.bus_resume		= ehci_bus_resume,
};

static int ehci_hcd_drv_probe(struct platform_device *pdev)
{
	if (usb_disabled())
		return -ENODEV;

	return usb_hcd_arc_probe(&ehci_arc_hc_driver, pdev);
}

static int __init_or_module ehci_hcd_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_hcd_arc_remove(hcd, pdev);

	return 0;
}

static struct platform_driver ehci_arc_driver = {
	.probe   = ehci_hcd_drv_probe,
	.remove  = ehci_hcd_drv_remove,
	.driver  = {
			.name = "arc-ehci",
		   },
};
