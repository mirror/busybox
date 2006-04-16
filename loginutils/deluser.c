/* vi: set sw=4 ts=4: */
/*
 * deluser (remove lusers from the system ;) for TinyLogin
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Unified with delgroup by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "busybox.h"

/* where to start and stop deletion */
typedef struct {
	size_t start;
	size_t stop;
} Bounds;

/* An interesting side-effect of boundary()'s
 * implementation is that the first user (typically root)
 * cannot be removed.  Let's call it a feature. */
static inline Bounds boundary(const char *buffer, const char *login)
{
	char needle[256];
	char *start;
	char *stop;
	Bounds b;

	snprintf(needle, 256, "\n%s:", login);
	needle[255] = 0;
	start = strstr(buffer, needle);
	if (!start) {
		b.start = 0;
		b.stop = 0;
		return b;
	}
	start++;

	stop = strchr(start, '\n');
	b.start = start - buffer;
	b.stop = stop - buffer;
	return b;
}

/* grep -v ^login (except it only deletes the first match) */
/* ...in fact, I think I'm going to simplify this later */
static void del_line_matching(const char *login, const char *filename)
{
	char *buffer;
	FILE *passwd;
	Bounds b;
	struct stat statbuf;


	if ((passwd = bb_wfopen(filename, "r"))) {
		xstat(filename, &statbuf);
		buffer = (char *) xmalloc(statbuf.st_size * sizeof(char));
		fread(buffer, statbuf.st_size, sizeof(char), passwd);
		fclose(passwd);
		/* find the user to remove */
		b = boundary(buffer, login);
		if (b.stop != 0) {
			/* write the file w/o the user */
			if ((passwd = bb_wfopen(filename, "w"))) {
				fwrite(buffer, (b.start - 1), sizeof(char), passwd);
				fwrite(&buffer[b.stop], (statbuf.st_size - b.stop), sizeof(char), passwd);
				fclose(passwd);
			}
		} else {
			bb_error_msg("Can't find '%s' in '%s'", login, filename);
		}
		free(buffer);
	}
}

int deluser_main(int argc, char **argv)
{
	if (argc != 2) {
		bb_show_usage();
	} else {
		if (ENABLE_DELUSER && bb_applet_name[3] == 'u') {
			del_line_matching(argv[1], bb_path_passwd_file);
			if (ENABLE_FEATURE_SHADOWPASSWDS)
				del_line_matching(argv[1], bb_path_shadow_file);
		}
		del_line_matching(argv[1], bb_path_group_file);
		if (ENABLE_FEATURE_SHADOWPASSWDS)
			del_line_matching(argv[1], bb_path_gshadow_file);
	}
	return (EXIT_SUCCESS);
}

/* $Id: deluser.c,v 1.4 2003/07/14 20:20:45 andersen Exp $ */
