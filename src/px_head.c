#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <paradox.h>
#include <px_intern.h>
#include <px_memory.h>

/* get_px_head() {{{
 * get the header info from the file
 * basic header info & field descriptions
 */
pxhead_t *get_px_head(pxdoc_t *pxdoc, FILE *fp)
{
	pxhead_t *pxh;
	TPxHeader pxhead;
	TPxDataHeader pxdatahead;
	TFldInfoRec pxinfo;
	pxfield_t *pfield;
	char dummy[300], c;
	int ret, i, j;

	if((pxh = (pxhead_t *) pxdoc->malloc(pxdoc, sizeof(pxhead_t), _("Couldn't get memory for document header."))) == NULL)
		return NULL;
	if(fseek(fp, 0, SEEK_SET) < 0)
		return NULL;
	if((ret = fread(&pxhead, sizeof(TPxHeader), 1, fp)) < 0) {
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
	switch(pxhead.fileVersionID) {
		case 3:
			pxh->px_fileversion = 30;
			break;
		case 4:
			pxh->px_fileversion = 35;
			break;
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
			pxh->px_fileversion = 40;
			break;
		case 10:
		case 11:
			pxh->px_fileversion = 50;
			break;
		case 12:
			pxh->px_fileversion = 70;
			break;
		default:
			pxh->px_fileversion = 0;
	}
	pxh->px_indexfieldnumber = pxhead.indexFieldNumber;
	pxh->px_writeprotected = pxhead.writeProtected;
	pxh->px_modifiedflags1 = pxhead.modifiedFlags1;
	pxh->px_modifiedflags2 = pxhead.modifiedFlags2;
	pxh->px_primarykeyfields = get_short_le(&pxhead.primaryKeyFields);

	if(((pxh->px_filetype == 0) ||
		  (pxh->px_filetype == 2) ||
		  (pxh->px_filetype == 3) ||
		  (pxh->px_filetype == 5)) &&
		  (pxh->px_fileversion >= 40)) {
		if((ret = fread(&pxdatahead, sizeof(TPxDataHeader), 1, fp)) < 0) {
			pxdoc->free(pxdoc, pxh);
			return NULL;
		}
		pxh->px_doscodepage = get_short_le(&pxdatahead.dosCodePage);
	}

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
		if((ret = fread(&pxinfo, sizeof(TFldInfoRec), 1, fp)) < 0) {
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
	if((ret = fread(dummy, sizeof(int), 1, fp)) < 0) {
		pxdoc->free(pxdoc, pxh->px_fields);
		pxdoc->free(pxdoc, pxh);
		return NULL;
	}

	/* skip the tfieldNamePtrArray, not present in index files */
	if(pxhead.fileType == 0 || pxhead.fileType == 2) {
		for(i=0; i<pxh->px_numfields; i++) {
			if((ret = fread(dummy, sizeof(int), 1, fp)) < 0) {
				pxdoc->free(pxdoc, pxh->px_fields);
				pxdoc->free(pxdoc, pxh);
				return NULL;
			}
		}
	}

	/* skip the tableName */
	if(pxh->px_fileversion >= 70)
		ret = fread(dummy, 261, 1, fp);
	else
		ret = fread(dummy, 79, 1, fp);
	if(ret < 0) {
		pxdoc->free(pxdoc, pxh->px_fields);
		pxdoc->free(pxdoc, pxh);
		return NULL;
	}
	pxh->px_tablename = px_strdup(pxdoc, dummy);

	pfield = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++) {
		j=0;
		while(((ret = fread(&c, 1, 1, fp)) >= 0) && (c != '\0')) {
			dummy[j++] = c;
		}
		if(ret < 0) {
			pxdoc->free(pxdoc, pxh->px_tablename);
			pxdoc->free(pxdoc, pxh->px_fields);
			pxdoc->free(pxdoc, pxh);
			return NULL;
		}
		dummy[j] = '\0';
		pfield->px_fname = px_strdup(pxdoc, (const char *)dummy);
		pfield++;
	}

	return pxh;
}
/* }}} */

/* put_px_head() {{{
 * writes the header and field information into a new file.
 */
int put_px_head(pxdoc_t *pxdoc, pxhead_t *pxh, FILE *fp) {
	TPxHeader pxhead;
	TPxDataHeader pxdatahead;
	TFldInfoRec pxinfo;
	pxfield_t *pxf;
	int nullint = 0;
	int i;

	memset(&pxhead, 0, sizeof(pxhead));
	memset(&pxdatahead, 0, sizeof(pxdatahead));

	put_short_le(&pxhead.recordSize, pxh->px_recordsize);
	put_short_le(&pxhead.headerSize, pxh->px_headersize);
	put_short_le(&pxhead.fileBlocks, pxh->px_fileblocks);
	pxhead.fileType = pxh->px_filetype;
	pxhead.maxTableSize = pxh->px_maxtablesize;
	put_long_le(&pxhead.numRecords, pxh->px_numrecords);
	pxhead.writeProtected = pxh->px_writeprotected;
	put_short_le(&pxhead.numFields, pxh->px_numfields);
	switch(pxh->px_fileversion) {
		case 70:
			pxhead.fileVersionID = 0x0C;
			put_short_le(&pxdatahead.fileVerID3, 0x010C);
			put_short_le(&pxdatahead.fileVerID4, 0x010C);
			break;
	}

	put_short_le(&pxdatahead.dosCodePage, pxh->px_doscodepage);
	put_short_le(&pxdatahead.hiFieldID, pxh->px_numfields+1);

	/* Goto the begining of the file */
	if(fseek(fp, 0, SEEK_SET) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not go to begining paradox file."));
		return -1;
	}

	if(fwrite(&pxhead, sizeof(TPxHeader), 1, fp) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read header from paradox file."));
		return -1;
	}

	if(fwrite(&pxdatahead, sizeof(TPxDataHeader), 1, fp) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read header from paradox file."));
		return -1;
	}

	pxf = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++, pxf++) {
		pxinfo.fType = pxf->px_ftype;
		pxinfo.fSize = pxf->px_flen;
		if(fwrite(&pxinfo, sizeof(TFldInfoRec), 1, fp) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write column specification."));
			return -1;
		}
		
	}

	/* write tableNamePtr */
	if(fwrite(&nullint, 4, 1, fp) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write column specification."));
		return -1;
	}
	/* write fieldNamePtrArray */
	for(i=0; i<pxh->px_numfields; i++) {
		if(fwrite(&nullint, 4, 1, fp) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write column specification."));
			return -1;
		}
	}

	/* write tablename */
	if(fwrite(pxh->px_tablename, strlen(pxh->px_tablename), 1, fp) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write column specification."));
		return -1;
	}

	/* write zeros to fill space for tablename */
	for(i=0; i<261-strlen(pxh->px_tablename); i++) {
		if(fwrite(&nullint, 1, 1, fp) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write column specification."));
			return -1;
		}
	}
	pxf = pxh->px_fields;
	for(i=0; i<pxh->px_numfields; i++, pxf++) {
		if(fwrite(pxf->px_fname, strlen(pxf->px_fname)+1, 1, fp) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write column specification."));
			return -1;
		}
	}

	return 0;
}
/* }}} */

