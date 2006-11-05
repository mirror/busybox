/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright 1998 by Albert Cahalan; all rights reserved.
 * Copyright (C) 2002 by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"


typedef struct {
	uid_t uid;
	char username[12];
} user_map_t;

static user_map_t *username_cache;
static int username_cache_size;

void clear_username_cache(void)
{
	free(username_cache);
	username_cache = NULL;
	username_cache_size = 0;
}

const char* get_cached_username(uid_t uid)
{
	int i;
	for (i = 0; i < username_cache_size; i++)
		if (username_cache[i].uid == uid)
			return username_cache[i].username;
	i = username_cache_size++;
	username_cache = xrealloc(username_cache, username_cache_size * sizeof(*username_cache));
	username_cache[i].uid = uid;
	bb_getpwuid(username_cache[i].username, uid, sizeof(username_cache[i].username));
	return username_cache[i].username;
}


#define PROCPS_BUFSIZE 1024

static int read_to_buf(const char *filename, void *buf)
{
	ssize_t ret;
	ret = open_read_close(filename, buf, PROCPS_BUFSIZE-1);
	((char *)buf)[ret > 0 ? ret : 0] = '\0';
	return ret;
}

procps_status_t* alloc_procps_scan(int flags)
{
	procps_status_t* sp = xzalloc(sizeof(procps_status_t));
	sp->dir = xopendir("/proc");
	return sp;
}

void free_procps_scan(procps_status_t* sp)
{
	closedir(sp->dir);
	free(sp->cmd);
	free(sp);
}

void BUG_comm_size(void);
procps_status_t* procps_scan(procps_status_t* sp, int flags)
{
	struct dirent *entry;
	char buf[PROCPS_BUFSIZE];
	char filename[sizeof("/proc//cmdline") + sizeof(int)*3];
	char *filename_tail;
	long tasknice;
	unsigned pid;
	int n;
	struct stat sb;

	if (!sp)
		sp = alloc_procps_scan(flags);

	for (;;) {
		entry = readdir(sp->dir);
		if (entry == NULL) {
			free_procps_scan(sp);
			return NULL;
		}
		if (safe_strtou(entry->d_name, &pid))
			continue;

		/* After this point we have to break, not continue
		 * ("continue" would mean that current /proc/NNN
		 * is not a valid process info) */

		memset(&sp->rss, 0, sizeof(*sp) - offsetof(procps_status_t, rss));

		sp->pid = pid;
		if (!(flags & ~PSSCAN_PID)) break;

		filename_tail = filename + sprintf(filename, "/proc/%d", pid);

		if (flags & PSSCAN_UIDGID) {
			if (stat(filename, &sb))
				break;
			/* Need comment - is this effective or read UID/GID? */
			sp->uid = sb.st_uid;
			sp->gid = sb.st_gid;
		}
	
		if (flags & PSSCAN_STAT) {
			char *cp;
			/* see proc(5) for some details on this */
			strcpy(filename_tail, "/stat");
			n = read_to_buf(filename, buf);
			if (n < 0)
				break;
			cp = strrchr(buf, ')'); /* split into "PID (cmd" and "<rest>" */
			if (!cp || cp[1] != ' ')
				break;
			cp[0] = '\0';
			if (sizeof(sp->comm) < 16)
				BUG_comm_size();
			sscanf(buf, "%*s (%15c", sp->comm);
			n = sscanf(cp+2,
				"%c %u "               /* state, ppid */
				"%u %u %*s %*s "       /* pgid, sid, tty, tpgid */
				"%*s %*s %*s %*s %*s " /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
				"%lu %lu "             /* utime, stime */
				"%*s %*s %*s "         /* cutime, cstime, priority */
				"%ld "                 /* nice */
				"%*s %*s %*s "         /* timeout, it_real_value, start_time */
				"%*s "                 /* vsize */
				"%lu",                 /* rss */
				sp->state, &sp->ppid,
				&sp->pgid, &sp->sid,
				&sp->utime, &sp->stime,
				&tasknice,
				&sp->rss);
			if (n != 8)
				break;

			if (sp->rss == 0 && sp->state[0] != 'Z')
				sp->state[1] = 'W';
			else
				sp->state[1] = ' ';
			if (tasknice < 0)
				sp->state[2] = '<';
			else if (tasknice > 0)
				sp->state[2] = 'N';
			else
				sp->state[2] = ' ';

#ifdef PAGE_SHIFT
			sp->rss <<= (PAGE_SHIFT - 10);     /* 2**10 = 1kb */
#else
			sp->rss *= (getpagesize() >> 10);     /* 2**10 = 1kb */
#endif
		}

		if (flags & PSSCAN_CMD) {
			free(sp->cmd);
			sp->cmd = NULL;
			strcpy(filename_tail, "/cmdline");
			n = read_to_buf(filename, buf);
			if (n <= 0)
				break;
			if (buf[n-1] == '\n') {
				if (!--n)
					break;
				buf[n] = '\0';
			}
			do {
				n--;
				if ((unsigned char)(buf[n]) < ' ')
					buf[n] = ' ';
			} while (n);
			sp->cmd = strdup(buf);
		}
		break;
	}
	return sp;
}
/* from kernel:
	//             pid comm S ppid pgid sid tty_nr tty_pgrp flg
        sprintf(buffer,"%d (%s) %c %d  %d   %d  %d     %d       %lu %lu \
%lu %lu %lu %lu %lu %ld %ld %ld %ld %d 0 %llu %lu %ld %lu %lu %lu %lu %lu \
%lu %lu %lu %lu %lu %lu %lu %lu %d %d %lu %lu %llu\n",
                task->pid,
                tcomm,
                state,
                ppid,
                pgid,
                sid,
                tty_nr,
                tty_pgrp,
                task->flags,
                min_flt,

                cmin_flt,
                maj_flt,
                cmaj_flt,
                cputime_to_clock_t(utime),
                cputime_to_clock_t(stime),
                cputime_to_clock_t(cutime),
                cputime_to_clock_t(cstime),
                priority,
                nice,
                num_threads,
	// 0,
                start_time,
                vsize,
                mm ? get_mm_rss(mm) : 0,
                rsslim,
                mm ? mm->start_code : 0,
                mm ? mm->end_code : 0,
                mm ? mm->start_stack : 0,
                esp,
                eip,
the rest is some obsolete cruft
*/
