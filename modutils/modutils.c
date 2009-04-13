/*
 * Common modutils related functions for busybox
 *
 * Copyright (C) 2008 by Timo Teras <timo.teras@iki.fi>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "modutils.h"

#ifdef __UCLIBC__
extern int init_module(void *module, unsigned long len, const char *options);
extern int delete_module(const char *module, unsigned int flags);
#else
# include <sys/syscall.h>
# define init_module(mod, len, opts) syscall(__NR_init_module, mod, len, opts)
# define delete_module(mod, flags) syscall(__NR_delete_module, mod, flags)
#endif

void FAST_FUNC replace(char *s, char what, char with)
{
	while (*s) {
		if (what == *s)
			*s = with;
		++s;
	}
}

char * FAST_FUNC replace_underscores(char *s)
{
	replace(s, '-', '_');
	return s;
}

int FAST_FUNC string_to_llist(char *string, llist_t **llist, const char *delim)
{
	char *tok;
	int len = 0;

	while ((tok = strsep(&string, delim)) != NULL) {
		if (tok[0] == '\0')
			continue;
		llist_add_to_end(llist, xstrdup(tok));
		len += strlen(tok);
	}
	return len;
}

char * FAST_FUNC filename2modname(const char *filename, char *modname)
{
	int i;
	char *from;

	if (filename == NULL)
		return NULL;
	if (modname == NULL)
		modname = xmalloc(MODULE_NAME_LEN);
	from = bb_get_last_path_component_nostrip(filename);
	for (i = 0; i < (MODULE_NAME_LEN-1) && from[i] != '\0' && from[i] != '.'; i++)
		modname[i] = (from[i] == '-') ? '_' : from[i];
	modname[i] = 0;

	return modname;
}

const char * FAST_FUNC moderror(int err)
{
	switch (err) {
	case -1:
		return "no such module";
	case ENOEXEC:
		return "invalid module format";
	case ENOENT:
		return "unknown symbol in module, or unknown parameter";
	case ESRCH:
		return "module has wrong symbol version";
	case ENOSYS:
		return "kernel does not support requested operation";
	default:
		return strerror(err);
	}
}

char * FAST_FUNC parse_cmdline_module_options(char **argv)
{
	char *options;
	int optlen;

	options = xzalloc(1);
	optlen = 0;
	while (*++argv) {
		options = xrealloc(options, optlen + 2 + strlen(*argv) + 2);
		/* Spaces handled by "" pairs, but no way of escaping quotes */
		optlen += sprintf(options + optlen, (strchr(*argv, ' ') ? "\"%s\" " : "%s "), *argv);
	}
	return options;
}

int FAST_FUNC bb_init_module(const char *filename, const char *options)
{
	size_t len;
	char *image;
	int rc;

	if (!options)
		options = "";

#if ENABLE_FEATURE_2_4_MODULES
	if (get_linux_version_code() < KERNEL_VERSION(2,6,0))
		return bb_init_module_24(filename, options);
#endif

	/* Use the 2.6 way */
	len = INT_MAX - 4095;
	rc = ENOENT;
	image = xmalloc_open_zipped_read_close(filename, &len);
	if (image) {
		rc = 0;
		if (init_module(image, len, options) != 0)
			rc = errno;
		free(image);
	}

	return rc;
}

int FAST_FUNC bb_delete_module(const char *module, unsigned int flags)
{
	return delete_module(module, flags);
}
