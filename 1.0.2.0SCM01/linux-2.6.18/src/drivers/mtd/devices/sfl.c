/*
 *
 * sfl.c
 * Description:
 * serial flash mtd driver. This driver currently handles all flash devices.
 * TODO :
 *   1. It is difficult to provide a common protection layer for all serial
 *      flashes and hence driver currently does not support locking/unlocking
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
 * Change Log:
 * May-2007 	- 	srinath						: First version for spansion
 * Oct-2007		-	mansoor.ahamed@ti.com		: Spansion SFI cleanup
 * Nov-2007		-	mansoor.ahamed@ti.com		: Multi manufacturer support
 * Apr-2008		-	mansoor.ahamed@ti.com		: Cleanup for multi flash probe
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/spi/avalanche_spi.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>


/********************** Debug helpers ******************/
#define __MODULE__ "sfl"

#if defined(CONFIG_SPI_DEBUG)
#define DEBUG_SFL(fmt,arg...)  printk(KERN_EMERG "%s:%s():line %d : " fmt , __MODULE__, __FUNCTION__, __LINE__, ##arg);
#else
#define DEBUG_SFL(fmt,arg...)
#endif

#define PRINTK(fmt,arg...)  printk(KERN_EMERG "%s:%s():line %d : " fmt , __MODULE__, __FUNCTION__, __LINE__, ##arg);

#define ENTER() DEBUG_SFL("Enter\n");
#define EXIT()  DEBUG_SFL("Exit\n");



/****************** Module param defaults ***************/
/* 8 seconds max delay */
#define SFL_DELAY						(msecs_to_jiffies(8000))
#define SFL_FAST_READ					1 /* NOT same as Dual Read */
#define SFL_NORMAL_READ					0
#define SFL_NUM_ADDR_BYTES				3
#define	FAST_READ_NUM_DUMMY_BYTES		1
#define NORMAL_READ_NUM_DUMMY_BYTES		0
#define SFL_CMD_FRAME_SIZE				4

/*****************************************************
*	Status Register bits.
*****************************************************/
#define	SR_WIP							0x01	/* Write in progress */
#define	SR_WEL							0x02	/* Write enable latch */
#define	SR_BP0							0x04	/* Block protect 0 */
#define	SR_BP1							0x08	/* Block protect 1 */
#define	SR_BP2							0x10	/* Block protect 2 */
#define	SR_BP3							0x20	/* Block protect 2 */
#define	SR_SRWD							0x80	/* SR write Disable */

#ifdef CONFIG_MTD_PARTITIONS
#define	mtd_has_partitions()			(1)
#else
#define	mtd_has_partitions()			(0)
#endif

struct sfl {
	unsigned char 	    command[5];
	unsigned int        partitioned;
	struct spi_device	*spi;
	struct semaphore	lock;
	struct mtd_info		mtd;
	unsigned int index;
};

/* serial flash command table */
struct sflash_cmd_table {
	unsigned char r; /* read */
	unsigned char fr; /* fast read */
	unsigned char rdid; /* read device id */
	unsigned char wren; /* write enable */
	unsigned char wrdi; /* write disable */
	unsigned char se; /* sector erase */
	unsigned char be; /* block erase */
	unsigned char ce; /* chip erase */
	unsigned char pp; /* page program */
	unsigned char cp; /* continuously program whole chip */
	unsigned char rdsr; /* read status reg */
	unsigned char wrsr; /* write status reg */
	unsigned char dp; /* deep power down */
	unsigned char rd; /* release deep power down */
	unsigned char res; /* read electronic device id */
};
/* default serial flash command table */
static const struct sflash_cmd_table def_cmd_table =
{
	.r 	= 0x03,
	.fr	= 0x0b,
	.rdid	= 0x9f,
	.wren	= 0x06,
	.wrdi	= 0x04,
	.se	= 0xd8,
	.be	= 0xd8,
	.ce	= 0xc7,
	.pp	= 0x02,
	.rdsr	= 0x05,
	.wrsr	= 0x01,
	.dp	= 0xb9,
	.rd	= 0xab,
	.res	= 0xab
};

/* serial flash command tables */
/* macronix cmd set */
static const struct sflash_cmd_table mx25lxxxx_cmd_table =
{
	.r 	= 0x03,
	.fr	= 0x0b,
	.rdid	= 0x9f,
	.wren	= 0x06,
	.wrdi	= 0x04,
	.se	= 0x20,
	.be	= 0xd8,
	.ce	= 0xc7,
	.pp	= 0x02,
	.rdsr	= 0x05,
	.wrsr	= 0x01,
	.dp	= 0xb9,
	.rd	= 0xab,
	.res	= 0xab
};


/* serial flash meta data structure */
struct sfl_flash_info {
    char    *name;
    int     (*probe)(struct spi_device   *spi, int index);
    unsigned int 	id;
    unsigned int	sector_size;
    unsigned int	n_sectors;
    int				page_size;
    unsigned int	lock_all;
    unsigned int	n_regions;
    unsigned int	prot_ratio;
    #define SFL_MAX_REGIONS	(5)
    struct mtd_erase_region_info *regions;
    const struct sflash_cmd_table *cmd;
    unsigned int sectors_per_block;
};

/* sector size in different flashes */
#define S25FLXXXX_SECT_SIZE		(64*1024)
#define S25FL128P00_SECT_SIZE		(256*1024)
#define S25FL128P01_SECT_SIZE		(64*1024)
#define M25FL128P_SECT_SIZE		(256*1024)
#define M25FL64P_SECT_SIZE		(64*1024)
#define MX25LXXXX_SECT_SIZE		(4*1024)

/* sectors per block */
#define S25FLXXXX_SECT_PER_BLOCK	(1)
#define M25FLXXXX_SECT_PER_BLOCK	(1)
#define MX25LXXXX_SECT_PER_BLOCK	(16)

/* page size */
#define S25FLXXXX_PAGE_SIZE		(256)
#define M25FLXXXX_PAGE_SIZE		(256)
#define MX25LXXXX_PAGE_SIZE		(256)

/* different Manufacturer ids */
#define S25FLxxxx_ID			(0x01)
#define M25FLxxxx_ID			(0x20)
#define MX25Lxxxx_ID			(0xC2)


/* different device ids */
#define S25FL004A_ID			(0x0212)
#define S25FL008A_ID			(0x0213)
#define S25FL016A_ID			(0x0214)
#define S25FL032A_ID			(0x0215)
#define S25FL064A_ID			(0x0216)
#define S25FL128P_ID 			(0x2018)
#define M25FL128P_ID 			(0x2018)
#define M25FL64P_ID 			(0x2017)
#define MX25L1605_ID 			(0x2015)
#define MX25L3205_ID 			(0x2016)
#define MX25L6405_ID 			(0x2017)
#define MX25L12805_ID 			(0x2018)


/* sector numbers in different flashes */
#define N_S25FL004A_SECTORS		(  8)
#define N_S25FL008A_SECTORS		( 16)
#define N_S25FL016A_SECTORS		( 32)
#define N_S25FL032A_SECTORS		( 64)
#define N_S25FL064A_SECTORS		(128)
#define N_S25FL128P00_SECTORS 		( 64)
#define N_S25FL128P01_SECTORS 		(256)
#define N_M25FL128P_SECTORS 		( 64)
#define N_M25FL64P_SECTORS		(128)
#define N_MX25L1605_SECTORS		(512)
#define N_MX25L3205_SECTORS		(1024)
#define N_MX25L6405_SECTORS		(2048)
#define N_MX25L12805_SECTORS		(4096)

