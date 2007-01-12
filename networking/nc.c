/* vi: set sw=4 ts=4: */
/*  nc: mini-netcat - built from the ground up for LRP
 *
 *  Copyright (C) 1998, 1999  Charles P. Wright
 *  Copyright (C) 1998  Dave Cinege
 *
 *  Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

static void timeout(int signum)
{
	bb_error_msg_and_die("timed out");
}

int nc_main(int argc, char **argv)
{
	int sfd = 0;
	int cfd = 0;
	SKIP_NC_SERVER(const) unsigned do_listen = 0;
	SKIP_NC_SERVER(const) unsigned lport = 0;
	SKIP_NC_EXTRA (const) unsigned wsecs = 0;
	SKIP_NC_EXTRA (const) unsigned delay = 0;
	SKIP_NC_EXTRA (const int execparam = 0;)
	USE_NC_EXTRA  (char **execparam = NULL;)
	struct sockaddr_in address;
	fd_set readfds, testfds;
	int opt; /* must be signed (getopt returns -1) */

	memset(&address, 0, sizeof(address));

	if (ENABLE_NC_SERVER || ENABLE_NC_EXTRA) {
		/* getopt32 is _almost_ usable:
		** it cannot handle "... -e prog -prog-opt" */
		while ((opt = getopt(argc, argv,
		        "" USE_NC_SERVER("lp:") USE_NC_EXTRA("w:i:f:e:") )) > 0
		) {
			if (ENABLE_NC_SERVER && opt=='l')      USE_NC_SERVER(do_listen++);
			else if (ENABLE_NC_SERVER && opt=='p') {
				USE_NC_SERVER(lport = bb_lookup_port(optarg, "tcp", 0));
				USE_NC_SERVER(lport = htons(lport));
			}
			else if (ENABLE_NC_EXTRA  && opt=='w') USE_NC_EXTRA( wsecs = xatou(optarg));
			else if (ENABLE_NC_EXTRA  && opt=='i') USE_NC_EXTRA( delay = xatou(optarg));
			else if (ENABLE_NC_EXTRA  && opt=='f') USE_NC_EXTRA( cfd = xopen(optarg, O_RDWR));
			else if (ENABLE_NC_EXTRA  && opt=='e' && optind<=argc) {
				/* We cannot just 'break'. We should let getopt finish.
				** Or else we won't be able to find where
				** 'host' and 'port' params are
				** (think "nc -w 60 host port -e prog"). */
				USE_NC_EXTRA(
					char **p;
					// +2: one for progname (optarg) and one for NULL
					execparam = xzalloc(sizeof(char*) * (argc - optind + 2));
					p = execparam;
					*p++ = optarg;
					while (optind < argc) {
						*p++ = argv[optind++];
					}
				)
				/* optind points to argv[arvc] (NULL) now.
				** FIXME: we assume that getopt will not count options
				** possibly present on "-e prog args" and will not
				** include them into final value of optind
				** which is to be used ...  */
			} else bb_show_usage();
		}
		argv += optind; /* ... here! */
		argc -= optind;
		// -l and -f don't mix
		if (do_listen && cfd) bb_show_usage();
		// Listen or file modes need zero arguments, client mode needs 2
		opt = ((do_listen || cfd) ? 0 : 2);
		if (argc != opt)
			bb_show_usage();
	} else {
		if (argc != 3) bb_show_usage();
		argc--;
		argv++;
	}

	if (wsecs) {
		signal(SIGALRM, timeout);
		alarm(wsecs);
	}

	if (!cfd) {
		sfd = xsocket(AF_INET, SOCK_STREAM, 0);
		fcntl(sfd, F_SETFD, FD_CLOEXEC);
		setsockopt_reuseaddr(sfd);
		address.sin_family = AF_INET;

		// Set local port.

		if (lport != 0) {
			address.sin_port = lport;
			xbind(sfd, (struct sockaddr *) &address, sizeof(address));
		}

		if (do_listen) {
			socklen_t addrlen = sizeof(address);

			xlisten(sfd, do_listen);

			// If we didn't specify a port number, query and print it to stderr.

			if (!lport) {
				socklen_t len = sizeof(address);
				getsockname(sfd, (struct sockaddr *) &address, &len);
				fdprintf(2, "%d\n", SWAP_BE16(address.sin_port));
			}
 repeatyness:
			cfd = accept(sfd, (struct sockaddr *) &address, &addrlen);
			if (cfd < 0)
				bb_perror_msg_and_die("accept");

			if (!execparam) close(sfd);
		} else {
			struct hostent *hostinfo;
			hostinfo = xgethostbyname(argv[0]);

			address.sin_addr = *(struct in_addr *) *hostinfo->h_addr_list;
			address.sin_port = bb_lookup_port(argv[1], "tcp", 0);
			address.sin_port = htons(address.sin_port);

			xconnect(sfd, (struct sockaddr *) &address, sizeof(address));
			cfd = sfd;
		}
	}

	if (wsecs) {
		alarm(0);
		signal(SIGALRM, SIG_DFL);
	}

	/* -e given? */
	if (execparam) {
		if (cfd) {
			signal(SIGCHLD, SIG_IGN);
			dup2(cfd, 0);
			close(cfd);
		}
		dup2(0, 1);
		dup2(0, 2);

		// With more than one -l, repeatedly act as server.

		if (do_listen > 1 && vfork()) {
			// This is a bit weird as cleanup goes, since we wind up with no
			// stdin/stdout/stderr.  But it's small and shouldn't hurt anything.
			// We check for cfd == 0 above.
			logmode = LOGMODE_NONE;
			close(0);
			close(1);
			close(2);

			goto repeatyness;
		}
		USE_NC_EXTRA(execvp(execparam[0], execparam);)
		/* Don't print stuff or it will go over the wire.... */
		_exit(127);
	}

	// Select loop copying stdin to cfd, and cfd to stdout.

	FD_ZERO(&readfds);
	FD_SET(cfd, &readfds);
	FD_SET(STDIN_FILENO, &readfds);

	for (;;) {
		int fd;
		int ofd;
		int nread;

		testfds = readfds;

		if (select(FD_SETSIZE, &testfds, NULL, NULL, NULL) < 0)
			bb_perror_msg_and_die("select");

		for (fd = 0; fd < FD_SETSIZE; fd++) {
			if (FD_ISSET(fd, &testfds)) {
				nread = safe_read(fd, bb_common_bufsiz1,
							sizeof(bb_common_bufsiz1));

				if (fd == cfd) {
					if (nread<1) exit(0);
					ofd = STDOUT_FILENO;
				} else {
					if (nread<1) {
						// Close outgoing half-connection so they get EOF, but
						// leave incoming alone so we can see response.
						shutdown(cfd, 1);
						FD_CLR(STDIN_FILENO, &readfds);
					}
					ofd = cfd;
				}

				xwrite(ofd, bb_common_bufsiz1, nread);
				if (delay > 0) sleep(delay);
			}
		}
	}
}
