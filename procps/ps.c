/* vi: set sw=4 ts=4: */
/*
 * Mini ps implementation(s) for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */

#include "busybox.h"

int ps_main(int argc, char **argv)
{
	procps_status_t * p;
	int i, len;
	SKIP_SELINUX(const) int use_selinux = 0;
	USE_SELINUX(security_context_t sid = NULL;)
#if !ENABLE_FEATURE_PS_WIDE
	enum { terminal_width = 79 };
#else
	int terminal_width;
	int w_count = 0;
#endif

#if ENABLE_FEATURE_PS_WIDE || ENABLE_SELINUX
#if ENABLE_FEATURE_PS_WIDE
	opt_complementary = "-:ww";
	USE_SELINUX(i =) getopt32(argc, argv, "w" USE_SELINUX("c"), &w_count);
	/* if w is given once, GNU ps sets the width to 132,
	 * if w is given more than once, it is "unlimited"
	 */
	if (w_count) {
		terminal_width = (w_count==1) ? 132 : INT_MAX;
	} else {
		get_terminal_width_height(1, &terminal_width, NULL);
		/* Go one less... */
		terminal_width--;
	}
#else /* only ENABLE_SELINUX */
	i = getopt32(argc, argv, "c");
#endif
#if ENABLE_SELINUX
	if ((i & (1+ENABLE_FEATURE_PS_WIDE)) && is_selinux_enabled())
		use_selinux = 1;
#endif
#endif /* ENABLE_FEATURE_PS_WIDE || ENABLE_SELINUX */

	if (use_selinux)
		puts("  PID Context                          Stat Command");
	else
		puts("  PID  Uid     VmSize Stat Command");

	while ((p = procps_scan(1)) != 0)  {
		char *namecmd = p->cmd;
#if ENABLE_SELINUX
		if (use_selinux) {
			char sbuf[128];
			len = sizeof(sbuf);

			if (is_selinux_enabled()) {
				if (getpidcon(p->pid, &sid) < 0)
					sid = NULL;
			}

			if (sid) {
				/* I assume sid initialized with NULL */
				len = strlen(sid)+1;
				safe_strncpy(sbuf, sid, len);
				freecon(sid);
				sid = NULL;
			} else {
				safe_strncpy(sbuf, "unknown", 7);
			}
			len = printf("%5u %-32s %s ", (unsigned)p->pid, sbuf, p->state);
		} else
#endif
			if (p->rss == 0)
				len = printf("%5u %-8s        %s ", (unsigned)p->pid, p->user, p->state);
			else
				len = printf("%5u %-8s %6ld %s ", (unsigned)p->pid, p->user, p->rss, p->state);

		i = terminal_width-len;

		if (namecmd && namecmd[0]) {
			if (i < 0)
				i = 0;
			if (strlen(namecmd) > (size_t)i)
				namecmd[i] = 0;
			puts(namecmd);
		} else {
			namecmd = p->short_cmd;
			if (i < 2)
				i = 2;
			if (strlen(namecmd) > ((size_t)i-2))
				namecmd[i-2] = 0;
			printf("[%s]\n", namecmd);
		}
		/* no check needed, but to make valgrind happy.. */
		if (ENABLE_FEATURE_CLEAN_UP && p->cmd)
			free(p->cmd);
	}
	return EXIT_SUCCESS;
}