/* protection ratio start */
/* TODO : protection is not used currently */
#define S25FL004A_PROT_RATIO			(  8)
#define S25FL008A_PROT_RATIO			( 16)
#define S25FL016A_PROT_RATIO			( 32)
#define S25FL032A_PROT_RATIO			( 64)
#define S25FL064A_PROT_RATIO			( 64)
#define S25FL128P00_PROT_RATIO 			( 64)
#define S25FL128P01_PROT_RATIO 			(128)
#define M25FL128P_PROT_RATIO 			(0xFFFFFFFF)
#define MX25LXXXX_PROT_RATIO 			(0xFFFFFFFF)

/* different flashes region info */
/* spansion flashes */
static struct mtd_erase_region_info s25fl004a_regions =
	{0, S25FLXXXX_SECT_SIZE, N_S25FL004A_SECTORS};

static struct mtd_erase_region_info s25fl008a_regions =
	{0, S25FLXXXX_SECT_SIZE, N_S25FL008A_SECTORS};

static struct mtd_erase_region_info s25fl016a_regions =
	{0, S25FLXXXX_SECT_SIZE, N_S25FL016A_SECTORS};

static struct mtd_erase_region_info s25fl032a_regions =
	{0, S25FLXXXX_SECT_SIZE, N_S25FL032A_SECTORS};

static struct mtd_erase_region_info s25fl064a_regions =
	{0, S25FLXXXX_SECT_SIZE, N_S25FL064A_SECTORS};

static struct mtd_erase_region_info s25fl128p_regions =
	{0, S25FLXXXX_SECT_SIZE, N_S25FL128P01_SECTORS};

/* numonyix flashes */
static struct mtd_erase_region_info m25fl128p_regions =
	{0, M25FL128P_SECT_SIZE, N_M25FL128P_SECTORS};

static struct mtd_erase_region_info m25fl64p_regions =
	{0, M25FL64P_SECT_SIZE, N_M25FL64P_SECTORS};

/* macronix flashes */
static struct mtd_erase_region_info mx25l1605_regions =
	{0, MX25LXXXX_SECT_SIZE, N_MX25L1605_SECTORS};

static struct mtd_erase_region_info mx25l3205_regions =
	{0, MX25LXXXX_SECT_SIZE, N_MX25L3205_SECTORS};

static struct mtd_erase_region_info mx25l6405_regions =
	{0, MX25LXXXX_SECT_SIZE, N_MX25L6405_SECTORS};

static struct mtd_erase_region_info mx25l12805_regions =
	{0, MX25LXXXX_SECT_SIZE, N_MX25L12805_SECTORS};


/* returns 1 on detection */
static int m25flxxxx_probe(struct spi_device *spi, int index);
static int s25fl128p_probe(struct spi_device *spi, int index);
static int s25fl0xxx_probe(struct spi_device *spi, int index);
static int mx25lxxxx_probe(struct spi_device *spi, int index);

/* empty fields will be filled by probe func */
static struct sfl_flash_info sfl_data [] = {
    { "s25fl004a", s25fl0xxx_probe, S25FL004A_ID, S25FLXXXX_SECT_SIZE,
			N_S25FL004A_SECTORS, S25FLXXXX_PAGE_SIZE, 0x1C, 1,
			S25FL004A_PROT_RATIO, &s25fl004a_regions, &def_cmd_table, S25FLXXXX_SECT_PER_BLOCK },

    { "s25fl008a", s25fl0xxx_probe, S25FL008A_ID, S25FLXXXX_SECT_SIZE,
			N_S25FL008A_SECTORS, S25FLXXXX_PAGE_SIZE, 0x1C, 1,
			S25FL008A_PROT_RATIO, &s25fl008a_regions, &def_cmd_table, S25FLXXXX_SECT_PER_BLOCK },

    { "s25fl016a", s25fl0xxx_probe, S25FL016A_ID, S25FLXXXX_SECT_SIZE,
			N_S25FL016A_SECTORS, S25FLXXXX_PAGE_SIZE, 0x1C, 1,
			S25FL016A_PROT_RATIO, &s25fl016a_regions, &def_cmd_table, S25FLXXXX_SECT_PER_BLOCK },

    { "s25fl032a", s25fl0xxx_probe, S25FL032A_ID, S25FLXXXX_SECT_SIZE,
			N_S25FL032A_SECTORS, S25FLXXXX_PAGE_SIZE, 0x1C, 1,
			S25FL032A_PROT_RATIO, &s25fl032a_regions, &def_cmd_table, S25FLXXXX_SECT_PER_BLOCK },

    { "s25fl064a", s25fl0xxx_probe, S25FL064A_ID, S25FLXXXX_SECT_SIZE,
			N_S25FL064A_SECTORS, S25FLXXXX_PAGE_SIZE, 0x1C, 1,
			S25FL064A_PROT_RATIO, &s25fl064a_regions, &def_cmd_table, S25FLXXXX_SECT_PER_BLOCK },

    { "s25fl128p", s25fl128p_probe, S25FL128P_ID, 0,
			0, S25FLXXXX_PAGE_SIZE, 0x3C, 1,
			0, &s25fl128p_regions, &def_cmd_table, S25FLXXXX_SECT_PER_BLOCK },

    { "m25fl128p", m25flxxxx_probe, M25FL128P_ID, M25FL128P_SECT_SIZE,
			N_M25FL128P_SECTORS, M25FLXXXX_PAGE_SIZE, 0x3C, 1,
			M25FL128P_PROT_RATIO, &m25fl128p_regions, &def_cmd_table, M25FLXXXX_SECT_PER_BLOCK },

    { "m25fl64p", m25flxxxx_probe, M25FL64P_ID, M25FL64P_SECT_SIZE,
			N_M25FL64P_SECTORS, M25FLXXXX_PAGE_SIZE, 0x3C, 1,
			M25FL128P_PROT_RATIO, &m25fl64p_regions, &def_cmd_table, M25FLXXXX_SECT_PER_BLOCK },

    { "mx25l1605", mx25lxxxx_probe, MX25L1605_ID, MX25LXXXX_SECT_SIZE,
			N_MX25L1605_SECTORS, MX25LXXXX_PAGE_SIZE, 0x3C, 1,
			MX25LXXXX_PROT_RATIO, &mx25l1605_regions, &mx25lxxxx_cmd_table, MX25LXXXX_SECT_PER_BLOCK },

    { "mx25l3205", mx25lxxxx_probe, MX25L3205_ID, MX25LXXXX_SECT_SIZE,
			N_MX25L3205_SECTORS, MX25LXXXX_PAGE_SIZE, 0x3C, 1,
			MX25LXXXX_PROT_RATIO, &mx25l3205_regions, &mx25lxxxx_cmd_table, MX25LXXXX_SECT_PER_BLOCK },

    { "mx25l6405", mx25lxxxx_probe, MX25L6405_ID, MX25LXXXX_SECT_SIZE,
			N_MX25L6405_SECTORS, MX25LXXXX_PAGE_SIZE, 0x3C, 1,
			MX25LXXXX_PROT_RATIO, &mx25l6405_regions, &mx25lxxxx_cmd_table, MX25LXXXX_SECT_PER_BLOCK },

