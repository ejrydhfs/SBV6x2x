# *************************************************************************
# * Makefile for building the SetKey application
# *************************************************************************
# *	(C) Copyright 2006, Texas Instruments, Inc.
# *************************************************************************

# List of all the commands needed for building the application
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
RANLIB = $(CROSS_COMPILE)ranlib
AR = $(CROSS_COMPILE)ar

# Directory Definitions
BASE_DIR	= ../..
SRC_DIR 	= ..

# Include Paths
DEFAULT_INCLUDES = -I$(BASE_DIR) -I$(SRC_DIR)/include -I$(SRC_DIR)/include-glibc -I$(SRC_DIR)/libipsec 

# Compile Time Flags
CFLAGS += -D_GNU_SOURCE -DHAVE_CONFIG_H -Os -Wall $(DEFAULT_INCLUDES)

# Linker Flags
LDFLAGS = -L$(LIBC_DIR) -L../libipsec
LIBS    = -lcrypt -lresolv -lipsec -lfl

# List of all objects that constitute the IPSec Library
OBJECTS = setkey.o parse.o token.o
#OBJECTS = test_setkey.o

# 
#token.o: token.c
#	$(CC) $(CFLAGS) -MT $@ -MD -MP -MF "pankajrule" -c $< -o $@
#setkey.o: setkey.c
#	$(CC) $(CFLAGS) -c setkey.c -o setkey.o
#parse.o: parse.c
#	$(CC) $(CFLAGS) -c parse.c -o parse.o

all: $(OBJECTS)
	 $(LD) $(LDFLAGS) $(OBJECTS) $(LIBS) -o setkey

clean:
	rm -f setkey
	rm -f $(OBJECTS)

# Rules for building the objects
.c.o: $(CC) $(CFLAGS) -c $< -o $@


#if gcc -DHAVE_CONFIG_H -I. -I. -I../..   -D_GNU_SOURCE -include ./src/include-glibc/glibc-bugs.h -I./src/include-glibc -I./src/include-glibc  -I./../../src/racoon/missing -I../../src/libipsec -include ../../src/include-glibc/glibc-bugs.h -I../../src/include-glibc -I../../src/include-glibc -g -O2  -Wall-Werror -Wno-unused -MT token.o -MD -MP -MF ".deps/token.Tpo" -c -o token.o token.c; \

