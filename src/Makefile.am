## Process this file with automake to produce Makefile.in

INCLUDES = -I@srcdir@/../include -I./include @RECODE_INCLUDEDIR@ @GSF_INCLUDEDIR@ -DLOCALEDIR=\""$(datadir)/locale"\"

noinst_HEADERS = fileformat.h \
                 px_error.h \
                 px_head.h \
								 px_io.h \
                 px_intern.h \
                 px_memory.h \
                 px_misc.h \
                 sdncal.h

## bin_PROGRAMS =

## bin_SCRIPTS=

lib_LTLIBRARIES = libpx.la

libpx_la_LDFLAGS = -version-info @PXLIB_VERSION_INFO@

libpx_la_SOURCES = \
		paradox.c \
		px_head.c \
		px_misc.c \
		px_io.c \
		px_memory.c \
		px_error.c \
		gregor.c

libpx_la_LIBADD = @RECODE_LIBS@ @GSF_LIBS@

BUILD_LIBS = -lm

