/*
 *    (c) Copyright 2003  Uwe Steinmann.
 *    All rights reserved.
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the
 *    Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *    Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "pxversion.h"
#include "px_intern.h"
#include "paradox.h"
#include "px_memory.h"
#include "px_head.h"
#include "px_error.h"
#include "px_misc.h"

PXLIB_API int PXLIB_CALL
PX_get_majorversion(void) {
	return(PXLIB_MAJOR_VERSION);
}

PXLIB_API int PXLIB_CALL
PX_get_minorversion(void) {
	return(PXLIB_MINOR_VERSION);
}

PXLIB_API int PXLIB_CALL
PX_get_subminorversion(void) {
	return(PXLIB_MICRO_VERSION);
}

PXLIB_API int PXLIB_CALL
PX_has_recode_support(void) {
#if PX_USE_RECODE
	return(1);
#else
#if PX_USE_ICONV
	return(2);
#endif
#endif
	return(0);
}

PXLIB_API int PXLIB_CALL
PX_is_bigendian(void) {
#if WORDS_BIGENDIAN
	return(1);
#else
	return(0);
#endif
}

PXLIB_API pxdoc_t* PXLIB_CALL
PX_new2(void  (*errorhandler)(pxdoc_t *p, int type, const char *msg),
        void* (*allocproc)(pxdoc_t *p, size_t size, const char *caller),
        void* (*reallocproc)(pxdoc_t *p, void *mem, size_t size, const char *caller),
        void  (*freeproc)(pxdoc_t *p, void *mem)) {
	pxdoc_t *pxdoc;

	if(allocproc == NULL) {
		allocproc = px_malloc;
		reallocproc = px_realloc;
		freeproc  = px_free;
	}
	if (errorhandler == NULL)
		errorhandler = px_errorhandler; 
	if(NULL == (pxdoc = (pxdoc_t *) (* allocproc) (NULL, sizeof(pxdoc_t), "PS new"))) {
		(*errorhandler)(NULL, PX_MemoryError, _("Couldn't allocate PS object"));
		return(NULL);
	}
	memset((void *)pxdoc, 0, (size_t) sizeof(pxdoc_t));
	pxdoc->errorhandler = errorhandler;
	pxdoc->malloc = allocproc;
	pxdoc->realloc = reallocproc;
	pxdoc->free = freeproc;
	pxdoc->px_fp = NULL;

#if PX_USE_RECODE
	pxdoc->recode_outer = recode_new_outer(false);
	pxdoc->recode_request = recode_new_request(pxdoc->recode_outer);
#else
#if PX_USE_ICONV
	pxdoc->iconvcd = (iconv_t) -1;
#endif
#endif
	pxdoc->targetencoding = NULL;

	return pxdoc;
}

PXLIB_API pxdoc_t* PXLIB_CALL
PX_new(void) {
	return(PX_new2(NULL, NULL, NULL, NULL));
}

PXLIB_API int PXLIB_CALL
PX_open_fp(pxdoc_t *pxdoc, FILE *fp) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if((pxdoc->px_head = get_px_head(pxdoc, fp)) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Unable to get header."));
		return -1;
	}

	pxdoc->px_fp = fp;

	return 0;
}

PXLIB_API int PXLIB_CALL
PX_open_file(pxdoc_t *pxdoc, char *filename) {
	FILE *fp;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if((fp = fopen(filename, "r")) == NULL) {
		return -1;
	}

	if(0 > PX_open_fp(pxdoc, fp)) {
		fclose(fp);
		return -1;
	}

	pxdoc->px_name = px_strdup(pxdoc, filename);
	pxdoc->closefp = px_true;
	return 0;
}

PXLIB_API char* PXLIB_CALL
PX_get_record(pxdoc_t *pxdoc, int recno, char *data) {
	int ret, found, blockcount;
	TDataBlock datablock;
	pxhead_t *pxh;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return NULL;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("File has no header"));
		return NULL;
	}
	pxh = pxdoc->px_head;

	if((recno < 0) || (recno >= pxh->px_numrecords)) {
		px_error(pxdoc, PX_RuntimeError, _("Record number out of range"));
		return NULL;
	}

	/* Go to the start of the data block (skip the header) */
	if((ret = fseek(pxdoc->px_fp, pxh->px_headersize, SEEK_SET)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of data block"));
		return NULL;
	}

	found = 0;
	blockcount = 0;
	while(!found && (blockcount < pxh->px_fileblocks)) {
		int datasize;
		/* Get the info about this data block */
		if((ret = fread(&datablock, sizeof(TDataBlock), 1, pxdoc->px_fp)) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not read"));
			return NULL;
		}
		datasize = get_short_le(&datablock.addDataSize);
//		printf("datasize = %d, recno = %d, platz verbraucht = %d\n", datasize, recno, (recno+1)*pxh->px_recordsize);
		if(recno*pxh->px_recordsize <= datasize) {
			found = 1;
			if((ret = fseek(pxdoc->px_fp, recno*pxh->px_recordsize, SEEK_CUR)) < 0) {
				px_error(pxdoc, PX_RuntimeError, _("Could not fseek"));
				return NULL;
			}
			if((ret = fread(data, pxh->px_recordsize, 1, pxdoc->px_fp)) < 0) {
				return NULL;
			}
		} else { /* skip rest of block */
//			printf("skippin rest of block %d\n", blockcount);
			if((ret = fseek(pxdoc->px_fp, pxh->px_maxtablesize*0x400-6, SEEK_CUR)) < 0) {
				px_error(pxdoc, PX_RuntimeError, _("Could not fseek"));
				return NULL;
			}
		}
		blockcount++;
		recno -= (datasize/pxh->px_recordsize+1);
	}

	if(found)
		return data;
	else
		return NULL;
}

