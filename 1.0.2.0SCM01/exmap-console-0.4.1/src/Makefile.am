
INCLUDES=-DSBINDIR=\"$(sbindir)\"

AM_CFLAGS = $(GCC_CFLAGS) $(GLIB_CFLAGS) $(READLINE_CFLAGS)

if INTERACTIVE
interactive=interactive.c
endif

bin_PROGRAMS=exmap exmapd exmapserver

exmapserver_SOURCES = exmapserver.c
exmapserver_LDADD   = $(GLIB_LIBS)

exmap_SOURCES = $(interactive) $(server) exmap.c exelf.c
exmap_LDADD   = $(GLIB_LIBS) $(READLINE_LIBS)

exmapd_SOURCES = exmapd.c
exmapd_LDADD   = $(GLIB_LIBS)


DISTCLEANFILES = *~ Makefile.in