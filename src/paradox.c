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

/* PX_new2() {{{
 * Create a new Paradox DB file and set memory management and error
 * handling functions.
 */
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

	pxdoc->px_pindex = NULL;

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
/* }}} */

/* PX_new() {{{
 * Create new Paradox DB file.
 * Use the default memory management and error handling functions.
 */
PXLIB_API pxdoc_t* PXLIB_CALL
PX_new(void) {
	return(PX_new2(NULL, NULL, NULL, NULL));
}
/* }}} */

/* PX_open_fp() {{{
 * Read from a Paradox DB file, which has already been opend with fopen.
 */
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
/* }}} */

/* PX_open_filename() {{{
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
	pxdoc->px_close_fp = px_true;
	return 0;
}
/* }}} */

/* PX_create_db() {{{
 * Create a new paradox database.
 */
PXLIB_API int PXLIB_CALL
PX_create_db(pxdoc_t *pxdoc, pxfield_t *fields, int numfields, char *filename) {
	FILE *fp;
	pxhead_t *pxh;
	pxfield_t *pxf;
	int i, recordsize = 0;

	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database."));
		return -1;
	}

	if((fp = fopen(filename, "w+")) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Could not open file of paradox database."));
		return -1;
	}

	if((pxh = (pxhead_t *) pxdoc->malloc(pxdoc, sizeof(pxhead_t), _("Couldn't get memory for document header."))) == NULL) {
		return -1;
	}
	pxh->px_tablename = px_strdup(pxdoc, filename);
	pxh->px_filetype = pxfFileTypIndexDB;
	pxh->px_fileversion = 70;
	pxh->px_numrecords = 0;
	pxh->px_numfields = numfields;
	pxh->px_fields = fields;
	pxh->px_writeprotected = 0;
	pxh->px_headersize = 0x0800;
	pxh->px_fileblocks = 0;
	pxh->px_maxtablesize = 16;
	pxh->px_doscodepage = 1251;
	pxh->px_primarykeyfields = 0;
	pxh->px_autoinc = 0;

	/* Calculate record size */
	pxf = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++, pxf++) {
		recordsize += pxf->px_flen;
	}
	pxh->px_recordsize = recordsize;
	if(recordsize < 30) {
		pxh->px_maxtablesize = 1;
	} else if(recordsize < 120) {
		pxh->px_maxtablesize = 2;
	}

	if(put_px_head(pxdoc, pxh, fp) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Unable to put header."));
		return -1;
	}

	pxdoc->px_head = pxh;
	pxdoc->px_name = px_strdup(pxdoc, filename);
	pxdoc->px_fp = fp;
	pxdoc->px_close_fp = px_true;
	return 0;
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

	return 0;
}
/* }}} */

/* PX_read_primary_index() {{{
 * Read the primary index completly into an internal array.
 */
PXLIB_API int PXLIB_CALL
PX_read_primary_index(pxdoc_t *pindex) {
	pxpindex_t *pindex_data;
	pxhead_t *pxh;
	pxfield_t *pxf;
	char *data;
	int j;

	if(pindex == NULL ||
	   pindex->px_head == NULL ||
	   pindex->px_head->px_filetype != pxfFileTypPrimIndex) {
		px_error(pindex, PX_RuntimeError, _("Did not pass a paradox primary index file"));
		return -1;
	}

	pxh = pindex->px_head;
	pindex->px_data = pindex->malloc(pindex, pxh->px_numrecords*sizeof(pxpindex_t), _("Couldn't get memory for primary index data."));
	if(!pindex->px_data) {
		px_error(pindex, PX_RuntimeError, _("Could not allocate memory for primary index data."));
		return -1;
	}

	pindex_data = (pxpindex_t *) pindex->px_data;
	if((data = (char *) pindex->malloc(pindex, pxh->px_recordsize, _("Could not allocate memory for record."))) == NULL) {
		px_error(pindex, PX_RuntimeError, _("Could not allocate memory for primary index data."));
		return -1;
	}

	for(j=0; j<pxh->px_numrecords; j++) {
		int offset, i;
		if(PX_get_record(pindex, j, data)) {
			short int value;
			offset = 0;
			/* Read over the field data.
			 * px_numfields does not count the fields with information about
			 * block position and num of records per block. */
			pxf = pxh->px_fields;
			for(i=0; i<pxh->px_numfields; i++) {
				offset += pxf->px_flen;
				pxf++;
			}
			PX_get_data_short(pindex, &data[offset], 2, &value);
			pindex_data[j].blocknumber = value;
			offset += 2;
			PX_get_data_short(pindex, &data[offset], 2, &value);
			pindex_data[j].numrecords = value;
			offset += 2;
			PX_get_data_short(pindex, &data[offset], 2, &value);
			pindex_data[j].dummy = value;
			offset += 2;
		} else {
			px_error(pindex, PX_RuntimeError, _("Could not read record no. %d of primary index data."), j);
			pindex->free(pindex, data);
			pindex->free(pindex, pindex->px_data);
			pindex->px_data = NULL;
			return -1;
		}
	}

	pindex->free(pindex, data);
	return 0;
}
/* }}} */

