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
#if defined(WIN32) || defined(OS2)
#include <fcntl.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#endif

#include "pxversion.h"
#include "px_intern.h"
#include "paradox-gsf.h"
#include "px_memory.h"
#include "px_head.h"
#include "px_io.h"
#include "px_error.h"
#include "px_misc.h"

/* PX_get_majorversion() {{{
 */
PXLIB_API int PXLIB_CALL
PX_get_majorversion(void) {
	return(PXLIB_MAJOR_VERSION);
}
/* }}} */

/* PX_get_minorversion() {{{
 */
PXLIB_API int PXLIB_CALL
PX_get_minorversion(void) {
	return(PXLIB_MINOR_VERSION);
}
/* }}} */

/* PX_get_subminorversion() {{{
 */
PXLIB_API int PXLIB_CALL
PX_get_subminorversion(void) {
	return(PXLIB_MICRO_VERSION);
}
/* }}} */

/* PX_has_recode_support() {{{
 */
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
/* }}} */

/* PX_has_gsf_support() {{{
 */
PXLIB_API int PXLIB_CALL
PX_has_gsf_support(void) {
#if PX_HAVE_GSF
	return(1);
#endif
	return(0);
}
/* }}} */

/* PX_is_bigendian() {{{
 */
PXLIB_API int PXLIB_CALL
PX_is_bigendian(void) {
#if WORDS_BIGENDIAN
	return(1);
#else
	return(0);
#endif
}
/* }}} */

/* PX_new3() {{{
 * Create a new Paradox DB file and set memory management, error
 * handling functions and the user data passed to the error handler.
 */
PXLIB_API pxdoc_t* PXLIB_CALL
PX_new3(void  (*errorhandler)(pxdoc_t *p, int type, const char *msg),
        void* (*allocproc)(pxdoc_t *p, size_t size, const char *caller),
        void* (*reallocproc)(pxdoc_t *p, void *mem, size_t size, const char *caller),
        void  (*freeproc)(pxdoc_t *p, void *mem),
		void* errorhandler_user_data) {
	pxdoc_t *pxdoc;

	if(allocproc == NULL) {
		allocproc = px_malloc;
		reallocproc = px_realloc;
		freeproc  = px_free;
	}
	if (errorhandler == NULL)
		errorhandler = px_errorhandler; 
	if(NULL == (pxdoc = (pxdoc_t *) (* allocproc) (NULL, sizeof(pxdoc_t), "PX_new3: Allocate memory for px document."))) {
		(*errorhandler)(NULL, PX_MemoryError, _("Couldn't allocate PX object"));
		return(NULL);
	}
	memset((void *)pxdoc, 0, (size_t) sizeof(pxdoc_t));
	pxdoc->errorhandler = errorhandler;
	pxdoc->errorhandler_user_data = errorhandler_user_data;
	pxdoc->malloc = allocproc;
	pxdoc->realloc = reallocproc;
	pxdoc->free = freeproc;
	pxdoc->px_stream = NULL;

	pxdoc->px_pindex = NULL;

#if PX_USE_RECODE
	pxdoc->recode_outer = recode_new_outer(false);
	pxdoc->out_recode_request = recode_new_request(pxdoc->recode_outer);
	pxdoc->in_recode_request = recode_new_request(pxdoc->recode_outer);
#else
#if PX_USE_ICONV
	pxdoc->out_iconvcd = (iconv_t) -1;
	pxdoc->in_iconvcd = (iconv_t) -1;
#endif
#endif
	pxdoc->targetencoding = NULL;
	pxdoc->inputencoding = NULL;
	pxdoc->px_data = NULL;
	pxdoc->px_datalen = 0;

	return pxdoc;
}
/* }}} */

/* PX_new2() {{{
 * Create a new Paradox DB file and set memory management and error
 * handling functions.
 */
PXLIB_API pxdoc_t* PXLIB_CALL
PX_new2(void  (*errorhandler)(pxdoc_t *p, int type, const char *msg),
        void* (*allocproc)(pxdoc_t *p, size_t size, const char *caller),
        void* (*reallocproc)(pxdoc_t *p, void *mem, size_t size, const char *caller),
        void  (*freeproc)(pxdoc_t *p, void *mem)) {
	return(PX_new3(errorhandler, allocproc, reallocproc, freeproc, NULL));
}
/* }}} */

/* PX_new() {{{
 * Create new Paradox DB file.
 * Use the default memory management and error handling functions.
 */
PXLIB_API pxdoc_t* PXLIB_CALL
PX_new(void) {
	return(PX_new3(NULL, NULL, NULL, NULL, NULL));
}
/* }}} */

/* build_primary_index() {{{
 * Build a primary index.
 */
static int build_primary_index(pxdoc_t *pxdoc) {
	pxhead_t *pxh;
	pxstream_t *pxs;
	pxpindex_t *pindex;
	int blockcount, blocknumber;

	pxh = pxdoc->px_head;
	pxs = pxdoc->px_stream;
	if(NULL == (pindex = pxdoc->malloc(pxdoc, pxh->px_fileblocks*sizeof(pxpindex_t), _("Allocate memory for self build internal primary index.")))) {
		px_error(pxdoc, PX_MemoryError, _("Could not allocate memory for self build internal index."));
		return -1;
	}

	pxdoc->px_indexdata = pindex;
	pxdoc->px_indexdatalen = pxh->px_fileblocks;
	blockcount = 0; /* Just a block counter */
	blocknumber = pxh->px_firstblock; /* Will be set to next block number */
	while((blockcount < pxh->px_fileblocks) && (blocknumber > 0)) {
		TDataBlock datablockhead;
		if(get_datablock_head(pxdoc, pxs, blocknumber, &datablockhead) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not get head of data block nr. %d."), blocknumber);
			pxdoc->free(pxdoc, pindex);
			return -1;
		}
		pindex[blockcount].data = NULL;
		pindex[blockcount].blocknumber = blocknumber;
		pindex[blockcount].numrecords = (get_short_le((char *) &datablockhead.addDataSize)/pxh->px_recordsize)+1;
		pindex[blockcount].myblocknumber = 0;
		pindex[blockcount].level = 1;
		blockcount++;
		blocknumber = get_short_le((char *) &datablockhead.nextBlock);
	}
}
/* }}} */

