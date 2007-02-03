/* vi: set sw=4 ts=4: */
/*
 * Mini tail implementation for busybox
 *
 * Copyright (C) 2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant (need fancy for -c) */
/* BB_AUDIT GNU compatible -c, -q, and -v options in 'fancy' configuration. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/tail.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Pretty much rewritten to fix numerous bugs and reduce realloc() calls.
 * Bugs fixed (although I may have forgotten one or two... it was pretty bad)
 * 1) mixing printf/write without fflush()ing stdout
 * 2) no check that any open files are present
 * 3) optstring had -q taking an arg
 * 4) no error checking on write in some cases, and a warning even then
 * 5) q and s interaction bug
 * 6) no check for lseek error
 * 7) lseek attempted when count==0 even if arg was +0 (from top)
 */

#include "busybox.h"

static const struct suffix_mult tail_suffixes[] = {
	{ "b", 512 },
	{ "k", 1024 },
	{ "m", 1024*1024 },
	{ NULL, 0 }
};

static int status;

static void tail_xprint_header(const char *fmt, const char *filename)
{
	if (fdprintf(STDOUT_FILENO, fmt, filename) < 0)
		bb_perror_nomsg_and_die();
}

static ssize_t tail_read(int fd, char *buf, size_t count)
{
	ssize_t r;
	off_t current, end;
	struct stat sbuf;

	end = current = lseek(fd, 0, SEEK_CUR);
	if (!fstat(fd, &sbuf))
		end = sbuf.st_size;
	lseek(fd, end < current ? 0 : current, SEEK_SET);
	r = safe_read(fd, buf, count);
	if (r < 0) {
		bb_perror_msg(bb_msg_read_error);
		status = EXIT_FAILURE;
	}

	return r;
}

static const char header_fmt[] = "\n==> %s <==\n";

static unsigned eat_num(const char *p) {
	if (*p == '-') p++;
	else if (*p == '+') { p++; status = 1; }
	return xatou_sfx(p, tail_suffixes);
}

