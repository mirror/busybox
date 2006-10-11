/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

/* All the functions starting with "x" call bb_error_msg_and_die() if they
 * fail, so callers never need to check for errors.  If it returned, it
 * succeeded. */

#ifndef DMALLOC
/* dmalloc provides variants of these that do abort() on failure.
 * Since dmalloc's prototypes overwrite the impls here as they are
 * included after these prototypes in libbb.h, all is well.
 */
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

	t = strdup (s);

	if (t == NULL)
		bb_error_msg_and_die(bb_msg_memory_exhausted);

	return t;
}

// Die if we can't allocate n+1 bytes (space for the null terminator) and copy
// the (possibly truncated to length n) string into it.
char * xstrndup(const char *s, int n)
{
	char *t;

	if (ENABLE_DEBUG && s == NULL)
		bb_error_msg_and_die("xstrndup bug");

	t = xmalloc(++n);

	return safe_strncpy(t,s,n);
}

// Die if we can't open a file and return a FILE * to it.
// Notice we haven't got xfread(), This is for use with fscanf() and friends.
FILE *xfopen(const char *path, const char *mode)
{
	FILE *fp;
	if ((fp = fopen(path, mode)) == NULL)
		bb_perror_msg_and_die("%s", path);
	return fp;
}

// Die if we can't open an existing file and return an fd.
int xopen(const char *pathname, int flags)
{
	if (ENABLE_DEBUG && (flags & O_CREAT))
		bb_error_msg_and_die("xopen() with O_CREAT");

	return xopen3(pathname, flags, 0777);
}

// Die if we can't open a new file and return an fd.
int xopen3(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		bb_perror_msg_and_die("%s", pathname);
	}
	return ret;
}

// Die with an error message if we can't read the entire buffer.
void xread(int fd, void *buf, size_t count)
{
	while (count) {
		ssize_t size = safe_read(fd, buf, count);
		if (size < 1)
			bb_error_msg_and_die("short read");
		count -= size;
		buf = ((char *) buf) + size;
	}
}

// Die with an error message if we can't write the entire buffer.
void xwrite(int fd, void *buf, size_t count)
{
	while (count) {
		ssize_t size = safe_write(fd, buf, count);
		if (size < 1)
			bb_error_msg_and_die("short write");
		count -= size;
		buf = ((char *) buf) + size;
	}
}

// Die with an error message if we can't lseek to the right spot.
void xlseek(int fd, off_t offset, int whence)
{
	if (offset != lseek(fd, offset, whence)) bb_error_msg_and_die("lseek");
}

// Die with an error message if we can't read one character.
unsigned char xread_char(int fd)
{
	char tmp;

	xread(fd, &tmp, 1);

	return tmp;
}

// Die with supplied error message if this FILE * has ferror set.
void xferror(FILE *fp, const char *fn)
{
	if (ferror(fp)) {
		bb_error_msg_and_die("%s", fn);
	}
}

// Die with an error message if stdout has ferror set.
void xferror_stdout(void)
{
	xferror(stdout, bb_msg_standard_output);
}

// Die with an error message if we have trouble flushing stdout.
void xfflush_stdout(void)
{
	if (fflush(stdout)) {
		bb_perror_msg_and_die(bb_msg_standard_output);
	}
}

// This does a fork/exec in one call, using vfork().  Return PID of new child,
// -1 for failure.  Runs argv[0], searching path if that has no / in it.
pid_t spawn(char **argv)
{
	static int failed;
	pid_t pid;
	void *app = ENABLE_FEATURE_SH_STANDALONE_SHELL ? find_applet_by_name(argv[0]) : 0;

	// Be nice to nommu machines.
	failed = 0;
	pid = vfork();
	if (pid < 0) return pid;
	if (!pid) {
		execvp(app ? CONFIG_BUSYBOX_EXEC_PATH : *argv, argv);

		// We're sharing a stack with blocked parent, let parent know we failed
		// and then exit to unblock parent (but don't run atexit() stuff, which
		// would screw up parent.)

		failed = -1;
		_exit(0);
	}
	return failed ? failed : pid;
}

// Die with an error message if we can't spawn a child process.
pid_t xspawn(char **argv)
{
	pid_t pid = spawn(argv);
	if (pid < 0) bb_perror_msg_and_die("%s", *argv);
	return pid;
}

// Wait for the specified child PID to exit, returning child's error return.
int wait4pid(int pid)
{
	int status;

	if (pid == -1 || waitpid(pid, &status, 0) == -1) return -1;
	if (WIFEXITED(status)) return WEXITSTATUS(status);
	if (WIFSIGNALED(status)) return WTERMSIG(status);
	return 0;
}

void xsetenv(const char *key, const char *value)
{
	if (setenv(key, value, 1))
		bb_error_msg_and_die(bb_msg_memory_exhausted);
}

