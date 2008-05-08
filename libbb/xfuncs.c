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

#include "libbb.h"

/* All the functions starting with "x" call bb_error_msg_and_die() if they
 * fail, so callers never need to check for errors.  If it returned, it
 * succeeded. */

#ifndef DMALLOC
/* dmalloc provides variants of these that do abort() on failure.
 * Since dmalloc's prototypes overwrite the impls here as they are
 * included after these prototypes in libbb.h, all is well.
 */
// Warn if we can't allocate size bytes of memory.
void *malloc_or_warn(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		bb_error_msg(bb_msg_memory_exhausted);
	return ptr;
}

// Die if we can't allocate size bytes of memory.
void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}

// Die if we can't resize previously allocated memory.  (This returns a pointer
// to the new memory, which may or may not be the same as the old memory.
// It'll copy the contents to a new chunk and free the old one if necessary.)
void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif /* DMALLOC */

// Die if we can't allocate and zero size bytes of memory.
void *xzalloc(size_t size)
{
	void *ptr = xmalloc(size);
	memset(ptr, 0, size);
	return ptr;
}

// Die if we can't copy a string to freshly allocated memory.
char * xstrdup(const char *s)
{
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup(s);

	if (t == NULL)
		bb_error_msg_and_die(bb_msg_memory_exhausted);

	return t;
}

// Die if we can't allocate n+1 bytes (space for the null terminator) and copy
// the (possibly truncated to length n) string into it.
char *xstrndup(const char *s, int n)
{
	int m;
	char *t;

	if (ENABLE_DEBUG && s == NULL)
		bb_error_msg_and_die("xstrndup bug");

	/* We can just xmalloc(n+1) and strncpy into it, */
	/* but think about xstrndup("abc", 10000) wastage! */
	m = n;
	t = (char*) s;
	while (m) {
		if (!*t) break;
		m--;
		t++;
	}
	n -= m;
	t = xmalloc(n + 1);
	t[n] = '\0';

	return memcpy(t, s, n);
}

// Die if we can't open a file and return a FILE * to it.
// Notice we haven't got xfread(), This is for use with fscanf() and friends.
FILE *xfopen(const char *path, const char *mode)
{
	FILE *fp = fopen(path, mode);
	if (fp == NULL)
		bb_perror_msg_and_die("can't open '%s'", path);
	return fp;
}

// Die if we can't open a file and return a fd.
int xopen3(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		bb_perror_msg_and_die("can't open '%s'", pathname);
	}
	return ret;
}

// Die if we can't open an existing file and return a fd.
int xopen(const char *pathname, int flags)
{
	return xopen3(pathname, flags, 0666);
}

// Warn if we can't open a file and return a fd.
int open3_or_warn(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		bb_perror_msg("can't open '%s'", pathname);
	}
	return ret;
}

// Warn if we can't open a file and return a fd.
int open_or_warn(const char *pathname, int flags)
{
	return open3_or_warn(pathname, flags, 0666);
}

void xunlink(const char *pathname)
{
	if (unlink(pathname))
		bb_perror_msg_and_die("can't remove file '%s'", pathname);
}

void xrename(const char *oldpath, const char *newpath)
{
	if (rename(oldpath, newpath))
		bb_perror_msg_and_die("can't move '%s' to '%s'", oldpath, newpath);
}

int rename_or_warn(const char *oldpath, const char *newpath)
{
	int n = rename(oldpath, newpath);
	if (n)
		bb_perror_msg("can't move '%s' to '%s'", oldpath, newpath);
	return n;
}

void xpipe(int filedes[2])
{
	if (pipe(filedes))
		bb_perror_msg_and_die("can't create pipe");
}

// Turn on nonblocking I/O on a fd
int ndelay_on(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd,F_GETFL) | O_NONBLOCK);
}

int close_on_exec_on(int fd)
{
	return fcntl(fd, F_SETFD, FD_CLOEXEC);
}

int ndelay_off(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd,F_GETFL) & ~O_NONBLOCK);
}

void xdup2(int from, int to)
{
	if (dup2(from, to) != to)
		bb_perror_msg_and_die("can't duplicate file descriptor");
}

// "Renumber" opened fd
void xmove_fd(int from, int to)
{
	if (from == to)
		return;
	xdup2(from, to);
	close(from);
}

// Die with an error message if we can't write the entire buffer.
void xwrite(int fd, const void *buf, size_t count)
{
	if (count) {
		ssize_t size = full_write(fd, buf, count);
		if (size != count)
			bb_error_msg_and_die("short write");
	}
}

