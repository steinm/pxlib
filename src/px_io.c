#include "config.h"
#include <stdio.h>
#include "px_intern.h"
#include "paradox-gsf.h"
#include "px_error.h"

/* regular file pointer */
int px_fread(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return(fread(buffer, len, 1, stream->s.fp));
}

int px_fseek(pxdoc_t *p, pxstream_t *stream, long offset, int whence) {
	return(fseek(stream->s.fp, offset, whence));
}

long px_ftell(pxdoc_t *p, pxstream_t *stream) {
	return(ftell(stream->s.fp));
}

long px_fwrite(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return(fwrite(buffer, len, 1, stream->s.fp));
}

/* gsf */
#if PX_HAVE_GSF
int px_gsfread(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return(gsf_input_read(stream->s.gsfin, len, buffer));
}

int px_gsfseek(pxdoc_t *p, pxstream_t *stream, long offset, int whence) {
	GSeekType gsfwhence = G_SEEK_SET;

	switch(whence) {
		case SEEK_CUR: gsfwhence = G_SEEK_CUR; break;
		case SEEK_END: gsfwhence = G_SEEK_END; break;
		case SEEK_SET: gsfwhence = G_SEEK_SET; break;
	}
	return(gsf_input_seek(stream->s.gsfin, offset, gsfwhence));
}

long px_gsftell(pxdoc_t *p, pxstream_t *stream) {
	return(gsf_input_tell(stream->s.gsfin));
}

long px_gsfwrite(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return(gsf_output_write(stream->s.gsfout, len, buffer));
}
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
