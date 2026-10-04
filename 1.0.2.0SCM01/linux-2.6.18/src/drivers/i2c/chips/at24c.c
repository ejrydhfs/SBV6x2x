/*
 * at24c.c -- support many I2C EEPROMs, including Atmel AT24C models
 *
 * Copyright (C) 2005-2006 David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#ifdef CONFIG_ARM
#include <asm/mach-types.h>
#endif


/* I2C EEPROMs from most vendors are inexpensive and mostly interchangeable.
 * Differences between different vendor product lines (like Atmel AT24C or
 * MicroChip 24LC, etc) won't much matter for typical read/write access.
 *
 * However, misconfiguration can lose data (e.g. "set 16 bit memory address"
 * could be interpreted as "write data at 8 bit address", or writing with too
 * big a page size), so board-specific EEPROM configuration should use static
 * board descriptions rather than dynamic probing when there's much potential
 * for confusion.
 *
 * Most such EEPROMS use addresses 0x50..0x57, but these of course are not
 * specific to such chips; a pcf8563 RTC uses 0x51, for example.
 *
 * Using /etc/modprobe.conf you might configure a system with several EEPROMs
 * (and using this driver as a module) something like this:
 *
 *   options at24c	chip_names=24c00,dimm,dimm,24c64  \
 *			force=0,50,1,50,1,51,2,52 \
 *			readonly=Y,Y,Y,N
 *
 * Or if you prefer something without error prone administrative steps, add
 * static board-specific code at the end of this driver, immediately before
 * this driver gets registered.
 *
 * CONFIGURATION MECHANISM IS SUBJECT TO CHANGE!!
 *
 * Key current differences from "eeprom" driver:
 *  (a) "at24c" also supports write access
 *  (b) "at24c" handles all common eeproms, not just 24c02 compatibles
 *  (c) "eeprom" will probe i2c and set up a 24c02 at each address;
 *      "at24c" works from a static config, with part types and addresses
 *  (d) "eeprom" handles SMBus-only hardware; "at24c" wants (*) real I2C.
 *
 * (*) Patches accepted...
 */
static unsigned short force[I2C_CLIENT_MAX_OPTS] = I2C_CLIENT_DEFAULTS;
static unsigned n_force;
module_param_array(force, ushort, &n_force, 0);
MODULE_PARM_DESC(force, "List of adapter,address pairs for I2C eeproms");

static unsigned short *forces[] = { force, NULL };

static unsigned short ignore[] = { I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	.normal_i2c	= ignore,
	.probe		= ignore,
	.ignore		= ignore,
	.forces		= forces,
};

static char *chip_names[I2C_CLIENT_MAX_OPTS / 2];
static unsigned n_chip_names;
module_param_array(chip_names, charp, &n_chip_names, 0);
MODULE_PARM_DESC(chip_names,
		"Type of chip; one chip name per force pair");

static int readonly[I2C_CLIENT_MAX_OPTS / 2];
static unsigned n_readonly;
module_param_array(readonly, bool, &n_readonly, 0);
MODULE_PARM_DESC(readonly, "Readonly flags, one per force pair");


/* As seen through Linux I2C, differences between the most common types
 * of I2C memory include:
 *	- How many I2C addresses the chip consumes: 1, 2, 4, or 8?
 *	- Memory address space for one I2C address:  256 bytes, or 64 KB?
 *	- How full that memory space is:  16 bytes, 256, 32Kb, etc?
 *	- What write page size does it support?
 */
struct chip_desc {
	u32		byte_len;		/* of 1..8 i2c addrs, total */
	char		name[10];
	u16		page_size;		/* for writes */
	u8		i2c_addr_mask;		/* for multi-addr chips */
	u8		flags;
#define	EE_ADDR2	0x0080			/* 16 bit addrs; <= 64 KB */
#define	EE_READONLY	0x0040
#define	EE_24RF08	0x0001			/* SMBUS_QUICK problem */
};

struct at24c_data {
	struct i2c_client	*client;
	struct semaphore	lock;
	struct bin_attribute	bin;

	struct chip_desc	chip;

	/* each chip has an internal "memory address" pointer;
	 * we remember it for faster read access.
	 */
	u32			next_addr;

