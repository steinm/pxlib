#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libintl.h>
#include <sys/types.h>
#include <regex.h>
#include <libgen.h>
#include <paradox.h>
#include "config.h"
#define _(String) gettext(String)


void errorhandler(pxdoc_t *p, int error, const char *str) {
	  fprintf(stderr, "PXLib: %s\n", str);
}

void usage(char *progname) {
	int recode;

	printf(_("Version: %s %s http://sourceforge.net/projects/pxlib"), progname, VERSION);
	printf("\n");
	printf(_("Copyright: Copyright (C) 2003 Uwe Steinmann <uwe@steinmann.cx>"));
	printf("\n\n");
	printf(_("%s writes a paradox file."), progname);
	printf("\n\n");
	printf(_("Usage: %s [OPTIONS] FILE"), progname);
	printf("\n\n");
	printf(_("Options:"));
	printf("\n\n");
	printf(_("  -h, --help          this usage information."));
	printf("\n");
	printf(_("  -v, --verbose       be more verbose."));
	printf("\n\n");

	recode = PX_has_recode_support();
	switch(recode) {
		case 1:
			printf(_("libpx uses librecode for recoding."));
			break;
		case 2:
			printf(_("libpx uses iconv for recoding."));
			break;
		case 0:
			printf(_("libpx has no support for recoding."));
			break;
	}
	printf("\n\n");
	if(PX_is_bigendian())
		printf(_("libpx has been compiled for big endian architecture."));
	else
		printf(_("libpx has been compiled for little endian architecture."));
	printf("\n\n");
}

