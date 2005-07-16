/* vi: set sw=4 ts=4: */
/*
 * Mini ps implementation(s) for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 *
 *
 * This contains _two_ implementations of ps for Linux.  One uses the
 * traditional /proc virtual filesystem, and the other use the devps kernel
 * driver (written by Erik Andersen to avoid using /proc thereby saving 100k+).
 *
 *
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
 *
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
#include <linux/major.h>
#include "busybox.h"

static const int TERMINAL_WIDTH = 79;      /* not 80 in case terminal has linefold bug */



#if ! defined BB_FEATURE_USE_DEVPS_PATCH

/* The following is the first ps implementation --
 * the one using the /proc virtual filesystem.
 */

typedef struct proc_s {
	char cmd[16];					/* basename of executable file in call to exec(2) */
	int ruid;						/* real only (sorry) */
	int pid;						/* process id */
	int ppid;						/* pid of parent process */
	char state;						/* single-char code for process state (S=sleeping) */
	unsigned int vmsize;			/* size of process as far as the vm is concerned */
	char ttyname[9];				/* tty device */
} proc_t;



static int file2str(char *filename, char *ret, int cap)
{
	int fd, num_read;

	if ((fd = open(filename, O_RDONLY, 0)) == -1)
		return -1;
	if ((num_read = read(fd, ret, cap - 1)) <= 0)
		return -1;
	ret[num_read] = 0;
	close(fd);
	return num_read;
}


static char master[] = "pqrstuvwxyzabcde";
#define MAJOR(x) ((x) >> 8)
#define MINOR(x) ((x) & 0xff)

static void dev_to_name(dev_t dev, char *name)
{
	if (MAJOR(dev) == TTY_MAJOR)
		if (MINOR(dev) < 64)
			sprintf(name,"tty%d", (int)MINOR(dev));
		else
			sprintf(name,"ttyS%d", (int)MINOR(dev)-64);
	else if (MAJOR(dev) == PTY_SLAVE_MAJOR)
		sprintf(name,"tty%c%x", master[MINOR(dev) / 16], (int)MINOR(dev) & 0xf);
	else
		strcpy(name, "");
}


static void parse_proc_status(char *S, proc_t * P)
{
	char *tmp;

	memset(P->cmd, 0, sizeof P->cmd);
	sscanf(S, "Name:\t%15c", P->cmd);
	tmp = strchr(P->cmd, '\n');
	if (tmp)
		*tmp = '\0';
	tmp = strstr(S, "State");
	sscanf(tmp, "State:\t%c", &P->state);

	P->vmsize = 0;
	tmp = strstr(S, "Pid:");
	if (tmp)
		sscanf(tmp, "Pid:\t%d\n" "PPid:\t%d\n", &P->pid, &P->ppid);
	else
		error_msg("Internal error!");

	/* For busybox, ignoring effective, saved, etc. */
	tmp = strstr(S, "Uid:");
	if (tmp)
		sscanf(tmp, "Uid:\t%d", &P->ruid);
	else
		error_msg("Internal error!");

	P->vmsize = 0;
	if ((tmp = strstr(S, "VmSize:")) != NULL)
		sscanf(tmp, "VmSize:\t%d", &P->vmsize);
	else if ((tmp = strstr(S, "Mem:")) != NULL)
		sscanf(tmp, "MemSize:\t%d", &P->vmsize);
#if 0
	else
		error_msg("Internal error!");
#endif
}


static void parse_proc_stat(char *S, proc_t * P)
{
	char *tmp;

	/* pid (name) */
	tmp = strrchr(S, ')');
	/* state */
	tmp += 4;
	/* ppid */
	tmp = strchr(tmp, ' ')+1;
	/* pgrp */
	tmp = strchr(tmp, ' ')+1;
	/* session */
	tmp = strchr(tmp, ' ')+1;
	/* tty */
	dev_to_name(atoi(tmp), P->ttyname);
	tmp = strchr(tmp, ' ')+1;

#if 0
	/* tty_pgrp */
	tmp = strchr(tmp, ' ')+1;
	/* flags */
	tmp = strchr(tmp, ' ')+1;
	/* min_flt */
	tmp = strchr(tmp, ' ')+1;
	/* cmin_flt */
	tmp = strchr(tmp, ' ')+1;
	/* maj_flt */
	tmp = strchr(tmp, ' ')+1;
	/* cmaj_flt */
	tmp = strchr(tmp, ' ')+1;
	/* utime */
	tmp = strchr(tmp, ' ')+1;
	/* stime */
	tmp = strchr(tmp, ' ')+1;
	/* cutime */
	tmp = strchr(tmp, ' ')+1;
	/* cstime */
	tmp = strchr(tmp, ' ')+1;
	/* priority */
	tmp = strchr(tmp, ' ')+1;
	/* nice */
	tmp = strchr(tmp, ' ')+1;
	/* timeout */
	tmp = strchr(tmp, ' ')+1;
	/* it_real_value */
	tmp = strchr(tmp, ' ')+1;
	/* start_time */
	tmp = strchr(tmp, ' ')+1;
	/* vsize */
	tmp = strchr(tmp, ' ')+1;
	/* rss */
	tmp = strchr(tmp, ' ')+1;
	/* rsslim*/
	tmp = strchr(tmp, ' ')+1;
	/* start_code */
	tmp = strchr(tmp, ' ')+1;
	/* end_code */
	tmp = strchr(tmp, ' ')+1;
	/* start_stack */
	tmp = strchr(tmp, ' ')+1;
	/* sp */
	tmp = strchr(tmp, ' ')+1;
	/* ip */
	tmp = strchr(tmp, ' ')+1;
	/* signal */
	tmp = strchr(tmp, ' ')+1;
	/* blocked */
	tmp = strchr(tmp, ' ')+1;
	/* sigignore */
	tmp = strchr(tmp, ' ')+1;
	/* sigcatch */
	tmp = strchr(tmp, ' ')+1;
	/* wchan */
	tmp = strchr(tmp, ' ')+1;
	/* nswap */
	tmp = strchr(tmp, ' ')+1;
	/* cnswap */
	tmp = strchr(tmp, ' ')+1;
#endif	
}


extern int ps_main(int argc, char **argv)
{
	proc_t p;
	DIR *dir;
	FILE *file;
	struct dirent *entry;
	char path[32], sbuf[512];
	char uidName[9];
	int len, i, c;
#ifdef BB_FEATURE_AUTOWIDTH
	struct winsize win = { 0, 0, 0, 0 };
	int terminal_width = TERMINAL_WIDTH;
#else
#define terminal_width  TERMINAL_WIDTH
#endif



	dir = opendir("/proc");
	if (!dir)
		error_msg_and_die("Can't open /proc");

#ifdef BB_FEATURE_AUTOWIDTH
		ioctl(fileno(stdout), TIOCGWINSZ, &win);
		if (win.ws_col > 0)
			terminal_width = win.ws_col - 1;
#endif

	printf("%5s %-7s %-8s %6s %5s %s\n", "PID", "TTY", "Uid",
			"Size", "State", "Command");
	while ((entry = readdir(dir)) != NULL) {
		if (!isdigit(*entry->d_name))
			continue;
		sprintf(path, "/proc/%s/status", entry->d_name);
		if ((file2str(path, sbuf, sizeof sbuf)) != -1) {
			parse_proc_status(sbuf, &p);
		}

		sprintf(path, "/proc/%s/stat", entry->d_name);
		if ((file2str(path, sbuf, sizeof sbuf)) != -1) {
			parse_proc_stat(sbuf, &p);
		}

		/* Make some adjustments as needed */
		my_getpwuid(uidName, p.ruid);
		if (*uidName == '\0')
			sprintf(uidName, "%d", p.ruid);

		sprintf(path, "/proc/%s/cmdline", entry->d_name);
		file = fopen(path, "r");
		if (file == NULL)
			continue;
		i = 0;
		len = printf("%5d %-7s %-8s %6d   %c   ",
				p.pid, p.ttyname, uidName, p.vmsize, p.state);
		while (((c = getc(file)) != EOF) && (i < (terminal_width-len))) {
			i++;
			if (c == '\0')
				c = ' ';
			putc(c, stdout);
		}
		fclose(file);
		if (i == 0)
			printf("[%s]", p.cmd);
		putchar('\n');
	}
	closedir(dir);
	return EXIT_SUCCESS;
}


#else /* BB_FEATURE_USE_DEVPS_PATCH */


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
#ifdef BB_FEATURE_AUTOWIDTH
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

#ifdef BB_FEATURE_AUTOWIDTH
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
		if (*uidName == '\0')
			sprintf(uidName, "%ld", info.euid);

		len = printf("%5d %-8s %c    ", info.pid, uidName, info.state);

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

#endif /* BB_FEATURE_USE_DEVPS_PATCH */