    { "mx25l12805", mx25lxxxx_probe, MX25L12805_ID, MX25LXXXX_SECT_SIZE,
			N_MX25L12805_SECTORS, MX25LXXXX_PAGE_SIZE, 0x3C, 1,
			MX25LXXXX_PROT_RATIO, &mx25l12805_regions, &mx25lxxxx_cmd_table, MX25LXXXX_SECT_PER_BLOCK },
};

/* Helper macros */
#define UNLOCK_ALL_SECTORS				(0x0)
#define DUMMY_ADDRESS					(0x0000)

#define NUM_REGIONS(i)					(sfl_data[i].n_regions)
#define NUM_SECTORS(i) 					(sfl_data[i].n_sectors)
#define SFL_PAGESIZE(i)					(sfl_data[i].page_size)
#define LOCK_ALL_SECTORS(i)				(sfl_data[i].lock_all)
#define PROTECT_BITS_MASK(i)			(LOCK_ALL_SECTORS(i))
#define SFL_SECTOR_SIZE(i)				(sfl_data[i].sector_size)
#define SFL_SIZE(i)						(SFL_SECTOR_SIZE(i) * NUM_SECTORS(i))
#define SFL_PROT_RATIO(i)				(sfl_data[i].prot_ratio)
#define ERASE_REGION(i)					(sfl_data[i].regions)
#define SFL_SECT_PER_BLOCK(i)				(sfl_data[i].sectors_per_block)

/* Command Table Access Macros  */
/* Read */
#define OPCODE_READ(i)                	(sfl_data[i].cmd->r)
#define OPCODE_FAST_READ(i)				(sfl_data[i].cmd->fr)
#define OPCODE_READ_DEV_ID(i)    		(sfl_data[i].cmd->rdid)
/* Write Control */
#define OPCODE_WRITE_ENABLE(i) 			(sfl_data[i].cmd->wren)
#define OPCODE_WRITE_DISABLE(i)    		(sfl_data[i].cmd->wrdi)
/* Erase */
#define OPCODE_SECTOR_ERASE(i)    		(sfl_data[i].cmd->se)
#define OPCODE_CHIP_ERASE(i)     		(sfl_data[i].cmd->ce)
#define OPCODE_BLOCK_ERASE(i)    		(sfl_data[i].cmd->be)

/* Program */
#define OPCODE_PAGE_PROGRAM(i)  		(sfl_data[i].cmd->pp)
/* Status Regs */
#define OPCODE_READ_STATUS_REG(i)    	(sfl_data[i].cmd->rdsr)
#define OPCODE_WRITE_STATUS_REG(i)    	(sfl_data[i].cmd->wrsr)
/* Power Saving */
#define OPCODE_DEEP_POWER_DOWN(i) 		(sfl_data[i].cmd->dp)
#define OPCODE_RELEASE_POWER_DOWN(i) 	(sfl_data[i].cmd->rd)
#define OPCODE_READ_JEDEC_ID(i)  		(sfl_data[i].cmd->res)


/* this is specific to sfl (may be for all spi flash devices) */
struct sfl_sfi_transfer_hdr_t
{
    unsigned int len;
    unsigned int tx_addr;
    unsigned int rx_addr;
 	void *tx_buff;
    void *rx_buff;
};


/* Global variables */
static s32 sfi_mode = -1;
static s32 fast_read = -1;
static u16 sfl_sfi_mode =  CONFIG_SFL_SFI;
static u16 sfl_fast_read = CONFIG_SFL_FAST_READ;
static struct sfl	flash_info[2];
/* Function definitions */

static inline struct sfl *mtd_to_sfl(struct mtd_info *mtd)
{
	return container_of(mtd, struct sfl, mtd);
}

#if 0 /* might require in future for locking/unlocking support */
/*
 * get_protect_bits_val return the value for the address range to lock the
 * flash.
 * Returns protection bit value on success.
 */
static unsigned char get_protect_bits_val(struct sfl *flash,
						unsigned int start_addr, unsigned int end_addr)
{
	ENTER();
	if( end_addr > SFL_SIZE(flash->index) )
	{
		EXIT();
		return 0;
	}
	return 0;
/* TODO */
#if 0
	if(start_addr >= 0x7E0000){
		return 0x04;
	}else if( start_addr >= 0x7C0000 ){
		return 0x08;
	}else if( start_addr >= 0x780000 ){
		return 0x0C;
	}else if( start_addr >= 0x700000 ){
		return 0x10;
	}else if( start_addr >= 0x600000 ){
		return 0x14;
	}else if( start_addr >= 0x400000 ){
		return 0x18;
	}else if ( start_addr >= 0x000000){
		return 0x1C;
	}else{
		return 0;
	}
#endif
}
#endif

/*
 * write_status_reg writes the vlaue to status reg.
 * Returns status on success or negetive valueif error.
 * NOTE:  write enable should be called before calling this function.
 */
static int write_status_reg(struct sfl *flash,unsigned char  val)
{
	s32 ret = 0;
	unsigned char write_val[2] = {OPCODE_WRITE_STATUS_REG(flash->index), val};
	DEBUG_SFL();
	spi_setup(flash->spi);
	if((ret = spi_write(flash->spi, write_val, 2)) < 0)
		PRINTK("failed\n");
	return 0;
}

/*
 * read_status_reg return the status of the flash
 * Returns status on success or negetive valueif error.
 */
static int read_status_reg(struct sfl *flash, unsigned char *val)
{
	ssize_t ret;
	unsigned char code = OPCODE_READ_STATUS_REG(flash->index);
	ENTER();
	*val = 0;

	spi_setup(flash->spi);
	ret = spi_write_then_read(flash->spi, &code, 1, val, 1);
	if( ret < 0 )
	{
		dev_err(&flash->spi->dev, "error %d reading SR\n",(int) ret);
		PRINTK("error %d reading SR\n",(int) ret);
	}
	EXIT();
	return ret;
}

/*
 * write_enable perpares the flash state to enable write/erase/lock/unlock
 * Returns 0 on success or negetive valueif error.
 */
static inline int write_enable(struct sfl *flash)
{
	ssize_t ret;
	unsigned char	code = OPCODE_WRITE_ENABLE(flash->index);

	ENTER();
	spi_setup(flash->spi);
	ret = spi_write(flash->spi, &code,1 );
	return ret;
}


/*
 * wait_till_ready to read status register until ready, or timeout occurs.
 * Returns non-zero if error.
 */
static int wait_till_ready(struct sfl *flash)
{
	unsigned char sr;
	unsigned long timeout = jiffies + SFL_DELAY;
	ENTER();
	do{
		if(read_status_reg(flash, &sr)) {
			break;
		}else if (!(sr & SR_WIP)){
			return 0;
		}
		mdelay(1);
	}while(time_after(timeout, jiffies));
	EXIT();
	PRINTK("wait_till_ready failed\n");
	return 1;
}

/* if flash wait times out then returns error */
#define WAIT_FOR_FLASH_READY(flash, lock, error) \
	if(wait_till_ready(flash) )\
	{ \
		PRINTK("wait_till_ready failed\n");\
		up(&lock); \
		return error; \
	}

/*
 * Erase an address range on the flash chip.  The address range may extend
 * one or more erase sectors.  Return an error is there is a problem erasing.
 */
