## Process this file with automake to produce Makefile.in

SUBDIRS = include src po $(DOCDIR)

spec = $(PACKAGE).spec

EXTRA_DIST = $(spec) $(spec).in autogen.sh pxlib.pc.in

#all-local: $(spec)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pxlib.pc

rpm: $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

