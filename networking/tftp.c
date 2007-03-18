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

#include "busybox.h"


#if ENABLE_FEATURE_TFTP_GET || ENABLE_FEATURE_TFTP_PUT

#define TFTP_BLOCKSIZE_DEFAULT 512	/* according to RFC 1350, don't change */
#define TFTP_TIMEOUT 5	/* seconds */
#define TFTP_NUM_RETRIES 5 /* number of retries */

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

#if ENABLE_FEATURE_TFTP_GET && !ENABLE_FEATURE_TFTP_PUT
#define USE_GETPUT(a)
#define CMD_GET(cmd) 1
#define CMD_PUT(cmd) 0
#elif !ENABLE_FEATURE_TFTP_GET && ENABLE_FEATURE_TFTP_PUT
#define USE_GETPUT(a)
#define CMD_GET(cmd) 0
#define CMD_PUT(cmd) 1
#else
#define USE_GETPUT(a) a
/* masks coming from getpot32 */
#define CMD_GET(cmd) ((cmd) & 1)
#define CMD_PUT(cmd) ((cmd) & 2)
#endif
/* NB: in the code below
 * CMD_GET(cmd) and CMD_GET(cmd) are mutually exclusive
 */


#if ENABLE_FEATURE_TFTP_BLOCKSIZE

