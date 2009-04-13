/*
 * Common modutils related functions for busybox
 *
 * Copyright (C) 2008 by Timo Teras <timo.teras@iki.fi>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#ifndef MODUTILS_H
#define MODUTILS_H 1

#include "libbb.h"

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

/* linux/include/linux/module.h has 64, but this is also used
 * internally for the maximum alias name length, which can be quite long */
#define MODULE_NAME_LEN 256

const char *moderror(int err) FAST_FUNC;
void replace(char *s, char what, char with) FAST_FUNC;
char *replace_underscores(char *s) FAST_FUNC;
int string_to_llist(char *string, llist_t **llist, const char *delim) FAST_FUNC;
char *filename2modname(const char *filename, char *modname) FAST_FUNC;
char *parse_cmdline_module_options(char **argv) FAST_FUNC;

#define INSMOD_OPTS \
	"vq" \
	USE_FEATURE_2_4_MODULES("sLo:fkx") \
	USE_FEATURE_INSMOD_LOAD_MAP("m")

#define INSMOD_ARGS USE_FEATURE_2_4_MODULES(, NULL)

enum {
	INSMOD_OPT_VERBOSE	= 0x0001,
	INSMOD_OPT_SILENT	= 0x0002,
	INSMOD_OPT_SYSLOG	= 0x0004 * ENABLE_FEATURE_2_4_MODULES,
	INSMOD_OPT_LOCK		= 0x0008 * ENABLE_FEATURE_2_4_MODULES,
	INSMOD_OPT_OUTPUTNAME	= 0x0010 * ENABLE_FEATURE_2_4_MODULES,
	INSMOD_OPT_FORCE	= 0x0020 * ENABLE_FEATURE_2_4_MODULES,
	INSMOD_OPT_KERNELD	= 0x0040 * ENABLE_FEATURE_2_4_MODULES,
	INSMOD_OPT_NO_EXPORT	= 0x0080 * ENABLE_FEATURE_2_4_MODULES,
	INSMOD_OPT_PRINT_MAP	= 0x0100 * ENABLE_FEATURE_INSMOD_LOAD_MAP,
#if ENABLE_FEATURE_2_4_MODULES
# if ENABLE_FEATURE_INSMOD_LOAD_MAP
	INSMOD_OPT_UNUSED	= 0x0200,
# else
	INSMOD_OPT_UNUSED	= 0x0100,
# endif
#else
	INSMOD_OPT_UNUSED	= 0x0004,
#endif
};

int FAST_FUNC bb_init_module(const char *module, const char *options);
int FAST_FUNC bb_delete_module(const char *module, unsigned int flags);

#if ENABLE_FEATURE_2_4_MODULES
int FAST_FUNC bb_init_module_24(const char *module, const char *options);
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
