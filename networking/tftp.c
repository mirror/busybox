/* ------------------------------------------------------------------------- */
/* tftp.c                                                                    */
/*                                                                           */
/* A simple tftp client for busybox.                                         */
/* Tries to follow RFC1350.                                                  */
/* Only "octet" mode and 512-byte data blocks are supported.                 */
/*                                                                           */
/* Copyright (C) 2001 Magnus Damm <damm@opensource.se>                       */
/*                                                                           */
/* Parts of the code based on:                                               */
/*                                                                           */
/* atftp:  Copyright (C) 2000 Jean-Pierre Lefebvre <helix@step.polymtl.ca>   */
/*                        and Remi Lefebvre <remi@debian.org>                */
/*                                                                           */
/* utftp:  Copyright (C) 1999 Uwe Ohse <uwe@ohse.de>                         */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU          */
/* General Public License for more details.                                  */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "busybox.h"

//#define BB_FEATURE_TFTP_DEBUG

/* we don't need #ifdefs with these constants and optimization... */

#ifdef BB_FEATURE_TFTP_GET
#define BB_TFTP_GET (1 << 0)
#else
#define BB_TFTP_GET 0
#endif

#ifdef BB_FEATURE_TFTP_PUT
#define BB_TFTP_PUT (1 << 1)
#else
#define BB_TFTP_PUT 0
#endif

#ifdef BB_FEATURE_TFTP_DEBUG
#define BB_TFTP_DEBUG 1
#else
#define BB_TFTP_DEBUG 0
#endif

#define BB_TFTP_NO_RETRIES 5
#define BB_TFTP_TIMEOUT    5	/* seconds */

#define	RRQ	1			/* read request */
#define	WRQ	2			/* write request */
#define	DATA	3		/* data packet */
#define	ACK	4			/* acknowledgement */
#define	ERROR	5		/* error code */

#define BUFSIZE (512+4)

static const char *tftp_error_msg[] = {
	"Undefined error",
	"File not found",
	"Access violation",
	"Disk full or allocation error",
	"Illegal TFTP operation",
	"Unknown transfer ID",
	"File already exists",
	"No such user"
};

