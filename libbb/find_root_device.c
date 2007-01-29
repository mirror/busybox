/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

char *find_block_device(const char *path)
{
	DIR *dir;
	struct dirent *entry;
	struct stat st;
	dev_t dev;
	char *retpath=NULL;

	if (stat(path, &st) || !(dir = opendir("/dev")))
		return NULL;
	dev = (st.st_mode & S_IFMT) == S_IFBLK ? st.st_rdev : st.st_dev;
	while ((entry = readdir(dir)) != NULL) {
		char devpath[PATH_MAX];
		sprintf(devpath,"/dev/%s", entry->d_name);
		if (!stat(devpath, &st) && S_ISBLK(st.st_mode) && st.st_rdev == dev) {
			retpath = xstrdup(devpath);
			break;
		}
	}
	closedir(dir);

	return retpath;
}