#if PX_HAVE_GSF
/* PX_open_gsf() {{{
 * Read from a Paradox DB file, which has already been opend with gsf.
 */
PXLIB_API int PXLIB_CALL
PX_open_gsf(pxdoc_t *pxdoc, GsfInput *gsf) {
	pxhead_t *pxh;
	pxstream_t *pxs;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if(NULL == (pxs = pxdoc->malloc(pxdoc, sizeof(pxstream_t), _("Allocate memory for io stream.")))) {
		px_error(pxdoc, PX_MemoryError, _("Could not allocate memory for io stream."));
		return -1;
	}
	pxs->type = pxfIOGsf;
	pxs->mode = pxfFileRead;
	pxs->close = px_false;
	pxs->s.gsfin = gsf;
	
	pxdoc->read = px_gsfread;
	pxdoc->seek = px_gsfseek;
	pxdoc->tell = px_gsftell;
	pxdoc->write = px_gsfwrite;

	if((pxdoc->px_head = get_px_head(pxdoc, pxs)) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Unable to get header."));
		return -1;
	}

	pxdoc->px_stream = pxs;

	/* Build primary index. This index misses all index blocks with a level
	 * greater than 1. Since they are not used currently this is of no harm.
	 */
	pxh = pxdoc->px_head;
	if(pxh->px_filetype == pxfFileTypIndexDB ||
	   pxh->px_filetype == pxfFileTypNonIndexDB) {
		if(build_primary_index(pxdoc) < 0) {
			return -1;
		}
	}
	return 0;
}
/* }}} */
#endif /* PX_HAVE_GSF */

/* PX_open_fp() {{{
 * Read from a Paradox DB file, which has already been opend with fopen.
 */
PXLIB_API int PXLIB_CALL
PX_open_fp(pxdoc_t *pxdoc, FILE *fp) {
	pxhead_t *pxh;
	pxstream_t *pxs;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if(NULL == (pxs = pxdoc->malloc(pxdoc, sizeof(pxstream_t), _("Allocate memory for io stream.")))) {
		px_error(pxdoc, PX_MemoryError, _("Could not allocate memory for io stream."));
		return -1;
	}
	pxs->type = pxfIOFile;
	pxs->mode = pxfFileRead;
	pxs->close = px_false;
	pxs->s.fp = fp;
	
	pxdoc->read = px_fread;
	pxdoc->seek = px_fseek;
	pxdoc->tell = px_ftell;
	pxdoc->write = px_fwrite;

	if((pxdoc->px_head = get_px_head(pxdoc, pxs)) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Unable to get header."));
		return -1;
	}

	pxdoc->px_stream = pxs;

	/* Build primary index. This index misses all index blocks with a level
	 * greater than 1. Since they are not used currently this is of no harm.
	 */
	pxh = pxdoc->px_head;
	if(pxh->px_filetype == pxfFileTypIndexDB ||
	   pxh->px_filetype == pxfFileTypNonIndexDB) {
		if(build_primary_index(pxdoc) < 0) {
			return -1;
		}
	}
	return 0;
}
/* }}} */

/* PX_open_file() {{{
 * Read from a Paradox DB file. Open the file itself. Use PX_open_fp()
 * if the file has been open already with fopen().
 */
PXLIB_API int PXLIB_CALL
PX_open_file(pxdoc_t *pxdoc, char *filename) {
	FILE *fp;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database."));
		return -1;
	}

	if((fp = fopen(filename, "r")) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Could not open file of paradox database."));
		return -1;
	}

	if(0 > PX_open_fp(pxdoc, fp)) {
		px_error(pxdoc, PX_RuntimeError, _("Could not open paradox database."));
		fclose(fp);
		return -1;
	}

	pxdoc->px_name = px_strdup(pxdoc, filename);
	pxdoc->px_stream->close = px_true;
	return 0;
}
/* }}} */

/* PX_create_fp() {{{
 * Create a new paradox database.
 */
PXLIB_API int PXLIB_CALL
PX_create_fp(pxdoc_t *pxdoc, pxfield_t *fields, int numfields, FILE *fp) {
	pxhead_t *pxh;
	pxfield_t *pxf;
	pxstream_t *pxs;
	int i, recordsize = 0;

	if((pxh = (pxhead_t *) pxdoc->malloc(pxdoc, sizeof(pxhead_t), _("PX_create_fp: Allocate memory for document header."))) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Could not allocate memory for document header."));
		return -1;
	}
	pxh->px_filetype = pxfFileTypNonIndexDB;
	pxh->px_fileversion = 70;
	pxh->px_tablename = NULL;
	pxh->px_numrecords = 0;
	pxh->px_numfields = numfields;
	pxh->px_fields = fields;
	pxh->px_writeprotected = 0;
	pxh->px_headersize = 0x0800;
	pxh->px_fileblocks = 0;
	pxh->px_firstblock = 0;
	pxh->px_lastblock = 0;
	pxh->px_maxtablesize = 16;
	pxh->px_doscodepage = 1252;
	pxh->px_primarykeyfields = 0;
	pxh->px_autoinc = 0;
	pxh->px_sortorder = 0x62;

	/* Calculate record size */
	pxf = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++, pxf++) {
		recordsize += pxf->px_flen;
	}
	pxh->px_recordsize = recordsize;
	if(recordsize < 30) {
		pxh->px_maxtablesize = 2;
	} else if(recordsize < 120) {
		pxh->px_maxtablesize = 3;
	}

	if(NULL == (pxs = pxdoc->malloc(pxdoc, sizeof(pxstream_t), _("Allocate memory for io stream.")))) {
		px_error(pxdoc, PX_MemoryError, _("Could not allocate memory for io stream."));
		return -1;
	}
	pxs->type = pxfIOFile;
	pxs->mode = pxfFileWrite;
	pxs->close = px_false;
	pxs->s.fp = fp;
	
	pxdoc->read = px_fread;
	pxdoc->seek = px_fseek;
	pxdoc->tell = px_ftell;
	pxdoc->write = px_fwrite;

	if(put_px_head(pxdoc, pxh, pxs) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Unable to put header."));
		return -1;
	}

	pxdoc->px_head = pxh;
	pxdoc->px_stream = pxs;
	return 0;
}
/* }}} */

/* PX_create_file() {{{
 * Create a Paradox DB file. Open the file itself. Use PX_create_fp()
 * if the file has been open already with fopen().
 */
