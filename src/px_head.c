#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <paradox.h>
#include <px_intern.h>
#include <px_memory.h>
#include <px_io.h>

/* TMPBUFFSIZE must be larger than 261 because the tablename must fit into
 * the buffer. It is also the maximum length of a field name. Field names
 * which are longer get cut off.
 */
#define TMPBUFFSIZE 300
/* get_px_head() {{{
 * get the header info from the file
 * basic header info & field descriptions
 */
pxhead_t *get_px_head(pxdoc_t *pxdoc, pxstream_t *pxs)
{
	pxhead_t *pxh;
	TPxHeader pxhead;
	TPxDataHeader pxdatahead;
	TFldInfoRec pxinfo;
	pxfield_t *pfield;
	char dummy[TMPBUFFSIZE], c;
	int ret, i, j, tablenamelen;

	if((pxh = (pxhead_t *) pxdoc->malloc(pxdoc, sizeof(pxhead_t), _("Allocate memory for document header."))) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Could not allocate memory for document header."));
		return NULL;
	}
	if(pxdoc->seek(pxdoc, pxs, 0, SEEK_SET) < 0)
		return NULL;
	if((ret = pxdoc->read(pxdoc, pxs, sizeof(TPxHeader), &pxhead)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read header from paradox file."));
		pxdoc->free(pxdoc, pxh);
		return NULL;
	}

	/* check some header fields for reasonable values */
	if(pxhead.fileType > 8) {
		pxdoc->free(pxdoc, pxh);
		px_error(pxdoc, PX_RuntimeError, _("Paradox file has unknown file type (%d)."), pxhead.fileType);
		return NULL;
	}
	if(pxhead.maxTableSize > 32 || pxhead.maxTableSize < 1) {
		pxdoc->free(pxdoc, pxh);
		px_error(pxdoc, PX_RuntimeError, _("Paradox file has unknown table size (%d)."), pxhead.maxTableSize);
		return NULL;
	}
	if(pxhead.fileVersionID > 15 || pxhead.fileVersionID < 3) {
		pxdoc->free(pxdoc, pxh);
		px_error(pxdoc, PX_RuntimeError, _("Paradox file has unknown file version (0x%X)."), pxhead.fileVersionID);
		return NULL;
	}

	pxh->px_recordsize = get_short_le(&pxhead.recordSize);
	if(pxh->px_recordsize == 0) {
		pxdoc->free(pxdoc, pxh);
		px_error(pxdoc, PX_RuntimeError, _("Paradox file has zero record size."));
		return NULL;
	}
	pxh->px_headersize = get_short_le(&pxhead.headerSize);
	if(pxh->px_headersize == 0) {
		pxdoc->free(pxdoc, pxh);
		px_error(pxdoc, PX_RuntimeError, _("Paradox file has zero header size."));
		return NULL;
	}
	pxh->px_filetype = pxhead.fileType;
	pxh->px_numrecords = get_long_le(&pxhead.numRecords);
	pxh->px_numfields = get_short_le(&pxhead.numFields);
	pxh->px_fileblocks = get_short_le(&pxhead.fileBlocks);
	pxh->px_firstblock = get_short_le(&pxhead.firstBlock);
	pxh->px_lastblock = get_short_le(&pxhead.lastBlock);
	switch(pxhead.fileVersionID) {
		case 3:
			pxh->px_fileversion = 30;
			tablenamelen = 79;
			break;
		case 4:
			pxh->px_fileversion = 35;
			tablenamelen = 79;
			break;
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
			pxh->px_fileversion = 40;
			tablenamelen = 79;
			break;
		case 10:
		case 11:
			pxh->px_fileversion = 50;
			tablenamelen = 79;
			break;
		case 12:
			pxh->px_fileversion = 70;
			tablenamelen = 261;
			break;
		default:
			pxh->px_fileversion = 0;
			tablenamelen = 79;
	}
	pxh->px_indexfieldnumber = pxhead.indexFieldNumber;
	pxh->px_indexroot = get_short_le(&pxhead.indexRoot);
	pxh->px_numindexlevels = pxhead.numIndexLevels;
	pxh->px_writeprotected = pxhead.writeProtected;
	pxh->px_modifiedflags1 = pxhead.modifiedFlags1;
	pxh->px_modifiedflags2 = pxhead.modifiedFlags2;
	pxh->px_primarykeyfields = get_short_le(&pxhead.primaryKeyFields);

	if(((pxh->px_filetype == pxfFileTypIndexDB) ||
		  (pxh->px_filetype == pxfFileTypNonIndexDB) ||
		  (pxh->px_filetype == pxfFileTypNonIncSecIndex) ||
		  (pxh->px_filetype == pxfFileTypIncSecIndex)) &&
		  (pxh->px_fileversion >= 40)) {
		if((ret = pxdoc->read(pxdoc, pxs, sizeof(TPxDataHeader), &pxdatahead)) < 0) {
			pxdoc->free(pxdoc, pxh);
			return NULL;
		}
		pxh->px_doscodepage = get_short_le(&pxdatahead.dosCodePage);
	}
	pxh->px_fileupdatetime = get_long_le(&pxdatahead.fileUpdateTime);

	pxh->px_maxtablesize = pxhead.maxTableSize;
	pxh->px_sortorder = pxhead.sortOrder;
	pxh->px_refintegrity = pxhead.refIntegrity;
	pxh->px_autoinc = get_long_le(&pxhead.autoInc);

	/* The theoretical number of records is calculated from the number
	 * of data blocks and the number of records that fit into a data
	 * block. The 'TDataBlock' is decreasing the available space of the data
	 * block due to its header, which takes up 6 Bytes.
	 */
	pxh->px_theonumrecords = pxh->px_fileblocks * (int) ((pxh->px_maxtablesize*0x400-sizeof(TDataBlock)) / pxh->px_recordsize);

	if((pxh->px_fields = (pxfield_t *) pxdoc->malloc(pxdoc, pxh->px_numfields*sizeof(pxfield_t), _("Could not get memory for field definitions."))) == NULL)
		return NULL;

	pfield = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++) {
		if((ret = pxdoc->read(pxdoc, pxs, sizeof(TFldInfoRec), &pxinfo)) < 0) {
			pxdoc->free(pxdoc, pxh->px_fields);
			pxdoc->free(pxdoc, pxh);
			return NULL;
		}
		pfield->px_ftype = pxinfo.fType;
		if(pfield->px_ftype == pxfBCD) {
			pfield->px_flen = 17;
			pfield->px_fdc = pxinfo.fSize;
		} else {
			pfield->px_flen = pxinfo.fSize;
			pfield->px_fdc = 0;
		}
		pfield++;
	}

	/* skip the tableNamePtr */
	if((ret = pxdoc->read(pxdoc, pxs, sizeof(int), dummy)) < 0) {
		pxdoc->free(pxdoc, pxh->px_fields);
		pxdoc->free(pxdoc, pxh);
		return NULL;
	}

	/* skip the tfieldNamePtrArray, not present in index files */
	if(pxhead.fileType == 0 || pxhead.fileType == 2) {
		for(i=0; i<pxh->px_numfields; i++) {
			if((ret = pxdoc->read(pxdoc, pxs, sizeof(int), dummy)) < 0) {
				pxdoc->free(pxdoc, pxh->px_fields);
				pxdoc->free(pxdoc, pxh);
				return NULL;
			}
		}
	}

	/* read the tableName */
	ret = pxdoc->read(pxdoc, pxs, tablenamelen, dummy);
	if(ret < 0) {
		pxdoc->free(pxdoc, pxh->px_fields);
		pxdoc->free(pxdoc, pxh);
		return NULL;
	}
	pxh->px_tablename = px_strdup(pxdoc, dummy);

	/* FIXME: The following will cut off field names longer than
	 * TMPBUFFSIZE-1 chars */
	pfield = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++) {
		j=0;
		while((j < TMPBUFFSIZE-1) && ((ret = pxdoc->read(pxdoc, pxs, 1, &c)) >= 0) && (c != '\0')) {
			dummy[j++] = c;
		}
		if(ret < 0) {
			pxdoc->free(pxdoc, pxh->px_tablename);
			pxdoc->free(pxdoc, pxh->px_fields);
			pxdoc->free(pxdoc, pxh);
			return NULL;
		}
		dummy[j] = '\0';
//		PX_get_data_alpha(pxdoc, dummy, strlen(dummy), &pfield->px_fname);
		pfield->px_fname = px_strdup(pxdoc, (const char *)dummy);
		pfield++;
	}

	return pxh;
}
/* }}} */
#undef TMPBUFFSIZE

