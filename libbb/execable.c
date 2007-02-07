/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* check if path points to an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int execable_file(const char *name)
{
	struct stat s;
	return (!access(name, X_OK) && !stat(name, &s) && S_ISREG(s.st_mode));
}

/* search $PATH for an executable file;
 * return allocated string containing full path if found;
 * return NULL otherwise;
 */
char *find_execable(const char *filename)
{
	char *path, *p, *n;

	p = path = xstrdup(getenv("PATH"));
	while (p) {
		n = strchr(p, ':');
		if (n)
			*n++ = '\0';
		if (*p != '\0') { /* it's not a PATH="foo::bar" situation */
			p = concat_path_file(p, filename);
			if (execable_file(p)) {
				free(path);
				return p;
			}
			free(p);
		}
		p = n;
	}
	free(path);
	return NULL;
}

/* search $PATH for an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int exists_execable(const char *filename)
{
	char *ret = find_execable(filename);
	if (ret) {
		free(ret);
		return 1;
	}
	return 0;
}

#if ENABLE_FEATURE_EXEC_PREFER_APPLETS
/* just like the real execvp, but try to launch an applet named 'file' first
 */
int bb_execvp(const char *file, char *const argv[])
{
	return execvp(find_applet_by_name(file) ? CONFIG_BUSYBOX_EXEC_PATH : file,
					argv);
}
#endif
