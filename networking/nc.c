/* vi: set sw=4 ts=4: */
/*  nc: mini-netcat - built from the ground up for LRP
    Copyright (C) 1998  Charles P. Wright

    0.0.1   6K      It works.
    0.0.2   5K      Smaller and you can also check the exit condition if you wish.
    0.0.3	    Uses select()	

    19980918 Busy Boxed! Dave Cinege
    19990512 Uses Select. Charles P. Wright
    19990513 Fixes stdin stupidity and uses buffers.  Charles P. Wright

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "busybox.h"

#define GAPING_SECURITY_HOLE

int nc_main(int argc, char **argv)
{
	int do_listen = 0, lport = 0, delay = 0, tmpfd, opt, sfd, x;
	char buf[BUFSIZ];
#ifdef GAPING_SECURITY_HOLE
	char * pr00gie = NULL;                  
#endif

	struct sockaddr_in address;
	struct hostent *hostinfo;

	fd_set readfds, testfds;

	while ((opt = getopt(argc, argv, "lp:i:e:")) > 0) {
		switch (opt) {
			case 'l':
				do_listen++;
				break;
			case 'p':
				lport = atoi(optarg);
				break;
			case 'i':
				delay = atoi(optarg);
				break;
#ifdef GAPING_SECURITY_HOLE
			case 'e':
				pr00gie = optarg;
				break;
#endif
			default:
				bb_show_usage();
		}
	}

#ifdef GAPING_SECURITY_HOLE
	if (pr00gie) {
		/* won't need stdin */
		close (fileno(stdin));                          
	}
#endif /* GAPING_SECURITY_HOLE */


	if ((do_listen && optind != argc) || (!do_listen && optind + 2 != argc))
		bb_show_usage();

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		bb_perror_msg_and_die("socket");
	x = 1;
	if (setsockopt (sfd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof (x)) == -1)
		bb_perror_msg_and_die ("reuseaddr failed");
	address.sin_family = AF_INET;

	if (lport != 0) {
		memset(&address.sin_addr, 0, sizeof(address.sin_addr));
		address.sin_port = htons(lport);

		if (bind(sfd, (struct sockaddr *) &address, sizeof(address)) < 0)
			bb_perror_msg_and_die("bind");
	}

	if (do_listen) {
		socklen_t addrlen = sizeof(address);

		if (listen(sfd, 1) < 0)
			bb_perror_msg_and_die("listen");

		if ((tmpfd = accept(sfd, (struct sockaddr *) &address, &addrlen)) < 0)
			bb_perror_msg_and_die("accept");

		close(sfd);
		sfd = tmpfd;
	} else {
		hostinfo = xgethostbyname(argv[optind]);

		address.sin_addr = *(struct in_addr *) *hostinfo->h_addr_list;
		address.sin_port = htons(atoi(argv[optind+1]));

		if (connect(sfd, (struct sockaddr *) &address, sizeof(address)) < 0)
			bb_perror_msg_and_die("connect");
	}

#ifdef GAPING_SECURITY_HOLE
	/* -e given? */
	if (pr00gie) {
		dup2(sfd, 0);
		close(sfd);
		dup2 (0, 1);
		dup2 (0, 2);
		execl (pr00gie, pr00gie, NULL);
		/* Don't print stuff or it will go over the wire.... */
		_exit(-1);
	}
#endif /* GAPING_SECURITY_HOLE */


	FD_ZERO(&readfds);
	FD_SET(sfd, &readfds);
	FD_SET(STDIN_FILENO, &readfds);

	while (1) {
		int fd;
		int ofd;
		int nread;

		testfds = readfds;

		if (select(FD_SETSIZE, &testfds, NULL, NULL, NULL) < 0)
			bb_perror_msg_and_die("select");

		for (fd = 0; fd < FD_SETSIZE; fd++) {
			if (FD_ISSET(fd, &testfds)) {
				if ((nread = safe_read(fd, buf, sizeof(buf))) < 0)
					bb_perror_msg_and_die("read");

				if (fd == sfd) {
					if (nread == 0)
						exit(0);
					ofd = STDOUT_FILENO;
				} else {
					if (nread == 0)
						shutdown(sfd, 1);
					ofd = sfd;
				}

				if (bb_full_write(ofd, buf, nread) < 0)
					bb_perror_msg_and_die("write");
				if (delay > 0) {
					sleep(delay);
				}
			}
		}
	}
}
