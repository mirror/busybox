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

		if (cc < 0)
			return cc;	/* read() returns -1 on failure. */

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
		if (size != count)
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
		offset = (off_t)(p-buffer) - (off_t)size;
		// set fd position right after '\n'
		if (offset && lseek(fd, offset, SEEK_CUR) == (off_t)-1)
			return NULL;
	}
	return buffer;
}

ssize_t read_close(int fd, void *buf, size_t size)
{
	int e;
	size = full_read(fd, buf, size);
	e = errno;
	close(fd);
	errno = e;
	return size;
}

ssize_t open_read_close(const char *filename, void *buf, size_t size)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return fd;
	return read_close(fd, buf, size);
}

void *xmalloc_open_read_close(const char *filename, size_t *sizep)
{
	char *buf;
	size_t size = sizep ? *sizep : INT_MAX;
	int fd = xopen(filename, O_RDONLY);
	/* /proc/N/stat files report len 0 here */
	/* In order to make such files readable, we add small const */
	off_t len = xlseek(fd, 0, SEEK_END) + 256;
	xlseek(fd, 0, SEEK_SET);

	if (len > size)
		bb_error_msg_and_die("file '%s' is too big", filename);
	size = len;
	buf = xmalloc(size + 1);
	size = read_close(fd, buf, size);
	if ((ssize_t)size < 0)
    		bb_perror_msg_and_die("'%s'", filename);
	xrealloc(buf, size + 1);
	buf[size] = '\0';
	if (sizep) *sizep = size;
	return buf;
}
