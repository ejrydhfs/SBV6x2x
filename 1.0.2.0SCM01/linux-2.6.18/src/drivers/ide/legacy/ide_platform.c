/*
 * Platform IDE driver
 *
 * Copyright (C) Kumar Gala <galak@kernel.crashing.org>
 * Copyright (C) 2007 MontaVista Software
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ide.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pata_platform.h>
#include <linux/io.h>

static struct {
	void __iomem *plat_ide_mapbase;
	void __iomem *plat_ide_alt_mapbase;
	ide_hwif_t *hwif;
	int index;
	int mmio;
} hwif_prop;

static ide_hwif_t *__devinit plat_ide_locate_hwif(void __iomem *base,
	    void __iomem *ctrl, struct pata_platform_info *pdata, int irq,
	    int mmio)
{
	unsigned long port = (unsigned long)base;
	ide_hwif_t *hwif;
	int index, i;

	for (index = 0; index < MAX_HWIFS; ++index) {
		hwif = ide_hwifs + index;
		if (hwif->io_ports[IDE_DATA_OFFSET] == port)
			goto found;
	}

	for (index = 0; index < MAX_HWIFS; ++index) {
		hwif = ide_hwifs + index;
		if (hwif->io_ports[IDE_DATA_OFFSET] == 0)
			goto found;
	}

	return NULL;

found:

	hwif->hw.io_ports[IDE_DATA_OFFSET] = port;

	port += (1 << pdata->ioport_shift);
	for (i = IDE_ERROR_OFFSET; i <= IDE_STATUS_OFFSET;
	     i++, port += (1 << pdata->ioport_shift))
		hwif->hw.io_ports[i] = port;

	hwif->hw.io_ports[IDE_CONTROL_OFFSET] = (unsigned long)ctrl;

	memcpy(hwif->io_ports, hwif->hw.io_ports, sizeof(hwif->hw.io_ports));
	hwif->hw.irq = hwif->irq = irq;

	hwif->hw.dma = NO_DMA;
	hwif->hw.chipset = ide_generic;

	if (mmio) {
		hwif->mmio = 2;
		default_hwif_mmiops(hwif);
	}

	hwif_prop.hwif = hwif;
	hwif_prop.index = index;
	hwif_prop.mmio = mmio;

	return hwif;
}

static int __devinit plat_ide_probe(struct platform_device *pdev)
{
	struct resource *res_base, *res_alt, *res_irq;
	ide_hwif_t *hwif;
	struct pata_platform_info *pdata;
	int mmio = 0;

	pdata = pdev->dev.platform_data;

	/* get a pointer to the register memory */
	res_base = platform_get_resource(pdev, IORESOURCE_IO, 0);
	res_alt = platform_get_resource(pdev, IORESOURCE_IO, 1);

	if (!res_base || !res_alt) {
		res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		res_alt = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res_base || !res_alt)
			return -EINVAL;
		mmio = 1;
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res_irq)
		return -EINVAL;

	if (mmio) {
		hwif_prop.plat_ide_mapbase = ioremap(res_base->start,
			res_base->end - res_base->start + 1);
		hwif_prop.plat_ide_alt_mapbase = ioremap(res_alt->start,
			res_alt->end - res_alt->start + 1);
	} else {
		hwif_prop.plat_ide_mapbase = ioport_map(res_base->start,
			res_base->end - res_base->start + 1);
		hwif_prop.plat_ide_alt_mapbase = ioport_map(res_alt->start,
			res_alt->end - res_alt->start + 1);
	}

	hwif = plat_ide_locate_hwif(hwif_prop.plat_ide_mapbase,
		hwif_prop.plat_ide_alt_mapbase, pdata, res_irq->start, mmio);

	if (!hwif)
		return -ENODEV;

	hwif->gendev.parent = &pdev->dev;
	hwif->noprobe = 0;

	probe_hwif_init(hwif);

	platform_set_drvdata(pdev, hwif);
	create_proc_ide_interfaces();

	return 0;
}

static int __devexit plat_ide_remove(struct platform_device *pdev)
{
	ide_hwif_t *hwif = pdev->dev.driver_data;

	if (hwif == hwif_prop.hwif) {
		ide_unregister(hwif_prop.index);
		if (hwif_prop.mmio) {
			iounmap(hwif_prop.plat_ide_mapbase);
			iounmap(hwif_prop.plat_ide_alt_mapbase);
		} else {
			ioport_unmap(hwif_prop.plat_ide_mapbase);
			ioport_unmap(hwif_prop.plat_ide_alt_mapbase);
		}
		hwif_prop.index = 0;
		hwif_prop.hwif = NULL;
	} else {
		dev_printk(KERN_DEBUG, &pdev->dev, "%s: hwif value error",
			   pdev->name);
	}

	return 0;
}

static struct platform_driver platform_ide_driver = {
	.driver = {
		.name = "pata_platform",
	},
	.probe = plat_ide_probe,
	.remove = __devexit_p(plat_ide_remove),
};

static int __init platform_ide_init(void)
{
	return platform_driver_register(&platform_ide_driver);
}

static void __exit platform_ide_exit(void)
{
	platform_driver_unregister(&platform_ide_driver);
}

MODULE_DESCRIPTION("Platform IDE driver");
MODULE_LICENSE("GPL");

module_init(platform_ide_init);
module_exit(platform_ide_exit);
