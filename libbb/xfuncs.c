/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
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

#ifdef L_xcalloc
void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);
	if (ptr == NULL && nmemb != 0 && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif
#endif /* DMALLOC */

#ifdef L_xstrdup
char * bb_xstrdup (const char *s)
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
char * bb_xstrndup (const char *s, int n)
{
	char *t;

	if (ENABLE_DEBUG && s == NULL)
		bb_error_msg_and_die("bb_xstrndup bug");

	t = xmalloc(++n);

	return safe_strncpy(t,s,n);
}
#endif

#ifdef L_xfopen
FILE *bb_xfopen(const char *path, const char *mode)
{
	FILE *fp;
	if ((fp = fopen(path, mode)) == NULL)
		bb_perror_msg_and_die("%s", path);
	return fp;
}
#endif

#ifdef L_xopen
int bb_xopen(const char *pathname, int flags)
{
	return bb_xopen3(pathname, flags, 0777);
}
#endif

#ifdef L_xopen3
int bb_xopen3(const char *pathname, int flags, int mode)
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
ssize_t bb_xread(int fd, void *buf, size_t count)
{
	ssize_t size;

	size = read(fd, buf, count);
	if (size < 0) {
		bb_perror_msg_and_die(bb_msg_read_error);
	}
	return(size);
}
#endif

#ifdef L_xread_all
void bb_xread_all(int fd, void *buf, size_t count)
{
	ssize_t size;

	while (count) {
		if ((size = bb_xread(fd, buf, count)) == 0) {	/* EOF */
			bb_error_msg_and_die("Short read");
		}
		count -= size;
		buf = ((char *) buf) + size;
	}
	return;
}
#endif

#ifdef L_xread_char
unsigned char bb_xread_char(int fd)
{
	char tmp;

	bb_xread_all(fd, &tmp, 1);

	return(tmp);
}
#endif

#ifdef L_xferror
void bb_xferror(FILE *fp, const char *fn)
{
	if (ferror(fp)) {
		bb_error_msg_and_die("%s", fn);
	}
}
#endif

#ifdef L_xferror_stdout
void bb_xferror_stdout(void)
{
	bb_xferror(stdout, bb_msg_standard_output);
}
#endif

#ifdef L_xfflush_stdout
void bb_xfflush_stdout(void)
{
	if (fflush(stdout)) {
		bb_perror_msg_and_die(bb_msg_standard_output);
	}
}
#endif

#ifdef L_spawn
// This does a fork/exec in one call, using vfork().
pid_t bb_spawn(char **argv)
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
pid_t bb_xspawn(char **argv)
{
	pid_t pid = bb_spawn(argv);
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