// Convert unsigned integer to ascii, writing into supplied buffer.  A
// truncated result is always null terminated (unless buflen is 0), and
// contains the first few digits of the result ala strncpy.
void utoa_to_buf(unsigned n, char *buf, unsigned buflen)
{
	int i, out = 0;
	if (buflen) {
		for (i=1000000000; i; i/=10) {
			int res = n/i;

			if ((res || out || i == 1) && --buflen>0) {
				out++;
				n -= res*i;
				*buf++ = '0' + res;
			}
		}
		*buf = 0;
	}
}

// Convert signed integer to ascii, like utoa_to_buf()
void itoa_to_buf(int n, char *buf, unsigned buflen)
{
	if (buflen && n<0) {
		n = -n;
		*buf++ = '-';
		buflen--;
	}
	utoa_to_buf((unsigned)n, buf, buflen);
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
	utoa_to_buf(n, local_buf, sizeof(local_buf));

	return local_buf;
}

// Convert signed integer to ascii using a static buffer (returned).
char *itoa(int n)
{
	itoa_to_buf(n, local_buf, sizeof(local_buf));

	return local_buf;
}

// Die with an error message if we can't set gid.  (Because resource limits may
// limit this user to a given number of processes, and if that fills up the
// setgid() will fail and we'll _still_be_root_, which is bad.)
void xsetgid(gid_t gid)
{
	if (setgid(gid)) bb_error_msg_and_die("setgid");
}

// Die with an error message if we cant' set uid.  (See xsetgid() for why.)
void xsetuid(uid_t uid)
{
	if (setuid(uid)) bb_error_msg_and_die("setuid");
}

// Return how long the file at fd is, if there's any way to determine it.
off_t fdlength(int fd)
{
	off_t bottom = 0, top = 0, pos;
	long size;

	// If the ioctl works for this, return it.

	if (ioctl(fd, BLKGETSIZE, &size) >= 0) return size*512;

	// If not, do a binary search for the last location we can read.  (Some
	// block devices don't do BLKGETSIZE right.)

	do {
		char temp;

		pos = bottom + (top - bottom) / 2;

		// If we can read from the current location, it's bigger.

		if (lseek(fd, pos, 0)>=0 && safe_read(fd, &temp, 1)==1) {
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

	if (r < 0) bb_error_msg_and_die(bb_msg_memory_exhausted);
	return string_ptr;
}

// Die with an error message if we can't copy an entire FILE * to stdout, then
// close that file.
void xprint_and_close_file(FILE *file)
{
	// copyfd outputs error messages for us.
	if (bb_copyfd_eof(fileno(file), 1) == -1)
		exit(xfunc_error_retval);

	fclose(file);
}

// Die if we can't chdir to a new path.
void xchdir(const char *path)
{
	if (chdir(path))
		bb_perror_msg_and_die("chdir(%s)", path);
}

// Print a warning message if opendir() fails, but don't die.
DIR *warn_opendir(const char *path)
{
	DIR *dp;

	if ((dp = opendir(path)) == NULL) {
		bb_perror_msg("unable to open `%s'", path);
		return NULL;
	}
	return dp;
}

// Die with an error message if opendir() fails.
DIR *xopendir(const char *path)
{
	DIR *dp;

	if ((dp = opendir(path)) == NULL)
		bb_perror_msg_and_die("unable to open `%s'", path);
	return dp;
}

#ifndef BB_NOMMU
// Die with an error message if we can't daemonize.
void xdaemon(int nochdir, int noclose)
{
	if (daemon(nochdir, noclose)) bb_perror_msg_and_die("daemon");
}
#endif

// Die with an error message if we can't open a new socket.
int xsocket(int domain, int type, int protocol)
{
	int r = socket(domain, type, protocol);

	if (r < 0) bb_perror_msg_and_die("socket");

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

// xstat() - a stat() which dies on failure with meaningful error message
void xstat(char *name, struct stat *stat_buf)
{
	if (stat(name, stat_buf))
		bb_perror_msg_and_die("can't stat '%s'", name);
}

/* It is perfectly ok to pass in a NULL for either width or for
 *  * height, in which case that value will not be set.  */
int get_terminal_width_height(int fd, int *width, int *height)
{
	struct winsize win = { 0, 0, 0, 0 };
	int ret = ioctl(fd, TIOCGWINSZ, &win);
	if (!win.ws_row) {
		char *s = getenv("LINES");
		if (s) win.ws_row = atoi(s);
	}
	if (win.ws_row <= 1) win.ws_row = 24;
	if (!win.ws_col) {
		char *s = getenv("COLUMNS");
		if (s) win.ws_col = atoi(s);
	}
	if (win.ws_col <= 1) win.ws_col = 80;
	if (height) *height = (int) win.ws_row;
	if (width) *width = (int) win.ws_col;

	return ret;
}
