/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
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
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "busybox.h"

#undef APPLET
#undef APPLET_NOUSAGE
#undef PROTOTYPES
#include "applets.h"

#define bb_need_full_version
#define BB_DECLARE_EXTERN
#include "messages.c"

struct BB_applet *applet_using;

/* The -1 arises because of the {0,NULL,0,-1} entry above. */
const size_t NUM_APPLETS = (sizeof (applets) / sizeof (struct BB_applet) - 1);

extern void show_usage(void)
{
	const char *format_string;
	const char *usage_string = usage_messages;
	int i;

	for (i = applet_using - applets; i > 0; ) {
		if (!*usage_string++) {
			--i;
		}
	}
	format_string = "%s\n\nUsage: %s %s\n\n";
	if(*usage_string == 0)
		format_string = "%s\n\nNo help available.\n\n";
	fprintf(stderr, format_string,
			full_version, applet_using->name, usage_string);
	exit(EXIT_FAILURE);
}

static int applet_name_compare(const void *x, const void *y)
{
	const char *name = x;
	const struct BB_applet *applet = y;

	return strcmp(name, applet->name);
}

extern size_t NUM_APPLETS;

struct BB_applet *find_applet_by_name(const char *name)
{
	return bsearch(name, applets, NUM_APPLETS, sizeof(struct BB_applet),
			applet_name_compare);
}

void run_applet_by_name(const char *name, int argc, char **argv)
{
	/* Do a binary search to find the applet entry given the name. */
	if ((applet_using = find_applet_by_name(name)) != NULL) {
		applet_name = applet_using->name;
		if (argv[1] && strcmp(argv[1], "--help") == 0)
			show_usage();
		exit((*(applet_using->main)) (argc, argv));
	}
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
