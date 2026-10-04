#----------------------------------------------------------------
# Copyright 2007, Texas Instruments Incorporated
#
# This program has been modified from its original operation by 
# Texas Instruments to do the following:
# 
# 1. Removed compilation support for libabz.
#
# THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
# "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS 
# OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
# TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY 
# PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR 
# DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, 
# COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. 
#---------------------------------------------------------------- 
# -*- sh -*-

#  Copyright (c) Abraham vd Merwe <abz@blio.com>
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the author nor the names of other contributors
#     may be used to endorse or promote products derived from this software
#     without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

CPPFLAGS = -I../include
LDFLAGS = -L../src -static
LDLIBS = -lber 

# path to toplevel directory from here
TOPDIR = ..

# subdirectories (leave as is if there is no subdirectories)
DIR =

# names of object files
OBJ =

# program name (leave as is if there is no program)
PRG =

# library name (leave as is if there is no library)
LIB =

include $(TOPDIR)/paths.mk
include $(TOPDIR)/defs.mk
include $(TOPDIR)/vars.mk
include $(TOPDIR)/rules.mk

TEST = test_encode test_decode test_utils

all:: $(TEST)

test_encode: test_encode.o
test_decode: test_decode.o
test_utils: test_utils.o

clean::
	rm -f $(TEST) $(addsuffix .o,$(TEST))


