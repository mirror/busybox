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

#include "libbb.h"

#if ENABLE_FEATURE_TFTP_GET || ENABLE_FEATURE_TFTP_PUT

#define TFTP_BLOCKSIZE_DEFAULT 512      /* according to RFC 1350, don't change */
#define TFTP_TIMEOUT_MS         50
#define TFTP_MAXTIMEOUT_MS    2000
#define TFTP_NUM_RETRIES        12      /* number of backed-off retries */

/* opcodes we support */
#define TFTP_RRQ   1
#define TFTP_WRQ   2
#define TFTP_DATA  3
#define TFTP_ACK   4
#define TFTP_ERROR 5
#define TFTP_OACK  6

#if ENABLE_FEATURE_TFTP_GET && !ENABLE_FEATURE_TFTP_PUT
#define USE_GETPUT(...)
#define CMD_GET(cmd) 1
#define CMD_PUT(cmd) 0
#elif !ENABLE_FEATURE_TFTP_GET && ENABLE_FEATURE_TFTP_PUT
#define USE_GETPUT(...)
#define CMD_GET(cmd) 0
#define CMD_PUT(cmd) 1
#else
#define USE_GETPUT(...) __VA_ARGS__
/* masks coming from getpot32 */
#define CMD_GET(cmd) ((cmd) & 1)
#define CMD_PUT(cmd) ((cmd) & 2)
#endif
/* NB: in the code below
 * CMD_GET(cmd) and CMD_PUT(cmd) are mutually exclusive
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

static char *tftp_option_get(char *buf, int len, const char *option)
{
	int opt_val = 0;
	int opt_found = 0;
	int k;

	while (len > 0) {
		/* Make sure the options are terminated correctly */
		for (k = 0; k < len; k++) {
			if (buf[k] == '\0') {
				goto nul_found;
			}
		}
		return NULL;
 nul_found:
		if (opt_val == 0) {
			if (strcasecmp(buf, option) == 0) {
				opt_found = 1;
			}
		} else if (opt_found) {
			return buf;
		}

		k++;
		buf += k;
		len -= k;
		opt_val ^= 1;
	}

	return NULL;
}

#endif

