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

#include "busybox.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>

#if !defined BB_SYSLOGD

#define SYSLOG_NAMES
#include <sys/syslog.h>

#else
/* We have to do this since the header file defines static
 * structures.  Argh.... bad libc, bad, bad...
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
 *  
 * Original copyright notice is retained at the end of this file.
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
 *
 * Original copyright notice is retained at the end of this file.
 */
static int pencode(char *s)
{
	char *save;
	int lev, fac = LOG_USER;

	for (save = s; *s && *s != '.'; ++s);
	if (*s) {
		*s = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0)
			error_msg_and_die("unknown facility name: %s\n", save);
		*s++ = '.';
	} else {
		s = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0)
		error_msg_and_die("unknown priority name: %s\n", save);
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
			int len = 1; /* for the '\0' */
			for (; *argv != NULL; argv++) {
				len += strlen(*argv);
				len += 1;  /* for the space between the args */
				message = xrealloc(message, len);
				strcat(message, *argv);
				strcat(message, " ");
			}
			message[strlen(message)-1] = '\0';
		} else {
			error_msg_and_die("No message\n");
		}
	}

	openlog(name, option, (pri | LOG_FACMASK));
	syslog(pri, "%s", message);
	closelog();

	return EXIT_SUCCESS;
}


/*-
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This is the original license statement for the decode and pencode functions.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. <BSD Advertising Clause omitted per the July 22, 1999 licensing change 
 *		ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change> 
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */



