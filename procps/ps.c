/* vi: set sw=4 ts=4: */
/*
 * Mini ps implementation(s) for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc.  Written by Erik Andersen
 * <andersen@lineo.com>, <andersee@debian.org>
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

#include "internal.h"
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/ioctl.h>
#define BB_DECLARE_EXTERN
#define bb_need_help
#include "messages.c"

#define TERMINAL_WIDTH  79      /* not 80 in case terminal has linefold bug */



#if ! defined BB_FEATURE_USE_DEVPS_PATCH

/* The following is the first ps implementation --
 * the one using the /proc virtual filesystem.
 */

#if ! defined BB_FEATURE_USE_PROCFS
#error Sorry, I depend on the /proc filesystem right now.
#endif

typedef struct proc_s {
	char
	 cmd[16];					/* basename of executable file in call to exec(2) */
	int
	 ruid, rgid,				/* real only (sorry) */
	 pid,						/* process id */
	 ppid;						/* pid of parent process */
	char
	 state;						/* single-char code for process state (S=sleeping) */
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

	tmp = strstr(S, "Pid:");
	if (tmp)
		sscanf(tmp, "Pid:\t%d\n" "PPid:\t%d\n", &P->pid, &P->ppid);
	else
		errorMsg("Internal error!\n");

	/* For busybox, ignoring effective, saved, etc */
	tmp = strstr(S, "Uid:");
	if (tmp)
		sscanf(tmp, "Uid:\t%d", &P->ruid);
	else
		errorMsg("Internal error!\n");

	tmp = strstr(S, "Gid:");
	if (tmp)
		sscanf(tmp, "Gid:\t%d", &P->rgid);
	else
		errorMsg("Internal error!\n");

}

extern int ps_main(int argc, char **argv)
{
	proc_t p;
	DIR *dir;
	FILE *file;
	struct dirent *entry;
	char path[32], sbuf[512];
	char uidName[10] = "";
	char groupName[10] = "";
	int len, i, c;
#ifdef BB_FEATURE_AUTOWIDTH
	struct winsize win = { 0, 0, 0, 0 };
	int terminal_width = TERMINAL_WIDTH;
#else
#define terminal_width  TERMINAL_WIDTH
#endif



	dir = opendir("/proc");
	if (!dir)
		fatalError("Can't open /proc\n");

#ifdef BB_FEATURE_AUTOWIDTH
		ioctl(fileno(stdout), TIOCGWINSZ, &win);
		if (win.ws_col > 0)
			terminal_width = win.ws_col - 1;
#endif

	fprintf(stdout, "%5s  %-8s %-3s %5s %s\n", "PID", "Uid", "Gid",
			"State", "Command");
	while ((entry = readdir(dir)) != NULL) {
		uidName[0] = '\0';
		groupName[0] = '\0';

		if (!isdigit(*entry->d_name))
			continue;
		sprintf(path, "/proc/%s/status", entry->d_name);
		if ((file2str(path, sbuf, sizeof sbuf)) != -1) {
			parse_proc_status(sbuf, &p);
		}

		/* Make some adjustments as needed */
		my_getpwuid(uidName, p.ruid);
		if (*uidName == '\0')
			sprintf(uidName, "%d", p.ruid);
		my_getgrgid(groupName, p.rgid);
		if (*groupName == '\0')
			sprintf(groupName, "%d", p.rgid);

		sprintf(path, "/proc/%s/cmdline", entry->d_name);
		file = fopen(path, "r");
		if (file == NULL)
			continue;
		i = 0;
		len = fprintf(stdout, "%5d %-8s %-8s %c ", p.pid, uidName, groupName,
				p.state);
		while (((c = getc(file)) != EOF) && (i < (terminal_width-len))) {
			i++;
			if (c == '\0')
				c = ' ';
			putc(c, stdout);
		}
		if (i == 0)
			fprintf(stdout, "[%s]", p.cmd);
		fprintf(stdout, "\n");
	}
	closedir(dir);
	return(TRUE);
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
	char uidName[10] = "";
	char groupName[10] = "";
#ifdef BB_FEATURE_AUTOWIDTH
	struct winsize win = { 0, 0, 0, 0 };
	int terminal_width = TERMINAL_WIDTH;
#else
#define terminal_width  TERMINAL_WIDTH
#endif

	if (argc > 1 && **(argv + 1) == '-') 
		usage(ps_usage);

	/* open device */ 
	fd = open(device, O_RDONLY);
	if (fd < 0) 
		fatalError( "open failed for `%s': %s\n", device, strerror (errno));

	/* Find out how many processes there are */
	if (ioctl (fd, DEVPS_GET_NUM_PIDS, &num_pids)<0) 
		fatalError( "\nDEVPS_GET_PID_LIST: %s\n", strerror (errno));
	
	/* Allocate some memory -- grab a few extras just in case 
	 * some new processes start up while we wait. The kernel will
	 * just ignore any extras if we give it too many, and will trunc.
	 * the list if we give it too few.  */
	pid_array = (pid_t*) calloc( num_pids+10, sizeof(pid_t));
	pid_array[0] = num_pids+10;

	/* Now grab the pid list */
	if (ioctl (fd, DEVPS_GET_PID_LIST, pid_array)<0) 
		fatalError("\nDEVPS_GET_PID_LIST: %s\n", strerror (errno));

#ifdef BB_FEATURE_AUTOWIDTH
		ioctl(fileno(stdout), TIOCGWINSZ, &win);
		if (win.ws_col > 0)
			terminal_width = win.ws_col - 1;
#endif

	/* Print up a ps listing */
	fprintf(stdout, "%5s  %-8s %-3s %5s %s\n", "PID", "Uid", "Gid",
			"State", "Command");

	for (i=1; i<pid_array[0] ; i++) {
		uidName[0] = '\0';
		groupName[0] = '\0';
	    info.pid = pid_array[i];

	    if (ioctl (fd, DEVPS_GET_PID_INFO, &info)<0)
			fatalError("\nDEVPS_GET_PID_INFO: %s\n", strerror (errno));
	    
		/* Make some adjustments as needed */
		my_getpwuid(uidName, info.euid);
		if (*uidName == '\0')
			sprintf(uidName, "%ld", info.euid);
		my_getgrgid(groupName, info.egid);
		if (*groupName == '\0')
			sprintf(groupName, "%ld", info.egid);

		len = fprintf(stdout, "%5d %-8s %-8s %c ", info.pid, uidName, groupName, info.state);

		if (strlen(info.command_line) > 1) {
			for( j=0; j<(sizeof(info.command_line)-1) && j < (terminal_width-len); j++) {
				if (*(info.command_line+j) == '\0' && *(info.command_line+j+1) != '\0') {
					*(info.command_line+j) = ' ';
				}
			}
			*(info.command_line+j) = '\0';
			fprintf(stdout, "%s\n", info.command_line);
		} else {
			fprintf(stdout, "[%s]\n", info.name);
		}
	}

	/* Free memory */
	free( pid_array);

	/* close device */
	if (close (fd) != 0) 
		fatalError("close failed for `%s': %s\n", device, strerror (errno));
 
	exit (0);
}

#endif /* BB_FEATURE_USE_DEVPS_PATCH */