PXLIB_API int PXLIB_CALL
PX_create_file(pxdoc_t *pxdoc, pxfield_t *fields, int numfields, char *filename) {
	FILE *fp;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database."));
		return -1;
	}

	if((fp = fopen(filename, "w+")) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Could not create file of paradox database."));
		return -1;
	}

	if(0 > PX_create_fp(pxdoc, fields, numfields, fp)) {
		px_error(pxdoc, PX_RuntimeError, _("Could not open paradox database."));
		fclose(fp);
		return -1;
	}

//	pxh->px_tablename = px_strdup(pxdoc, filename);
	PX_set_tablename(pxdoc, filename);
	pxdoc->px_name = px_strdup(pxdoc, filename);
	pxdoc->px_close_fp = px_true;
	return 0;
}
/* }}} */

/* PX_set_value() {{{
 * Sets a numeric value
 */
PXLIB_API void PXLIB_CALL
PX_set_value(pxdoc_t *pxdoc, char *name, float value) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database file"));
		return;
	}
}
/* }}} */

/* PX_get_value() {{{
 * Gets a numeric value
 */
PXLIB_API float PXLIB_CALL
PX_get_value(pxdoc_t *pxdoc, char *name) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database file"));
		return;
	}
}
/* }}} */

/* PX_set_parameter() {{{
 * Sets a string value
 */
PXLIB_API void PXLIB_CALL
PX_set_parameter(pxdoc_t *pxdoc, char *name, char *value) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database file"));
		return;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Header of file has not been read"));
		return;
	}

	if(strcmp(name, "tablename") == 0) {
		if(pxdoc->px_head->px_tablename)
			px_free(pxdoc, pxdoc->px_head->px_tablename);

		pxdoc->px_head->px_tablename = px_strdup(pxdoc, value);
		if(pxdoc->px_stream->mode & pxfFileWrite) {
			if(put_px_head(pxdoc, pxdoc->px_head, pxdoc->px_stream) < 0) {
				return;
			}
		} else {
			px_error(pxdoc, PX_Warning, _("File is not writable. Setting '%s' has no effect."), name);
			return;
		}
	} else if(strcmp(name, "targetencoding") == 0) {
#if PX_USE_RECODE || PX_USE_ICONV
		if(pxdoc->targetencoding)
			pxdoc->free(pxdoc, pxdoc->targetencoding);
		pxdoc->targetencoding = px_strdup(pxdoc, value);
		if(pxdoc->targetencoding) {
			char buffer[30];
#if PX_USE_RECODE
			sprintf(buffer, "CP%d/CR-LF..%s", pxdoc->px_head->px_doscodepage, pxdoc->targetencoding);
			recode_scan_request(pxdoc->out_recode_request, buffer);
#else
#if PX_USE_ICONV
			sprintf(buffer, "CP%d", pxdoc->px_head->px_doscodepage);
			if(pxdoc->out_iconvcd > 0)
				iconv_close(pxdoc->out_iconvcd);
			if((iconv_t)(-1) == (pxdoc->out_iconvcd = iconv_open(pxdoc->targetencoding, buffer))) {
				pxdoc->free(pxdoc, pxdoc->targetencoding);
				pxdoc->targetencoding = NULL;
				px_error(pxdoc, PX_RuntimeError, _("Target encoding could not be set."));
				return;
			}
#endif
#endif
		}
#else
		px_error(pxdoc, PX_RuntimeError, _("Library has not been compiled with support for reencoding."));
#endif
	} else if(strcmp(name, "inputencoding") == 0) {
#if PX_USE_RECODE || PX_USE_ICONV
		if(pxdoc->inputencoding)
			pxdoc->free(pxdoc, pxdoc->inputencoding);
		pxdoc->inputencoding = px_strdup(pxdoc, value);
		if(pxdoc->inputencoding) {
			char buffer[30];
#if PX_USE_RECODE
			sprintf(buffer, "%s..CP%d/CR-LF", pxdoc->inputencoding, pxdoc->px_head->px_doscodepage);
			recode_scan_request(pxdoc->in_recode_request, buffer);
#else
#if PX_USE_ICONV
			sprintf(buffer, "CP%d", pxdoc->px_head->px_doscodepage);
			if(pxdoc->in_iconvcd > 0)
				iconv_close(pxdoc->in_iconvcd);
			if((iconv_t)(-1) == (pxdoc->in_iconvcd = iconv_open(buffer, pxdoc->inputencoding))) {
				pxdoc->free(pxdoc, pxdoc->inputencoding);
				pxdoc->inputencoding = NULL;
				px_error(pxdoc, PX_RuntimeError, _("Input encoding could not be set."));
				return;
			}
#endif
#endif
		}
#else
		px_error(pxdoc, PX_RuntimeError, _("Library has not been compiled with support for reencoding."));
#endif
	}
}
/* }}} */

/* PX_get_parameter() {{{
 * Gets a string value
 */
PXLIB_API const char * PXLIB_CALL
PX_get_parameter(pxdoc_t *pxdoc, char *name) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database file"));
		return;
	}
}
/* }}} */

/* PX_add_primary_index() {{{
 * Use a primary index for an DB file. The index has to open before with
 * PX_open_fp() PX_open_filename(). After adding an index it will be
 * used for accessing database records.
 * If this function has been called before for the same DB file, the
 * old index will be deleted first. Make sure to actually read the
 * index with PX_read_primary_index() and not just open it.
 */
PXLIB_API int PXLIB_CALL
PX_add_primary_index(pxdoc_t *pxdoc, pxdoc_t *pindex) {
	if(pxdoc == NULL ||
	   pxdoc->px_head == NULL ||
	   pxdoc->px_head->px_filetype != pxfFileTypIndexDB) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database file"));
		return -1;
	}

	if(pindex == NULL ||
	   pindex->px_head == NULL ||
	   pindex->px_head->px_filetype != pxfFileTypPrimIndex) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox primary index file"));
		return -1;
	}

	if(pindex->px_data == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Primary index file has no index data"));
		return -1;
	}

	/* Delete an existing primary index file */
	if(pxdoc->px_pindex) {
		PX_delete(pxdoc->px_pindex);
	}
	pxdoc->px_pindex = pindex;
	pxdoc->px_indexdata = pindex->px_data;
	pxdoc->px_indexdatalen = pindex->px_head->px_numrecords;

	return 0;
}
/* }}} */