static int sfl_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct sfl *flash = mtd_to_sfl(mtd);
	unsigned int addr;
	int len;
	unsigned char status;
	int ret = 0;

	DEBUG_SFL("addr=0x%08x, len=%d\n", (unsigned int)instr->addr, instr->len);

	/* sanity checks */
	if( ( instr->addr + instr->len ) > flash->mtd.size)
	{
		PRINTK("invalid address (or) length\n");
		return -EINVAL;
	}
	if ( (( instr->addr & ( mtd->erasesize - 1 ) ) != 0 )	||
		 ( ( instr->len & ( mtd->erasesize - 1 ) ) != 0 )  )
	{
		PRINTK("invalid address (or) length, exceeds partition size\n");
		return -EINVAL;
	}

  	down(&flash->lock);

	DEBUG_SFL( "Erasing address 0x%x, length 0x%x\n",  instr->addr, instr->len );

	instr->state = 	MTD_ERASING;
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
	if(read_status_reg(flash, &status) < 0)	{
		instr->state = MTD_ERASE_FAILED;
		ret = -EIO;
		goto erase_err;
	}
	status &= PROTECT_BITS_MASK(flash->index);

	/* Check if flash is locked */
	if(status)
	{
	    /* sfl unlock layout fix */
	    status = UNLOCK_ALL_SECTORS;

	    write_enable(flash);
	    WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
	    if((ret = write_status_reg(flash,status))){
		    PRINTK("write_status_reg failed\n");
	    }

	    /* Wait until finished previous write command. */
	    WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

		if(read_status_reg(flash, &status) < 0)	{
			ret = -EIO;
			goto erase_err;
		}

		status &= PROTECT_BITS_MASK(flash->index);

		if(status)
		{
		    PRINTK("ERROR: Erasing in locked region BPx = 0x%0x\n",status);
		    ret = -EROFS;
		    goto erase_err;
		}
  	}

	addr = instr->addr;
	len = instr->len;

	/* now erase these sectors */
	while( len > 0)
	{
	    unsigned int block_erase = 0;

	    /* Send write enable, then erase commands. */
	    write_enable(flash);
	    WAIT_FOR_FLASH_READY(flash, flash->lock, -EIO);

	    /* For Macronix, erase blocks when possible for faster performance */
	    if( SFL_SECT_PER_BLOCK(flash->index) > 1 )
	    {
			/* Check if we are in block boundry, and if erasing a block will not overflow */
			if ( ( ( addr & ( ( mtd->erasesize*SFL_SECT_PER_BLOCK(flash->index) ) - 1 ) ) == 0) && ( len >= ( mtd->erasesize*SFL_SECT_PER_BLOCK(flash->index) ) ) )
			{
			   block_erase = 1;
			   DEBUG_SFL( "Block erase, addr 0x%x\n", addr );
			}
	    }

	    /* Set up command buffer. */
	    flash->command[0] = block_erase ? OPCODE_BLOCK_ERASE(flash->index) : OPCODE_SECTOR_ERASE(flash->index);
	    flash->command[1] = addr >> 16;
	    flash->command[2] = addr >> 8;
	    flash->command[3] = addr;
	    spi_write(flash->spi, flash->command,SFL_CMD_FRAME_SIZE);

	    WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

	    if( !block_erase )
	    {
			addr += mtd->erasesize;
			len -= mtd->erasesize;
	    }
	    else
	    {
			addr += ( mtd->erasesize * SFL_SECT_PER_BLOCK(flash->index) );
			len -= ( mtd->erasesize * SFL_SECT_PER_BLOCK(flash->index) );
	    }
	}
	instr->state = MTD_ERASE_DONE;
erase_err:
  	up(&flash->lock);
	mtd_erase_callback(instr);
	return ret;
}

/*
 * Read an address range from the flash chip.  The address range
 * may be any size provided it is within the physical boundaries.
 */

static int sfl_read( struct mtd_info *mtd, loff_t from, size_t len,
						  size_t *retlen, u_char *buf)
{
	struct sfl *flash = mtd_to_sfl(mtd);
	struct spi_transfer t[2];
	struct spi_message m;
	struct sfl_sfi_transfer_hdr_t sfi_hdr;
	unsigned int page_offset, page_size,i;
	unsigned short index = 0;
	unsigned char old_mode,temp;
	int ret = 0;

	DEBUG_SFL("%s %s 0x%08x, len %zd\n ", __FUNCTION__,"from",(unsigned int)from, len);

	if( !len ){
		PRINTK("Invalid length\n");
		return 0;
	}
	if( ((from + len ) > flash->mtd.size) || (retlen == NULL)){
		PRINTK("Argument exceeds partitions size \n");
		return -EINVAL;
	}
	down(&flash->lock);
	spi_message_init(&m);
	memset(t, 0, sizeof(t));
	/* we need the flash command packet only if we are operating in core SFI
	 * mode
	 */
	if( !sfl_sfi_mode )
	{
		t[index].tx_buf = flash->command;
		/* only fast read needs dummy byte address */
		if(sfl_fast_read == SFL_NORMAL_READ){
			t[index].len = SFL_CMD_FRAME_SIZE;
		}else {
			/* In Fast read mode ADDRESS + DUMMY BYTE */
			t[index].len = (SFL_CMD_FRAME_SIZE + 1);
		}
		DEBUG_SFL("length of command is %d\n",t[index].len);
		spi_message_add_tail(&t[index], &m);
		index++;
    	t[index].rx_buf = buf;
	}
	else
	{
		DEBUG_SFL(" In function %s initializing sfi_hdr index = %d \n",
				__FUNCTION__,index);

		memset(&sfi_hdr, 0, sizeof(sfi_hdr));
		sfi_hdr.len 	= len;
		sfi_hdr.rx_buff = buf;
		sfi_hdr.rx_addr = from;
		t[index].rx_buf = &sfi_hdr;
		t[index].len = sizeof(sfi_hdr);

		DEBUG_SFL("sfl sfi header = 0x%0x, len=%d rx_addr=0x%0x\n",
   			   (unsigned)t[index].rx_buf, sfi_hdr.len, sfi_hdr.rx_addr);
	}

	spi_message_add_tail(&t[index], &m);
	*retlen = 0;

	/* Wait till previous write/erase is done. */
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

	/* if we are working in sfi mode then do the wholke read and
	 *	return immediately
 	 */
	if(sfl_sfi_mode)
	{
		DEBUG_SFL("Switching to SFI MODE\n");
		SFI_SET_MODE(flash->spi,AVALANCHE_SFI_MODE,old_mode);
		DEBUG_SFL("Calling SPI_SYNC in SFI mode \n");
		spi_sync(flash->spi, &m);
		DEBUG_SFL("Switching back to old mode \n");
		SFI_SET_MODE(flash->spi,old_mode,temp);
		*retlen = sfi_hdr.len;
		goto SFL_READ_REARM;
	}
	/* sfl read in core spi mode */
	if(sfl_fast_read == SFL_NORMAL_READ)
	{
		DEBUG_SFL("initializing the NORMAL READ CMD \n");
		flash->command[0] = OPCODE_READ(flash->index);
	}else {
		DEBUG_SFL("initializing the FAST READ CMD \n");
		flash->command[0] = OPCODE_FAST_READ(flash->index);
	}
	flash->command[1] = from >> 16;
	flash->command[2] = from >> 8;
	flash->command[3] = from;
	flash->command[4] = DUMMY_ADDRESS;

	/* what page do we start with? */
	page_offset = from & ( SFL_PAGESIZE(flash->index) - 1 );

	/* do all the bytes fit onto one page? */
    if (page_offset + len <= SFL_PAGESIZE(flash->index)) {
		DEBUG_SFL("Read with in a page \n");
        t[index].len = len;
        spi_sync(flash->spi, &m);
		DEBUG_SFL("returned = %d index = %d, cmd size = %d\n",
			   m.actual_length, index, t[0].len);
        *retlen = m.actual_length - t[0].len;
    }
	else
	{
		DEBUG_SFL("Read Multiple pages \n");
        /* the size of data remaining on the first page */
	    page_size = SFL_PAGESIZE(flash->index) - page_offset;
        t[index].len = page_size;
        spi_sync(flash->spi, &m);
        *retlen = m.actual_length - t[0].len;

        /* write everything in PAGESIZE chunks */
        for (i = page_size; i < len; i += page_size)
		{
            page_size = len - i;

            if (page_size > SFL_PAGESIZE(flash->index))
			{
                page_size = SFL_PAGESIZE(flash->index);
			}

			if(sfl_fast_read == SFL_NORMAL_READ)
			{
				DEBUG_SFL("NORMAL READ CMD \n");
				flash->command[0] = OPCODE_READ(flash->index);
			}else {
				DEBUG_SFL("FAST READ  CMD \n");
				flash->command[0] = OPCODE_FAST_READ(flash->index);
			}
    	    flash->command[1] = (from + i) >> 16;
        	flash->command[2] = (from + i) >> 8;
           	flash->command[3] = (from + i);
			flash->command[4] = DUMMY_ADDRESS;

            t[index].rx_buf = buf + i;
            t[index].len = page_size;

			WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

            spi_sync(flash->spi, &m);

            if (retlen){
				DEBUG_SFL("returned = %d index = %d, cmd size = %d\n",
					m.actual_length, index,t[0].len);
                *retlen += m.actual_length - (t[0].len);
			}
		}
    }

SFL_READ_REARM:
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
  	up(&flash->lock);
	DEBUG_SFL("Actual Length : %d\n", *retlen);
	return ret;

}

