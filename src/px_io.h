#ifndef __PX_IO_H__
#define __PX_IO_H__
int px_fread(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer);
int px_fseek(pxdoc_t *p, pxstream_t *stream, long offset, int whence);
long px_ftell(pxdoc_t *p, pxstream_t *stream);
long px_fwrite(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer);

#ifdef HAVE_GSF
int px_gsfread(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer);
int px_gsfseek(pxdoc_t *p, pxstream_t *stream, long offset, int whence);
long px_gsftell(pxdoc_t *p, pxstream_t *stream);
long px_gsfwrite(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer);
#endif

#endif
