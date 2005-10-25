/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include "libbb.h"

extern char *find_block_device(char *path)
{
	DIR *dir;
	struct dirent *entry;
	struct stat st;
	dev_t dev;
	char *retpath=NULL;

	if(stat(path, &st) || !(dir = opendir("/dev"))) return NULL;
	dev = (st.st_mode & S_IFMT) == S_IFBLK ? st.st_rdev : st.st_dev;
	while((entry = readdir(dir)) != NULL) {
		char devpath[PATH_MAX];
		sprintf(devpath,"/dev/%s", entry->d_name);
		if(!stat(devpath, &st) && S_ISBLK(st.st_mode) && st.st_rdev == dev) {
			retpath = bb_xstrdup(devpath);
			break;
		}
	}
	closedir(dir);

	return retpath;
}
