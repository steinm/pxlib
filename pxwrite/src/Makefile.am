## Process this file with automake to produce Makefile.in

INCLUDES = $(PX_INCLUDEDIR) -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"

bin_PROGRAMS = pxwrite

pxwrite_SOURCES = main.c csv.c

noinst_HEADERS = msgno.h

pxwrite_LDADD = $(PX_LIBDIR) $(PX_LIBS)
