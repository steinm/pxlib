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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "pxversion.h"
#include "px_intern.h"
#include "paradox.h"
#include "px_memory.h"
#include "px_head.h"
#include "px_error.h"

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

	return pxdoc;
}

PXLIB_API pxdoc_t* PXLIB_CALL
PX_new(void) {
	return(PX_new2(NULL, NULL, NULL, NULL));
}

PXLIB_API int PXLIB_CALL
PX_open_fp(pxdoc_t *pxdoc, FILE *fp) {

	if((pxdoc->px_head = get_px_head(pxdoc, fp)) == NULL) {
		fprintf(stderr, "Unable to get header\n");
		return -1;
	}

	pxdoc->px_fp = fp;
}

PXLIB_API int PXLIB_CALL
PX_open_file(pxdoc_t *pxdoc, char *filename) {
	FILE *fp;

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

	if((pxdoc == NULL) || (pxdoc->px_head == NULL)) {
		return NULL;
	}
	pxh = pxdoc->px_head;

	/* Go to the start of the data block (skip the header) */
	if((ret = fseek(pxdoc->px_fp, pxh->px_headersize, SEEK_SET)) < 0) {
		printf("Could not fseek\n");
		return NULL;
	}

	found = 0;
	blockcount = 0;
	while(!found && (blockcount < pxh->px_fileblocks)) {
		int datasize;
		/* Get the info about this data block */
		if((ret = fread(&datablock, sizeof(TDataBlock), 1, pxdoc->px_fp)) < 0) {
			printf("Could not read\n");
			return NULL;
		}
		datasize = get_short(&datablock.addDataSize);
//		printf("datasize = %d, recno = %d, platz verbraucht = %d\n", datasize, recno, (recno+1)*pxh->px_recordsize);
		if(recno*pxh->px_recordsize <= datasize) {
			found = 1;
			if((ret = fseek(pxdoc->px_fp, recno*pxh->px_recordsize, SEEK_CUR)) < 0) {
				printf("Could not fseek\n");
				return NULL;
			}
			if((ret = fread(data, pxh->px_recordsize, 1, pxdoc->px_fp)) < 0) {
				return NULL;
			}
		} else { /* skip rest of block */
//			printf("skippin rest of block %d\n", blockcount);
			if((ret = fseek(pxdoc->px_fp, pxh->px_maxtablesize*0x400-6, SEEK_CUR)) < 0) {
				printf("Could not fseek\n");
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
	if((pxdoc->closefp) && (pxdoc->px_fp != 0))
		fclose(pxdoc->px_fp);
}

/******* Function to access Blob files *******/

PXLIB_API pxblob_t* PXLIB_CALL
PX_new_blob(pxdoc_t *pxdoc) {
	pxblob_t *pxblob;

	pxblob = px_malloc(pxdoc, sizeof(pxblob_t), _("Couldn't get memory for blob."));
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

	if(!pxblob || !pxblob->px_fp) {
		px_error(pxblob->pxdoc, PX_RuntimeError, _("PXDoc may not be NULL."));
		return(NULL);
	}

	if(size <= 0) {
		px_error(pxblob->pxdoc, PX_RuntimeError, _("Makes no sense to read blob with 0 or less bytes."));
		return(NULL);
	}

	if((ret = fseek(pxblob->px_fp, offset, SEEK_SET)) < 0) {
		printf("Could not fseek\n");
		return NULL;
	}

	if((ret = fread(head, 9, 1, pxblob->px_fp)) < 0) {
		printf("Could not read head of blob data.\n");
		return NULL;
	}

	if(size != get_long(&head[3])) {
		px_error(pxblob->pxdoc, PX_RuntimeError, _("Blob does not have expected size (%d != %d)"), size, get_long(&head[3]));
		return(NULL);
	}

	blobdata = px_malloc(pxblob->pxdoc, size, _("Couldn't get memory for blob."));
	if(!blobdata) {
		return(NULL);
	}

	if((ret = fread(blobdata, size, 1, pxblob->px_fp)) < 0) {
		printf("Could not read all blob data.\n");
		return NULL;
	}

	return(blobdata);
}

PXLIB_API int PXLIB_CALL
PX_get_data_double(char *data, int len, double *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
		*value = *((double *)data);
		return 1;
	} else if(*((long long int *)data) != 0) {
		int k = 0;
		for(k=0; k<len; k++)
			data[k] = ~data[k];
		*value = *((double *)data);
		return 1;
	}
	return 0;
}

PXLIB_API int PXLIB_CALL
PX_get_data_long(char *data, int len, long *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
		*value = *((long int *)data);
		return 1;
	} else if(*((long int *)data) != 0) {
		data[0] |= 0x80;
		*value = *((long int *)data);
		return 1;
	} 
	return 0;
}

PXLIB_API int PXLIB_CALL
PX_get_data_short(char *data, int len, short int *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
		*value = *((short int *)data);
		return 1;
	} else if(*((short int *)data) != 0) {
		data[0] |= 0x80;
		*value = *((short int *)data);
		return 1;
	} 
	return 0;
}

