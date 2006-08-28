/* vi: set sw=4 ts=4: */
/*
 * Mini dd implementation for busybox
 *
 *
 * Copyright (C) 2000,2001  Matt Kraai
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <signal.h>  /* For FEATURE_DD_SIGNAL_HANDLING */

static const struct suffix_mult dd_suffixes[] = {
	{ "c", 1 },
	{ "w", 2 },
	{ "b", 512 },
	{ "kD", 1000 },
	{ "k", 1024 },
	{ "MD", 1000000 },
	{ "M", 1048576 },
	{ "GD", 1000000000 },
	{ "G", 1073741824 },
	{ NULL, 0 }
};

static size_t out_full, out_part, in_full, in_part;

static void dd_output_status(int ATTRIBUTE_UNUSED cur_signal)
{
	bb_fprintf(stderr, "%ld+%ld records in\n%ld+%ld records out\n",
			(long)in_full, (long)in_part,
			(long)out_full, (long)out_part);
}

int dd_main(int argc, char **argv)
{
#define sync_flag	(1<<0)
#define noerror		(1<<1)
#define trunc_flag	(1<<2)
#define twobufs_flag (1<<3)
	int flags = trunc_flag;
	size_t count = -1, oc = 0, ibs = 512, obs = 512;
	ssize_t n;
	off_t seek = 0, skip = 0;
	int oflag, ifd, ofd;
	const char *infile = NULL, *outfile = NULL;
	char *ibuf, *obuf;

	if (ENABLE_FEATURE_DD_SIGNAL_HANDLING) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = dd_output_status;
		sa.sa_flags = SA_RESTART;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGUSR1, &sa, 0);
	}

	for (n = 1; n < argc; n++) {
		if (ENABLE_FEATURE_DD_IBS_OBS && !strncmp("ibs=", argv[n], 4)) {
			ibs = bb_xparse_number(argv[n]+4, dd_suffixes);
			flags |= twobufs_flag;
		} else if (ENABLE_FEATURE_DD_IBS_OBS && !strncmp("obs=", argv[n], 4)) {
			obs = bb_xparse_number(argv[n]+4, dd_suffixes);
			flags |= twobufs_flag;
		} else if (!strncmp("bs=", argv[n], 3))
			ibs = obs = bb_xparse_number(argv[n]+3, dd_suffixes);
		else if (!strncmp("count=", argv[n], 6))
			count = bb_xparse_number(argv[n]+6, dd_suffixes);
		else if (!strncmp("seek=", argv[n], 5))
			seek = bb_xparse_number(argv[n]+5, dd_suffixes);
		else if (!strncmp("skip=", argv[n], 5))
			skip = bb_xparse_number(argv[n]+5, dd_suffixes);
		else if (!strncmp("if=", argv[n], 3))
			infile = argv[n]+3;
		else if (!strncmp("of=", argv[n], 3))
			outfile = argv[n]+3;
		else if (ENABLE_FEATURE_DD_IBS_OBS && !strncmp("conv=", argv[n], 5)) {
			ibuf = argv[n]+5;
			while (1) {
				if (!strncmp("notrunc", ibuf, 7)) {
					flags ^= trunc_flag;
					ibuf += 7;
				} else if (!strncmp("sync", ibuf, 4)) {
					flags |= sync_flag;
					ibuf += 4;
				} else if (!strncmp("noerror", ibuf, 7)) {
					flags |= noerror;
					ibuf += 7;
				} else {
					bb_error_msg_and_die(bb_msg_invalid_arg, argv[n]+5, "conv");
				}
				if (ibuf[0] == '\0') break;
				if (ibuf[0] == ',') ibuf++;
			}
		} else
			bb_show_usage();
	}
	ibuf = xmalloc(ibs);

	if (flags & twobufs_flag)
		obuf = xmalloc(obs);
	else
		obuf = ibuf;

	if (infile != NULL)
		ifd = xopen(infile, O_RDONLY);
	else {
		ifd = STDIN_FILENO;
		infile = bb_msg_standard_input;
	}

	if (outfile != NULL) {
		oflag = O_WRONLY | O_CREAT;

		if (!seek && (flags & trunc_flag))
			oflag |= O_TRUNC;

		ofd = xopen3(outfile, oflag, 0666);

		if (seek && (flags & trunc_flag)) {
			if (ftruncate(ofd, seek * obs) < 0) {
				struct stat st;

				if (fstat(ofd, &st) < 0 || S_ISREG(st.st_mode) ||
						S_ISDIR(st.st_mode))
					goto die_outfile;
			}
		}
	} else {
		ofd = STDOUT_FILENO;
		outfile = bb_msg_standard_output;
	}

	if (skip) {
		if (lseek(ifd, skip * ibs, SEEK_CUR) < 0) {
			while (skip-- > 0) {
				n = safe_read(ifd, ibuf, ibs);
				if (n < 0)
					bb_perror_msg_and_die("%s", infile);
				if (n == 0)
					break;
			}
		}
	}

	if (seek) {
		if (lseek(ofd, seek * obs, SEEK_CUR) < 0)
			goto die_outfile;
	}

	while (in_full + in_part != count) {
		if (flags & noerror) {
			/* Pre-zero the buffer when doing the noerror thing */
			memset(ibuf, '\0', ibs);
		}

		n = safe_read(ifd, ibuf, ibs);
		if (n == 0)
			break;
		if (n < 0) {
			if (flags & noerror) {
				n = ibs;
				bb_perror_msg("%s", infile);
			} else
				bb_perror_msg_and_die("%s", infile);
		}
		if ((size_t)n == ibs)
			in_full++;
		else {
			in_part++;
			if (sync_flag) {
				memset(ibuf + n, '\0', ibs - n);
				n = ibs;
			}
		}
		if (flags & twobufs_flag) {
			char *tmp = ibuf;
			while (n) {
				size_t d = obs - oc;

				if (d > n)
					d = n;
				memcpy(obuf + oc, tmp, d);
				n -= d;
				tmp += d;
				oc += d;
				if (oc == obs) {
					xwrite(ofd, obuf, obs);
					out_full++;
					oc = 0;
				}
			}
		} else {
			xwrite(ofd, ibuf, n);
			if (n == ibs)
				out_full++;
			else
				out_part++;
		}
	}
	
	if (ENABLE_FEATURE_DD_IBS_OBS && oc) {
		xwrite(ofd, obuf, oc);
		out_part++;
	}
	if (close (ifd) < 0) {
		bb_perror_msg_and_die("%s", infile);
	}

	if (close (ofd) < 0) {
die_outfile:
		bb_perror_msg_and_die("%s", outfile);
	}

	dd_output_status(0);

	return EXIT_SUCCESS;
}
