/*
 * A tiny 'top' utility.
 *
 * This is written specifically for the linux /proc/<PID>/status
 * file format, but it checks that the file actually conforms to the
 * format that this utility expects.
 
 * This reads the PIDs of all processes at startup and then shows the
 * status of those processes at given intervals.  User can give
 * maximum number of processes to show.  If a process exits, it's PID
 * is shown as 'EXIT'.  If new processes are started while this works,
 * it doesn't add them to the list of shown processes.
 * 
 * NOTES:
 * - At startup this changes to /proc, all the reads are then
 *   relative to that.
 * - Includes code from the scandir() manual page.
 * 
 * TODO:
 * - ppid, uid etc could be read only once when program starts
 *   and rest of the information could be gotten from the
 *   /proc/<PID>/statm file.
 * - Add process CPU and memory usage *percentages*.
 * 
 * (C) Eero Tamminen <oak at welho dot com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/ioctl.h>
#include "busybox.h"


/* process information taken from /proc,
 * The code takes into account how long the fields below are,
 * starting from copying the file from 'status' file to displaying it!
 */
typedef struct {
	char uid[6];    /* User ID */
	char pid[6];	/* Pid */
	char ppid[6];   /* Parent Pid */
	char name[12];	/* Name */
	char cmd[20];	/* command line[read/show size] */
	char state[2];	/* State: S, W... */
	char size[9];	/* VmSize */
	char lck[9];	/* VmLck */
	char rss[9];	/* VmRSS */
	char data[9];	/* VmData */
	char stk[9];	/* VmStk */
	char exe[9];	/* VmExe */
	char lib[9];	/* VmLib */
} status_t;

/* display generic info (meminfo / loadavg) */
static void display_generic(void)
{
	FILE *fp;
	char buf[80];
	float avg1, avg2, avg3;
	unsigned long total, used, mfree, shared, buffers, cached;

	/* read memory info */
	fp = fopen("meminfo", "r");
	if (!fp) {
		perror("fopen('meminfo')");
		return;
	}
	fgets(buf, sizeof(buf), fp);	/* skip first line */

	if (fscanf(fp, "Mem: %lu %lu %lu %lu %lu %lu",
		   &total, &used, &mfree, &shared, &buffers, &cached) != 6) {
		fprintf(stderr, "Error: failed to read 'meminfo'");
		fclose(fp);
	}
	fclose(fp);
	
	/* read load average */
	fp = fopen("loadavg", "r");
	if (!fp) {
		perror("fopen('loadavg')");
			return;
	}
	if (fscanf(fp, "%f %f %f", &avg1, &avg2, &avg3) != 3) {
		fprintf(stderr, "Error: failed to read 'loadavg'");
		fclose(fp);
		return;
	}
	fclose(fp);

	/* convert to kilobytes */
	if (total) total /= 1024;
	if (used) used /= 1024;
	if (mfree) mfree /= 1024;
	if (shared) shared /= 1024;
	if (buffers) buffers /= 1024;
	if (cached) cached /= 1024;
	
	/* output memory info and load average */
	printf("Mem: %ldK, %ldK used, %ldK free, %ldK shrd, %ldK buff, %ldK cached\n",
	       total, used, mfree, shared, buffers, cached);
	printf("Load average: %.2f, %.2f, %.2f    (State: S=sleeping R=running, W=waiting)\n",
	       avg1, avg2, avg3);
}


/* display process statuses */
static void display_status(int count, const status_t *s)
{
	const char *fmt, *cmd;
	
	/* clear screen & go to top */
	printf("\e[2J\e[1;1H");

	display_generic();
	
	/* what info of the processes is shown */
	printf("\n%*s %*s %*s %*s %*s %*s  %-*s\n",
	       sizeof(s->pid)-1, "Pid:",
	       sizeof(s->state)-1, "",
	       sizeof(s->ppid)-1, "PPid:",
	       sizeof(s->uid)-1, "UID:",
	       sizeof(s->size)-1, "WmSize:",
	       sizeof(s->rss)-1, "WmRSS:",
	       sizeof(s->cmd)-1, "command line:");

	while (count--) {
		if (s->cmd[0]) {
			/* normal process, has command line */
			cmd = s->cmd;
			fmt = "%*s %*s %*s %*s %*s %*s  %s\n";
		} else {
			/* no command line, show only process name */
			cmd = s->name;
			fmt = "%*s %*s %*s %*s %*s %*s  [%s]\n";
		}
		printf(fmt,
		       sizeof(s->pid)-1, s->pid,
		       sizeof(s->state)-1, s->state,
		       sizeof(s->ppid)-1, s->ppid,
		       sizeof(s->uid)-1, s->uid,
		       sizeof(s->size)-1, s->size,
		       sizeof(s->rss)-1, s->rss,
		       cmd);
		s++;
	}
}


