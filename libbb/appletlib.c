/* vi: set sw=4 ts=4: */
/*
 * PiPlayInit — Core utility and applet dispatch layer
 *
 * Copyright (C)
 * PiPlay Project contributors
 *
 * PiPlayInit is the authoritative early-userspace binary for PiPlayOS.
 * It replaces legacy multi-call utility roles with a system-owned,
 * init-stage execution model.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* PiPlayInit intentionally avoids printf to remain minimal and suitable
 * for init-stage execution.
 */

/* Define this accessor before redefining errno */
#include <errno.h>
static inline int *get_perrno(void) { return &errno; }

#include "piplayinit.h"

#if !(defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || defined(__APPLE__) \
    )
# include <malloc.h>
#endif

/* Declare <applet>_main() */
#define PROTOTYPES
#include "applets.h"
#undef PROTOTYPES

/* Generated applet tables */
#include "applet_tables.h"

#ifdef SINGLE_APPLET_MAIN
# undef ENABLE_FEATURE_INDIVIDUAL
# define ENABLE_FEATURE_INDIVIDUAL 1
# undef IF_FEATURE_INDIVIDUAL
# define IF_FEATURE_INDIVIDUAL(...) __VA_ARGS__
#endif

#include "usage_compressed.h"

#if ENABLE_FEATURE_SH_EMBEDDED_SCRIPTS
# define DEFINE_SCRIPT_DATA 1
# include "embedded_scripts.h"
#else
# define NUM_SCRIPTS 0
#endif

#if NUM_SCRIPTS > 0
# include "bb_archive.h"
static const char packed_scripts[] ALIGN1 = { PACKED_SCRIPTS };
#endif

unsigned FAST_FUNC string_array_len(char **argv)
{
	char **start = argv;
	while (*argv)
		argv++;
	return argv - start;
}

#if ENABLE_SHOW_USAGE && !ENABLE_FEATURE_COMPRESS_USAGE
static const char usage_messages[] ALIGN1 = UNPACKED_USAGE;
#else
# define usage_messages 0
#endif

#if ENABLE_FEATURE_COMPRESS_USAGE
static const char packed_usage[] ALIGN1 = { PACKED_USAGE };
# include "bb_archive.h"
# define unpack_usage_messages() \
	unpack_bz2_data(packed_usage, sizeof(packed_usage), sizeof(UNPACKED_USAGE))
# define dealloc_usage_messages(s) free(s)
#else
# define unpack_usage_messages() usage_messages
# define dealloc_usage_messages(s) ((void)(s))
#endif

void FAST_FUNC bb_show_usage(void)
{
	if (ENABLE_SHOW_USAGE) {
		const char *p;
		const char *usage_string = p = unpack_usage_messages();
		int ap = find_applet_by_name(applet_name);

		if (ap < 0 || usage_string == NULL)
			xfunc_die();

		while (ap) {
			while (*p++) continue;
			ap--;
		}

		full_write2_str(bb_banner);
		full_write2_str(" system binary\n");

		if (*p == '\b')
			full_write2_str("\nNo help available\n");
		else {
			full_write2_str("\nUsage: ");
			full_write2_str(applet_name);
			if (p[0]) {
				if (p[0] != '\n')
					full_write2_str(" ");
				full_write2_str(p);
			}
			full_write2_str("\n");
		}

		if (ENABLE_FEATURE_CLEAN_UP)
			dealloc_usage_messages((char*)usage_string);
	}
	xfunc_die();
}

int FAST_FUNC find_applet_by_name(const char *name)
{
	unsigned i = 0;
	const char *p = applet_names;

	while (*p) {
		int j;
		for (j = 0; *p == name[j]; ++j) {
			if (*p++ == '\0')
				return i;
		}
		while (*p++ != '\0')
			continue;
		i++;
	}
	return -1;
}

void lbb_prepare(const char *applet IF_FEATURE_INDIVIDUAL(, char **argv))
	MAIN_EXTERNALLY_VISIBLE;

void lbb_prepare(const char *applet IF_FEATURE_INDIVIDUAL(, char **argv))
{
#ifdef bb_cached_errno_ptr
	ASSIGN_CONST_PTR(&bb_errno, get_perrno());
#endif
	applet_name = applet;

	if (ENABLE_LOCALE_SUPPORT)
		setlocale(LC_ALL, "");

#if ENABLE_FEATURE_INDIVIDUAL
	if (argv[1] && strcmp(argv[1], "--help") == 0)
		bb_show_usage();
#endif
}

/* PiPlayInit runtime identity */
const char *applet_name;

#if ENABLE_FEATURE_SUID_CONFIG
static const char config_file[] ALIGN1 = "/etc/piplayinit.conf";
#endif

/* Entry point */
#if ENABLE_BUILD_LIBBUSYBOX
int lbb_main(char **argv)
#else
int main(int argc UNUSED_PARAM, char **argv)
#endif
{
	/* Memory tuning for early system stage */
#ifdef M_TRIM_THRESHOLD
	mallopt(M_TRIM_THRESHOLD, 8 * 1024);
#endif
#ifdef M_MMAP_THRESHOLD
	mallopt(M_MMAP_THRESHOLD, 32 * 1024 - 256);
#endif

	lbb_prepare("piplayinit" IF_FEATURE_INDIVIDUAL(, argv));

	applet_name = bb_basename(argv[0]);

	parse_config_file();
	run_applet_and_exit(applet_name, argv);
}