PXLIB_API void PXLIB_CALL
PX_close(pxdoc_t *pxdoc) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return;
	}

	if((pxdoc->closefp) && (pxdoc->px_fp != NULL))
		fclose(pxdoc->px_fp);
	pxdoc->px_fp = NULL;
}

PXLIB_API void PXLIB_CALL
PX_delete(pxdoc_t *pxdoc) {
	pxfield_t *pfield;
	int i;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return;
	}

#if PX_USE_RECODE
	if(pxdoc->recode_outer)
		recode_delete_outer(pxdoc->recode_outer);

	if(pxdoc->recode_request)
		recode_delete_request(pxdoc->recode_request);
#else
#if PX_USE_ICONV
	if(pxdoc->iconvcd > 0)
		iconv_close(pxdoc->iconvcd);
#endif
#endif

	if(pxdoc->targetencoding)
		pxdoc->free(pxdoc, pxdoc->targetencoding);

	if(pxdoc->px_head != NULL) {
		if(pxdoc->px_head->px_tablename) pxdoc->free(pxdoc, pxdoc->px_head->px_tablename);
		pfield = pxdoc->px_head->px_fields;
		if(pfield != NULL) {
			for(i=0; i<pxdoc->px_head->px_numfields; i++) {
				if(pfield->px_fname) pxdoc->free(pxdoc, pfield->px_fname);
				pfield++;
			}
			pxdoc->free(pxdoc, pxdoc->px_head->px_fields);
		}
		pxdoc->free(pxdoc, pxdoc->px_head);
	}
	pxdoc->free(pxdoc, pxdoc);
}

PXLIB_API pxfield_t* PXLIB_CALL
PX_get_fields(pxdoc_t *pxdoc) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return NULL;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("File has no header"));
		return NULL;
	}

	return(pxdoc->px_head->px_fields);
}

