/* vi: set sw=4 ts=4: */
/*
 * Mini ps implementation(s) for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen  
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

/*
 * This contains _two_ implementations of ps for Linux.  One uses the
 * traditional /proc virtual filesystem, and the other use the devps kernel
 * driver (written by Erik Andersen to avoid using /proc thereby saving 100k+).
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

static const int TERMINAL_WIDTH = 79;      /* not 80 in case terminal has linefold bug */



#if ! defined CONFIG_FEATURE_USE_DEVPS_PATCH

extern int ps_main(int argc, char **argv)
{
	procps_status_t * p;
	int i, len;
#ifdef CONFIG_FEATURE_AUTOWIDTH
	struct winsize win = { 0, 0, 0, 0 };
	int terminal_width = TERMINAL_WIDTH;
#else
#define terminal_width  TERMINAL_WIDTH
#endif


#ifdef CONFIG_FEATURE_AUTOWIDTH
		ioctl(fileno(stdout), TIOCGWINSZ, &win);
		if (win.ws_col > 0)
			terminal_width = win.ws_col - 1;
#endif

	printf("  PID  Uid     VmSize Stat Command\n");
	while ((p = procps_scan(1)) != 0) {
		char *namecmd = p->cmd;

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


#else /* CONFIG_FEATURE_USE_DEVPS_PATCH */


/* The following is the second ps implementation --
 * this one uses the nifty new devps kernel device.
 */

#include <linux/devps.h> /* For Erik's nifty devps device driver */


extern int ps_main(int argc, char **argv)
{
	char device[] = "/dev/ps";
	int i, j, len, fd;
	pid_t num_pids;
	pid_t* pid_array = NULL;
	struct pid_info info;
	char uidName[9];
#ifdef CONFIG_FEATURE_AUTOWIDTH
	struct winsize win = { 0, 0, 0, 0 };
	int terminal_width = TERMINAL_WIDTH;
#else
#define terminal_width  TERMINAL_WIDTH
#endif

	if (argc > 1 && **(argv + 1) == '-') 
		show_usage();

	/* open device */ 
	fd = open(device, O_RDONLY);
	if (fd < 0) 
		perror_msg_and_die( "open failed for `%s'", device);

	/* Find out how many processes there are */
	if (ioctl (fd, DEVPS_GET_NUM_PIDS, &num_pids)<0) 
		perror_msg_and_die( "\nDEVPS_GET_PID_LIST");
	
	/* Allocate some memory -- grab a few extras just in case 
	 * some new processes start up while we wait. The kernel will
	 * just ignore any extras if we give it too many, and will trunc.
	 * the list if we give it too few.  */
	pid_array = (pid_t*) xcalloc( num_pids+10, sizeof(pid_t));
	pid_array[0] = num_pids+10;

	/* Now grab the pid list */
	if (ioctl (fd, DEVPS_GET_PID_LIST, pid_array)<0) 
		perror_msg_and_die("\nDEVPS_GET_PID_LIST");

#ifdef CONFIG_FEATURE_AUTOWIDTH
		ioctl(fileno(stdout), TIOCGWINSZ, &win);
		if (win.ws_col > 0)
			terminal_width = win.ws_col - 1;
#endif

	/* Print up a ps listing */
	printf("  PID  Uid     Stat Command\n");

	for (i=1; i<pid_array[0] ; i++) {
	    info.pid = pid_array[i];

	    if (ioctl (fd, DEVPS_GET_PID_INFO, &info)<0)
			perror_msg_and_die("\nDEVPS_GET_PID_INFO");
	    
		/* Make some adjustments as needed */
		my_getpwuid(uidName, info.euid);

		if(p.vmsize == 0)
			len = printf("%5d %-8s        %c    ", p.pid, uidName, p.state);
		else
			len = printf("%5d %-8s %6d %c    ", p.pid, uidName, p.vmsize, p.state);
		if (strlen(info.command_line) > 1) {
			for( j=0; j<(sizeof(info.command_line)-1) && j < (terminal_width-len); j++) {
				if (*(info.command_line+j) == '\0' && *(info.command_line+j+1) != '\0') {
					*(info.command_line+j) = ' ';
				}
			}
			*(info.command_line+j) = '\0';
			puts(info.command_line);
		} else {
			printf("[%s]\n", info.name);
		}
	}

	/* Free memory */
	free( pid_array);

	/* close device */
	if (close (fd) != 0) 
		perror_msg_and_die("close failed for `%s'", device);
 
	exit (0);
}

#endif /* CONFIG_FEATURE_USE_DEVPS_PATCH */