static int tftp_blocksize_check(int blocksize, int bufsize)
{
	/* Check if the blocksize is valid:
	 * RFC2348 says between 8 and 65464,
	 * but our implementation makes it impossible
	 * to use blocksizes smaller than 22 octets.
	 */

	if ((bufsize && (blocksize > bufsize))
	 || (blocksize < 8) || (blocksize > 65564)
	) {
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

static int tftp(
#if ENABLE_FEATURE_TFTP_GET && ENABLE_FEATURE_TFTP_PUT
		const int cmd,
#endif
		len_and_sockaddr *peer_lsa,
		const char *remotefile, const int localfd,
		unsigned port, int tftp_bufsize)
{
	struct timeval tv;
	fd_set rfds;
	int socketfd;
	int len;
	int opcode = 0;
	int finished = 0;
	int timeout = TFTP_NUM_RETRIES;
	uint16_t block_nr = 1;
	uint16_t tmp;
	char *cp;

	USE_FEATURE_TFTP_BLOCKSIZE(int want_option_ack = 0;)

	unsigned org_port;
	len_and_sockaddr *const from = alloca(offsetof(len_and_sockaddr, sa) + peer_lsa->len);

	/* Can't use RESERVE_CONFIG_BUFFER here since the allocation
	 * size varies meaning BUFFERS_GO_ON_STACK would fail */
	/* We must keep the transmit and receive buffers seperate */
	/* In case we rcv a garbage pkt and we need to rexmit the last pkt */
	char *xbuf = xmalloc(tftp_bufsize += 4);
	char *rbuf = xmalloc(tftp_bufsize);

	port = org_port = htons(port);

	socketfd = xsocket(peer_lsa->sa.sa_family, SOCK_DGRAM, 0);

	/* build opcode */
	opcode = TFTP_WRQ;
	if (CMD_GET(cmd)) {
		opcode = TFTP_RRQ;
	}

	while (1) {
		cp = xbuf;

		/* first create the opcode part */
		/* (this 16bit store is aligned) */
		*((uint16_t*)cp) = htons(opcode);
		cp += 2;

		/* add filename and mode */
		if (CMD_GET(cmd) ? (opcode == TFTP_RRQ) : (opcode == TFTP_WRQ)) {
			int too_long = 0;

			/* see if the filename fits into xbuf
			 * and fill in packet.  */
			len = strlen(remotefile) + 1;

			if ((cp + len) >= &xbuf[tftp_bufsize - 1]) {
				too_long = 1;
			} else {
				safe_strncpy(cp, remotefile, len);
				cp += len;
			}

			if (too_long || (&xbuf[tftp_bufsize - 1] - cp) < sizeof("octet")) {
				bb_error_msg("remote filename too long");
				break;
			}

			/* add "mode" part of the package */
			memcpy(cp, "octet", sizeof("octet"));
			cp += sizeof("octet");

#if ENABLE_FEATURE_TFTP_BLOCKSIZE

			len = tftp_bufsize - 4;	/* data block size */

			if (len != TFTP_BLOCKSIZE_DEFAULT) {

				if ((&xbuf[tftp_bufsize - 1] - cp) < 15) {
					bb_error_msg("remote filename too long");
					break;
				}

				/* add "blksize" + number of blocks  */
				memcpy(cp, "blksize", sizeof("blksize"));
				cp += sizeof("blksize");
				cp += snprintf(cp, 6, "%d", len) + 1;

				want_option_ack = 1;
			}
#endif
		}

		/* add ack and data */

		if (CMD_GET(cmd) ? (opcode == TFTP_ACK) : (opcode == TFTP_DATA)) {
			/* TODO: unaligned access! */
			*((uint16_t*)cp) = htons(block_nr);
			cp += 2;
			block_nr++;

			if (CMD_PUT(cmd) && (opcode == TFTP_DATA)) {
				len = full_read(localfd, cp, tftp_bufsize - 4);

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
			len = cp - xbuf;
#if ENABLE_DEBUG_TFTP
			fprintf(stderr, "sending %u bytes\n", len);
			for (cp = xbuf; cp < &xbuf[len]; cp++)
				fprintf(stderr, "%02x ", (unsigned char) *cp);
			fprintf(stderr, "\n");
#endif
			if (sendto(socketfd, xbuf, len, 0,
					&peer_lsa->sa, peer_lsa->len) < 0) {
				bb_perror_msg("send");
				len = -1;
				break;
			}

			if (finished && (opcode == TFTP_ACK)) {
				break;
			}

			/* receive packet */
 recv_again:
			tv.tv_sec = TFTP_TIMEOUT;
			tv.tv_usec = 0;

			FD_ZERO(&rfds);
			FD_SET(socketfd, &rfds);

			switch (select(socketfd + 1, &rfds, NULL, NULL, &tv)) {
				unsigned from_port;
			case 1:
				from->len = peer_lsa->len;
				memset(&from->sa, 0, peer_lsa->len);
				len = recvfrom(socketfd, rbuf, tftp_bufsize, 0,
							&from->sa, &from->len);
				if (len < 0) {
					bb_perror_msg("recvfrom");
					break;
				}
				from_port = get_nport(from);
				if (port == org_port) {
					/* Our first query went to port 69
					 * but reply will come from different one.
					 * Remember and use this new port */
					port = from_port;
					set_nport(peer_lsa, from_port);
				}
				if (port != from_port)
					goto recv_again;
				timeout = 0;
				break;
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

		if (finished || (len < 0)) {
			break;
		}

		/* process received packet */
		/* (both accesses seems to be aligned) */

		opcode = ntohs( ((uint16_t*)rbuf)[0] );
		tmp = ntohs( ((uint16_t*)rbuf)[1] );

#if ENABLE_DEBUG_TFTP
		fprintf(stderr, "received %d bytes: %04x %04x\n", len, opcode, tmp);
#endif

		if (opcode == TFTP_ERROR) {
			const char *msg = NULL;

			if (rbuf[4] != '\0') {
				msg = &rbuf[4];
				rbuf[tftp_bufsize - 1] = '\0';
			} else if (tmp < (sizeof(tftp_bb_error_msg)
							  / sizeof(char *))) {
				msg = tftp_bb_error_msg[tmp];
			}

			if (msg) {
				bb_error_msg("server says: %s", msg);
			}

			break;
		}
#if ENABLE_FEATURE_TFTP_BLOCKSIZE
		if (want_option_ack) {

			want_option_ack = 0;

			if (opcode == TFTP_OACK) {
				/* server seems to support options */
				char *res;

				res = tftp_option_get(&rbuf[2], len - 2, "blksize");

				if (res) {
					int blksize = xatoi_u(res);

					if (tftp_blocksize_check(blksize, tftp_bufsize - 4)) {
						if (CMD_PUT(cmd)) {
							opcode = TFTP_DATA;
						} else {
							opcode = TFTP_ACK;
						}
#if ENABLE_DEBUG_TFTP
						fprintf(stderr, "using blksize %u\n",
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

		if (CMD_GET(cmd) && (opcode == TFTP_DATA)) {
			if (tmp == block_nr) {
				len = full_write(localfd, &rbuf[4], len - 4);

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
// tmp==(block_nr-1) and (tmp+1)==block_nr is always same, I think. wtf?
			} else if (tmp + 1 == block_nr) {
				/* Server lost our TFTP_ACK.  Resend it */
				block_nr = tmp;
				opcode = TFTP_ACK;
				continue;
			}
		}

		if (CMD_PUT(cmd) && (opcode == TFTP_ACK)) {
			if (tmp == (uint16_t) (block_nr - 1)) {
				if (finished) {
					break;
				}

				opcode = TFTP_DATA;
				continue;
			}
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
		close(socketfd);
		free(xbuf);
		free(rbuf);
	}

	return finished ? EXIT_SUCCESS : EXIT_FAILURE;
}

int tftp_main(int argc, char **argv)
{
	len_and_sockaddr *peer_lsa;
	const char *localfile = NULL;
	const char *remotefile = NULL;
#if ENABLE_FEATURE_TFTP_BLOCKSIZE
	const char *sblocksize = NULL;
#endif
	int port;
	USE_GETPUT(int cmd;)
	int fd = -1;
	int flags = 0;
	int result;
	int blocksize = TFTP_BLOCKSIZE_DEFAULT;

	/* -p or -g is mandatory, and they are mutually exclusive */
	opt_complementary = "" USE_FEATURE_TFTP_GET("g:") USE_FEATURE_TFTP_PUT("p:")
			USE_GETPUT("?g--p:p--g");

	USE_GETPUT(cmd =) getopt32(argc, argv,
			USE_FEATURE_TFTP_GET("g") USE_FEATURE_TFTP_PUT("p")
				"l:r:" USE_FEATURE_TFTP_BLOCKSIZE("b:"),
			&localfile, &remotefile
			USE_FEATURE_TFTP_BLOCKSIZE(, &sblocksize));

	flags = O_RDONLY;
	if (CMD_GET(cmd))
		flags = O_WRONLY | O_CREAT | O_TRUNC;

#if ENABLE_FEATURE_TFTP_BLOCKSIZE
	if (sblocksize) {
		blocksize = xatoi_u(sblocksize);
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

	if (localfile == NULL || LONE_DASH(localfile)) {
		fd = CMD_GET(cmd) ? STDOUT_FILENO : STDIN_FILENO;
	} else {
		fd = xopen3(localfile, flags, 0644);
	}

	port = bb_lookup_port(argv[optind + 1], "udp", 69);
	peer_lsa = host2sockaddr(argv[optind], port);

#if ENABLE_DEBUG_TFTP
	fprintf(stderr, "using server \"%s\", "
			"remotefile \"%s\", localfile \"%s\".\n",
			xmalloc_sockaddr2dotted(&peer_lsa->sa, peer_lsa->len),
			remotefile, localfile);
#endif

	result = tftp(
#if ENABLE_FEATURE_TFTP_GET && ENABLE_FEATURE_TFTP_PUT
		cmd,
#endif
		peer_lsa, remotefile, fd, port, blocksize);

	if (fd > 1) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
		if (CMD_GET(cmd) && result != EXIT_SUCCESS)
			unlink(localfile);
	}
	return result;
}

#endif /* ENABLE_FEATURE_TFTP_GET || ENABLE_FEATURE_TFTP_PUT */
