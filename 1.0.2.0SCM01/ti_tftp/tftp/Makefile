
all: tftp

include ../MCONFIG
include ../MRULES

TI_TFTP_PLUGIN=1

ifdef TI_TFTP_PLUGIN
CFLAGS += -rdynamic -DTI_TFTP_PLUGIN -I.
LDFLAGS += -L. -rdynamic -ldl 
OBJS = tftp.o main.o tftpsubs.o TI_plugin.o
SHARED = tftp_sample_plugin.so

tftp_sample_plugin.so: plugins/tftp_sample.o
	$(CC) -shared tftp_sample.o -o $@

else
CFLAGS += -I.
OBJS = tftp.o main.o tftpsubs.o TI_plugin.o tftp_sample.o
tftp_sample.o: plugins/tftp_sample.o
endif

tftp: $(OBJS) $(SHARED)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(OBJS): tftpsubs.h
tftp.o: ../version.h

install: tftp
	install -d $(INSTALLROOT)$(BINDIR)
	install -s -m$(BINMODE) tftp $(INSTALLROOT)$(BINDIR)
	install -d $(INSTALLROOT)$(MANDIR)/man1
	install -m$(MANMODE) tftp.1 $(INSTALLROOT)$(MANDIR)/man1

clean:
	rm -f *.o tftp *.so *~

