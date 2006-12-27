/* vi: set sw=4 ts=4: */
/*
 *  Copyright (C) 2000 by Glenn McGrath
 *
 *  based on the function base64_encode from http.c in wget v1.6
 *  Copyright (C) 1995, 1996, 1997, 1998, 2000 Free Software Foundation, Inc.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"


#define SRC_BUF_SIZE	45  // This *MUST* be a multiple of 3
#define DST_BUF_SIZE    4 * ((SRC_BUF_SIZE + 2) / 3)
int uuencode_main(int argc, char **argv)
{
	const size_t src_buf_size = SRC_BUF_SIZE;
	const size_t dst_buf_size = DST_BUF_SIZE;
	size_t write_size = dst_buf_size;
	struct stat stat_buf;
	FILE *src_stream = stdin;
	const char *tbl;
	size_t size;
	mode_t mode;
	RESERVE_CONFIG_BUFFER(src_buf, SRC_BUF_SIZE + 1);
	RESERVE_CONFIG_BUFFER(dst_buf, DST_BUF_SIZE + 1);

	tbl = bb_uuenc_tbl_std;
	if (getopt32(argc, argv, "m") & 1) {
		tbl = bb_uuenc_tbl_base64;
	}

	switch (argc - optind) {
		case 2:
			src_stream = xfopen(argv[optind], "r");
			xstat(argv[optind], &stat_buf);
			mode = stat_buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
			if (src_stream == stdout) {
				puts("NULL");
			}
			break;
		case 1:
			mode = 0666 & ~umask(0666);
			break;
		default:
			bb_show_usage();
	}

	printf("begin%s %o %s", tbl == bb_uuenc_tbl_std ? "" : "-base64", mode, argv[argc - 1]);

	while ((size = fread(src_buf, 1, src_buf_size, src_stream)) > 0) {
		if (size != src_buf_size) {
			/* write_size is always 60 until the last line */
			write_size = (4 * ((size + 2) / 3));
			/* pad with 0s so we can just encode extra bits */
			memset(&src_buf[size], 0, src_buf_size - size);
		}
		/* Encode the buffer we just read in */
		bb_uuencode((unsigned char*)src_buf, dst_buf, size, tbl);

		putchar('\n');
		if (tbl == bb_uuenc_tbl_std) {
			putchar(tbl[size]);
		}
		if (fwrite(dst_buf, 1, write_size, stdout) != write_size) {
			bb_perror_msg_and_die(bb_msg_write_error);
		}
	}
	printf(tbl == bb_uuenc_tbl_std ? "\n`\nend\n" : "\n====\n");

	die_if_ferror(src_stream, "source");	/* TODO - Fix this! */

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
