/* vi: set sw=4 ts=4: */
/*
 * micro lpd
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/*
 * A typical usage of BB lpd looks as follows:
 * # tcpsvd -E 0 515 lpd SPOOLDIR [HELPER-PROG [ARGS...]]
 * 
 * This means a network listener is started on port 515 (default for LP protocol). 
 * When a client connection is made (via lpr) lpd first change its working directory to SPOOLDIR.
 * 
 * SPOOLDIR is the spool directory which contains printing queues 
 * and should have the following structure:
 * 
 * SPOOLDIR/
 * 	<queue1>
 * 	...
 * 	<queueN>
 * 
 * <queueX> can be of two types:
 * 	A. a printer character device or an ordinary file a link to such;
 * 	B. a directory.
 * 
 * In case A lpd just dumps the data it receives from client (lpr) to the 
 * end of queue file/device. This is non-spooling mode.
 * 
 * In case B lpd enters spooling mode. It reliably saves client data along with control info 
 * in two unique files under the queue directory. These files are named dfAXXXHHHH and cfAXXXHHHH, 
 * where XXX is the job number and HHHH is the client hostname. Unless a printing helper application 
 * is specified lpd is done at this point.
 * 
 * If HELPER-PROG (with optional arguments) is specified then lpd continues to process client data:
 * 	1. it reads and parses control file (cfA...). The parse process results in setting environment 
 * 	variables whose values were passed in control file; when parsing is complete, lpd deletes 
 * 	control file.
 * 	2. it spawns specified helper application. It is then the helper application who is responsible 
 * 	for both actual printing and deleting processed data file.
 * 
 * A good lpr passes control files which when parsed provide the following variables:
 * $H = host which issues the job
 * $P = user who prints
 * $C = class of printing (what is printed on banner page)
 * $J = the name of the job
 * $L = print banner page
 * $M = the user to whom a mail should be sent if a problem occurs
 * $l = name of datafile ("dfAxxx") - file whose content are to be printed
 * 
 * Thus, a typical helper can be something like this:
 * #!/bin/sh
 * cat "$l" >/dev/lp0
 * mv -f "$l" save/
 * 
 */
#include "libbb.h"

// TODO: xmalloc_reads is vulnerable to remote OOM attack!

// strip argument of bad chars
static char *sane(char *str)
{
	char *s = str;
	char *p = s;
	while (*s) {
		if (isalnum(*s) || '-' == *s) {
			*p++ = *s;
		}
		s++;
	}
	*p = '\0';
	return str;
}

static void exec_helper(const char *fname, char **argv)
{
	char *p, *q, *file;
	char *our_env[12];
	int env_idx;

	// read control file
	file = q = xmalloc_open_read_close(fname, NULL);
	// delete control file
	unlink(fname);
	// parse control file by "\n"
	env_idx = 0;
	while ((p = strchr(q, '\n')) != NULL
	 && isalpha(*q)
	 && env_idx < ARRAY_SIZE(our_env)
	) {
		*p++ = '\0';
		// here q is a line of <SYM><VALUE>
		// let us set environment string <SYM>=<VALUE>
		// N.B. setenv is leaky!
		// We have to use putenv(malloced_str),
		// and unsetenv+free (in parent)
		our_env[env_idx] = xasprintf("%c=%s", *q, q+1);
		putenv(our_env[env_idx]);
		env_idx++;
		// next line, plz!
		q = p;
	}

	if (vfork() == 0) {
		// CHILD
		// we are the helper. we wanna be silent
		// this call reopens stdio fds to "/dev/null"
		// (no daemonization is done)
		bb_daemonize_or_rexec(DAEMON_DEVNULL_STDIO | DAEMON_ONLY_SANITIZE, NULL);
		BB_EXECVP(*argv, argv);
		_exit(127);
	}

	// PARENT (or vfork error)
	// clean up...
	free(file);
	while (--env_idx >= 0) {
		*strchrnul(our_env[env_idx], '=') = '\0';
		unsetenv(our_env[env_idx]);
	}
}

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

	// goto spool directory
	if (*++argv)
		xchdir(*argv++);

	// parse command: "\x2QUEUE_NAME\n"
	queue = s + 1;
	*strchrnul(s, '\n') = '\0';

	// protect against "/../" attacks
	if (!*sane(queue))
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
			// job in flight has mode 0200 "only writable"
			fd = xopen3(sane(fname), O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 0200);
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
		// chmod completely downloaded file as "readable+writable" ...
		if (spooling) {
			fchmod(fd, 0600);
			// ... and accumulate dump state.
			// N.B. after all files are dumped spooling should be 1+2+3==6
			spooling += s[0];
		}
		close(fd); // NB: can do close(-1). Who cares?

		// are all files dumped? -> spawn spool helper
		if (6 == spooling && *argv) {
			fname[0] = 'c'; // pass control file name
			exec_helper(fname, argv);
		}
		// get ACK and see whether it is NUL (ok)
		if (read(STDIN_FILENO, s, 1) != 1 || s[0] != 0) {
			// don't send error msg to peer - it obviously
			// don't follow the protocol, so probably
			// it can't understand us either
			return EXIT_FAILURE;
		}
		free(s);
	} /* while (1) */
}
