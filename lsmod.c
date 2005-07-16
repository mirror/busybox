/* vi: set sw=4 ts=4: */
/*
 * Mini lsmod implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 *
 * Modified by Alcove, Julien Gaulmin <julien.gaulmin@alcove.fr> and
 * Nicolas Ferre <nicolas.ferre@alcove.fr> to support pre 2.1 kernels
 * (which lack the query_module() interface).
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <assert.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include "busybox.h"


#define TAINT_FILENAME                  "/proc/sys/kernel/tainted"
#define TAINT_PROPRIETORY_MODULE        (1<<0)
#define TAINT_FORCED_MODULE             (1<<1)
#define TAINT_UNSAFE_SMP                (1<<2)

void check_tainted(void) 
{
	int tainted;
	FILE *f;

	tainted = 0;
	if ((f = fopen(TAINT_FILENAME, "r"))) {
		fscanf(f, "%d", &tainted);
		fclose(f);
	}
	if (f && tainted) {
		printf("    Tainted: %c%c%c\n",
				tainted & TAINT_PROPRIETORY_MODULE      ? 'P' : 'G',
				tainted & TAINT_FORCED_MODULE           ? 'F' : ' ',
				tainted & TAINT_UNSAFE_SMP              ? 'S' : ' ');
	}
	else {
		printf("    Not tainted\n");
	}
}


#ifdef BB_FEATURE_NEW_MODULE_INTERFACE

struct module_info
{
	unsigned long addr;
	unsigned long size;
	unsigned long flags;
	long usecount;
};


int query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret);

/* Values for query_module's which.  */
static const int QM_MODULES = 1;
static const int QM_DEPS = 2;
static const int QM_REFS = 3;
static const int QM_SYMBOLS = 4;
static const int QM_INFO = 5;

/* Bits of module.flags.  */
static const int NEW_MOD_RUNNING = 1;
static const int NEW_MOD_DELETED = 2;
static const int NEW_MOD_AUTOCLEAN = 4;
static const int NEW_MOD_VISITED = 8;
static const int NEW_MOD_USED_ONCE = 16;
static const int NEW_MOD_INITIALIZING = 64;

static int my_query_module(const char *name, int which, void **buf,
		size_t *bufsize, size_t *ret)
{
	int my_ret;

	my_ret = query_module(name, which, *buf, *bufsize, ret);

	if (my_ret == -1 && errno == ENOSPC) {
		*buf = xrealloc(*buf, *ret);
		*bufsize = *ret;

		my_ret = query_module(name, which, *buf, *bufsize, ret);
	}

	return my_ret;
}

extern int lsmod_main(int argc, char **argv)
{
	struct module_info info;
	char *module_names, *mn, *deps, *dn;
	size_t bufsize, depsize, nmod, count, i, j;

	module_names = xmalloc(bufsize = 256);
	if (my_query_module(NULL, QM_MODULES, (void **)&module_names, &bufsize,
				&nmod)) {
		perror_msg_and_die("QM_MODULES");
	}

	deps = xmalloc(depsize = 256);
	printf("Module                  Size  Used by");
	check_tainted();

	for (i = 0, mn = module_names; i < nmod; mn += strlen(mn) + 1, i++) {
		if (query_module(mn, QM_INFO, &info, sizeof(info), &count)) {
			if (errno == ENOENT) {
				/* The module was removed out from underneath us. */
				continue;
			}
			/* else choke */
			perror_msg_and_die("module %s: QM_INFO", mn);
		}
		if (my_query_module(mn, QM_REFS, (void **)&deps, &depsize, &count)) {
			if (errno == ENOENT) {
				/* The module was removed out from underneath us. */
				continue;
			}
			perror_msg_and_die("module %s: QM_REFS", mn);
		}
		printf("%-20s%8lu%4ld", mn, info.size, info.usecount);
		if (info.flags & NEW_MOD_DELETED)
			printf(" (deleted)");
		else if (info.flags & NEW_MOD_INITIALIZING)
			printf(" (initializing)");
		else if (!(info.flags & NEW_MOD_RUNNING))
			printf(" (uninitialized)");
		else {
			if (info.flags & NEW_MOD_AUTOCLEAN)
				printf(" (autoclean) ");
			if (!(info.flags & NEW_MOD_USED_ONCE))
				printf(" (unused)");
		}
		if (count) printf(" [");
		for (j = 0, dn = deps; j < count; dn += strlen(dn) + 1, j++) {
			printf("%s%s", dn, (j==count-1)? "":" ");
		}
		if (count) printf("]");

		printf("\n");
	}


	return( 0);
}

#else /*BB_FEATURE_OLD_MODULE_INTERFACE*/

extern int lsmod_main(int argc, char **argv)
{
	int fd, i;
	char line[128];

	printf("Module                  Size  Used by");
	check_tainted();
	fflush(stdout);

	if ((fd = open("/proc/modules", O_RDONLY)) >= 0 ) {
		while ((i = read(fd, line, sizeof(line))) > 0) {
			write(fileno(stdout), line, i);
		}
		close(fd);
		return 0;
	}
	perror_msg_and_die("/proc/modules");
	return 1;
}

#endif /*BB_FEATURE_OLD_MODULE_INTERFACE*/
