/* vi: set sw=4 ts=4: */
/*
 * issue.c: issue printing code
 *
 * Copyright (C) 2003 Bastian Blank <waldi@tuxbox.org>
 *
 * Optimize and correcting OCRNL by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
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
	char buf[256+1];
	const char *outbuf;
	time_t t;
	struct utsname uts;

	time(&t);
	uname(&uts);

	puts("\r");	/* start a new line */

	fd = fopen(issue_file, "r");
	if (!fd)
		return;
	while ((c = fgetc(fd)) != EOF) {
		outbuf = buf;
		buf[0] = c;
		buf[1] = '\0';
		if(c == '\n') {
			buf[1] = '\r';
			buf[2] = '\0';
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
				c = getdomainname(buf, sizeof(buf) - 1);
				buf[c >= 0 ? c : 0] = '\0';
				break;
			case 'd':
				strftime(buf, sizeof(buf), fmtstr_d, localtime(&t));
				break;
			case 't':
				strftime(buf, sizeof(buf), fmtstr_t, localtime(&t));
				break;
			case 'h':
				gethostname(buf, sizeof(buf) - 1);
				buf[sizeof(buf) - 1] = '\0';
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

void print_login_prompt(void)
{
	char buf[MAXHOSTNAMELEN+1];

	if (gethostname(buf, MAXHOSTNAMELEN) == 0)
		fputs(buf, stdout);

	fputs(LOGIN, stdout);
	fflush(stdout);
}
