## Process this file with automake to produce Makefile.in

SUBDIRS = include src po $(DOCDIR)

spec = $(PACKAGE).spec

EXTRA_DIST = intltool-extract.in intltool-merge.in intltool-update.in $(spec) $(spec).in autogen.sh pxlib.pc.in CMakeLists.txt cmakeconfig.h.in

#all-local: $(spec)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pxlib.pc

rpm: $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

clean-am:
	rm -rf CMakeFiles cmake_install.cmake
