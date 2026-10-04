#
# linux/arch/arm/boot/compressed/Makefile
#
# create a compressed vmlinuz image from the original vmlinux
#

#/* Copyright 2008, Texas Instruments Incorporated
#*
#* This program has been modified from its original operation by 
#* Texas Instruments to do the following:
#* 
#* Explanation of modification:
#*  LZMA support
#*  
#*
#* THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
#* "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
#* OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
#* TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
#* PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
#* DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
#* COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
#* See The GNU General Public License for more details.
#*
#* These changes are covered under version 2 of the GNU General Public License,
#* dated June 1991.
#*/

HEAD	= head.o

ifeq ($(CONFIG_KERNEL_COMPRESS_7ZIP),y)
OBJS	+= Decoder.o
else
OBJS	= misc.o
endif

FONTC	= drivers/video/console/font_acorn_8x8.c

FONT = $(addprefix ../../../../drivers/video/console/, font_acorn_8x8.o)

#
# Architecture dependencies
#
ifeq ($(CONFIG_ARCH_ACORN),y)
OBJS		+= ll_char_wr.o $(FONT)
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
OBJS		+= head-shark.o ofw-shark.o
endif

ifeq ($(CONFIG_ARCH_L7200),y)
OBJS		+= head-l7200.o
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
HEAD		= head-clps7500.o
endif

ifeq ($(CONFIG_ARCH_P720T),y)
# Borrow this code from SA1100
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_CPU_XSCALE),y)
OBJS		+= head-xscale.o
endif

ifeq ($(CONFIG_PXA_SHARPSL),y)
OBJS		+= head-sharpsl.o
endif

ifeq ($(CONFIG_ARCH_AT91RM9200),y)
OBJS		+= head-at91rm9200.o
endif

ifeq ($(CONFIG_MACH_OMAP_PERSEUS2),y)
OBJS		+= head-omap.o
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
ifeq ($(CONFIG_CPU_CP15),y)
OBJS		+= big-endian.o
else
# The endian should be set by h/w design.
endif
endif

#
# We now have a PIC decompressor implementation.  Decompressors running
# from RAM should not define ZTEXTADDR.  Decompressors running directly
# from ROM or Flash must define ZTEXTADDR (preferably via the config)
# FIXME: Previous assignment to ztextaddr-y is lost here. See SHARK
ifeq ($(CONFIG_ZBOOT_ROM),y)
ZTEXTADDR	:= $(CONFIG_ZBOOT_ROM_TEXT)
ZBSSADDR	:= $(CONFIG_ZBOOT_ROM_BSS)
else
ZTEXTADDR	:= 0
ZBSSADDR	:= ALIGN(4)
endif

SEDFLAGS	= s/TEXT_START/$(ZTEXTADDR)/;s/BSS_START/$(ZBSSADDR)/

ifdef ($(CONFIG_KERNEL_COMPRESS_7ZIP),y)
targets       := vmlinux vmlinux.lds piggy.gz piggy.o $(FONT) \
		 head.o Decoder.o $(OBJS)
else
targets       := vmlinux vmlinux.lds piggy.gz piggy.o $(FONT) \
		 head.o misc.o $(OBJS)
endif
EXTRA_CFLAGS  := -fpic
EXTRA_AFLAGS  :=

# Supply ZRELADDR and PARAMS_PHYS to the decompressor via linker
# symbols.  We only define params_phys if the machine class defined
# the corresponding makefile variable.
LDFLAGS_vmlinux := --defsym zreladdr=$(ZRELADDR)
ifneq ($(PARAMS_PHYS),)
LDFLAGS_vmlinux += --defsym params_phys=$(PARAMS_PHYS)
endif
LDFLAGS_vmlinux += -p --no-undefined -X \
	$(shell $(CC) $(CFLAGS) --print-libgcc-file-name) -T

# Don't allow any static data in misc.o, which
# would otherwise mess up our GOT table
CFLAGS_misc.o := -Dstatic=

# Don't allow any static data in misc.o, which
# would otherwise mess up our GOT table
CFLAGS_Decoder.o := -Dstatic=


$(obj)/vmlinux: $(obj)/vmlinux.lds $(obj)/$(HEAD) $(obj)/piggy.o \
	 	$(addprefix $(obj)/, $(OBJS)) FORCE
	$(call if_changed,ld)
	@:

ifneq ($(CONFIG_KERNEL_COMPRESS_7ZIP),y)
$(obj)/piggy.gz: $(obj)/../Image FORCE
	$(call if_changed,gzip)
else
$(obj)/piggy.gz: FORCE
ifdef SEVENZIP
	rm -f $(obj)/7zip
	cp -f $(SEVENZIP) $(obj)/7zip
else
	rm -f $(obj)/7zip
	make -C $(TOPDIR)/../../tools/src/7zip/ clean
	make -C $(TOPDIR)/../../tools/src/7zip/
	cp $(TOPDIR)/../../tools/src/7zip/7zip $(obj)/7zip
endif
#	$(call if_changed, ../../../../../../tools/bin/7zip)
	$(obj)/7zip $(obj)/../Image $(obj)/piggy.gz
	rm -f $(obj)/7zip
endif


$(obj)/piggy.o:  $(obj)/piggy.gz FORCE

CFLAGS_font_acorn_8x8.o := -Dstatic=

$(obj)/vmlinux.lds: $(obj)/vmlinux.lds.in arch/arm/boot/Makefile .config
	@sed "$(SEDFLAGS)" < $< > $@

$(obj)/misc.o: $(obj)/misc.c include/asm/arch/uncompress.h lib/inflate.c

$(obj)/Decoder.o: $(obj)/Decoder.c $(obj)/Decoder.h $(obj)/lzma_decoder.h include/asm/arch/uncompress.h


