/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright 1998 by Albert Cahalan; all rights reserved.
 * Copyright (C) 2002 by Vladimir Oleynik <dzo@simtreas.ru>
 * GNU Library General Public License Version 2, or any later version
 *
 */

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/page.h>

#include "libbb.h"

extern procps_status_t * procps_scan(int save_user_arg0
#ifdef CONFIG_SELINUX
	, int use_selinux , security_id_t *sid
#endif
	)
{
	static DIR *dir;
	struct dirent *entry;
	static procps_status_t ret_status;
	char *name;
	int n;
	char status[32];
	char buf[1024];
	FILE *fp;
	procps_status_t curstatus;
	int pid;
	long tasknice;
	struct stat sb;

	if (!dir) {
		dir = opendir("/proc");
		if(!dir)
			bb_error_msg_and_die("Can't open /proc");
	}
	for(;;) {
		if((entry = readdir(dir)) == NULL) {
			closedir(dir);
			dir = 0;
			return 0;
		}
		name = entry->d_name;
		if (!(*name >= '0' && *name <= '9'))
			continue;

		memset(&curstatus, 0, sizeof(procps_status_t));
		pid = atoi(name);
		curstatus.pid = pid;

		sprintf(status, "/proc/%d", pid);
		if(stat(status, &sb))
			continue;
		my_getpwuid(curstatus.user, sb.st_uid, sizeof(curstatus.user));

		sprintf(status, "/proc/%d/stat", pid);
		if((fp = fopen(status, "r")) == NULL)
			continue;
#ifdef CONFIG_SELINUX
		if(use_selinux)
		{
			if(fstat_secure(fileno(fp), &sb, sid))
				continue;
		}
		else
#endif
		name = fgets(buf, sizeof(buf), fp);
		fclose(fp);
		if(name == NULL)
			continue;
		name = strrchr(buf, ')'); /* split into "PID (cmd" and "<rest>" */
		if(name == 0 || name[1] != ' ')
			continue;
		*name = 0;
		sscanf(buf, "%*s (%15c", curstatus.short_cmd);
		n = sscanf(name+2,
		"%c %d "
		"%*s %*s %*s %*s "     /* pgrp, session, tty, tpgid */
		"%*s %*s %*s %*s %*s " /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
#ifdef FEATURE_CPU_USAGE_PERCENTAGE
		"%lu %lu "
#else
		"%*s %*s "
#endif
		"%*s %*s %*s "         /* cutime, cstime, priority */
		"%ld "
		"%*s %*s %*s "         /* timeout, it_real_value, start_time */
		"%*s "                 /* vsize */
		"%ld",
		curstatus.state, &curstatus.ppid,
#ifdef FEATURE_CPU_USAGE_PERCENTAGE
		&curstatus.utime, &curstatus.stime,
#endif
		&tasknice,
		&curstatus.rss);
#ifdef FEATURE_CPU_USAGE_PERCENTAGE
		if(n != 6)
#else
		if(n != 4)
#endif
			continue;

		if (curstatus.rss == 0 && curstatus.state[0] != 'Z')
			curstatus.state[1] = 'W';
		else
			curstatus.state[1] = ' ';
		if (tasknice < 0)
			curstatus.state[2] = '<';
		else if (tasknice > 0)
			curstatus.state[2] = 'N';
		else
			curstatus.state[2] = ' ';

#ifdef PAGE_SHIFT
		curstatus.rss <<= (PAGE_SHIFT - 10);     /* 2**10 = 1kb */
#else
		curstatus.rss *= (getpagesize() >> 10);     /* 2**10 = 1kb */
#endif

		if(save_user_arg0) {
			sprintf(status, "/proc/%d/cmdline", pid);
			if((fp = fopen(status, "r")) == NULL)
				continue;
			if((n=fread(buf, 1, sizeof(buf)-1, fp)) > 0) {
				if(buf[n-1]=='\n')
					buf[--n] = 0;
				name = buf;
				while(n) {
					if(((unsigned char)*name) < ' ')
						*name = ' ';
					name++;
					n--;
				}
				*name = 0;
				if(buf[0])
					curstatus.cmd = strdup(buf);
				/* if NULL it work true also */
			}
			fclose(fp);
		}
		return memcpy(&ret_status, &curstatus, sizeof(procps_status_t));
	}
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
