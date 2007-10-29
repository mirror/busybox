/* vi: set sw=4 ts=4: */
/*
 * Disallocate virtual terminal(s)
 *
 * Copyright (C) 2003 by Tito Ragusa <farmatito@tiscali.it>
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* no options, no getopt */

#include "libbb.h"

/* From <linux/vt.h> */
enum { VT_DISALLOCATE = 0x5608 }; /* free memory associated to vt */

int deallocvt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int deallocvt_main(int argc, char **argv)
{
	/* num = 0 deallocate all unused consoles */
	int num = 0;

	switch (argc) {
	case 2:
		num = xatou_range(argv[1], 1, 63);
		/* Fallthrough */
	case 1:
		break;
	default:
		bb_show_usage();
	}

	/* double cast suppresses "cast to ptr from int of different size" */
	xioctl(get_console_fd(), VT_DISALLOCATE, (void *)(ptrdiff_t)num);
	return EXIT_SUCCESS;
}