/* PX_read_primary_index() {{{
 * Read the primary index completly into an internal array.
 */
PXLIB_API int PXLIB_CALL
PX_read_primary_index(pxdoc_t *pindex) {
	pxpindex_t *pindex_data, pdata;
	pxhead_t *pxh;
	pxfield_t *pxf;
	char *data;
	int i, j, swap, datalen;

	if(pindex == NULL ||
	   pindex->px_head == NULL ||
	   pindex->px_head->px_filetype != pxfFileTypPrimIndex) {
		px_error(pindex, PX_RuntimeError, _("Did not pass a paradox primary index file"));
		return -1;
	}

	pxh = pindex->px_head;
	pindex->px_data = pindex->malloc(pindex, pxh->px_numrecords*sizeof(pxpindex_t), _("Allocate memory for primary index data."));
	if(!pindex->px_data) {
		px_error(pindex, PX_RuntimeError, _("Could not allocate memory for primary index data."));
		return -1;
	}
	pindex->px_datalen = pxh->px_numrecords;

	pindex_data = (pxpindex_t *) pindex->px_data;
	memset(pindex_data, 0, pxh->px_numrecords*sizeof(pxpindex_t));

	if((data = (char *) pindex->malloc(pindex, pxh->px_recordsize, _("Allocate memory for data of index record."))) == NULL) {
		px_error(pindex, PX_RuntimeError, _("Could not allocate memory for primary index data."));
		pindex->free(pindex, pindex->px_data);
		return -1;
	}

	/* Read over the field data.
	 * px_numfields does not count the fields with information about
	 * block position and num of records per block. It is only the
	 * number of index fields. */
	datalen = 0;
	pxf = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++) {
		datalen += pxf->px_flen;
		pxf++;
	}
	if(datalen != pxh->px_recordsize-6) {
		px_error(pindex, PX_RuntimeError, _("Inconsistency in length of primary index record. Expected %d but calculated %d."), pxh->px_recordsize-6, datalen);
		pindex->free(pindex, data);
		pindex->free(pindex, pindex->px_data);
		pindex->px_data = NULL;
		return(-1);
	}
	for(j=0; j<pxh->px_numrecords; j++) {
		pxdatablockinfo_t pxdbinfo;
		int isdeleted=0;
		if(PX_get_record2(pindex, j, data, &isdeleted, &pxdbinfo)) {
			short int value;
			/* Copy the data part for later sorting */
			pindex_data[j].data = pindex->malloc(pindex, datalen, _("Allocate memory for data part of index record."));
			memcpy(pindex_data[j].data, data, datalen);
			/* Get the index data */
			PX_get_data_short(pindex, &data[datalen], 2, &value);
			pindex_data[j].blocknumber = value;
			PX_get_data_short(pindex, &data[datalen+2], 2, &value);
			pindex_data[j].numrecords = value;
			PX_get_data_short(pindex, &data[datalen+4], 2, &value);
			pindex_data[j].dummy = value;
			pindex_data[j].myblocknumber = pxdbinfo.number;
		} else {
			px_error(pindex, PX_RuntimeError, _("Could not read record no. %d of primary index data."), j);
			/* Free so far allocated data memory */
			for(j--; j>=0; j--)
				pindex->free(pindex, pindex_data->data);
			pindex->free(pindex, data);
			pindex->free(pindex, pindex->px_data);
			pindex->px_data = NULL;
			return -1;
		}
	}
	/* find level of index blocks. Index blocks of level 1 contain references
	 * to data blocks. Index blocks of level n+1 contain references to index
	 * blocks of level n. */
	/* If the number of data blocks is 1 then there will be no index blocks
	 * of level 2, and all blocks will be of level 1.
	 * In all other case we expect only blocks of level 2 and 1.
	 * This is an assumption which is only true for a
	 * certain number of records, which is usually quite high. If for example
	 * each data block in the database contains 10 records, and each level 1
	 * index block contains 50 block references, you will end up in 500
	 * records. The next index level will enlarge this to 25000 if only
	 * on block will be used in this level.
	 * For now this has to be sufficient. */
	if(pxh->px_fileblocks == 1) {
		for(j=0; j<pxh->px_numrecords; j++)
			pindex_data[j].level = 1;
	} else {
		int firstblock = pindex_data[0].myblocknumber;
		int numrecords = 0;
		for(j=0; j<pxh->px_numrecords, pindex_data[j].myblocknumber == firstblock; j++) {
			numrecords += pindex_data[j].numrecords;
			pindex_data[j].level = 2;
		}
		for(; j<pxh->px_numrecords; j++) {
			numrecords -= pindex_data[j].numrecords;
			pindex_data[j].level = 1;
		}
		if(numrecords != 0) {
			px_error(pindex, PX_Warning, _("The number of records coverd by index level 2 is unequal to level 1."));
		}
	}
//	for(j=0; j<pxh->px_numrecords-1; j++) {
//		printf("%d\t%d\n", pindex_data[j].myblocknumber, pindex_data[j].level);
//	}

	pindex->free(pindex, data);
	return 0;
}
/* }}} */

/* px_get_record_pos_with_index() {{{
 * Locates a database record by using the primary index.
 * The index is used by adding the number of records per block
 * until the block is found where the record with the given number
 * is stored. The function still disregards any sorting within the
 * index. The record number is not an absolut value. Accessing a
 * database file with and without the index may result in different
 * record numbers for the same record.
 * Returns 1 if record could be found, otherwise 0
 */
