#
# Makefile for the i2c algorithms
#
#/* Copyright 2008, Texas Instruments Incorporated
#*
#* This program has been modified from its original operation by 
#* Texas Instruments to do the following:
#* 
#* Explanation of modification:
#*  added avalanche i2c support
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

obj-$(CONFIG_I2C_ALGOAVALANCHE) += i2c-algo-avalanche.o 
obj-$(CONFIG_I2C_ALGOBIT)	+= i2c-algo-bit.o
obj-$(CONFIG_I2C_ALGOPCF)	+= i2c-algo-pcf.o
obj-$(CONFIG_I2C_ALGOPCA)	+= i2c-algo-pca.o
obj-$(CONFIG_I2C_ALGOITE)	+= i2c-algo-ite.o
obj-$(CONFIG_I2C_ALGO_SIBYTE)	+= i2c-algo-sibyte.o
obj-$(CONFIG_I2C_ALGO_SGI)	+= i2c-algo-sgi.o

ifeq ($(CONFIG_I2C_DEBUG_ALGO),y)
EXTRA_CFLAGS += -DDEBUG
endif
