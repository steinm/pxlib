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
	char *outputfile = NULL;
	int c, i;
	int verbose = 0;
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
			{"verbose", 0, 0, 'v'},
			{"recode", 1, 0, 'r'},
			{"output", 1, 0, 'o'},
			{"help", 0, 0, 'h'},
			{"separator", 1, 0, 0},
			{"enclosure", 1, 0, 1},
			{"fields", 1, 0, 'f'},
			{"tablename", 1, 0, 3},
			{"mode", 1, 0, 4},
			{0, 0, 0, 0}
		};
		c = getopt_long (argc, argv, "vtf:r:o:h",
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
			case 't':
				modetest = 1;
				break;
			case 'r':
				targetencoding = strdup(optarg);
				break;
			case 4:
				if(!strcmp(optarg, "test")) {
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

		for(i=-1000; i<10; i++) {
			char buffer[30];
			PX_put_data_short(pxdoc, &data[0], 2, i);
			PX_put_data_short(pxdoc, &data[2], 2, -23);
			PX_put_data_long(pxdoc, &data[4], 4, i*2);
			PX_put_data_double(pxdoc, &data[8], 8, i*1.0001);
			snprintf(buffer, 30, "---------- Nummer %d", i);
			PX_put_data_alpha(pxdoc, &data[16], 20, buffer);
			if(0 > PX_put_record(pxdoc, data)) {
				fprintf(stderr, _("Could not write record."));
				fprintf(stderr, "\n");
				exit(1);
			}
		}
		PX_close(pxdoc);
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
