/* vi: set sw=4 ts=4: */
/*
 * deluser (remove lusers from the system ;) for TinyLogin
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "busybox.h"

#define PASSWD_FILE     "/etc/passwd"
#define GROUP_FILE      "/etc/group"

/* where to start and stop deletion */
typedef struct {
	size_t start;
	size_t stop;
} Bounds;

/* An interesting side-effect of boundary()'s
 * implementation is that the first user (typically root)
 * cannot be removed.  Let's call it a feature. */
static Bounds boundary(const char *buffer, const char *login)
{
	char needle[256];
	char *start;
	char *stop;
	Bounds b;

	snprintf(needle, 256, "\n%s", login);
	needle[255] = 0;
	start = strstr(buffer, needle);
	if (!start) {
		b.start = 0;
		b.stop = 0;
		return b;
	}
	start++;

	stop = index(start, '\n');	/* index is a BSD-ism */
	b.start = start - buffer;
	b.stop = stop - buffer;
	return b;
}

/* grep -v ^login (except it only deletes the first match) */
/* ...in fact, I think I'm going to simplify this later */
static int del_line_matching(char *login, char *filename)
{
	char *buffer;
	FILE *passwd;
	size_t len;
	Bounds b;
	struct stat statbuf;

	/* load into buffer */
	passwd = fopen(filename, "r");
	if (!passwd) {
		return 1;
	}
	stat(filename, &statbuf);
	len = statbuf.st_size;
	buffer = (char *) malloc(len * sizeof(char));

	if (!buffer) {
		fclose(passwd);
		return 1;
	}
	fread(buffer, len, sizeof(char), passwd);

	fclose(passwd);

	/* find the user to remove */
	b = boundary(buffer, login);
	if (b.stop == 0) {
		free(buffer);
		return 1;
	}

	/* write the file w/o the user */
	passwd = fopen(filename, "w");
	if (!passwd) {
		return 1;
	}
	fwrite(buffer, (b.start - 1), sizeof(char), passwd);
	fwrite(&buffer[b.stop], (len - b.stop), sizeof(char), passwd);

	fclose(passwd);

	return 0;
}

/* ________________________________________________________________________ */
int delgroup_main(int argc, char **argv)
{
	/* int successful; */
	int failure;

	if (argc != 2) {
		show_usage();
	} else {

		failure = del_line_matching(argv[1], GROUP_FILE);
#ifdef TLG_FEATURE_SHADOWPASSWDS
		if (access(GSHADOW_FILE, W_OK) == 0) {
			/* EDR the |= works if the error is not 0, so he had it wrong */
			failure |= del_line_matching(argv[1], GSHADOW_FILE);
		}
#endif							/* TLG_FEATURE_SHADOWPASSWDS */
		/* if (!successful) { */
		if (failure) {
			error_msg_and_die("%s: Group could not be removed\n", argv[1]);
		}

	}
	return (EXIT_SUCCESS);
}

/* ________________________________________________________________________ */
int deluser_main(int argc, char **argv)
{
	/* int successful; */
	int failure;

	if (argc != 2) {
		show_usage();
	} else {

		failure = del_line_matching(argv[1], PASSWD_FILE);
		/* if (!successful) { */
		if (failure) {
			error_msg_and_die("%s: User could not be removed from %s\n",
							  argv[1], PASSWD_FILE);
		}
#ifdef TLG_FEATURE_SHADOWPASSWDS
		failure = del_line_matching(argv[1], SHADOW_FILE);
		/* if (!successful) { */
		if (failure) {
			error_msg_and_die("%s: User could not be removed from %s\n",
							  argv[1], SHADOW_FILE);
		}
#endif							/* TLG_FEATURE_SHADOWPASSWDS */
		failure = del_line_matching(argv[1], GROUP_FILE);
		/* if (!successful) { */
		if (failure) {
			error_msg_and_die("%s: User could not be removed from %s\n",
							  argv[1], GROUP_FILE);
		}

	}
	return (EXIT_SUCCESS);
}

/* $Id: deluser.c,v 1.1 2001/08/21 16:18:59 andersen Exp $ */