/* put_px_head() {{{
 * writes the header and field information into a new file.
 */
int put_px_head(pxdoc_t *pxdoc, pxhead_t *pxh, pxstream_t *pxs) {
	TPxHeader pxhead;
	TPxDataHeader pxdatahead;
	TFldInfoRec pxinfo;
	pxfield_t *pxf;
	char *ptr;
	int nullint = 0;
	int i, len;
	int sumfieldlen;   /* sum of all field name length include the 0 */
	char *basehead;
	int base, offset, dataheadoffset;
	short int tmp;
	int isindex;
	int tablenamelen;
	long dummy;

	memset(&pxhead, 0, sizeof(pxhead));
	memset(&pxdatahead, 0, sizeof(pxdatahead));

	basehead = (char *) &pxhead;
	isindex = !((pxh->px_filetype == pxfFileTypIndexDB) ||
		        (pxh->px_filetype == pxfFileTypNonIndexDB) ||
		        (pxh->px_filetype == pxfFileTypNonIncSecIndex) ||
		        (pxh->px_filetype == pxfFileTypIncSecIndex));

	put_short_le(&pxhead.recordSize, pxh->px_recordsize);
	put_short_le(&pxhead.headerSize, pxh->px_headersize);
	put_short_le(&pxhead.fileBlocks, pxh->px_fileblocks);
	/* nextBlock is the number of the next block to use. It is always
	 * identical to fileBlocks unless, there are empty blocks in the
	 * file, which will not happen. */
	put_short_le(&pxhead.nextBlock, pxh->px_fileblocks);
	/* firstBlock should be zero unless there is at least one data
	 * block in the file. */
	if(pxh->px_fileblocks > 0)
		put_short_le(&pxhead.firstBlock, 1);
	else
		put_short_le(&pxhead.firstBlock, 0);
	/* The last block is similar to nextBlock. If all blocks are filled
	 * this is identical to fileBlocks. */
	put_short_le(&pxhead.lastBlock, pxh->px_fileblocks);
	put_short_le(&pxhead.maxBlocks, pxh->px_fileblocks);
	pxhead.fileType = pxh->px_filetype;
	put_long_le(&pxhead.autoInc, pxh->px_autoinc);
	pxhead.maxTableSize = pxh->px_maxtablesize;
	put_long_le(&pxhead.numRecords, pxh->px_numrecords);
	pxhead.writeProtected = pxh->px_writeprotected;
	put_short_le(&pxhead.numFields, pxh->px_numfields);
	pxhead.indexFieldNumber = pxh->px_indexfieldnumber;
	put_short_le(&pxhead.indexRoot, pxh->px_indexroot);
	pxhead.numIndexLevels = pxh->px_numindexlevels;
	switch(pxh->px_filetype) {
		case pxfFileTypIndexDB:
			/* The field unknown12x13 is probably the number of changes.
			 * In several files it was just 12 in .DB and 17 in .PX files. */
			put_short_le(&pxhead.unknown12x13, 12);
			put_short_le(&pxhead.primaryKeyFields, pxh->px_primarykeyfields);
			put_long_le(&pxhead.primaryIndexWorkspace, basehead-100);  /* just to set a value */
			put_long_le(&pxhead.unknownPtr1A, basehead-500);  /* just to set a value */
			break;
		case pxfFileTypPrimIndex:
			put_short_le(&pxhead.unknown12x13, 17);
			pxhead.unknown2Bx2C[1] = 102;
			break;
		case pxfFileTypNonIncSecIndex:
		case pxfFileTypIncSecIndex:
		case pxfFileTypNonIncSecIndexG:
		case pxfFileTypIncSecIndexG:
			put_short_le(&pxhead.primaryKeyFields, 2);
			break;
	}
	switch(pxh->px_filetype) {
		case pxfFileTypIndexDB:
		case pxfFileTypNonIndexDB:
			put_long_le(&pxhead.encryption1, 0xFF00FF00);
			pxhead.unknown3Ex3F[0] = 0x1f; // this seems to be a fixed value
			pxhead.unknown3Ex3F[1] = 0x0f; // this seems to be a fixed value
			pxhead.unknown56x57[0] = 0x20;
			pxhead.changeCount1 = 2;  // Set this to at least two until pxindex
			                          // changes the header itself
			pxhead.changeCount2 = 1;
			break;
	}
	pxhead.sortOrder = pxh->px_sortorder;
	if(!isindex && (pxh->px_fileversion >= 40)) {
		dataheadoffset = 0x78;
	} else {
		dataheadoffset = 0x58;
	}
	/* All the pointers, though we probably don't need them for a valid file */
	put_long_le(&pxhead.fldInfoPtr, basehead+dataheadoffset);
	put_long_le(&pxhead.tableNamePtrPtr, basehead+dataheadoffset+pxh->px_numfields*2);
	switch(pxh->px_fileversion) {
		case 70:
			pxhead.fileVersionID = 0x0C;
			tablenamelen = 261;
			break;
		case 50:
			pxhead.fileVersionID = 0x0B;
			tablenamelen = 79;
			break;
	}
	pxf = pxh->px_fields;
	sumfieldlen = 0;
	for(i=0; i<pxh->px_numfields; i++, pxf++) {
		if(pxf->px_fname)
			sumfieldlen += strlen(pxf->px_fname)+1;
		else
			sumfieldlen += 1;
	}
	/* +9 is for sortOrderID and trailing 0 */
	switch(pxh->px_filetype) {
		case pxfFileTypPrimIndex:
			put_short_le(&pxhead.realHeaderSize, dataheadoffset+pxh->px_numfields*2+4+tablenamelen);
			break;
		default:
			put_short_le(&pxhead.realHeaderSize, dataheadoffset+pxh->px_numfields*(2+4+2)+4+tablenamelen+sumfieldlen+9);
	}
	/* The datahead is only in .DB and .Xnn files. If it exists the
	 * common file header will continue at 0x78
	 */
	if(dataheadoffset == 0x78) {
		switch(pxh->px_fileversion) {
			case 70:
				put_short_le(&pxdatahead.fileVerID3, 0x010C);
				put_short_le(&pxdatahead.fileVerID4, 0x010C);
				break;
			case 50:
				put_short_le(&pxdatahead.fileVerID3, 0x010B);
				put_short_le(&pxdatahead.fileVerID4, 0x010B);
				break;
		}

		/* Writing the file update time leads to random writes at other
		 * postions in the file (usually either right after the field,
		 * or some bytes before). Strange: the order of bytes is always
		 * reversed.
		 * Update: The error has disappeared. Maybe this has been fixed
		 * when the header size was calculated properly.
		 */
		put_long_le(&pxdatahead.encryption2, 0);
		put_long_le(&pxdatahead.fileUpdateTime, 0x12345678);
		dummy = (long) time();
		put_long_le(&pxdatahead.fileUpdateTime, dummy);
		put_short_le(&pxdatahead.hiFieldID, pxh->px_numfields+1);
		put_short_le(&pxdatahead.hiFieldIDinfo, 0x20+pxh->px_numfields*(2+4)+4+tablenamelen+sumfieldlen);
		/* +8 is for sortOrderID */
		put_short_le(&pxdatahead.unknown6Cx6F[2], 0x18+pxh->px_numfields*(2+4+2)+4+tablenamelen+sumfieldlen+8);
		put_short_le(&pxdatahead.dosCodePage, pxh->px_doscodepage);
		switch(pxh->px_filetype) {
			case pxfFileTypIndexDB:
			case pxfFileTypNonIndexDB:
				pxdatahead.unknown6Cx6F[0] = 0x01;
				pxdatahead.unknown6Cx6F[1] = 0x01;
				break;
			case pxfFileTypPrimIndex:
				pxdatahead.unknown6Cx6F[0] = 0x0;
				pxdatahead.unknown6Cx6F[1] = 0x0;
		}
	}

	/* Goto the begining of the file */
	if(pxdoc->seek(pxdoc, pxs, 0, SEEK_SET) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not go to the begining paradox file."));
		return -1;
	}

	if(pxdoc->write(pxdoc, pxs, sizeof(TPxHeader), &pxhead) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write header of paradox file."));
		return -1;
	}

	if(dataheadoffset == 0x78) {
		if(pxdoc->write(pxdoc, pxs, sizeof(TPxDataHeader), &pxdatahead) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write header of paradox file."));
			return -1;
		}
	}

	pxf = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++, pxf++) {
		pxinfo.fType = pxf->px_ftype;
		pxinfo.fSize = pxf->px_flen;
		if(pxdoc->write(pxdoc, pxs, sizeof(TFldInfoRec), &pxinfo) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write field specification."));
			return -1;
		}
		
	}

	/* write tableNamePtr */
	/* dataheadoffset Bytes is the header. The continued header at
	 * dataheadoffset starts
	 * with numfields fields specifications (each 2 Bytes), followed
	 * by this pointer (tableNamePtr) and numfield pointers to the
	 * field names. */
	put_long_le(&ptr, basehead+dataheadoffset+pxh->px_numfields*(2+4)+4);
	if(pxdoc->write(pxdoc, pxs, 4, &ptr) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write pointer to tablename."));
		return -1;
	}
	/* write fieldNamePtrArray */
	/* base = 'Paradox Common File Header' + numfields * sizeof(TFldInfoRec) +
	 * numfields sizeof(* Fieldname) + sizeof(* Tablename) + strlen(tablename)
	 */
	if(!isindex) {
		base = (int) basehead+dataheadoffset+pxh->px_numfields*(2+4)+4+tablenamelen;
		pxf = pxh->px_fields;
		offset = 0;
		for(i=0; i<pxh->px_numfields; i++, pxf++) {
			put_long_le(&ptr, base+offset);
			offset += strlen(pxf->px_fname)+1;
			if(pxdoc->write(pxdoc, pxs, 4, &ptr) < 1) {
				px_error(pxdoc, PX_RuntimeError, _("Could not write pointers to field names."));
				return -1;
			}
		}
	}

	/* write tablename */
	if(pxh->px_tablename == NULL) {
		len = 0;
		px_error(pxdoc, PX_Warning, _("Tablename is empty."));
	} else {
		len = strlen(pxh->px_tablename);
		if(pxdoc->write(pxdoc, pxs, len, pxh->px_tablename) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write tablename."));
			return -1;
		}
	}

	/* write zeros to fill space for tablename */
	for(i=0; i<tablenamelen-len; i++) {
		if(pxdoc->write(pxdoc, pxs, 1, &nullint) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write tablename."));
			return -1;
		}
	}

	if(!isindex) {
		pxf = pxh->px_fields;
		for(i=0; i<pxh->px_numfields; i++, pxf++) {
			if(pxf->px_fname != NULL) {
				if(pxdoc->write(pxdoc, pxs, strlen(pxf->px_fname)+1, pxf->px_fname) < 1) {
					px_error(pxdoc, PX_RuntimeError, _("Could not write field name %d."), i);
					return -1;
				}
			} else {
				px_error(pxdoc, PX_Warning, _("Field name is NULL."));
				if(pxdoc->write(pxdoc, pxs, 1, &nullint) != 0) {
					px_error(pxdoc, PX_RuntimeError, _("Could not write field name %d."), i);
					return -1;
				}
			}
		}

		/* write fieldNumbers */
		for(i=0; i<pxh->px_numfields; i++) {
			put_short_le(&tmp, i+1);
			if(pxdoc->write(pxdoc, pxs, 2, &tmp) < 1) {
				px_error(pxdoc, PX_RuntimeError, _("Could not write field number %d."), i);
				return -1;
			}
		}

		/* write sortOrderID */
		if(pxdoc->write(pxdoc, pxs, 8, "ANSIINTL") < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write field numbers."));
			return -1;
		}
	}

	i = pxdoc->tell(pxdoc, pxs);
	if(i<pxh->px_headersize-1) {
		if(pxdoc->seek(pxdoc, pxs, pxh->px_headersize-1, SEEK_SET) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not fill header with zeros."));
			return -1;
		}
		if(pxdoc->write(pxdoc, pxs, 1, "\0") < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not fill header with zeros."));
			return -1;
		}
			
	}
	
	return 0;
}
/* }}} */

