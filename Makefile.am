## Process this file with automake to produce Makefile.in

SUBDIRS = include src doc

spec = $(PACKAGE).spec

EXTRA_DIST = 

#all-local: $(spec)

rpm: $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