	/* this device goes away with "new style" driver support */
	struct i2c_client	dev;

	/* some chips tie up multiple I2C addresses ... */
	int			users;
	struct i2c_client	extras[];
};

/* Specs often allow 5 msec for a page write, sometimes 20 msec;
 * it's important to recover from write timeouts.
 */
#define	EE_TIMEOUT	25


/*-------------------------------------------------------------------------*/

/* REVISIT want a way to remove the enumeration/probe infrastructure
 * after driver binding, in the common case where the infrastructure
 * (controllers, their drivers) is statically linked.
 */

static struct chip_desc chips[] = {

/* this first block of EEPROMS uses 8 bit memory addressing */
{
	/* 128 bit chip */
	.name		= "24c00",
	.byte_len	= 128 / 8,
	.i2c_addr_mask	= 0x07,			/* I2C A0-A2 ignored */
	.page_size	= 1,
}, {
	/* 1 Kbit chip */
	.name		= "24c01",
	.byte_len	= 1024 / 8,
	/* some have 16 byte pages: */
	.page_size	= 8,
}, {
	/* 2 Kbit chip */
	.name		= "24c02",
	.byte_len	= 2048 / 8,
	/* some have 16 byte pages: */
	.page_size	= 8,
}, {
	/* 2 Kbit chip */
	.name		= "dimm",		/* 24c02 in memory DIMMs */
	.byte_len	= 2048 / 8,
	.flags		= EE_READONLY,
	/* some have 16 byte pages: */
	.page_size	= 8,
}, {
	/* 4 Kbit chip */
	.name		= "24c04",
	.byte_len	= 4096 / 8,
	.page_size	= 16,
	.i2c_addr_mask	= 0x01,			/* I2C A0 is MEM A8 */
}, {
	/* 8 Kbit chip */
	.name		= "24c08",
	.byte_len	= 8192 / 8,
	.page_size	= 16,
	.i2c_addr_mask	= 0x03,			/* I2C A1-A0 is MEM A9-A8 */
}, {
	/* 8 Kbit chip */
	.name		= "24rf08",
	.byte_len	= 8192 / 8,
	.flags		= EE_24RF08,
	.page_size	= 16,
	.i2c_addr_mask	= 0x03,			/* I2C A1-A0 is MEM A9-A8 */
}, {
	/* 16 Kbit chip */
	.name		= "24c16",
	.byte_len	= 16384 / 8,
	.page_size	= 16,
	.i2c_addr_mask	= 0x07,			/* I2C A2-A0 is MEM A10-A8 */
},

/* this second block of EEPROMS uses 16 bit memory addressing */
{
	/* 32 Kbits */
	.name		= "24c32",
	.byte_len	= 32768 / 8,
	.flags		= EE_ADDR2,
	.page_size	= 32,
}, {
	/* 64 Kbits */
	.name		= "24c64",
	.byte_len	= 65536 / 8,
	.flags		= EE_ADDR2,
	.page_size	= 32,
}, {
	/* 128 Kbits */
	.name		= "24c128",
	.byte_len	= 131072 / 8,
	.flags		= EE_ADDR2,
	.page_size	= 64,
}, {
	/* 256 Kbits */
	.name		= "24c256",
	.byte_len	= 262144 / 8,
	.flags		= EE_ADDR2,
	.page_size	= 64,
}, {
	/* 512 Kbits */
	.name		= "24c512",
	.byte_len	= 524288 / 8,
	.flags		= EE_ADDR2,
	.page_size	= 128,
}, {
	/* 1 Mbits */
	.name		= "24c1024",
	.byte_len	= 1048576 / 8,
	.flags		= EE_ADDR2,
	.page_size	= 256,
	.i2c_addr_mask	= 0x01,			/* I2C A0 is MEM A16 */
}

};

static inline const struct chip_desc *
find_chip(const char *s)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(chips); i++)
		if (strnicmp(s, chips[i].name, sizeof chips[i].name) == 0)
			return &chips[i];
	return NULL;
}

