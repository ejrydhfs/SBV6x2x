/*
 * serial_flash.h
 * Description:
 * This file contains data structures for handling serial flash and the ID list
 * of the supported flash devices.
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
 */

#ifndef __SERIAL_FLASH_H__
#define __SERIAL_FLASH_H__

#define SR_WIP                          0x01    /* Write in progress */
#define SERIAL_FLASH_TIMEOUT            0xFFFFF
#define SF_CMD_SIZE                     4


/****************************************************************************
* NOTE:
*	Currently SPANSION, ST/NUMONYIX and MACRONIX serial flash are supported
*	both have common command Set
****************************************************************************/
/* Spansion family  Serial Flash with uniform sector size */
#define FLASH_S25FL004A				0x00F2
#define FLASH_S25FL008A				0x00F3
#define FLASH_S25FL016A				0x00F4
#define FLASH_S25FL032A				0x00F5
#define FLASH_S25FL064A				0x00F6
#define FLASH_S25FL128P				0x00F7
/* ST/Numonyix family Serial Flash with uniform sector size */
#define FLASH_M25P128				0x00F8
#define FLASH_M25P64				0x00FB
/* Macronix */
#define FLASH_MX25l6405D            0x00F9
#define FLASH_MX25l12805D           0x00FA

typedef struct {
#define WRITE_EN(x)         (x)
    unsigned char write_en;
#define WRITE_DIS(x)        (x)
    unsigned char write_dis;
#define WRITE_STS(x)        (x)
    unsigned char write_sts;
#define READ_ID(x)          (x)
    unsigned char read_id;
#define READ_STS(x)         (x)
    unsigned char read_sts;
#define READ_DATA(x)        (x)
    unsigned char read_data;
#define READ_FAST(x)        (x)
    unsigned char read_fast;
#define ERASE_SECT(x)       (x)
    unsigned char erase_sect;
#define ERASE_BLOCK(x)      (x)
    unsigned char erase_block;
#define ERASE_CHIP(x)       (x)
    unsigned char erase_chip;
#define PROG_PAGE(x)        (x)
    unsigned char prog_page;
} sf_cmd_tbl_t;

typedef struct {
    unsigned long flash_id;
#define MAN_ID(x)           x
    unsigned char manf_id;
#define MEM_TYPE(x)         x
    unsigned char mem_type;
#define MEM_DENS(x)         x
    unsigned char mem_density;
#define EXD_ID(x, y)        {x, y}
    unsigned char extended_id[2];
#define FLASH_SIZE(x)       x
    unsigned int  flash_size;
#define SECT_CNT(x)         x
    unsigned int  num_sectors;
#define SECT_SIZE(x)        x
    unsigned int  sector_size;
#define SECT_PER_BLOCK(x)   x
    unsigned int  sect_per_block;
    unsigned int  protect_mask;
    unsigned int  bytes_per_page;
    const sf_cmd_tbl_t* cmd_tbl;
    char*         flash_name;
} sf_info_t;
#endif /*  __SERIAL_FLASH_H___ */