/* px_get_record_pos_with_index() {{{
 * Locates a database record by using the primary index.
 * Returns 1 if record could be found, otherwise 0
 */
int
px_get_record_pos_with_index(pxdoc_t *pxdoc, int recno, int *deleted, pxdatablockinfo_t *pxdbinfo) {
	int j, numrecords, n;
	pxdoc_t *pindexdoc;
	pxhead_t *pxh, *pxih;
	pxpindex_t *pindex_data;

	pxh = pxdoc->px_head;
	pindexdoc = pxdoc->px_pindex;
	pxih = pindexdoc->px_head;
	pindex_data = pindexdoc->px_data;

	if(!pindex_data)
		return 0;

	numrecords = 0 ;
	for(j=0; j<pxih->px_numrecords; j++) {
		/* Check if the number of records is in the possible range.
		 * I have seen prim. index files with very strange values.
		 */
		if(pindex_data[j].numrecords > pxh->px_maxtablesize*0x400/pxh->px_recordsize) {
			int ret;
			TDataBlock datablock;
//			printf("Records per block in index file is wrong (%d)\n", pindex_data[j].numrecords);
			/* Go to the start of the data block (skip the header) */
			if((ret = fseek(pxdoc->px_fp, pxh->px_headersize + (pindex_data[j].blocknumber-1)*pxh->px_maxtablesize*0x400, SEEK_SET)) < 0) {
				px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of first data block"));
				return 0;
			}

			/* Get the info about this data block */
			if((ret = fread(&datablock, sizeof(TDataBlock), 1, pxdoc->px_fp)) < 0) {
				px_error(pxdoc, PX_RuntimeError, _("Could not read data block header."));
				return 0;
			}
			n = get_short_le((char *) &datablock.addDataSize)/pxh->px_recordsize+1;
			if(n > pxh->px_maxtablesize*0x400/pxh->px_recordsize) {
				n = 0;
			}
			pindex_data[j].numrecords = n;
//			printf("Set number of records in block to %d (%d)\n", n, get_short_le((char *) &datablock.addDataSize));

		} else {
			n = pindex_data[j].numrecords;
		}
		numrecords += n;
		if(recno >= n) {
			recno -= n;
		} else {
			int blocksize, ret;
			TDataBlock datablock;

			pxdbinfo->realnumber = pindex_data[j].blocknumber-1;
			pxdbinfo->recno = recno;
			pxdbinfo->blockpos = pxh->px_headersize + pxdbinfo->realnumber*pxh->px_maxtablesize*0x400;
			pxdbinfo->recordpos = pxdbinfo->blockpos + sizeof(TDataBlock) + recno*pxh->px_recordsize;

			/* Go to the start of the data block (skip the header) */
			if((ret = fseek(pxdoc->px_fp, pxdbinfo->blockpos, SEEK_SET)) < 0) {
				px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of first data block"));
				return 0;
			}

			/* Get the info about this data block */
			if((ret = fread(&datablock, sizeof(TDataBlock), 1, pxdoc->px_fp)) < 0) {
				px_error(pxdoc, PX_RuntimeError, _("Could not read"));
				return 0;
			}

			blocksize = get_short_le((char *) &datablock.addDataSize);

			pxdbinfo->number = get_short_le((char *) &datablock.blockNumber);
			pxdbinfo->size = blocksize+pxh->px_recordsize;
			pxdbinfo->numrecords = pxdbinfo->size/pxh->px_recordsize;
			deleted = 0;
			return 1;
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
	int ret, found, blockcount;
	TDataBlock datablock;
	pxhead_t *pxh;

	pxh = pxdoc->px_head;

	/* Go to the start of the data block (skip the header) */
	if((ret = fseek(pxdoc->px_fp, pxh->px_headersize, SEEK_SET)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of first data block"));
		return 0;
	}

	found = 0;
	blockcount = 0;
	while(!found && (blockcount < pxh->px_fileblocks)) {
		int datasize, blocksize;
		/* Get the info about this data block */
		if((ret = fread(&datablock, sizeof(TDataBlock), 1, pxdoc->px_fp)) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not read header of data block"));
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
		if ((datasize+pxh->px_recordsize) > (pxh->px_maxtablesize*0x400-6)) {
//			printf("Size of data block %d as set in its header is to large: %d (%3.2f records)\n", get_short_le(&datablock.blockNumber), datasize, (float) datasize/pxh->px_recordsize + 1);
			if((ret = fseek(pxdoc->px_fp, pxh->px_maxtablesize*0x400-sizeof(TDataBlock), SEEK_CUR)) < 0) {
				px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of next data block."));
				return 0;
			}
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
					pxdbinfo->number = get_short_le((char *) &datablock.blockNumber);
					pxdbinfo->realnumber = blockcount;
					pxdbinfo->size = datasize+pxh->px_recordsize;
					pxdbinfo->recno = recno;
					pxdbinfo->numrecords = pxdbinfo->size/pxh->px_recordsize;
					pxdbinfo->blockpos = ftell(pxdoc->px_fp)-sizeof(TDataBlock);
					pxdbinfo->recordpos = pxdbinfo->blockpos + sizeof(TDataBlock) + recno*pxh->px_recordsize;
				}
			} else { /* skip rest of block */
	//			printf("skippin rest of block %d\n", blockcount);
				if((ret = fseek(pxdoc->px_fp, pxh->px_maxtablesize*0x400-sizeof(TDataBlock), SEEK_CUR)) < 0) {
					px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of next data block."));
					return 0;
				}
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

	if(pxdoc->px_pindex)
		found = px_get_record_pos_with_index(pxdoc, recno, deleted, &tmppxdbinfo);
	else
		found = px_get_record_pos(pxdoc, recno, deleted, &tmppxdbinfo);

	if(found) {
		if(pxdbinfo) {
			memcpy(pxdbinfo, &tmppxdbinfo, sizeof(pxdatablockinfo_t));
		}

		if((ret = fseek(pxdoc->px_fp, tmppxdbinfo.recordpos, SEEK_SET)) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of record data."));
			return NULL;
		}
		if((ret = fread(data, pxh->px_recordsize, 1, pxdoc->px_fp)) < 0) {
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

	/* Check if record still fits into a existing data block */
	recsperdatablock = (pxh->px_maxtablesize*0x400-sizeof(TDataBlock)) / pxh->px_recordsize;
	/* Calculate the number of the data block for this record */
	datablocknr = (pxh->px_numrecords+1) / recsperdatablock;
	/* Calculate the position within the datablock */
	recdatablocknr = pxh->px_numrecords % recsperdatablock;

	fprintf(stderr, "Data goes into block %d at record no %d (%d)\n", datablocknr, recdatablocknr, recsperdatablock);
	/* Check if we need a new datablock */
	if(datablocknr >= pxh->px_fileblocks) {
		fprintf(stderr, "We need an new datablock\n");
		itmp = put_px_datablock(pxdoc, pxh, pxdoc->px_fp);
		fprintf(stderr, "Added data block no. %d\n", itmp);
	
		/* The datablock number return by px_put_datablock() should be
		 * the same as the calculated datablocknr.
		 */
		if(datablocknr != itmp) {
			px_error(pxdoc, PX_RuntimeError, _("Inconsistency in writing data block."));
			return -1;
		}

		pxh->px_fileblocks++;
	}

	/* write data */
	itmp = px_add_data_to_block(pxdoc, pxh, datablocknr, data, pxdoc->px_fp);

	/* The record number within the data block must be the same
	 * as the calculated one.
	 */
	if(recdatablocknr != itmp) {
		px_error(pxdoc, PX_RuntimeError, _("Inconsistency in writing record into data block."));
//		return -1;
	}
	
	/* Update header */
	pxh->px_numrecords++;

	put_px_head(pxdoc, pxh, pxdoc->px_fp);
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

	if((pxdoc->px_close_fp) && (pxdoc->px_fp != NULL))
		fclose(pxdoc->px_fp);
	pxdoc->px_fp = NULL;
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
		res = recode_buffer_to_buffer(pxdoc->recode_request, value, strlen(value), &obuf, &olen, &oallocated);
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
		if(0 > (res = iconv(pxdoc->iconvcd, &iptr, &ilen, &optr, &olen))) {
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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
