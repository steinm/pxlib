## Process this file with automake to produce Makefile.in

pxlibincdir = $(includedir)

pxlibinc_HEADERS = \
	paradox.h \
	paradox-gsf.h \
	paradox-mp.h \
	pxversion.h

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(pxlibincdir)

EXTRA_DIST = paradox.h.in paradox-gsf.h.in pxversion.h.in