static inline const struct chip_desc *
which_chip(struct i2c_adapter *adapter, int address, int *writable)
{
	unsigned		i;

	for (i = 0; i < n_force; i++) {
		const struct chip_desc	*chip;
		char			*name;

		if (force[i++] != adapter->nr)
			continue;
		if (force[i] != address)
			continue;

		i >>= 1;
		if ((name = chip_names[i]) == NULL) {
			dev_err(adapter->class_dev.dev,
				"no chipname for addr %d\n",
				address);
			return NULL;

		}
		chip = find_chip(name);
		if (chip == NULL)
			dev_err(adapter->class_dev.dev,
				"unknown chipname %s\n", name);

		/* user specified r/o value will overide the default */
		if (i < n_readonly)
			*writable = !readonly[i];
		else
			*writable = !(chip->flags & EE_READONLY);
		return chip;
	}

	/* "can't happen" */
	dev_dbg(adapter->class_dev.dev,
		"addr 0x%02x not in force[]\n",
		address);
	return NULL;
}

/*-------------------------------------------------------------------------*/

/* This parameter is to help this driver avoid blocking other drivers
 * out of I2C for potentially troublesome amounts of time.  With a
 * 100 KHz I2C clock, one 256 byte read takes about 1/43 second.
 * VALUE MUST BE A POWER OF TWO so writes will align on pages!!
 */
static unsigned io_limit = 128;
module_param(io_limit, uint, 0);
MODULE_PARM_DESC(io_limit, "maximum bytes per i/o (default 128)");

static inline int at24c_ee_address(
	const struct chip_desc	*chip,
	struct i2c_msg		*msg,
	unsigned		*offset,
	u32			*next_addr
)
{
	unsigned	per_address = 256;

	if (*offset >= chip->byte_len)
		return -EINVAL;

	/* Nothing to do unless accessing that memory would go to some
	 * later I2C address.  In that case, modify the output params.
	 * Yes, it can loop more than once on 24c08 and 24c16.
	 */
	if (chip->flags & EE_ADDR2)
		per_address = 64 * 1024;
	while (*offset >= per_address) {
		msg->addr++;
		*offset -= per_address;
		*next_addr -= per_address;
	}
	return 0;
}

static ssize_t
at24c_ee_read(
	struct at24c_data	*at24c,
	char			*buf,
	unsigned		offset,
	size_t			count
)
{
	struct i2c_msg		msg;
	ssize_t			status;
	u32			next_addr;
	size_t			transferred = 0;

	down(&at24c->lock);
	msg.addr = at24c->client->addr;
	msg.flags = 0;

	/* maybe adjust i2c address and offset
	 * NOTE:  we could now search at24c->extras to choose use
	 * some i2c_client other than the main one...
	 */
	next_addr = at24c->next_addr;
	status = at24c_ee_address(&at24c->chip, &msg, &offset, &next_addr);
	if (status < 0)
		goto done;

	/* Maybe change the current on-chip address with a dummy write */
	if (next_addr != offset) {
		u8	tmp[2];

		msg.buf = tmp;
		tmp[1] = (u8) offset;
		tmp[0] = (u8) (offset >> 8);
		if (at24c->chip.flags & EE_ADDR2) {
			/* NOTE:  could i2c_smbus_write_byte_data() */
			msg.len = 2;
		} else {
			/* NOTE:  could i2c_smbus_write_byte() */
			msg.len = 1;
			msg.buf++;
		}
		status = i2c_transfer(at24c->client->adapter, &msg, 1);
		dev_dbg(&at24c->client->dev,
				"addr %02x, set current to %d --> %d\n",
				msg.addr, offset, status);
		if (status != 1) {
			if (status == 0)
				status = -EIO;
			goto done;
		}
		next_addr = at24c->next_addr = offset;
	}

	/* issue bounded sequential reads for the data bytes, knowing that
	 * read page rollover goes to the next page and/or memory block.
	 */
	while (count > 0) {
		unsigned	segment;

		/* NOTE:  could use many i2c_smbus_read_byte() calls,
		 * if the adapter doesn't support I2C ... or sometimes
		 * i2c_smbus_read_i2c_block_data().
		 */
		if (count > io_limit)
			segment = io_limit;
		else
			segment = count;

		msg.len = segment;
		msg.buf = buf;
		msg.flags = I2C_M_RD;
		status = i2c_transfer(at24c->client->adapter, &msg, 1);
		dev_dbg(&at24c->client->dev, "read %d, %d --> %d\n",
				transferred, count, status);

		if (status != 1) {
			if (status == 0)
				status = -EIO;
			break;
		}

		count -= segment;

		at24c->next_addr += segment;
		offset += segment;
		buf += segment;
		transferred += segment;
	}
done:
	if (status <= 0)
		at24c->next_addr = at24c->bin.size;
	up(&at24c->lock);
	return transferred ? transferred : status;
}

