## Process this file with automake to produce Makefile.in

SUBDIRS = src pxview

spec = $(PACKAGE).spec

EXTRA_DIST = px_intern.h pxlib-config.1 pxlib.4 $(spec)

all-local: $(spec)

rpm: $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