/* checks if given 'buf' for process starts with 'id' + ':' + TAB
 * and stores rest of the buf to 'store' with max size 'size'
 */
static int process_status(const char *buf, const char *id, char *store, size_t size)
{
	int len, i;
	
	/* check status field name */
	len = strlen(id);
	if (strncmp(buf, id, len) != 0) {
		if(store)
			error_msg_and_die("ERROR status: line doesn't start with '%s' in:\n%s\n", id, buf);
		else
			return 0;
	}
	if (!store) {
		/* ignoring this field */
		return 1;
	}
	buf += len;
	
	/* check status field format */
	if ((*buf++ != ':') || (*buf++ != '\t')) {
		error_msg_and_die("ERROR status: field '%s' not followed with ':' + TAB in:\n%s\n", id, buf);
	}
	
	/* skip whitespace in Wm* fields */
	if (id[0] == 'V' && id[1] == 'm') {
		i = 3;
		while (i--) {
			if (*buf == ' ') {
				buf++;
			} else {
				error_msg_and_die("ERROR status: can't skip whitespace for "
					"'%s' field in:\n%s\n", id, buf);
			}
		}
	}
	
	/* copy at max (size-1) chars and force '\0' to the end */
	while (--size) {
		if (*buf < ' ') {
			break;
		}
		*store++ = *buf++;
	}
	*store = '\0';
	return 1;
}

/* read process statuses */
static void read_status(int num, status_t *s)
{
	char status[20];
	char buf[80];
	FILE *fp;
	
	while (num--) {
		sprintf(status, "%s/status", s->pid);

		/* read the command line from 'cmdline' in PID dir */
		fp = fopen(status, "r");
		if (!fp) {
			strncpy(s->pid, "EXIT", sizeof(s->pid));
			continue;
		}

		/* get and process the information */
		fgets(buf, sizeof(buf), fp);
		process_status(buf, "Name", s->name, sizeof(s->name));
		fgets(buf, sizeof(buf), fp);
		process_status(buf, "State", s->state, sizeof(s->state));
		fgets(buf, sizeof(buf), fp);
		if(process_status(buf, "Tgid", NULL, 0))
			fgets(buf, sizeof(buf), fp);
		process_status(buf, "Pid", NULL, 0);
		fgets(buf, sizeof(buf), fp);
		process_status(buf, "PPid", s->ppid, sizeof(s->ppid));
		fgets(buf, sizeof(buf), fp);
		if(process_status(buf, "TracerPid", NULL, 0))
			fgets(buf, sizeof(buf), fp);
		process_status(buf, "Uid", s->uid, sizeof(s->uid));
		fgets(buf, sizeof(buf), fp);
		process_status(buf, "Gid", NULL, 0);
		fgets(buf, sizeof(buf), fp);
		if(process_status(buf, "FDSize", NULL, 0))
			fgets(buf, sizeof(buf), fp);
		process_status(buf, "Groups", NULL, 0);
		fgets(buf, sizeof(buf), fp);
		/* only user space processes have command line
		 * and memory statistics
		 */
		if (s->cmd[0]) {
			process_status(buf, "VmSize", s->size, sizeof(s->size));
			fgets(buf, sizeof(buf), fp);
			process_status(buf, "VmLck", s->lck, sizeof(s->lck));
			fgets(buf, sizeof(buf), fp);
			process_status(buf, "VmRSS", s->rss, sizeof(s->rss));
			fgets(buf, sizeof(buf), fp);
			process_status(buf, "VmData", s->data, sizeof(s->data));
			fgets(buf, sizeof(buf), fp);
			process_status(buf, "VmStk", s->stk, sizeof(s->stk));
			fgets(buf, sizeof(buf), fp);
			process_status(buf, "VmExe", s->exe, sizeof(s->exe));
			fgets(buf, sizeof(buf), fp);
			process_status(buf, "VmLib", s->lib, sizeof(s->lib));
		}
		fclose(fp);
		
		/* next process */
		s++;
	}
}


