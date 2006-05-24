/* vi:set ts=4: */
/* nohup - invoke a utility immune to hangups.
 * 
 * Busybox version based on nohup specification at
 * http://www.opengroup.org/onlinepubs/007904975/utilities/nohup.html
 * 
 * Copyright 2006 Rob Landley <rob@landley.net>
 * 
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "busybox.h"

int nohup_main(int argc, char *argv[])
{
	int temp, nullfd;
	char *nohupout = "nohup.out", *home = NULL;

	// I have no idea why the standard cares about this.

	bb_default_error_retval = 127;

	if (argc<2) bb_show_usage();

	nullfd = bb_xopen(bb_dev_null, O_WRONLY|O_APPEND);
	// If stdin is a tty, detach from it.

	if (isatty(0)) dup2(nullfd, 0);

	// Redirect stdout to nohup.out, either in "." or in "$HOME".

	if (isatty(1)) {
		close(1);
		if (open(nohupout, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR) < 0) {
			home = getenv("HOME");
			if (home) {
				home = concat_path_file(home, nohupout);
				bb_xopen3(nohupout, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR);
			}
		}
	} else dup2(nullfd, 1);

	// If we have a tty on strderr, announce filename and redirect to stdout.
	// Else redirect to /dev/null.

	temp = isatty(2);
	if (temp) fdprintf(2,"Writing to %s\n", home ? home : nohupout);
	dup2(temp ? 1 : nullfd, 2);
	close(nullfd);
	signal (SIGHUP, SIG_IGN);

	// Exec our new program.

	execvp(argv[1],argv+1);
	if (ENABLE_FEATURE_CLEAN_UP) free(home);
	bb_error_msg_and_die("exec %s",argv[1]);
}
