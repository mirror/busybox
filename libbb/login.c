/*
 * issue.c: issue printing code
 *
 * Copyright (C) 2003 Bastian Blank <waldi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: login.c,v 1.2 2003/02/09 22:40:33 bug1 Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include "busybox.h"

#include <sys/utsname.h>
#include <time.h>

#define LOGIN " login: "

static char fmtstr_d[] = { "%A, %d %B %Y" };
static char fmtstr_t[] = { "%H:%M:%S" };

void print_login_issue(const char *issue_file, const char *tty)
{
	FILE *fd;
	int c;
	char buf[256];
	time_t t;
	struct utsname uts;

	time(&t);
	uname(&uts);

	puts("");	/* start a new line */

	if ((fd = fopen(issue_file, "r"))) {
		while ((c = fgetc(fd)) != EOF) {
			if (c == '\\' || c == '%') {
				c = fgetc(fd);

				switch (c) {
					case 's':
						fputs(uts.sysname, stdout);
						break;

					case 'n':
						fputs(uts.nodename, stdout);
						break;

					case 'r':
						fputs(uts.release, stdout);
						break;

					case 'v':
						fputs(uts.version, stdout);
						break;

					case 'm':
						fputs(uts.machine, stdout);
						break;

					case 'D':
					case 'o':
						getdomainname(buf, sizeof(buf));
						buf[sizeof(buf) - 1] = '\0';
						fputs(buf, stdout);
						break;

					case 'd':
						strftime(buf, sizeof(buf), fmtstr_d, localtime(&t));
						fputs(buf, stdout);
						break;

					case 't':
						strftime(buf, sizeof(buf), fmtstr_t, localtime(&t));
						fputs(buf, stdout);
						break;

					case 'h':
						gethostname(buf, sizeof(buf));
						fputs(buf, stdout);
						break;

					case 'l':
						printf("%s", tty);

					default:
						putchar(c);
				}
			} else
				putchar(c);
		}

		puts("");	/* start a new line */
		fflush(stdout);

		fclose(fd);
	}
}

void print_login_prompt(void)
{
	char buf[MAXHOSTNAMELEN];

	gethostname(buf, MAXHOSTNAMELEN);
	fputs(buf, stdout);

	fputs(LOGIN, stdout);
	fflush(stdout);
}