int main(int argc, char *argv[]) {
	pxdoc_t *pxdoc = NULL;
	char *progname = NULL;
	char *targetencoding = NULL;
	char *inputencoding = NULL;
	char *inputfile = NULL;
	char *outputfile = NULL;
	char *tablename = NULL;
	FILE *infp;
	int c, i;
	int verbose = 0;
	int modecsv = 0;
	int modetest = 0;
	int modedebug = 0;

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	setlocale (LC_NUMERIC, "C");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	textdomain (GETTEXT_PACKAGE);
#endif

	progname = basename(strdup(argv[0]));
	while(1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"test", 0, 0, 't'},
			{"csv", 0, 0, 'c'},
			{"verbose", 0, 0, 'v'},
			{"input-file", 1, 0, 'f'},
			{"help", 0, 0, 'h'},
			{"separator", 1, 0, 0},
			{"enclosure", 1, 0, 1},
			{"tablename", 1, 0, 3},
			{"mode", 1, 0, 4},
			{"input-encoding", 1, 0, 5},
			{"output-encoding", 1, 0, 'r'},
			{0, 0, 0, 0}
		};
		c = getopt_long (argc, argv, "vtcf:o:r:h",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage(progname);
				exit(0);
				break;
			case 'v':
				verbose = 1;
				break;
			case 'c':
				modecsv = 1;
				break;
			case 't':
				modetest = 1;
				break;
			case 5:
				inputencoding = strdup(optarg);
				break;
			case 'r':
				targetencoding = strdup(optarg);
				break;
			case 'f':
				inputfile = strdup(optarg);
				break;
			case 3:
				tablename = strdup(optarg);
				break;
			case 4:
				if(!strcmp(optarg, "csv")) {
					modecsv = 1;
				} else if(!strcmp(optarg, "test")) {
					modetest = 1;
				} else if(!strcmp(optarg, "debug")) {
					modedebug = 1;
				}
				break;
		}
	}

	if (optind < argc) {
		outputfile = strdup(argv[optind]);
	}

	if(!outputfile) {
		fprintf(stderr, _("You must at least specify an output file."));
		fprintf(stderr, "\n");
		fprintf(stderr, "\n");
		usage(progname);
		exit(1);
	}

	if((inputfile == NULL) || !strcmp(inputfile, "-")) {
		infp = stdin;
	} else {
		infp = fopen(inputfile, "r");
		if(infp == NULL) {
			fprintf(stderr, _("Could not open input file."));
			fprintf(stderr, "\n");
			exit(1);
		}
	}

	if(modetest) {
		int numfields = 5;
		pxfield_t *pxf, *pxfield;
		char data[40];

		if(NULL == (pxdoc = PX_new2(errorhandler, NULL, NULL, NULL))) {
			fprintf(stderr, _("Could not create new paradox instance."));
			fprintf(stderr, "\n");
			exit(1);
		}

		if((pxf = (pxfield_t *) pxdoc->malloc(pxdoc, numfields*sizeof(pxfield_t), _("Could not get memory for field definitions."))) == NULL){
			fprintf(stderr, "\n");
			exit(1);
		}
		pxf[0].px_fname = strdup("column1");
		pxf[0].px_ftype = pxfShort;
		pxf[0].px_flen = 2;
		pxf[0].px_fdc = 0;
		pxf[1].px_fname = strdup("column2");
		pxf[1].px_ftype = pxfShort;
		pxf[1].px_flen = 2;
		pxf[1].px_fdc = 0;
		pxf[2].px_fname = strdup("column3");
		pxf[2].px_ftype = pxfLong;
		pxf[2].px_flen = 4;
		pxf[2].px_fdc = 0;
		pxf[3].px_fname = strdup("column4");
		pxf[3].px_ftype = pxfNumber;
		pxf[3].px_flen = 8;
		pxf[3].px_fdc = 0;
		pxf[4].px_fname = strdup("column5");
		pxf[4].px_ftype = pxfAlpha;
		pxf[4].px_flen = 20;
		pxf[4].px_fdc = 0;

		if(0 > PX_create_file(pxdoc, pxf, numfields, outputfile)) {
			fprintf(stderr, _("Could not create output file."));
			fprintf(stderr, "\n");
			exit(1);
		}
		if(tablename)
			PX_set_tablename(pxdoc, tablename);

		for(i=0; i<200; i++) {
			char buffer[30];
			memset(data, 0, 40);
			PX_put_data_short(pxdoc, &data[0], 2, i);
			PX_put_data_short(pxdoc, &data[2], 2, -23);
			PX_put_data_long(pxdoc, &data[4], 4, i*4);
			PX_put_data_double(pxdoc, &data[8], 8, i*1.0001);
			snprintf(buffer, 30, "------ Nummer %d", i);
			PX_put_data_alpha(pxdoc, &data[16], 20, buffer);
//			hex_dump(stderr, data, 30);
//			    fprintf(stderr, "\n");

			if(0 > PX_put_record(pxdoc, data)) {
				fprintf(stderr, _("Could not write record."));
				fprintf(stderr, "\n");
				exit(1);
			}
		}
		PX_close(pxdoc);
	}