static ssize_t
at24c_bin_read(struct kobject *kobj, char *buf, loff_t off, size_t count)
{
	struct i2c_client	*client;
	struct at24c_data	*at24c;

	client = to_i2c_client(container_of(kobj, struct device, kobj));
	at24c = i2c_get_clientdata(client);

	if (unlikely(off >= at24c->bin.size))
		return 0;
	if ((off + count) > at24c->bin.size)
		count = at24c->bin.size - off;
	if (unlikely(!count))
		return count;

	/* we don't maintain caches for any data:  simpler, cheaper */
	return at24c_ee_read(at24c, buf, off, count);
}


/* Note that if the hardware write-protect pin is pulled high, the whole
 * chip is normally write protected.  But there are plenty of product
 * variants here, including OTP fuses and partial chip protect.
 *
 * For now, we only support page mode writes; the alternative is sloooow.
 * SMBus block writes are no help at all; they always embed the length.
 */
static ssize_t
at24c_ee_write(struct at24c_data *at24c, char *buf, loff_t off, size_t count)
{
	struct i2c_msg		msg;
	ssize_t			status = 0;
	unsigned		written = 0;
	u32			scratch;
	unsigned		buf_size;
	unsigned long		timeout, retries;
	/* temp buffer lets us stick the address at the beginning */
	buf_size = at24c->chip.page_size;
	if (buf_size > io_limit)
		buf_size = io_limit;
//	msg.buf = kmalloc(buf_size + 2, GFP_KERNEL);
	msg.buf = kzalloc(buf_size + 2, GFP_KERNEL);

	if (!msg.buf)
		return -ENOMEM;
	msg.flags = 0;

	/* For write, rollover is within the page ... so we write at
	 * most one page, then manually roll over to the next page.
	 */
	down(&at24c->lock);
	timeout = jiffies + msecs_to_jiffies(EE_TIMEOUT);
	retries = 0;
	do {
		unsigned	segment;
		unsigned	offset = (unsigned) off;

		/* maybe adjust i2c address and offset */
		msg.addr = at24c->client->addr;
		status = at24c_ee_address(&at24c->chip, &msg,
						&offset, &scratch);
		if (status < 0)
			break;

		/* write as much of a page as we can */
		segment = buf_size - (offset % buf_size);
		if (segment > count)
			segment = count;
		msg.len = segment + 1;
		if (at24c->chip.flags & EE_ADDR2) {
			msg.len++;
			msg.buf[1] = (u8) offset;
			msg.buf[0] = (u8) (offset >> 8);
			memcpy(&msg.buf[2], buf, segment);
		} else {
			msg.buf[0] = offset;
			memcpy(&msg.buf[1], buf, segment);
		}

		/* The chip may take a while to finish its previous write;
		 * this write also polls for completion of the last one.
		 * So we always retry a few times.
		 */
		status = i2c_transfer(at24c->client->adapter, &msg, 1);
		dev_dbg(&at24c->client->dev,
			"write %d bytes to %02x at %d --> %d (%ld)\n",
			segment, msg.addr, offset, status, jiffies);
		if (status < 0 ) {
			if (retries++ < 3 || time_after(timeout, jiffies)) {
				/* REVISIT:  at HZ=100, this is sloooow */
				msleep(1);
				continue;
			}
			dev_err(&at24c->client->dev,
				"write %d bytes offset %d to %02x, "
				"%ld ticks err %d\n",
				segment, offset, msg.addr,
				jiffies - (timeout - EE_TIMEOUT),
				(int) status);
			status = -ETIMEDOUT;
			break;
		}

		off += segment;
		buf += segment;
		count -= segment;
		written = segment;
		if (status < 0 || count == 0) {
			if (written != 0)
				status = written;
			break;
		}

		/* yielding may avoid the losing msleep() path above */
		yield();
		timeout = jiffies + msecs_to_jiffies(EE_TIMEOUT);
		retries = 0;
	} while (count > 0);
	at24c->next_addr = at24c->bin.size;
	up(&at24c->lock);

	kfree(msg.buf);
	return status;
}