/* get_datablock_head() {{{
 */
int get_datablock_head(pxdoc_t *pxdoc, pxstream_t *pxs, int datablocknr, TDataBlock *datablockhead)
{
	pxhead_t *pxh;
	int position, ret;

	pxh = pxdoc->px_head;
	position = pxh->px_headersize+(datablocknr-1)*pxh->px_maxtablesize*0x400;
	if((ret = pxdoc->seek(pxdoc, pxs, position, SEEK_SET)) < 0) {
		return -1;
	}

	if((ret = pxdoc->read(pxdoc, pxs, sizeof(TDataBlock), datablockhead)) < 0) {
		return -1;
	}

	return 0;
}
/* }}} */

/* put_datablock_head() {{{
 */
int put_datablock_head(pxdoc_t *pxdoc, pxstream_t *pxs, int datablocknr, TDataBlock *datablockhead)
{
	pxhead_t *pxh;
	int position, ret;

	pxh = pxdoc->px_head;
	position =  pxh->px_headersize+(datablocknr-1)*pxh->px_maxtablesize*0x400;
	if((ret = pxdoc->seek(pxdoc, pxs, position, SEEK_SET)) < 0) {
		return -1;
	}

	if((ret = pxdoc->write(pxdoc, pxs, sizeof(TDataBlock), datablockhead)) < 0) {
		return -1;
	}

	return 0;
}
/* }}} */

