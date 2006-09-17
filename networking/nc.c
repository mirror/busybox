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
	bb_error_msg_and_die("Timed out");
}

int nc_main(int argc, char **argv)
{
	int do_listen = 0, lport = 0, delay = 0, wsecs = 0, execflag = 0, opt,
	   	sfd = 0, cfd;
	struct sockaddr_in address;
	struct hostent *hostinfo;
	fd_set readfds, testfds;
	char *infile = NULL;

	memset(&address, 0, sizeof(address));

	if (ENABLE_NC_SERVER || ENABLE_NC_EXTRA) {
		while ((opt = getopt(argc, argv, "lp:" USE_NC_EXTRA("i:ew:f:"))) > 0) {
			if (ENABLE_NC_SERVER && opt=='l') do_listen++;
			else if (ENABLE_NC_SERVER && opt=='p')
				lport = bb_lookup_port(optarg, "tcp", 0);
			else if (ENABLE_NC_EXTRA && opt=='w') wsecs = atoi(optarg);
			else if (ENABLE_NC_EXTRA && opt=='i') delay = atoi(optarg);
			else if (ENABLE_NC_EXTRA && opt=='f') infile = optarg;
			else if (ENABLE_NC_EXTRA && opt=='e' && optind!=argc) {
				execflag++;
				break;
			} else bb_show_usage();
		}
	}


	// For listen or file we need zero arguments, dialout is 2.
	// For exec we need at least one more argument at the end, more ok

	opt = (do_listen  || infile) ? 0 : 2 + execflag;
	if (execflag ? argc-optind < opt : argc-optind!=opt ||
		(infile && do_listen))
			bb_show_usage();

	if (wsecs) {
		signal(SIGALRM, timeout);
		alarm(wsecs);
	}

	if (infile) cfd = xopen(infile, O_RDWR);
	else {
		opt = 1;
		sfd = xsocket(AF_INET, SOCK_STREAM, 0);
		fcntl(sfd, F_SETFD, FD_CLOEXEC);
		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
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
				getsockname(sfd, &address, &len);
				fdprintf(2, "%d\n", SWAP_BE16(address.sin_port));
			}
repeatyness:
			if ((cfd = accept(sfd, (struct sockaddr *) &address, &addrlen)) < 0)
				bb_perror_msg_and_die("accept");

			if (!execflag) close(sfd);
		} else {
			hostinfo = xgethostbyname(argv[optind]);

			address.sin_addr = *(struct in_addr *) *hostinfo->h_addr_list;
			address.sin_port = bb_lookup_port(argv[optind+1], "tcp", 0);

			if (connect(sfd, (struct sockaddr *) &address, sizeof(address)) < 0)
				bb_perror_msg_and_die("connect");
			cfd = sfd;
		}
	}

	if (wsecs) {
		alarm(0);
		signal(SIGALRM, SIG_DFL);
	}

	/* -e given? */
	if (execflag) {
		if(cfd) {
			signal(SIGCHLD, SIG_IGN);
			dup2(cfd, 0);
			close(cfd);
		}
		dup2(0, 1);
		dup2(0, 2);

		// With more than one -l, repeatedly act as server.

		if (do_listen>1 && vfork()) {
			// This is a bit weird as cleanup goes, since we wind up with no
			// stdin/stdout/stderr.  But it's small and shouldn't hurt anything.
			// We check for cfd == 0 above.
			close(0);
			close(1);
			close(2);

			goto repeatyness;
		}
		execvp(argv[optind], argv+optind);
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
