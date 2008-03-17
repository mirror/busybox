/* vi: set sw=4 ts=4: */
/*
 * micro lpd
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

// TODO: xmalloc_reads is vulnerable to remote OOM attack!

int lpd_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int lpd_main(int argc ATTRIBUTE_UNUSED, char *argv[])
{
	int spooling;
	char *s, *queue;

	// read command
	s = xmalloc_reads(STDIN_FILENO, NULL);

	// we understand only "receive job" command
	if (2 != *s) {
 unsupported_cmd:
		printf("Command %02x %s\n",
			(unsigned char)s[0], "is not supported");
		return EXIT_FAILURE;
	}

	// spool directory contains either links to real printer devices or just simple files
	// these links or files are called "queues"
	// OR
	// if a directory named as given queue exists within spool directory
	// then LPD enters spooling mode and just dumps both control and data files to it

	// goto spool directory
	if (argv[1])
		xchdir(argv[1]);

	// parse command: "\x2QUEUE_NAME\n"
	queue = s + 1;
	*strchrnul(s, '\n') = '\0';

	// protect against "/../" attacks
	if (queue[0] == '.' || strstr(queue, "/."))
		return EXIT_FAILURE;

	// queue is a directory -> chdir to it and enter spooling mode
	spooling = chdir(queue) + 1; /* 0: cannot chdir, 1: done */

	xdup2(STDOUT_FILENO, STDERR_FILENO);

	while (1) {
		char *fname;
		int fd;
		// int is easier than ssize_t: can use xatoi_u,
		// and can correctly display error returns (-1)
		int expected_len, real_len;

		// signal OK
		write(STDOUT_FILENO, "", 1);

		// get subcommand
		s = xmalloc_reads(STDIN_FILENO, NULL);
		if (!s)
			return EXIT_SUCCESS; // probably EOF
		// we understand only "control file" or "data file" cmds
		if (2 != s[0] && 3 != s[0])
			goto unsupported_cmd;

		*strchrnul(s, '\n') = '\0';
		// valid s must be of form: SUBCMD | LEN | SP | FNAME
		// N.B. we bail out on any error
		fname = strchr(s, ' ');
		if (!fname) {
			printf("Command %02x %s\n",
				(unsigned char)s[0], "lacks filename");
			return EXIT_FAILURE;
		}
		*fname++ = '\0';
		if (spooling) {
			// spooling mode: dump both files
			// make "/../" attacks in file names ineffective
			xchroot(".");
			// job in flight has mode 0200 "only writable"
			fd = xopen3(fname, O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 0200);
		} else {
			// non-spooling mode:
			// 2: control file (ignoring), 3: data file
			fd = -1;
			if (3 == s[0])
				fd = xopen(queue, O_RDWR | O_APPEND);
		}
		expected_len = xatoi_u(s + 1);
		real_len = bb_copyfd_size(STDIN_FILENO, fd, expected_len);
		if (spooling && real_len != expected_len) {
			unlink(fname); // don't keep corrupted files
			printf("Expected %d but got %d bytes\n",
				expected_len, real_len);
			return EXIT_FAILURE;
		}
		// get ACK and see whether it is NUL (ok)
		if (read(STDIN_FILENO, s, 1) != 1 || s[0] != 0) {
			// don't send error msg to peer - it obviously
			// don't follow the protocol, so probably
			// it can't understand us either
			return EXIT_FAILURE;
		}
		// chmod completely downloaded job as "readable+writable"
		if (spooling)
			fchmod(fd, 0600);
		close(fd); // NB: can do close(-1). Who cares?
		free(s);
	} /* while (1) */
}