int
px_get_record_pos_with_index(pxdoc_t *pxdoc, int recno, int *deleted, pxdatablockinfo_t *pxdbinfo) {
	int j, numrecords, n, recsperdatablock;
//	pxdoc_t *pindexdoc;
	pxhead_t *pxh; //, *pxih;
	pxpindex_t *pindex_data;

	pxh = pxdoc->px_head;
//	pindexdoc = pxdoc->px_pindex;
//	pxih = pindexdoc->px_head;
//	pindex_data = pindexdoc->px_data;
	pindex_data = pxdoc->px_indexdata;

	if(!pindex_data)
		return 0;

	numrecords = 0 ;
	recsperdatablock = (pxh->px_maxtablesize*0x400-sizeof(TDataBlock))/pxh->px_recordsize;
	for(j=0; j<pxdoc->px_indexdatalen; j++) {
		/* We currently just take level 1 index blocks into account.
		 * This is only for large databases a speed disadvantage.
		 */
		if(pindex_data[j].level == 1) {
			n = pindex_data[j].numrecords;
			numrecords += n;
			if(recno >= n) {
				recno -= n;
			} else {
				int blocksize, ret;
				TDataBlock datablock;

				pxdbinfo->number = pindex_data[j].blocknumber;
				pxdbinfo->recno = recno;
				pxdbinfo->blockpos = pxh->px_headersize + (pxdbinfo->number-1)*pxh->px_maxtablesize*0x400;
				pxdbinfo->recordpos = pxdbinfo->blockpos + sizeof(TDataBlock) + recno*pxh->px_recordsize;

				/* Go to the start of the data block (skip the header) */
				if((ret = pxdoc->seek(pxdoc, pxdoc->px_stream, pxdbinfo->blockpos, SEEK_SET)) < 0) {
					px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of first data block"));
					return 0;
				}

				/* Get the info about this data block */
				if((ret = pxdoc->read(pxdoc, pxdoc->px_stream, sizeof(TDataBlock), &datablock)) < 0) {
					px_error(pxdoc, PX_RuntimeError, _("Could not read"));
					return 0;
				}

				blocksize = get_short_le((char *) &datablock.addDataSize);

				pxdbinfo->prev = get_short_le((char *) &datablock.prevBlock);
				pxdbinfo->next = get_short_le((char *) &datablock.nextBlock);
				pxdbinfo->size = blocksize+pxh->px_recordsize;
				pxdbinfo->numrecords = pxdbinfo->size/pxh->px_recordsize;
				deleted = 0;
				return 1;
			}
		}
	}
	return 0;
}
/* }}} */

/* px_get_record_pos() {{{
 * Reads all data blocks until the requested recno is in the block.
 * This function doesn't use a primary index and is therefore far
 * from being efficient for large files.
 * Returns 1 if record could be found, otherwise 0
 */
int
px_get_record_pos(pxdoc_t *pxdoc, int recno, int *deleted, pxdatablockinfo_t *pxdbinfo) {
	int ret, found, blockcount, blocknumber;
	TDataBlock datablock;
	pxhead_t *pxh;

	pxh = pxdoc->px_head;

	found = 0;
	blockcount = 0; /* Just a block counter */
	blocknumber = pxh->px_firstblock; /* Will be set to next block number */
	while(!found && (blockcount < pxh->px_fileblocks) && (blocknumber > 0)) {
		int datasize, blocksize;

		if(get_datablock_head(pxdoc, pxdoc->px_stream, blocknumber, &datablock) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not get head of data block nr. %d."), blocknumber);
			return 0;
		}

		/* if deleted is set, then we will disregard the block size in the
		 * data block header but take the maximum block size as indicated
		 * by pxh->px_maxtablesize. The variable blocksize is just to test
		 * whether a record is valid or not.
		 * If a block is not completely used the blocksize will be less than
		 * the theortical size of a block. If a block is not used at all
		 * its blocksize is usually much bigger than the maximal data block
		 * size. In the second case we set it -1.
		 */
		blocksize = get_short_le((char *) &datablock.addDataSize);
		if(!*deleted)
			datasize = blocksize; //get_short_le((char *) &datablock.addDataSize);
		else
			datasize = pxh->px_maxtablesize*0x400-sizeof(TDataBlock)-pxh->px_recordsize;
		if(blocksize > pxh->px_maxtablesize*0x400-sizeof(TDataBlock)-pxh->px_recordsize) {
			/* setting blocksize to -1 means that this block contains no valid
			 * records. All records are deleted. The -1 is later used to set
			 * 'deleted' on the proper value.
			 */
			blocksize = -1;
		}

//		printf("datasize = %d, recno = %d, platz verbraucht = %d\n", datasize, recno, (recno+1)*pxh->px_recordsize);
		/* addDataSize is the number of bytes in this data block. It must
		 * be less then
		 * 'pxh->px_maxtablesize*0x400-sizeof(TDataBlock)-pxh->px_recordsize'
		 * and a multiple of pxh->recordsize. If this is not the case
		 * (especially if addDataSize is to big, then this data block
		 * does not contain any valid records. Actually you could read
		 * them, because the data is still there, but considered to be
		 * deleted.
		 */
		if ((datasize+pxh->px_recordsize) > (pxh->px_maxtablesize*0x400-sizeof(TDataBlock))) {
//			printf("Size of data block %d as set in its header is to large: %d (%3.2f records)\n", get_short_le(&datablock.prevBlock), datasize, (float) datasize/pxh->px_recordsize + 1);
			/* Set the number of the next block */
			blocknumber = get_short_le((char *) &datablock.nextBlock);
		} else {
			if(recno*pxh->px_recordsize <= datasize) {
				found = 1;
				/* if we are within the range of valid data in the block,
				 * then set the deleted flag to 0
				 */
				if(recno*pxh->px_recordsize <= blocksize) {
					*deleted = 0;
				}
				if(pxdbinfo != NULL) {
					pxdbinfo->prev = get_short_le((char *) &datablock.prevBlock);
					pxdbinfo->next = get_short_le((char *) &datablock.nextBlock);
					pxdbinfo->number = blocknumber;
					pxdbinfo->size = datasize+pxh->px_recordsize;
					pxdbinfo->recno = recno;
					pxdbinfo->numrecords = pxdbinfo->size/pxh->px_recordsize;
					pxdbinfo->blockpos = pxdoc->tell(pxdoc, pxdoc->px_stream)-sizeof(TDataBlock);
					pxdbinfo->recordpos = pxdbinfo->blockpos + sizeof(TDataBlock) + recno*pxh->px_recordsize;
				}
			} else { /* skip rest of block */
				blocknumber = get_short_le((char *) &datablock.nextBlock);
			}
			recno -= (datasize/pxh->px_recordsize+1);
		}
		blockcount++;
	}
	return(found);
}
/* }}} */

/* PX_get_record() {{{
 * Reads one record from a Paradox file. This function can be used
 * for different types of Paradox files. This function will not
 * return any information about the datablock in which the record
 * was stored.
 */
