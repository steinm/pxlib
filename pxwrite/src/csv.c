/* csv - read write comma separated value format
 * Copyright (c) 2003 Michael B. Allen <mballen@erols.com>
 *
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "msgno.h"

#define ST_START     1
#define ST_COLLECT   2
#define ST_TAILSPACE 3
#define ST_END_QUOTE 4

struct input {
	FILE *in;
	const char *src;
	size_t sn;
	size_t count;
};

static int
nextch(struct input *in)
{
	int ch;
	if (in->in) {
		if ((ch = fgetc(in->in)) == EOF) {
			if (ferror(in->in)) {
				PMNO(errno);
				return -1;
			}
			return 0;
		}
	} else {
		if (in->sn == 0) {
			return 0;
		}
		ch = *(in->src)++;
		in->sn--;
	}
	in->count++;
	return ch;
}

static int
csv_parse(struct input *in, char *buf, size_t bn, char *row[], int rn, int trim)
{
	int ch, state, r, j, t, inquotes;

	state = ST_START;
	inquotes = 0;
	ch = r = j = t = 0;

	memset(row, 0, sizeof(char *) * rn);

	while (rn && bn && (ch = nextch(in)) > 0) {
		switch (state) {
			case ST_START:
				if (ch != '\n' && isspace(ch)) {
					if (!trim) {
						buf[j++] = ch; bn--;
						t = j;
					}
					break;
				} else if (ch == '"') {
					j = t = 0;
					state = ST_COLLECT;
					inquotes = 1;
					break;
				}
				state = ST_COLLECT;
			case ST_COLLECT:
				if (inquotes) {
					if (ch == '"') {
						state = ST_END_QUOTE;
						break;
					}
				} else if (ch == ',' || ch == '\n') {
					row[r++] = buf; rn--;
					buf[t] = '\0'; bn--;
					buf += t + 1;
					j = t = 0;
					state = ST_START;
					inquotes = 0;
					if (ch == '\n') {
						rn = 0;
					}
					break;
				} else if (ch == '"') {
					errno = EILSEQ;
					PMNF(errno, ": unexpected quote in element %d", (r + 1));
					return -1;
				}
				buf[j++] = ch; bn--;
				if (!trim || isspace(ch) == 0) {
					t = j;
				}
				break;
			case ST_TAILSPACE:
			case ST_END_QUOTE:
				if (ch == ',' || ch == '\n') {
					row[r++] = buf; rn--;
					buf[j] = '\0'; bn--;
					buf += j + 1;
					j = t =  0;
					state = ST_START;
					inquotes = 0;
					if (ch == '\n') {
						rn = 0;
					}
					break;
				} else if (ch == '"' && state != ST_TAILSPACE) {
					buf[j++] = '"';	bn--;		 /* nope, just an escaped quote */
					t = j;
					state = ST_COLLECT;
					break;
				} else if (isspace(ch)) {
					state = ST_TAILSPACE;
					break;
				}
				errno = EILSEQ;
				PMNF(errno, ": bad end quote in element %d", (r + 1));
				return -1;
		}
	}
	if (ch == -1) {
		AMSG("");
		return -1;
	}
	if (bn == 0) {
		errno = E2BIG;
		PMNO(errno);
		return -1;
	}
	if (rn) {
		if (inquotes) {
			errno = EILSEQ;
			PMNO(errno);
			return -1;
		}
		row[r] = buf;
		buf[t] = '\0';
	}

//	return in->count;
	return r;
}
int
csv_row_parse(const char *src, size_t sn, char *buf, size_t bn, char *row[], int rn, int trim)
{
	struct input input;
	input.in = NULL;
	input.src = src;
	input.sn = sn;
	input.count = 0;
	return csv_parse(&input, buf, bn, row, rn, trim);
}
int
csv_row_fread(FILE *in, char *buf, size_t bn, char *row[], int rn, int trim)
{
	struct input input;
	input.in = in;
	input.count = 0;
	return csv_parse(&input, buf, bn, row, rn, trim);
}

