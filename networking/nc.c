/* vi: set sw=4 ts=4: */
/*  nc: mini-netcat - built from the ground up for LRP
 *
 *  Copyright (C) 1998, 1999  Charles P. Wright
 *  Copyright (C) 1998  Dave Cinege
 *
 *  Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#if ENABLE_DESKTOP
#include "nc_bloaty.c"
#else

/* Lots of small differences in features
 * when compared to "standard" nc
 */

static void timeout(int signum ATTRIBUTE_UNUSED)
{
	bb_error_msg_and_die("timed out");
}

int nc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nc_main(int argc, char **argv)
{
	/* sfd sits _here_ only because of "repeat" option (-l -l). */
	int sfd = sfd; /* for gcc */
	int cfd = 0;
	unsigned lport = 0;
	SKIP_NC_SERVER(const) unsigned do_listen = 0;
	SKIP_NC_EXTRA (const) unsigned wsecs = 0;
	SKIP_NC_EXTRA (const) unsigned delay = 0;
	SKIP_NC_EXTRA (const int execparam = 0;)
	USE_NC_EXTRA  (char **execparam = NULL;)
	len_and_sockaddr *lsa;
	fd_set readfds, testfds;
	int opt; /* must be signed (getopt returns -1) */

	if (ENABLE_NC_SERVER || ENABLE_NC_EXTRA) {
		/* getopt32 is _almost_ usable:
		** it cannot handle "... -e prog -prog-opt" */
		while ((opt = getopt(argc, argv,
		        "" USE_NC_SERVER("lp:") USE_NC_EXTRA("w:i:f:e:") )) > 0
		) {
			if (ENABLE_NC_SERVER && opt=='l')
				USE_NC_SERVER(do_listen++);
			else if (ENABLE_NC_SERVER && opt=='p')
				USE_NC_SERVER(lport = bb_lookup_port(optarg, "tcp", 0));
			else if (ENABLE_NC_EXTRA && opt=='w')
				USE_NC_EXTRA( wsecs = xatou(optarg));
			else if (ENABLE_NC_EXTRA && opt=='i')
				USE_NC_EXTRA( delay = xatou(optarg));
			else if (ENABLE_NC_EXTRA && opt=='f')
				USE_NC_EXTRA( cfd = xopen(optarg, O_RDWR));
			else if (ENABLE_NC_EXTRA && opt=='e' && optind <= argc) {
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
		if (do_listen || cfd) {
			if (argc) bb_show_usage();
		} else {
			if (!argc || argc > 2) bb_show_usage();
		}
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
		if (do_listen) {
			/* create_and_bind_stream_or_die(NULL, lport)
			 * would've work wonderfully, but we need
			 * to know lsa */
			sfd = xsocket_stream(&lsa);
			if (lport)
				set_nport(lsa, htons(lport));
			setsockopt_reuseaddr(sfd);
			xbind(sfd, &lsa->u.sa, lsa->len);
			xlisten(sfd, do_listen); /* can be > 1 */
			/* If we didn't specify a port number,
			 * query and print it after listen() */
			if (!lport) {
				socklen_t addrlen = lsa->len;
				getsockname(sfd, &lsa->u.sa, &addrlen);
				lport = get_nport(&lsa->u.sa);
				fdprintf(2, "%d\n", ntohs(lport));
			}
			close_on_exec_on(sfd);
 accept_again:
			cfd = accept(sfd, NULL, 0);
			if (cfd < 0)
				bb_perror_msg_and_die("accept");
			if (!execparam)
				close(sfd);
		} else {
			cfd = create_and_connect_stream_or_die(argv[0],
				argv[1] ? bb_lookup_port(argv[1], "tcp", 0) : 0);
		}
	}

	if (wsecs) {
		alarm(0);
		signal(SIGALRM, SIG_DFL);
	}

	/* -e given? */
	if (execparam) {
		signal(SIGCHLD, SIG_IGN);
		// With more than one -l, repeatedly act as server.
		if (do_listen > 1 && vfork()) {
			/* parent */
			// This is a bit weird as cleanup goes, since we wind up with no
			// stdin/stdout/stderr.  But it's small and shouldn't hurt anything.
			// We check for cfd == 0 above.
			logmode = LOGMODE_NONE;
			close(0);
			close(1);
			close(2);
			goto accept_again;
		}
		/* child (or main thread if no multiple -l) */
		if (cfd) {
			dup2(cfd, 0);
			close(cfd);
		}
		dup2(0, 1);
		dup2(0, 2);
		USE_NC_EXTRA(BB_EXECVP(execparam[0], execparam);)
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

#define iobuf bb_common_bufsiz1
		for (fd = 0; fd < FD_SETSIZE; fd++) {
			if (FD_ISSET(fd, &testfds)) {
				nread = safe_read(fd, iobuf, sizeof(iobuf));
				if (fd == cfd) {
					if (nread < 1)
						exit(EXIT_SUCCESS);
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
				xwrite(ofd, iobuf, nread);
				if (delay > 0) sleep(delay);
			}
		}
	}
}
#endif
