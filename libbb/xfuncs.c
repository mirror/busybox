/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

#ifndef DMALLOC
#ifdef L_xmalloc
void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif

#ifdef L_xrealloc
void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif

#ifdef L_xzalloc
void *xzalloc(size_t size)
{
	void *ptr = xmalloc(size);
	memset(ptr, 0, size);
	return ptr;
}
#endif

#endif /* DMALLOC */

#ifdef L_xstrdup
char * xstrdup (const char *s)
{
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup (s);

	if (t == NULL)
		bb_error_msg_and_die(bb_msg_memory_exhausted);

	return t;
}
#endif

#ifdef L_xstrndup
char * xstrndup (const char *s, int n)
{
	char *t;

	if (ENABLE_DEBUG && s == NULL)
		bb_error_msg_and_die("xstrndup bug");

	t = xmalloc(++n);

	return safe_strncpy(t,s,n);
}
#endif

#ifdef L_xfopen
FILE *xfopen(const char *path, const char *mode)
{
	FILE *fp;
	if ((fp = fopen(path, mode)) == NULL)
		bb_perror_msg_and_die("%s", path);
	return fp;
}
#endif

#ifdef L_xopen
int xopen(const char *pathname, int flags)
{
	return xopen3(pathname, flags, 0777);
}
#endif

#ifdef L_xopen3
int xopen3(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		bb_perror_msg_and_die("%s", pathname);
	}
	return ret;
}
#endif

#ifdef L_xread

// Die with an error message if we can't read the entire buffer.

void xread(int fd, void *buf, size_t count)
{
	while (count) {
		ssize_t size;

		if ((size = safe_read(fd, buf, count)) < 1)
			bb_error_msg_and_die("Short read");
		count -= size;
		buf = ((char *) buf) + size;
	}
}
#endif

#ifdef L_xwrite

// Die with an error message if we can't write the entire buffer.

void xwrite(int fd, void *buf, size_t count)
{
	while (count) {
		ssize_t size;

		if ((size = safe_write(fd, buf, count)) < 1)
			bb_error_msg_and_die("Short write");
		count -= size;
		buf = ((char *) buf) + size;
	}
}
#endif

#ifdef L_xlseek

// Die if we can't lseek to the right spot.

void xlseek(int fd, off_t offset, int whence)
{
	if (whence != lseek(fd, offset, whence)) bb_error_msg_and_die("lseek");
}
#endif

#ifdef L_xread_char
unsigned char xread_char(int fd)
{
	char tmp;

	xread(fd, &tmp, 1);

	return(tmp);
}
#endif

#ifdef L_xferror
void xferror(FILE *fp, const char *fn)
{
	if (ferror(fp)) {
		bb_error_msg_and_die("%s", fn);
	}
}
#endif

#ifdef L_xferror_stdout
void xferror_stdout(void)
{
	xferror(stdout, bb_msg_standard_output);
}
#endif

#ifdef L_xfflush_stdout
void xfflush_stdout(void)
{
	if (fflush(stdout)) {
		bb_perror_msg_and_die(bb_msg_standard_output);
	}
}
#endif

#ifdef L_spawn
// This does a fork/exec in one call, using vfork().
pid_t spawn(char **argv)
{
	static int failed;
	pid_t pid;
	void *app = find_applet_by_name(argv[0]);

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
#endif

#ifdef L_xspawn
pid_t xspawn(char **argv)
{
	pid_t pid = spawn(argv);
	if (pid < 0) bb_perror_msg_and_die("%s", *argv);
	return pid;
}
#endif

#ifdef L_wait4
int wait4pid(int pid)
{
	int status;

	if (pid == -1 || waitpid(pid, &status, 0) == -1) return -1;
	if (WIFEXITED(status)) return WEXITSTATUS(status);
	if (WIFSIGNALED(status)) return WTERMSIG(status);
	return 0;
}
#endif

#ifdef L_itoa
// Largest 32 bit integer is -2 billion plus null terminator.
// Int should always be 32 bits on a Unix-oid system, see
// http://www.unix.org/whitepapers/64bit.html
static char local_buf[12];

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

// Note: uses static buffer, calling it twice in a row will overwrite.

char *utoa(unsigned n)
{
	utoa_to_buf(n, local_buf, sizeof(local_buf));

	return local_buf;
}

void itoa_to_buf(int n, char *buf, unsigned buflen)
{
	if (buflen && n<0) {
		n = -n;
		*buf++ = '-';
		buflen--;
	}
	utoa_to_buf((unsigned)n, buf, buflen);
}

// Note: uses static buffer, calling it twice in a row will overwrite.

char *itoa(int n)
{
	itoa_to_buf(n, local_buf, sizeof(local_buf));

	return local_buf;
}
#endif

#ifdef L_setuid
void xsetgid(gid_t gid)
{
	if (setgid(gid)) bb_error_msg_and_die("setgid");
}

void xsetuid(uid_t uid)
{
	if (setuid(uid)) bb_error_msg_and_die("setuid");
}
#endif

#ifdef L_fdlength
off_t fdlength(int fd)
{
	off_t bottom = 0, top = 0, pos;
	long size;

	// If the ioctl works for this, return it.

	if (ioctl(fd, BLKGETSIZE, &size) >= 0) return size*512;

	// If not, do a binary search for the last location we can read.

	do {
		char temp;

		pos = bottom + (top - bottom) / 2;;

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
#endif

#ifdef L_xasprintf
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

	if (r < 0) bb_perror_msg_and_die("xasprintf");
	return string_ptr;
}
#endif

#ifdef L_xprint_and_close_file
void xprint_and_close_file(FILE *file)
{
	// copyfd outputs error messages for us.
	if (bb_copyfd_eof(fileno(file), 1) == -1) exit(bb_default_error_retval);

	fclose(file);
}
#endif

#ifdef L_xchdir
void xchdir(const char *path)
{
	if (chdir(path))
		bb_perror_msg_and_die("chdir(%s)", path);
}
#endif

#ifdef L_warn_opendir
DIR *warn_opendir(const char *path)
{
	DIR *dp;

	if ((dp = opendir(path)) == NULL) {
		bb_perror_msg("unable to open `%s'", path);
		return NULL;
	}
	return dp;
}
#endif

#ifdef L_xopendir
DIR *xopendir(const char *path)
{
	DIR *dp;

	if ((dp = opendir(path)) == NULL)
		bb_perror_msg_and_die("unable to open `%s'", path);
	return dp;
}
#endif

#ifdef L_xdaemon
#ifndef BB_NOMMU
void xdaemon(int nochdir, int noclose)
{
	    if (daemon(nochdir, noclose)) bb_perror_msg_and_die("daemon");
}
#endif
#endif

#ifdef L_xbind
void xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
{
	if (bind(sockfd, my_addr, addrlen)) bb_perror_msg_and_die("bind");
}
#endif

#ifdef L_xsocket
int xsocket(int domain, int type, int protocol)
{
	int r = socket(domain, type, protocol);

	if (r < 0) bb_perror_msg_and_die("socket");

	return r;
}
#endif

#ifdef L_xlisten
void xlisten(int s, int backlog)
{
	if (listen(s, backlog)) bb_perror_msg_and_die("listen");
}
#endif
