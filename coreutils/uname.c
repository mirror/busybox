/* vi: set sw=4 ts=4: */
/* uname -- print system information
   Copyright (C) 1989-1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Option		Example

   -s, --sysname	SunOS
   -n, --nodename	rocky8
   -r, --release	4.0
   -v, --version
   -m, --machine	sun
   -a, --all		SunOS rocky8 4.0  sun

   The default behavior is equivalent to `-s'.

   David MacKenzie <djm@gnu.ai.mit.edu> */

/* Busyboxed by Erik Andersen */

/* Further size reductions by Glenn McGrath and Manuel Novoa III. */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <getopt.h>
#include "busybox.h"

typedef struct {
	struct utsname name;
	char processor[8];			/* for "unknown" */
} uname_info_t;

static const char options[] = "snrvmpa";
static const char flags[] = "\x01\x02\x04\x08\x10\x20\x3f";
static const unsigned short int utsname_offset[] = {
	offsetof(uname_info_t,name.sysname),
	offsetof(uname_info_t,name.nodename),
	offsetof(uname_info_t,name.release),
	offsetof(uname_info_t,name.version),
	offsetof(uname_info_t,name.machine),
	offsetof(uname_info_t,processor)
};

int uname_main(int argc, char **argv)
{
	uname_info_t uname_info;

#if defined(__sparc__) && defined(__linux__)
	char *fake_sparc = getenv("FAKE_SPARC");
#endif

	const unsigned short int *delta;
	int opt;
	char toprint = 0;

	while ((opt = getopt(argc, argv, options)) != -1) {
		const char *p = strchr(options,opt);
		if (p == NULL) {
			show_usage();
		}
		toprint |= flags[(int)(p-options)];
	}

	if (toprint == 0) {
		toprint = flags[0];		/* sysname */
	}

	if (uname(&uname_info.name) == -1) {
		error_msg_and_die("cannot get system name");
	}

#if defined(__sparc__) && defined(__linux__)
 	if ((fake_sparc != NULL)
		&& ((fake_sparc[0] == 'y')
			|| (fake_sparc[0] == 'Y'))) {
		strcpy(uname_info.name.machine, "sparc");
	}
#endif

	strcpy(uname_info.processor, "unknown");

	delta=utsname_offset;
	do {
		if (toprint & 1) {
			printf(((char *)(&uname_info)) + *delta);
			if (toprint > 1) {
				putchar(' ');
			}
		}
		++delta;
	} while (toprint >>= 1);
	putchar('\n');

	return EXIT_SUCCESS;
}
