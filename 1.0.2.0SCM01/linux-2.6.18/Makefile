##*******************************************************************************
#*+--------------------------------------------------------------------------+**
#*|                            ****                                          |**
#*|                            ****                                          |**
#*|                            ******o***                                    |**
#*|                      ********_///_****                                   |**
#*|                      ***** /_//_/ ****                                   |**
#*|                       ** ** (__/ ****                                    |**
#*|                           *********                                      |**
#*|                            ****                                          |**
#*|                            ***                                           |**
#*|                                                                          |**
#*|         Copyright (c) 1998-2008 Texas Instruments Incorporated           |**
#*|                        ALL RIGHTS RESERVED                               |**
#*|                                                                          |**
#*| Permission is hereby granted to licensees of Texas Instruments           |**
#*| Incorporated (TI) products to use this computer program for the sole     |**
#*| purpose of implementing a licensee product based on TI products.         |**
#*| No other rights to reproduce, use, or disseminate this computer          |**
#*| program, whether in part or in whole, are granted.                       |**
#*|                                                                          |**
#*| TI makes no representation or warranties with respect to the             |**
#*| performance of this computer program, and specifically disclaims         |**
#*| any responsibility for any damages, special or consequential,            |**
#*| connected with the use of this program.                                  |**
#*|                                                                          |**
#*+--------------------------------------------------------------------------+**
#*******************************************************************************

# ******************************************************************************
# * FILE PURPOSE:    PSP Linux package  Makefile
# ******************************************************************************
# * FILE NAME:       Makefile
# *
# * DESCRIPTION:     PSP Linux package Makefile
# *
# * (C) Copyright 2008, Texas Instruments, Inc
# *****************************************************************************/

-include $(TI_BUILD_ROOT)/TI_Build.make
-include $(TI_BUILD_ROOT)/.config

TI_IMAGES_ROOT:=$(TI_BUILD_ROOT)/build/$(TI_PRODUCT)/images

.PHONY: build config install uninstall clean distclean package

LINUXPREFIX  := $(shell pwd)
#CROSS_TOOLS := $(CROSS)
CP=/bin/cp 
RM=/bin/rm 

#export LINUXPREFIX CROSS_TOOLS DEBUG
export LINUXPREFIX DEBUG CP RM



# Endian Selection - this variable comes from kernel .config file
ifeq ($(CONFIG_CPU_BIG_ENDIAN), y)
CROSS?=arm_v6_be_uclibc-
else
CROSS?=arm_v6_le_uclibc-
endif


CROSS_FLAGS=CROSS_COMPILE=$(CROSS)

install:build
	-$(CP) -f src/arch/arm/boot/zImage $(TI_IMAGES_ROOT)/Image.bin
#	-$(CP) -f src/Image.sre $(TI_IMAGES_ROOT)/

build:config
	$(MAKE) -C src zImage modules SEVENZIP=$(TI_tools_path)/bin/7zip $(CROSS_FLAGS)
	@ln -sf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs/autoconf.h $(KERNEL_DIR)/include/linux/

config:
	-$(CP) -f $(TI_BUILD_ROOT)/.config src/
	$(MAKE) -C src oldconfig


uninstall:
	-$(RM) -f $(TI_IMAGES_ROOT)/Image.bin
#	-$(RM) -f $(TI_IMAGES_ROOT)/Image.sre


clean:uninstall
	-$(CP) -f $(TI_BUILD_ROOT)/.config src/
	$(MAKE) -C src clean


distclean:clean
	$(MAKE) -C src distclean

package:
	$(TI_BUILD_ROOT)/tools/scripts/mkpkg -s -d $(LINUXPREFIX) -o $(LINUXPREFIX)

