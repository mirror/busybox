/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t n;

	do {
		n = read(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

/* Suppose that you are a shell. You start child processes.
 * They work and eventually exit. You want to get user input.
 * You read stdin. But what happens if last child switched
 * its stdin into O_NONBLOCK mode?
 *
 * *** SURPRISE! It will affect the parent too! ***
 * *** BIG SURPRISE! It stays even after child exits! ***
 *
 * This is a design bug in UNIX API.
 *      fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);
 * will set nonblocking mode not only on _your_ stdin, but
 * also on stdin of your parent, etc.
 *
 * In general,
 *      fd2 = dup(fd1);
 *      fcntl(fd2, F_SETFL, fcntl(fd2, F_GETFL, 0) | O_NONBLOCK);
 * sets both fd1 and fd2 to O_NONBLOCK. This includes cases
 * where duping is done implicitly by fork() etc.
 *
 * We need
 *      fcntl(fd2, F_SETFD, fcntl(fd2, F_GETFD, 0) | O_NONBLOCK);
 * (note SETFD, not SETFL!) but such thing doesn't exist.
 *
 * Alternatively, we need nonblocking_read(fd, ...) which doesn't
 * require O_NONBLOCK dance at all. Actually, it exists:
 *      n = recv(fd, buf, len, MSG_DONTWAIT);
 *      "MSG_DONTWAIT:
 *      Enables non-blocking operation; if the operation
 *      would block, EAGAIN is returned."
 * but recv() works only for sockets!
 *
 * So far I don't see any good solution, I can only propose
 * that affected readers should be careful and use this routine,
 * which detects EAGAIN and uses poll() to wait on the fd.
 * Thankfully, poll() doesn't care about O_NONBLOCK flag.
 */
ssize_t nonblock_safe_read(int fd, void *buf, size_t count)
{
	struct pollfd pfd[1];
	ssize_t n;

	while (1) {
		n = safe_read(fd, buf, count);
		if (n >= 0 || errno != EAGAIN)
			return n;
		/* fd is in O_NONBLOCK mode. Wait using poll and repeat */
		pfd[0].fd = fd;
		pfd[0].events = POLLIN;
		safe_poll(pfd, 1, -1);
	}
}

/*
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
ssize_t full_read(int fd, void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len) {
		cc = safe_read(fd, buf, len);

		if (cc < 0) {
			if (total) {
				/* we already have some! */
				/* user can do another read to know the error code */
				return total;
			}
			return cc; /* read() returns -1 on failure. */
		}
		if (cc == 0)
			break;
		buf = ((char *)buf) + cc;
		total += cc;
		len -= cc;
	}

	return total;
}

// Die with an error message if we can't read the entire buffer.
void xread(int fd, void *buf, size_t count)
{
	if (count) {
		ssize_t size = full_read(fd, buf, count);
		if ((size_t)size != count)
			bb_error_msg_and_die("short read");
	}
}

// Die with an error message if we can't read one character.
unsigned char xread_char(int fd)
{
	char tmp;
	xread(fd, &tmp, 1);
	return tmp;
}

// Read one line a-la fgets. Works only on seekable streams
char *reads(int fd, char *buffer, size_t size)
{
	char *p;

	if (size < 2)
		return NULL;
	size = full_read(fd, buffer, size-1);
	if ((ssize_t)size <= 0)
		return NULL;

	buffer[size] = '\0';
	p = strchr(buffer, '\n');
	if (p) {
		off_t offset;
		*p++ = '\0';
		// avoid incorrect (unsigned) widening
		offset = (off_t)(p - buffer) - (off_t)size;
		// set fd position right after '\n'
		if (offset && lseek(fd, offset, SEEK_CUR) == (off_t)-1)
			return NULL;
	}
	return buffer;
}

// Reads one line a-la fgets (but doesn't save terminating '\n').
// Reads byte-by-byte. Useful when it is important to not read ahead.
// Bytes are appended to pfx (which must be malloced, or NULL).
char *xmalloc_reads(int fd, char *buf, size_t *maxsz_p)
{
	char *p;
	size_t sz = buf ? strlen(buf) : 0;
	size_t maxsz = maxsz_p ? *maxsz_p : MAXINT(size_t);

	goto jump_in;
	while (sz < maxsz) {
		if ((size_t)(p - buf) == sz) {
 jump_in:
			buf = xrealloc(buf, sz + 128);
			p = buf + sz;
			sz += 128;
		}
		/* nonblock_safe_read() because we are used by e.g. shells */
		if (nonblock_safe_read(fd, p, 1) != 1) { /* EOF/error */
			if (p == buf) { /* we read nothing */
				free(buf);
				return NULL;
			}
			break;
		}
		if (*p == '\n')
			break;
		p++;
	}
	*p = '\0';
	if (maxsz_p)
		*maxsz_p  = p - buf;
	p++;
	return xrealloc(buf, p - buf);
}

ssize_t read_close(int fd, void *buf, size_t size)
{
	/*int e;*/
	size = full_read(fd, buf, size);
	/*e = errno;*/
	close(fd);
	/*errno = e;*/
	return size;
}

ssize_t open_read_close(const char *filename, void *buf, size_t size)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return fd;
	return read_close(fd, buf, size);
}

// Read (potentially big) files in one go. File size is estimated
// by stat.
void *xmalloc_open_read_close(const char *filename, size_t *sizep)
{
	char *buf;
	size_t size;
	int fd;
	off_t len;
	struct stat st;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	st.st_size = 0; /* in case fstat fail, define to 0 */
	fstat(fd, &st);
	/* /proc/N/stat files report len 0 here */
	/* In order to make such files readable, we add small const */
	len = st.st_size | 0x3ff; /* read only 1k on unseekable files */
	size = sizep ? *sizep : INT_MAX;
	if (len < size)
		size = len;
	buf = xmalloc(size + 1);
	size = read_close(fd, buf, size);
	if ((ssize_t)size < 0) {
		free(buf);
		return NULL;
	}
	xrealloc(buf, size + 1);
	buf[size] = '\0';

	if (sizep)
		*sizep = size;
	return buf;
}

#ifdef USING_LSEEK_TO_GET_SIZE
/* Alternatively, file size can be obtained by lseek to the end.
 * The code is slightly bigger. Retained in case fstat approach
 * will not work for some weird cases (/proc, block devices, etc).
 * (NB: lseek also can fail to work for some weird files) */

// Read (potentially big) files in one go. File size is estimated by
// lseek to end.
void *xmalloc_open_read_close(const char *filename, size_t *sizep)
{
	char *buf;
	size_t size;
	int fd;
	off_t len;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;

	/* /proc/N/stat files report len 0 here */
	/* In order to make such files readable, we add small const */
	size = 0x3ff; /* read only 1k on unseekable files */
	len = lseek(fd, 0, SEEK_END) | 0x3ff; /* + up to 1k */
	if (len != (off_t)-1) {
		xlseek(fd, 0, SEEK_SET);
		size = sizep ? *sizep : INT_MAX;
		if (len < size)
			size = len;
	}

	buf = xmalloc(size + 1);
	size = read_close(fd, buf, size);
	if ((ssize_t)size < 0) {
		free(buf);
		return NULL;
	}
	xrealloc(buf, size + 1);
	buf[size] = '\0';

	if (sizep)
		*sizep = size;
	return buf;
}
#endif

void *xmalloc_xopen_read_close(const char *filename, size_t *sizep)
{
	void *buf = xmalloc_open_read_close(filename, sizep);
	if (!buf)
		bb_perror_msg_and_die("can't read '%s'", filename);
	return buf;
}