/* allocs statuslist and reads process command lines, frees namelist,
 * returns filled statuslist or NULL in case of error.
 */
static status_t *read_info(int num, struct dirent **namelist)
{
	status_t *statuslist, *s;
	char cmdline[20];
	FILE *fp;
	int idx;

	/* allocate & zero status for each of the processes */
	statuslist = calloc(num, sizeof(status_t));
	if (!statuslist) {
		return NULL;
	}
	
	/* go through the processes */
	for (idx = 0; idx < num; idx++) {

		/* copy PID string to status struct and free name */
		s = &(statuslist[idx]);
		if (strlen(namelist[idx]->d_name) > sizeof(s->pid)-1) {
			fprintf(stderr, "PID '%s' too long\n", namelist[idx]->d_name);
			return NULL;
		}
		strncpy(s->pid, namelist[idx]->d_name, sizeof(s->pid));
		s->pid[sizeof(s->pid)-1] = '\0';
		free(namelist[idx]);

		/* read the command line from 'cmdline' in PID dir */
		sprintf(cmdline, "%s/cmdline", s->pid);
		fp = fopen(cmdline, "r");
		if (!fp) {
			perror("fopen('cmdline')");
			return NULL;
		}
		fgets(statuslist[idx].cmd, sizeof(statuslist[idx].cmd), fp);
		fclose(fp);
	}
	free(namelist);
	return statuslist;
}


/* returns true for file names which are PID dirs
 * (i.e. start with number)
 */
static int filter_pids(const struct dirent *dir)
{
	status_t dummy;
	char *name = dir->d_name;

	if (*name >= '0' && *name <= '9') {
		if (strlen(name) > sizeof(dummy.pid)-1) {
			fprintf(stderr, "PID name '%s' too long\n", name);
			return 0;
		}
		return 1;
	}
	return 0;
}


/* compares two directory entry names as numeric strings
 */
static int num_sort(const void *a, const void *b)
{
	int ia = atoi((*(struct dirent **)a)->d_name);
	int ib = atoi((*(struct dirent **)b)->d_name);
	
	if (ia == ib) {
		return 0;
	}
	/* NOTE: by switching the check, you change the process sort order */
	if (ia < ib) {
		return -1;
	} else {
		return 1;
	}
}

int top_main(int argc, char **argv)
{
	status_t *statuslist;
	struct dirent **namelist;
	int opt, num, interval, lines;
#if defined CONFIG_FEATURE_AUTOWIDTH && defined CONFIG_FEATURE_USE_TERMIOS
	struct winsize win = { 0, 0, 0, 0 };
#endif
	/* Default update rate is 5 seconds */
	interval = 5;
	/* Default to 25 lines - 5 lines for status */
	lines = 25 - 5;

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "d:")) > 0) {
	    switch (opt) {
		case 'd':
		    interval = atoi(optarg);
		    break;
		default:
		    show_usage();
	    }
	}

#if defined CONFIG_FEATURE_AUTOWIDTH && defined CONFIG_FEATURE_USE_TERMIOS
	ioctl(fileno(stdout), TIOCGWINSZ, &win);
	if (win.ws_row > 4)
	    lines = win.ws_row - 5;
#endif
	
	/* change to proc */
	if (chdir("/proc") < 0) {
		perror_msg_and_die("chdir('/proc')");
	}
	
	/* read process IDs for all the processes from the procfs */
	num = scandir(".", &namelist, filter_pids, num_sort);
	if (num < 0) {
		perror_msg_and_die("scandir('/proc')");
	}
	if (lines > num) {
		lines = num;
	}

	/* read command line for each of the processes */
	statuslist = read_info(num, namelist);
	if (!statuslist) {
		return EXIT_FAILURE;
	}

	while (1) {
		/* read status for each of the processes */
		read_status(num, statuslist);

		/* display status */
		display_status(lines, statuslist);
		
		sleep(interval);
	}
	
	free(statuslist);
	return EXIT_SUCCESS;
}