PXLIB_API pxfield_t* PXLIB_CALL
PX_get_field(pxdoc_t *pxdoc, int fieldno) {
	pxhead_t *pxh;
	pxfield_t *pfield;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return NULL;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("File has no header"));
		return NULL;
	}

	pxh = pxdoc->px_head;

	if((fieldno < 0) || (fieldno >= pxh->px_numfields)) {
		px_error(pxdoc, PX_RuntimeError, _("Field number out of range"));
		return NULL;
	}

	pfield = pxh->px_fields;
	pfield += fieldno;

	return(pfield);
}

PXLIB_API int PXLIB_CALL
PX_get_num_fields(pxdoc_t *pxdoc) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("File has no header"));
		return -1;
	}

	return(pxdoc->px_head->px_numfields);
}

PXLIB_API int PXLIB_CALL
PX_get_num_records(pxdoc_t *pxdoc) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("File has no header"));
		return -1;
	}

	return(pxdoc->px_head->px_numrecords);
}

PXLIB_API int PXLIB_CALL
PX_set_targetencoding(pxdoc_t *pxdoc, char *encoding) {
#if PX_USE_RECODE || PX_USE_ICONV
	char buffer[30];

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Header of file has not been read"));
		return -1;
	}

	pxdoc->targetencoding = px_strdup(pxdoc, encoding);
	if(pxdoc->targetencoding) {
#if PX_USE_RECODE
		sprintf(buffer, "CP%d/CR-LF..%s", pxdoc->px_head->px_doscodepage, pxdoc->targetencoding);
		recode_scan_request(pxdoc->recode_request, buffer);
#else
#if PX_USE_ICONV
		sprintf(buffer, "CP%d", pxdoc->px_head->px_doscodepage);
		if(pxdoc->iconvcd > 0)
			iconv_close(pxdoc->iconvcd);
		if((iconv_t)(-1) == (pxdoc->iconvcd = iconv_open(pxdoc->targetencoding, buffer))) {
			pxdoc->free(pxdoc, pxdoc->targetencoding);
			px_error(pxdoc, PX_RuntimeError, _("Target encoding could not be set."));
			return -1;
		}
	
#endif
#endif
	}
#else
	px_error(pxdoc, PX_RuntimeError, _("Library has not been compiled with support for target encoding."));
#endif
	return 0;
}

/******* Function to access Blob files *******/

PXLIB_API pxblob_t* PXLIB_CALL
PX_new_blob(pxdoc_t *pxdoc) {
	pxblob_t *pxblob;

	pxblob = pxdoc->malloc(pxdoc, sizeof(pxblob_t), _("Couldn't get memory for blob."));
	if(!pxblob) {
		return(NULL);
	} else {
		pxblob->pxdoc = pxdoc;
		return(pxblob);
	}
}

PXLIB_API int PXLIB_CALL
PX_open_blob_fp(pxblob_t *pxblob, FILE *fp) {

	pxblob->px_fp = fp;
}

PXLIB_API int PXLIB_CALL
PX_open_blob_file(pxblob_t *pxblob, char *filename) {
	FILE *fp;

	if(!pxblob) {
		return(-1);
	}

	if((fp = fopen(filename, "r")) == NULL) {
		return -1;
	}

	if(0 > PX_open_blob_fp(pxblob, fp)) {
		fclose(fp);
		return -1;
	}

	pxblob->px_name = px_strdup(pxblob->pxdoc, filename);
	pxblob->closefp = px_true;
	return 0;
}

PXLIB_API void PXLIB_CALL
PX_close_blob(pxblob_t *pxblob) {
	if((pxblob->closefp) && (pxblob->px_fp != 0)) {
		fclose(pxblob->px_fp);
		pxblob->px_fp = NULL;
		pxblob->pxdoc = NULL;
	}
}

