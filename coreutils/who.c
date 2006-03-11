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

int who_main(int argc, char **argv)
{
    struct utmp *ut;
    struct stat st;
    time_t     idle;
    char *name;

    if (argc > 1)
	bb_show_usage();

    setutent();
    printf("USER       TTY      IDLE      FROM           HOST\n");

    while ((ut = getutent()) != NULL) {

	if (ut->ut_user[0] && ut->ut_type == USER_PROCESS) {
		/* ut->ut_line is device name of tty - "/dev/" */
	    printf("%-10s %-8s ", ut->ut_user, ut->ut_line);

		name = concat_path_file("/dev/", ut->ut_line);
	    if (stat(name, &st) == 0) {
		idle = time(NULL) -  st.st_atime;

		if (idle < 60)
		    printf("00:00m    ");
		else if (idle < (60 * 60))
		    printf("00:%02dm    ", (int)(idle / 60));
		else if (idle < (24 * 60 * 60))
		    printf("%02d:%02dm    ", (int)(idle / (60 * 60)),
			   (int)(idle % (60 * 60)) / 60);
		else if (idle < (24 * 60 * 60 * 365))
		    printf("%03ddays   ", (int)(idle / (24 * 60 * 60)));
		else
		    printf("%02dyears   ", (int) (idle / (24 * 60 * 60 * 365)));
	    } else
		printf("%-8s  ", "?");

	    printf("%-12.12s   %s\n", ctime((time_t*)&(ut->ut_tv.tv_sec)) + 4, ut->ut_host);
		free(name);
	}
    }
    endutent();

    return 0;
}