static int sfl_write( struct mtd_info *mtd, loff_t to, size_t len,
							   size_t *retlen, const u_char *buf)
{

/*****************************************************************************
     Currently Write is not supported in SFI mode so use SPI Mode writes
*****************************************************************************/
	struct sfl *flash = mtd_to_sfl(mtd);
	unsigned int page_offset, page_size;
	struct spi_transfer t[2];
	struct spi_message m;
	unsigned char status = 0,old_mode;
	int ret = 0;

	ENTER();
	DEBUG_SFL("Write to  0x%08x, len %zd\n",(unsigned int)to, len);

	if( retlen ){
		*retlen = 0;
	}

	/* sanity checks */
	if( !len ){
		PRINTK("invalid length\n");
		return -EINVAL;
	}

	if( ( to + len ) >= flash->mtd.size){
		PRINTK("length exceeds partition size\n");
		return -EINVAL;
	}

  	down(&flash->lock);

	/* Wait until finished previous write command. */
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

	if(read_status_reg(flash, &status) < 0)	{
		ret = -EIO;
		goto write_err;
	}

	status &= PROTECT_BITS_MASK(flash->index);

	/* Check if flash is locked */
	if(status)
	{
		DEBUG_SFL("Warning: Writing in locked region BPx = 0x%0x, trying to unlock\n",status);

		/* sfl unlock layout fix */
		status = UNLOCK_ALL_SECTORS;

		write_enable(flash);
		WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
		if((ret = write_status_reg(flash,status))){
			PRINTK("write_status_reg failed\n");
		}

		/* Wait until finished previous write command. */
		WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

		if(read_status_reg(flash, &status) < 0)	{
			ret = -EIO;
			goto write_err;
		}

		status &= PROTECT_BITS_MASK(flash->index);

		if(status)
		{
		    PRINTK("ERROR: Writing in locked region BPx = 0x%0x\n",status);
		    ret = -EROFS;
		    goto write_err;
		}
  	}

	memset(t, 0, (sizeof t));
	spi_message_init(&m);
	t[0].tx_buf = flash->command;
	/* operating in Core SPI Mode CMD length needed is 4 bytes no
	   dummy byte required
	*/
	/* Take care while implementing SFI write */
	t[0].len = SFL_CMD_FRAME_SIZE;
	spi_message_add_tail(&t[0], &m);

	t[1].tx_buf = buf;
	spi_message_add_tail(&t[1], &m);


	DEBUG_SFL("Inside the function %s:%d\n",__FUNCTION__, __LINE__);

	write_enable(flash);
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
	/* Writes is only supported in CORE_SPI mode so make sure to be
	 * in CORE_SPI mode
	 */
	SFI_SET_MODE(flash->spi, AVALANCHE_CORE_SPI_MODE, old_mode);

	/* Set up the opcode in the write buffer. */
	flash->command[0] = OPCODE_PAGE_PROGRAM(flash->index);
	flash->command[1] = to >> 16;
	flash->command[2] = to >> 8;
	flash->command[3] = to;

	/* what page do we start with? */
	page_offset = to & ( SFL_PAGESIZE(flash->index) - 1 );

	/* do all the bytes fit onto one page? */
	if (page_offset + len <= SFL_PAGESIZE(flash->index))
	{
		t[1].len = len;
		DEBUG_SFL("Within Single Page, page_offset = %d, len = %d, total = %d\n", page_offset, len, page_offset+len );
		spi_sync(flash->spi, &m);
		*retlen = m.actual_length - t[0].len;
	}
	else
	{
		unsigned int i;
		DEBUG_SFL("Spawning multiple pages, page_offset = %d, len = %d, total = %d\n", page_offset, len, page_offset+len );

		/* the size of data remaining on the first page */
		page_size = SFL_PAGESIZE(flash->index) - page_offset;

		t[1].len = page_size;
		spi_sync(flash->spi, &m);

		/* operating in Core SPI Mode CMD length needed is 4 bytes no dummy byte
		 *	required
		 */
		*retlen = m.actual_length - t[0].len;

		/* write everything in PAGESIZE chunks */
		for (i = page_size; i < len; i += SFL_PAGESIZE(flash->index))
		{
			/* Short delay is required in order to prevent the flash from entering a non-recoverable busy state */
			udelay(10);
		    WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
		    page_size = len - i;

		    if (page_size > SFL_PAGESIZE(flash->index))
				page_size = SFL_PAGESIZE(flash->index);

		    DEBUG_SFL( "Write i = %d, page_size = %d\n", i, page_size );

		    /* write the next page to flash */
		    flash->command[0] = OPCODE_PAGE_PROGRAM(flash->index);
		    flash->command[1] = (to + i) >> 16;
		    flash->command[2] = (to + i) >> 8;
		    flash->command[3] = (to + i);

		    t[1].tx_buf = buf + i;
		    t[1].len = page_size;

		    WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
		    write_enable(flash);
		    WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
		    spi_sync(flash->spi, &m);
		    *retlen += m.actual_length 	- t[0].len;
 		}
	}
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
write_err:
	up(&flash->lock);
	return ret;
}


