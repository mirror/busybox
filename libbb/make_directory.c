/* vi: set sw=4 ts=4: */
/*
 * Mini make_directory implementation for busybox
 *
 * Copyright (C) 2001  Matt Kraai.
 * 
 * Rewriten in 2002
 * Copyright (C) 2002 Glenn McGrath
 * Copyright (C) 2002 Vladimir N. Oleynik
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
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "libbb.h"

/* Create the directory PATH with mode MODE, or the default if MODE is -1.
 * Also create parent directories as necessary if flags contains
 * FILEUTILS_RECUR.  */

int make_directory (char *path, long mode, int flags)
{
	int ret;
	
	/* Calling apps probably should use 0777 instead of -1
	 * then we dont need this condition
	 */
	if (mode == -1) {
		mode = 0777;
	}
	if (flags == FILEUTILS_RECUR) {
		char *pp = strrchr(path, '/');
		if ((pp) && (pp != path)) {
			*pp = '\0';
			make_directory(path, mode, flags);
			*pp = '/';
		}
	}
	ret = mkdir(path, mode);
	if (ret == -1) {
		if ((flags == FILEUTILS_RECUR) && (errno == EEXIST)) {
			ret = 0;
		} else {
			perror_msg_and_die("Cannot create directory '%s'", path);
		}
	}
	return(ret);
}