PXLIB_API char* PXLIB_CALL
PX_get_record(pxdoc_t *pxdoc, int recno, char *data) {
	int d = 0;
	return(PX_get_record2(pxdoc, recno, data, &d, NULL));
}
/* }}} */

/* PX_get_record2() {{{
 * Reads one record from a Paradox file. This function can be used
 * for different types of Paradox files. The function will return
 * information about the datablock where the record is stored.
 * It also reports if a record is deleted. Read the man page of
 * this function to get an explanation on what it means if a record
 * is deleted.
 */
PXLIB_API char* PXLIB_CALL
PX_get_record2(pxdoc_t *pxdoc, int recno, char *data, int *deleted, pxdatablockinfo_t *pxdbinfo) {
	int ret, found, blockcount;
	pxhead_t *pxh;
	pxdatablockinfo_t tmppxdbinfo;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return NULL;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("File has no header"));
		return NULL;
	}
	pxh = pxdoc->px_head;

	/* Allow to read records up to the theoretical number of records
	 * in the file or the actual number of records depending on 'deleted'.
	 * If a primary index exists do not care about 'deleted' and read
	 * in any case only up to the actual number of records.
	 */
	if((recno < 0) ||
	   (*deleted && (recno >= pxh->px_theonumrecords)) ||
	   (pxdoc->px_pindex && (recno >= pxh->px_numrecords)) ||
	   (!*deleted && (recno >= pxh->px_numrecords))) {
		px_error(pxdoc, PX_RuntimeError, _("Record number out of range"));
		return NULL;
	}

	if(pxdoc->px_indexdata)
		found = px_get_record_pos_with_index(pxdoc, recno, deleted, &tmppxdbinfo);
	else
		found = px_get_record_pos(pxdoc, recno, deleted, &tmppxdbinfo);

	if(found) {
		if(pxdbinfo) {
			memcpy(pxdbinfo, &tmppxdbinfo, sizeof(pxdatablockinfo_t));
		}

		if((ret = pxdoc->seek(pxdoc, pxdoc->px_stream, tmppxdbinfo.recordpos, SEEK_SET)) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of record data."));
			return NULL;
		}
		if((ret = pxdoc->read(pxdoc, pxdoc->px_stream, pxh->px_recordsize, data)) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not read data of record."));
			return NULL;
		}
		return data;
	} else
		return NULL;
}
/* }}} */

/* PX_put_record() {{{
 * Store a record into the paradox file.
 * Returns the record number starting at 0.
 */
PXLIB_API int PXLIB_CALL
PX_put_record(pxdoc_t *pxdoc, char *data) {
	pxhead_t *pxh;
	int recsperdatablock, datablocknr, recdatablocknr;
	int itmp;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("File has no header"));
		return -1;
	}
	pxh = pxdoc->px_head;

	/* All the following calculation assume sequentially writting of
	 * records and filling a datablock first before starting a new one.
	 * This should be fixed. Better would be, if we keep record
	 * in px_data (like a primary index) which datablocks are available
	 * an how much space they still have.
	 */
	/* Check if record still fits into a existing data block */
	recsperdatablock = (pxh->px_maxtablesize*0x400-sizeof(TDataBlock)) / pxh->px_recordsize;
	/* Calculate the number of the data block for this record.
	 * Datablock numbers start at 1. */
	datablocknr = (pxh->px_numrecords / recsperdatablock) + 1;
	/* Calculate the position within the datablock */
	recdatablocknr = (pxh->px_numrecords) % recsperdatablock;

//	fprintf(stderr, "Data goes into block %d at record no %d (%d)\n", datablocknr, recdatablocknr, recsperdatablock);
	/* Check if we need a new datablock */
	if(datablocknr > pxh->px_fileblocks) {
//		fprintf(stderr, "We need an new datablock\n");
		itmp = put_px_datablock(pxdoc, pxh, pxh->px_lastblock, pxdoc->px_stream);
//		fprintf(stderr, "Added data block no. %d\n", itmp);
	
		/* The datablock number return by px_put_datablock() should be
		 * the same as the calculated datablocknr.
		 */
		if(datablocknr != itmp) {
			px_error(pxdoc, PX_RuntimeError, _("Inconsistency in writing data block. Expected data block nr. %d, but got %d."), datablocknr, itmp);
			return -1;
		}
	}

	/* write data */
	itmp = px_add_data_to_block(pxdoc, pxh, datablocknr, data, pxdoc->px_stream);

	/* The record number within the data block must be the same
	 * as the calculated one.
	 */
	if(recdatablocknr != itmp) {
		px_error(pxdoc, PX_RuntimeError, _("Inconsistency in writing record into data block. Expected record nr. %d, but got %d."), recdatablocknr, itmp);
		return -1;
	}
	
	/* Update header */
	pxh->px_numrecords++;

	put_px_head(pxdoc, pxh, pxdoc->px_stream);
	return(pxh->px_numrecords-1);
}
/* }}} */

/* PX_close() {{{
 * Close a Paradox file, but only if it was opened with PX_open_filename().
 * This function will not free any memory.
 */
PXLIB_API void PXLIB_CALL
PX_close(pxdoc_t *pxdoc) {
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return;
	}

	if((pxdoc->px_stream->close) && (pxdoc->px_stream->s.fp != NULL))
		fclose(pxdoc->px_stream->s.fp);
	pxdoc->px_stream->s.fp = NULL;
}
/* }}} */

/* PX_delete() {{{
 * Frees all memory use by the Paradox file. If PX_close() had not
 * been called before, it will be now.
 */
PXLIB_API void PXLIB_CALL
PX_delete(pxdoc_t *pxdoc) {
	pxfield_t *pfield;
	int i;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return;
	}

	/* Make sure the files are closed. If they were already closed
	 * it is not problem to call the functions again.
	 */
	PX_close(pxdoc);

#if PX_USE_RECODE
	if(pxdoc->recode_outer)
		recode_delete_outer(pxdoc->recode_outer);

	if(pxdoc->out_recode_request)
		recode_delete_request(pxdoc->out_recode_request);
	if(pxdoc->in_recode_request)
		recode_delete_request(pxdoc->in_recode_request);
#else
#if PX_USE_ICONV
	if(pxdoc->out_iconvcd > 0)
		iconv_close(pxdoc->out_iconvcd);
	if(pxdoc->in_iconvcd > 0)
		iconv_close(pxdoc->in_iconvcd);
