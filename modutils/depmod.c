/* vi: set sw=4 ts=4: */
/*
 * depmod - generate modules.dep
 * Copyright (c) 2008 Bernhard Reutner-Fischer
 * Copyrihgt (c) 2008 Timo Teras <timo.teras@iki.fi>
 * Copyright (c) 2008 Vladimir Dronnikov
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <libbb.h>
#include <sys/utsname.h> /* uname() */
#include "modutils.h"

/*
 * Theory of operation:
 * - iterate over all modules and record their full path
 * - iterate over all modules looking for "depends=" entries
 *   for each depends, look through our list of full paths and emit if found
 */

typedef struct module_info {
	struct module_info *next;
	char *name, *modname;
	llist_t *dependencies;
	llist_t *aliases;
	llist_t *symbols;
	struct module_info *dnext, *dprev;
} module_info;

enum {
	ARG_a = (1<<0), /* All modules, ignore mods in argv */
	ARG_A = (1<<1), /* Only emit .ko that are newer than modules.dep file */
	ARG_b = (1<<2), /* not /lib/modules/$(uname -r)/ but this base-dir */
	ARG_e = (1<<3), /* with -F, print unresolved symbols */
	ARG_F = (1<<4), /* System.map that contains the symbols */
	ARG_n = (1<<5)  /* dry-run, print to stdout only */
};

static int FAST_FUNC parse_module(const char *fname, struct stat *sb,
				  void *data, int UNUSED_PARAM depth)
{
	module_info **first = (module_info **) data;
	char *image, *ptr;
	module_info *info;
	size_t len = sb->st_size;

	if (strrstr(fname, ".ko") == NULL)
		return TRUE;

	image = xmalloc_open_zipped_read_close(fname, &len);
	info = xzalloc(sizeof(module_info));

	info->next = *first;
	*first = info;

	info->dnext = info->dprev = info;
	info->name = xstrdup(fname);
	info->modname = filename2modname(fname, NULL);
	for (ptr = image; ptr < image + len - 10; ptr++) {
		if (strncmp(ptr, "depends=", 8) == 0) {
			char *u;

			ptr += 8;
			for (u = ptr; *u; u++)
				if (*u == '-')
					*u = '_';
			ptr += string_to_llist(ptr, &info->dependencies, ",");
		} else if (ENABLE_FEATURE_MODUTILS_ALIAS &&
			   strncmp(ptr, "alias=", 6) == 0) {
			llist_add_to(&info->aliases, xstrdup(ptr + 6));
			ptr += strlen(ptr);
		} else if (ENABLE_FEATURE_MODUTILS_SYMBOLS &&
			   strncmp(ptr, "__ksymtab_", 10) == 0) {
			ptr += 10;
			if (strncmp(ptr, "gpl", 3) == 0 ||
			    strcmp(ptr, "strings") == 0)
				continue;
			llist_add_to(&info->symbols, xstrdup(ptr));
			ptr += strlen(ptr);
		}
	}
	free(image);

	return TRUE;
}

static module_info *find_module(module_info *modules, const char *modname)
{
	module_info *m;

	for (m = modules; m != NULL; m = m->next)
		if (strcmp(m->modname, modname) == 0)
			return m;
	return NULL;
}

static void order_dep_list(module_info *modules, module_info *start,
			   llist_t *add)
{
	module_info *m;
	llist_t *n;

	for (n = add; n != NULL; n = n->link) {
		m = find_module(modules, n->data);
		if (m == NULL)
			continue;

		/* unlink current entry */
		m->dnext->dprev = m->dprev;
		m->dprev->dnext = m->dnext;

		/* and add it to tail */
		m->dnext = start;
		m->dprev = start->dprev;
		start->dprev->dnext = m;
		start->dprev = m;

		/* recurse */
		order_dep_list(modules, start, m->dependencies);
	}
}

int depmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int depmod_main(int argc UNUSED_PARAM, char **argv)
{
	module_info *modules = NULL, *m, *dep;
	char *moddir_base = (char *)CONFIG_DEFAULT_MODULES_DIR;
	int tmp;

	getopt32(argv, "aAb:eF:n", &moddir_base, NULL);
	argv += optind;

	/* goto modules location */

	/* If a version is provided, then that kernel version's module directory
	 * is used, rather than the current kernel version (as returned by
	 * "uname -r").  */
	xchdir(moddir_base);
	if (*argv && (sscanf(*argv, "%d.%d.%d", &tmp, &tmp, &tmp) == 3)) {
		xchdir(*argv++);
	} else {
		struct utsname uts;
		uname(&uts);
		xchdir(uts.release);
	}
	/* If no modules are given on the command-line, -a is on per default.  */
	option_mask32 |= *argv == NULL;

	/* Scan modules */
	moddir_base = xrealloc_getcwd_or_warn(NULL);
	do {
		recursive_action((option_mask32 & ARG_a) ? moddir_base : *argv,
				ACTION_RECURSE, parse_module, NULL,  &modules, 0);
	} while (!(option_mask32 & ARG_a) && *(++argv));
	if (ENABLE_FEATURE_CLEAN_UP)
		free(moddir_base);

	/* Generate dependency and alias files */
	if (!(option_mask32 & ARG_n))
		freopen(CONFIG_DEFAULT_DEPMOD_FILE, "w", stdout);
	for (m = modules; m != NULL; m = m->next) {
		printf("%s:", m->name);

		order_dep_list(modules, m, m->dependencies);
		while (m->dnext != m) {
			dep = m->dnext;
			printf(" %s", dep->name);

			/* unlink current entry */
			dep->dnext->dprev = dep->dprev;
			dep->dprev->dnext = dep->dnext;
			dep->dnext = dep->dprev = dep;
		}
		puts("");
	}

#if ENABLE_FEATURE_MODUTILS_ALIAS
	if (!(option_mask32 & ARG_n))
		freopen("modules.alias", "w", stdout);
	for (m = modules; m != NULL; m = m->next) {
		while (m->aliases) {
			printf("alias %s %s\n",
				(char*)llist_pop(&m->aliases),
				m->modname);
		}
	}
#endif
#if ENABLE_FEATURE_MODUTILS_SYMBOLS
	if (!(option_mask32 & ARG_n))
		freopen("modules.symbols", "w", stdout);
	for (m = modules; m != NULL; m = m->next) {
		while (m->symbols) {
			printf("alias symbol:%s %s\n",
				(char*)llist_pop(&m->symbols),
				m->modname);
		}
	}
#endif

	if (ENABLE_FEATURE_CLEAN_UP) {
		while (modules) {
			module_info *old = modules;
			modules = modules->next;
			free(old->name);
			free(old->modname);
			free(old);
		}
	}

	return EXIT_SUCCESS;
}
