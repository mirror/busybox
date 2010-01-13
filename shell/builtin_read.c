/* vi: set sw=4 ts=4: */
/*
 * Adapted from ash applet code
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 * Copyright (c) 2010 Denys Vlasenko
 * Split from ash.c
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
#include "libbb.h"
#include "shell_common.h"
#include "builtin_read.h"

//TODO: use more efficient setvar() which takes a pointer to malloced "VAR=VAL"
//string. hush naturally has it, and ash has setvareq().
//Here we can simply store "VAR=" at buffer start and store read data directly
//after "=", then pass buffer to setvar() to consume.

const char* FAST_FUNC
shell_builtin_read(void FAST_FUNC (*setvar)(const char *name, const char *val),
	char       **argv,
	const char *ifs,
	int        read_flags,
	const char *opt_n,
	const char *opt_p,
	const char *opt_t,
	const char *opt_u
)
{
	unsigned end_ms; /* -t TIMEOUT */
	int fd; /* -u FD */
	int nchars; /* -n NUM */
	char **pp;
	char *buffer;
	struct termios tty, old_tty;
	const char *retval;
	int bufpos; /* need to be able to hold -1 */
	int startword;
	smallint backslash;

	pp = argv;
	while (*pp) {
		if (!is_well_formed_var_name(*pp, '\0')) {
			/* Mimic bash message */
			bb_error_msg("read: '%s': not a valid identifier", *pp);
			return (const char *)(uintptr_t)1;
		}
		pp++;
	}

	nchars = 0; /* if != 0, -n is in effect */
	if (opt_n) {
		nchars = bb_strtou(opt_n, NULL, 10);
		if (nchars < 0 || errno)
			return "invalid count";
		/* note: "-n 0": off (bash 3.2 does this too) */
	}
	end_ms = 0;
	if (opt_t) {
		end_ms = bb_strtou(opt_t, NULL, 10);
		if (errno || end_ms > UINT_MAX / 2048)
			return "invalid timeout";
		end_ms *= 1000;
#if 0 /* even bash has no -t N.NNN support */
		ts.tv_sec = bb_strtou(opt_t, &p, 10);
		ts.tv_usec = 0;
		/* EINVAL means number is ok, but not terminated by NUL */
		if (*p == '.' && errno == EINVAL) {
			char *p2;
			if (*++p) {
				int scale;
				ts.tv_usec = bb_strtou(p, &p2, 10);
				if (errno)
					return "invalid timeout";
				scale = p2 - p;
				/* normalize to usec */
				if (scale > 6)
					return "invalid timeout";
				while (scale++ < 6)
					ts.tv_usec *= 10;
			}
		} else if (ts.tv_sec < 0 || errno) {
			return "invalid timeout";
		}
		if (!(ts.tv_sec | ts.tv_usec)) { /* both are 0? */
			return "invalid timeout";
		}
#endif /* if 0 */
	}
	fd = STDIN_FILENO;
	if (opt_u) {
		fd = bb_strtou(opt_u, NULL, 10);
		if (fd < 0 || errno)
			return "invalid file descriptor";
	}

	if (opt_p && isatty(fd)) {
		fputs(opt_p, stderr);
		fflush_all();
	}

	if (ifs == NULL)
		ifs = defifs;

	if (nchars || (read_flags & BUILTIN_READ_SILENT)) {
		tcgetattr(fd, &tty);
		old_tty = tty;
		if (nchars) {
			tty.c_lflag &= ~ICANON;
			tty.c_cc[VMIN] = nchars < 256 ? nchars : 255;
		}
		if (read_flags & BUILTIN_READ_SILENT) {
			tty.c_lflag &= ~(ECHO | ECHOK | ECHONL);
		}
		/* This forces execution of "restoring" tcgetattr later */
		read_flags |= BUILTIN_READ_SILENT;
		/* if tcgetattr failed, tcsetattr will fail too.
		 * Ignoring, it's harmless. */
		tcsetattr(fd, TCSANOW, &tty);
	}

	retval = (const char *)(uintptr_t)0;
	startword = 1;
	backslash = 0;
	if (end_ms) /* NB: end_ms stays nonzero: */
		end_ms = ((unsigned)monotonic_ms() + end_ms) | 1;
	buffer = NULL;
	bufpos = 0;
	do {
		char c;

		if (end_ms) {
			int timeout;
			struct pollfd pfd[1];

			pfd[0].fd = fd;
			pfd[0].events = POLLIN;
			timeout = end_ms - (unsigned)monotonic_ms();
			if (timeout <= 0 /* already late? */
			 || safe_poll(pfd, 1, timeout) != 1 /* no? wait... */
			) { /* timed out! */
				retval = (const char *)(uintptr_t)1;
				goto ret;
			}
		}

		if ((bufpos & 0xff) == 0)
			buffer = xrealloc(buffer, bufpos + 0x100);
		if (nonblock_safe_read(fd, &buffer[bufpos], 1) != 1) {
			retval = (const char *)(uintptr_t)1;
			break;
		}
		c = buffer[bufpos];
		if (c == '\0')
			continue;
		if (backslash) {
			backslash = 0;
			if (c != '\n')
				goto put;
			continue;
		}
		if (!(read_flags & BUILTIN_READ_RAW) && c == '\\') {
			backslash = 1;
			continue;
		}
		if (c == '\n')
			break;

		/* $IFS splitting. NOT done if we run "read"
		 * without variable names (bash compat).
		 * Thus, "read" and "read REPLY" are not the same.
		 */
		if (argv[0]) {
/* http://www.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06_05 */
			const char *is_ifs = strchr(ifs, c);
			if (startword && is_ifs) {
				if (isspace(c))
					continue;
				/* it is a non-space ifs char */
				startword--;
				if (startword == 1) /* first one? */
					continue; /* yes, it is not next word yet */
			}
			startword = 0;
			if (argv[1] != NULL && is_ifs) {
				buffer[bufpos] = '\0';
				bufpos = 0;
				setvar(*argv, buffer);
				argv++;
				/* can we skip one non-space ifs char? (2: yes) */
				startword = isspace(c) ? 2 : 1;
				continue;
			}
		}
 put:
		bufpos++;
	} while (--nchars);

	if (argv[0]) {
		/* Remove trailing space $IFS chars */
		while (--bufpos >= 0 && isspace(buffer[bufpos]) && strchr(ifs, buffer[bufpos]) != NULL)
			continue;
		buffer[bufpos + 1] = '\0';
		/* Use the remainder as a value for the next variable */
		setvar(*argv, buffer);
		/* Set the rest to "" */
		while (*++argv)
			setvar(*argv, "");
	} else {
		/* Note: no $IFS removal */
		buffer[bufpos] = '\0';
		setvar("REPLY", buffer);
	}

 ret:
	free(buffer);
	if (read_flags & BUILTIN_READ_SILENT)
		tcsetattr(fd, TCSANOW, &old_tty);
	return retval;
}
