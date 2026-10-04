#
# Makefile for the Linux networking core.
#
#----------------------------------------------------------------
# Copyright 2006-2007, Texas Instruments Incorporated
#
#   1) TI Device Specific Protocol Handling for the 
#      Unmanaged Bridge feature.
#
# THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
# "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS 
# OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
# TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY 
# PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR 
# DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, 
# COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. 
#----------------------------------------------------------------
obj-y := sock.o request_sock.o skbuff.o iovec.o datagram.o stream.o scm.o \
	 gen_stats.o gen_estimator.o

obj-$(CONFIG_SYSCTL) += sysctl_net_core.o

obj-y		     += dev.o ethtool.o dev_mcast.o dst.o netevent.o \
			neighbour.o rtnetlink.o utils.o link_watch.o filter.o ti_dev.o

obj-$(CONFIG_XFRM) += flow.o
obj-$(CONFIG_SYSFS) += net-sysfs.o
obj-$(CONFIG_NET_DIVERT) += dv.o
obj-$(CONFIG_NET_PKTGEN) += pktgen.o
obj-$(CONFIG_WIRELESS_EXT) += wireless.o
obj-$(CONFIG_NETPOLL) += netpoll.o
obj-$(CONFIG_NET_DMA) += user_dma.o
obj-$(CONFIG_FIB_RULES) += fib_rules.o

