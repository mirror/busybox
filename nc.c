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

#warning This applet has moved to netkit-tiny.  After BusyBox 0.49, this
#warning applet will be removed from BusyBox.  All maintenance efforts
#warning should be done in the netkit-tiny source tree.

#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/ioctl.h>

int nc_main(int argc, char **argv)
{
	int sfd;
	char buf[BUFSIZ];

	struct sockaddr_in address;
	struct hostent *hostinfo;

	fd_set readfds, testfds;

	argc--;
	argv++;
	if (argc < 2 || **argv == '-') {
		usage(nc_usage);
	}

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		perror_msg_and_die("socket");

	if ((hostinfo = gethostbyname(*argv)) == NULL)
		error_msg_and_die("cannot resolve %s\n", *argv);

	address.sin_family = AF_INET;
	address.sin_addr = *(struct in_addr *) *hostinfo->h_addr_list;
	address.sin_port = htons(atoi(*(++argv)));

	if (connect(sfd, (struct sockaddr *) &address, sizeof(address)) < 0)
		perror_msg_and_die("connect");

	FD_ZERO(&readfds);
	FD_SET(sfd, &readfds);
	FD_SET(STDIN_FILENO, &readfds);

	while (1) {
		int fd;
		int ofd;
		int nread;

		testfds = readfds;

		if (select(FD_SETSIZE, &testfds, NULL, NULL, NULL) < 0)
			perror_msg_and_die("select");

		for (fd = 0; fd < FD_SETSIZE; fd++) {
			if (FD_ISSET(fd, &testfds)) {
				if ((nread = safe_read(fd, buf, sizeof(buf))) < 0)
					perror_msg_and_die("read");

				if (fd == sfd) {
					if (nread == 0)
						exit(0);
					ofd = STDOUT_FILENO;
				} else {
					if (nread == 0)
						shutdown(sfd, 1);
					ofd = sfd;
				}

				if (full_write(ofd, buf, nread) < 0)
					perror_msg_and_die("write");
			}
		}
	}
}
