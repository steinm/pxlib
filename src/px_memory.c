#include "config.h"
#include <stdio.h>
#include "px_intern.h"
#include "paradox.h"
#include "px_error.h"

void *px_malloc(pxdoc_t *p, size_t len, const char *caller) {
	return((void *) malloc(len));
}

void *px_realloc(pxdoc_t *p, void *mem, size_t len, const char *caller) {
	return((void *) realloc(mem, len));
}

void px_free(pxdoc_t *p, void *ptr) {
	free(ptr);
	ptr = NULL;
}

size_t px_strlen(const char *str) {
	return(strlen(str));
}

char *px_strdup(pxdoc_t *p, const char *str) {
	size_t len;
	char *buf;

	if (str == NULL) {
		p->errorhandler(p, PX_Warning, "NULL string in px_strdup");
		return(NULL);
	}
	len = px_strlen(str);
	buf = (char *) p->malloc(p, len+1, "ps_strdup");
	memcpy(buf, str, len+1);
	return(buf);
}
