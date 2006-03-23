/*-------------------------------------------------------------------------
 * Filename:      xmodem.c
 * Version:       $Id: rx.c,v 1.2 2004/03/15 08:28:46 andersen Exp $
 * Copyright:     Copyright (C) 2001, Hewlett-Packard Company
 * Author:        Christopher Hoover <ch@hpl.hp.com>
 * Description:   xmodem functionality for uploading of kernels
 *                and the like
 * Created at:    Thu Dec 20 01:58:08 PST 2001
 *-----------------------------------------------------------------------*/
/*
 * xmodem.c: xmodem functionality for uploading of kernels and
 *            the like
 *
 * Copyright (C) 2001 Hewlett-Packard Laboratories
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * This was originally written for blob and then adapted for busybox.
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "busybox.h"


#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define BS  0x08

/*

Cf:

  http://www.textfiles.com/apple/xmodem
  http://www.phys.washington.edu/~belonis/xmodem/docxmodem.txt
  http://www.phys.washington.edu/~belonis/xmodem/docymodem.txt
  http://www.phys.washington.edu/~belonis/xmodem/modmprot.col

*/

#define TIMEOUT 1
#define TIMEOUT_LONG 10
#define MAXERRORS 10

static inline void write_byte(int fd, char cc) {
	write(fd, &cc, 1);
}

static inline void write_flush(int fd) {
	tcdrain(fd);
}

static inline void read_flush(int fd) {
	tcflush(fd, TCIFLUSH);
}

static int read_byte(int fd, unsigned int timeout) {
	char buf[1];
	int n;

	alarm(timeout);

	n = read(fd, &buf, 1);

	alarm(0);

	if (n == 1)
		return buf[0] & 0xff;
	else
		return -1;
}

