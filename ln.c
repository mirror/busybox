/* vi: set sw=4 ts=4: */
/*
 * Mini ln implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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

#include "busybox.h"
#define BB_DECLARE_EXTERN
#define bb_need_not_a_directory
#include "messages.c"

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

static const int LN_SYMLINK = 1;
static const int LN_FORCE = 2;
static const int LN_NODEREFERENCE = 4;

/*
 * linkDestName is where the link points to,
 * linkSrcName is the name of the link to be created.
 */
static int fs_link(const char *link_DestName, const char *link_SrcName, const int flag)
{
	int status;
	int srcIsDir;
	char *srcName;

	if (link_DestName==NULL)
		return(FALSE);

	srcName = (char *) malloc(strlen(link_SrcName)+strlen(link_DestName)+1);

	if (link_SrcName==NULL)
		strcpy(srcName, link_DestName);
	else
		strcpy(srcName, link_SrcName);

	if (flag&LN_NODEREFERENCE)
		srcIsDir = is_directory(srcName, TRUE, NULL);
	else
		srcIsDir = is_directory(srcName, FALSE, NULL);	
	
	if ((srcIsDir==TRUE)&&((flag&LN_NODEREFERENCE)==0)) {
		strcat(srcName, "/");
		strcat(srcName, link_DestName);
	}
	
	if (flag&LN_FORCE)
		unlink(srcName);
		
	if (flag&LN_SYMLINK)
		status = symlink(link_DestName, srcName);
	else
		status = link(link_DestName, srcName);

	if (status != 0) {
		perror(srcName);
		return(FALSE);
	}
	return(TRUE);
}

extern int ln_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int flag = 0;
	int opt;
	
	/* Parse any options */
	while ((opt=getopt(argc, argv, "sfn")) != -1) {
		switch(opt) {
			case 's':
				flag |= LN_SYMLINK;
				break;
			case 'f':
				flag |= LN_FORCE;
				break;
			case 'n':
				flag |= LN_NODEREFERENCE;
				break;
			default:
				show_usage();
		}
	}
	while(optind<(argc-1)) {
		if (fs_link(argv[optind], argv[argc-1], flag)==FALSE)
			status = EXIT_FAILURE;
		optind++;
	}
	return(status);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
