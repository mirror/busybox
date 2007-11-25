/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

/*
In Linux we have three ways to determine "process name":
1. /proc/PID/stat has "...(name)...", among other things. It's so-called "comm" field.
2. /proc/PID/cmdline's first NUL-terminated string. It's argv[0] from exec syscall.
3. /proc/PID/exe symlink. Points to the running executable file.

kernel threads:
 comm: thread name
 cmdline: empty
 exe: <readlink fails>

executable
 comm: first 15 chars of base name
 (if executable is a symlink, then first 15 chars of symlink name are used)
 cmdline: argv[0] from exec syscall
 exe: points to executable (resolves symlink, unlike comm)

script (an executable with #!/path/to/interpreter):
 comm: first 15 chars of script's base name (symlinks are not resolved)
 cmdline: /path/to/interpreter (symlinks are not resolved)
 (script name is in argv[1], args are pushed into argv[2] etc)
 exe: points to interpreter's executable (symlinks are resolved)

If FEATURE_PREFER_APPLETS=y (and more so if FEATURE_SH_STANDALONE=y),
some commands started from busybox shell, xargs or find are started by
execXXX("/proc/self/exe", applet_name, params....)
and therefore comm field contains "exe".
*/

/* find_pid_by_name()
 *
 *  Modified by Vladimir Oleynik for use with libbb/procps.c
 *  This finds the pid of the specified process.
 *  Currently, it's implemented by rummaging through
 *  the proc filesystem.
 *
 *  Returns a list of all matching PIDs
 *  It is the caller's duty to free the returned pidlist.
 */
pid_t* find_pid_by_name(const char* procName)
{
	pid_t* pidList;
	int i = 0;
	procps_status_t* p = NULL;

	pidList = xmalloc(sizeof(*pidList));
	while ((p = procps_scan(p, PSSCAN_PID|PSSCAN_COMM|PSSCAN_ARGV0))) {
		if (
		/* we require comm to match and to not be truncated */
		/* in Linux, if comm is 15 chars, it may be a truncated
		 * name, so we don't allow that to match */
		    (!p->comm[sizeof(p->comm)-2] && strcmp(p->comm, procName) == 0)
		/* or we require argv0 to match (essential for matching reexeced /proc/self/exe)*/
		 || (p->argv0 && strcmp(bb_basename(p->argv0), procName) == 0)
		/* TOOD: we can also try /proc/NUM/exe link, do we want that? */
		) {
			pidList = xrealloc(pidList, sizeof(*pidList) * (i+2));
			pidList[i++] = p->pid;
		}
	}

	pidList[i] = 0;
	return pidList;
}

pid_t *pidlist_reverse(pid_t *pidList)
{
	int i = 0;
	while (pidList[i])
		i++;
	if (--i >= 0) {
		pid_t k;
		int j;
		for (j = 0; i > j; i--, j++) {
			k = pidList[i];
			pidList[i] = pidList[j];
			pidList[j] = k;
		}
	}
	return pidList;
}