#endif
#endif

	if(pxdoc->targetencoding)
		pxdoc->free(pxdoc, pxdoc->targetencoding);
	if(pxdoc->inputencoding)
		pxdoc->free(pxdoc, pxdoc->inputencoding);

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
	if(pxdoc->px_data) {
		/* Free the data of the file. In case of an primary index file
		 * this is the index data
		 * FIXME: need to free the memory pointed to by px_data->data
		 */
		pxdoc->free(pxdoc, pxdoc->px_data);
		pxdoc->px_datalen = 0;
	}
	/* px_indexdata will be set if the index was read from an index file
	 * or build during PX_open_fp(). In the first case it is just a
	 * pointer to pxdoc->px_index->px_data and should not be freed
	 * because it is freed when the index file is deleted.
	 */
	if(pxdoc->px_indexdata && !pxdoc->px_pindex) {
		pxdoc->free(pxdoc, pxdoc->px_indexdata);
		pxdoc->px_indexdatalen = 0;
	}

	pxdoc->free(pxdoc, pxdoc);
}
/* }}} */

/* PX_get_fields() {{{
 * Returns a pointer onto the first field specification. This is identical
 * to calling PX_get_field() with a recno of 0.
 */
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
/* }}} */

/* PX_get_field() {{{
 * Returns a pointer onto a field (column) specification. The first
 * column/field has index 0.
 */
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
/* }}} */

/* PX_get_num_fields() {{{
 * Returns the number of fields/columns.
 */
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
/* }}} */

/* PX_get_num_records() {{{
 * Returns the number of records in a Paradox file.
 */
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
/* }}} */

/* PX_set_targetencoding() {{{
 * Sets the encoding of the output data. This is one of
 * the encodings supported by iconv or recode.
 */
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

	if(pxdoc->targetencoding) {
		px_error(pxdoc, PX_RuntimeError, _("Target encoding already set"));
		return -1;
	}

	pxdoc->targetencoding = px_strdup(pxdoc, encoding);
	if(pxdoc->targetencoding) {
#if PX_USE_RECODE
		sprintf(buffer, "CP%d/CR-LF..%s", pxdoc->px_head->px_doscodepage, pxdoc->targetencoding);
		recode_scan_request(pxdoc->out_recode_request, buffer);
#else
#if PX_USE_ICONV
		sprintf(buffer, "CP%d", pxdoc->px_head->px_doscodepage);
		if(pxdoc->out_iconvcd > 0)
			iconv_close(pxdoc->out_iconvcd);
		if((iconv_t)(-1) == (pxdoc->out_iconvcd = iconv_open(pxdoc->targetencoding, buffer))) {
			pxdoc->free(pxdoc, pxdoc->targetencoding);
			pxdoc->targetencoding = NULL;
			px_error(pxdoc, PX_RuntimeError, _("Target encoding could not be set."));
			return -1;
		}
	
#endif
#endif
	}
#else
	px_error(pxdoc, PX_RuntimeError, _("Library has not been compiled with support for reencoding."));
#endif
	return 0;
}
/* }}} */

/* PX_set_inputencoding() {{{
 * Sets the encoding of the input data. This is one of
 * the encodings supported by iconv or recode. The input encoding
 * must be set before the target encoding to have effect. If the input
 * encoding is not set it will be taken from the paradox header.
 */
PXLIB_API int PXLIB_CALL
PX_set_inputencoding(pxdoc_t *pxdoc, char *encoding) {
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

	if(pxdoc->inputencoding) {
		px_error(pxdoc, PX_RuntimeError, _("Input encoding already set"));
		return -1;
	}

	pxdoc->inputencoding = px_strdup(pxdoc, encoding);

	if(pxdoc->inputencoding) {
#if PX_USE_RECODE
		sprintf(buffer, "%s..CP%d/CR-LF", pxdoc->inputencoding, pxdoc->px_head->px_doscodepage);
		recode_scan_request(pxdoc->in_recode_request, buffer);
#else
#if PX_USE_ICONV
		sprintf(buffer, "CP%d", pxdoc->px_head->px_doscodepage);
		if(pxdoc->in_iconvcd > 0)
			iconv_close(pxdoc->in_iconvcd);
		if((iconv_t)(-1) == (pxdoc->in_iconvcd = iconv_open(buffer, pxdoc->inputencoding))) {
			pxdoc->free(pxdoc, pxdoc->inputencoding);
			pxdoc->inputencoding = NULL;
			px_error(pxdoc, PX_RuntimeError, _("Input encoding could not be set."));
			return -1;
		}
#endif
#endif
	}
#else
	px_error(pxdoc, PX_RuntimeError, _("Library has not been compiled with support for reencoding."));
#endif
	return 0;
}
/* }}} */

/* PX_set_tablename() {{{
 * Sets the name of the table as stored in database file.
 */
PXLIB_API int PXLIB_CALL
PX_set_tablename(pxdoc_t *pxdoc, char *tablename) {
	char buffer[30];

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database"));
		return -1;
	}

	if(pxdoc->px_head == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Header of file has not been read"));
		return -1;
	}

	if(pxdoc->px_head->px_tablename)
		px_free(pxdoc, pxdoc->px_head->px_tablename);

	pxdoc->px_head->px_tablename = px_strdup(pxdoc, tablename);
	if(put_px_head(pxdoc, pxdoc->px_head, pxdoc->px_stream) < 0) {
		return -1;
	}
	return 0;
}
/* }}} */

/******* Function to access Blob files *******/

/* PX_new_blob() {{{
 * Create a new blob document
 */
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
/* }}} */

/* PX_open_blob_fp() {{{
 * Opens a blob with a given already open file pointer
 */
PXLIB_API int PXLIB_CALL
PX_open_blob_fp(pxblob_t *pxblob, FILE *fp) {

	pxblob->px_fp = fp;
}
/* }}} */

/* PX_open_blob_file() {{{
 * Opens a file of a blob with the given filename
 */
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
	pxblob->px_close_fp = px_true;
	return 0;
}
/* }}} */

/* PX_close_blob() {{{
 * Close a blob file
 */
PXLIB_API void PXLIB_CALL
PX_close_blob(pxblob_t *pxblob) {
	if((pxblob->px_close_fp) && (pxblob->px_fp != 0)) {
		fclose(pxblob->px_fp);
		pxblob->px_fp = NULL;
		pxblob->pxdoc = NULL;
	}
}
/* }}} */

