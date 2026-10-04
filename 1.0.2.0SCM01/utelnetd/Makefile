#-------------------------------------------------------------------------------------
# Copyright 2006, Texas Instruments Incorporated
#
# This program has been modified from its original operation by Texas Instruments
# to do the following:
#
# 1. Removed BSD specific code 
# 2. Removed the stip command
#
# THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
# "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
# OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
# TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
# PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
# DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
# COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
#
# These changes are covered as per original license.
#-------------------------------------------------------------------------------------*/ 


PROGS     = utelnetd

prefix	= ${TI_FILESYSTEM}
INSTDIR   = $(prefix)/sbin/
INSTMODE  = 0755
INSTOWNER = root
INSTGROUP = root

OBJS = utelnetd.o

all: $(PROGS)

CFLAGS   += -DSHELLPATH=\"$(run_prefix)/bin/login\"

ifneq ("","$(DEBUG)")
CFLAGS   += -DDEBUG
endif

$(PROGS): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: install
install: $(PROGS)
	$(INSTALL) -d $(INSTDIR)
	$(INSTALL) -m $(INSTMODE) $(PROGS) $(INSTDIR)

.PHONY: clean
clean:
	rm -f $(PROGS) *.o core
