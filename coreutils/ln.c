/* vi: set sw=4 ts=4: */
/*
 * Mini ln implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "busybox.h"


static const int LN_SYMLINK = 1;
static const int LN_FORCE = 2;
static const int LN_NODEREFERENCE = 4;

/*
 * linkDestName is where the link points to,
 * linkSrcName is the name of the link to be created.
 */
static int fs_link(const char *link_destname, const char *link_srcname, 
		const int flag)
{
	int status;
	int src_is_dir;
	char *src_name;

	if (link_destname==NULL)
		return(FALSE);

	src_name = (char *) xmalloc(strlen(link_srcname)+strlen(link_destname)+1);

	if (link_srcname==NULL)
		strcpy(src_name, link_destname);
	else
		strcpy(src_name, link_srcname);

	if (flag&LN_NODEREFERENCE)
		src_is_dir = is_directory(src_name, TRUE, NULL);
	else
		src_is_dir = is_directory(src_name, FALSE, NULL);	
	
	if ((src_is_dir==TRUE)&&((flag&LN_NODEREFERENCE)==0)) {
		char* srcdir_name;

		srcdir_name = xstrdup(link_destname);
		strcat(src_name, "/");
		strcat(src_name, get_last_path_component(srcdir_name));
		free(srcdir_name);
	}
	
	if (flag&LN_FORCE)
		unlink(src_name);
		
	if (flag&LN_SYMLINK)
		status = symlink(link_destname, src_name);
	else
		status = link(link_destname, src_name);

	if (status != 0) {
		perror_msg(src_name);
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
	if (optind > (argc-1)) {
		show_usage();
	} 
	if (optind == (argc-1)) {
		if (fs_link(argv[optind], 
					get_last_path_component(argv[optind]), flag)==FALSE)
			status = EXIT_FAILURE;
	}
	while(optind<(argc-1)) {
		if (fs_link(argv[optind], argv[argc-1], flag)==FALSE)
			status = EXIT_FAILURE;
		optind++;
	}
	exit(status);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
