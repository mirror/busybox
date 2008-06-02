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
#if ENABLE_DEBUG
#include <assert.h>
#define dbg_assert assert
#else
#define dbg_assert(stuff) do {} while (0)
#endif
/*
 * Theory of operation:
 * - iterate over all modules and record their full path
 * - iterate over all modules looking for "depends=" entries
 *   for each depends, look through our list of full paths and emit if found
 */

typedef struct dep_lst_t {
	char *name;
	llist_t *dependencies;
	struct dep_lst_t *next;
} dep_lst_t;

struct globals {
	dep_lst_t *lst; /* modules without their corresponding extension */
};
#define G (*(struct globals*)&bb_common_bufsiz1)
/* We have to zero it out because of NOEXEC */
#define INIT_G() memset(&G, 0, sizeof(G))

static int fill_lst(const char *modulename, struct stat ATTRIBUTE_UNUSED *sb,
					void ATTRIBUTE_UNUSED *data, int ATTRIBUTE_UNUSED depth)
{

	/* We get a file here. If the file does not have ".ko" but an
	 * intermittent dentry has, it's just their fault.
	 */
	if (strrstr(modulename, ".ko") != NULL) {
		dep_lst_t *new = xzalloc(sizeof(dep_lst_t));
		new->name = xstrdup(modulename);
		new->next = G.lst;
		G.lst = new;
	}
	return TRUE;
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
	ptr = the_module;
	this = G.lst;
	do {
		if (!strcmp(fname, this->name))
			break;
		this = this->next;
	} while (this);
	dbg_assert (this);
//bb_info_msg("fname='%s'", fname);
	do {
		/* search for a 'd' */
		ptr = memchr(ptr, 'd', len - (ptr - (char*)the_module));
		if (ptr == NULL) /* no d left, done */
			goto none;
		if (!strncmp(ptr, "depends=", sizeof("depends=")-1))
			break;
		++ptr;
	} while (1);
	deps = depends = xstrdup (ptr + sizeof("depends=")-1);
//bb_info_msg(" depends='%s'", depends);
	while (deps) {
		dep_lst_t *all = G.lst;

		ptr = strsep(&deps, ",");
		while (all) {
			/* Compare the recorded filenames ignoring ".ko*" at the end.  */
			char *tmp = bb_get_last_path_component_nostrip(all->name);
			if (!strncmp(ptr, tmp, MAX(strlen(ptr),strrstr(tmp, ".ko") - tmp)))
				break; /* found it */
			all = all->next;
		}
		if (all) {
			dbg_assert(all->name); /* this cannot be empty */
//bb_info_msg("[%s] -> '%s'", (char*)ptr, all->name);
			llist_add_to_end(&this->dependencies, all->name);
		}
	}
	free(depends);
 none:
	munmap(the_module, sb->st_size);
 skip:
	return TRUE;
}

int depmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int depmod_main(int ATTRIBUTE_UNUSED argc, char **argv)
{
	int ret;
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
		xchdir(moddir_base);
	}

	if (!(option_mask32 & ARG_n)) { /* --dry-run */
		chp = concat_path_file(moddir, CONFIG_DEFAULT_DEPMOD_FILE);
		filedes = xfopen(chp, "w");
		if (ENABLE_FEATURE_CLEAN_UP)
			free(chp);
	}
	ret = EXIT_SUCCESS;
	/* We have to do a full walk to collect all needed data.  */
	if (!recursive_action(moddir,
			ACTION_RECURSE, /* flags */
			fill_lst, /* file action */
			NULL, /* dir action */
			NULL, /* user data */
			0)) { /* depth */
		if (ENABLE_FEATURE_CLEAN_UP)
			ret = EXIT_FAILURE;
		else
			return EXIT_FAILURE;
	}
#if ENABLE_FEATURE_CLEAN_UP
	else
#endif
	do {
		chp = option_mask32 & ARG_a ? moddir : *argv++;

		if (!recursive_action(chp,
				ACTION_RECURSE, /* flags */
				fileAction, /* file action */
				NULL, /* dir action */
				NULL, /* user data */
				0)) { /* depth */
			ret = EXIT_FAILURE;
		}
	} while (!(option_mask32 & ARG_a) && *argv);

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
	{
	dep_lst_t *mods = G.lst;
#if ENABLE_FEATURE_DEPMOD_PRUNE_FANCY
	while (mods) {
		llist_t *deps = mods->dependencies;
		while (deps) {
			dep_lst_t *all = G.lst;
			while (all) {
				if (!strcmp(all->name, deps->data)) {
					llist_t *implied = all->dependencies;
					while (implied) {
						/* erm, nicer would be to just
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

	mods = G.lst;
#endif
	/* Finally print them.  */
	while (mods) {
		fprintf(filedes, "%s:", mods->name);
		while (mods->dependencies)
			fprintf(filedes, " %s", (char*)llist_pop(&mods->dependencies));
		fprintf(filedes, "\n");
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
