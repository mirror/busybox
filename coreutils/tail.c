/* vi: set sw=4 ts=4: */
/*
 * Mini tail implementation for busybox
 *
 *
 * Copyright (C) 2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */


#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "busybox.h"

static const struct suffix_mult tail_suffixes[] = {
	{ "b", 512 },
	{ "k", 1024 },
	{ "m", 1048576 },
	{ NULL, 0 }
};

static const int BYTES = 0;
static const int LINES = 1;

static char *tailbuf;
static int taillen;
static int newline;

static void tailbuf_append(char *buf, int len)
{
	tailbuf = xrealloc(tailbuf, taillen + len);
	memcpy(tailbuf + taillen, buf, len);
	taillen += len;
}

static void tailbuf_trunc(void)
{
	char *s;
	s = memchr(tailbuf, '\n', taillen);
	memmove(tailbuf, s + 1, taillen - ((s + 1) - tailbuf));
	taillen -= (s + 1) - tailbuf;
	newline = 0;
}

int tail_main(int argc, char **argv)
{
	int from_top = 0, units = LINES, count = 10, sleep_period = 1;
	int show_headers = 0, hide_headers = 0, follow = 0;
	int *fds, nfiles = 0, status = EXIT_SUCCESS, nread, nwrite, seen = 0;
	char *s, *start, *end, buf[BUFSIZ];
	int i, opt;

	if (( argc >= 2 ) && ( argv [1][0] == '-' ) && isdigit ( argv [1][1] )) {
		count = atoi ( &argv [1][1] );
		optind = 2;
	}

	while ((opt = getopt(argc, argv, "c:fhn:q:s:v")) > 0) {
		switch (opt) {
			case 'f':
				follow = 1;
				break;
#ifdef CONFIG_FEATURE_FANCY_TAIL
			case 'c':
				units = BYTES;
				/* FALLS THROUGH */
#endif
			case 'n':
				count = parse_number(optarg, tail_suffixes);
				if (count < 0)
					count = -count;
				if (optarg[0] == '+')
					from_top = 1;
				break;
#ifdef CONFIG_FEATURE_FANCY_TAIL
			case 'q':
				hide_headers = 1;
				break;
			case 's':
				sleep_period = parse_number(optarg, 0);
				break;
			case 'v':
				show_headers = 1;
				break;
#endif
			default:
				show_usage();
		}
	}

	/* open all the files */
	fds = (int *)xmalloc(sizeof(int) * (argc - optind + 1));
	if (argc == optind) {
		fds[nfiles++] = STDIN_FILENO;
		argv[optind] = "standard input";
	} else {
		for (i = optind; i < argc; i++) {
			if (strcmp(argv[i], "-") == 0) {
				fds[nfiles++] = STDIN_FILENO;
				argv[i] = "standard input";
			} else if ((fds[nfiles++] = open(argv[i], O_RDONLY)) < 0) {
				perror_msg("%s", argv[i]);
				status = EXIT_FAILURE;
			}
		}
	}
	
#ifdef CONFIG_FEATURE_FANCY_TAIL
	/* tail the files */
	if (!from_top && units == BYTES)
		tailbuf = xmalloc(count);
#endif

	for (i = 0; i < nfiles; i++) {
		if (fds[i] == -1)
			continue;
		if (!count) {
			lseek(fds[i], 0, SEEK_END);
			continue;
		}
		seen = 0;
		if (show_headers || (!hide_headers && nfiles > 1))
			printf("%s==> %s <==\n", i == 0 ? "" : "\n", argv[optind + i]);
		while ((nread = safe_read(fds[i], buf, sizeof(buf))) > 0) {
			if (from_top) {
#ifdef CONFIG_FEATURE_FANCY_TAIL
				if (units == BYTES) {
					if (count - 1 <= seen)
						nwrite = nread;
					else if (count - 1 <= seen + nread)
						nwrite = nread + seen - (count - 1);
					else
						nwrite = 0;
					seen += nread;
				} else {
#else
				{
#endif
					if (count - 1 <= seen)
						nwrite = nread;
					else {
						nwrite = 0;
						for (s = memchr(buf, '\n', nread); s != NULL;
								s = memchr(s+1, '\n', nread - (s + 1 - buf))) {
							if (count - 1 <= ++seen) {
								nwrite = nread - (s + 1 - buf);
								break;
							}
						}
					}
				}
				if (full_write(STDOUT_FILENO, buf + nread - nwrite,
							nwrite) < 0) {
					perror_msg("write");
					status = EXIT_FAILURE;
					break;
				}
			} else {
#ifdef CONFIG_FEATURE_FANCY_TAIL
				if (units == BYTES) {
					if (nread < count) {
						memmove(tailbuf, tailbuf + nread, count - nread);
						memcpy(tailbuf + count - nread, buf, nread);
					} else {
						memcpy(tailbuf, buf + nread - count, count);
					}
					seen += nread;
				} else {
#else
				{
#endif
					for (start = buf, end = memchr(buf, '\n', nread);
							end != NULL; start = end+1,
							end = memchr(start, '\n', nread - (start - buf))) {
						if (newline && count <= seen)
							tailbuf_trunc();
						tailbuf_append(start, end - start + 1);
						seen++;
						newline = 1;
					}
					if (newline && count <= seen && nread - (start - buf) > 0)
						tailbuf_trunc();
					tailbuf_append(start, nread - (start - buf));
				}
			}
		}

		if (nread < 0) {
			perror_msg("read");
			status = EXIT_FAILURE;
		}

#ifdef CONFIG_FEATURE_FANCY_TAIL
		if (!from_top && units == BYTES) {
			if (count < seen)
				seen = count;
			if (full_write(STDOUT_FILENO, tailbuf + count - seen, seen) < 0) {
				perror_msg("write");
				status = EXIT_FAILURE;
			}
		}
#endif

		if (!from_top && units == LINES) {
			if (full_write(STDOUT_FILENO, tailbuf, taillen) < 0) {
				perror_msg("write");
				status = EXIT_FAILURE;
			}
		}

		taillen = 0;
	}

	while (follow) {
		sleep(sleep_period);

		for (i = 0; i < nfiles; i++) {
			if (fds[i] == -1)
				continue;

			if ((nread = safe_read(fds[i], buf, sizeof(buf))) > 0) {
				if (show_headers || (!hide_headers && nfiles > 1))
					printf("\n==> %s <==\n", argv[optind + i]);

				do {
					full_write(STDOUT_FILENO, buf, nread);
				} while ((nread = safe_read(fds[i], buf, sizeof(buf))) > 0);
			}

			if (nread < 0) {
				perror_msg("read");
				status = EXIT_FAILURE;
			}
		}
	}

	return status;
}
