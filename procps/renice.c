/*
 * Mini renice implementation for busybox
 *
 *
 * Copyright (C) 2000 Dave 'Kill a Cop' Cinege <dcinege@psychosis.com>
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

#include "internal.h"
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>


extern int renice_main(int argc, char **argv)
{
	int prio, err = 0;
	
	if (argc < 3)	usage(renice_usage);
		
	prio = atoi(*++argv);
	if (prio > 20)		prio = 20;
	if (prio < -20)		prio = -20;
	
	while (*++argv) {
		int ps = atoi(*argv);
		int oldp = getpriority(PRIO_PROCESS, ps);
		
		if (setpriority(PRIO_PROCESS, ps, prio) == 0) {
			printf("%d: old priority %d, new priority %d\n", ps, oldp, prio );
		} else {
			fprintf(stderr, "renice: %d: setpriority: ", ps);
			perror("");
			err = 1;
		}
	}
	exit(err);
}