static int receive(char *error_buf, size_t error_buf_size,
				   int ttyfd, int filefd)
{
	char blockBuf[1024];
	unsigned int errors = 0;
	unsigned int wantBlockNo = 1;
	unsigned int length = 0;
	int docrc = 1;
	char nak = 'C';
	unsigned int timeout = TIMEOUT_LONG;

#define note_error(fmt,args...) \
	snprintf(error_buf, error_buf_size, fmt,##args)

	read_flush(ttyfd);

	/* Ask for CRC; if we get errors, we will go with checksum */
	write_byte(ttyfd, nak);
	write_flush(ttyfd);

	for (;;) {
		int blockBegin;
		int blockNo, blockNoOnesCompl;
		int blockLength;
		int cksum = 0;
		int crcHi = 0;
		int crcLo = 0;

		blockBegin = read_byte(ttyfd, timeout);
		if (blockBegin < 0)
			goto timeout;

		timeout = TIMEOUT;
		nak = NAK;

		switch (blockBegin) {
		case SOH:
		case STX:
			break;

		case EOT:
			write_byte(ttyfd, ACK);
			write_flush(ttyfd);
			goto done;

		default:
			goto error;
		}

		/* block no */
		blockNo = read_byte(ttyfd, TIMEOUT);
		if (blockNo < 0)
			goto timeout;

		/* block no one's compliment */
		blockNoOnesCompl = read_byte(ttyfd, TIMEOUT);
		if (blockNoOnesCompl < 0)
			goto timeout;

		if (blockNo != (255 - blockNoOnesCompl)) {
			note_error("bad block ones compl");
			goto error;
		}

		blockLength = (blockBegin == SOH) ? 128 : 1024;

		{
			int i;

			for (i = 0; i < blockLength; i++) {
				int cc = read_byte(ttyfd, TIMEOUT);
				if (cc < 0)
					goto timeout;
				blockBuf[i] = cc;
			}
		}

		if (docrc) {
			crcHi = read_byte(ttyfd, TIMEOUT);
			if (crcHi < 0)
				goto timeout;

			crcLo = read_byte(ttyfd, TIMEOUT);
			if (crcLo < 0)
				goto timeout;
		} else {
			cksum = read_byte(ttyfd, TIMEOUT);
			if (cksum < 0)
				goto timeout;
		}

		if (blockNo == ((wantBlockNo - 1) & 0xff)) {
			/* a repeat of the last block is ok, just ignore it. */
			/* this also ignores the initial block 0 which is */
			/* meta data. */
			goto next;
		} else if (blockNo != (wantBlockNo & 0xff)) {
			note_error("unexpected block no, 0x%08x, expecting 0x%08x", blockNo, wantBlockNo);
			goto error;
		}

		if (docrc) {
			int crc = 0;
			int i, j;
			int expectedCrcHi;
			int expectedCrcLo;

			for (i = 0; i < blockLength; i++) {
				crc = crc ^ (int) blockBuf[i] << 8;
				for (j = 0; j < 8; j++)
					if (crc & 0x8000)
						crc = crc << 1 ^ 0x1021;
					else
						crc = crc << 1;
			}

			expectedCrcHi = (crc >> 8) & 0xff;
			expectedCrcLo = crc & 0xff;

			if ((crcHi != expectedCrcHi) ||
			    (crcLo != expectedCrcLo)) {
				note_error("crc error, expected 0x%02x 0x%02x, got 0x%02x 0x%02x", expectedCrcHi, expectedCrcLo, crcHi, crcLo);
				goto error;
			}
		} else {
			unsigned char expectedCksum = 0;
			int i;

			for (i = 0; i < blockLength; i++)
				expectedCksum += blockBuf[i];

			if (cksum != expectedCksum) {
				note_error("checksum error, expected 0x%02x, got 0x%02x", expectedCksum, cksum);
				goto error;
			}
		}

		wantBlockNo++;
		length += blockLength;

		if (bb_full_write(filefd, blockBuf, blockLength) < 0) {
			note_error("write to file failed: %m");
			goto fatal;
		}

	next:
		errors = 0;
		write_byte(ttyfd, ACK);
		write_flush(ttyfd);
		continue;

	error:
	timeout:
		errors++;
		if (errors == MAXERRORS) {
			/* Abort */
			int i;

			// if using crc, try again w/o crc
			if (nak == 'C') {
				nak = NAK;
				errors = 0;
				docrc = 0;
				goto timeout;
			}

			note_error("too many errors; giving up");

		fatal:
			for (i = 0; i < 5; i ++)
				write_byte(ttyfd, CAN);
			for (i = 0; i < 5; i ++)
				write_byte(ttyfd, BS);
			write_flush(ttyfd);
			return -1;
		}

		read_flush(ttyfd);
		write_byte(ttyfd, nak);
		write_flush(ttyfd);
	}

 done:
	return length;

#undef note_error
}

static void sigalrm_handler(int signum)
{
}

int rx_main(int argc, char **argv)
{
	char *fn;
	int ttyfd, filefd;
	struct termios tty, orig_tty;
	struct sigaction act;
	int n;
	char error_buf[256];

	if (argc != 2)
			bb_show_usage();

	fn = argv[1];
	ttyfd = open("/dev/tty", O_RDWR);
	if (ttyfd < 0)
			bb_error_msg_and_die("%s: open on /dev/tty failed: %m\n", argv[0]);

	filefd = open(fn, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (filefd < 0)
			bb_error_msg_and_die("%s: open on %s failed: %m\n", argv[0], fn);

	if (tcgetattr(ttyfd, &tty) < 0)
			bb_error_msg_and_die("%s: tcgetattr failed: %m\n", argv[0]);

	orig_tty = tty;

	cfmakeraw(&tty);
	tcsetattr(ttyfd, TCSAFLUSH, &tty);

	memset(&act, 0, sizeof(act));
	act.sa_handler = sigalrm_handler;
	sigaction(SIGALRM, &act, 0);

	n = receive(error_buf, sizeof(error_buf), ttyfd, filefd);

	close(filefd);

	tcsetattr(ttyfd, TCSAFLUSH, &orig_tty);

	if (n < 0)
			bb_error_msg_and_die("\n%s: receive failed:\n  %s\n",
							   argv[0], error_buf);

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