static inline int tftp(int cmd, struct hostent *host,
					   char *serverfile, int localfd, int port)
{
	struct sockaddr_in sa;
	int socketfd;
	struct timeval tv;
	fd_set rfds;
	struct sockaddr_in from;
	socklen_t fromlen;
	char *cp;
	unsigned short tmp;
	int len, opcode, finished;
	int timeout, block_nr;

	RESERVE_BB_BUFFER(buf, BUFSIZE);

	opcode = finished = timeout = 0;
	block_nr = 1;

	if ((socketfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror_msg("socket");
		return EXIT_FAILURE;
	}

	len = sizeof(sa);

	memset(&sa, 0, len);
	bind(socketfd, (struct sockaddr *)&sa, len);

	sa.sin_family = host->h_addrtype;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr, (struct in_addr *) host->h_addr,
		   sizeof(sa.sin_addr));

	/* build opcode */

	if (cmd & BB_TFTP_GET) {
		opcode = RRQ;
	}

	if (cmd & BB_TFTP_PUT) {
		opcode = WRQ;
	}

	while (1) {


		/* build packet of type "opcode" */


		cp = buf;

		*((unsigned short *) cp) = htons(opcode);

		cp += 2;

		/* add filename and mode */

		if ((BB_TFTP_GET && (opcode == RRQ)) ||
			(BB_TFTP_PUT && (opcode == WRQ))) {

			while (cp != &buf[BUFSIZE - 1]) {
				if ((*cp = *serverfile++) == '\0')
					break;
				cp++;
			}

			if ((*cp != '\0') || (&buf[BUFSIZE - 1] - cp) < 7) {
				error_msg("too long server-filename");
				break;
			}

			memcpy(cp + 1, "octet", 6);
			cp += 7;
		}

		/* add ack and data */

		if ((BB_TFTP_GET && (opcode == ACK)) ||
			(BB_TFTP_PUT && (opcode == DATA))) {

			*((unsigned short *) cp) = htons(block_nr);

			cp += 2;

			block_nr++;

			if (BB_TFTP_PUT && (opcode == DATA)) {
				len = read(localfd, cp, BUFSIZE - 4);

				if (len < 0) {
					perror_msg("read");
					break;
				}

				if (len != (BUFSIZE - 4)) {
					finished++;
				}

				cp += len;
			} else if (finished) {
				break;
			}
		}


		/* send packet */


		do {

			len = cp - buf;

			if (BB_TFTP_DEBUG) {
				printf("sending %u bytes\n", len);

				for (cp = buf; cp < &buf[len]; cp++)
					printf("%02x ", *cp);
				printf("\n");
			}

			if (sendto(socketfd, buf, len, 0,
					   (struct sockaddr *) &sa, sizeof(sa)) < 0) {
				perror_msg("send");
				len = -1;
				break;
			}


			/* receive packet */


			memset(&from, 0, sizeof(from));
			fromlen = sizeof(from);

			tv.tv_sec = BB_TFTP_TIMEOUT;
			tv.tv_usec = 0;

			FD_ZERO(&rfds);
			FD_SET(socketfd, &rfds);

			switch (select(FD_SETSIZE, &rfds, NULL, NULL, &tv)) {
			case 1:
				len = recvfrom(socketfd, buf,
							   BUFSIZE, 0,
							   (struct sockaddr *) &from, &fromlen);

				if (len < 0) {
					perror_msg("recvfrom");
					break;
				}

				timeout = 0;

				if (sa.sin_port == htons(port)) {
					sa.sin_port = from.sin_port;
					break;
				}

				if (sa.sin_port == from.sin_port) {
					break;
				}

				/* fall-through for bad packets! */
				/* discard the packet - treat as timeout */

			case 0:
				error_msg("timeout");

				if (!timeout) {
					timeout = BB_TFTP_NO_RETRIES;
				} else {
					timeout--;
				}

				if (!timeout) {
					len = -1;
					error_msg("last timeout");
				}
				break;

			default:
				perror_msg("select");
				len = -1;
			}

		} while (timeout && (len >= 0));

		if (len < 0) {
			break;
		}

		/* process received packet */


		opcode = ntohs(*((unsigned short *) buf));
		tmp = ntohs(*((unsigned short *) &buf[2]));

		if (BB_TFTP_DEBUG) {
			printf("received %d bytes: %04x %04x\n", len, opcode, tmp);
		}

		if (BB_TFTP_GET && (opcode == DATA)) {

			if (tmp == block_nr) {
				len = write(localfd, &buf[4], len - 4);

				if (len < 0) {
					perror_msg("write");
					break;
				}

				if (len != (BUFSIZE - 4)) {
					finished++;
				}

				opcode = ACK;
				continue;
			}
		}

		if (BB_TFTP_PUT && (opcode == ACK)) {

			if (tmp == (block_nr - 1)) {
				if (finished) {
					break;
				}

				opcode = DATA;
				continue;
			}
		}

		if (opcode == ERROR) {
			char *msg = NULL;

			if (buf[4] != '\0') {
				msg = &buf[4];
				buf[BUFSIZE - 1] = '\0';
			} else if (tmp < (sizeof(tftp_error_msg) / sizeof(char *))) {
				msg = (char *) tftp_error_msg[tmp];
			}

			if (msg) {
				error_msg("server says: %s", msg);
			}

			break;
		}
	}

	close(socketfd);

	return finished ? EXIT_SUCCESS : EXIT_FAILURE;
}

int tftp_main(int argc, char **argv)
{
	char *cp, *s;
	char *serverstr;
	struct hostent *host;
	char *serverfile;
	char *localfile;
	int cmd, flags, fd, bad;

	host = (void *) serverstr = serverfile = localfile = NULL;
	flags = cmd = 0;
	bad = 1;

	if (argc > 3) {
		if (BB_TFTP_GET && (strcmp(argv[1], "get") == 0)) {
			cmd = BB_TFTP_GET;
			flags = O_WRONLY | O_CREAT;
			serverstr = argv[2];
			localfile = argv[3];
		}

		if (BB_TFTP_PUT && (strcmp(argv[1], "put") == 0)) {
			cmd = BB_TFTP_PUT;
			flags = O_RDONLY;
			localfile = argv[2];
			serverstr = argv[3];
		}

	}

	if (!(cmd & (BB_TFTP_GET | BB_TFTP_PUT))) {
		show_usage();
	}

	for (cp = serverstr; *cp != '\0'; cp++)
		if (*cp == ':')
			break;

	if (*cp == ':') {

		serverfile = cp + 1;

		s = xstrdup(serverstr);
		s[cp - serverstr] = '\0';

		host = xgethostbyname(s);

		free(s);
	}

	if (BB_TFTP_DEBUG) {
		printf("using server \"%s\", serverfile \"%s\","
			   "localfile \"%s\".\n",
			   inet_ntoa(*((struct in_addr *) host->h_addr)),
			   serverfile, localfile);
	}

	if ((fd = open(localfile, flags, 0644)) < 0) {
		perror_msg_and_die("local file");
	}

	flags = tftp(cmd, host, serverfile, fd, 69);

	close(fd);

	return flags;
}