static int tftp( USE_GETPUT(const int cmd,)
		len_and_sockaddr *peer_lsa,
		const char *remotefile, const int localfd,
		unsigned port, int tftp_bufsize)
{
	struct pollfd pfd[1];
#define socketfd (pfd[0].fd)
	int len;
	int send_len;
	USE_FEATURE_TFTP_BLOCKSIZE(smallint want_option_ack = 0;)
	smallint finished = 0;
	uint16_t opcode;
	uint16_t block_nr = 1;
	uint16_t recv_blk;
	int retries, waittime_ms;
	char *cp;

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
	cp = xbuf + 2;
	/* add filename and mode */
	/* fill in packet if the filename fits into xbuf */
	len = strlen(remotefile) + 1;
	if (2 + len + sizeof("octet") >= tftp_bufsize) {
		bb_error_msg("remote filename is too long");
		goto ret;
	}
	strcpy(cp, remotefile);
	cp += len;
	/* add "mode" part of the package */
	strcpy(cp, "octet");
	cp += sizeof("octet");

#if ENABLE_FEATURE_TFTP_BLOCKSIZE
	len = tftp_bufsize - 4;	/* data block size */
	if (len != TFTP_BLOCKSIZE_DEFAULT) {
		/* rfc2348 says that 65464 is a max allowed value */
		if ((&xbuf[tftp_bufsize - 1] - cp) < sizeof("blksize NNNNN")) {
			bb_error_msg("remote filename is too long");
			goto ret;
		}
		/* add "blksize", <nul>, blocksize */
		strcpy(cp, "blksize");
		cp += sizeof("blksize");
		cp += snprintf(cp, 6, "%d", len) + 1;
		want_option_ack = 1;
	}
#endif
	/* First packet is built, so skip packet generation */
	goto send_pkt;

	/* Using mostly goto's - continue/break will be less clear
	 * in where we actually jump to */

	while (1) {
		/* Build ACK or DATA */
		cp = xbuf + 2;
		*((uint16_t*)cp) = htons(block_nr);
		cp += 2;
		block_nr++;
		opcode = TFTP_ACK;
		if (CMD_PUT(cmd)) {
			opcode = TFTP_DATA;
			len = full_read(localfd, cp, tftp_bufsize - 4);
			if (len < 0) {
				bb_perror_msg(bb_msg_read_error);
				goto ret;
			}
			if (len != (tftp_bufsize - 4)) {
				finished = 1;
			}
			cp += len;
		}
 send_pkt:
		/* Send packet */
		*((uint16_t*)xbuf) = htons(opcode); /* fill in opcode part */
		send_len = cp - xbuf;
		/* NB: send_len value is preserved in code below
		 * for potential resend */

		retries = TFTP_NUM_RETRIES;	/* re-initialize */
		waittime_ms = TFTP_TIMEOUT_MS;

 send_again:
#if ENABLE_DEBUG_TFTP
		fprintf(stderr, "sending %u bytes\n", send_len);
		for (cp = xbuf; cp < &xbuf[send_len]; cp++)
			fprintf(stderr, "%02x ", (unsigned char) *cp);
		fprintf(stderr, "\n");
#endif
		xsendto(socketfd, xbuf, send_len, &peer_lsa->sa, peer_lsa->len);
		/* Was it final ACK? then exit */
		if (finished && (opcode == TFTP_ACK))
			goto ret;

 recv_again:
		/* Receive packet */
		/*pfd[0].fd = socketfd;*/
		pfd[0].events = POLLIN;
		switch (safe_poll(pfd, 1, waittime_ms)) {
			unsigned from_port;
		case 1:
			from->len = peer_lsa->len;
			memset(&from->sa, 0, peer_lsa->len);
			len = recvfrom(socketfd, rbuf, tftp_bufsize, 0,
						&from->sa, &from->len);
			if (len < 0) {
				bb_perror_msg("recvfrom");
				goto ret;
			}
			from_port = get_nport(&from->sa);
			if (port == org_port) {
				/* Our first query went to port 69
				 * but reply will come from different one.
				 * Remember and use this new port */
				port = from_port;
				set_nport(peer_lsa, from_port);
			}
			if (port != from_port)
				goto recv_again;
			goto process_pkt;
		case 0:
			retries--;
			if (retries == 0) {
				bb_error_msg("timeout");
				goto ret;
			}

			/* exponential backoff with limit */
			waittime_ms += waittime_ms/2;
			if (waittime_ms > TFTP_MAXTIMEOUT_MS) {
				waittime_ms = TFTP_MAXTIMEOUT_MS;
			}

			goto send_again; /* resend last sent pkt */
		default:
			/*bb_perror_msg("poll"); - done in safe_poll */
			goto ret;
		}
 process_pkt:
		/* Process recv'ed packet */
		opcode = ntohs( ((uint16_t*)rbuf)[0] );
		recv_blk = ntohs( ((uint16_t*)rbuf)[1] );

#if ENABLE_DEBUG_TFTP
		fprintf(stderr, "received %d bytes: %04x %04x\n", len, opcode, recv_blk);
#endif

		if (opcode == TFTP_ERROR) {
			static const char *const errcode_str[] = {
				"",
				"file not found",
				"access violation",
				"disk full",
				"illegal TFTP operation",
				"unknown transfer id",
				"file already exists",
				"no such user",
				"bad option"
			};

			const char *msg = "";

			if (rbuf[4] != '\0') {
				msg = &rbuf[4];
				rbuf[tftp_bufsize - 1] = '\0';
			} else if (recv_blk < ARRAY_SIZE(errcode_str)) {
				msg = errcode_str[recv_blk];
			}
			bb_error_msg("server error: (%u) %s", recv_blk, msg);
			goto ret;
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
					if (!tftp_blocksize_check(blksize, tftp_bufsize - 4)) {
						/* send ERROR 8 to server... */
						/* htons can be impossible to use in const initializer: */
						/*static const uint16_t error_8[2] = { htons(TFTP_ERROR), htons(8) };*/
						/* thus we open-code big-endian layout */
						static const uint8_t error_8[4] = { 0,TFTP_ERROR, 0,8 };
						xsendto(socketfd, error_8, 4, &peer_lsa->sa, peer_lsa->len);
						bb_error_msg("server proposes bad blksize %d, exiting", blksize);
						goto ret;
					}
#if ENABLE_DEBUG_TFTP
					fprintf(stderr, "using blksize %u\n",
							blksize);
#endif
					tftp_bufsize = blksize + 4;
					/* Send ACK for OACK ("block" no: 0) */
					block_nr = 0;
					continue;
				}
				/* rfc2347:
				 * "An option not acknowledged by the server
				 *  must be ignored by the client and server
				 *  as if it were never requested." */
			}

			bb_error_msg("blksize is not supported by server"
						" - reverting to 512");
			tftp_bufsize = TFTP_BLOCKSIZE_DEFAULT + 4;
		}
