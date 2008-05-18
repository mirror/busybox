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
int ndelay_on(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd,F_GETFL) | O_NONBLOCK);
}

int ndelay_off(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd,F_GETFL) & ~O_NONBLOCK);
}

int close_on_exec_on(int fd)
{
	return fcntl(fd, F_SETFD, FD_CLOEXEC);
}

/* Convert unsigned long long value into compact 4-char
 * representation. Examples: "1234", "1.2k", " 27M", "123T"
 * String is not terminated (buf[4] is untouched) */
void smart_ulltoa4(unsigned long long ul, char buf[5], const char *scale)
{
	const char *fmt;
	char c;
	unsigned v, u, idx = 0;

	if (ul > 9999) { // do not scale if 9999 or less
		ul *= 10;
		do {
			ul /= 1024;
			idx++;
		} while (ul >= 10000);
	}
	v = ul; // ullong divisions are expensive, avoid them

	fmt = " 123456789";
	u = v / 10;
	v = v % 10;
	if (!idx) {
		// 9999 or less: use "1234" format
		// u is value/10, v is last digit
		c = buf[0] = " 123456789"[u/100];
		if (c != ' ') fmt = "0123456789";
		c = buf[1] = fmt[u/10%10];
		if (c != ' ') fmt = "0123456789";
		buf[2] = fmt[u%10];
		buf[3] = "0123456789"[v];
	} else {
		// u is value, v is 1/10ths (allows for 9.2M format)
		if (u >= 10) {
			// value is >= 10: use "123M', " 12M" formats
			c = buf[0] = " 123456789"[u/100];
			if (c != ' ') fmt = "0123456789";
			v = u % 10;
			u = u / 10;
			buf[1] = fmt[u%10];
		} else {
			// value is < 10: use "9.2M" format
			buf[0] = "0123456789"[u];
			buf[1] = '.';
		}
		buf[2] = "0123456789"[v];
		buf[3] = scale[idx]; /* typically scale = " kmgt..." */
	}
}

/* Convert unsigned long long value into compact 5-char representation.
 * String is not terminated (buf[5] is untouched) */
void smart_ulltoa5(unsigned long long ul, char buf[6], const char *scale)
{
	const char *fmt;
	char c;
	unsigned v, u, idx = 0;

	if (ul > 99999) { // do not scale if 99999 or less
		ul *= 10;
		do {
			ul /= 1024;
			idx++;
		} while (ul >= 100000);
	}
	v = ul; // ullong divisions are expensive, avoid them

	fmt = " 123456789";
	u = v / 10;
	v = v % 10;
	if (!idx) {
		// 99999 or less: use "12345" format
		// u is value/10, v is last digit
		c = buf[0] = " 123456789"[u/1000];
		if (c != ' ') fmt = "0123456789";
		c = buf[1] = fmt[u/100%10];
		if (c != ' ') fmt = "0123456789";
		c = buf[2] = fmt[u/10%10];
		if (c != ' ') fmt = "0123456789";
		buf[3] = fmt[u%10];
		buf[4] = "0123456789"[v];
	} else {
		// value has been scaled into 0..9999.9 range
		// u is value, v is 1/10ths (allows for 92.1M format)
		if (u >= 100) {
			// value is >= 100: use "1234M', " 123M" formats
			c = buf[0] = " 123456789"[u/1000];
			if (c != ' ') fmt = "0123456789";
			c = buf[1] = fmt[u/100%10];
			if (c != ' ') fmt = "0123456789";
			v = u % 10;
			u = u / 10;
			buf[2] = fmt[u%10];
		} else {
			// value is < 100: use "92.1M" format
			c = buf[0] = " 123456789"[u/10];
			if (c != ' ') fmt = "0123456789";
			buf[1] = fmt[u%10];
			buf[2] = '.';
		}
		buf[3] = "0123456789"[v];
		buf[4] = scale[idx]; /* typically scale = " kmgt..." */
	}
}


// Convert unsigned integer to ascii, writing into supplied buffer.
// A truncated result contains the first few digits of the result ala strncpy.
// Returns a pointer past last generated digit, does _not_ store NUL.
void BUG_sizeof_unsigned_not_4(void);
char *utoa_to_buf(unsigned n, char *buf, unsigned buflen)
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
char *itoa_to_buf(int n, char *buf, unsigned buflen)
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
char *utoa(unsigned n)
{
	*(utoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

/* Convert signed integer to ascii using a static buffer (returned). */
char *itoa(int n)
{
	*(itoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

/* Emit a string of hex representation of bytes */
char *bin2hex(char *p, const char *cp, int count)
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
off_t fdlength(int fd)
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

/* It is perfectly ok to pass in a NULL for either width or for
 * height, in which case that value will not be set.  */
int get_terminal_width_height(int fd, unsigned *width, unsigned *height)
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
