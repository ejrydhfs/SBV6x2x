#
# Makefile for the IEEE 802.1d ethernet bridging layer.
#
#----------------------------------------------------------------
# Copyright 2006-2007, Texas Instruments Incorporated
#
#   1) TI Unmanaged Bridge
#
# THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
# "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS 
# OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
# TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY 
# PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR 
# DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, 
# COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. 
#----------------------------------------------------------------

obj-$(CONFIG_BRIDGE) += bridge.o

bridge-y	:= br.o br_device.o br_fdb.o br_forward.o br_if.o br_input.o \
			br_ioctl.o br_notify.o br_stp.o br_stp_bpdu.o \
			br_stp_if.o br_stp_timer.o br_netlink.o ti_br_notify.o

bridge-$(CONFIG_SYSFS) += br_sysfs_if.o br_sysfs_br.o

bridge-$(CONFIG_TI_L2_SELECTIVE_FORWARDER) += ti_br_sf.o
bridge-$(CONFIG_TI_L2_SELECTIVE_PACKET_HANDLING) += ti_br_sph.o

bridge-$(CONFIG_BRIDGE_NETFILTER) += br_netfilter.o

obj-$(CONFIG_BRIDGE_NF_EBTABLES) += netfilter/
