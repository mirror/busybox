/* vi: set sw=4 ts=4: */
/*
 * Mini lsmod implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
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

#include "internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <assert.h>
#include <getopt.h>
#include <sys/utsname.h>



struct module_info
{
	unsigned long addr;
	unsigned long size;
	unsigned long flags;
	long usecount;
};


int query_module(const char *name, int which, void *buf, size_t bufsize,
		 size_t *ret);

/* Values for query_module's which.  */
#define QM_MODULES	1
#define QM_DEPS		2
#define QM_REFS		3
#define QM_SYMBOLS	4
#define QM_INFO		5

/* Bits of module.flags.  */
#define NEW_MOD_RUNNING		1
#define NEW_MOD_DELETED		2
#define NEW_MOD_AUTOCLEAN	4
#define NEW_MOD_VISITED		8
#define NEW_MOD_USED_ONCE	16
#define NEW_MOD_INITIALIZING	64


extern int lsmod_main(int argc, char **argv)
{
	struct module_info info;
	char *module_names, *mn, *deps, *dn;
	size_t bufsize, nmod, count, i, j;

	module_names = xmalloc(bufsize = 256);
	deps = xmalloc(bufsize);
	if (query_module(NULL, QM_MODULES, module_names, bufsize, &nmod)) {
		fatalError("QM_MODULES: %s\n", strerror(errno));
	}

	printf("Module                  Size  Used by\n");
	for (i = 0, mn = module_names; i < nmod; mn += strlen(mn) + 1, i++) {
		if (query_module(mn, QM_INFO, &info, sizeof(info), &count)) {
			if (errno == ENOENT) {
				/* The module was removed out from underneath us. */
				continue;
			}
			/* else choke */
			fatalError("module %s: QM_INFO: %s\n", mn, strerror(errno));
		}
		while (query_module(mn, QM_REFS, deps, bufsize, &count)) {
			if (errno == ENOENT) {
				/* The module was removed out from underneath us. */
				continue;
			}
			if (errno != ENOSPC) {
				fatalError("module %s: QM_REFS: %s", mn, strerror(errno));
			}
			deps = xrealloc(deps, bufsize = count);
		}
		printf("%-20s%8lu%4ld ", mn, info.size, info.usecount);
		if (count) printf("[");
		for (j = 0, dn = deps; j < count; dn += strlen(dn) + 1, j++) {
			printf("%s%s", dn, (j==count-1)? "":" ");
		}
		if (count) printf("]");
		printf("\n");

		if (info.flags & NEW_MOD_DELETED)
			printf(" (deleted)");
		else if (info.flags & NEW_MOD_INITIALIZING)
			printf(" (initializing)");
		else if (!(info.flags & NEW_MOD_RUNNING))
			printf(" (uninitialized)");
		else {
			if (info.flags & NEW_MOD_AUTOCLEAN)
				printf(" (autoclean)");
			if (!(info.flags & NEW_MOD_USED_ONCE))
				printf(" (unused)");
		}
	}


	return( 0);
}
