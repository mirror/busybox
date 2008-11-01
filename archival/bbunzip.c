/* vi: set sw=4 ts=4: */
/*
 *  Common code for gunzip-like applets
 *
 *  Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

enum {
	OPT_STDOUT = 0x1,
	OPT_FORCE = 0x2,
/* gunzip and bunzip2 only: */
	OPT_VERBOSE = 0x4,
	OPT_DECOMPRESS = 0x8,
	OPT_TEST = 0x10,
};

static
int open_to_or_warn(int to_fd, const char *filename, int flags, int mode)
{
	int fd = open3_or_warn(filename, flags, mode);
	if (fd < 0) {
		return 1;
	}
	xmove_fd(fd, to_fd);
	return 0;
}

int FAST_FUNC bbunpack(char **argv,
	char* (*make_new_name)(char *filename),
	USE_DESKTOP(long long) int (*unpacker)(unpack_info_t *info)
)
{
	struct stat stat_buf;
	USE_DESKTOP(long long) int status;
	char *filename, *new_name;
	smallint exitcode = 0;
	unpack_info_t info;

	do {
		/* NB: new_name is *maybe* malloc'ed! */
		new_name = NULL;
		filename = *argv; /* can be NULL - 'streaming' bunzip2 */

		if (filename && LONE_DASH(filename))
			filename = NULL;

		/* Open src */
		if (filename) {
			if (stat(filename, &stat_buf) != 0) {
				bb_simple_perror_msg(filename);
 err:
				exitcode = 1;
				goto free_name;
			}
			if (open_to_or_warn(STDIN_FILENO, filename, O_RDONLY, 0))
				goto err;
		}

		/* Special cases: test, stdout */
		if (option_mask32 & (OPT_STDOUT|OPT_TEST)) {
			if (option_mask32 & OPT_TEST)
				if (open_to_or_warn(STDOUT_FILENO, bb_dev_null, O_WRONLY, 0))
					goto err;
			filename = NULL;
		}

		/* Open dst if we are going to unpack to file */
		if (filename) {
			new_name = make_new_name(filename);
			if (!new_name) {
				bb_error_msg("%s: unknown suffix - ignored", filename);
				goto err;
			}

			/* -f: overwrite existing output files */
			if (option_mask32 & OPT_FORCE) {
				unlink(new_name);
			}

			/* O_EXCL: "real" bunzip2 doesn't overwrite files */
			/* GNU gunzip does not bail out, but goes to next file */
			if (open_to_or_warn(STDOUT_FILENO, new_name, O_WRONLY | O_CREAT | O_EXCL,
					stat_buf.st_mode))
				goto err;
		}

		/* Check that the input is sane */
		if (isatty(STDIN_FILENO) && (option_mask32 & OPT_FORCE) == 0) {
			bb_error_msg_and_die("compressed data not read from terminal, "
					"use -f to force it");
		}

		/* memset(&info, 0, sizeof(info)); */
		info.mtime = 0; /* so far it has one member only */
		status = unpacker(&info);
		if (status < 0)
			exitcode = 1;

		if (filename) {
			char *del = new_name;
			if (status >= 0) {
				/* TODO: restore other things? */
				if (info.mtime) {
					struct utimbuf times;

					times.actime = info.mtime;
					times.modtime = info.mtime;
					/* Close first.
					 * On some systems calling utime
					 * then closing resets the mtime. */
					close(STDOUT_FILENO);
					/* Ignoring errors */
					utime(new_name, &times);
				}

				/* Delete _compressed_ file */
				del = filename;
				/* restore extension (unless tgz -> tar case) */
				if (new_name == filename)
					filename[strlen(filename)] = '.';
			}
			xunlink(del);

#if 0 /* Currently buggy - wrong name: "a.gz: 261% - replaced with a.gz" */
			/* Extreme bloat for gunzip compat */
			if (ENABLE_DESKTOP && (option_mask32 & OPT_VERBOSE) && status >= 0) {
				fprintf(stderr, "%s: %u%% - replaced with %s\n",
					filename, (unsigned)(stat_buf.st_size*100 / (status+1)), new_name);
			}
#endif

 free_name:
			if (new_name != filename)
				free(new_name);
		}
	} while (*argv && *++argv);

	return exitcode;
}

#if ENABLE_BUNZIP2 || ENABLE_UNLZMA || ENABLE_UNCOMPRESS

static
char* make_new_name_generic(char *filename, const char *expected_ext)
{
	char *extension = strrchr(filename, '.');
	if (!extension || strcmp(extension + 1, expected_ext) != 0) {
		/* Mimic GNU gunzip - "real" bunzip2 tries to */
		/* unpack file anyway, to file.out */
		return NULL;
	}
	*extension = '\0';
	return filename;
}

#endif


