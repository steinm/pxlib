#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <paradox.h>

void usage(char *progname) {
	printf("usage: %s paradox file\n", progname);
}

int main(int argc, char *argv[]) {
	pxhead_t *pxh;
	pxfield_t *pxf;
	pxdoc_t *pxdoc;
	char *data, buffer[1000];
	int i, j, c;
	int outputcsv = 0;
	int outputinfo = 1;
	int outputsql = 0;
	char delimiter = ';';
	char enclosure = '"';
	char *inputfile = NULL;

	while(1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"info", 0, 0, 'i'},
			{"csv", 0, 0, 'c'},
			{"sql", 0, 0, 's'},
			{"file", 1, 0, 'f'},
			{"output", 1, 0, 'o'},
			{"help", 0, 0, 'h'},
			{0, 0, 0, 0}
		};
		c = getopt_long (argc, argv, "icsf:o:h",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage(argv[0]);
				exit(0);
				break;
			case 'f':
				inputfile = strdup(optarg);
				break;
			case 'o':
				break;
			case 'i':
				outputinfo = 1;
				break;
			case 'c':
				outputcsv = 1;
				outputinfo = 0;
				break;
			case 's':
				outputsql = 1;
				outputinfo = 0;
				break;
		}
	}
	if (optind < argc) {
		inputfile = strdup(argv[optind]);
	}

	if(!inputfile) {
		fprintf(stderr, "You must at least specify an input file\n");
		usage(argv[0]);
		exit(0);
	}

	pxdoc = PX_new();
	if(0 > PX_open_file(pxdoc, inputfile)) {
		exit(1);
	}
	pxh = pxdoc->px_head;

	if(outputinfo) {
		printf("File Version:        %1.1f\n", (float) pxh->px_fileversion/10.0);
		printf("File Type:           ");
		switch(pxh->px_filetype) {
			case pxfFileTypIndexDB:
				printf("indexed .DB data file");
				break;
			case pxfFileTypPrimIndex:
				printf("primary index .PX file");
				break;
			case pxfFileTypNonIndexDB:
				printf("non-indexed .DB data file");
				break;
			case pxfFileTypNonIncSecIndex:
				printf("non-incrementing secondary index .Xnn file");
				break;
			case pxfFileTypSecIndex:
				printf("secondary index .Ynn file (inc or non-inc)");
				break;
			case pxfFileTypIncSecIndex:
				printf("incrementing secondary index .Xnn file");
				break;
			case pxfFileTypNonIncSecIndexG:
				printf("non-incrementing secondary index .XGn file");
				break;
			case pxfFileTypSecIndexG:
				printf("secondary index .YGn file (inc or non inc)");
				break;
			case pxfFileTypIncSecIndexG:
				printf("incrementing secondary index .XGn file");
				break;
		}
		printf("\n");
		printf("Tablename:           %s\n", pxh->px_tablename);
		printf("Num. of Records:     %d\n", pxh->px_numrecords);
		printf("Num. of Fields:      %d\n", pxh->px_numfields);
		printf("Header size:         %d (0x%X)\n", pxh->px_headersize, pxh->px_headersize);
		printf("Max. Table size:     %d (0x%X)\n", pxh->px_maxtablesize, pxh->px_maxtablesize*0x400);
		printf("Num. of Data Blocks: %d\n", pxh->px_fileblocks);
		if((pxh->px_filetype == pxfFileTypNonIncSecIndex) ||
			 (pxh->px_filetype == pxfFileTypIncSecIndex))
			printf("Num. of Index Field: %d\n", pxh->px_indexfieldnumber);
		printf("Num. of prim. Key fields: %d\n", pxh->px_primarykeyfields);
		printf("Write protected:     %d\n", pxh->px_writeprotected);
		printf("Code Page:           %d (0x%X)\n", pxh->px_doscodepage, pxh->px_doscodepage);
		printf("\n");

		printf("Fieldname          | Type\n");
		printf("------------------------------------\n");
		pxf = pxh->px_fields;
		for(i=0; i<pxh->px_numfields; i++) {
			printf("%18s | ", pxf->px_fname);
			switch(pxf->px_ftype) {
				case pxfAlpha:
					printf("char(%d)\n", pxf->px_flen);
					break;
				case pxfDate:
					printf("date(%d)\n", pxf->px_flen);
					break;
				case pxfShort:
					printf("int(%d)\n", pxf->px_flen);
					break;
				case pxfLong:
					printf("int(%d)\n", pxf->px_flen);
					break;
				case pxfCurrency:
					printf("currency(%d)\n", pxf->px_flen);
					break;
				case pxfNumber:
					printf("double(%d)\n", pxf->px_flen);
					break;
				case pxfLogical:
					printf("bool(%d)\n", pxf->px_flen);
					break;
				case pxfMemoBLOb:
					printf("bool(%d)\n", pxf->px_flen);
					break;
				case pxfBLOb:
					printf("blob(%d)\n", pxf->px_flen);
					break;
				case pxfFmtMemoBLOb:
					printf("bool(%d)\n", pxf->px_flen);
					break;
				case pxfOLE:
					printf("ole(%d)\n", pxf->px_flen);
					break;
				case pxfGraphic:
					printf("graphic(%d)\n", pxf->px_flen);
					break;
				case pxfTime:
					printf("time(%d)\n", pxf->px_flen);
					break;
				case pxfTimestamp:
					printf("timestamp(%d)\n", pxf->px_flen);
					break;
				case pxfAutoInc:
					printf("timestamp(%d)\n", pxf->px_flen);
					break;
				case pxfBCD:
					printf("timestamp(%d)\n", pxf->px_flen);
					break;
				case pxfBytes:
					printf("timestamp(%d)\n", pxf->px_flen);
					break;
				default:
					printf("%c(%d)\n", pxf->px_ftype, pxf->px_flen);
			}
			pxf++;
		}
	}

	if(outputcsv) {
		if((data = (char *) px_malloc(pxdoc, pxh->px_recordsize, "Could not allocate memory for record.")) == NULL) {
			exit(1);
		}

		for(j=0; j<pxh->px_numrecords; j++) {
			int offset;
			if(PX_get_record(pxdoc, j, data)) {
				pxf = pxh->px_fields;
				offset = 0;
				for(i=0; i<pxh->px_numfields; i++) {
					switch(pxf->px_ftype) {
						case pxfAlpha:
							memcpy(buffer, &data[offset], pxf->px_flen);
							buffer[pxf->px_flen] = '\0';
							printf("%c%s%c", enclosure, buffer, enclosure);
							break;
						case pxfDate:
							data[offset] ^= data[offset];
							printf("%d", *((int *)(&data[offset])));
							break;
						case pxfShort:
							data[offset] ^= data[offset];
							printf("%d", *((short int *)(&data[offset])));
							break;
						case pxfLong:
							data[offset] ^= data[offset];
							printf("%ld", *((long int *)(&data[offset])));
							break;
						case pxfNumber:
							printf("%f", *((double *)(&data[offset])));
							break;
						case pxfGraphic:
						case pxfBLOb:
							hex_dump(&data[offset], pxf->px_flen);
							break;
						default:
							printf("");
					}
					if(i < (pxh->px_numfields-1))
						printf("%c", delimiter);
					offset += pxf->px_flen;
					pxf++;
				}
				printf("\n");
			} else {
				printf("Couldn't get record\n");
			}
		}
		px_free(data);
	}

	PX_close(pxdoc);
}