/* put_px_datablock() {{{
 * adds an empty data block logically after the block 'after'.
 * The block is physically always added at the end of the file but
 * logically inserted into the link list. All header entries (px_firstblock,
 * px_lastblock and px_fileblocks) will be updated.
 * Returns the number of the new datablock. The first one has number
 * 1 as stored in the datablock head as well.
 */
int put_px_datablock(pxdoc_t *pxdoc, pxhead_t *pxh, int after, pxstream_t *pxs) {
	TDataBlock newdatablockhead, prevdatablockhead, nextdatablockhead;
	int i, next, ret, nullint = 0;

	if(after > pxh->px_fileblocks) {
		px_error(pxdoc, PX_RuntimeError, _("Trying to insert data block after block number %d, but file has only %d blocks."), after, pxh->px_fileblocks);
		return -1;
	}

	if(after < 0) {
		px_error(pxdoc, PX_RuntimeError, _("You did not pass a valid block number."));
		return -1;
	}

	/* Goto the block before the new block and read its header. */
	if(after != 0) {
		if((ret = get_datablock_head(pxdoc, pxs, after, &prevdatablockhead)) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not get head of data block before the new block."));
			return -1;
		}
		/* Goto the block which will be after the new block and read its header. */
		next = get_short_le((char *) &prevdatablockhead.nextBlock);
	} else {
		next = pxh->px_firstblock;
	}
	
//	fprintf(stderr, "Inserting new block after %d and before %d\n", after, next);
	if(next != 0) {
		if((ret = get_datablock_head(pxdoc, pxs, next, &nextdatablockhead)) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not get head of data block after the new block."));
			return -1;
		}
	}

	memset(&newdatablockhead, 0, sizeof(TDataBlock));
	put_short_le(&newdatablockhead.prevBlock, after);
	put_short_le(&newdatablockhead.nextBlock, next);
	/* This block is still empty, so set it to -recordsize */
	put_short_le(&newdatablockhead.addDataSize, -pxh->px_recordsize);
//	fprintf(stderr, "Hexdump of new datablock: ");
//	hex_dump(stderr, &newdatablockhead, sizeof(TDataBlock));
//	fprintf(stderr, "\n");
	/* Write new datablock at the end of the file */
	if(put_datablock_head(pxdoc, pxs, pxh->px_fileblocks+1, &newdatablockhead) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write new data block header."));
		return -1;
	}

	/* write an empty block. File pointer is still at right position. */
	for(i=0; i<pxh->px_maxtablesize*0x400-sizeof(TDataBlock); i++) {
		if(pxdoc->write(pxdoc, pxs, 1, &nullint) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write empty data block."));
			return -1;
		}
	}

	/* Update the block before the new one */
	if(after != 0) {
		put_short_le(&prevdatablockhead.nextBlock, pxh->px_fileblocks+1);
		if(put_datablock_head(pxdoc, pxs, after, &prevdatablockhead) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not update data block header before new block."));
			return -1;
		}
	}

	/* Update the block after the new one */
	if(next != 0) {
		put_short_le(&nextdatablockhead.prevBlock, pxh->px_fileblocks+1);
		if(put_datablock_head(pxdoc, pxs, after, &nextdatablockhead) < 0) {
			px_error(pxdoc, PX_RuntimeError, _("Could not update datablock header after new block."));
			return -1;
		}
	}

	/* Update the header */
	pxh->px_fileblocks++;
	if(after == 0)
		pxh->px_firstblock = pxh->px_fileblocks;
	if(next == 0)
		pxh->px_lastblock = pxh->px_fileblocks;
	if(put_px_head(pxdoc, pxh, pxs) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Unable to write file header."));
		return -1;
	}
	return(pxh->px_fileblocks);
}
/* }}} */

