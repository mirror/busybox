/*
 * Mini du implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by John Beppu <beppu@line.com>
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

#include "internal.h"
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
/*
#include <unistd.h>
#include <sys/stat.h>
*/


typedef void (Display)(size_t, char *);

static void
print(size_t size, char *filename)
{
    fprintf(stdout, "%-7d %s\n", (size >> 1), filename);
}

/* tiny recursive du */
static size_t
size(char *filename)
{
    struct stat statbuf;
    size_t	sum;

    if ((lstat(filename, &statbuf)) != 0) { return 0; }
    sum = statbuf.st_blocks;

    if (S_ISDIR(statbuf.st_mode)) {
	DIR		*dir;
	struct dirent	*entry;

	dir = opendir(filename);
	if (!dir) { return 0; }
	while ((entry = readdir(dir))) {
	    char newfile[512];
	    char *name = entry->d_name;

	    if (  (strcmp(name, "..") == 0)
	       || (strcmp(name, ".")  == 0)) 
	    { continue; }

	    sprintf(newfile, "%s/%s", filename, name);
	    sum += size(newfile);
	}
	closedir(dir);
	print(sum, filename);
    }
    return sum;
}

int 
du_main(int argc, char **argv)
{
    /* I'll fill main() in shortly */
    size(".");
    exit(0);
}

