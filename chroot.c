/* vi: set sw=4 ts=4: */
/*
 * Mini chroot implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "busybox.h"

int chroot_main(int argc, char **argv)
{
	char *prog;

	if ((argc < 2) || (**(argv + 1) == '-')) {
		show_usage();
	}
	argc--;
	argv++;

	if (chroot(*argv) || (chdir("/"))) {
		perror_msg_and_die("cannot change root directory to %s", *argv);
	}

	argc--;
	argv++;
	if (argc >= 1) {
		prog = *argv;
		execvp(*argv, argv);
	} else {
#if defined shell_main && defined BB_FEATURE_SH_STANDALONE_SHELL
		char shell[] = "/bin/sh";
		char *shell_argv[2] = { shell, NULL };
		applet_name = shell;
		shell_main(1, shell_argv);
		return EXIT_SUCCESS;
#else
		prog = getenv("SHELL");
		if (!prog)
			prog = "/bin/sh";
		execlp(prog, prog, NULL);
#endif
	}
	perror_msg_and_die("cannot execute %s", prog);

}


/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