static int sfl_unlockall(struct mtd_info *mtd)
{
	struct sfl *flash = mtd_to_sfl(mtd);
	unsigned char  status;
	s32 ret=0;

	down(&flash->lock);
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

	/* sfl unlock layout fix */
	status = UNLOCK_ALL_SECTORS;

	write_enable(flash);
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
	if((ret = write_status_reg(flash,status))){
		PRINTK("write_status_reg failed\n");
	}else{
		DEBUG_SFL("Whole flash unlocked\n");
	}
	DEBUG_SFL(" 1 Status reg = 0x%x", read_status_reg(flash));
	up(&flash->lock);
	return ret;
}


/* Lock and Unlock functions */
static  int sfl_lock (struct mtd_info *mtd, loff_t ofs, size_t len)
{
	s32 ret = 0;
/* TODO */
#if 0
	struct sfl *flash = mtd_to_sfl(mtd);
	unsigned char status = 0;

	ENTER();
	/* sanity checks */
	if( ( ofs + len ) > flash->mtd.size)
	{
		PRINTK("Argument exceeds partition size\n");
		return -EINVAL;
	}
	down(&flash->lock);
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
	status = get_protect_bits_val(flash, ofs,(ofs + len));
	DEBUG_SFL("Status reg = 0x%x\n",(unsigned)status);

	/* sfl lock layout fix */
	status = LOCK_ALL_SECTORS(flash->index);

	if ( status <= (unsigned char) LOCK_ALL_SECTORS(flash->index) )
	{
		write_enable(flash);
		WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
		if((ret = write_status_reg(flash,status)) != 0)
		{
			PRINTK("write_status_reg failed\n");
		}else{
			DEBUG_SFL("Sector(s) corrosponding to Address  0x%8x to  0x%8x is \
					 locked\n",(unsigned)ofs,(unsigned) (ofs +len) );
		}
	}
	up(&flash->lock);
#endif
	return ret;
}

/*
 *sfl_unlock unlocks the Faash sector form the  given address  to
 * MAx flash address.
 * returns 0 on success , negetive value otherwise
 */
static int sfl_unlock (struct mtd_info *mtd, loff_t ofs, size_t len)
{
	s32 ret=0;
/* TODO */
#if 0
	struct sfl *flash = mtd_to_sfl(mtd);
	unsigned char  status;
	unsigned char  temp;

	DEBUG_SFL("addr = 0x%8x and len = %d\n",(unsigned)ofs,(unsigned)len);

	/* sanity checks */
	if( ( ofs + len ) > flash->mtd.size)
	{
		PRINTK("Argument exceeds partition size\n");
		return -EINVAL;
	}
	down(&flash->lock);
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);

	status = read_status_reg(flash);
	status &= PROTECT_BITS_MASK(flash->index);

	/* unlock the whole flash */
	if((len + ofs) >= (SFL_SIZE(flash->index) - (SFL_SECTOR_SIZE(flash->index) *2)))
	{
		DEBUG_SFL("Unlocking the whole flash \n");
		status = 0;
	}
	else
	{
		temp = get_protect_bits_val(flash, (ofs + len), SFL_SIZE(flash->index));
		if(status > temp)
			status = temp;
	}

	DEBUG_SFL("0 Status reg = 0x%x", read_status_reg(flash));

	/* sfl unlock layout fix */
	status = UNLOCK_ALL_SECTORS;

	if ( status <= (unsigned char) LOCK_ALL_SECTORS(flash->index) )
	{
		write_enable(flash);
		WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
		if((ret = write_status_reg(flash,status))){
			PRINTK("write_status_reg failed\n");
		}else{
			DEBUG_SFL("Sector(s) corrosponding to Address 0x%x to  0x%x is unlocked\n",
					(unsigned)ofs, (unsigned )(ofs +len));
		}
	}
	DEBUG_SFL(" 1 Status reg = 0x%x", read_status_reg(flash));
	up(&flash->lock);
#endif
	return ret;
}

/* Power Management functions */
static int sfl_suspend (struct mtd_info *mtd)
{
	struct sfl *flash = mtd_to_sfl(mtd);
	struct spi_transfer t[2];
	struct spi_message m;
	s32 ret=0;
	memset(t, 0, (sizeof t));
	spi_message_init(&m);

	t[0].tx_buf = flash->command;
	/* operating in Core SPI Mode CMD length needed is 4 bytes no
	 *  dummy byte required
	 */
	t[0].len = SFL_CMD_FRAME_SIZE;
	spi_message_add_tail(&t[0], &m);

	ENTER();
  	down(&flash->lock);
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
	write_enable(flash);
	WAIT_FOR_FLASH_READY(flash, flash->lock, -EBUSY);
	/* Set up the opcode in the write buffer. */
	flash->command[0] = OPCODE_DEEP_POWER_DOWN(flash->index);
	flash->command[1] = DUMMY_ADDRESS >> 16;
	flash->command[2] = DUMMY_ADDRESS >> 8;
	flash->command[3] = DUMMY_ADDRESS;
	DEBUG_SFL("Going to Deep Sleep ..............Bye\n");
	spi_sync(flash->spi, &m);
  	up(&flash->lock);
	return ret;

}

static void sfl_resume (struct mtd_info *mtd)
{
	struct sfl *flash = mtd_to_sfl(mtd);
	struct spi_transfer t[2];
	struct spi_message m;
	memset(t, 0, (sizeof t));
	spi_message_init(&m);

	t[0].tx_buf = flash->command;
	/* operating in Core SPI Mode CMD length needed is 4 bytes no
	 *  dummy byte required
	 */
	t[0].len = SFL_CMD_FRAME_SIZE;
	spi_message_add_tail(&t[0], &m);
	ENTER();
	/* No other command works just wake up First */
  	down(&flash->lock);
    wait_till_ready(flash);
	write_enable(flash);
	/* write enable requires wait till ready */
    wait_till_ready(flash);
	/* Set up the opcode in the write buffer. */
	flash->command[0] = OPCODE_RELEASE_POWER_DOWN(flash->index);
	flash->command[1] = DUMMY_ADDRESS >> 16;
	flash->command[2] = DUMMY_ADDRESS >> 8;
	flash->command[3] = DUMMY_ADDRESS;
	DEBUG_SFL("waking Up form ........ Deep Sleep Hi :)\n");
	spi_sync(flash->spi, &m);
  	up(&flash->lock);
	return;
}


/*
 * sfl_sfi_transfer Client transfer function used in SFI mode of
 * operation.
 * On success returns number of bytes transfered otherwise 0.
 */
