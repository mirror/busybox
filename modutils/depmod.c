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
struct globals {
	llist_t *lst;
	unsigned moddir_base_len;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
/* We have to zero it out because of NOEXEC */
#define INIT_G() memset(&G, 0, sizeof(G))

static int fill_lst(const char *modulename, struct stat ATTRIBUTE_UNUSED *sb,
					void ATTRIBUTE_UNUSED *data, int ATTRIBUTE_UNUSED depth)
{
	if (last_char_is(modulename, 'o') != NULL) /* it is a module, remember it */
		llist_add_to(&G.lst, xstrdup(modulename + G.moddir_base_len));
	return TRUE;
}

static int fileAction(const char *fname, struct stat *sb,
					void *data, int ATTRIBUTE_UNUSED depth)
{
	size_t len = sb->st_size;
	void *the_module, *ptr;
	int fd;
	char *depends, *deps;

/*XXX: FIXME: does not handle compressed modules!
 * There should be a function that looks at the extension and sets up
 * open_transformer for us.
 */
	if (last_char_is(fname, 'o') == NULL) /* not a module */
		goto skip;

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

	fprintf((FILE*)data, "%s:", fname + G.moddir_base_len);
//bb_info_msg("fname='%s'", fname + G.moddir_base_len);
	do {
		/* search for a 'd' */
		ptr = memchr(ptr, 'd', len - (ptr - the_module));
		if (ptr == NULL) /* no d left, done */
			goto done;
		if (memcmp(ptr, "depends=", sizeof("depends=")-1) == 0)
			break;
		++ptr;
	} while (1);
	deps = depends = xstrdup (ptr + sizeof("depends=")-1);
//bb_info_msg(" depends='%s'", depends);
	while (*deps) {
		llist_t * _lst = G.lst;
		char *buf1;

		ptr = strchr(deps, ',');
		if (ptr != NULL)
			*(char*)ptr = '\0';
		/* remember the length of the current dependency plus eventual 0 byte */
		len = strlen(deps) + (ptr != NULL);
		buf1 = xasprintf("/%s.", deps); /* match the correct file */
		while (_lst) {
			ptr = strstr(_lst->data, buf1);
			if (ptr != NULL)
				break; /* found it */
			_lst = _lst->link;
		}
		free(buf1);
		if (_lst /*&& _lst->data*/) {
//bb_info_msg("[%s] -> '%s'", deps, _lst->data);
			fprintf((FILE*)data, " %s", _lst->data);
			deps += len;
		}
	}
	free(depends);
	fprintf((FILE*)data, "\n");
done:
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

	getopt32(argv, "aAb:eF:n", &moddir_base, &system_map);
	argv += optind;

	/* got a version to use? */
	if (*argv && (sscanf(*argv, "%u.%u.%u", &ret, &ret, &ret) == 3)) {
		moddir = concat_path_file(CONFIG_DEFAULT_MODULES_DIR, *argv++);
	} else {
		struct utsname uts;
		if (uname(&uts) < 0)
			bb_simple_perror_msg_and_die("uname");
		moddir = concat_path_file(CONFIG_DEFAULT_MODULES_DIR, uts.release);
	}
	/* if no modules are given on the command-line, -a is on per default */
	option_mask32 |= *argv == NULL;

	if (option_mask32 & ARG_b) {
		G.moddir_base_len = strlen(moddir_base);
		if (ENABLE_FEATURE_CLEAN_UP) {
			chp = moddir;
			moddir = concat_path_file(moddir_base, moddir);
			free (chp);
		} else
			moddir = concat_path_file(moddir_base, moddir);
	}

	if (!(option_mask32 & ARG_n)) { /* --dry-run */
		chp = concat_path_file(moddir, CONFIG_DEFAULT_DEPMOD_FILE);
		filedes = xfopen(chp, "w");
		if (ENABLE_FEATURE_CLEAN_UP)
			free(chp);
	}
	ret = EXIT_SUCCESS;
	/* have to do a full walk to collect all needed data */
	if (!recursive_action(moddir,
			ACTION_RECURSE, /* flags */
			fill_lst, /* file action */
			NULL, /* dir action */
			NULL, /* user data */
			0)) {
		ret = EXIT_FAILURE;
	} else
	do {
		chp = option_mask32 & ARG_a ? moddir : *argv++;

		if (!recursive_action(chp,
				ACTION_RECURSE, /* flags */
				fileAction, /* file action */
				NULL, /* dir action */
				(void*)filedes, /* user data */
				0)) { /* depth */
			ret = EXIT_FAILURE;
		}
	} while (!(option_mask32 & ARG_a) && *argv);

	if (ENABLE_FEATURE_CLEAN_UP) {
		fclose_if_not_stdin(filedes);
		llist_free(G.lst, free);
		free(moddir);
	}
	return ret;
}
