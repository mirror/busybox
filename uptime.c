/* vi: set sw=4 ts=4: */
/*
 * Mini uptime implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

/* This version of uptime doesn't display the number of users on the system,
 * since busybox init doesn't mess with utmp.  For folks using utmp that are
 * just dying to have # of users reported, feel free to write it as some type
 * of BB_FEATURE_UTMP_SUPPORT #define
 */

/* getopt not needed */


#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include "busybox.h"

static const int FSHIFT = 16;              /* nr of bits of precision */
#define FIXED_1         (1<<FSHIFT)     /* 1.0 as fixed-point */
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)


extern int uptime_main(int argc, char **argv)
{
	int updays, uphours, upminutes;
	struct sysinfo info;
	struct tm *current_time;
	time_t current_secs;

	time(&current_secs);
	current_time = localtime(&current_secs);

	sysinfo(&info);

	printf(" %2d:%02d%s  up ", 
			current_time->tm_hour%12 ? current_time->tm_hour%12 : 12, 
			current_time->tm_min, current_time->tm_hour > 11 ? "pm" : "am");
	updays = (int) info.uptime / (60*60*24);
	if (updays)
		printf("%d day%s, ", updays, (updays != 1) ? "s" : "");
	upminutes = (int) info.uptime / 60;
	uphours = (upminutes / 60) % 24;
	upminutes %= 60;
	if(uphours)
		printf("%2d:%02d, ", uphours, upminutes);
	else
		printf("%d min, ", upminutes);

	printf("load average: %ld.%02ld, %ld.%02ld, %ld.%02ld\n", 
			LOAD_INT(info.loads[0]), LOAD_FRAC(info.loads[0]), 
			LOAD_INT(info.loads[1]), LOAD_FRAC(info.loads[1]), 
			LOAD_INT(info.loads[2]), LOAD_FRAC(info.loads[2]));

	return EXIT_SUCCESS;
}
