/* vi: set sw=4 ts=4: */
/*----------------------------------------------------------------------
 * Mini who is used to display user name, login time,
 * idle time and host name.
 *
 * Author: Da Chen  <dchen@ayrnetworks.com>
 *
 * This is a free document; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation:
 *    http://www.gnu.org/copyleft/gpl.html
 *
 * Copyright (c) 2002 AYR Networks, Inc.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 *----------------------------------------------------------------------
 */
/* BB_AUDIT SUSv3 _NOT_ compliant -- missing options -b, -d, -H, -l, -m, -p, -q, -r, -s, -t, -T, -u; Missing argument 'file'.  */

#include "libbb.h"
#include <utmp.h>
#include <time.h>

static void idle_string(char *str6, time_t t)
{
	t = time(NULL) - t;

	/*if (t < 60) {
		str6[0] = '.';
		str6[1] = '\0';
		return;
	}*/
	if (t >= 0 && t < (24 * 60 * 60)) {
		sprintf(str6, "%02d:%02d",
				(int) (t / (60 * 60)),
				(int) ((t % (60 * 60)) / 60));
		return;
	}
	strcpy(str6, "old");
}

int who_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int who_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	char str6[6];
	struct utmp *ut;
	struct stat st;
	char *name;
	unsigned opt;

	opt_complementary = "=0";
	opt = getopt32(argv, "a");

	setutent();
	printf("USER       TTY      IDLE      TIME            HOST\n");
	while ((ut = getutent()) != NULL) {
		if (ut->ut_user[0] && (opt || ut->ut_type == USER_PROCESS)) {
			time_t tmp;
			/* ut->ut_line is device name of tty - "/dev/" */
			name = concat_path_file("/dev", ut->ut_line);
			str6[0] = '?';
			str6[1] = '\0';
			if (stat(name, &st) == 0)
				idle_string(str6, st.st_atime);
			/* manpages say ut_tv.tv_sec *is* time_t,
			 * but some systems have it wrong */
			tmp = ut->ut_tv.tv_sec;
			/* 15 chars for time:   Nov 10 19:33:20 */
			printf("%-10s %-8s %-9s %-15.15s %s\n",
					ut->ut_user, ut->ut_line, str6,
					ctime(&tmp) + 4, ut->ut_host);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(name);
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		endutent();
	return EXIT_SUCCESS;
}
