/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* check if path points to an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int FAST_FUNC file_is_executable(const char *name)
{
	struct stat s;
	return (!access(name, X_OK) && !stat(name, &s) && S_ISREG(s.st_mode));
}

/* search (*PATHp) for an executable file;
 * return allocated string containing full path if found;
 *  PATHp points to the component after the one where it was found
 *  (or NULL if found in last component),
 *  you may call find_executable again with this PATHp to continue
 * return NULL otherwise (PATHp is undefined)
 */
char* FAST_FUNC find_executable(const char *name, const char **PATHp)
{
	/* About empty components in $PATH:
	 * http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap08.html
	 * 8.3 Other Environment Variables - PATH
	 * A zero-length prefix is a legacy feature that indicates the current
	 * working directory. It appears as two adjacent colons ( "::" ), as an
	 * initial colon preceding the rest of the list, or as a trailing colon
	 * following the rest of the list.
	 */
	char *p = (char*) *PATHp;

	if (!p)
		return NULL;
	while (1) {
		const char *end = strchrnul(p, ':');
		int sz = end - p;

		if (sz != 0) {
			p = xasprintf("%.*s/%s", sz, p, name);
		} else {
			/* We have xxx::yyy in $PATH,
			 * it means "use current dir" */
			p = xstrdup(name);
// A bit of discrepancy wrt the path used if file is found here.
// bash 5.2.15 "type" returns "./NAME".
// GNU which v2.21 returns "/CUR/DIR/NAME".
// With -a, both skip over all colons: xxx::::yyy is the same as xxx::yyy,
// current dir is not tried the second time.
		}
		if (file_is_executable(p)) {
			*PATHp = (*end ? end+1 : NULL);
			return p;
		}
		free(p);
		if (*end == '\0')
			return NULL;
		p = (char *) end + 1;
	}
}

/* search $PATH for an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int FAST_FUNC executable_exists(const char *name)
{
	const char *path = getenv("PATH");
	char *ret = find_executable(name, &path);
	free(ret);
	return ret != NULL;
}

#if ENABLE_FEATURE_PREFER_APPLETS
/* just like the real execvp, but try to launch an applet named 'file' first */
int FAST_FUNC BB_EXECVP(const char *file, char *const argv[])
{
	if (find_applet_by_name(file) >= 0)
		execvp(bb_busybox_exec_path, argv);
	return execvp(file, argv);
}
#endif

void FAST_FUNC BB_EXECVP_or_die(char **argv)
{
	BB_EXECVP(argv[0], argv);
	/* SUSv3-mandated exit codes */
	xfunc_error_retval = (errno == ENOENT) ? 127 : 126;
	bb_perror_msg_and_die("can't execute '%s'", argv[0]);
}
