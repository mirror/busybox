/* vi: set sw=4 ts=4: */
/* -------------------------------------------------------------------------
 * tftp.c
 *
 * A simple tftp client for busybox.
 * Tries to follow RFC1350.
 * Only "octet" mode supported.
 * Optional blocksize negotiation (RFC2347 + RFC2348)
 *
 * Copyright (C) 2001 Magnus Damm <damm@opensource.se>
 *
 * Parts of the code based on:
 *
 * atftp:  Copyright (C) 2000 Jean-Pierre Lefebvre <helix@step.polymtl.ca>
 *                        and Remi Lefebvre <remi@debian.org>
 *
 * utftp:  Copyright (C) 1999 Uwe Ohse <uwe@ohse.de>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 * ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "busybox.h"


#define TFTP_BLOCKSIZE_DEFAULT 512	/* according to RFC 1350, don't change */
#define TFTP_TIMEOUT 5	/* seconds */
#define TFTP_NUM_RETRIES 5 /* number of retries */

static const char * const MODE_OCTET = "octet";
#define MODE_OCTET_LEN 6 /* sizeof(MODE_OCTET)*/

static const char * const OPTION_BLOCKSIZE = "blksize";
#define OPTION_BLOCKSIZE_LEN 8 /* sizeof(OPTION_BLOCKSIZE) */

/* opcodes we support */
#define TFTP_RRQ   1
#define TFTP_WRQ   2
#define TFTP_DATA  3
#define TFTP_ACK   4
#define TFTP_ERROR 5
#define TFTP_OACK  6

static const char *const tftp_bb_error_msg[] = {
	"Undefined error",
	"File not found",
	"Access violation",
	"Disk full or allocation error",
	"Illegal TFTP operation",
	"Unknown transfer ID",
	"File already exists",
	"No such user"
};

#define tftp_cmd_get ENABLE_FEATURE_TFTP_GET

#if ENABLE_FEATURE_TFTP_PUT
# define tftp_cmd_put (tftp_cmd_get+ENABLE_FEATURE_TFTP_PUT)
#else
# define tftp_cmd_put 0
#endif


#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE

static int tftp_blocksize_check(int blocksize, int bufsize)
{
	/* Check if the blocksize is valid:
	 * RFC2348 says between 8 and 65464,
	 * but our implementation makes it impossible
	 * to use blocksizes smaller than 22 octets.
	 */

	if ((bufsize && (blocksize > bufsize)) ||
		(blocksize < 8) || (blocksize > 65564)) {
		bb_error_msg("bad blocksize");
		return 0;
	}

	return blocksize;
}

