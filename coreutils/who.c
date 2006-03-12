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
 *----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <utmp.h>
#include <sys/stat.h>
#include <time.h>
#include "busybox.h"

static const char * idle_string (time_t t)
{
	static char str[6];
	
	time_t s = time(NULL) - t;

	if (s < 60)
		return ".";
	if (s < (24 * 60 * 60)) {
		sprintf (str, "%02d:%02d",
				(int) (s / (60 * 60)),
				(int) ((s % (60 * 60)) / 60));
		return (const char *) str;
	}
	return "old";
}

int who_main(int argc, char **argv)
{
	struct utmp *ut;
	struct stat st;
	char *name;
	
	if (argc > 1) {
		bb_show_usage();
	}
	
	setutent();
	printf("USER       TTY      IDLE      TIME           HOST\n");
	while ((ut = getutent()) != NULL) {
		if (ut->ut_user[0] && ut->ut_type == USER_PROCESS) {
			/* ut->ut_line is device name of tty - "/dev/" */
			name = concat_path_file("/dev", ut->ut_line);
			printf("%-10s %-8s %-8s  %-12.12s   %s\n", ut->ut_user, ut->ut_line,
									(stat(name, &st)) ?  "?" : idle_string(st.st_atime),
									ctime((time_t*)&(ut->ut_tv.tv_sec)) + 4, ut->ut_host);
			free(name);
		}
	}
	endutent();
	return 0;
}