/* put_px_datablock() {{{
 * writes an empty data block. Returns the number of the new
 * datablock. The first one has number 0. In the datablock head
 * the first block has number 1.
 */
int put_px_datablock(pxdoc_t *pxdoc, pxhead_t *pxh, FILE *fp) {
	TDataBlock datablockhead;
	int i, ret, nullint = 0;

	/* Go to the start of the data block (skip the header) */
	if((ret = fseek(fp, pxh->px_headersize+pxh->px_fileblocks*pxh->px_maxtablesize*0x400, SEEK_SET)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek start of first data block"));
		return -1;
	}

	memset(&datablockhead, 0, sizeof(TDataBlock));
	put_short_le(&datablockhead.blockNumber, pxh->px_fileblocks+1);
	put_short_le(&datablockhead.addDataSize, -pxh->px_recordsize);
	if(fwrite(&datablockhead, sizeof(TDataBlock), 1, fp) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write empty datablock header."));
		return -1;
	}

	/* write an empty block */
	for(i=0; i<pxh->px_maxtablesize*0x400-sizeof(TDataBlock); i++) {
		if(fwrite(&nullint, 1, 1, fp) < 1) {
			px_error(pxdoc, PX_RuntimeError, _("Could not write empty datablock."));
			return -1;
		}
	}

	return(pxh->px_fileblocks);
}

int px_add_data_to_block(pxdoc_t *pxdoc, pxhead_t *pxh, int datablocknr, char *data, FILE *fp) {
	TDataBlock datablockhead;
	int ret, n;

	/* Go to the start of the data block (skip the header) */
	if((ret = fseek(fp, pxh->px_headersize+datablocknr*pxh->px_maxtablesize*0x400, SEEK_SET)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek to start of first data block"));
		return -1;
	}

	if((ret = fread(&datablockhead, sizeof(TDataBlock), 1, fp)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not read data block header."));
		return -1;
	}
	n = get_short_le((char *) &datablockhead.addDataSize)/pxh->px_recordsize+1;
	fprintf(stderr, "Hexdump des alten datablock headers: ");
	hex_dump(stderr, &datablockhead, sizeof(TDataBlock));
	fprintf(stderr, "\n");
	fprintf(stderr, "Größe des Datenblocks: %d\n", get_short_le((char *) &datablockhead.addDataSize));
	fprintf(stderr, "Datablock %d hat %d Datensätze\n", datablocknr, n);

	/* Go back the beginning of the block */
	if((ret = fseek(fp, -sizeof(TDataBlock), SEEK_CUR)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek to start of data block"));
		return -1;
	}

	/* Update size of data block and write it back */
	put_short_le(&datablockhead.addDataSize, n*pxh->px_recordsize);
	fprintf(stderr, "Hexdump des neuen datablock headers: ");
	hex_dump(stderr, &datablockhead, sizeof(TDataBlock));
	fprintf(stderr, "\n");
	if(fwrite(&datablockhead, sizeof(TDataBlock), 1, fp) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write empty datablock header."));
		return -1;
	}

	/* Goto start of record data */
	if((ret = fseek(fp, n*pxh->px_recordsize, SEEK_CUR)) < 0) {
		px_error(pxdoc, PX_RuntimeError, _("Could not fseek to start of new record"));
		return -1;
	}

	/* Write the record data */
	if(fwrite(data, pxh->px_recordsize, 1, fp) < 1) {
		px_error(pxdoc, PX_RuntimeError, _("Could not write record."));
		return -1;
	}
	
	return n;
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