static char *tftp_option_get(char *buf, int len, const char * const option)
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
		} else {
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

static int tftp(const int cmd, const struct hostent *host,
		   const char *remotefile, const int localfd,
		   const unsigned short port, int tftp_bufsize)
{
	struct sockaddr_in sa;
	struct sockaddr_in from;
	struct timeval tv;
	socklen_t fromlen;
	fd_set rfds;
	int socketfd;
	int len;
	int opcode = 0;
	int finished = 0;
	int timeout = TFTP_NUM_RETRIES;
	unsigned short block_nr = 1;
	unsigned short tmp;
	char *cp;

	USE_FEATURE_TFTP_BLOCKSIZE(int want_option_ack = 0;)

	/* Can't use RESERVE_CONFIG_BUFFER here since the allocation
	 * size varies meaning BUFFERS_GO_ON_STACK would fail */
	char *buf=xmalloc(tftp_bufsize += 4);

	if ((socketfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		/* need to unlink the localfile, so don't use bb_xsocket here. */
		bb_perror_msg("socket");
		return EXIT_FAILURE;
	}

	len = sizeof(sa);

	memset(&sa, 0, len);
	bb_xbind(socketfd, (struct sockaddr *)&sa, len);

	sa.sin_family = host->h_addrtype;
	sa.sin_port = port;
	memcpy(&sa.sin_addr, (struct in_addr *) host->h_addr,
		   sizeof(sa.sin_addr));

	/* build opcode */
	if (cmd & tftp_cmd_get) {
		opcode = TFTP_RRQ;
	}
	if (cmd & tftp_cmd_put) {
		opcode = TFTP_WRQ;
	}

	while (1) {

		cp = buf;

		/* first create the opcode part */
		*((unsigned short *) cp) = htons(opcode);
		cp += 2;

		/* add filename and mode */
		if (((cmd & tftp_cmd_get) && (opcode == TFTP_RRQ)) ||
			((cmd & tftp_cmd_put) && (opcode == TFTP_WRQ)))
		{
			int too_long = 0;

			/* see if the filename fits into buf
			 * and fill in packet.  */
			len = strlen(remotefile) + 1;

			if ((cp + len) >= &buf[tftp_bufsize - 1]) {
				too_long = 1;
			} else {
				safe_strncpy(cp, remotefile, len);
				cp += len;
			}

			if (too_long || ((&buf[tftp_bufsize - 1] - cp) < MODE_OCTET_LEN)) {
				bb_error_msg("remote filename too long");
				break;
			}

			/* add "mode" part of the package */
			memcpy(cp, MODE_OCTET, MODE_OCTET_LEN);
			cp += MODE_OCTET_LEN;

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE

			len = tftp_bufsize - 4;	/* data block size */

			if (len != TFTP_BLOCKSIZE_DEFAULT) {

				if ((&buf[tftp_bufsize - 1] - cp) < 15) {
					bb_error_msg("remote filename too long");
					break;
				}

				/* add "blksize" + number of blocks  */
				memcpy(cp, OPTION_BLOCKSIZE, OPTION_BLOCKSIZE_LEN);
				cp += OPTION_BLOCKSIZE_LEN;
				cp += snprintf(cp, 6, "%d", len) + 1;

				want_option_ack = 1;
			}
#endif
		}

		/* add ack and data */

		if (((cmd & tftp_cmd_get) && (opcode == TFTP_ACK)) ||
			((cmd & tftp_cmd_put) && (opcode == TFTP_DATA))) {

			*((unsigned short *) cp) = htons(block_nr);

			cp += 2;

			block_nr++;

			if ((cmd & tftp_cmd_put) && (opcode == TFTP_DATA)) {
				len = bb_full_read(localfd, cp, tftp_bufsize - 4);

				if (len < 0) {
					bb_perror_msg(bb_msg_read_error);
					break;
				}

				if (len != (tftp_bufsize - 4)) {
					finished++;
				}

				cp += len;
			}
		}


		/* send packet */


		timeout = TFTP_NUM_RETRIES;	/* re-initialize */
		do {

			len = cp - buf;

#ifdef CONFIG_DEBUG_TFTP
			fprintf(stderr, "sending %u bytes\n", len);
			for (cp = buf; cp < &buf[len]; cp++)
				fprintf(stderr, "%02x ", (unsigned char) *cp);
			fprintf(stderr, "\n");
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

			switch (select(socketfd + 1, &rfds, NULL, NULL, &tv)) {
			case 1:
				len = recvfrom(socketfd, buf, tftp_bufsize, 0,
							   (struct sockaddr *) &from, &fromlen);

				if (len < 0) {
					bb_perror_msg("recvfrom");
					break;
				}

				timeout = 0;

				if (sa.sin_port == port) {
					sa.sin_port = from.sin_port;
				}
				if (sa.sin_port == from.sin_port) {
					break;
				}

				/* fall-through for bad packets! */
				/* discard the packet - treat as timeout */
				timeout = TFTP_NUM_RETRIES;
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

#ifdef CONFIG_DEBUG_TFTP
		fprintf(stderr, "received %d bytes: %04x %04x\n", len, opcode, tmp);
#endif

		if (opcode == TFTP_ERROR) {
			const char *msg = NULL;

			if (buf[4] != '\0') {
				msg = &buf[4];
				buf[tftp_bufsize - 1] = '\0';
			} else if (tmp < (sizeof(tftp_bb_error_msg)
							  / sizeof(char *))) {

				msg = tftp_bb_error_msg[tmp];
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

				res = tftp_option_get(&buf[2], len - 2, OPTION_BLOCKSIZE);

				if (res) {
					int blksize = atoi(res);

					if (tftp_blocksize_check(blksize, tftp_bufsize - 4)) {

						if (cmd & tftp_cmd_put) {
							opcode = TFTP_DATA;
						} else {
							opcode = TFTP_ACK;
						}
#ifdef CONFIG_DEBUG_TFTP
						fprintf(stderr, "using %s %u\n", OPTION_BLOCKSIZE,
								blksize);
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

		if ((cmd & tftp_cmd_get) && (opcode == TFTP_DATA)) {

			if (tmp == block_nr) {

				len = bb_full_write(localfd, &buf[4], len - 4);

				if (len < 0) {
					bb_perror_msg(bb_msg_write_error);
					break;
				}

				if (len != (tftp_bufsize - 4)) {
					finished++;
				}

				opcode = TFTP_ACK;
				continue;
			}
			/* in case the last ack disappeared into the ether */
			if (tmp == (block_nr - 1)) {
				--block_nr;
				opcode = TFTP_ACK;
				continue;
			} else if (tmp + 1 == block_nr) {
				/* Server lost our TFTP_ACK.  Resend it */
				block_nr = tmp;
				opcode = TFTP_ACK;
				continue;
			}
		}

		if ((cmd & tftp_cmd_put) && (opcode == TFTP_ACK)) {

			if (tmp == (unsigned short) (block_nr - 1)) {
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
	const char *localfile = NULL;
	const char *remotefile = NULL;
	int port;
	int cmd = 0;
	int fd = -1;
	int flags = 0;
	int result;
	int blocksize = TFTP_BLOCKSIZE_DEFAULT;

	/* figure out what to pass to getopt */

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE
	char *sblocksize = NULL;

#define BS "b:"
#define BS_ARG , &sblocksize
#else
#define BS
#define BS_ARG
#endif

#ifdef CONFIG_FEATURE_TFTP_GET
#define GET "g"
#define GET_COMPL ":g"
#else
#define GET
#define GET_COMPL
#endif

#ifdef CONFIG_FEATURE_TFTP_PUT
#define PUT "p"
#define PUT_COMPL ":p"
#else
#define PUT
#define PUT_COMPL
#endif

#if defined(CONFIG_FEATURE_TFTP_GET) && defined(CONFIG_FEATURE_TFTP_PUT)
	bb_opt_complementally = GET_COMPL PUT_COMPL ":?g--p:p--g";
#elif defined(CONFIG_FEATURE_TFTP_GET) || defined(CONFIG_FEATURE_TFTP_PUT)
	bb_opt_complementally = GET_COMPL PUT_COMPL;
#endif


	cmd = bb_getopt_ulflags(argc, argv, GET PUT "l:r:" BS,
							&localfile, &remotefile BS_ARG);

	cmd &= (tftp_cmd_get | tftp_cmd_put);
#ifdef CONFIG_FEATURE_TFTP_GET
	if (cmd == tftp_cmd_get)
		flags = O_WRONLY | O_CREAT | O_TRUNC;
#endif
#ifdef CONFIG_FEATURE_TFTP_PUT
	if (cmd == tftp_cmd_put)
		flags = O_RDONLY;
#endif

#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE
	if (sblocksize) {
		blocksize = atoi(sblocksize);
		if (!tftp_blocksize_check(blocksize, 0)) {
			return EXIT_FAILURE;
		}
	}
#endif

	if (localfile == NULL)
		localfile = remotefile;
	if (remotefile == NULL)
		remotefile = localfile;
	if ((localfile == NULL && remotefile == NULL) || (argv[optind] == NULL))
		bb_show_usage();

	if (localfile == NULL || strcmp(localfile, "-") == 0) {
		fd = (cmd == tftp_cmd_get) ? STDOUT_FILENO : STDIN_FILENO;
	} else {
		fd = open(localfile, flags, 0644); /* fail below */
	}
	if (fd < 0) {
		bb_perror_msg_and_die("local file");
	}

	host = xgethostbyname(argv[optind]);
	port = bb_lookup_port(argv[optind + 1], "udp", 69);

#ifdef CONFIG_DEBUG_TFTP
	fprintf(stderr, "using server \"%s\", remotefile \"%s\", "
			"localfile \"%s\".\n",
			inet_ntoa(*((struct in_addr *) host->h_addr)),
			remotefile, localfile);
#endif

	result = tftp(cmd, host, remotefile, fd, port, blocksize);

	if (!(fd == STDOUT_FILENO || fd == STDIN_FILENO)) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
		if (cmd == tftp_cmd_get && result != EXIT_SUCCESS)
			unlink(localfile);
	}
	return (result);
}
