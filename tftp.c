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

const int tftp_cmd_get = 1;
const int tftp_cmd_put = 2;

static inline int tftp(const int cmd, const struct hostent *host,
	const char *serverfile, int localfd, const int port, int tftp_bufsize)
{
	const int cmd_get = cmd & tftp_cmd_get;
	const int cmd_put = cmd & tftp_cmd_put;
	const int bb_tftp_num_retries = 5;

	struct sockaddr_in sa;
	struct sockaddr_in from;
	struct timeval tv;
	socklen_t fromlen;
	fd_set rfds;
	char *cp;
	unsigned short tmp;
	int socketfd;
	int len;
	int opcode = 0;
	int finished = 0;
	int timeout = bb_tftp_num_retries;
	int block_nr = 1;
	RESERVE_BB_BUFFER(buf, tftp_bufsize + 4); // Why 4 ?

	tftp_bufsize += 4;

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

	if (cmd_get) {
		opcode = 1;	// read request = 1
	}

	if (cmd_put) {
		opcode = 2;	// write request = 2
	}

	while (1) {


		/* build packet of type "opcode" */


		cp = buf;

		*((unsigned short *) cp) = htons(opcode);

		cp += 2;

		/* add filename and mode */

		if ((cmd_get && (opcode == 1)) || // read request = 1
			(cmd_put && (opcode == 2))) { // write request = 2

			/* what is this trying to do ? */
			while (cp != &buf[tftp_bufsize - 1]) {
				if ((*cp = *serverfile++) == '\0')
					break;
				cp++;
			}
			/* and this ? */
			if ((*cp != '\0') || (&buf[tftp_bufsize - 1] - cp) < 7) {
				error_msg("too long server-filename");
				break;
			}

			memcpy(cp + 1, "octet", 6);
			cp += 7;
		}

		/* add ack and data */

		if ((cmd_get && (opcode == 4)) || // acknowledgement = 4
			(cmd_put && (opcode == 3))) { // data packet == 3

			*((unsigned short *) cp) = htons(block_nr);

			cp += 2;

			block_nr++;

			if (cmd_put && (opcode == 3)) { // data packet == 3
				len = read(localfd, cp, tftp_bufsize - 4);

				if (len < 0) {
					perror_msg("read");
					break;
				}

				if (len != (tftp_bufsize - 4)) {
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

#ifdef BB_FEATURE_TFTP_DEBUG
			printf("sending %u bytes\n", len);
			for (cp = buf; cp < &buf[len]; cp++)
				printf("%02x ", *cp);
			printf("\n");
#endif
			if (sendto(socketfd, buf, len, 0,
					(struct sockaddr *) &sa, sizeof(sa)) < 0) {
				perror_msg("send");
				len = -1;
				break;
			}


			/* receive packet */


			memset(&from, 0, sizeof(from));
			fromlen = sizeof(from);

			tv.tv_sec = 5; // BB_TFPT_TIMEOUT = 5
			tv.tv_usec = 0;

			FD_ZERO(&rfds);
			FD_SET(socketfd, &rfds);

			switch (select(FD_SETSIZE, &rfds, NULL, NULL, &tv)) {
			case 1:
				len = recvfrom(socketfd, buf, tftp_bufsize, 0,
						(struct sockaddr *) &from, &fromlen);

				if (len < 0) {
					perror_msg("recvfrom");
					break;
				}

				timeout = 0;

				if (sa.sin_port == htons(port)) {
					sa.sin_port = from.sin_port;
				}
				if (sa.sin_port == from.sin_port) {
					break;
				}

				/* fall-through for bad packets! */
				/* discard the packet - treat as timeout */
				timeout = bb_tftp_num_retries;

			case 0:
				error_msg("timeout");

				if (timeout == 0) {
					len = -1;
					error_msg("last timeout");
				} else {
					timeout--;
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

#ifdef BB_FEATURE_TFTP_DEBUG
		printf("received %d bytes: %04x %04x\n", len, opcode, tmp);
#endif

		if (cmd_get && (opcode == 3)) { // data packet == 3

			if (tmp == block_nr) {
				len = write(localfd, &buf[4], len - 4);

				if (len < 0) {
					perror_msg("write");
					break;
				}

				if (len != (tftp_bufsize - 4)) {
					finished++;
				}

				opcode = 4; // acknowledgement = 4
				continue;
			}
		}

		if (cmd_put && (opcode == 4)) { // acknowledgement = 4

			if (tmp == (block_nr - 1)) {
				if (finished) {
					break;
				}

				opcode = 3; // data packet == 3
				continue;
			}
		}

		if (opcode == 5) { // error code == 5
			char *msg = NULL;

			if (buf[4] != '\0') {
				msg = &buf[4];
				buf[tftp_bufsize - 1] = '\0';
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
	struct hostent *host = NULL;
	char *localfile = NULL;
	char *remotefile = NULL;
	int port = 69;
	int cmd = 0;
	int fd = -1;
	int flags = 0;
	int opt;
	int result;
	int blocksize = 512;

	while ((opt = getopt(argc, argv, "b:gpl:r:")) != -1) {
		switch (opt) {
		case 'b':
			blocksize = atoi(optarg);
			break;
#ifdef BB_FEATURE_TFTP_GET
		case 'g':
			cmd = tftp_cmd_get;
			flags = O_WRONLY | O_CREAT;
			break;
#endif
#ifdef BB_FEATURE_TFTP_PUT
		case 'p':
			cmd = tftp_cmd_put;
			flags = O_RDONLY;
			break;
#endif
		case 'l': 
			localfile = xstrdup(optarg);
			break;
		case 'r':
			remotefile = xstrdup(optarg);
			break;
		}
	}

	if ((cmd == 0) || (optind == argc)) {
		show_usage();
	}

	fd = open(localfile, flags, 0644);
	if (fd < 0) {
		perror_msg_and_die("local file");
	}

	host = xgethostbyname(argv[optind]);

	if (optind + 2 == argc) {
		port = atoi(argv[optind + 1]);
	}

#ifdef BB_FEATURE_TFTP_DEBUG
	printf("using server \"%s\", serverfile \"%s\","
		"localfile \"%s\".\n",
		inet_ntoa(*((struct in_addr *) host->h_addr)),
		remotefile, localfile);
#endif

	result = tftp(cmd, host, remotefile, fd, port, blocksize);
	close(fd);

	return(result);
}