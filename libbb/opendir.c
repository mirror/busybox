/* vi: set sw=4 ts=4: */
/*
 * wrapper for opendir()
 *
 * Copyright (C) 2006 Bernhard Fischer <busybox@busybox.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <sys/types.h>
#include <dirent.h>
#include "libbb.h"

#ifdef L_bb_opendir
DIR *bb_opendir(const char *path)
{
	DIR *dp;

	if ((dp = opendir(path)) == NULL) {
		bb_perror_msg("unable to open `%s'", path);
		return NULL;
	}
	return dp;
}
#endif

#ifdef L_bb_xopendir
DIR *bb_xopendir(const char *path)
{
	DIR *dp;

	if ((dp = opendir(path)) == NULL) {
		bb_perror_msg_and_die("unable to open `%s'", path);
	}
	return dp;
}
#endif