int tail_main(int argc, char **argv);
int tail_main(int argc, char **argv)
{
	unsigned count = 10;
	unsigned sleep_period = 1;
	bool from_top;
	int header_threshhold = 1;
	const char *str_c, *str_n, *str_s;

	char *tailbuf;
	size_t tailbufsize;
	int taillen = 0;
	int newline = 0;
	int nfiles, nread, nwrite, seen, i, opt;

	int *fds;
	char *s, *buf;
	const char *fmt;

#if ENABLE_INCLUDE_SUSv2 || ENABLE_FEATURE_FANCY_TAIL
	/* Allow legacy syntax of an initial numeric option without -n. */
	if (argc >= 2 && (argv[1][0] == '+' || argv[1][0] == '-')
	 && isdigit(argv[1][1])
	) {
		argv[0] = (char*)"-n";
		argv--;
		argc++;
	}
#endif

	opt = getopt32(argc, argv, "fc:n:" USE_FEATURE_FANCY_TAIL("qs:v"), &str_c, &str_n, &str_s);
#define FOLLOW (opt & 0x1)
#define COUNT_BYTES (opt & 0x2)
	//if (opt & 0x1) // -f
	if (opt & 0x2) count = eat_num(str_c); // -c
	if (opt & 0x4) count = eat_num(str_n); // -n
#if ENABLE_FEATURE_FANCY_TAIL
	if (opt & 0x8) header_threshhold = INT_MAX; // -q
	if (opt & 0x10) sleep_period = xatou(str_s); // -s
	if (opt & 0x20) header_threshhold = 0; // -v
#endif
	argc -= optind;
	argv += optind;
	from_top = status;

	/* open all the files */
	fds = xmalloc(sizeof(int) * (argc + 1));
	status = nfiles = i = 0;
	if (argc == 0) {
		struct stat statbuf;

		if (!fstat(STDIN_FILENO, &statbuf) && S_ISFIFO(statbuf.st_mode)) {
			opt &= ~1; /* clear FOLLOW */
		}
		*argv = (char *) bb_msg_standard_input;
		goto DO_STDIN;
	}

	do {
		if (NOT_LONE_DASH(argv[i])) {
			fds[nfiles] = open(argv[i], O_RDONLY);
			if (fds[nfiles] < 0) {
				bb_perror_msg("%s", argv[i]);
				status = EXIT_FAILURE;
				continue;
			}
		} else {
 DO_STDIN:		/* "-" */
			fds[nfiles] = STDIN_FILENO;
		}
		argv[nfiles] = argv[i];
		++nfiles;
	} while (++i < argc);

	if (!nfiles)
		bb_error_msg_and_die("no files");

	tailbufsize = BUFSIZ;

	/* tail the files */
	if (!from_top && COUNT_BYTES) {
		if (tailbufsize < count) {
			tailbufsize = count + BUFSIZ;
		}
	}

	buf = tailbuf = xmalloc(tailbufsize);

	fmt = header_fmt + 1;	/* Skip header leading newline on first output. */
	i = 0;
	do {
		/* Be careful.  It would be possible to optimize the count-bytes
		 * case if the file is seekable.  If you do though, remember that
		 * starting file position may not be the beginning of the file.
		 * Beware of backing up too far.  See example in wc.c.
		 */
		if (!(count | from_top) && lseek(fds[i], 0, SEEK_END) >= 0) {
			continue;
		}

		if (nfiles > header_threshhold) {
			tail_xprint_header(fmt, argv[i]);
			fmt = header_fmt;
		}

		buf = tailbuf;
		taillen = 0;
		seen = 1;
		newline = 0;

		while ((nread = tail_read(fds[i], buf, tailbufsize-taillen)) > 0) {
			if (from_top) {
				nwrite = nread;
				if (seen < count) {
					if (COUNT_BYTES) {
						nwrite -= (count - seen);
						seen = count;
					} else {
						s = buf;
						do {
							--nwrite;
							if (*s++ == '\n' && ++seen == count) {
								break;
							}
						} while (nwrite);
					}
				}
				xwrite(STDOUT_FILENO, buf + nread - nwrite, nwrite);
			} else if (count) {
				if (COUNT_BYTES) {
					taillen += nread;
					if (taillen > count) {
						memmove(tailbuf, tailbuf + taillen - count, count);
						taillen = count;
					}
				} else {
					int k = nread;
					int nbuf = 0;

					while (k) {
						--k;
						if (buf[k] == '\n') {
							++nbuf;
						}
					}

					if (newline + nbuf < count) {
						newline += nbuf;
						taillen += nread;

					} else {
						int extra = 0;
						if (buf[nread-1] != '\n') {
							extra = 1;
						}

						k = newline + nbuf + extra - count;
						s = tailbuf;
						while (k) {
							if (*s == '\n') {
								--k;
							}
							++s;
						}

						taillen += nread - (s - tailbuf);
						memmove(tailbuf, s, taillen);
						newline = count - extra;
					}
					if (tailbufsize < taillen + BUFSIZ) {
						tailbufsize = taillen + BUFSIZ;
						tailbuf = xrealloc(tailbuf, tailbufsize);
					}
				}
				buf = tailbuf + taillen;
			}
		}

		if (!from_top) {
			xwrite(STDOUT_FILENO, tailbuf, taillen);
		}

		taillen = 0;
	} while (++i < nfiles);

	buf = xrealloc(tailbuf, BUFSIZ);

	fmt = NULL;

	if (FOLLOW) while (1) {
		sleep(sleep_period);
		i = 0;
		do {
			if (nfiles > header_threshhold) {
				fmt = header_fmt;
			}
			while ((nread = tail_read(fds[i], buf, sizeof(buf))) > 0) {
				if (fmt) {
					tail_xprint_header(fmt, argv[i]);
					fmt = NULL;
				}
				xwrite(STDOUT_FILENO, buf, nread);
			}
		} while (++i < nfiles);
	}

	return status;
}
