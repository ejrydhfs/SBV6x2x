
/*******************************************************************************
**+--------------------------------------------------------------------------+**
**|                            ****                                          |**
**|                            ****                                          |**
**|                            ******o***                                    |**
**|                      ********_///_****                                   |**
**|                      ***** /_//_/ ****                                   |**
**|                       ** ** (__/ ****                                    |**
**|                           *********                                      |**
**|                            ****                                          |**
**|                            ***                                           |**
**|                                                                          |**
**|         Copyright (c) 1998-2005 Texas Instruments Incorporated           |**
**|                        ALL RIGHTS RESERVED                               |**
**|                                                                          |**
**|                                                                          |**
**+--------------------------------------------------------------------------+**
*******************************************************************************/

/*******************************************************************************
 * FILE PURPOSE:    SSP Module Device Driver Header.
 *******************************************************************************
 * FILE NAME:       ssp_drv.h
 *
 * DESCRIPTION:     OS abstracted and platform independent SSP module Header
 *
 * REVISION HISTORY:
 * 19 Mar 03 - Creation - PSP TII  
 *
 * (C) Copyright 2003, Texas Instruments, Inc
 ******************************************************************************/

#ifndef __SSP_DRV_H__
#define __SSP_DRV_H__ 

/*The following definitions needed for including ssp_hal.h, shared between vxworks and linux*/
#ifndef INT32
#define INT32 long
#endif

#ifndef UINT8
#define UINT8 unsigned char
#endif

#ifndef UINT16
#define UINT16 unsigned short
#endif

#ifndef UINT32
#define UINT32 unsigned long
#endif

#ifndef BOOL
#define BOOL UINT8
#endif


#include "ssp_hal.h"

/**
 * \defgroup SspDrvReturnCodes SSP Driver Return Codes
 * 
 * SSP Driver Return Codes
 */
/*@{*/

/** \def SSP_DRV_OK SSP Driver API Success */
#define SSP_DRV_OK          SSP_HAL_OK

/** \def SSP_DRV_ERROR SSP Driver API Failure */
#define SSP_DRV_ERROR       SSP_HAL_ERROR

/*@}*/

/**
 *  \brief Device Descriptor returned by the driver
 *
 *  The device descriptor is returned by the driver on completion of
 *  a successful open call. The application should preserve this
 *  descriptor and use it for any further operations on the device.
 */
typedef void* SSP_DRV_DESC_T;

#define SSP_DRIVER_MAGIC	   0xD1

/* I2C */
#define I2C_MAX_BUF_SIZE       100

/* IOCTLs*/
#define I2C_SET_ADDR 	        _IOW( SSP_DRIVER_MAGIC, 1, UINT8 ) 

/* LCD Control IOCTLs */
#define LCD_CTRL_CONTRAST_UP    _IO( SSP_DRIVER_MAGIC, 2 ) 
#define LCD_CTRL_CONTRAST_DOWN  _IO( SSP_DRIVER_MAGIC, 3 ) 


int platform_glcd_get_contrast(void);
int platform_glcd_set_contrast(int val);

SSP_HAL_I2C_INFO_T* ssp_i2c_open( void );
int ssp_i2c_close(SSP_HAL_I2C_INFO_T *     id);
int ssp_i2c_read( SSP_HAL_I2C_INFO_T  *info, UINT8    *buffer, UINT32   len);
int ssp_i2c_write( SSP_HAL_I2C_INFO_T  *info, UINT8    *buffer, UINT32   len);
SSP_HAL_LCD_CTRL_INFO_T* ssp_lcd_ctrl_open( void );
int ssp_lcd_ctrl_close(SSP_HAL_LCD_CTRL_INFO_T * info);


#endif /* __SSP_DRV_H__ */

