/* vi: set sw=4 ts=4: */
/* nohup - invoke a utility immune to hangups.
 *
 * Busybox version based on nohup specification at
 * http://www.opengroup.org/onlinepubs/007904975/utilities/nohup.html
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 * Copyright 2006 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

int nohup_main(int argc, char **argv);
int nohup_main(int argc, char **argv)
{
	int nullfd;
	const char *nohupout;
	char *home = NULL;

	xfunc_error_retval = 127;

	if (argc < 2) bb_show_usage();

	nullfd = xopen(bb_dev_null, O_WRONLY|O_APPEND);
	/* If stdin is a tty, detach from it. */
	if (isatty(STDIN_FILENO))
		dup2(nullfd, STDIN_FILENO);

	nohupout = "nohup.out";
	/* Redirect stdout to nohup.out, either in "." or in "$HOME". */
	if (isatty(STDOUT_FILENO)) {
		close(STDOUT_FILENO);
		if (open(nohupout, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR) < 0) {
			home = getenv("HOME");
			if (home) {
				nohupout = concat_path_file(home, nohupout);
				xopen3(nohupout, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR);
			}
		}
	} else dup2(nullfd, STDOUT_FILENO);

	/* If we have a tty on stderr, announce filename and redirect to stdout.
	 * Else redirect to /dev/null.
	 */
	if (isatty(STDERR_FILENO)) {
		bb_error_msg("appending to %s", nohupout);
		dup2(STDOUT_FILENO, STDERR_FILENO);
	} else dup2(nullfd, STDERR_FILENO);

	if (nullfd > 2)
		close(nullfd);
	signal(SIGHUP, SIG_IGN);

	BB_EXECVP(argv[1], argv+1);
	if (ENABLE_FEATURE_CLEAN_UP && home)
		free((char*)nohupout);
	bb_perror_msg_and_die("%s", argv[1]);
}
