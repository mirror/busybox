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

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include "libbb.h"



extern char *find_real_root_device_name(const char* name)
{
	DIR *dir;
	struct dirent *entry;
	struct stat statBuf, rootStat;
	char *fileName = NULL;
	dev_t dev;

	if (stat("/", &rootStat) != 0)
		bb_perror_msg("could not stat '/'");
	else {
		/* This check is here in case they pass in /dev name */
		if ((rootStat.st_mode & S_IFMT) == S_IFBLK)
			dev = rootStat.st_rdev;
		else
			dev = rootStat.st_dev;

		dir = opendir("/dev");
		if (!dir)
			bb_perror_msg("could not open '/dev'");
		else {
			while((entry = readdir(dir)) != NULL) {
				const char *myname = entry->d_name;
				/* Must skip ".." since that is "/", and so we
				 * would get a false positive on ".."  */
				if (myname[0] == '.' && myname[1] == '.' && !myname[2])
					continue;

				fileName = concat_path_file("/dev", myname);

				/* Some char devices have the same dev_t as block
				 * devices, so make sure this is a block device */
				if (stat(fileName, &statBuf) == 0 &&
						S_ISBLK(statBuf.st_mode)!=0 &&
						statBuf.st_rdev == dev)
					break;
				free(fileName);
				fileName=NULL;
			}
			closedir(dir);
		}
	}
	if(fileName==NULL)
		fileName = bb_xstrdup("/dev/root");
	return fileName;
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
