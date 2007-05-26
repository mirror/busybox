/* vi: set sw=4 ts=4: */
/*
 * Mini dd implementation for busybox
 *
 *
 * Copyright (C) 2000,2001  Matt Kraai
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <signal.h>  /* For FEATURE_DD_SIGNAL_HANDLING */
#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */


static const struct suffix_mult dd_suffixes[] = {
	{ "c", 1 },
	{ "w", 2 },
	{ "b", 512 },
	{ "kD", 1000 },
	{ "k", 1024 },
	{ "K", 1024 },	/* compat with coreutils dd */
	{ "MD", 1000000 },
	{ "M", 1048576 },
	{ "GD", 1000000000 },
	{ "G", 1073741824 },
	{ NULL, 0 }
};

struct globals {
	off_t out_full, out_part, in_full, in_part;
};
#define G (*(struct globals*)&bb_common_bufsiz1)

static void dd_output_status(int ATTRIBUTE_UNUSED cur_signal)
{
	fprintf(stderr, "%"OFF_FMT"d+%"OFF_FMT"d records in\n"
			"%"OFF_FMT"d+%"OFF_FMT"d records out\n",
			G.in_full, G.in_part,
			G.out_full, G.out_part);
}

static ssize_t full_write_or_warn(int fd, const void *buf, size_t len,
	const char * const filename)
{
	ssize_t n = full_write(fd, buf, len);
	if (n < 0)
		bb_perror_msg("writing '%s'", filename);
	return n;
}

static bool write_and_stats(int fd, const void *buf, size_t len, size_t obs,
	const char * const filename)
{
	ssize_t n = full_write_or_warn(fd, buf, len, filename);
	if (n < 0)
		return 1;
	if (n == obs)
		G.out_full++;
	else if (n) /* > 0 */
		G.out_part++;
	return 0;
}

#if ENABLE_LFS
#define XATOU_SFX xatoull_sfx
#else
#define XATOU_SFX xatoul_sfx
#endif

int dd_main(int argc, char **argv);
int dd_main(int argc, char **argv)
{
	enum {
		SYNC_FLAG    = 1 << 0,
		NOERROR      = 1 << 1,
		TRUNC_FLAG   = 1 << 2,
		TWOBUFS_FLAG = 1 << 3,
	};
	static const char * const keywords[] = {
		"bs=", "count=", "seek=", "skip=", "if=", "of=",
#if ENABLE_FEATURE_DD_IBS_OBS
		"ibs=", "obs=", "conv=", "notrunc", "sync", "noerror",
#endif
		NULL
	};
	enum {
		OP_bs = 1,
		OP_count,
		OP_seek,
		OP_skip,
		OP_if,
		OP_of,
#if ENABLE_FEATURE_DD_IBS_OBS
		OP_ibs,
		OP_obs,
		OP_conv,
		OP_conv_notrunc,
		OP_conv_sync,
		OP_conv_noerror,
#endif
	};
	int flags = TRUNC_FLAG;
	size_t oc = 0, ibs = 512, obs = 512;
	ssize_t n, w;
	off_t seek = 0, skip = 0, count = OFF_T_MAX;
	int ifd, ofd;
	const char *infile = NULL, *outfile = NULL;
	char *ibuf, *obuf;

	memset(&G, 0, sizeof(G)); /* because of NOEXEC */

	if (ENABLE_FEATURE_DD_SIGNAL_HANDLING) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = dd_output_status;
		sa.sa_flags = SA_RESTART;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGUSR1, &sa, 0);
	}

	for (n = 1; n < argc; n++) {
		smalluint key_len;
		smalluint what;
		char *key;
		char *arg = argv[n];

//XXX:FIXME: we reject plain "dd --" This would cost ~20 bytes, so..
//if (*arg == '-' && *++arg == '-' && !*++arg) continue;
		key = strstr(arg, "=");
		if (key == NULL)
			bb_show_usage();
		key_len = key - arg + 1;
		key = xstrndup(arg, key_len);
		what = index_in_str_array(keywords, key) + 1;
		if (ENABLE_FEATURE_CLEAN_UP)
			free(key);
		if (what == 0)
			bb_show_usage();
		arg += key_len;
		/* Must fit into positive ssize_t */
#if ENABLE_FEATURE_DD_IBS_OBS
			if (what == OP_ibs) {
				ibs = xatoul_range_sfx(arg, 1, ((size_t)-1L)/2, dd_suffixes);
				continue;
			}
			if (what == OP_obs) {
				obs = xatoul_range_sfx(arg, 1, ((size_t)-1L)/2, dd_suffixes);
				continue;
			}
			if (what == OP_conv) {
				while (1) {
					/* find ',', replace them with nil so we can use arg for
					 * index_in_str_array without copying.
					 * We rely on arg being non-null, else strchr would fault.
					 */
					key = strchr(arg, ',');
					if (key)
						*key = '\0';
					what = index_in_str_array(keywords, arg) + 1;
					if (what < OP_conv_notrunc)
						bb_error_msg_and_die(bb_msg_invalid_arg, arg, "conv");
					if (what == OP_conv_notrunc)
						flags &= ~TRUNC_FLAG;
					if (what == OP_conv_sync)
						flags |= SYNC_FLAG;
					if (what == OP_conv_noerror)
						flags |= NOERROR;
					if (!key) /* no ',' left, so this was the last specifier */
						break;
					arg = key + 1; /* skip this keyword and ',' */
				}
				continue;
			}
#endif
		if (what == OP_bs) {
			ibs = obs = xatoul_range_sfx(arg, 1, ((size_t)-1L)/2, dd_suffixes);
			continue;
		}
		/* These can be large: */
		if (what == OP_count) {
			count = XATOU_SFX(arg, dd_suffixes);
			continue;
		}
		if (what == OP_seek) {
			seek = XATOU_SFX(arg, dd_suffixes);
			continue;
		}
		if (what == OP_skip) {
			skip = XATOU_SFX(arg, dd_suffixes);
			continue;
		}
		if (what == OP_if) {
			infile = arg;
			continue;
		}
		if (what == OP_of)
			outfile = arg;
	}
