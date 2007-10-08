/* vi: set sw=4 ts=4: */
/*
 * Support for main() which needs to end up in libbusybox, not busybox,
 * if one builds libbusybox.
 *
 * Copyright (C) 2007 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file License in this tarball for details.
 */

#include <assert.h>
#include "busybox.h"


/* Declare <applet>_main() */
#define PROTOTYPES
#include "applets.h"
#undef PROTOTYPES

#if ENABLE_SHOW_USAGE && !ENABLE_FEATURE_COMPRESS_USAGE
/* Define usage_messages[] */
static const char usage_messages[] ALIGN1 = ""
#define MAKE_USAGE
#include "usage.h"
#include "applets.h"
;
#undef MAKE_USAGE
#else
#define usage_messages 0
#endif /* SHOW_USAGE */

/* Define struct bb_applet applets[] */
#include "applets.h"

#if ENABLE_FEATURE_SH_STANDALONE
/* -1 because last entry is NULL */
const unsigned short NUM_APPLETS = ARRAY_SIZE(applets) - 1;
#endif


#if ENABLE_FEATURE_COMPRESS_USAGE

#include "usage_compressed.h"
#include "unarchive.h"

static const char *unpack_usage_messages(void)
{
	char *outbuf = NULL;
	bunzip_data *bd;
	int i;

	i = start_bunzip(&bd,
			/* src_fd: */ -1,
			/* inbuf:  */ packed_usage,
			/* len:    */ sizeof(packed_usage));
	/* read_bunzip can longjmp to start_bunzip, and ultimately
	 * end up here with i != 0 on read data errors! Not trivial */
	if (!i) {
		/* Cannot use xmalloc: will leak bd in NOFORK case! */
		outbuf = malloc_or_warn(SIZEOF_usage_messages);
		if (outbuf)
			read_bunzip(bd, outbuf, SIZEOF_usage_messages);
	}
	dealloc_bunzip(bd);
	return outbuf;
}
#define dealloc_usage_messages(s) free(s)

#else

#define unpack_usage_messages() usage_messages
#define dealloc_usage_messages(s) ((void)(s))

#endif /* FEATURE_COMPRESS_USAGE */


void bb_show_usage(void)
{
	if (ENABLE_SHOW_USAGE) {
		const char *format_string;
		const char *p;
		const char *usage_string = p = unpack_usage_messages();
		const struct bb_applet *ap = find_applet_by_name(applet_name);
		int i;

		if (!ap) /* never happens, paranoia */
			xfunc_die();

		i = ap - applets;
		while (i) {
			while (*p++) continue;
			i--;
		}

		fprintf(stderr, "%s multi-call binary\n", bb_banner);
		format_string = "\nUsage: %s %s\n\n";
		if (*p == '\b')
			format_string = "\nNo help available.\n\n";
		fprintf(stderr, format_string, applet_name, p);
		dealloc_usage_messages((char*)usage_string);
	}
	xfunc_die();
}


static int applet_name_compare(const void *name, const void *vapplet)
{
	const struct bb_applet *applet = vapplet;

	return strcmp(name, applet->name);
}

const struct bb_applet *find_applet_by_name(const char *name)
{
	/* Do a binary search to find the applet entry given the name. */
	return bsearch(name, applets, ARRAY_SIZE(applets)-1, sizeof(applets[0]),
				applet_name_compare);
}


#ifdef __GLIBC__
/* Make it reside in R/W memory: */
int *const bb_errno __attribute__ ((section (".data")));
#endif

void bbox_prepare_main(char **argv)
{
#ifdef __GLIBC__
	(*(int **)&bb_errno) = __errno_location();
#endif

	/* Set locale for everybody except 'init' */
	if (ENABLE_LOCALE_SUPPORT && getpid() != 1)
		setlocale(LC_ALL, "");

	/* Redundant for busybox, but needed for individual applets */
	if (argv[1] && strcmp(argv[1], "--help") == 0)
		bb_show_usage();
}
