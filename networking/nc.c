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

static void timeout(int signum UNUSED_PARAM)
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
	IF_NOT_NC_SERVER(const) unsigned do_listen = 0;
	IF_NOT_NC_EXTRA (const) unsigned wsecs = 0;
	IF_NOT_NC_EXTRA (const) unsigned delay = 0;
	IF_NOT_NC_EXTRA (const int execparam = 0;)
	IF_NC_EXTRA     (char **execparam = NULL;)
	fd_set readfds, testfds;
	int opt; /* must be signed (getopt returns -1) */

	if (ENABLE_NC_SERVER || ENABLE_NC_EXTRA) {
		/* getopt32 is _almost_ usable:
		** it cannot handle "... -e prog -prog-opt" */
		while ((opt = getopt(argc, argv,
		        "" IF_NC_SERVER("lp:") IF_NC_EXTRA("w:i:f:e:") )) > 0
		) {
			if (ENABLE_NC_SERVER && opt == 'l')
				IF_NC_SERVER(do_listen++);
			else if (ENABLE_NC_SERVER && opt == 'p')
				IF_NC_SERVER(lport = bb_lookup_port(optarg, "tcp", 0));
			else if (ENABLE_NC_EXTRA && opt == 'w')
				IF_NC_EXTRA( wsecs = xatou(optarg));
			else if (ENABLE_NC_EXTRA && opt == 'i')
				IF_NC_EXTRA( delay = xatou(optarg));
			else if (ENABLE_NC_EXTRA && opt == 'f')
				IF_NC_EXTRA( cfd = xopen(optarg, O_RDWR));
			else if (ENABLE_NC_EXTRA && opt == 'e' && optind <= argc) {
				/* We cannot just 'break'. We should let getopt finish.
				** Or else we won't be able to find where
				** 'host' and 'port' params are
				** (think "nc -w 60 host port -e prog"). */
				IF_NC_EXTRA(
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
		// File mode needs need zero arguments, listen mode needs zero or one,
		// client mode needs one or two
		if (cfd) {
			if (argc) bb_show_usage();
		} else if (do_listen) {
			if (argc > 1) bb_show_usage();
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
			sfd = create_and_bind_stream_or_die(argv[0], lport);
			xlisten(sfd, do_listen); /* can be > 1 */
#if 0  /* nc-1.10 does not do this (without -v) */
			/* If we didn't specify a port number,
			 * query and print it after listen() */
			if (!lport) {
				len_and_sockaddr lsa;
				lsa.len = LSA_SIZEOF_SA;
				getsockname(sfd, &lsa.u.sa, &lsa.len);
				lport = get_nport(&lsa.u.sa);
				fdprintf(2, "%d\n", ntohs(lport));
			}
#endif
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
		/* Non-ignored signals revert to SIG_DFL on exec anyway */
		/*signal(SIGALRM, SIG_DFL);*/
	}

	/* -e given? */
	if (execparam) {
		pid_t pid;
		/* With more than one -l, repeatedly act as server */
		if (do_listen > 1 && (pid = vfork()) != 0) {
			/* parent or error */
			if (pid < 0)
				bb_perror_msg_and_die("vfork");
			/* prevent zombies */
			signal(SIGCHLD, SIG_IGN);
			close(cfd);
			goto accept_again;
		}
		/* child, or main thread if only one -l */
		xmove_fd(cfd, 0);
		xdup2(0, 1);
		xdup2(0, 2);
		IF_NC_EXTRA(BB_EXECVP(execparam[0], execparam);)
		/* Don't print stuff or it will go over the wire... */
		_exit(127);
	}

	/* Select loop copying stdin to cfd, and cfd to stdout */

	FD_ZERO(&readfds);
	FD_SET(cfd, &readfds);
	FD_SET(STDIN_FILENO, &readfds);

	for (;;) {
		int fd;
		int ofd;
		int nread;

		testfds = readfds;

		if (select(cfd + 1, &testfds, NULL, NULL, NULL) < 0)
			bb_perror_msg_and_die("select");

#define iobuf bb_common_bufsiz1
		fd = STDIN_FILENO;
		while (1) {
			if (FD_ISSET(fd, &testfds)) {
				nread = safe_read(fd, iobuf, sizeof(iobuf));
				if (fd == cfd) {
					if (nread < 1)
						exit(EXIT_SUCCESS);
					ofd = STDOUT_FILENO;
				} else {
					if (nread < 1) {
						/* Close outgoing half-connection so they get EOF,
						 * but leave incoming alone so we can see response */
						shutdown(cfd, 1);
						FD_CLR(STDIN_FILENO, &readfds);
					}
					ofd = cfd;
				}
				xwrite(ofd, iobuf, nread);
				if (delay > 0)
					sleep(delay);
			}
			if (fd == cfd)
				break;
			fd = cfd;
		}
	}
}
#endif
