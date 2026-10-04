# *************************************************************************
# * Makefile for building the IPSec Library
# *************************************************************************
# *	(C) Copyright 2006, Texas Instruments, Inc.
# *************************************************************************

# List of all the commands needed for building the Library
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
RANLIB = $(CROSS_COMPILE)ranlib
AR = $(CROSS_COMPILE)ar

# Directory Definitions
BASE_DIR	= ../..
SRC_DIR 	= ..

# Include Paths
DEFAULT_INCLUDES = -I$(BASE_DIR) -I$(SRC_DIR)/include -I$(SRC_DIR)/include-glibc

# Compile Time Flags
CFLAGS += -DHAVE_PFKEY_POLICY_PRIORITY -D_GNU_SOURCE -DHAVE_CONFIG_H -Os -Wall $(DEFAULT_INCLUDES) -fPIC

# Linker Flags
LDFLAGS = -shared -L$(LIBC_DIR)
LIBS    = -lcrypt -lresolv

# List of all objects that constitute the IPSec Library
OBJECTS = 	ipsec_dump_policy.o ipsec_get_policylen.o \
			ipsec_strerror.o key_debug.o pfkey.o pfkey_dump.o \
			policy_parse.o policy_token.o

all: $(OBJECTS)
	 $(LD) $(LDFLAGS) $(OBJECTS) $(LIBS) -o libipsec.so

clean: 
	rm -f libipsec.so
	rm -f $(OBJECTS)

#libipsec.la: $(libipsec_la_OBJECTS) $(libipsec_la_DEPENDENCIES) 
#	$(LINK) -rpath $(libdir) $(libipsec_la_LDFLAGS) $(libipsec_la_OBJECTS) $(libipsec_la_LIBADD) $(LIBS)

# Rules for building objects.
.c.o: $(CC) $(CFLAGS) -c $<