//XXX:FIXME for huge ibs or obs, malloc'ing them isn't the brightest idea ever
	ibuf = obuf = xmalloc(ibs);
	if (ibs != obs) {
		flags |= TWOBUFS_FLAG;
		obuf = xmalloc(obs);
	}
	if (infile != NULL)
		ifd = xopen(infile, O_RDONLY);
	else {
		ifd = STDIN_FILENO;
		infile = bb_msg_standard_input;
	}
	if (outfile != NULL) {
		int oflag = O_WRONLY | O_CREAT;

		if (!seek && (flags & TRUNC_FLAG))
			oflag |= O_TRUNC;

		ofd = xopen(outfile, oflag);

		if (seek && (flags & TRUNC_FLAG)) {
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
					goto die_infile;
				if (n == 0)
					break;
			}
		}
	}
	if (seek) {
		if (lseek(ofd, seek * obs, SEEK_CUR) < 0)
			goto die_outfile;
	}

	while (G.in_full + G.in_part != count) {
		if (flags & NOERROR) /* Pre-zero the buffer when for NOERROR */
			memset(ibuf, '\0', ibs);
		n = safe_read(ifd, ibuf, ibs);
		if (n == 0)
			break;
		if (n < 0) {
			if (flags & NOERROR) {
				n = ibs;
				bb_perror_msg("%s", infile);
			} else
				goto die_infile;
		}
		if ((size_t)n == ibs)
			G.in_full++;
		else {
			G.in_part++;
			if (flags & SYNC_FLAG) {
				memset(ibuf + n, '\0', ibs - n);
				n = ibs;
			}
		}
		if (flags & TWOBUFS_FLAG) {
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
					if (write_and_stats(ofd, obuf, obs, obs, outfile))
						goto out_status;
					oc = 0;
				}
			}
		} else if (write_and_stats(ofd, ibuf, n, obs, outfile))
			goto out_status;
	}

	if (ENABLE_FEATURE_DD_IBS_OBS && oc) {
		w = full_write_or_warn(ofd, obuf, oc, outfile);
		if (w < 0) goto out_status;
		if (w > 0)
			G.out_part++;
	}
	if (close(ifd) < 0) {
die_infile:
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