/* PX_read_blobdata() {{{
 * Reads data of blob into memory and returns a pointer to it
 */
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
/* }}} */

/******* Function to access record data ******/

/* PX_get_data_alpha() {{{
 * Extracts an alpha field value from a data block
 */
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
		res = recode_buffer_to_buffer(pxdoc->out_recode_request, data, len, &obuf, &olen, &oallocated);
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
		if(0 > (res = iconv(pxdoc->out_iconvcd, &iptr, &ilen, &optr, &olen))) {
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
/* }}} */

/* PX_get_data_bytes() {{{
 * Extracts a bytes field value from a data block
 */
PXLIB_API int PXLIB_CALL
PX_get_data_bytes(pxdoc_t *pxdoc, char *data, int len, char **value) {
	char *buffer, *obuf = NULL;
	size_t olen;
	int res;

	if(data[0] == '\0') {
		*value = NULL;
		return 0;
	}

	memcpy(*value, data, len);

	return 1;
}
/* }}} */

/* PX_get_data_double() {{{
 * Extracts a double from a data block
 */
PXLIB_API int PXLIB_CALL
PX_get_data_double(pxdoc_t *pxdoc, char *data, int len, double *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
	} else if(*((long long int *)data) != 0) {
		int k;
		for(k=0; k<len; k++)
			data[k] = ~data[k];
	} else {
		*value = 0;
		return 0;
	}
	*value = get_double_be(data); //*((double *)data);
	return 1;
}
/* }}} */

/* PX_get_data_long() {{{
 * Extracts a long integer from a data block
 */
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
/* }}} */

/* PX_get_data_short() {{{
 * Extracts a short integer in a data block
 */
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
/* }}} */

/* PX_get_data_byte() {{{
 * Extracts a byte in a data block
 */
PXLIB_API int PXLIB_CALL
PX_get_data_byte(pxdoc_t *pxdoc, char *data, int len, char *value) {
	if(data[0] & 0x80) {
		data[0] &= 0x7f;
	} else if(*data != 0) {
		data[0] |= 0x80;
	} else {
		return 0;
	}
	*value = *data;
	return 1;
}
/* }}} */

/* PX_put_data_alpha() {{{
 * Stores a string in a data block.
 */
PXLIB_API void PXLIB_CALL
PX_put_data_alpha(pxdoc_t *pxdoc, char *data, int len, char *value) {
	char *obuf = NULL;
	size_t olen;
	int res;

	memset(data, 0, len);
	if((value == NULL) || (value[0] == '\0')) {
		return;
	}

	if(pxdoc->targetencoding != NULL) {
#if PX_USE_RECODE
		int oallocated = 0;
		res = recode_buffer_to_buffer(pxdoc->in_recode_request, value, strlen(value), &obuf, &olen, &oallocated);
#else
#if PX_USE_ICONV
		size_t ilen = strlen(value);
		char *iptr, *optr;
		olen = len + 1;
		/* Do not pxdoc->malloc because the memory is freed with free
		 * We use free because the memory allocated by recode_buffer_to_buffer()
		 * is requested with malloc and must be freed with free.
		 */
		optr = obuf = (char *) malloc(olen);
		iptr = value;
//		printf("value(%d) = '%s'\n", ilen, value);
//		printf("obuf(%d) = '%s'\n", olen, obuf);
		if(0 > (res = iconv(pxdoc->in_iconvcd, &iptr, &ilen, &optr, &olen))) {
			memset(data, 0, len);
			free(obuf);
			return;
		}
//		printf("value(%d) = '%s'\n", ilen, value);
//		printf("obuf(%d) = '%s'\n", olen, obuf);
		olen = strlen(value);
#endif
#endif
	} else {
		olen = strlen(value);
		obuf = value;
	}

	memcpy(data, obuf, olen < len ? olen : len);

	if(pxdoc->targetencoding != NULL) {
		free(obuf);
	}

}
/* }}} */

/* PX_put_data_bytes() {{{
 * Stores raw bytes in a data block.
 */
PXLIB_API void PXLIB_CALL
PX_put_data_bytes(pxdoc_t *pxdoc, char *data, int len, char *value) {
	char *obuf = NULL;
	size_t olen;
	int res;

	memcpy(data, value, len);
}
/* }}} */

/* PX_put_data_double() {{{
 * Stores a double in a data block.
 * A len of 0 means to store a NULL value.
 */
PXLIB_API void PXLIB_CALL
PX_put_data_double(pxdoc_t *pxdoc, char *data, int len, double value) {
	if(len == 0) {
		memset(data, 0, 8);
	} else {
		put_double_be(data, value);
		if(value >= 0) {
			data[0] |= 0x80;
		} else {
			int k;
			for(k=0; k<len; k++)
				data[k] = ~data[k];
		}
	}
}
/* }}} */

/* PX_put_data_long() {{{
 * Stores an integer in a data block.
 * A len of 0 means to store a NULL value.
 */
PXLIB_API void PXLIB_CALL
PX_put_data_long(pxdoc_t *pxdoc, char *data, int len, int value) {
	if(len == 0) {
		memset(data, 0, 4);
	} else {
		put_long_be(data, value);
		if(value >= 0) {
			data[0] |= 0x80;
		} else {
			data[0] &= 0x7f;
		}
	}
}
/* }}} */

/* PX_put_data_short() {{{
 * Stores a short integer in a data block.
 * A len of 0 means to store a NULL value.
 */
PXLIB_API void PXLIB_CALL
PX_put_data_short(pxdoc_t *pxdoc, char *data, int len, short int value) {
	if(len == 0) {
		memset(data, 0, 2);
	} else {
		put_short_be(data, value);
		if(value >= 0) {
			data[0] |= 0x80;
		} else {
			data[0] &= 0x7f;
		}
	}
}
/* }}} */

/* PX_put_data_byte() {{{
 * Stores a byte in a data block.
 * A len of 0 means to store a NULL value.
 */
PXLIB_API void PXLIB_CALL
PX_put_data_byte(pxdoc_t *pxdoc, char *data, int len, char value) {
	if(len == 0) {
		memset(data, 0, 1);
	} else {
		*data = value;
		if(value >= 0) {
			data[0] |= 0x80;
		} else {
			data[0] &= 0x7f;
		}
	}
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