/*
 *  Modified for busybox by Glenn McGrath
 *  Added support output to stdout by Thomas Lundquist <thomasez@zelow.no>
 *
 *  Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#if ENABLE_BUNZIP2

static
char* make_new_name_bunzip2(char *filename)
{
	return make_new_name_generic(filename, "bz2");
}

static
USE_DESKTOP(long long) int unpack_bunzip2(unpack_info_t *info UNUSED_PARAM)
{
	return unpack_bz2_stream_prime(STDIN_FILENO, STDOUT_FILENO);
}

int bunzip2_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bunzip2_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "cfvdt");
	argv += optind;
	if (applet_name[2] == 'c')
		option_mask32 |= OPT_STDOUT;

	return bbunpack(argv, make_new_name_bunzip2, unpack_bunzip2);
}

#endif


/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersen@codepoet.org> to support files as
 * well as stdin/stdout, and to generally behave itself wrt command line
 * handling.
 *
 * General cleanup to better adhere to the style guide and make use of standard
 * busybox functions by Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * gzip (GNU zip) -- compress files with zip algorithm and 'compress' interface
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * The unzip code was written and put in the public domain by Mark Adler.
 * Portions of the lzw code are derived from the public domain 'compress'
 * written by Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
 *
 * See the license_msg below and the file COPYING for the software license.
 * See the file algorithm.doc for the compression algorithms and file formats.
 */

#if ENABLE_GUNZIP

static
char* make_new_name_gunzip(char *filename)
{
	char *extension = strrchr(filename, '.');

	if (!extension)
		return NULL;

	extension++;
	if (strcmp(extension, "tgz" + 1) == 0
#if ENABLE_FEATURE_SEAMLESS_Z
	 || (extension[0] == 'Z' && extension[1] == '\0')
#endif
	) {
		extension[-1] = '\0';
	} else if (strcmp(extension, "tgz") == 0) {
		filename = xstrdup(filename);
		extension = strrchr(filename, '.');
		extension[2] = 'a';
		extension[3] = 'r';
	} else {
		return NULL;
	}
	return filename;
}

static
USE_DESKTOP(long long) int unpack_gunzip(unpack_info_t *info)
{
	USE_DESKTOP(long long) int status = -1;

	/* do the decompression, and cleanup */
	if (xread_char(STDIN_FILENO) == 0x1f) {
		unsigned char magic2;

		magic2 = xread_char(STDIN_FILENO);
		if (ENABLE_FEATURE_SEAMLESS_Z && magic2 == 0x9d) {
			status = unpack_Z_stream(STDIN_FILENO, STDOUT_FILENO);
		} else if (magic2 == 0x8b) {
			status = unpack_gz_stream_with_info(STDIN_FILENO, STDOUT_FILENO, info);
		} else {
			goto bad_magic;
		}
		if (status < 0) {
			bb_error_msg("error inflating");
		}
	} else {
 bad_magic:
		bb_error_msg("invalid magic");
		/* status is still == -1 */
	}
	return status;
}

/*
 * Linux kernel build uses gzip -d -n. We accept and ignore it.
 * Man page says:
 * -n --no-name
 * gzip: do not save the original file name and time stamp.
 * (The original name is always saved if the name had to be truncated.)
 * gunzip: do not restore the original file name/time even if present
 * (remove only the gzip suffix from the compressed file name).
 * This option is the default when decompressing.
 * -N --name
 * gzip: always save the original file name and time stamp (this is the default)
 * gunzip: restore the original file name and time stamp if present.
 */

int gunzip_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int gunzip_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "cfvdtn");
	argv += optind;
	/* if called as zcat */
	if (applet_name[1] == 'c')
		option_mask32 |= OPT_STDOUT;

	return bbunpack(argv, make_new_name_gunzip, unpack_gunzip);
}

#endif


/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Based on bunzip.c from busybox
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 */

#if ENABLE_UNLZMA

static
char* make_new_name_unlzma(char *filename)
{
	return make_new_name_generic(filename, "lzma");
}

static
USE_DESKTOP(long long) int unpack_unlzma(unpack_info_t *info UNUSED_PARAM)
{
	return unpack_lzma_stream(STDIN_FILENO, STDOUT_FILENO);
}

int unlzma_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int unlzma_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "cf");
	argv += optind;
	/* lzmacat? */
	if (applet_name[4] == 'c')
		option_mask32 |= OPT_STDOUT;

	return bbunpack(argv, make_new_name_unlzma, unpack_unlzma);
}

#endif


/*
 *	Uncompress applet for busybox (c) 2002 Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#if ENABLE_UNCOMPRESS

static
char* make_new_name_uncompress(char *filename)
{
	return make_new_name_generic(filename, "Z");
}

static
USE_DESKTOP(long long) int unpack_uncompress(unpack_info_t *info UNUSED_PARAM)
{
	USE_DESKTOP(long long) int status = -1;

	if ((xread_char(STDIN_FILENO) != 0x1f) || (xread_char(STDIN_FILENO) != 0x9d)) {
		bb_error_msg("invalid magic");
	} else {
		status = unpack_Z_stream(STDIN_FILENO, STDOUT_FILENO);
	}
	return status;
}

int uncompress_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uncompress_main(int argc UNUSED_PARAM, char **argv)
{
	getopt32(argv, "cf");
	argv += optind;

	return bbunpack(argv, make_new_name_uncompress, unpack_uncompress);
}

#endif