int sfl_sfi_transfer(struct spi_device *spi, struct spi_transfer *t)
{
	int i = 0;
    struct sfl_sfi_transfer_hdr_t *sfi_hdr = NULL;

	 struct avalanche_sfi_dev_info_t *sfi =
		(struct avalanche_sfi_dev_info_t *)spi->controller_data;

	volatile unsigned char *s_ptr = NULL;
	volatile unsigned char *d_ptr = NULL;

	ENTER();

	if(sfi == NULL)
	{
		PRINTK("Got NULL Pointer sfi = 0x%x\n",(unsigned)sfi);
		return 0;
	}

	if(sfi->mode != AVALANCHE_SFI_MODE) {
		PRINTK("Not in SFI mode\n");
		return 0;
	}

	if(t->rx_buf != NULL ){
	   DEBUG_SFL("t->rx_buf is 0x%x\n",(unsigned)t->rx_buf );
	   sfi_hdr = (struct sfl_sfi_transfer_hdr_t*)(t->rx_buf);
	}
	else
	{
		DEBUG_SFL("t->rx_buf is NULL 0x%x\n",(unsigned)t->rx_buf );
		return 0;
	}

    if(sfi_hdr->rx_buff)
	{
		d_ptr = (volatile unsigned char*)(sfi_hdr->rx_buff);
		s_ptr = (volatile unsigned char*)( sfi->sfi_base +
				(unsigned)(sfi_hdr->rx_addr));
		DEBUG_SFL("Doing RX \n");

		DEBUG_SFL("Copyng the data to rx_bf [d_ptr=0x%0x s_ptr=0x%0x] \n",
				(unsigned) d_ptr,(unsigned) s_ptr);
	}

    if(sfi_hdr->tx_buff)
	{
		s_ptr = (unsigned char*)(sfi_hdr->tx_buff);
		d_ptr = (unsigned char*)(sfi->sfi_base +
				(unsigned int)(sfi_hdr->tx_addr));
		DEBUG_SFL("Doing TX \n");
		DEBUG_SFL("Copyng the data to tx_bf [d_ptr=0x%0x s_ptr=0x%0x] \n",
				(unsigned) d_ptr,(unsigned) s_ptr);
	}

	for(i = 0; i< sfi_hdr->len; i++ )
		d_ptr[i] = s_ptr[i];

	DEBUG_SFL("\nlength in %s : %d\n", __FUNCTION__, sfi_hdr->len);
    return (int)(sfi_hdr->len);
}

/**************** Helper functions used by probe ****************/
/* function to read device id */
static void sfl_read_id(struct spi_device *spi, int index, unsigned int id_size, unsigned char* id)
{
    unsigned char old_mode = 0;
	unsigned char cmd = OPCODE_READ_DEV_ID(index);

    SFI_SET_MODE(spi, AVALANCHE_CORE_SPI_MODE, old_mode);
	spi_write_then_read(spi, &cmd, 1, id, id_size);
}

/* returns 1 on successful s25f0xxx detection */
static int s25fl0xxx_probe(struct spi_device   *spi, int index)
{
    unsigned char old_mode = 0;
    unsigned char  id[3];

	if(sfl_sfi_mode)
	    SFI_SET_MODE(spi, AVALANCHE_CORE_SPI_MODE, old_mode);

	sfl_read_id(spi, index, 3, id);

	/* check manufacturer */
	if(id[0] != S25FLxxxx_ID)
		return 0;

    if(id[2] != (sfl_data[index].id & 0xFF))
        return 0;

    return 1;
}

/* returns 1 on successful m25flxxxx detection */
static int m25flxxxx_probe(struct spi_device   *spi, int index)
{
    unsigned char old_mode = 0;
    unsigned char  id[3];

	if(sfl_sfi_mode)
	    SFI_SET_MODE(spi, AVALANCHE_CORE_SPI_MODE, old_mode);

	sfl_read_id(spi, index, 3, id);

	/* check manufacturer */
	if(id[0] != M25FLxxxx_ID)
		return 0;

    if(id[2] != (sfl_data[index].id & 0xFF))
        return 0;

    return 1;
}

/* returns 1 on successful mx25lxxxx  detection */
static int mx25lxxxx_probe(struct spi_device   *spi, int index)
{
    unsigned char old_mode = 0;
    unsigned char  id[3];

    if(sfl_sfi_mode)
	SFI_SET_MODE(spi, AVALANCHE_CORE_SPI_MODE, old_mode);

    sfl_read_id(spi, index, 3, id);

    /* check manufacturer */
    if(id[0] != MX25Lxxxx_ID)
		return 0;

    if(id[2] != (sfl_data[index].id & 0xFF))
        return 0;

    return 1;
}

/* returns 1 on successful s25fl128p detection */
static int s25fl128p_probe(struct spi_device   *spi, int index)
{
    unsigned char old_mode = 0;
    unsigned char  id[5];

	if(sfl_sfi_mode)
	    SFI_SET_MODE(spi, AVALANCHE_CORE_SPI_MODE, old_mode);

	sfl_read_id(spi, index, 5, id);

	/* check manufacturer */
	if(id[0] != S25FLxxxx_ID)
		return 0;

	/* check if it is s25fl128p */
    if(id[2] != (sfl_data[index].id & 0xFF))
        return 0;

	/* based on extended device id fill sector and n_sector info */
	switch(id[4])
    {
		case 0:
			sfl_data[index].sector_size 	= S25FL128P00_SECT_SIZE;
			sfl_data[index].n_sectors		= N_S25FL128P00_SECTORS;
			sfl_data[index].prot_ratio	= S25FL128P00_PROT_RATIO;
			break;

		case 1:
		default:
			sfl_data[index].sector_size 	= S25FL128P01_SECT_SIZE;
			sfl_data[index].n_sectors		= N_S25FL128P01_SECTORS;
			sfl_data[index].prot_ratio	= S25FL128P01_PROT_RATIO;
			break;
    }

	sfl_data[index].regions[0].offset 		= 	0;
	sfl_data[index].regions[0].erasesize	= sfl_data[index].sector_size;
	sfl_data[index].regions[0].numblocks	= sfl_data[index].n_sectors;

    return 1;
}


