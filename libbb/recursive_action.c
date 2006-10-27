/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#undef DEBUG_RECURS_ACTION


/*
 * Walk down all the directories under the specified
 * location, and do something (something specified
 * by the fileAction and dirAction function pointers).
 *
 * Unfortunately, while nftw(3) could replace this and reduce
 * code size a bit, nftw() wasn't supported before GNU libc 2.1,
 * and so isn't sufficiently portable to take over since glibc2.1
 * is so stinking huge.
 */

static int true_action(const char *fileName, struct stat *statbuf, void* userData)
{
	return TRUE;
}

int recursive_action(const char *fileName,
		int recurse, int followLinks, int depthFirst,
		int (*fileAction) (const char *fileName, struct stat * statbuf, void* userData),
		int (*dirAction) (const char *fileName, struct stat * statbuf, void* userData),
		void* userData)
{
	struct stat statbuf;
	int status;
	DIR *dir;
	struct dirent *next;

	if (!fileAction) fileAction = true_action;
	if (!dirAction) dirAction = true_action;

	status = (followLinks ? stat : lstat)(fileName, &statbuf);

	if (status < 0) {
#ifdef DEBUG_RECURS_ACTION
		bb_error_msg("status=%d followLinks=%d TRUE=%d",
				status, followLinks, TRUE);
#endif
		bb_perror_msg("%s", fileName);
		return FALSE;
	}

	if (!followLinks && (S_ISLNK(statbuf.st_mode))) {
		return fileAction(fileName, &statbuf, userData);
	}

	if (!recurse) {
		if (S_ISDIR(statbuf.st_mode)) {
			return dirAction(fileName, &statbuf, userData);
		}
	}

	if (!S_ISDIR(statbuf.st_mode))
		return fileAction(fileName, &statbuf, userData);

	if (!depthFirst) {
		status = dirAction(fileName, &statbuf, userData);
		if (!status) {
			bb_perror_msg("%s", fileName);
			return FALSE;
		}
		if (status == SKIP)
			return TRUE;
	}

	dir = opendir(fileName);
	if (!dir) {
		return FALSE;
	}
	status = TRUE;
	while ((next = readdir(dir)) != NULL) {
		char *nextFile;

		nextFile = concat_subpath_file(fileName, next->d_name);
		if (nextFile == NULL)
			continue;
		if (!recursive_action(nextFile, TRUE, followLinks, depthFirst,
					fileAction, dirAction, userData)) {
			status = FALSE;
		}
		free(nextFile);
	}
	closedir(dir);
	if (depthFirst) {
		if (!dirAction(fileName, &statbuf, userData)) {
			bb_perror_msg("%s", fileName);
			return FALSE;
		}
	}

	if (!status)
		return FALSE;
	return TRUE;
}
