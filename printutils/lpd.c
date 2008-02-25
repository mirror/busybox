/* vi: set sw=4 ts=4: */
/*
 * micro lpd - a small non-queueing lpd
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

int lpd_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int lpd_main(int argc, char *argv[])
{
	char *s;

	// goto spool directory
	// spool directory contains either links to real printer devices or just simple files
	// these links or files are called "queues"
	if (!argv[1])
		bb_show_usage();

	xchdir(argv[1]);

	xdup2(1, 2);

	// read command
	s = xmalloc_reads(STDIN_FILENO, NULL);

	// N.B. we keep things simple
	// only "receive job" command is meaningful here...
	if (2 == *s) {
		char *queue;

		// parse command: "\x2QUEUE_NAME\n"
		queue = s + 1;
		*strchrnul(s, '\n') = '\0';

		while (1) {
			// signal OK
			write(STDOUT_FILENO, "", 1);
			// get subcommand
			s = xmalloc_reads(STDIN_FILENO, NULL);
			if (!s)
				return EXIT_SUCCESS; // EOF (probably)
			// valid s must be of form: SUBCMD | LEN | SP | FNAME
			// N.B. we bail out on any error
			// control or data file follows
			if (2 == s[0] || 3 == s[0]) {
				int fd;
				size_t len;
				// 2: control file (ignoring), 3: data file
				fd = -1;
				if (3 == s[0])
					fd = xopen(queue, O_RDWR | O_APPEND);
				// get data length
				*strchrnul(s, ' ') = '\0';
				len = xatou(s + 1);
				// dump exactly len bytes to file, or die
				bb_copyfd_exact_size(STDIN_FILENO, fd, len);
				close(fd); // NB: can do close(-1). Who cares?
				free(s);
				// got no ACK? -> bail out
				if (safe_read(STDIN_FILENO, s, 1) <= 0 || s[0]) {
					// don't talk to peer - it obviously
					// don't follow the protocol
					return EXIT_FAILURE;
				}
			} else {
				// any other subcommand aborts receiving job
				// N.B. abort subcommand itself doesn't contain
				// fname so it failed earlier...
				break;
			}
		}
	}

	printf("Command %02x not supported\n", (unsigned char)*s);
	return EXIT_FAILURE;
}