static int __devinit sfl_probe(struct spi_device *spi)
{
	unsigned int i, index;
	struct sfl	*flash;
	struct flash_platform_data	*data;
	int s25fl_found = 0;
	struct avalanche_sfi_dev_info_t *sfi = NULL;
	ENTER();
	data = spi->dev.platform_data;


	for(i = 0; i < ARRAY_SIZE(sfl_data); i++)
	{
		if(sfl_data[i].probe)
		{
			if(sfl_data[i].probe(spi,i))
			{
				printk("Serial Flash [Bus:%1d CS:%1d] : ",
					spi->master->bus_num, spi->chip_select);
				printk("%s %dKB, %3d sectors each %3dKB \n",
					sfl_data[i].name,
					(sfl_data[i].sector_size*sfl_data[i].n_sectors/1024),
					sfl_data[i].n_sectors, (sfl_data[i].sector_size/1024));
				s25fl_found 	= 1;
				index 			= i;
				break;
			}
		}
	}

	if(!s25fl_found){
		printk("Serial Flash [Bus:%1d CS:%1d] : ",
			spi->master->bus_num, spi->chip_select);
		printk("No device found\n");
		return -ENODEV;
	}
    flash 			= &(flash_info[spi->chip_select]);
	flash->index 	= index;
	flash->spi 		= spi;

	init_MUTEX(&flash->lock);
	dev_set_drvdata(&spi->dev, flash);
	flash->mtd.name 		= (char*)(spi->modalias);
	flash->mtd.type		 	= MTD_NORFLASH;
	flash->mtd.writesize 	= SFL_PAGESIZE(flash->index);
	flash->mtd.flags		= MTD_CAP_NORFLASH;
	flash->mtd.size 		= SFL_SIZE(flash->index);
	flash->mtd.erasesize 	= SFL_SECTOR_SIZE(flash->index);
	flash->mtd.numeraseregions = NUM_REGIONS(flash->index);
	flash->mtd.eraseregions = ERASE_REGION(flash->index);
	flash->mtd.erase 		= sfl_erase;
	flash->mtd.read 		= sfl_read;
	flash->mtd.write 		= sfl_write;
	flash->mtd.lock  		= sfl_lock;
	flash->mtd.unlock 		= sfl_unlock;
	flash->mtd.suspend 		= sfl_suspend;
	flash->mtd.resume 		= sfl_resume;


	DEBUG_SFL("mtd .name = %s, .size = 0x%.8x (%uM) "
		  ".erasesize = 0x%.8x (%uK) .numeraseregions = %d\n",
		  sfl_data[flash->index].name,	flash->mtd.size,
          flash->mtd.size / (1024*1024),flash->mtd.erasesize,
          flash->mtd.erasesize / 1024,	flash->mtd.numeraseregions);

	/*
	 * Initialise the controller data  set Read/Fast_read, write ,
	 * read mode(Noraml/Fast) for SFI transfer
	 */
	sfi	=( struct avalanche_sfi_dev_info_t *)spi->controller_data;
	if(  sfi != NULL )
	{
		sfi->initialized = 0;
		sfi->mode 		= AVALANCHE_CORE_SPI_MODE;
		sfi->write_cmd 	= OPCODE_PAGE_PROGRAM(flash->index);
		if(sfl_fast_read == SFL_NORMAL_READ){
			sfi->dual_read 			= SFL_NORMAL_READ;
			sfi->num_dummy_bytes	= NORMAL_READ_NUM_DUMMY_BYTES;
			/* Num adddrss byte count starts form zero i,e -1  */
			sfi->num_addr_bytes 	= (SFL_NUM_ADDR_BYTES - 1);
			sfi->read_cmd 			= OPCODE_READ(flash->index);
		}else{
			/* Fast Read CMD values */
			sfi->dual_read 			= SFL_NORMAL_READ;
			sfi->num_dummy_bytes 	= FAST_READ_NUM_DUMMY_BYTES;
			/* Num adddrss byte count starts form zero i, e -1 */
			sfi->num_addr_bytes 	= (SFL_NUM_ADDR_BYTES -1);
			sfi->read_cmd 			= OPCODE_FAST_READ(flash->index);
		}
		sfi->sfi_transfer = sfl_sfi_transfer;
		DEBUG_SFL("SFI = 0x%0x  Operating in SFI mode with write_cmd = 0x%x, \
			   read_cmd = 0x%x dual_read = %d num_dummy_bytes = %d \
			   num_addr_bytes %d\n",(unsigned int)sfi,
               (unsigned int)sfi->write_cmd,(unsigned int)sfi->read_cmd,
               sfi->dual_read, sfi->num_dummy_bytes,sfi->num_addr_bytes);
	}
	else
	{
		/* FIXME: is this required ??? */
		sfl_sfi_mode = AVALANCHE_CORE_SPI_MODE;
		DEBUG_SFL(" Operating in Core SPI mode\n");
	}


	/* partitions should match sector boundaries; and it may be good to
	 * use readonly partitions for writeprotected sectors (BP2..BP0).
	 */
	if( mtd_has_partitions() )
	 {
		int	 nr_parts = 0;
		struct mtd_partition	*parts = NULL;

#ifdef CONFIG_MTD_CMDLINE_PARTS
		static const char *part_probes[] = { "cmdlinepart", NULL,};
		nr_parts = parse_mtd_partitions(&flash->mtd,part_probes, &parts, 0);
#endif
		if( nr_parts <= 0 && data && data->parts )
		{
			parts = data->parts;
			nr_parts = data->nr_parts;
		}

		if (nr_parts > 0) {
			for (i = 0; i < nr_parts; i++){
				printk("partitions[%d] = ""{.name = %s, .offset = 0x%.8x,"
                      ".size = 0x%.8x (%uK) }\n",i, parts[i].name,
					  parts[i].offset,parts[i].size,
	                  parts[i].size / 1024);
			}
			flash->partitioned = 1;
			return add_mtd_partitions(&flash->mtd, parts, nr_parts);
		}
	}else if( data->nr_parts ){
		printk("ignoring %d default partitions on %s\n",data->nr_parts,
       		    data->name );
	}
	return (add_mtd_device(&flash->mtd) == 1) ? (-ENODEV) : sfl_unlockall(&flash->mtd);
}


static int __devexit sfl_remove(struct spi_device *spi)
{
	struct 	sfl *flash = dev_get_drvdata(&spi->dev);
	int		status;
	ENTER();
	/* Clean up MTD stuff. */
	if( mtd_has_partitions() && flash->partitioned ){
		status = del_mtd_partitions(&flash->mtd);
	}else{
		status = del_mtd_device(&flash->mtd);
	}

	if (status == 0) {
		kfree(flash);
	}
	return 0;
}


static struct spi_driver sfl_mtd_driver = {
    .driver = {
        .name       = "spansion",
        .bus        = &spi_bus_type,
        .owner      = THIS_MODULE,
    },
    .probe   = sfl_probe,
    .remove  = __devexit_p(sfl_remove),
};

static struct spi_driver sfl1_mtd_driver = {
    .driver = {
        .name       = "spansion1",
        .bus        = &spi_bus_type,
        .owner      = THIS_MODULE,
    },
    .probe   = sfl_probe,
    .remove  = __devexit_p(sfl_remove),
};

static int sfl_init( void )
{
	int ret0, ret1;
	ENTER();
#ifdef CONFIG_MTD_SFL_MODULE
	if( sfi_mode == AVALANCHE_CORE_SPI_MODE ||
		 sfi_mode == AVALANCHE_SFI_MODE ) {
		sfl_sfi_mode = sfi_mode;
	}else{
		DEBUG_SFL("setting sfl_sfi_mode to default  SFI MODE\n");
		sfl_sfi_mode = AVALANCHE_SFI_MODE;
	}
	DEBUG_SFL("sfl_sfi_mode = %d\n",sfl_sfi_mode);

	if( fast_read == SFL_NORMAL_READ ||
		fast_read == SFL_FAST_READ ){
		sfl_fast_read = fast_read;
	}else{
		DEBUG_SFL("setting sfl_fast_read to default Fast read Mode\n");
		sfl_fast_read = SFL_FAST_READ ;
	}

	DEBUG_SFL("sfl_fast_read = %d\n",sfl_fast_read);
#endif
	ret0 = spi_register_driver(&sfl_mtd_driver);
	ret1 = spi_register_driver(&sfl1_mtd_driver);

	return (ret0 == 0) ? ret1 : ret0;
}


static void sfl_exit( void )
{
	ENTER();
	spi_unregister_driver(&sfl_mtd_driver);
	spi_unregister_driver(&sfl1_mtd_driver);
}


module_init(sfl_init);
module_exit(sfl_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("R.Srinath/Mansoor Ahmed/Hai Shalom - Texas Instruments Inc.");
MODULE_DESCRIPTION("MTD SPI driver for Serial flashes");
module_param(sfi_mode,int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(sfi_mode,"\n\t\t Set sfi_mode = 1 for SFI mode, sfi_mode = 0 for CORE SPI mode<Range:0/1> Default is SFI mode");
module_param(fast_read,int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(fast_read,"\n\t\t Set fast_read = 0 for NORMAL READ, fast_read  = 1 for FAST READ <Range:0/1> Default is FAST READ");