static ssize_t
at24c_bin_write(struct kobject *kobj, char *buf, loff_t off, size_t count)
{
	struct i2c_client	*client;
	struct at24c_data	*at24c;
	int ret;
	client = to_i2c_client(container_of(kobj, struct device, kobj));
	at24c = i2c_get_clientdata(client);

	if (unlikely(off >= at24c->bin.size))
		return -EFBIG;
	if ((off + count) > at24c->bin.size)
		count = at24c->bin.size - off;
	if (unlikely(!count))
		return count;
	ret = at24c_ee_write(at24c, buf, off, count);
	return ret;
}

/*-------------------------------------------------------------------------*/

static struct i2c_driver at24c_driver;

static int
at24c_activate(
	struct i2c_adapter	*adapter,
	int			address,
	const struct chip_desc	*chip,
	int			writable
)
{
	struct i2c_client	*client;
	struct at24c_data	*at24c;
	int			err;
	if (!(at24c = kcalloc(1, sizeof(struct at24c_data)
			+ chip->i2c_addr_mask * sizeof(struct i2c_client),
			GFP_KERNEL)))
		return -ENOMEM;

	init_MUTEX(&at24c->lock);
	at24c->chip = *chip;

	client = &at24c->dev;
	strncpy(client->name, at24c->chip.name, I2C_NAME_SIZE);

	/* Export the EEPROM bytes through sysfs, since that's convenient.
	 * By default, only root should see the data (maybe passwords etc)
	 */
	at24c->bin.attr.name = "eeprom";
	at24c->bin.attr.mode = S_IRUSR;
	at24c->bin.attr.owner = THIS_MODULE;
	at24c->bin.read = at24c_bin_read;

	at24c->bin.size = at24c->chip.byte_len;
	at24c->next_addr = at24c->bin.size;
	if (writable) {
		at24c->bin.write = at24c_bin_write;
		at24c->bin.attr.mode |= S_IWUSR;
	}

	/* register the first chip address */
	address &= ~at24c->chip.i2c_addr_mask;

	client->addr = address;
	client->adapter = adapter;
	client->driver = &at24c_driver;
	client->flags = 0;

	/* prevent AT24RF08 corruption, a possible consequence of the
	 * probe done by the i2c core (verifying a chip is present).
	 */
	if (chip->flags & EE_24RF08)
		i2c_smbus_write_quick(at24c->client, 0);

	/* FIXME read a byte, to make sure the chip is actually
	 * present (vs misconfiguration, or not-populated-here)
	 */

	at24c->client = client;
	i2c_set_clientdata(client, at24c);

	if ((err = i2c_attach_client(at24c->client)))
		goto fail;
	at24c->users++;

	/* then register any other addresses, so other drivers can't */
	if (chip->i2c_addr_mask) {
		unsigned		i;
		struct i2c_client	*c;

		for (i = 0; i < chip->i2c_addr_mask; i++) {
			c = &at24c->extras[i];
			c->addr = client->addr + i + 1;
			c->adapter = client->adapter;
			c->driver = &at24c_driver;

			i2c_set_clientdata(c, at24c);
			snprintf(c->name, sizeof c->name,
				"%s address %d", chip->name, i + 2);
			err = i2c_attach_client(c);
			if (err == 0)
				at24c->users++;
		}
	}
	err = sysfs_create_bin_file(&client->dev.kobj, &at24c->bin);
	if (err != 0) {
		(void) i2c_detach_client(at24c->client);
fail:
		dev_dbg(&client->dev, "probe err %d\n", err);
		kfree(at24c);
		return err;
	}

	dev_info(&client->dev, "%Zd byte %s EEPROM%s\n",
		at24c->bin.size, chip->name,
		writable ? " (writable)" : "");
	dev_dbg(&client->dev, "page_size %d, i2c_addr_mask %d\n",
		at24c->chip.page_size, at24c->chip.i2c_addr_mask);
	return 0;

}

