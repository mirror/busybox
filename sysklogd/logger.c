/* vi: set sw=4 ts=4: */
/*
 * Mini logger implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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

#include "internal.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#if !defined BB_SYSLOGD

#define SYSLOG_NAMES
#include <sys/syslog.h>

#else
/* We have to do this since the header file defines static
 * structues.  Argh.... bad libc, bad, bad...
 */
#include <sys/syslog.h>
typedef struct _code {
	char *c_name;
	int c_val;
} CODE;
extern CODE prioritynames[];
extern CODE facilitynames[];
#endif

/* Decode a symbolic name to a numeric value 
 * this function is based on code
 * Copyright (c) 1983, 1993
 * The Regents of the University of California.  All rights reserved.
 */
static int decode(char *name, CODE * codetab)
{
	CODE *c;

	if (isdigit(*name))
		return (atoi(name));
	for (c = codetab; c->c_name; c++) {
		if (!strcasecmp(name, c->c_name)) {
			return (c->c_val);
		}
	}

	return (-1);
}

/* Decode a symbolic name to a numeric value 
 * this function is based on code
 * Copyright (c) 1983, 1993
 * The Regents of the University of California.  All rights reserved.
 */
static int pencode(char *s)
{
	char *save;
	int lev, fac = LOG_USER;

	for (save = s; *s && *s != '.'; ++s);
	if (*s) {
		*s = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0) {
			errorMsg("unknown facility name: %s\n", save);
			exit(FALSE);
		}
		*s++ = '.';
	} else {
		s = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0) {
		errorMsg("unknown priority name: %s\n", save);
		exit(FALSE);
	}
	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}


extern int logger_main(int argc, char **argv)
{
	int pri = LOG_USER | LOG_NOTICE;
	int option = 0;
	int fromStdinFlag = FALSE;
	int stopLookingAtMeLikeThat = FALSE;
	char *message=NULL, buf[1024], name[128];

	/* Fill out the name string early (may be overwritten later */
	my_getpwuid(name, geteuid());

	/* Parse any options */
	while (--argc > 0 && **(++argv) == '-') {
		if (*((*argv) + 1) == '\0') {
			fromStdinFlag = TRUE;
		}
		stopLookingAtMeLikeThat = FALSE;
		while (*(++(*argv)) && stopLookingAtMeLikeThat == FALSE) {
			switch (**argv) {
			case 's':
				option |= LOG_PERROR;
				break;
			case 'p':
				if (--argc == 0) {
					usage(logger_usage);
				}
				pri = pencode(*(++argv));
				stopLookingAtMeLikeThat = TRUE;
				break;
			case 't':
				if (--argc == 0) {
					usage(logger_usage);
				}
				strncpy(name, *(++argv), sizeof(name));
				stopLookingAtMeLikeThat = TRUE;
				break;
			default:
				usage(logger_usage);
			}
		}
	}

	if (fromStdinFlag == TRUE) {
		/* read from stdin */
		int c;
		unsigned int i = 0;

		while ((c = getc(stdin)) != EOF && i < sizeof(buf)) {
			buf[i++] = c;
		}
		message = buf;
	} else {
		if (argc >= 1) {
			message = *argv;
		} else {
			errorMsg("No message\n");
			exit(FALSE);
		}
	}

	openlog(name, option, (pri | LOG_FACMASK));
	syslog(pri, message);
	closelog();

	return(TRUE);
}
