## Process this file with automake to produce Makefile.in

pxlibincdir = $(includedir)

pxlibinc_HEADERS = \
	paradox.h \
	pxversion.h

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(pxlibincdir)

EXTRA_DIST = pxversion.h.in
