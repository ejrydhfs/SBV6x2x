
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
 * FILE PURPOSE:    SSP Module Device Driver Configuration File.
 *******************************************************************************
 * FILE NAME:       ssp_config.h
 *
 * DESCRIPTION:     Contains the configuration information for the devices 
 *                  connected to the SSP module. 
 *
 * REVISION HISTORY:
 * 19 Mar 03 - Creation - PSP TII  
 *
 * (C) Copyright 2003, Texas Instruments, Inc
 ******************************************************************************/

#ifndef __SSP_CONFIG_H__
#define __SSP_CONFIG_H__

#define SSP_I2C_OUTPUT_CLK_FREQ                 400000    /* 400 KHz */ 
#define SSP_SPI_OUTPUT_CLK_FREQ                 2000000   /* 2 MHz */
#define SSP_UW_OUTPUT_CLK_FREQ                  1000000   /* 1 MHz */
#define SSP_LCDCTRL_OUTPUT_CLK_FREQ             250000    /* 250 KHz */ 


/*
 * The SSP driver draws the configuration it needs to pass 
 * to the HAL from these structures. For I2C devices, The 
 * address parameter defined in this structure is ignored
 * as the information on address passed by the application.
 * Similarly the chip select number is ignored for SPI devices.
 */

#if defined( TNETV1050SDB ) || defined( CONFIG_MIPS_TITAN )
SSP_HAL_I2C_INFO_T ssp_i2c_static_config = {
    1, 0, 0x00, SSP_I2C_OUTPUT_CLK_FREQ, SSP_HAL_MODE_INTR        
};

SSP_HAL_LCD_CTRL_INFO_T ssp_lcd_ctrl_static_config = { 
    0, 0, 0, 4, SSP_LCDCTRL_OUTPUT_CLK_FREQ, SSP_HAL_MODE_INTR 
};

/* TNETV1050SDB does NOT contain SPI device. This is added here for the sake 
 * of avoid  the cluttering of driver code with ifdef statements 
 */
SSP_HAL_SPI_INFO_T ssp_spi_static_config = { 
    0, 0, 1, 2, SSP_SPI_OUTPUT_CLK_FREQ, SSP_HAL_MODE_INTR 
};
#endif

#ifdef TNETV1050VDB
SSP_HAL_I2C_INFO_T ssp_i2c_static_config = {
    0, 1, 0x00, SSP_I2C_OUTPUT_CLK_FREQ, SSP_HAL_MODE_INTR        
};

SSP_HAL_LCD_CTRL_INFO_T ssp_lcd_ctrl_static_config = { 
    0, 0, 0, 4, SSP_LCDCTRL_OUTPUT_CLK_FREQ, SSP_HAL_MODE_INTR 
};

SSP_HAL_SPI_INFO_T ssp_spi_static_config = {
    0, 0, 1, 2, SSP_SPI_OUTPUT_CLK_FREQ, SSP_HAL_MODE_INTR 
};
#endif

#endif /* __SSP_CONFIG_H__ */


