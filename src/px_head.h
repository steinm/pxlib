#ifndef __PX_HEAD_H__
#define __PX_HEAD_H__
pxhead_t *get_px_head(pxdoc_t *pxdoc, FILE *fp);
int put_px_head(pxdoc_t *pxdoc, pxhead_t *pxh, FILE *fp);
int put_px_datablock(pxdoc_t *pxdoc, pxhead_t *pxh, int after, FILE *fp);
int px_add_data_to_block(pxdoc_t *pxdoc, pxhead_t *pxh, int datablocknr, char *data, FILE *fp);
#endif