// Die with an error message if we can't lseek to the right spot.
off_t xlseek(int fd, off_t offset, int whence)
{
	off_t off = lseek(fd, offset, whence);
	if (off == (off_t)-1) {
		if (whence == SEEK_SET)
			bb_perror_msg_and_die("lseek(%"OFF_FMT"u)", offset);
		bb_perror_msg_and_die("lseek");
	}
	return off;
}

// Die with supplied filename if this FILE * has ferror set.
void die_if_ferror(FILE *fp, const char *fn)
{
	if (ferror(fp)) {
		/* ferror doesn't set useful errno */
		bb_error_msg_and_die("%s: I/O error", fn);
	}
}

// Die with an error message if stdout has ferror set.
void die_if_ferror_stdout(void)
{
	die_if_ferror(stdout, bb_msg_standard_output);
}

// Die with an error message if we have trouble flushing stdout.
void xfflush_stdout(void)
{
	if (fflush(stdout)) {
		bb_perror_msg_and_die(bb_msg_standard_output);
	}
}

void xsetenv(const char *key, const char *value)
{
	if (setenv(key, value, 1))
		bb_error_msg_and_die(bb_msg_memory_exhausted);
}

/* Converts unsigned long long value into compact 4-char
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

/* Converts unsigned long long value into compact 5-char representation.
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

// Convert signed integer to ascii, like utoa_to_buf()
char *itoa_to_buf(int n, char *buf, unsigned buflen)
{
	if (buflen && n<0) {
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
// Int should always be 32 bits on any remotely Unix-like system, see
// http://www.unix.org/whitepapers/64bit.html for the reasons why.

static char local_buf[12];

// Convert unsigned integer to ascii using a static buffer (returned).
char *utoa(unsigned n)
{
	*(utoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

// Convert signed integer to ascii using a static buffer (returned).
char *itoa(int n)
{
	*(itoa_to_buf(n, local_buf, sizeof(local_buf))) = '\0';

	return local_buf;
}

// Emit a string of hex representation of bytes
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

// Die with an error message if we can't set gid.  (Because resource limits may
// limit this user to a given number of processes, and if that fills up the
// setgid() will fail and we'll _still_be_root_, which is bad.)
void xsetgid(gid_t gid)
{
	if (setgid(gid)) bb_perror_msg_and_die("setgid");
}

// Die with an error message if we can't set uid.  (See xsetgid() for why.)
void xsetuid(uid_t uid)
{
	if (setuid(uid)) bb_perror_msg_and_die("setuid");
}

// Return how long the file at fd is, if there's any way to determine it.
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

int bb_putchar(int ch)
{
	/* time.c needs putc(ch, stdout), not putchar(ch).
	 * it does "stdout = stderr;", but then glibc's putchar()
	 * doesn't work as expected. bad glibc, bad */
	return putc(ch, stdout);
}

