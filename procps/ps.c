/* vi: set sw=4 ts=4: */
/*
 * Mini ps implementation(s) for busybox
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "busybox.h"
#ifdef CONFIG_SELINUX
#include <fs_secure.h>
#include <ss.h>
#include <flask_util.h>          /* for is_flask_enabled() */
#endif

static const int TERMINAL_WIDTH = 79;      /* not 80 in case terminal has linefold bug */



extern int ps_main(int argc, char **argv)
{
	procps_status_t * p;
	int i, len;
	int terminal_width = TERMINAL_WIDTH;

#ifdef CONFIG_SELINUX
	int use_selinux = 0;
	security_id_t sid;
	if(is_flask_enabled() && argv[1] && !strcmp(argv[1], "-c") )
		use_selinux = 1;
#endif

	get_terminal_width_height(0, &terminal_width, NULL);
	/* Go one less... */
	terminal_width--;

#ifdef CONFIG_SELINUX
	if(use_selinux)
		printf("  PID Context                          Stat Command\n");
	else
#endif
	printf("  PID  Uid     VmSize Stat Command\n");
#ifdef CONFIG_SELINUX
	while ((p = procps_scan(1, use_selinux, &sid)) != 0) {
#else
	while ((p = procps_scan(1)) != 0) {
#endif
		char *namecmd = p->cmd;

#ifdef CONFIG_SELINUX
		if(use_selinux)
		{
			char sbuf[128];
			len = sizeof(sbuf);
			if(security_sid_to_context(sid, (security_context_t)&sbuf, &len))
				strcpy(sbuf, "unknown");

			len = printf("%5d %-32s %s ", p->pid, sbuf, p->state);
		}
		else
#endif
		if(p->rss == 0)
			len = printf("%5d %-8s        %s ", p->pid, p->user, p->state);
		else
			len = printf("%5d %-8s %6ld %s ", p->pid, p->user, p->rss, p->state);
		i = terminal_width-len;

		if(namecmd != 0 && namecmd[0] != 0) {
			if(i < 0)
		i = 0;
			if(strlen(namecmd) > i)
				namecmd[i] = 0;
			printf("%s\n", namecmd);
		} else {
			namecmd = p->short_cmd;
			if(i < 2)
				i = 2;
			if(strlen(namecmd) > (i-2))
				namecmd[i-2] = 0;
			printf("[%s]\n", namecmd);
		}
		free(p->cmd);
	}
	return EXIT_SUCCESS;
}

