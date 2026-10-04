# Makefile for TLS testsuite in uClibc
#
# Copyright (C) 2005 Steven J. Hill <sjhill@uclibc.org>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Library General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Library General Public License for more
# details.
#
# You should have received a copy of the GNU Library General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

all:

TESTS = tst-tls1 tst-tls2 tst-tls3 tst-tls4 tst-tls5		\
	tst-tls6 tst-tls7 tst-tls8 tst-tls9 tst-tls10		\
	tst-tls11 tst-tls12 tst-tls13 tst-tls14 tst-tls15
#	tst-tls1-static tst-tls2-static tst-tls9-static

include ../Test.mak

TOPDIR = ../../
PTDIR = $(TOPDIR)libpthread/nptl

TARGET_ARCH := $(strip $(subst ",, $(strip $(TARGET_ARCH))))

INCLUDES := -I. -I$(TOPDIR)include				\
	    -I$(PTDIR)						\
            -I$(PTDIR)/sysdeps/unix/sysv/linux/$(TARGET_ARCH)	\
            -I$(PTDIR)/sysdeps/$(TARGET_ARCH)			\
            -I$(PTDIR)/sysdeps/unix/sysv/linux			\
            -I$(PTDIR)/sysdeps/pthread				\
            -I$(PTDIR)/sysdeps/pthread/bits			\
            -I$(PTDIR)/sysdeps/generic				\
	    -I$(TOPDIR)ldso/include				\
	    -I$(TOPDIR)ldso/ldso/$(TARGET_ARCH)

EXTRA_CFLAGS = -D_LIBC -D_GNU_SOURCE $(INCLUDES) -std=gnu99 -Os

LDFLAGS +=

#
# Targets and defines to build DSO modules
#
# SO_CFLAGS = $(CFLAGS) -fPIC -DSHARED -shared
# SO_LDFLAGS = $(LDFLAGS) -shared -static-libgcc -L$(TOPDIR)lib

#
# Dependencies for particular tests
#
tst-tls3: tst-tlsmod1.so
LDFLAGS_tst-tls3 = tst-tlsmod1.so

tst-tls4: tst-tlsmod2.so # loaded with dlopen
LDFLAGS_tst-tls4 = -ldl

tst-tls5: tst-tlsmod2.so # loaded with dlopen
LDFLAGS_tst-tls5 = -ldl

tst-tls6: tst-tlsmod2.so # loaded with dlopen
LDFLAGS_tst-tls6 = -ldl

tst-tls7: tst-tlsmod3.so # loaded with dlopen
LDFLAGS_tst-tls7 = -ldl

tst-tls8: tst-tlsmod3.so tst-tlsmod4.so # loaded with dlopen
LDFLAGS_tst-tls8 = -ldl

tst-tls9: tst-tlsmod5.so tst-tlsmod6.so # loaded with dlopen
LDFLAGS_tst-tls9 = -ldl

tst-tls10: tst-tlsmod8.so
LDFLAGS_tst-tls10 = tst-tlsmod8.so

tst-tls11: tst-tlsmod10.so
LDFLAGS_tst-tls11 = tst-tlsmod10.so

tst-tls12: tst-tlsmod12.so
LDFLAGS_tst-tls12 = tst-tlsmod12.so

tst-tls13: tst-tlsmod13a.so # loaded with dlopen
LDFLAGS_tst-tls13 = -ldl

tst-tls14: tst-tlsmod14a.so
tst-tls14: tst-tlsmod14b.so # loaded with dlopen
LDFLAGS_tst-tls14 = tst-tlsmod14a.so -ldl

tst-tls15: tst-tlsmod15a.so tst-tlsmod15b.so # loaded with dlopen
LDFLAGS_tst-tls15 = -ldl

#
# Dependencies for particular modules
#
tst-tlsmod3.so: tst-tlsmod2.so
LDFLAGS_tst-tlsmod3.so = tst-tlsmod2.so

LDFLAGS_tst-tlsmod5.so = $(TOPDIR)lib/libc.so

LDFLAGS_tst-tlsmod6.so = $(TOPDIR)lib/libc.so

tst-tlsmod8.so: tst-tlsmod7.so
LDFLAGS_tst-tlsmod8.so = tst-tlsmod7.so

tst-tlsmod10.so: tst-tlsmod9.so
LDFLAGS_tst-tlsmod10.so = tst-tlsmod9.so

tst-tlsmod12.so: tst-tlsmod11.so
LDFLAGS_tst-tlsmod12.so = tst-tlsmod11.so

tst-tlsmod13a.so: tst-tlsmod13.so
LDFLAGS_tst-tlsmod13a.so = tst-tlsmod13.so
