/* vi: set sw=4 ts=4: */
/*
 * tac implementation for busybox
 *
 * Copyright (C) 2003  Yang Xiaopeng  <yxp at hanwang.com.cn>
 * Copyright (C) 2007  Natanael Copa  <natanael.copa@gmail.com>
 * Copyright (C) 2007  Tito Ragusa    <farmatito@tiscali.it>
 *
 * Licensed under GPLv2, see file License in this tarball for details.
 *
 */

/* tac - concatenate and print files in reverse */

/* Based on Yang Xiaopeng's (yxp at hanwang.com.cn) patch
 * http://www.uclibc.org/lists/busybox/2003-July/008813.html
 */

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */

int tac_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tac_main(int argc, char **argv)
{
	char **name;
	FILE *f;
	char *line;
	llist_t *list = NULL;
	int retval = EXIT_SUCCESS;
	
	argv++;
	if (!*argv)
		*--argv = (char *)"-";
	/* We will read from last file to first */
	name = argv;
	while (*name)
		name++;

	do {
		name--;
		f = fopen_or_warn_stdin(*name);
		if (f == NULL) {
			retval = EXIT_FAILURE;
			continue;
		}

		errno = 0;
		/* FIXME: NUL bytes are mishandled. */
		while ((line = xmalloc_fgets(f)) != NULL)
			llist_add_to(&list, line);	

		/* xmalloc_fgets uses getc and returns NULL on error or EOF. */
		/* It sets errno to ENOENT on EOF, but fopen_or_warn_stdin would */
		/* catch this error so we can filter it out here. */
		if (errno && errno != ENOENT) {
			bb_simple_perror_msg(*name);
			retval = EXIT_FAILURE;
		}
	} while (name != argv);

	while (list) {
		printf("%s", list->data);
		list = list->link;
	}
	
	return retval;
}
