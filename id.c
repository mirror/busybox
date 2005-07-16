/* vi: set sw=4 ts=4: */
/*
 * Mini id implementation for busybox
 *
 * Copyright (C) 2000 by Randolph Chung <tausq@debian.org>
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
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>

extern int id_main(int argc, char **argv)
{
	int no_user = 0, no_group = 0, print_real = 0;
	int name_not_number = 0;
	char user[9], group[9];
	long gid;
	long pwnam, grnam;
	int opt;
	
	gid = 0;

	while ((opt = getopt(argc, argv, "ugrn")) > 0) {
		switch (opt) {
			case 'u':
				no_group++;
				break;
			case 'g':
				no_user++;
				break;
			case 'r':
				print_real++;
				break;
			case 'n':
				name_not_number++;
				break;
			default:
				show_usage();
		}
	}

	if (no_user && no_group) show_usage();

	if (argv[optind] == NULL) {
		if (print_real) {
			my_getpwuid(user, getuid());
			my_getgrgid(group, getgid());
		} else {
			my_getpwuid(user, geteuid());
			my_getgrgid(group, getegid());
		}
	} else {
		strncpy(user, argv[optind], 8);
		user[8] = '\0';
	    gid = my_getpwnamegid(user);
		my_getgrgid(group, gid);
	}

	pwnam=my_getpwnam(user);
	grnam=my_getgrnam(group);

	if (no_group) {
		if(name_not_number && user)
			puts(user);
		else
			printf("%ld\n", pwnam);
	} else if (no_user) {
		if(name_not_number && group)
			puts(group);
		else
			printf("%ld\n", grnam);
	} else {
		printf("uid=%ld(%s) gid=%ld(%s)\n", pwnam, user, grnam, group);
	}
	return(0);
}


/* END CODE */
