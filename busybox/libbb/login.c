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
 * Optimize and correcting OCRNL by Vladimir Oleynik <dzo@simtreas.ru>
 */

#include <sys/param.h>  /* MAXHOSTNAMELEN */
#include <stdio.h>
#include <unistd.h>
#include "libbb.h"

#include <sys/utsname.h>
#include <time.h>

#define LOGIN " login: "

static const char fmtstr_d[] = "%A, %d %B %Y";
static const char fmtstr_t[] = "%H:%M:%S";

void print_login_issue(const char *issue_file, const char *tty)
{
	FILE *fd;
	int c;
	char buf[256];
	const char *outbuf;
	time_t t;
	struct utsname uts;

	time(&t);
	uname(&uts);

	puts("\r");	/* start a new line */

	if ((fd = fopen(issue_file, "r"))) {
		while ((c = fgetc(fd)) != EOF) {
			outbuf = buf;
			buf[0] = c;
			if(c == '\n') {
				buf[1] = '\r';
				buf[2] = 0;
			} else {
				buf[1] = 0;
			}
			if (c == '\\' || c == '%') {
				c = fgetc(fd);
				switch (c) {
					case 's':
						outbuf = uts.sysname;
						break;

					case 'n':
						outbuf = uts.nodename;
						break;

					case 'r':
						outbuf = uts.release;
						break;

					case 'v':
						outbuf = uts.version;
						break;

					case 'm':
						outbuf = uts.machine;
						break;

					case 'D':
					case 'o':
						getdomainname(buf, sizeof(buf));
						buf[sizeof(buf) - 1] = '\0';
						break;

					case 'd':
						strftime(buf, sizeof(buf), fmtstr_d, localtime(&t));
						break;

					case 't':
						strftime(buf, sizeof(buf), fmtstr_t, localtime(&t));
						break;

					case 'h':
						gethostname(buf, sizeof(buf) - 1);
						break;

					case 'l':
						outbuf = tty;
						break;

					default:
						buf[0] = c;
				}
		}
			fputs(outbuf, stdout);
		}

		fclose(fd);

		fflush(stdout);
	}
}

void print_login_prompt(void)
{
	char buf[MAXHOSTNAMELEN+1];

	gethostname(buf, MAXHOSTNAMELEN);
	fputs(buf, stdout);

	fputs(LOGIN, stdout);
	fflush(stdout);
}

