## Process this file with automake to produce Makefile.in

SUBDIRS = include src $(DOCDIR)

spec = $(PACKAGE).spec

EXTRA_DIST = $(spec) $(spec).in

#all-local: $(spec)

rpm: $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

