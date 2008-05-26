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

struct globals {
	llist_t *lst;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
/* We have to zero it out because of NOEXEC */
#define INIT_G() memset(&G, 0, sizeof(G))

static int fill_lst(const char *modulename, struct stat ATTRIBUTE_UNUSED *sb,
					void ATTRIBUTE_UNUSED *data, int ATTRIBUTE_UNUSED depth)
{
	llist_add_to_end(&G.lst, strdup(modulename));
	return TRUE;
}

static int fileAction(const char *fname, struct stat ATTRIBUTE_UNUSED *sb,
					void *data, int ATTRIBUTE_UNUSED depth)
{
	size_t seen = 0;
	size_t len = MAXINT(ssize_t);
	void *the_module = xmalloc_open_read_close(fname, &len), *ptr = the_module;
	const char *deps;
	RESERVE_CONFIG_BUFFER(depends, 512);
	RESERVE_CONFIG_BUFFER(buf1, 512);

	memset(buf1, 0, sizeof(buf1));
	memset(depends, 0, sizeof(depends));

	if (last_char_is(fname, 'o') == NULL) /* not a module */
		goto done;
	fprintf((FILE*)data, "\n%s:", fname);
//bb_info_msg("[%d] fname='%s'", (int)data, fname);
	do {
		/* search for a 'd' */
		ptr = memchr(ptr, 'd', len - seen);
		if (ptr == NULL) /* no d left, done */
			break;
		if (sscanf(ptr, "depends=%s", depends) == 1)
			break;
		seen = ++ptr - the_module;
	} while (1);
//bb_info_msg(" depends='%s'", depends);
	deps = depends;
	while (*deps) {
		llist_t * _lst = G.lst;
		ptr = memchr(deps, ',', strlen(deps));
		if (ptr != NULL)
			*(char*)ptr = '\0';
		/* remember the length of the current dependency plus eventual 0 byte */
		len = strlen(deps) + (ptr != NULL);
		sprintf(buf1, "/%s.", deps); /* make sure we match the correct file */
		while (_lst) {
			ptr = strstr(_lst->data, buf1);
			if (ptr != NULL)
				break; /* found it */
			_lst = _lst->link;
		}
		if (_lst && _lst->data) {
//bb_info_msg("[%s] -> '%s'", deps, _lst->data);
			fprintf((FILE*)data, " %s", _lst->data);
			deps += len;
		}
	}
done:
	RELEASE_CONFIG_BUFFER(depends);
	RELEASE_CONFIG_BUFFER(buf1);
	free(the_module);
	return TRUE;
}

int depmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int depmod_main(int ATTRIBUTE_UNUSED argc, char **argv)
{
	int retval = EXIT_SUCCESS;
//	static const char moddir_base[] ALIGN1 = "/lib/modules/%s";
	FILE *filedes = xfopen("/tmp/modules.dep", "w");

	argv++;
	do {
		if (!recursive_action(*argv,
				ACTION_RECURSE, /* flags */
				fill_lst, /* file action */
				NULL, /* dir action */
				NULL, /* user data */
				0) || /* depth */
			!recursive_action(*argv,
				ACTION_RECURSE, /* flags */
				fileAction, /* file action */
				NULL, /* dir action */
				(void*)filedes, /* user data */
				0)) { /* depth */
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	if (ENABLE_FEATURE_CLEAN_UP)
		fclose(filedes);
	return retval;
}