/* px_add_data_to_block() {{{
 * stores a record into a data block. datablocknr is the logical number
 * of the block (the first block has number 1).
 */
int px_add_data_to_block(pxdoc_t *pxdoc, pxhead_t *pxh, int datablocknr, char *data, pxstream_t *pxs) {
	TDataBlock datablockhead;
	int ret, n;

	/* Get header of the data block */
	if((ret = get_datablock_head(pxdoc, pxs, datablocknr, &datablockhead)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read data block header."));
		return -1;
	}

	n = get_short_le((char *) &datablockhead.addDataSize)/pxh->px_recordsize+1;
//	fprintf(stderr, "Hexdump des alten datablock headers: ");
//	hex_dump(stderr, &datablockhead, sizeof(TDataBlock));
//	fprintf(stderr, "\n");
//	fprintf(stderr, "Größe des Datenblocks: %d\n", get_short_le((char *) &datablockhead.addDataSize));
//	fprintf(stderr, "Datablock %d has %d records\n", datablocknr, n);

	/* Update size of data block and write it back */
//	fprintf(stderr, "Hexdump des neuen datablock headers: ");
//	hex_dump(stderr, &datablockhead, sizeof(TDataBlock));
//	fprintf(stderr, "\n");
	put_short_le(&datablockhead.addDataSize, n*pxh->px_recordsize);
	if(put_datablock_head(pxdoc, pxs, datablocknr, &datablockhead) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write updated data block header."));
		return -1;
	}

	/* Goto start of record data */
	if((ret = pxdoc->seek(pxdoc, pxs, n*pxh->px_recordsize, SEEK_CUR)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek to start of new record."));
		return -1;
	}

	/* Write the record data */
	if(pxdoc->write(pxdoc, pxs, pxh->px_recordsize, data) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write record."));
		return -1;
	}
	
	return n;
}
/* }}} */

/* get_mb_head() {{{
 * get the header info from the file
 * basic header info & field descriptions
 */
mbhead_t *get_mb_head(pxblob_t *pxblob, pxstream_t *pxs) {
	pxdoc_t *pxdoc;
	TMbHeader mbhead;
	mbhead_t *mbh;
	int ret;

	pxdoc = pxblob->pxdoc;
	if(NULL == pxdoc) {
		return(NULL);
	}

	if((mbh = (mbhead_t *) pxdoc->malloc(pxdoc, sizeof(mbhead_t), _("Allocate memory for document header."))) == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Could not allocate memory for document header."));
		return NULL;
	}
	if(pxblob->seek(pxdoc, pxs, 0, SEEK_SET) < 0)
		return NULL;
	if((ret = pxblob->read(pxdoc, pxs, sizeof(TMbHeader), &mbhead)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read header from paradox file."));
		pxdoc->free(pxdoc, mbh);
		return NULL;
	}

	mbh->modcount = get_short_le(&mbhead.modcount);
	return(mbh);
}
/* }}} */

/* put_mb_head() {{{
 * writes the header of a .mb file.
 */
int put_mb_head(pxblob_t *pxblob, mbhead_t *mbh, pxstream_t *pxs) {
	pxdoc_t *pxdoc;
	TMbHeader mbhead;
	int nullint = 0, i;

	pxdoc = pxblob->pxdoc;
	if(NULL == pxdoc) {
		px_error(pxdoc, PX_RuntimeError, _("Blob file has no associated paradox database."));
		return(-1);
	}

	if(pxblob->seek(pxdoc, pxs, 0, SEEK_SET) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not go to the begining paradox file."));
		return -1;
	}

	memset(&mbhead, 0, sizeof(TMbHeader));
	put_short_le(&mbhead.blocksize, 1);
	put_short_le(&mbhead.modcount, 1);
	put_short_le(&mbhead.basesize, 0x1000);
	put_short_le(&mbhead.subblocksize, 0x1000);
	mbhead.subchunksize = 0x10;
	put_short_le(&mbhead.suballoc, 0x0040);
	put_short_le(&mbhead.subthresh, 0x0800);
	if(pxblob->write(pxdoc, pxs, sizeof(TMbHeader), &mbhead) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write header of paradox file."));
		return -1;
	}

	/* write zeros to fill space of first block */
	for(i=0; i<4096-sizeof(TMbHeader); i++) {
		if(pxblob->write(pxdoc, pxs, 1, &nullint) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write remaining blob file header."));
			return -1;
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
