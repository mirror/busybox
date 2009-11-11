/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

/* We need to have separate xfuncs.c and xfuncs_printf.c because
 * with current linkers, even with section garbage collection,
 * if *.o module references any of XXXprintf functions, you pull in
 * entire printf machinery. Even if you do not use the function
 * which uses XXXprintf.
 *
 * xfuncs.c contains functions (not necessarily xfuncs)
 * which do not pull in printf, directly or indirectly.
 * xfunc_printf.c contains those which do.
 *
 * TODO: move xmalloc() and xatonum() here.
 */

#include "libbb.h"

/* Turn on nonblocking I/O on a fd */
int FAST_FUNC ndelay_on(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

int FAST_FUNC ndelay_off(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
}

int FAST_FUNC close_on_exec_on(int fd)
{
	return fcntl(fd, F_SETFD, FD_CLOEXEC);
}

char* FAST_FUNC strncpy_IFNAMSIZ(char *dst, const char *src)
{
#ifndef IFNAMSIZ
	enum { IFNAMSIZ = 16 };
#endif
	return strncpy(dst, src, IFNAMSIZ);
}


// Convert unsigned integer to ascii, writing into supplied buffer.
// A truncated result contains the first few digits of the result ala strncpy.
// Returns a pointer past last generated digit, does _not_ store NUL.
void BUG_sizeof_unsigned_not_4(void);
char* FAST_FUNC utoa_to_buf(unsigned n, char *buf, unsigned buflen)
{
	unsigned i, out, res;
	if (sizeof(unsigned) != 4)
		BUG_sizeof_unsigned_not_4();
	if (buflen) {
		out = 0;
		for (i = 1000000000; i; i /= 10) {
			res = n / i;
			if (res || out || i == 1) {
				if (!--buflen) break;
				out++;
				n -= res*i;
				*buf++ = '0' + res;
			}
		}
	}
	return buf;
}

/* Convert signed integer to ascii, like utoa_to_buf() */
char* FAST_FUNC itoa_to_buf(int n, char *buf, unsigned buflen)
{
	if (buflen && n < 0) {
		n = -n;
		*buf++ = '-';
		buflen--;
	}
	return utoa_to_buf((unsigned)n, buf, buflen);
}

// The following two functions use a static buffer, so calling either one a
// second time will overwrite previous results.
//
// The largest 32 bit integer is -2 billion plus null terminator, or 12 bytes.
// It so happens that sizeof(int) * 3 is enough for 32+ bits.
// (sizeof(int) * 3 + 2 is correct for any width, even 8-bit)

static char local_buf[sizeof(int) * 3];

// Convert unsigned integer to ascii using a static buffer (returned).
char* FAST_FUNC utoa(unsigned n)
{
	*(utoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

/* Convert signed integer to ascii using a static buffer (returned). */
char* FAST_FUNC itoa(int n)
{
	*(itoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

/* Emit a string of hex representation of bytes */
char* FAST_FUNC bin2hex(char *p, const char *cp, int count)
{
	while (count) {
		unsigned char c = *cp++;
		/* put lowercase hex digits */
		*p++ = 0x20 | bb_hexdigits_upcase[c >> 4];
		*p++ = 0x20 | bb_hexdigits_upcase[c & 0xf];
		count--;
	}
	return p;
}

/* Return how long the file at fd is, if there's any way to determine it. */
#ifdef UNUSED
off_t FAST_FUNC fdlength(int fd)
{
	off_t bottom = 0, top = 0, pos;
	long size;

	// If the ioctl works for this, return it.

	if (ioctl(fd, BLKGETSIZE, &size) >= 0) return size*512;

	// FIXME: explain why lseek(SEEK_END) is not used here!

	// If not, do a binary search for the last location we can read.  (Some
	// block devices don't do BLKGETSIZE right.)

	do {
		char temp;

		pos = bottom + (top - bottom) / 2;

		// If we can read from the current location, it's bigger.

		if (lseek(fd, pos, SEEK_SET)>=0 && safe_read(fd, &temp, 1)==1) {
			if (bottom == top) bottom = top = (top+1) * 2;
			else bottom = pos;

		// If we can't, it's smaller.

		} else {
			if (bottom == top) {
				if (!top) return 0;
				bottom = top/2;
			}
			else top = pos;
		}
	} while (bottom + 1 != top);

	return pos + 1;
}
#endif

char* FAST_FUNC xmalloc_ttyname(int fd)
{
	char *buf = xzalloc(128);
	int r = ttyname_r(fd, buf, 127);
	if (r) {
		free(buf);
		buf = NULL;
	}
	return buf;
}

/* It is perfectly ok to pass in a NULL for either width or for
 * height, in which case that value will not be set.  */
int FAST_FUNC get_terminal_width_height(int fd, unsigned *width, unsigned *height)
{
	struct winsize win = { 0, 0, 0, 0 };
	int ret = ioctl(fd, TIOCGWINSZ, &win);

	if (height) {
		if (!win.ws_row) {
			char *s = getenv("LINES");
			if (s) win.ws_row = atoi(s);
		}
		if (win.ws_row <= 1 || win.ws_row >= 30000)
			win.ws_row = 24;
		*height = (int) win.ws_row;
	}

	if (width) {
		if (!win.ws_col) {
			char *s = getenv("COLUMNS");
			if (s) win.ws_col = atoi(s);
		}
		if (win.ws_col <= 1 || win.ws_col >= 30000)
			win.ws_col = 80;
		*width = (int) win.ws_col;
	}

	return ret;
}

int FAST_FUNC tcsetattr_stdin_TCSANOW(const struct termios *tp)
{
	return tcsetattr(STDIN_FILENO, TCSANOW, tp);
}

void FAST_FUNC generate_uuid(uint8_t *buf)
{
	/* http://www.ietf.org/rfc/rfc4122.txt
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                          time_low                             |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |       time_mid                |         time_hi_and_version   |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |clk_seq_and_variant            |         node (0-1)            |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                         node (2-5)                            |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * IOW, uuid has this layout:
	 * uint32_t time_low (big endian)
	 * uint16_t time_mid (big endian)
	 * uint16_t time_hi_and_version (big endian)
	 *  version is a 4-bit field:
	 *   1 Time-based
	 *   2 DCE Security, with embedded POSIX UIDs
	 *   3 Name-based (MD5)
	 *   4 Randomly generated
	 *   5 Name-based (SHA-1)
	 * uint16_t clk_seq_and_variant (big endian)
	 *  variant is a 3-bit field:
	 *   0xx Reserved, NCS backward compatibility
	 *   10x The variant specified in rfc4122
	 *   110 Reserved, Microsoft backward compatibility
	 *   111 Reserved for future definition
	 * uint8_t node[6]
	 *
	 * For version 4, these bits are set/cleared:
	 * time_hi_and_version & 0x0fff | 0x4000
	 * clk_seq_and_variant & 0x3fff | 0x8000
	 */
	pid_t pid;
	int i;

	i = open("/dev/urandom", O_RDONLY);
	if (i >= 0) {
		read(i, buf, 16);
		close(i);
	}
	/* Paranoia. /dev/urandom may be missing.
	 * rand() is guaranteed to generate at least [0, 2^15) range,
	 * but lowest bits in some libc are not so "random".  */
	srand(monotonic_us());
	pid = getpid();
	while (1) {
		for (i = 0; i < 16; i++)
			buf[i] ^= rand() >> 5;
		if (pid == 0)
			break;
		srand(pid);
		pid = 0;
	}

	/* version = 4 */
	buf[4 + 2    ] = (buf[4 + 2    ] & 0x0f) | 0x40;
	/* variant = 10x */
	buf[4 + 2 + 2] = (buf[4 + 2 + 2] & 0x3f) | 0x80;
}