static int
at24c_old_probe(struct i2c_adapter *adapter, int address, int kind)
{
	const struct chip_desc	*chip;
	int writable = 0;
	int ret; 
	chip = which_chip(adapter, address, &writable);
	ret =  at24c_activate(adapter, address, chip, writable); 
	return chip ? ret : 0;
}


static int at24c_attach_adapter(struct i2c_adapter *adapter)
{
	/* REVISIT: using SMBUS calls would improve portability, though
	 * at the cost of performance, especially for writes where
	 * page mode support wouldn't be available.
	 */
	int ret;
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_dbg(adapter->class_dev.dev, "%s probe, no I2C\n",
			at24c_driver.driver.name);
		return 0;
	}

	ret = i2c_probe(adapter, &addr_data, at24c_old_probe); 
	return ret;
}

static int __devexit at24c_detach_client(struct i2c_client *client)
{
	int			err;
	struct at24c_data	*at24c;

	err = i2c_detach_client(client);
	if (err) {
		dev_err(&client->dev, "deregistration failed, %d\n", err);
		return err;
	}

	/* up to eight clients per chip, detached in any order... */
	at24c = i2c_get_clientdata(client);
	if (at24c->users-- == 0) {
		kfree(at24c);
	}
	return 0;
}

/*-------------------------------------------------------------------------*/

static struct i2c_driver at24c_driver = {
	.driver = {
		.name		= "at24c",
		.owner		= THIS_MODULE,
	},
	.attach_adapter	= at24c_attach_adapter,
	.detach_client	= __devexit_p(at24c_detach_client),
};

static int __init at24c_init(void)
{	
	int ret;
#ifdef	CONFIG_ARM
	/* There are several runtime config mechanisms too (command line from
	 * bootloader or kernel default, /etc/modprobe.conf) but some of us
	 * like less error-prone configuration management.  Ideally this data
	 * would be in arch/arm/mach-xxx/board-yyy.c tables, with other devices.
	 */

#ifdef CONFIG_OMAP_OSK_MISTRAL
	if (machine_is_omap_osk()) {
		n_chip_names = 1;
		n_force = 2;
		/* lcd/touchscreen board has 512 Byte eeprom */
		chip_names[0] = "24c04";
		force[0] = 0;
		force[1] = 0x50;
	}
#endif	/* OSK + Mistral */

	if (machine_is_at91rm9200dk()) {
		n_chip_names = 1;
		n_force = 2;
		/* 128 KByte bootable eeprom */
		chip_names[0] = "24c1024";
		force[0] = 0;
		force[1] = 0x50;
	}

	if (machine_is_davinci_evm()) {
		n_chip_names = 1;
		n_force = 2;
		/* 32 KByte eeprom */
		chip_names[0] = "24c256";
		force[0] = 0;
		force[1] = 0x50;

		/* msp430 firmware workaround */
		if (io_limit > 32)
			io_limit = 32;
	}

	if (machine_is_omap_h4()) {
		n_chip_names = 2;
		n_force = 4;
		/* 128 Byte eeprom on mainboard (WP = S2.2) */
		chip_names[0] = "24c01";
		force[0] = 0;
		force[1] = 0x52;
		/* 128 Byte eeprom on cpu card (WP = S3.4) */
		chip_names[1] = "24c01";
		force[2] = 0;
		force[3] = 0x57;
	}
   /* Do we need to statically fix it or probe for Client device ?*/
	if (machine_is_puma5()) {
		n_chip_names = 1;
		n_force = 2;
		/* 1024 Byte (8k) eeprom */
		chip_names[0] = "24c08";
		force[0] = 0;
		force[1] = 0x50; /* 0x51 */
		force[2] = 0x51; /* 0x51 */
		force[3] = 0x52; /* 0x51 */
		force[4] = 0x53; /* 0x51 */


        }
#endif /* ARM */

	ret = i2c_add_driver(&at24c_driver);	
	return ret;
	
}
module_init(at24c_init);

static void __exit at24c_exit(void)
{
	i2c_del_driver(&at24c_driver);
}
module_exit(at24c_exit);

MODULE_DESCRIPTION("Driver for most I2C EEPROMs");
MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");


