/* vi: set sw=4 ts=4: */
/*
 * Mini ps implementation(s) for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
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
#if ENABLE_SELINUX
#include <selinux/selinux.h>  /* for is_selinux_enabled()  */
#endif

#define TERMINAL_WIDTH 80

extern int ps_main(int argc, char **argv)
{
	procps_status_t * p;
	int i, len, terminal_width;
#if ENABLE_SELINUX
	int use_selinux = 0;
	security_context_t sid=NULL;
#endif

	get_terminal_width_height(0, &terminal_width, NULL);

#if ENABLE_FEATURE_PS_WIDE || ENABLE_SELINUX
	/* handle arguments */
	/* bb_getopt_ulflags(argc, argv,) would force a leading dash */
	for (len = 1; len < argc; len++) {
		char *c = argv[len];
		while (*c) {
			if (ENABLE_FEATURE_PS_WIDE && *c == 'w')
				/* if w is given once, GNU ps sets the width to 132,
				 * if w is given more than once, it is "unlimited"
				 */
				terminal_width =
					(terminal_width==TERMINAL_WIDTH) ? 132 : INT_MAX;
#if ENABLE_SELINUX
			if (*c == 'c' && is_selinux_enabled())
				use_selinux = 1;
#endif
			c++;
		}
	}
#endif

	/* Go one less... */
	terminal_width--;
#if ENABLE_SELINUX
	if (use_selinux)
	  printf("  PID Context                          Stat Command\n");
	else
#endif
	  printf("  PID  Uid     VmSize Stat Command\n");

	while ((p = procps_scan(1)) != 0)  {
		char *namecmd = p->cmd;
#if ENABLE_SELINUX
		if (use_selinux )
		  {
			char sbuf[128];
			len = sizeof(sbuf);

			if (is_selinux_enabled()) {
			  if (getpidcon(p->pid,&sid)<0)
			    sid=NULL;
			}

			if (sid) {
			  /*  I assume sid initilized with NULL  */
			  len = strlen(sid)+1;
			  safe_strncpy(sbuf, sid, len);
			  freecon(sid);
			  sid=NULL;
			}else {
			  safe_strncpy(sbuf, "unknown",7);
			}
			len = printf("%5d %-32s %s ", p->pid, sbuf, p->state);
		}
		else
#endif
		  if(p->rss == 0)
		    len = printf("%5d %-8s        %s ", p->pid, p->user, p->state);
		  else
		    len = printf("%5d %-8s %6ld %s ", p->pid, p->user, p->rss, p->state);

		i = terminal_width-len;

		if(namecmd && namecmd[0]) {
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
		/* no check needed, but to make valgrind happy..  */
		if (ENABLE_FEATURE_CLEAN_UP && p->cmd)
			free(p->cmd);
	}
	return EXIT_SUCCESS;
}

