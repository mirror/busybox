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
	{ "K", 1024 },	// compat with coreutils dd
	{ "MD", 1000000 },
	{ "M", 1048576 },
	{ "GD", 1000000000 },
	{ "G", 1073741824 },
	{ NULL, 0 }
};

static off_t out_full, out_part, in_full, in_part;

static void dd_output_status(int ATTRIBUTE_UNUSED cur_signal)
{
	fprintf(stderr, "%"OFF_FMT"d+%"OFF_FMT"d records in\n"
			"%"OFF_FMT"d+%"OFF_FMT"d records out\n",
			in_full, in_part,
			out_full, out_part);
}

static ssize_t full_write_or_warn(int fd, const void *buf, size_t len,
		const char* filename)
{
	ssize_t n = full_write(fd, buf, len);
	if (n < 0)
		bb_perror_msg("writing '%s'", filename);
	return n;
}

#if ENABLE_LFS
#define XATOU_SFX xatoull_sfx
#else
#define XATOU_SFX xatoul_sfx
#endif

int dd_main(int argc, char **argv)
{
	enum {
		sync_flag    = 1 << 0,
		noerror      = 1 << 1,
		trunc_flag   = 1 << 2,
		twobufs_flag = 1 << 3,
	};
	int flags = trunc_flag;
	size_t oc = 0, ibs = 512, obs = 512;
	ssize_t n, w;
	off_t seek = 0, skip = 0, count = OFF_T_MAX;
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
		char *arg = argv[n];
		/* Must fit into positive ssize_t */
		if (ENABLE_FEATURE_DD_IBS_OBS && !strncmp("ibs=", arg, 4))
			ibs = xatoul_range_sfx(arg+4, 0, ((size_t)-1L)/2, dd_suffixes);
		else if (ENABLE_FEATURE_DD_IBS_OBS && !strncmp("obs=", arg, 4))
			obs = xatoul_range_sfx(arg+4, 0, ((size_t)-1L)/2, dd_suffixes);
		else if (!strncmp("bs=", arg, 3))
			ibs = obs = xatoul_range_sfx(arg+3, 0, ((size_t)-1L)/2, dd_suffixes);
		/* These can be large: */
		else if (!strncmp("count=", arg, 6))
			count = XATOU_SFX(arg+6, dd_suffixes);
		else if (!strncmp("seek=", arg, 5))
			seek = XATOU_SFX(arg+5, dd_suffixes);
		else if (!strncmp("skip=", arg, 5))
			skip = XATOU_SFX(arg+5, dd_suffixes);

		else if (!strncmp("if=", arg, 3))
			infile = arg+3;
		else if (!strncmp("of=", arg, 3))
			outfile = arg+3;
		else if (ENABLE_FEATURE_DD_IBS_OBS && !strncmp("conv=", arg, 5)) {
			arg += 5;
			while (1) {
				if (!strncmp("notrunc", arg, 7)) {
					flags &= ~trunc_flag;
					arg += 7;
				} else if (!strncmp("sync", arg, 4)) {
					flags |= sync_flag;
					arg += 4;
				} else if (!strncmp("noerror", arg, 7)) {
					flags |= noerror;
					arg += 7;
				} else {
					bb_error_msg_and_die(bb_msg_invalid_arg, arg, "conv");
				}
				if (arg[0] == '\0') break;
				if (*arg++ != ',') bb_show_usage();
			}
		} else
			bb_show_usage();
	}

	ibuf = obuf = xmalloc(ibs);
	if (ibs != obs) {
		flags |= twobufs_flag;
		obuf = xmalloc(obs);
	}

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

		ofd = xopen(outfile, oflag);

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
			if (flags & sync_flag) {
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
					w = full_write_or_warn(ofd, obuf, obs, outfile);
					if (w < 0) goto out_status;
					if (w == obs)
						out_full++;
					else if (w > 0)
						out_part++;
					oc = 0;
				}
			}
		} else {
			w = full_write_or_warn(ofd, ibuf, n, outfile);
			if (w < 0) goto out_status;
			if (w == obs)
				out_full++;
			else if (w > 0)
				out_part++;
		}
	}

	if (ENABLE_FEATURE_DD_IBS_OBS && oc) {
		w = full_write_or_warn(ofd, obuf, oc, outfile);
		if (w < 0) goto out_status;
		if (w > 0)
			out_part++;
	}
	if (close(ifd) < 0) {
		bb_perror_msg_and_die("%s", infile);
	}

	if (close(ofd) < 0) {
 die_outfile:
		bb_perror_msg_and_die("%s", outfile);
	}
 out_status:
	dd_output_status(0);

	return EXIT_SUCCESS;
}
