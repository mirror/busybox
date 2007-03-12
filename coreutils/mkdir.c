/* vi: set sw=4 ts=4: */
/*
 * Mini mkdir implementation for busybox
 *
 * Copyright (C) 2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/mkdir.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Fixed broken permission setting when -p was used; especially in
 * conjunction with -m.
 */

/* Nov 28, 2006      Yoshinori Sato <ysato@users.sourceforge.jp>: Add SELinux Support.
 */

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h> /* struct option */
#include "busybox.h"

#if ENABLE_FEATURE_MKDIR_LONG_OPTIONS
static const struct option mkdir_long_options[] = {
	{ "mode", 1, NULL, 'm' },
	{ "parents", 0, NULL, 'p' },
#if ENABLE_SELINUX
	{ "context", 1, NULL, 'Z' },
#endif
	{ 0, 0, 0, 0 }
};
#endif

int mkdir_main(int argc, char **argv);
int mkdir_main(int argc, char **argv)
{
	mode_t mode = (mode_t)(-1);
	int status = EXIT_SUCCESS;
	int flags = 0;
	unsigned opt;
	char *smode;
#if ENABLE_SELINUX
	security_context_t scontext;
#endif

#if ENABLE_FEATURE_MKDIR_LONG_OPTIONS
	applet_long_options = mkdir_long_options;
#endif
	opt = getopt32(argc, argv, "m:p" USE_SELINUX("Z:"), &smode USE_SELINUX(,&scontext));
	if (opt & 1) {
		mode = 0777;
		if (!bb_parse_mode(smode, &mode)) {
			bb_error_msg_and_die("invalid mode '%s'", smode);
		}
	}
	if (opt & 2)
		flags |= FILEUTILS_RECUR;
#if ENABLE_SELINUX
	if (opt & 4) {
		selinux_or_die();
		setfscreatecon_or_die(scontext);
	}
#endif

	if (optind == argc) {
		bb_show_usage();
	}

	argv += optind;

	do {
		if (bb_make_directory(*argv, mode, flags)) {
			status = EXIT_FAILURE;
		}
	} while (*++argv);

	return status;
}