#endif
		/* block_nr is already advanced to next block# we expect
		 * to get / block# we are about to send next time */

		if (CMD_GET(cmd) && (opcode == TFTP_DATA)) {
			if (recv_blk == block_nr) {
				len = full_write(localfd, &rbuf[4], len - 4);
				if (len < 0) {
					bb_perror_msg(bb_msg_write_error);
					goto ret;
				}
				if (len != (tftp_bufsize - 4)) {
					finished = 1;
				}
				continue; /* send ACK */
			}
			if (recv_blk == (block_nr - 1)) {
				/* Server lost our TFTP_ACK.  Resend it */
				block_nr = recv_blk;
				continue;
			}
		}

		if (CMD_PUT(cmd) && (opcode == TFTP_ACK)) {
			/* did server ACK our last DATA pkt? */
			if (recv_blk == (uint16_t) (block_nr - 1)) {
				if (finished)
					goto ret;
				continue; /* send next block */
			}
		}
		/* Awww... recv'd packet is not recognized! */
		goto recv_again;
		/* why recv_again? - rfc1123 says:
		 * "The sender (i.e., the side originating the DATA packets)
		 *  must never resend the current DATA packet on receipt
		 *  of a duplicate ACK".
		 * DATA pkts are resent ONLY on timeout.
		 * Thus "goto send_again" will ba a bad mistake above.
		 * See:
		 * http://en.wikipedia.org/wiki/Sorcerer's_Apprentice_Syndrome
		 */
	}
 ret:
	if (ENABLE_FEATURE_CLEAN_UP) {
		close(socketfd);
		free(xbuf);
		free(rbuf);
	}
	return finished == 0; /* returns 1 on failure */
}

int tftp_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
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

	USE_GETPUT(cmd =) getopt32(argv,
			USE_FEATURE_TFTP_GET("g") USE_FEATURE_TFTP_PUT("p")
				"l:r:" USE_FEATURE_TFTP_BLOCKSIZE("b:"),
			&localfile, &remotefile
			USE_FEATURE_TFTP_BLOCKSIZE(, &sblocksize));
	argv += optind;

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

	if (!localfile)
		localfile = remotefile;
	if (!remotefile)
		remotefile = localfile;
	/* Error if filename or host is not known */
	if (!remotefile || !argv[0])
		bb_show_usage();

	fd = CMD_GET(cmd) ? STDOUT_FILENO : STDIN_FILENO;
	if (!LONE_DASH(localfile)) {
		fd = xopen(localfile, flags);
	}

	port = bb_lookup_port(argv[1], "udp", 69);
	peer_lsa = xhost2sockaddr(argv[0], port);

#if ENABLE_DEBUG_TFTP
	fprintf(stderr, "using server '%s', remotefile '%s', localfile '%s'\n",
			xmalloc_sockaddr2dotted(&peer_lsa->sa),
			remotefile, localfile);
#endif

	result = tftp( USE_GETPUT(cmd,) peer_lsa, remotefile, fd, port, blocksize);

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);
	if (result != EXIT_SUCCESS && !LONE_DASH(localfile) && CMD_GET(cmd)) {
		unlink(localfile);
	}
	return result;
}

#endif /* ENABLE_FEATURE_TFTP_GET || ENABLE_FEATURE_TFTP_PUT */