PXLIB_API char* PXLIB_CALL
PX_read_blobdata(pxblob_t *pxblob, int offset, size_t size) {
	int ret;
	char *blobdata;
	char head[9];
	pxdoc_t *pxdoc = pxblob->pxdoc;

	if(!pxblob || !pxblob->px_fp) {
		px_error(pxdoc, PX_RuntimeError, _("PXDoc may not be NULL."));
		return(NULL);
	}

	if(size <= 0) {
		px_error(pxdoc, PX_RuntimeError, _("Makes no sense to read blob with 0 or less bytes."));
		return(NULL);
	}

	if((ret = fseek(pxblob->px_fp, offset, SEEK_SET)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek"));
		return NULL;
	}

	if((ret = fread(head, 9, 1, pxblob->px_fp)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read head of blob data."));
		return NULL;
	}

	if(size != get_long_be(&head[3])) {
		px_error(pxdoc, PX_RuntimeError, _("Blob does not have expected size (%d != %d)"), size, get_long_be(&head[3]));
		return(NULL);
	}

	blobdata = pxdoc->malloc(pxblob->pxdoc, size, _("Couldn't get memory for blob."));
	if(!blobdata) {
		return(NULL);
	}

	if((ret = fread(blobdata, size, 1, pxblob->px_fp)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read all blob data."));
		return NULL;
	}

	return(blobdata);
}

PXLIB_API int PXLIB_CALL
PX_get_data_alpha(pxdoc_t *pxdoc, char *data, int len, char **value) {
	char *buffer, *obuf = NULL;
	size_t olen;
	int res;

	if(data[0] == '\0') {
		*value = NULL;
		return 0;
	}

	if(pxdoc->targetencoding != NULL) {
#if PX_USE_RECODE
		int oallocated = 0;
		res = recode_buffer_to_buffer(pxdoc->recode_request, data, len, &obuf, &olen, &oallocated);
#else
#if PX_USE_ICONV
		size_t ilen = len;
		char *iptr, *optr;
		olen = len + 1;
		/* Do not pxdoc->malloc because the memory is freed with free
		 * We use free because the memory allocated by recode_buffer_to_buffer()
		 * is requested with malloc and must be freed with free.
		 */
		optr = obuf = (char *) malloc(olen);
		iptr = data;
//		printf("data(%d) = '%s'\n", ilen, data);
//		printf("obuf(%d) = '%s'\n", olen, obuf);
		if(0 > (res = iconv(pxdoc->iconvcd, &iptr, &ilen, &optr, &olen))) {
			*value = NULL;
			free(obuf);
			return 0;
		}
//		printf("data(%d) = '%s'\n", ilen, data);
//		printf("obuf(%d) = '%s'\n", olen, obuf);
		olen = len;
#endif
#endif
	} else {
		olen = len;
		obuf = data;
	}
	/* Copy the encoded string into memory which belongs to pxlib */
	buffer = (char *) pxdoc->malloc(pxdoc, olen+1,  _("Could not allocate memory for field data."));
	if(!buffer) {
		if(pxdoc->targetencoding != NULL) {
			free(obuf);
		}
		*value = NULL;
		return 0;
	}
	memcpy(buffer, obuf, olen);
	buffer[olen] = '\0';
	*value = buffer;

	if(pxdoc->targetencoding != NULL) {
		free(obuf);
	}

	return 1;
}

PXLIB_API int PXLIB_CALL
PX_get_data_double(pxdoc_t *pxdoc, char *data, int len, double *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
	} else if(*((long long int *)data) != 0) {
		int k = 0;
		for(k=0; k<len; k++)
			data[k] = ~data[k];
	} else {
		*value = 0;
		return 0;
	}
	*value = get_double_be(data); //*((double *)data);
	return 1;
}

PXLIB_API int PXLIB_CALL
PX_get_data_long(pxdoc_t *pxdoc, char *data, int len, long *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
	} else if(*((long int *)data) != 0) {
		data[0] |= 0x80;
	} else {
		return 0;
	}
	*value = get_long_be(data); //*((long int *)data);
	return 1;
}

PXLIB_API int PXLIB_CALL
PX_get_data_short(pxdoc_t *pxdoc, char *data, int len, short int *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
	} else if(*((short int *)data) != 0) {
		data[0] |= 0x80;
	} else {
		return 0;
	}
	*value = get_short_be(data);
	return 1;
}