// Die with an error message if we can't malloc() enough space and do an
// sprintf() into that space.
char *xasprintf(const char *format, ...)
{
	va_list p;
	int r;
	char *string_ptr;

#if 1
	// GNU extension
	va_start(p, format);
	r = vasprintf(&string_ptr, format, p);
	va_end(p);
#else
	// Bloat for systems that haven't got the GNU extension.
	va_start(p, format);
	r = vsnprintf(NULL, 0, format, p);
	va_end(p);
	string_ptr = xmalloc(r+1);
	va_start(p, format);
	r = vsnprintf(string_ptr, r+1, format, p);
	va_end(p);
#endif

	if (r < 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return string_ptr;
}

#if 0 /* If we will ever meet a libc which hasn't [f]dprintf... */
int fdprintf(int fd, const char *format, ...)
{
	va_list p;
	int r;
	char *string_ptr;

#if 1
	// GNU extension
	va_start(p, format);
	r = vasprintf(&string_ptr, format, p);
	va_end(p);
#else
	// Bloat for systems that haven't got the GNU extension.
	va_start(p, format);
	r = vsnprintf(NULL, 0, format, p) + 1;
	va_end(p);
	string_ptr = malloc(r);
	if (string_ptr) {
		va_start(p, format);
		r = vsnprintf(string_ptr, r, format, p);
		va_end(p);
	}
#endif

	if (r >= 0) {
		full_write(fd, string_ptr, r);
		free(string_ptr);
	}
	return r;
}
#endif

// Die with an error message if we can't copy an entire FILE * to stdout, then
// close that file.
void xprint_and_close_file(FILE *file)
{
	fflush(stdout);
	// copyfd outputs error messages for us.
	if (bb_copyfd_eof(fileno(file), 1) == -1)
		xfunc_die();

	fclose(file);
}

// Die if we can't chdir to a new path.
void xchdir(const char *path)
{
	if (chdir(path))
		bb_perror_msg_and_die("chdir(%s)", path);
}

void xchroot(const char *path)
{
	if (chroot(path))
		bb_perror_msg_and_die("can't change root directory to %s", path);
}

// Print a warning message if opendir() fails, but don't die.
DIR *warn_opendir(const char *path)
{
	DIR *dp;

	dp = opendir(path);
	if (!dp)
		bb_perror_msg("can't open '%s'", path);
	return dp;
}

// Die with an error message if opendir() fails.
DIR *xopendir(const char *path)
{
	DIR *dp;

	dp = opendir(path);
	if (!dp)
		bb_perror_msg_and_die("can't open '%s'", path);
	return dp;
}

// Die with an error message if we can't open a new socket.
int xsocket(int domain, int type, int protocol)
{
	int r = socket(domain, type, protocol);

	if (r < 0) {
		/* Hijack vaguely related config option */
#if ENABLE_VERBOSE_RESOLUTION_ERRORS
		const char *s = "INET";
		if (domain == AF_PACKET) s = "PACKET";
		if (domain == AF_NETLINK) s = "NETLINK";
USE_FEATURE_IPV6(if (domain == AF_INET6) s = "INET6";)
		bb_perror_msg_and_die("socket(AF_%s)", s);
#else
		bb_perror_msg_and_die("socket");
#endif
	}

	return r;
}

// Die with an error message if we can't bind a socket to an address.
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
{
	if (bind(sockfd, my_addr, addrlen)) bb_perror_msg_and_die("bind");
}

// Die with an error message if we can't listen for connections on a socket.
void xlisten(int s, int backlog)
{
	if (listen(s, backlog)) bb_perror_msg_and_die("listen");
}

/* Die with an error message if sendto failed.
 * Return bytes sent otherwise  */
ssize_t xsendto(int s, const  void *buf, size_t len, const struct sockaddr *to,
				socklen_t tolen)
{
	ssize_t ret = sendto(s, buf, len, 0, to, tolen);
	if (ret < 0) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(s);
		bb_perror_msg_and_die("sendto");
	}
	return ret;
}

// xstat() - a stat() which dies on failure with meaningful error message
void xstat(const char *name, struct stat *stat_buf)
{
	if (stat(name, stat_buf))
		bb_perror_msg_and_die("can't stat '%s'", name);
}

// selinux_or_die() - die if SELinux is disabled.
void selinux_or_die(void)
{
#if ENABLE_SELINUX
	int rc = is_selinux_enabled();
	if (rc == 0) {
		bb_error_msg_and_die("SELinux is disabled");
	} else if (rc < 0) {
		bb_error_msg_and_die("is_selinux_enabled() failed");
	}
#else
	bb_error_msg_and_die("SELinux support is disabled");
#endif
}

/* It is perfectly ok to pass in a NULL for either width or for
 * height, in which case that value will not be set.  */
int get_terminal_width_height(int fd, int *width, int *height)
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

void ioctl_or_perror_and_die(int fd, unsigned request, void *argp, const char *fmt,...)
{
	va_list p;

	if (ioctl(fd, request, argp) < 0) {
		va_start(p, fmt);
		bb_verror_msg(fmt, p, strerror(errno));
		/* xfunc_die can actually longjmp, so be nice */
		va_end(p);
		xfunc_die();
	}
}

int ioctl_or_perror(int fd, unsigned request, void *argp, const char *fmt,...)
{
	va_list p;
	int ret = ioctl(fd, request, argp);

	if (ret < 0) {
		va_start(p, fmt);
		bb_verror_msg(fmt, p, strerror(errno));
		va_end(p);
	}
	return ret;
}

#if ENABLE_IOCTL_HEX2STR_ERROR
int bb_ioctl_or_warn(int fd, unsigned request, void *argp, const char *ioctl_name)
{
	int ret;

	ret = ioctl(fd, request, argp);
	if (ret < 0)
		bb_simple_perror_msg(ioctl_name);
	return ret;
}
void bb_xioctl(int fd, unsigned request, void *argp, const char *ioctl_name)
{
	if (ioctl(fd, request, argp) < 0)
		bb_simple_perror_msg_and_die(ioctl_name);
}
#else
int bb_ioctl_or_warn(int fd, unsigned request, void *argp)
{
	int ret;

	ret = ioctl(fd, request, argp);
	if (ret < 0)
		bb_perror_msg("ioctl %#x failed", request);
	return ret;
}
void bb_xioctl(int fd, unsigned request, void *argp)
{
	if (ioctl(fd, request, argp) < 0)
		bb_perror_msg_and_die("ioctl %#x failed", request);
}
#endif
