/* vi: set sw=4 ts=4: */
/*
 *  Copyright 2003, Glenn McGrath <bug1@iinet.net.au>
 *
 *  Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 *  Based on specification from
 *  http://www.opengroup.org/onlinepubs/007904975/utilities/uuencode.html
 *
 *  Bugs: the spec doesn't mention anything about "`\n`\n" prior to the
 *        "end" line
 */


#include "busybox.h"

static void read_stduu(FILE *src_stream, FILE *dst_stream)
{
	char *line;

	while ((line = xmalloc_getline(src_stream)) != NULL) {
		int length;
		char *line_ptr = line;

		if (strcmp(line, "end") == 0) {
			return;
		}
		length = ((*line_ptr - 0x20) & 0x3f)* 4 / 3;

		if (length <= 0) {
			/* Ignore the "`\n" line, why is it even in the encode file ? */
			continue;
		}
		if (length > 60) {
			bb_error_msg_and_die("line too long");
		}

		line_ptr++;
		/* Tolerate an overly long line to accomodate a possible exta '`' */
		if (strlen(line_ptr) < (size_t)length) {
			bb_error_msg_and_die("short file");
		}

		while (length > 0) {
			/* Merge four 6 bit chars to three 8 bit chars */
			fputc(((line_ptr[0] - 0x20) & 077) << 2 | ((line_ptr[1] - 0x20) & 077) >> 4, dst_stream);
			line_ptr++;
			length--;
			if (length == 0) {
				break;
			}

			fputc(((line_ptr[0] - 0x20) & 077) << 4 | ((line_ptr[1] - 0x20) & 077) >> 2, dst_stream);
			line_ptr++;
			length--;
			if (length == 0) {
				break;
			}

			fputc(((line_ptr[0] - 0x20) & 077) << 6 | ((line_ptr[1] - 0x20) & 077), dst_stream);
			line_ptr += 2;
			length -= 2;
		}
		free(line);
	}
	bb_error_msg_and_die("short file");
}

static void read_base64(FILE *src_stream, FILE *dst_stream)
{
	static const char base64_table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n";
	int term_count = 0;

	while (1) {
		char translated[4];
		int count = 0;

		while (count < 4) {
			char *table_ptr;
			int ch;

			/* Get next _valid_ character */
			do {
				ch = fgetc(src_stream);
				if (ch == EOF) {
					bb_error_msg_and_die("short file");
				}
			} while ((table_ptr = strchr(base64_table, ch)) == NULL);

			/* Convert encoded charcter to decimal */
			ch = table_ptr - base64_table;

			if (*table_ptr == '=') {
				if (term_count == 0) {
					translated[count] = 0;
					break;
				}
				term_count++;
			}
			else if (*table_ptr == '\n') {
				/* Check for terminating line */
				if (term_count == 5) {
					return;
				}
				term_count = 1;
				continue;
			} else {
				translated[count] = ch;
				count++;
				term_count = 0;
			}
		}

		/* Merge 6 bit chars to 8 bit */
	    fputc(translated[0] << 2 | translated[1] >> 4, dst_stream);
		if (count > 2) {
			fputc(translated[1] << 4 | translated[2] >> 2, dst_stream);
		}
		if (count > 3) {
			fputc(translated[2] << 6 | translated[3], dst_stream);
		}
	}
}

int uudecode_main(int argc, char **argv);
int uudecode_main(int argc, char **argv)
{
	FILE *src_stream;
	char *outname = NULL;
	char *line;

	getopt32(argc, argv, "o:", &outname);

	if (optind == argc) {
		src_stream = stdin;
	} else if (optind + 1 == argc) {
		src_stream = xfopen(argv[optind], "r");
	} else {
		bb_show_usage();
	}

	/* Search for the start of the encoding */
	while ((line = xmalloc_getline(src_stream)) != NULL) {
		void (*decode_fn_ptr)(FILE * src, FILE * dst);
		char *line_ptr;
		FILE *dst_stream;
		int mode;

		if (strncmp(line, "begin-base64 ", 13) == 0) {
			line_ptr = line + 13;
			decode_fn_ptr = read_base64;
		} else if (strncmp(line, "begin ", 6) == 0) {
			line_ptr = line + 6;
			decode_fn_ptr = read_stduu;
		} else {
			free(line);
			continue;
		}

		mode = strtoul(line_ptr, NULL, 8);
		if (outname == NULL) {
			outname = strchr(line_ptr, ' ');
			if ((outname == NULL) || (*outname == '\0')) {
				break;
			}
			outname++;
		}
		if (LONE_DASH(outname)) {
			dst_stream = stdout;
		} else {
			dst_stream = xfopen(outname, "w");
			chmod(outname, mode & (S_IRWXU | S_IRWXG | S_IRWXO));
		}
		free(line);
		decode_fn_ptr(src_stream, dst_stream);
		fclose_if_not_stdin(src_stream);
		return EXIT_SUCCESS;
	}
	bb_error_msg_and_die("no 'begin' line");
}
