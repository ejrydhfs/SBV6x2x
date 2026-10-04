#
# Makefile for kernel SPI drivers.
#

#/* Copyright 2008, Texas Instruments Incorporated
#*
#* This program has been modified from its original operation by 
#* Texas Instruments to do the following:
#* 
#* Explanation of modification:
#*  added avalanche support
#*  
#*
#* THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
#* "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
#* OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
#* TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
#* PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
#* DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
#* COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
#* See The GNU General Public License for more details.
#*
#* These changes are covered under version 2 of the GNU General Public License,
#* dated June 1991.
#*/

ifeq ($(CONFIG_SPI_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif

# small core, mostly translating board-specific
# config declarations into driver model code
obj-$(CONFIG_SPI_MASTER)		+= spi.o

# SPI master controller drivers (bus)
obj-$(CONFIG_SPI_BITBANG)		 += spi_bitbang.o
obj-$(CONFIG_SPI_BUTTERFLY)		 += spi_butterfly.o
obj-$(CONFIG_SPI_PXA2XX)		 += pxa2xx_spi.o
obj-$(CONFIG_SPI_MPC83xx)		 += spi_mpc83xx.o
obj-$(CONFIG_SPI_S3C24XX_GPIO)	 += spi_s3c24xx_gpio.o
obj-$(CONFIG_SPI_S3C24XX)		 += spi_s3c24xx.o
obj-$(CONFIG_SPI_OMAP24XX)		 += omap2_mcspi.o
obj-$(CONFIG_SPI_OMAP_UWIRE)	 += omap_uwire.o
obj-$(CONFIG_SPI_MXC)            += mxc_spi.o
obj-$(CONFIG_ARM_AVALANCHE_SPI)	 += avalanche_spi_bitbang.o
obj-$(CONFIG_ARM_AVALANCHE_SPI)	 += avalanche_spi_master.o
obj-$(CONFIG_SPI_TI_CODEC) 		 += spi_ticodec.o

# 	... add above this line ...

# SPI protocol drivers (device/link on bus)
obj-$(CONFIG_TSC2102)			+= tsc2102.o
obj-$(CONFIG_SPI_SPIDEV)   		+= spidev.o
# 	... add above this line ...

# SPI slave controller drivers (upstream link)
# 	... add above this line ...

# SPI slave drivers (protocol for that link)
# 	... add above this line ...
