= Open Source Software OSS components of SBV6X2X Voice Cable Modem =

== CONTENTS==
   
     Overview
     Package Info
     Installation
     Tools Required for Build
     Building Instructions
     End

== OVERVIEW ==

The SBV6X2X is a Docsis 3.0 compliant voice enabled cable modem. This package
contains all the files used in SBV6X2X that are open source and meant for
re-distribution to the community.

== PACKAGE INFO ==

A brief description of the files included in this package ...

README.txt - This file. It explains the OSS package contents and build
             procedure.

OSS Package Directories
-----------------------

Here is a one liner on the OSS packages that SB6120 uses. For details of each
package, refer to the README in respective directories.

Linux-2.6.18 : This is the linux kernel used in our product.

u-boot-1.2.0 : This is the bootloader that is used in our ARM based SB6120 board
           
uClibc       : This contains the C library functions for embedded Linux systems.               Version 0.2.29 is used.

busybox      : Contains free unix utilities. Version 1.4.2 is used.

dmalloc-5.5.2: Contains memory allocation and management routines.

dropbear-0.50: It is a ssh2 sever and client.

iostat-2.2   : This contains Linux I/O performance monitoring utility.

squashfs     : This is a read only file system for Linux. Version used is 4.57.0

utelnetd     : A small telnet deamon for embedded systems. Version is 0.1.2

udhcp        : DHCP client and server for embedded systems. Version is 0.9.7

ipsec_tools  : This package provides a way to use native IPsec functionality
               in the Linux 2.6 + kernel. Version used is 0.6.6

udev         : A user space implementation of dev file system. Version 055.

bridge_utils : This package is for Linux ethernet bridge code. Version is 1.0.4

ti_tftp      : Contains the tftp and tftpd package. Only tftp is used. Version
               is 0.16

ti_dhcpv6    : Modified open source DHCPv6 implementation. Version is 1.2

ti_udhcp     : Implements the DHCPv4 RFC. Version is 0.9.9

libber       : Contains the encoder/decoder functionality. Version is 0.4.2

liboop       : Version 1.0

flex         : Contains parsers/lex analysers. Version is 2.5.4

kconfig      : Version is 1.4

ruli         : Version 1.19

7zip         : ---- LGPLv2

Exmap        : Version 0.9


Proprietry Directories
----------------------
hal_* , bbu_driver, cable_ni, Engine_Pro_C_Src, core, fw_env and dbridge_dlm
contains proprietry source code which is GPLv2 dependent.


== INSTALLATION ==

Untar the contents of each package using tar -xjf <packagename>.tar.bz2.
This will create a directory with same name as <packagename>/
and inflate the files in directories inside.

== TOOLS REQUIRED FOR BUILD ==

cat /etc/redhat-release
Linux Version :  RedHat Enterprise Linux WS release 4 (Nahant Update 5)

gcc -v
GCC Compiler  :  Version 3.4.6 20060404 ( Red Hat 3.4.6-8 )

arm_v6_be_uclibc-gcc -v
CROSS Compiler:  Montavista ToolChain 4.2.0-2.0.0 custom 2007-02-12


== BUILD INSTRUCTIONS ==

Refer to individual OSS package README or INSTALL for building binaries.

== END ==

**********************************************************************
Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com
**********************************************************************
Portions Copyright 2009 Motorola - http://www.motorola.com
**********************************************************************