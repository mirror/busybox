/* ------------------------------------------------------------------------- */
/* tftp.c                                                                    */
/*                                                                           */
/* A simple tftp client for busybox.                                         */
/* Tries to follow RFC1350.                                                  */
/* Only "octet" mode supported.                                              */
/* Optional blocksize negotiation (RFC2347 + RFC2348)                        */
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

//#define CONFIG_FEATURE_TFTP_DEBUG

#define TFTP_BLOCKSIZE_DEFAULT 512 /* according to RFC 1350, don't change */
#define TFTP_TIMEOUT 5             /* seconds */

/* opcodes we support */

#define TFTP_RRQ   1
#define TFTP_WRQ   2
#define TFTP_DATA  3
#define TFTP_ACK   4
#define TFTP_ERROR 5
#define TFTP_OACK  6

static const char *tftp_bb_error_msg[] = {
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

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE

static int tftp_blocksize_check(int blocksize, int bufsize)  
{
        /* Check if the blocksize is valid: 
	 * RFC2348 says between 8 and 65464,
	 * but our implementation makes it impossible
	 * to use blocksizes smaller than 22 octets.
	 */

        if ((bufsize && (blocksize > bufsize)) || 
	    (blocksize < 8) || (blocksize > 65464)) {
	        bb_error_msg("bad blocksize");
	        return 0;
	}

	return blocksize;
}

static char *tftp_option_get(char *buf, int len, char *option)  
{
        int opt_val = 0;
	int opt_found = 0;
	int k;
  
	while (len > 0) {

	        /* Make sure the options are terminated correctly */

	        for (k = 0; k < len; k++) {
		        if (buf[k] == '\0') {
			        break;
			}
		}

		if (k >= len) {
		        break;
		}

		if (opt_val == 0) {
			if (strcasecmp(buf, option) == 0) {
			        opt_found = 1;
			}
		}      
		else {
		        if (opt_found) {
				return buf;
			}
		}
    
		k++;
		
		buf += k;
		len -= k;
		
		opt_val ^= 1;
	}
	
	return NULL;
}

#endif

static inline int tftp(const int cmd, const struct hostent *host,
	const char *remotefile, int localfd, const int port, int tftp_bufsize)
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

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE
	int want_option_ack = 0;
#endif

	/* Can't use RESERVE_CONFIG_BUFFER here since the allocation
	 * size varies meaning BUFFERS_GO_ON_STACK would fail */
	char *buf=xmalloc(tftp_bufsize + 4);

	tftp_bufsize += 4;

	if ((socketfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		bb_perror_msg("socket");
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
		opcode = TFTP_RRQ;
	}

	if (cmd_put) {
		opcode = TFTP_WRQ;
	}

	while (1) {

		cp = buf;

		/* first create the opcode part */

		*((unsigned short *) cp) = htons(opcode);

		cp += 2;

		/* add filename and mode */

		if ((cmd_get && (opcode == TFTP_RRQ)) ||
			(cmd_put && (opcode == TFTP_WRQ))) {
                        int too_long = 0; 

			/* see if the filename fits into buf */
			/* and fill in packet                */

			len = strlen(remotefile) + 1;

			if ((cp + len) >= &buf[tftp_bufsize - 1]) {
			        too_long = 1;
			}
			else {
			        safe_strncpy(cp, remotefile, len);
				cp += len;
			}

			if (too_long || ((&buf[tftp_bufsize - 1] - cp) < 6)) {
				bb_error_msg("too long remote-filename");
				break;
			}

			/* add "mode" part of the package */

			memcpy(cp, "octet", 6);
			cp += 6;

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE

			len = tftp_bufsize - 4; /* data block size */

			if (len != TFTP_BLOCKSIZE_DEFAULT) {

			        if ((&buf[tftp_bufsize - 1] - cp) < 15) {
				        bb_error_msg("too long remote-filename");
					break;
				}

				/* add "blksize" + number of blocks  */

				memcpy(cp, "blksize", 8);
				cp += 8;

				cp += snprintf(cp, 6, "%d", len) + 1;

				want_option_ack = 1;
			}
#endif
		}

		/* add ack and data */

		if ((cmd_get && (opcode == TFTP_ACK)) ||
			(cmd_put && (opcode == TFTP_DATA))) {

			*((unsigned short *) cp) = htons(block_nr);

			cp += 2;

			block_nr++;

			if (cmd_put && (opcode == TFTP_DATA)) {
				len = read(localfd, cp, tftp_bufsize - 4);

				if (len < 0) {
					bb_perror_msg("read");
					break;
				}

				if (len != (tftp_bufsize - 4)) {
					finished++;
				}

				cp += len;
			}
		}


		/* send packet */


		timeout = bb_tftp_num_retries;  /* re-initialize */
		do {

			len = cp - buf;

#ifdef CONFIG_FEATURE_TFTP_DEBUG
			printf("sending %u bytes\n", len);
			for (cp = buf; cp < &buf[len]; cp++)
				printf("%02x ", *cp);
			printf("\n");
#endif
			if (sendto(socketfd, buf, len, 0,
					(struct sockaddr *) &sa, sizeof(sa)) < 0) {
				bb_perror_msg("send");
				len = -1;
				break;
			}


			if (finished && (opcode == TFTP_ACK)) {
				break;
			}

			/* receive packet */

			memset(&from, 0, sizeof(from));
			fromlen = sizeof(from);

			tv.tv_sec = TFTP_TIMEOUT;
			tv.tv_usec = 0;

			FD_ZERO(&rfds);
			FD_SET(socketfd, &rfds);

			switch (select(FD_SETSIZE, &rfds, NULL, NULL, &tv)) {
			case 1:
				len = recvfrom(socketfd, buf, tftp_bufsize, 0,
						(struct sockaddr *) &from, &fromlen);

				if (len < 0) {
					bb_perror_msg("recvfrom");
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
				bb_error_msg("timeout");

				timeout--;
				if (timeout == 0) {
					len = -1;
					bb_error_msg("last timeout");
				}
				break;

			default:
				bb_perror_msg("select");
				len = -1;
			}

		} while (timeout && (len >= 0));

		if ((finished) || (len < 0)) {
			break;
		}

		/* process received packet */


		opcode = ntohs(*((unsigned short *) buf));
		tmp = ntohs(*((unsigned short *) &buf[2]));

#ifdef CONFIG_FEATURE_TFTP_DEBUG
		printf("received %d bytes: %04x %04x\n", len, opcode, tmp);
#endif

		if (opcode == TFTP_ERROR) {
			char *msg = NULL;

			if (buf[4] != '\0') {
				msg = &buf[4];
				buf[tftp_bufsize - 1] = '\0';
			} else if (tmp < (sizeof(tftp_bb_error_msg) 
					  / sizeof(char *))) {

				msg = (char *) tftp_bb_error_msg[tmp];
			}

			if (msg) {
				bb_error_msg("server says: %s", msg);
			}

			break;
		}

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE
		if (want_option_ack) {

			 want_option_ack = 0;

		         if (opcode == TFTP_OACK) {

			         /* server seems to support options */

			         char *res;

				 res = tftp_option_get(&buf[2], len-2, 
						       "blksize");

				 if (res) {
				         int blksize = atoi(res);
			     
					 if (tftp_blocksize_check(blksize,
							   tftp_bufsize - 4)) {

					         if (cmd_put) {
				                         opcode = TFTP_DATA;
						 }
						 else {
				                         opcode = TFTP_ACK;
						 }
#ifdef CONFIG_FEATURE_TFTP_DEBUG
						 printf("using blksize %u\n", blksize);
#endif
					         tftp_bufsize = blksize + 4;
						 block_nr = 0;
						 continue;
					 }
				 }
				 /* FIXME:
				  * we should send ERROR 8 */
				 bb_error_msg("bad server option");
				 break;
			 }

			 bb_error_msg("warning: blksize not supported by server"
				   " - reverting to 512");

			 tftp_bufsize = TFTP_BLOCKSIZE_DEFAULT + 4;
		}
#endif

		if (cmd_get && (opcode == TFTP_DATA)) {

			if (tmp == block_nr) {
			    
				len = write(localfd, &buf[4], len - 4);

				if (len < 0) {
					bb_perror_msg("write");
					break;
				}

				if (len != (tftp_bufsize - 4)) {
					finished++;
				}

				opcode = TFTP_ACK;
				continue;
			}
		}

		if (cmd_put && (opcode == TFTP_ACK)) {

			if (tmp == (block_nr - 1)) {
				if (finished) {
					break;
				}

				opcode = TFTP_DATA;
				continue;
			}
		}
	}

#ifdef CONFIG_FEATURE_CLEAN_UP
	close(socketfd);

        free(buf);
#endif

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
	int blocksize = TFTP_BLOCKSIZE_DEFAULT;

	/* figure out what to pass to getopt */

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE
#define BS "b:"
#else
#define BS
#endif

#ifdef CONFIG_FEATURE_TFTP_GET
#define GET "g"
#else
#define GET 
#endif

#ifdef CONFIG_FEATURE_TFTP_PUT
#define PUT "p"
#else
#define PUT 
#endif

	while ((opt = getopt(argc, argv, BS GET PUT "l:r:")) != -1) {
		switch (opt) {
#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE
		case 'b':
			blocksize = atoi(optarg);
			if (!tftp_blocksize_check(blocksize, 0)) {
                                return EXIT_FAILURE;
			}
			break;
#endif
#ifdef CONFIG_FEATURE_TFTP_GET
		case 'g':
			cmd = tftp_cmd_get;
			flags = O_WRONLY | O_CREAT;
			break;
#endif
#ifdef CONFIG_FEATURE_TFTP_PUT
		case 'p':
			cmd = tftp_cmd_put;
			flags = O_RDONLY;
			break;
#endif
		case 'l': 
			localfile = bb_xstrdup(optarg);
			break;
		case 'r':
			remotefile = bb_xstrdup(optarg);
			break;
		}
	}

	if ((cmd == 0) || (optind == argc)) {
		bb_show_usage();
	}
	if(localfile && strcmp(localfile, "-") == 0) {
	    fd = fileno((cmd==tftp_cmd_get)? stdout : stdin);
	}
	if(localfile == NULL)
	    localfile = remotefile;
	if(remotefile == NULL)
	    remotefile = localfile;
	if (fd==-1) {
	    fd = open(localfile, flags, 0644);
	}
	if (fd < 0) {
		bb_perror_msg_and_die("local file");
	}

	host = xgethostbyname(argv[optind]);

	if (optind + 2 == argc) {
		port = atoi(argv[optind + 1]);
	}

#ifdef CONFIG_FEATURE_TFTP_DEBUG
	printf("using server \"%s\", remotefile \"%s\", "
		"localfile \"%s\".\n",
		inet_ntoa(*((struct in_addr *) host->h_addr)),
		remotefile, localfile);
#endif

	result = tftp(cmd, host, remotefile, fd, port, blocksize);

#ifdef CONFIG_FEATURE_CLEAN_UP
	if (!(fd == fileno(stdout) || fd == fileno(stdin))) {
	    close(fd);
	}
#endif
	return(result);
}
