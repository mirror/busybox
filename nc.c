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
#include "internal.h"
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

#define BUFSIZE 100

static const char nc_usage[] = "nc [IP] [port]\n" 
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nNetcat opens a pipe to IP:port\n"
#endif
	;

int nc_main(int argc, char **argv)
{
	int sfd;
	int result;
	int len;
	char ch[BUFSIZE];

	struct sockaddr_in address;
	struct hostent *hostinfo;

	fd_set readfds, testfds;

	argc--;
	argv++;
	if (argc < 2 || **(argv + 1) == '-') {
		usage(nc_usage);
	}

	sfd = socket(AF_INET, SOCK_STREAM, 0);

	hostinfo = (struct hostent *) gethostbyname(*argv);

	if (!hostinfo) {
		fatalError("cannot resolve %s\n", *argv);
	}

	address.sin_family = AF_INET;
	address.sin_addr = *(struct in_addr *) *hostinfo->h_addr_list;
	address.sin_port = htons(atoi(*(++argv)));

	len = sizeof(address);

	result = connect(sfd, (struct sockaddr *) &address, len);

	if (result < 0) {
		perror("nc: connect");
		exit(2);
	}

	FD_ZERO(&readfds);
	FD_SET(sfd, &readfds);
	FD_SET(fileno(stdin), &readfds);

	while (1) {
		int fd;
		int ofd;
		int nread;

		testfds = readfds;

		result =
			select(FD_SETSIZE, &testfds, (fd_set *) NULL, (fd_set *) NULL,
				   (struct timeval *) 0);

		if (result < 1) {
			perror("nc: select");
			exit(3);
		}

		for (fd = 0; fd < FD_SETSIZE; fd++) {
			if (FD_ISSET(fd, &testfds)) {
				int trn = 0;
				int rn;

				ioctl(fd, FIONREAD, &nread);

				if (fd == sfd) {
					if (nread == 0)
						exit(0);
					ofd = fileno(stdout);
				} else {
					ofd = sfd;
				}



				do {
					rn = (BUFSIZE < nread - trn) ? BUFSIZE : nread - trn;
					trn += rn;
					read(fd, ch, rn);
					write(ofd, ch, rn);
				}
				while (trn < nread);
			}
		}
	}
}
