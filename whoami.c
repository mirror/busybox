/* vi: set sw=4 ts=4: */
/*
 * Mini whoami implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
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
#include <stdio.h>
#include <pwd.h>

static const char whoami_usage[] = "whoami\n\n"
	"Print the user name associated with the current effective user id.\n"

	"Same as id -un.\n";

extern int whoami_main(int argc, char **argv)
{
	struct passwd *pw;
	uid_t uid;

	if (argc > 1)
		usage(whoami_usage);

	uid = geteuid();
	pw = getpwuid(uid);
	if (pw) {
		puts(pw->pw_name);
		exit(TRUE);
	}
	fprintf(stderr, "%s: cannot find username for UID %u\n", argv[0],
			(unsigned) uid);
	exit(FALSE);
}
