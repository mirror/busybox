/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 * Patched by a bunch of people.  Feel free to acknowledge your work.
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
		perror_msg("could not stat '/'");
	else {
		if ((dev = rootStat.st_rdev)==0) 
			dev=rootStat.st_dev;

		dir = opendir("/dev");
		if (!dir) 
			perror_msg("could not open '/dev'");
		else {
			while((entry = readdir(dir)) != NULL) {

				/* Must skip ".." since that is "/", and so we 
				 * would get a false positive on ".."  */
				if (strcmp(entry->d_name, "..") == 0)
					continue;

				fileName = concat_path_file("/dev", entry->d_name);

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
		fileName=xstrdup("/dev/root");
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
