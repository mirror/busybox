/* vi: set sw=4 ts=4: */
/*
 *  openvt.c - open a vt to run a command.
 *
 *  busyboxed by Quy Tonthat <quy@signal3.com>
 *  hacked by Tito <farmatito@tiscali.it>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* getopt not needed */

#include "libbb.h"

int openvt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int openvt_main(int argc, char **argv)
{
	char vtname[sizeof(VC_FORMAT) + 2];

	if (argc < 3)
		bb_show_usage();

	/* check for illegal vt number: < 1 or > 63 */
	sprintf(vtname, VC_FORMAT, (int)xatou_range(argv[1], 1, 63));

	bb_daemonize_or_rexec(DAEMON_CLOSE_EXTRA_FDS, argv);
	/* grab new one */
	close(0);
	xopen(vtname, O_RDWR);
	dup2(0, STDOUT_FILENO);
	dup2(0, STDERR_FILENO);

	BB_EXECVP(argv[2], &argv[2]);
	_exit(1);
}
