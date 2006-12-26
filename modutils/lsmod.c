/* vi: set sw=4 ts=4: */
/*
 * Mini lsmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Modified by Alcove, Julien Gaulmin <julien.gaulmin@alcove.fr> and
 * Nicolas Ferre <nicolas.ferre@alcove.fr> to support pre 2.1 kernels
 * (which lack the query_module() interface).
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"


#ifndef CONFIG_FEATURE_CHECK_TAINTED_MODULE
static void check_tainted(void) { puts(""); }
#else
#define TAINT_FILENAME                  "/proc/sys/kernel/tainted"
#define TAINT_PROPRIETORY_MODULE        (1<<0)
#define TAINT_FORCED_MODULE             (1<<1)
#define TAINT_UNSAFE_SMP                (1<<2)

static void check_tainted(void)
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
#endif

#ifdef CONFIG_FEATURE_QUERY_MODULE_INTERFACE

struct module_info
{
	unsigned long addr;
	unsigned long size;
	unsigned long flags;
	long usecount;
};


int query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret);

enum {
/* Values for query_module's which.  */
	QM_MODULES = 1,
	QM_DEPS = 2,
	QM_REFS = 3,
	QM_SYMBOLS = 4,
	QM_INFO = 5,

/* Bits of module.flags.  */
	NEW_MOD_RUNNING = 1,
	NEW_MOD_DELETED = 2,
	NEW_MOD_AUTOCLEAN = 4,
	NEW_MOD_VISITED = 8,
	NEW_MOD_USED_ONCE = 16,
	NEW_MOD_INITIALIZING = 64
};

int lsmod_main(int argc, char **argv)
{
	struct module_info info;
	char *module_names, *mn, *deps, *dn;
	size_t bufsize, depsize, nmod, count, i, j;

	module_names = deps = NULL;
	bufsize = depsize = 0;
	while (query_module(NULL, QM_MODULES, module_names, bufsize, &nmod)) {
		if (errno != ENOSPC) bb_perror_msg_and_die("QM_MODULES");
		module_names = xmalloc(bufsize = nmod);
	}

	deps = xmalloc(depsize = 256);
	printf("Module\t\t\tSize  Used by");
	check_tainted();

	for (i = 0, mn = module_names; i < nmod; mn += strlen(mn) + 1, i++) {
		if (query_module(mn, QM_INFO, &info, sizeof(info), &count)) {
			if (errno == ENOENT) {
				/* The module was removed out from underneath us. */
				continue;
			}
			/* else choke */
			bb_perror_msg_and_die("module %s: QM_INFO", mn);
		}
		while (query_module(mn, QM_REFS, deps, depsize, &count)) {
			if (errno == ENOENT) {
				/* The module was removed out from underneath us. */
				continue;
			} else if (errno != ENOSPC)
				bb_perror_msg_and_die("module %s: QM_REFS", mn);
			deps = xrealloc(deps, count);
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

		puts("");
	}

#ifdef CONFIG_FEATURE_CLEAN_UP
	free(module_names);
#endif

	return 0;
}

#else /* CONFIG_FEATURE_QUERY_MODULE_INTERFACE */

int lsmod_main(int argc, char **argv)
{
	FILE *file = xfopen("/proc/modules", "r");

	printf("Module                  Size  Used by");
	check_tainted();
#if defined(CONFIG_FEATURE_LSMOD_PRETTY_2_6_OUTPUT)
	{
		char *line;
		while ((line = xmalloc_fgets(file)) != NULL) {
			char *tok;

			tok = strtok(line, " \t");
			printf("%-19s", tok);
			tok = strtok(NULL, " \t\n");
			printf(" %8s", tok);
			tok = strtok(NULL, " \t\n");
			/* Null if no module unloading support. */
			if (tok) {
				printf("  %s", tok);
				tok = strtok(NULL, "\n");
				if (!tok)
					tok = "";
				/* New-style has commas, or -.  If so,
				truncate (other fields might follow). */
				else if (strchr(tok, ',')) {
					tok = strtok(tok, "\t ");
					/* Strip trailing comma. */
					if (tok[strlen(tok)-1] == ',')
						tok[strlen(tok)-1] = '\0';
				} else if (tok[0] == '-'
						&& (tok[1] == '\0' || isspace(tok[1])))
					tok = "";
					printf(" %s", tok);
			}
			puts("");
			free(line);
		}
		fclose(file);
	}
#else
	xprint_and_close_file(file);
#endif  /*  CONFIG_FEATURE_2_6_MODULES  */
	return EXIT_SUCCESS;
}

#endif /* CONFIG_FEATURE_QUERY_MODULE_INTERFACE */
