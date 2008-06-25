/* vi: set sw=4 ts=4: */
/*
 * depmod - generate modules.dep
 * Copyright (c) 2008 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <libbb.h>
#include <sys/utsname.h> /* uname() */
/*
 * Theory of operation:
 * - iterate over all modules and record their full path
 * - iterate over all modules looking for "depends=" entries
 *   for each depends, look through our list of full paths and emit if found
 */

typedef struct dep_lst_t {
	char *name;
	llist_t *dependencies;
	llist_t *aliases;
	struct dep_lst_t *next;
} dep_lst_t;

struct globals {
	dep_lst_t *lst; /* modules without their corresponding extension */
};
#define G (*(struct globals*)&bb_common_bufsiz1)
/* We have to zero it out because of NOEXEC */
#define INIT_G() memset(&G, 0, sizeof(G))

static char* find_keyword(void *the_module, size_t len, const char * const word)
{
	char *ptr = the_module;
	do {
		/* search for the first char in word */
		ptr = memchr(ptr, *word, len - (ptr - (char*)the_module));
		if (ptr == NULL) /* no occurance left, done */
			return NULL;
		if (!strncmp(ptr, word, strlen(word))) {
			ptr += strlen(word);
			break;
		}
		++ptr;
	} while (1);
	return ptr;
}
static int fileAction(const char *fname, struct stat *sb,
					void ATTRIBUTE_UNUSED *data, int ATTRIBUTE_UNUSED depth)
{
	size_t len = sb->st_size;
	void *the_module;
	char *ptr;
	int fd;
	char *depends, *deps;
	dep_lst_t *this;

	if (strrstr(fname, ".ko") == NULL) /* not a module */
		goto skip;

/*XXX: FIXME: does not handle compressed modules!
 * There should be a function that looks at the extension and sets up
 * open_transformer for us.
 */
	fd = xopen(fname, O_RDONLY);
	the_module = mmap(NULL, len, PROT_READ, MAP_SHARED
#if defined MAP_POPULATE
						|MAP_POPULATE
#endif
						, fd, 0);
	close(fd);
	if (the_module == MAP_FAILED)
		bb_perror_msg_and_die("mmap");

	this = xzalloc(sizeof(dep_lst_t));
	this->name = xstrdup(fname);
	this->next = G.lst;
	G.lst = this;
//bb_info_msg("fname='%s'", fname);
	ptr = find_keyword(the_module, len, "depends=");
	if (!*ptr)
		goto d_none;
	deps = depends = xstrdup(ptr);
//bb_info_msg(" depends='%s'", depends);
	while (deps) {
		ptr = strsep(&deps, ",");
//bb_info_msg("[%s] -> '%s'", fname, (char*)ptr);
		llist_add_to_end(&this->dependencies, xstrdup(ptr));
	}
	free(depends);
 d_none:
	if (ENABLE_FEATURE_DEPMOD_ALIAS)
	{
		size_t pos = 0;
		do {
			ptr = find_keyword(the_module + pos, len - pos, "alias=");
			if (ptr) {
//bb_info_msg("[%s] alias '%s'", fname, (char*)ptr);
					llist_add_to_end(&this->aliases, xstrdup(ptr));
			} else
				break;
			pos = (ptr - (char*)the_module);
		} while (1);
	}
	munmap(the_module, sb->st_size);
 skip:
	return TRUE;
}

int depmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int depmod_main(int ATTRIBUTE_UNUSED argc, char **argv)
{
	int ret;
	size_t moddir_base_len = 0; /* length of the "-b basedir" */
	char *moddir_base = NULL, *moddir, *system_map, *chp;
	FILE *filedes = stdout;
	enum {
		ARG_a = (1<<0), /* All modules, ignore mods in argv */
		ARG_A = (1<<1), /* Only emit .ko that are newer than modules.dep file */
		ARG_b = (1<<2), /* not /lib/modules/$(uname -r)/ but this base-dir */
		ARG_e = (1<<3), /* with -F, print unresolved symbols */
		ARG_F = (1<<4), /* System.map that contains the symbols */
		ARG_n = (1<<5)  /* dry-run, print to stdout only */
	};
	INIT_G();

	getopt32(argv, "aAb:eF:n", &moddir_base, &system_map);
	argv += optind;

	/* If a version is provided, then that kernel version’s module directory
	 * is used, rather than the current kernel version (as returned by
	 * "uname -r").  */
	if (*argv && (sscanf(*argv, "%d.%d.%d", &ret, &ret, &ret) == 3)) {
		moddir = concat_path_file(CONFIG_DEFAULT_MODULES_DIR, *argv++);
	} else {
		struct utsname uts;
		if (uname(&uts) < 0)
			bb_simple_perror_msg_and_die("uname");
		moddir = concat_path_file(CONFIG_DEFAULT_MODULES_DIR, uts.release);
	}
	/* If no modules are given on the command-line, -a is on per default.  */
	option_mask32 |= *argv == NULL;

	if (option_mask32 & ARG_b) {
		moddir_base_len = strlen(moddir_base) + 1;
		xchdir(moddir_base);
	}

	if (!(option_mask32 & ARG_n)) { /* --dry-run */
		chp = concat_path_file(moddir, CONFIG_DEFAULT_DEPMOD_FILE);
		filedes = xfopen(chp, "w");
		if (ENABLE_FEATURE_CLEAN_UP)
			free(chp);
	}
	ret = EXIT_SUCCESS;
	do {
		chp = option_mask32 & ARG_a ? moddir : (*argv + moddir_base_len);

		if (!recursive_action(chp,
				ACTION_RECURSE, /* flags */
				fileAction, /* file action */
				NULL, /* dir action */
				NULL, /* user data */
				0)) { /* depth */
			ret = EXIT_FAILURE;
		}
	} while (!(option_mask32 & ARG_a) && *++argv);

	{
	dep_lst_t *mods = G.lst;

	/* Fixup the module names in the depends list */
	while (mods) {
		llist_t *deps = NULL, *old_deps = mods->dependencies;

		while (old_deps) {
			dep_lst_t *all = G.lst;
			char *longname = NULL;
			char *shortname = llist_pop(&old_deps);

			while (all) {
				char *nam =
					xstrdup(bb_get_last_path_component_nostrip(all->name));
				char *tmp = strrstr(nam, ".ko");

				*tmp = '\0';
				if (!strcmp(nam, shortname)) {
					if (ENABLE_FEATURE_CLEAN_UP)
						free(nam);
					longname = all->name;
					break;
				}
				free(nam);
				all = all->next;
			}
			llist_add_to_end(&deps, longname);
		}
		mods->dependencies = deps;
		mods = mods->next;
	}

#if ENABLE_FEATURE_DEPMOD_PRUNE_FANCY
	/* modprobe allegedly wants dependencies without duplicates, i.e.
	 * mod1: mod2 mod3
	 * mod2: mod3
	 * mod3:
	 * implies that mod1 directly depends on mod2 and _not_ mod3 as mod3 is
	 * already implicitely pulled in via mod2. This leaves us with:
	 * mod1: mod2
	 * mod2: mod3
	 * mod3:
	 */
	mods = G.lst;
	while (mods) {
		llist_t *deps = mods->dependencies;
		while (deps) {
			dep_lst_t *all = G.lst;
			while (all) {
				if (!strcmp(all->name, deps->data)) {
					llist_t *implied = all->dependencies;
					while (implied) {
						/* XXX:FIXME: erm, it would be nicer to just
						 * llist_unlink(&mods->dependencies, implied)  */
						llist_t *prune = mods->dependencies;
						while (prune) {
							if (!strcmp(implied->data, prune->data))
								break;
							prune = prune->link;
						}
//if (prune) bb_info_msg("[%s] '%s' implies '%s', removing", mods->name, all->name, implied->data);
						llist_unlink(&mods->dependencies, prune);
						implied = implied->link;
					}
				}
				all = all->next;
			}
			deps = deps->link;
		}
		mods = mods->next;
	}
#endif

	mods = G.lst;
	/* Finally print them.  */
	while (mods) {
		fprintf(filedes, "%s:", mods->name);
		/* If we did not resolve all modules, then it's likely that we just did
		 * not see the names of all prerequisites (which will be NULL in this
		 * case).  */
		while (mods->dependencies) {
			char *the_dep = llist_pop(&mods->dependencies);
			if (the_dep)
				fprintf(filedes, " %s", the_dep);
		}
		fprintf(filedes, "\n");
		if (ENABLE_FEATURE_DEPMOD_ALIAS)
		{
			char *shortname =
				xstrdup(bb_get_last_path_component_nostrip(mods->name));
			char *tmp = strrstr(shortname, ".ko");

			*tmp = '\0';

			while (mods->aliases) {
				fprintf(filedes, "alias %s %s\n",
					(char*)llist_pop(&mods->aliases),
					shortname);
			}
			free(shortname);
		}
		mods = mods->next;
	}
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
		fclose_if_not_stdin(filedes);
		free(moddir);
		while (G.lst) {
			dep_lst_t *old = G.lst;
			G.lst = G.lst->next;
			free(old->name);
			free(old);
		}
	}
	return ret;
}
