#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "px_intern.h"
#include "paradox.h"

void px_errorhandler(pxdoc_t *p, int error, const char *str) {
	printf("PXLib: %s\n", str);
}

void px_error(pxdoc_t *p, int type, const char *fmt, ...) {
	char msg[256];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(msg, fmt, ap);

	if (!p->in_error) {
		p->in_error = 1; /* avoid recursive errors */
		if(p->errorhandler)
			(p->errorhandler)(p, type, msg);
	}

	/* If the error handler returns the error was non-fatal */
	p->in_error = 0;

	va_end(ap);
}