#define CSV_BUFSIZE 1000
#define CSV_MAXCOLS 300
	if(modecsv) {
		pxfield_t *pxf, *pxfield;
		char buffer[CSV_BUFSIZE];
		char *data;
		char **row;
		char **rowptr;
		int i, len, numfields, itmp;

		if(NULL == (pxdoc = PX_new2(errorhandler, NULL, NULL, NULL))) {
			fprintf(stderr, _("Could not create new paradox instance."));
			fprintf(stderr, "\n");
			exit(1);
		}

		/* Create a piece of memory where the pointers to the field values
		 * can be stored. This array of pointers is used for any row.
		 * csv_row_fread() initialized it to 0 before refilling it.
		 */
		if(NULL == (row = (char **) pxdoc->malloc(pxdoc, CSV_MAXCOLS*sizeof(char *), _("Allocate memory for single line")))) {
			fprintf(stderr, _("Allocate memory for single row."));
			fprintf(stderr, "\n");
			exit(1);
		}

		/* Read first line of input, which ist the column specification.
		 * We expect each field of the form Name,Type,Size,Prec.
		 * Name: Name of column
		 * Type: A = Alpha
		 *       S = Short Int
		 *       I = Long Int
		 *       D = Date
		 *       L = Logical (Boolean)
		 *       N = Number (Double)
		 * Size: Size of field in Bytes
		 * Prec: Precision (not needed)
		 */
		if(0 > (numfields = csv_row_fread(infp, buffer, CSV_BUFSIZE, row, CSV_MAXCOLS, 1))) {
			fprintf(stderr, _("Could not read first line of csv input."));
			fprintf(stderr, "\n");
			exit(1);
		}

		/* Read the field specification an build the field array for
		 * PX_create_fp() */
		if((pxf = (pxfield_t *) pxdoc->malloc(pxdoc, numfields*sizeof(pxfield_t), _("Allocate memory for field definitions."))) == NULL){
			fprintf(stderr, _("Could not allocate memory for field specification array."));
			fprintf(stderr, "\n");
			exit(1);
		}

		rowptr = row;
		i = 0;
		while((*rowptr != NULL) && (i < numfields)) {
			char *fieldstr, *tmp;
			int len, needsize;

			needsize = 0;
			fieldstr = *rowptr;

			/* Search for the first comma which is the end of the field name. */
			tmp = strchr(fieldstr, ',');
			if(NULL == tmp) {
				fprintf(stderr, _("Field specification must be a comma separated value (Name,Type,Size,Prec)."));
				fprintf(stderr, "\n");
				exit(1);
			}
			len = tmp-fieldstr;
			if(NULL == (pxf[i].px_fname = pxdoc->malloc(pxdoc, len+1, _("Allocate memory for column name.")))) {
				fprintf(stderr, _("Could not allocate memory for %d. field name."), i);
				fprintf(stderr, "\n");
				exit(1);
			}
			strncpy(pxf[i].px_fname, fieldstr, len);
			pxf[i].px_fname[len] = '\0';

			/* Get the field Type */
			fieldstr = tmp+1;
			if(*fieldstr == '\0') {
				fprintf(stderr, _("%d. field specification ended unexpectetly."), i);
				fprintf(stderr, "\n");
				exit(1);
			}
			if(*fieldstr == ',') {
				fprintf(stderr, _("%d. field specification misses type."), i);
				fprintf(stderr, "\n");
				exit(1);
			}
			switch((int) *fieldstr) {
				case 'S':
					pxf[i].px_ftype = pxfShort;
					pxf[i].px_flen = 2;
					break;
				case 'I':
					pxf[i].px_ftype = pxfLong;
					pxf[i].px_flen = 4;
					break;
				case 'A':
					pxf[i].px_ftype = pxfAlpha;
					needsize = 1;
					break;
				case 'N':
					pxf[i].px_ftype = pxfNumber;
					pxf[i].px_flen = 8;
					break;
				case '$':
					pxf[i].px_ftype = pxfCurrency;
					pxf[i].px_flen = 8;
					break;
				case 'L':
					pxf[i].px_ftype = pxfLogical;
					pxf[i].px_flen = 1;
					break;
				case 'D':
					pxf[i].px_ftype = pxfDate;
					pxf[i].px_flen = 4;
					break;
				case '+':
					pxf[i].px_ftype = pxfAutoInc;
					pxf[i].px_flen = 4;
					break;
				default:
					fprintf(stderr, _("%d. field type '%c' is unknown."), i, *fieldstr);
					fprintf(stderr, "\n");
					exit(1);
			}

			if(needsize) {
				char *endptr;
				/* find end of type definition */
				tmp = strchr(fieldstr, ',');
				if(NULL == tmp || *(tmp+1) == '\0') {
					fprintf(stderr, _("Field specification misses the column size."));
					fprintf(stderr, "\n");
					exit(1);
				}
				fieldstr = tmp+1;
				pxf[i].px_flen = strtol(fieldstr, &endptr, 10);
				if((endptr == NULL) || (fieldstr == endptr)) {
					fprintf(stderr, _("Field specification misses the column size."));
					fprintf(stderr, "\n");
					exit(1);
				}
				if(*endptr != '\0') {
					/* There is also precision which we do not care about. */
					fieldstr = endptr+1;
					fprintf(stderr, _("The remainder '%s' of the specification for field %d is being disregarded."), fieldstr, i+1);
					fprintf(stderr, "\n");
				} 
			}

			rowptr++;
			i++;
		}

		/* Create the paradox file */
		if(0 > PX_create_file(pxdoc, pxf, numfields, outputfile)) {
			fprintf(stderr, _("Could not create output file."));
			fprintf(stderr, "\n");
			exit(1);
		}

		if(inputencoding != NULL)
			PX_set_inputencoding(pxdoc, inputencoding);
		if(tablename)
			PX_set_tablename(pxdoc, tablename);

		if((data = (char *) pxdoc->malloc(pxdoc, pxdoc->px_head->px_recordsize, _("Allocate memory for record data."))) == NULL) {
			fprintf(stderr, _("Could not allocate memory for record data."));
			fprintf(stderr, "\n");
			exit(1);
		}

		itmp = numfields;
		int linecounter = 1;
		while(itmp == numfields) {
			int offset;

			/* Read the data */
			if(0 > (itmp = csv_row_fread(infp, buffer, CSV_BUFSIZE, row, CSV_MAXCOLS, 1))) {
				fprintf(stderr, _("Could not read data line of csv input."));
				fprintf(stderr, "\n");
				exit(1);
			}

			/* if itmp ist 0, this is an indication for having reached the
			 * end of the file. In all other case the record has an unexpected
			 * number of fields, but can be saved. If there are more fields
			 * than numfields the will be skipped. If there are less, they will
			 * be set to NULL.
			 */
			if((itmp != numfields) && (itmp > 0)) {
				fprintf(stderr, _("Input data in line %d has not the expected number of columns. Expected %d got %d."), i+1, numfields, itmp);
				fprintf(stderr, "\n");
			}

			rowptr = row;
			i = 0;
			offset = 0;
			memset(data, 0, pxdoc->px_head->px_recordsize);
			while((*rowptr != NULL) && (i < numfields) && (i < itmp)) {
				char *fieldstr, *tmp;
				int len, needsize;
				switch(pxf[i].px_ftype) {
					case pxfShort: {
						short int value;
						value = atoi(*rowptr);
						PX_put_data_short(pxdoc, &data[offset], 2, value);
						offset += pxf[i].px_flen;
						break;
					}
					case pxfLong:
					case pxfAutoInc: {
						int value;
						value = atoi(*rowptr);
						PX_put_data_long(pxdoc, &data[offset], 4, value);
						offset += pxf[i].px_flen;
						break;
					}
					case pxfCurrency:
					case pxfNumber: {
						double value;
						sscanf(*rowptr, "%lf", &value);
						PX_put_data_double(pxdoc, &data[offset], 8, value);
						offset += pxf[i].px_flen;
						break;
					}
					case pxfAlpha: {
						char *value = *rowptr;
						if(strlen(value) > pxf[i].px_flen) {
							fprintf(stderr, "Field %d in line %d has been cut off.", i+1, linecounter);
							fprintf(stderr, "\n");
						}
						PX_put_data_alpha(pxdoc, &data[offset], pxf[i].px_flen, value);
						offset += pxf[i].px_flen;
						break;
					}
				}
				rowptr++;
				i++;
			}

			if((i > 0) && (0 > PX_put_record(pxdoc, data))) {
				fprintf(stderr, _("Could not write record number %d."), i+1);
				fprintf(stderr, "\n");
				PX_delete(pxdoc);
				exit(1);
			}

			linecounter++;
		}

		pxdoc->free(pxdoc, data);
		pxdoc->free(pxdoc, row);
		PX_close(pxdoc);
		PX_delete(pxdoc);
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
