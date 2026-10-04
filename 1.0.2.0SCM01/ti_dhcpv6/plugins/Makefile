#-----------------------------------------------------------------
# Copyright 2007, Texas Instruments Incorporated
#
# Plugin Makefile that is invoked for all TI DHCPv6C Plugins 
#-----------------------------------------------------------------

# Use the configuration file to determine the PLUGINs that need to be built.
#-include ../../.config

# Use this MACRO to pass any common C Flags that are shared by all PLUGINS
PLUGIN_CFLAGS = "-I../.. -Os"

# Use this MACRO to pass any common LD flags. All plugins are shared libraries.
LDFLAGS=-shared

# The TI_DHCPv6C Plugin is always built.
PLUGINS-y += sample1/dhcp6c_sample_na_plugin.so
PLUGINS-y += sample2/dhcp6c_sample_na_rc_plugin.so
PLUGINS-y += sample3/dhcp6c_sample_multiple_na_plugin.so
PLUGINS-y += sample4/dhcp6c_sample_pd_plugin.so
PLUGINS-y += sample5/dhcp6c_sample_na_ra_plugin.so
PLUGINS-y += sample6/dhcp6c_sample_na_ra_rc_plugin.so

# Build all the Configured Plugins.
all: $(PLUGINS-y)

#-----------------------------------------------------------------
# Individual Rule for building each Plugin.
#-----------------------------------------------------------------
sample1/dhcp6c_sample_na_plugin.so:
	$(MAKE) -C sample1 CC=$(CC) PLUGIN_CFLAGS=$(PLUGIN_CFLAGS) LDFLAGS=$(LDFLAGS) all
sample2/dhcp6c_sample_na_rc_plugin.so:
	$(MAKE) -C sample2 CC=$(CC) PLUGIN_CFLAGS=$(PLUGIN_CFLAGS) LDFLAGS=$(LDFLAGS) all
sample3/dhcp6c_sample_multiple_na_plugin.so:
	$(MAKE) -C sample3 CC=$(CC) PLUGIN_CFLAGS=$(PLUGIN_CFLAGS) LDFLAGS=$(LDFLAGS) all
sample4/dhcp6c_sample_pd_plugin.so:
	$(MAKE) -C sample4 CC=$(CC) PLUGIN_CFLAGS=$(PLUGIN_CFLAGS) LDFLAGS=$(LDFLAGS) all
sample5/dhcp6c_sample_na_ra_plugin.so:
	$(MAKE) -C sample5 CC=$(CC) PLUGIN_CFLAGS=$(PLUGIN_CFLAGS) LDFLAGS=$(LDFLAGS) all
sample6/dhcp6c_sample_na_ra_rc_plugin.so:
	$(MAKE) -C sample6 CC=$(CC) PLUGIN_CFLAGS=$(PLUGIN_CFLAGS) LDFLAGS=$(LDFLAGS) all

#-----------------------------------------------------------------
# Clean all the plugins
#-----------------------------------------------------------------
clean:
	$(MAKE) -C sample1 clean
	$(MAKE) -C sample2 clean
	$(MAKE) -C sample3 clean
	$(MAKE) -C sample4 clean
	$(MAKE) -C sample5 clean
	$(MAKE) -C sample6 clean

