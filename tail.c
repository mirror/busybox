/* vi: set sw=4 ts=4: */
#include "internal.h"

/* This file contains _two_ implementations of tail.  One is
 * a bit more full featured, but costs 6k.  The other (i.e. the
 * SIMPLE_TAIL one) is less capable, but is good enough for about
 * 99% of the things folks want to use tail for, and only costs 2k.
 */


#ifdef BB_FEATURE_SIMPLE_TAIL

/* tail -- output the last part of file(s)
   Copyright (C) 89, 90, 91, 95, 1996 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Original version by Paul Rubin <phr@ocf.berkeley.edu>.
   Extensions by David MacKenzie <djm@gnu.ai.mit.edu>.
   tail -f for multiple files by Ian Lance Taylor <ian@airs.com>.  

   Rewrote the option parser, removed locales support,
    and generally busyboxed, Erik Andersen <andersen@lineo.com>

   Removed superfluous options and associated code ("-c", "-n", "-q").
   Removed "tail -f" support for multiple files.
   Both changes by Friedrich Vedder <fwv@myrtle.lahn.de>.

 */


#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#define BB_DECLARE_EXTERN
#define bb_need_help
#include "messages.c"


#define XWRITE(fd, buffer, n_bytes)					\
  do {									\
      if (n_bytes > 0 && fwrite ((buffer), 1, (n_bytes), stdout) == 0)	\
	  errorMsg("write error");					\
  } while (0)

/* Number of items to tail.  */
#define DEFAULT_N_LINES 10

/* Size of atomic reads.  */
#ifndef BUFSIZ
#define BUFSIZ (512 * 8)
#endif

/* If nonzero, read from the end of one file until killed.  */
static int forever;

/* If nonzero, print filename headers.  */
static int print_headers;

const char tail_usage[] =
	"tail [OPTION] [FILE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nPrint last 10 lines of each FILE to standard output.\n"
	"With more than one FILE, precede each with a header giving the\n"
	"file name. With no FILE, or when FILE is -, read standard input.\n\n"
	"Options:\n"
	"\t-n NUM\t\tPrint last NUM lines instead of first 10\n"

	"\t-f\t\tOutput data as the file grows.  This version\n"
	"\t\t\tof 'tail -f' supports only one file at a time.\n"
#endif
	;


static void write_header(const char *filename)
{
	static int first_file = 1;

	printf("%s==> %s <==\n", (first_file ? "" : "\n"), filename);
	first_file = 0;
}

/* Print the last N_LINES lines from the end of file FD.
   Go backward through the file, reading `BUFSIZ' bytes at a time (except
   probably the first), until we hit the start of the file or have
   read NUMBER newlines.
   POS starts out as the length of the file (the offset of the last
   byte of the file + 1).
   Return 0 if successful, 1 if an error occurred.  */

static int
file_lines(const char *filename, int fd, long int n_lines, off_t pos)
{
	char buffer[BUFSIZ];
	int bytes_read;
	int i;						/* Index into `buffer' for scanning.  */

	if (n_lines == 0)
		return 0;

	/* Set `bytes_read' to the size of the last, probably partial, buffer;
	   0 < `bytes_read' <= `BUFSIZ'.  */
	bytes_read = pos % BUFSIZ;
	if (bytes_read == 0)
		bytes_read = BUFSIZ;
	/* Make `pos' a multiple of `BUFSIZ' (0 if the file is short), so that all
	   reads will be on block boundaries, which might increase efficiency.  */
	pos -= bytes_read;
	lseek(fd, pos, SEEK_SET);
	bytes_read = fullRead(fd, buffer, bytes_read);
	if (bytes_read == -1)
		errorMsg("read error");

	/* Count the incomplete line on files that don't end with a newline.  */
	if (bytes_read && buffer[bytes_read - 1] != '\n')
		--n_lines;

	do {
		/* Scan backward, counting the newlines in this bufferfull.  */
		for (i = bytes_read - 1; i >= 0; i--) {
			/* Have we counted the requested number of newlines yet?  */
			if (buffer[i] == '\n' && n_lines-- == 0) {
				/* If this newline wasn't the last character in the buffer,
				   print the text after it.  */
				if (i != bytes_read - 1)
					XWRITE(STDOUT_FILENO, &buffer[i + 1],
						   bytes_read - (i + 1));
				return 0;
			}
		}
		/* Not enough newlines in that bufferfull.  */
		if (pos == 0) {
			/* Not enough lines in the file; print the entire file.  */
			lseek(fd, (off_t) 0, SEEK_SET);
			return 0;
		}
		pos -= BUFSIZ;
		lseek(fd, pos, SEEK_SET);
	}
	while ((bytes_read = fullRead(fd, buffer, BUFSIZ)) > 0);
	if (bytes_read == -1)
		errorMsg("read error");

	return 0;
}

/* Print the last N_LINES lines from the end of the standard input,
   open for reading as pipe FD.
   Buffer the text as a linked list of LBUFFERs, adding them as needed.
   Return 0 if successful, 1 if an error occured.  */

static int pipe_lines(const char *filename, int fd, long int n_lines)
{
	struct linebuffer {
		int nbytes, nlines;
		char buffer[BUFSIZ];
		struct linebuffer *next;
	};
	typedef struct linebuffer LBUFFER;
	LBUFFER *first, *last, *tmp;
	int i;						/* Index into buffers.  */
	int total_lines = 0;		/* Total number of newlines in all buffers.  */
	int errors = 0;

	first = last = (LBUFFER *) xmalloc(sizeof(LBUFFER));
	first->nbytes = first->nlines = 0;
	first->next = NULL;
	tmp = (LBUFFER *) xmalloc(sizeof(LBUFFER));

	/* Input is always read into a fresh buffer.  */
	while ((tmp->nbytes = fullRead(fd, tmp->buffer, BUFSIZ)) > 0) {
		tmp->nlines = 0;
		tmp->next = NULL;

		/* Count the number of newlines just read.  */
		for (i = 0; i < tmp->nbytes; i++)
			if (tmp->buffer[i] == '\n')
				++tmp->nlines;
		total_lines += tmp->nlines;

		/* If there is enough room in the last buffer read, just append the new
		   one to it.  This is because when reading from a pipe, `nbytes' can
		   often be very small.  */
		if (tmp->nbytes + last->nbytes < BUFSIZ) {
			memcpy(&last->buffer[last->nbytes], tmp->buffer, tmp->nbytes);
			last->nbytes += tmp->nbytes;
			last->nlines += tmp->nlines;
		} else {
			/* If there's not enough room, link the new buffer onto the end of
			   the list, then either free up the oldest buffer for the next
			   read if that would leave enough lines, or else malloc a new one.
			   Some compaction mechanism is possible but probably not
			   worthwhile.  */
			last = last->next = tmp;
			if (total_lines - first->nlines > n_lines) {
				tmp = first;
				total_lines -= first->nlines;
				first = first->next;
			} else
				tmp = (LBUFFER *) xmalloc(sizeof(LBUFFER));
		}
	}
	if (tmp->nbytes == -1)
		errorMsg("read error");

	free((char *) tmp);

	/* This prevents a core dump when the pipe contains no newlines.  */
	if (n_lines == 0)
		goto free_lbuffers;

	/* Count the incomplete line on files that don't end with a newline.  */
	if (last->buffer[last->nbytes - 1] != '\n') {
		++last->nlines;
		++total_lines;
	}

	/* Run through the list, printing lines.  First, skip over unneeded
	   buffers.  */
	for (tmp = first; total_lines - tmp->nlines > n_lines; tmp = tmp->next)
		total_lines -= tmp->nlines;

	/* Find the correct beginning, then print the rest of the file.  */
	if (total_lines > n_lines) {
		char *cp;

		/* Skip `total_lines' - `n_lines' newlines.  We made sure that
		   `total_lines' - `n_lines' <= `tmp->nlines'.  */
		cp = tmp->buffer;
		for (i = total_lines - n_lines; i; --i)
			while (*cp++ != '\n')
				/* Do nothing.  */ ;
		i = cp - tmp->buffer;
	} else
		i = 0;
	XWRITE(STDOUT_FILENO, &tmp->buffer[i], tmp->nbytes - i);

	for (tmp = tmp->next; tmp; tmp = tmp->next)
		XWRITE(STDOUT_FILENO, tmp->buffer, tmp->nbytes);

  free_lbuffers:
	while (first) {
		tmp = first->next;
		free((char *) first);
		first = tmp;
	}
	return errors;
}

/* Display file FILENAME from the current position in FD to the end.
   If `forever' is nonzero, keep reading from the end of the file
   until killed.  Return the number of bytes read from the file.  */

static long dump_remainder(const char *filename, int fd)
{
	char buffer[BUFSIZ];
	int bytes_read;
	long total;

	total = 0;
  output:
	while ((bytes_read = fullRead(fd, buffer, BUFSIZ)) > 0) {
		XWRITE(STDOUT_FILENO, buffer, bytes_read);
		total += bytes_read;
	}
	if (bytes_read == -1)
		errorMsg("read error");
	if (forever) {
		fflush(stdout);
		sleep(1);
		goto output;
	}

	return total;
}

/* Output the last N_LINES lines of file FILENAME open for reading in FD.
   Return 0 if successful, 1 if an error occurred.  */

static int tail_lines(const char *filename, int fd, long int n_lines)
{
	struct stat stats;
	off_t length;

	if (print_headers)
		write_header(filename);

	if (fstat(fd, &stats))
		errorMsg("fstat error");

	/* Use file_lines only if FD refers to a regular file with
	   its file pointer positioned at beginning of file.  */
	/* FIXME: adding the lseek conjunct is a kludge.
	   Once there's a reasonable test suite, fix the true culprit:
	   file_lines.  file_lines shouldn't presume that the input
	   file pointer is initially positioned to beginning of file.  */
	if (S_ISREG(stats.st_mode)
		&& lseek(fd, (off_t) 0, SEEK_CUR) == (off_t) 0) {
		length = lseek(fd, (off_t) 0, SEEK_END);
		if (length != 0 && file_lines(filename, fd, n_lines, length))
			return 1;
		dump_remainder(filename, fd);
	} else
		return pipe_lines(filename, fd, n_lines);

	return 0;
}

/* Display the last N_UNITS lines of file FILENAME.
   "-" for FILENAME means the standard input.
   Return 0 if successful, 1 if an error occurred.  */

static int tail_file(const char *filename, off_t n_units)
{
	int fd, errors;

	if (!strcmp(filename, "-")) {
		filename = "standard input";
		errors = tail_lines(filename, 0, (long) n_units);
	} else {
		/* Not standard input.  */
		fd = open(filename, O_RDONLY);
		if (fd == -1)
			fatalError("open error");

		errors = tail_lines(filename, fd, (long) n_units);
		close(fd);
	}

	return errors;
}

extern int tail_main(int argc, char **argv)
{
	int exit_status = 0;
	int n_units = DEFAULT_N_LINES;
	int n_tmp, i;
	char opt;

	forever = print_headers = 0;

	/* parse argv[] */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			opt = argv[i][1];
			switch (opt) {
			case 'f':
				forever = 1;
				break;
			case 'n':
				n_tmp = 0;
				if (++i < argc)
					n_tmp = atoi(argv[i]);
				if (n_tmp < 1)
					usage(tail_usage);
				n_units = n_tmp;
				break;
			case '-':
			case 'h':
				usage(tail_usage);
			default:
				if ((n_units = atoi(&argv[i][1])) < 1) {
					fprintf(stderr, "tail: invalid option -- %c\n", opt);
					usage(tail_usage);
				}
			}
		} else {
			break;
		}
	}

	if (i + 1 < argc) {
		if (forever) {
			fprintf(stderr,
					"tail: option -f is invalid with multiple files\n");
			usage(tail_usage);
		}
		print_headers = 1;
	}

	if (i >= argc) {
		exit_status |= tail_file("-", n_units);
	} else {
		for (; i < argc; i++)
			exit_status |= tail_file(argv[i], n_units);
	}

	exit(exit_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}


#else
// Here follows the code for the full featured tail code


/* tail -- output the last part of file(s)
   Copyright (C) 89, 90, 91, 95, 1996 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Original version by Paul Rubin <phr@ocf.berkeley.edu>.
   Extensions by David MacKenzie <djm@gnu.ai.mit.edu>.
   tail -f for multiple files by Ian Lance Taylor <ian@airs.com>.  

   Rewrote the option parser, removed locales support,
    and generally busyboxed, Erik Andersen <andersen@lineo.com>
 
 */

#include "internal.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>



/* Disable assertions.  Some systems have broken assert macros.  */
#define NDEBUG 1


static void detailed_error(int i, int errnum, char *fmt, ...)
			  __attribute__ ((format (printf, 3, 4)));
static void detailed_error(int i, int errnum, char *fmt, ...)
{
	va_list arguments;

	va_start(arguments, fmt);
	vfprintf(stderr, fmt, arguments);
	fprintf(stderr, "\n%s\n", strerror(errnum));
	va_end(arguments);
	exit(i);
}


#define XWRITE(fd, buffer, n_bytes)					\
  do									\
    {									\
      assert ((fd) == 1);						\
      assert ((n_bytes) >= 0);						\
      if (n_bytes > 0 && fwrite ((buffer), 1, (n_bytes), stdout) == 0)	\
	detailed_error (EXIT_FAILURE, errno, "write error");			\
    }									\
  while (0)

/* Number of items to tail.  */
#define DEFAULT_N_LINES 10

/* Size of atomic reads.  */
#ifndef BUFSIZ
#define BUFSIZ (512 * 8)
#endif

/* If nonzero, interpret the numeric argument as the number of lines.
   Otherwise, interpret it as the number of bytes.  */
static int count_lines;

/* If nonzero, read from the end of one file until killed.  */
static int forever;

/* If nonzero, read from the end of multiple files until killed.  */
static int forever_multiple;

/* Array of file descriptors if forever_multiple is 1.  */
static int *file_descs;

/* Array of file sizes if forever_multiple is 1.  */
static off_t *file_sizes;

/* If nonzero, count from start of file instead of end.  */
static int from_start;

/* If nonzero, print filename headers.  */
static int print_headers;

/* When to print the filename banners.  */
enum header_mode {
	multiple_files, always, never
};

/* The name this program was run with.  */
char *program_name;

/* Nonzero if we have ever read standard input.  */
static int have_read_stdin;


static const char tail_usage[] = "tail [OPTION]... [FILE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
"\nPrint last 10 lines of each FILE to standard output.\n\
With more than one FILE, precede each with a header giving the file name.\n\
With no FILE, or when FILE is -, read standard input.\n\
\n\
  -c=N[kbm]       output the last N bytes\n\
  -f              output appended data as the file grows\n\
  -n=N            output the last N lines, instead of last 10\n\
  -q              never output headers giving file names\n\
  -v              always output headers giving file names\n\
\n\
If the first character of N (bytes or lines) is a `+', output begins with \n\
the Nth item from the start of each file, otherwise, print the last N items\n\
in the file.  N bytes may be suffixed by k (x1024), b (x512), or m (1024^2).\n"
#endif
;

static void write_header(const char *filename, const char *comment)
{
	static int first_file = 1;

	printf("%s==> %s%s%s <==\n", (first_file ? "" : "\n"), filename,
		   (comment ? ": " : ""), (comment ? comment : ""));
	first_file = 0;
}

/* Print the last N_LINES lines from the end of file FD.
   Go backward through the file, reading `BUFSIZ' bytes at a time (except
   probably the first), until we hit the start of the file or have
   read NUMBER newlines.
   POS starts out as the length of the file (the offset of the last
   byte of the file + 1).
   Return 0 if successful, 1 if an error occurred.  */

static int
file_lines(const char *filename, int fd, long int n_lines, off_t pos)
{
	char buffer[BUFSIZ];
	int bytes_read;
	int i;						/* Index into `buffer' for scanning.  */

	if (n_lines == 0)
		return 0;

	/* Set `bytes_read' to the size of the last, probably partial, buffer;
	   0 < `bytes_read' <= `BUFSIZ'.  */
	bytes_read = pos % BUFSIZ;
	if (bytes_read == 0)
		bytes_read = BUFSIZ;
	/* Make `pos' a multiple of `BUFSIZ' (0 if the file is short), so that all
	   reads will be on block boundaries, which might increase efficiency.  */
	pos -= bytes_read;
	lseek(fd, pos, SEEK_SET);
	bytes_read = fullRead(fd, buffer, bytes_read);
	if (bytes_read == -1) {
		detailed_error(0, errno, "%s", filename);
		return 1;
	}

	/* Count the incomplete line on files that don't end with a newline.  */
	if (bytes_read && buffer[bytes_read - 1] != '\n')
		--n_lines;

	do {
		/* Scan backward, counting the newlines in this bufferfull.  */
		for (i = bytes_read - 1; i >= 0; i--) {
			/* Have we counted the requested number of newlines yet?  */
			if (buffer[i] == '\n' && n_lines-- == 0) {
				/* If this newline wasn't the last character in the buffer,
				   print the text after it.  */
				if (i != bytes_read - 1)
					XWRITE(STDOUT_FILENO, &buffer[i + 1],
						   bytes_read - (i + 1));
				return 0;
			}
		}
		/* Not enough newlines in that bufferfull.  */
		if (pos == 0) {
			/* Not enough lines in the file; print the entire file.  */
			lseek(fd, (off_t) 0, SEEK_SET);
			return 0;
		}
		pos -= BUFSIZ;
		lseek(fd, pos, SEEK_SET);
	}
	while ((bytes_read = fullRead(fd, buffer, BUFSIZ)) > 0);
	if (bytes_read == -1) {
		detailed_error(0, errno, "%s", filename);
		return 1;
	}
	return 0;
}

/* Print the last N_LINES lines from the end of the standard input,
   open for reading as pipe FD.
   Buffer the text as a linked list of LBUFFERs, adding them as needed.
   Return 0 if successful, 1 if an error occured.  */

static int pipe_lines(const char *filename, int fd, long int n_lines)
{
	struct linebuffer {
		int nbytes, nlines;
		char buffer[BUFSIZ];
		struct linebuffer *next;
	};
	typedef struct linebuffer LBUFFER;
	LBUFFER *first, *last, *tmp;
	int i;						/* Index into buffers.  */
	int total_lines = 0;		/* Total number of newlines in all buffers.  */
	int errors = 0;

	first = last = (LBUFFER *) xmalloc(sizeof(LBUFFER));
	first->nbytes = first->nlines = 0;
	first->next = NULL;
	tmp = (LBUFFER *) xmalloc(sizeof(LBUFFER));

	/* Input is always read into a fresh buffer.  */
	while ((tmp->nbytes = fullRead(fd, tmp->buffer, BUFSIZ)) > 0) {
		tmp->nlines = 0;
		tmp->next = NULL;

		/* Count the number of newlines just read.  */
		for (i = 0; i < tmp->nbytes; i++)
			if (tmp->buffer[i] == '\n')
				++tmp->nlines;
		total_lines += tmp->nlines;

		/* If there is enough room in the last buffer read, just append the new
		   one to it.  This is because when reading from a pipe, `nbytes' can
		   often be very small.  */
		if (tmp->nbytes + last->nbytes < BUFSIZ) {
			memcpy(&last->buffer[last->nbytes], tmp->buffer, tmp->nbytes);
			last->nbytes += tmp->nbytes;
			last->nlines += tmp->nlines;
		} else {
			/* If there's not enough room, link the new buffer onto the end of
			   the list, then either free up the oldest buffer for the next
			   read if that would leave enough lines, or else malloc a new one.
			   Some compaction mechanism is possible but probably not
			   worthwhile.  */
			last = last->next = tmp;
			if (total_lines - first->nlines > n_lines) {
				tmp = first;
				total_lines -= first->nlines;
				first = first->next;
			} else
				tmp = (LBUFFER *) xmalloc(sizeof(LBUFFER));
		}
	}
	if (tmp->nbytes == -1) {
		detailed_error(0, errno, "%s", filename);
		errors = 1;
		free((char *) tmp);
		goto free_lbuffers;
	}

	free((char *) tmp);

	/* This prevents a core dump when the pipe contains no newlines.  */
	if (n_lines == 0)
		goto free_lbuffers;

	/* Count the incomplete line on files that don't end with a newline.  */
	if (last->buffer[last->nbytes - 1] != '\n') {
		++last->nlines;
		++total_lines;
	}

	/* Run through the list, printing lines.  First, skip over unneeded
	   buffers.  */
	for (tmp = first; total_lines - tmp->nlines > n_lines; tmp = tmp->next)
		total_lines -= tmp->nlines;

	/* Find the correct beginning, then print the rest of the file.  */
	if (total_lines > n_lines) {
		char *cp;

		/* Skip `total_lines' - `n_lines' newlines.  We made sure that
		   `total_lines' - `n_lines' <= `tmp->nlines'.  */
		cp = tmp->buffer;
		for (i = total_lines - n_lines; i; --i)
			while (*cp++ != '\n')
				/* Do nothing.  */ ;
		i = cp - tmp->buffer;
	} else
		i = 0;
	XWRITE(STDOUT_FILENO, &tmp->buffer[i], tmp->nbytes - i);

	for (tmp = tmp->next; tmp; tmp = tmp->next)
		XWRITE(STDOUT_FILENO, tmp->buffer, tmp->nbytes);

  free_lbuffers:
	while (first) {
		tmp = first->next;
		free((char *) first);
		first = tmp;
	}
	return errors;
}

/* Print the last N_BYTES characters from the end of pipe FD.
   This is a stripped down version of pipe_lines.
   Return 0 if successful, 1 if an error occurred.  */

static int pipe_bytes(const char *filename, int fd, off_t n_bytes)
{
	struct charbuffer {
		int nbytes;
		char buffer[BUFSIZ];
		struct charbuffer *next;
	};
	typedef struct charbuffer CBUFFER;
	CBUFFER *first, *last, *tmp;
	int i;						/* Index into buffers.  */
	int total_bytes = 0;		/* Total characters in all buffers.  */
	int errors = 0;

	first = last = (CBUFFER *) xmalloc(sizeof(CBUFFER));
	first->nbytes = 0;
	first->next = NULL;
	tmp = (CBUFFER *) xmalloc(sizeof(CBUFFER));

	/* Input is always read into a fresh buffer.  */
	while ((tmp->nbytes = fullRead(fd, tmp->buffer, BUFSIZ)) > 0) {
		tmp->next = NULL;

		total_bytes += tmp->nbytes;
		/* If there is enough room in the last buffer read, just append the new
		   one to it.  This is because when reading from a pipe, `nbytes' can
		   often be very small.  */
		if (tmp->nbytes + last->nbytes < BUFSIZ) {
			memcpy(&last->buffer[last->nbytes], tmp->buffer, tmp->nbytes);
			last->nbytes += tmp->nbytes;
		} else {
			/* If there's not enough room, link the new buffer onto the end of
			   the list, then either free up the oldest buffer for the next
			   read if that would leave enough characters, or else malloc a new
			   one.  Some compaction mechanism is possible but probably not
			   worthwhile.  */
			last = last->next = tmp;
			if (total_bytes - first->nbytes > n_bytes) {
				tmp = first;
				total_bytes -= first->nbytes;
				first = first->next;
			} else {
				tmp = (CBUFFER *) xmalloc(sizeof(CBUFFER));
			}
		}
	}
	if (tmp->nbytes == -1) {
		detailed_error(0, errno, "%s", filename);
		errors = 1;
		free((char *) tmp);
		goto free_cbuffers;
	}

	free((char *) tmp);

	/* Run through the list, printing characters.  First, skip over unneeded
	   buffers.  */
	for (tmp = first; total_bytes - tmp->nbytes > n_bytes; tmp = tmp->next)
		total_bytes -= tmp->nbytes;

	/* Find the correct beginning, then print the rest of the file.
	   We made sure that `total_bytes' - `n_bytes' <= `tmp->nbytes'.  */
	if (total_bytes > n_bytes)
		i = total_bytes - n_bytes;
	else
		i = 0;
	XWRITE(STDOUT_FILENO, &tmp->buffer[i], tmp->nbytes - i);

	for (tmp = tmp->next; tmp; tmp = tmp->next)
		XWRITE(STDOUT_FILENO, tmp->buffer, tmp->nbytes);

  free_cbuffers:
	while (first) {
		tmp = first->next;
		free((char *) first);
		first = tmp;
	}
	return errors;
}

/* Skip N_BYTES characters from the start of pipe FD, and print
   any extra characters that were read beyond that.
   Return 1 on error, 0 if ok.  */

static int start_bytes(const char *filename, int fd, off_t n_bytes)
{
	char buffer[BUFSIZ];
	int bytes_read = 0;

	while (n_bytes > 0 && (bytes_read = fullRead(fd, buffer, BUFSIZ)) > 0)
		n_bytes -= bytes_read;
	if (bytes_read == -1) {
		detailed_error(0, errno, "%s", filename);
		return 1;
	} else if (n_bytes < 0)
		XWRITE(STDOUT_FILENO, &buffer[bytes_read + n_bytes], -n_bytes);
	return 0;
}

/* Skip N_LINES lines at the start of file or pipe FD, and print
   any extra characters that were read beyond that.
   Return 1 on error, 0 if ok.  */

static int start_lines(const char *filename, int fd, long int n_lines)
{
	char buffer[BUFSIZ];
	int bytes_read = 0;
	int bytes_to_skip = 0;

	while (n_lines && (bytes_read = fullRead(fd, buffer, BUFSIZ)) > 0) {
		bytes_to_skip = 0;
		while (bytes_to_skip < bytes_read)
			if (buffer[bytes_to_skip++] == '\n' && --n_lines == 0)
				break;
	}
	if (bytes_read == -1) {
		detailed_error(0, errno, "%s", filename);
		return 1;
	} else if (bytes_to_skip < bytes_read) {
		XWRITE(STDOUT_FILENO, &buffer[bytes_to_skip],
			   bytes_read - bytes_to_skip);
	}
	return 0;
}

/* Display file FILENAME from the current position in FD to the end.
   If `forever' is nonzero, keep reading from the end of the file
   until killed.  Return the number of bytes read from the file.  */

static long dump_remainder(const char *filename, int fd)
{
	char buffer[BUFSIZ];
	int bytes_read;
	long total;

	total = 0;
  output:
	while ((bytes_read = fullRead(fd, buffer, BUFSIZ)) > 0) {
		XWRITE(STDOUT_FILENO, buffer, bytes_read);
		total += bytes_read;
	}
	if (bytes_read == -1)
		detailed_error(EXIT_FAILURE, errno, "%s", filename);
	if (forever) {
		fflush(stdout);
		sleep(1);
		goto output;
	} else {
		if (forever_multiple)
			fflush(stdout);
	}

	return total;
}

/* Tail NFILES (>1) files forever until killed.  The file names are in
   NAMES.  The open file descriptors are in `file_descs', and the size
   at which we stopped tailing them is in `file_sizes'.  We loop over
   each of them, doing an fstat to see if they have changed size.  If
   none of them have changed size in one iteration, we sleep for a
   second and try again.  We do this until the user interrupts us.  */

static void tail_forever(char **names, int nfiles)
{
	int last;

	last = -1;

	while (1) {
		int i;
		int changed;

		changed = 0;
		for (i = 0; i < nfiles; i++) {
			struct stat stats;

			if (file_descs[i] < 0)
				continue;
			if (fstat(file_descs[i], &stats) < 0) {
				detailed_error(0, errno, "%s", names[i]);
				file_descs[i] = -1;
				continue;
			}
			if (stats.st_size == file_sizes[i])
				continue;

			/* This file has changed size.  Print out what we can, and
			   then keep looping.  */

			changed = 1;

			if (stats.st_size < file_sizes[i]) {
				write_header(names[i], "file truncated");
				last = i;
				lseek(file_descs[i], stats.st_size, SEEK_SET);
				file_sizes[i] = stats.st_size;
				continue;
			}

			if (i != last) {
				if (print_headers)
					write_header(names[i], NULL);
				last = i;
			}
			file_sizes[i] += dump_remainder(names[i], file_descs[i]);
		}

		/* If none of the files changed size, sleep.  */
		if (!changed)
			sleep(1);
	}
}

/* Output the last N_BYTES bytes of file FILENAME open for reading in FD.
   Return 0 if successful, 1 if an error occurred.  */

static int tail_bytes(const char *filename, int fd, off_t n_bytes)
{
	struct stat stats;

	/* FIXME: resolve this like in dd.c.  */
	/* Use fstat instead of checking for errno == ESPIPE because
	   lseek doesn't work on some special files but doesn't return an
	   error, either.  */
	if (fstat(fd, &stats)) {
		detailed_error(0, errno, "%s", filename);
		return 1;
	}

	if (from_start) {
		if (S_ISREG(stats.st_mode))
			lseek(fd, n_bytes, SEEK_CUR);
		else if (start_bytes(filename, fd, n_bytes))
			return 1;
		dump_remainder(filename, fd);
	} else {
		if (S_ISREG(stats.st_mode)) {
			off_t current_pos, end_pos;
			size_t bytes_remaining;

			if ((current_pos = lseek(fd, (off_t) 0, SEEK_CUR)) != -1
				&& (end_pos = lseek(fd, (off_t) 0, SEEK_END)) != -1) {
				off_t diff;

				/* Be careful here.  The current position may actually be
				   beyond the end of the file.  */
				bytes_remaining = (diff =
								   end_pos - current_pos) < 0 ? 0 : diff;
			} else {
				detailed_error(0, errno, "%s", filename);
				return 1;
			}

			if (bytes_remaining <= n_bytes) {
				/* From the current position to end of file, there are no
				   more bytes than have been requested.  So reposition the
				   file pointer to the incoming current position and print
				   everything after that.  */
				lseek(fd, current_pos, SEEK_SET);
			} else {
				/* There are more bytes remaining than were requested.
				   Back up.  */
				lseek(fd, -n_bytes, SEEK_END);
			}
			dump_remainder(filename, fd);
		} else
			return pipe_bytes(filename, fd, n_bytes);
	}
	return 0;
}

/* Output the last N_LINES lines of file FILENAME open for reading in FD.
   Return 0 if successful, 1 if an error occurred.  */

static int tail_lines(const char *filename, int fd, long int n_lines)
{
	struct stat stats;
	off_t length;

	if (fstat(fd, &stats)) {
		detailed_error(0, errno, "%s", filename);
		return 1;
	}

	if (from_start) {
		if (start_lines(filename, fd, n_lines))
			return 1;
		dump_remainder(filename, fd);
	} else {
		/* Use file_lines only if FD refers to a regular file with
		   its file pointer positioned at beginning of file.  */
		/* FIXME: adding the lseek conjunct is a kludge.
		   Once there's a reasonable test suite, fix the true culprit:
		   file_lines.  file_lines shouldn't presume that the input
		   file pointer is initially positioned to beginning of file.  */
		if (S_ISREG(stats.st_mode)
			&& lseek(fd, (off_t) 0, SEEK_CUR) == (off_t) 0) {
			length = lseek(fd, (off_t) 0, SEEK_END);
			if (length != 0 && file_lines(filename, fd, n_lines, length))
				return 1;
			dump_remainder(filename, fd);
		} else
			return pipe_lines(filename, fd, n_lines);
	}
	return 0;
}

/* Display the last N_UNITS units of file FILENAME, open for reading
   in FD.
   Return 0 if successful, 1 if an error occurred.  */

static int tail(const char *filename, int fd, off_t n_units)
{
	if (count_lines)
		return tail_lines(filename, fd, (long) n_units);
	else
		return tail_bytes(filename, fd, n_units);
}

/* Display the last N_UNITS units of file FILENAME.
   "-" for FILENAME means the standard input.
   FILENUM is this file's index in the list of files the user gave.
   Return 0 if successful, 1 if an error occurred.  */

static int tail_file(const char *filename, off_t n_units, int filenum)
{
	int fd, errors;
	struct stat stats;

	if (!strcmp(filename, "-")) {
		have_read_stdin = 1;
		filename = "standard input";
		if (print_headers)
			write_header(filename, NULL);
		errors = tail(filename, 0, n_units);
		if (forever_multiple) {
			if (fstat(0, &stats) < 0) {
				detailed_error(0, errno, "standard input");
				errors = 1;
			} else if (!S_ISREG(stats.st_mode)) {
				detailed_error(0, 0,
							   "standard input: cannot follow end of non-regular file");
				errors = 1;
			}
			if (errors)
				file_descs[filenum] = -1;
			else {
				file_descs[filenum] = 0;
				file_sizes[filenum] = stats.st_size;
			}
		}
	} else {
		/* Not standard input.  */
		fd = open(filename, O_RDONLY);
		if (fd == -1) {
			if (forever_multiple)
				file_descs[filenum] = -1;
			detailed_error(0, errno, "%s", filename);
			errors = 1;
		} else {
			if (print_headers)
				write_header(filename, NULL);
			errors = tail(filename, fd, n_units);
			if (forever_multiple) {
				if (fstat(fd, &stats) < 0) {
					detailed_error(0, errno, "%s", filename);
					errors = 1;
				} else if (!S_ISREG(stats.st_mode)) {
					detailed_error(0, 0,
								   "%s: cannot follow end of non-regular file",
								   filename);
					errors = 1;
				}
				if (errors) {
					close(fd);
					file_descs[filenum] = -1;
				} else {
					file_descs[filenum] = fd;
					file_sizes[filenum] = stats.st_size;
				}
			} else {
				if (close(fd)) {
					detailed_error(0, errno, "%s", filename);
					errors = 1;
				}
			}
		}
	}

	return errors;
}

extern int tail_main(int argc, char **argv)
{
	int stopit = 0;
	enum header_mode header_mode = multiple_files;
	int exit_status = 0;

	/* If from_start, the number of items to skip before printing; otherwise,
	   the number of items at the end of the file to print.  Initially, -1
	   means the value has not been set.  */
	off_t n_units = -1;
	int n_files;
	char **file;

	program_name = argv[0];
	have_read_stdin = 0;
	count_lines = 1;
	forever = forever_multiple = from_start = print_headers = 0;

	/* Parse any options */
	//fprintf(stderr, "argc=%d, argv=%s\n", argc, *argv);
	while (--argc > 0 && (**(++argv) == '-' || **argv == '+')) {
		if (**argv == '+') {
			from_start = 1;
		}
		stopit = 0;
		while (stopit == 0 && *(++(*argv))) {
			switch (**argv) {
			case 'c':
				count_lines = 0;

				if (--argc < 1) {
					usage(tail_usage);
				}
				n_units = getNum(*(++argv));
				stopit = 1;
				break;

			case 'f':
				forever = 1;
				break;

			case 'n':
				count_lines = 1;

				if (--argc < 1) {
					usage(tail_usage);
				}
				n_units = atol(*(++argv));
				stopit = 1;
				break;

			case 'q':
				header_mode = never;
				break;

			case 'v':
				header_mode = always;
				break;

			default:
				usage(tail_usage);
			}
		}
	}


	if (n_units == -1)
		n_units = DEFAULT_N_LINES;

	/* To start printing with item N_UNITS from the start of the file, skip
	   N_UNITS - 1 items.  `tail +0' is actually meaningless, but for Unix
	   compatibility it's treated the same as `tail +1'.  */
	if (from_start) {
		if (n_units)
			--n_units;
	}

	n_files = argc;
	file = argv;

	if (n_files > 1 && forever) {
		forever_multiple = 1;
		forever = 0;
		file_descs = (int *) xmalloc(n_files * sizeof(int));

		file_sizes = (off_t *) xmalloc(n_files * sizeof(off_t));
	}

	if (header_mode == always
		|| (header_mode == multiple_files && n_files > 1))
		print_headers = 1;

	if (n_files == 0) {
		exit_status |= tail_file("-", n_units, 0);
	} else {
		int i;

		for (i = 0; i < n_files; i++)
			exit_status |= tail_file(file[i], n_units, i);

		if (forever_multiple)
			tail_forever(file, n_files);
	}

	if (have_read_stdin && close(0) < 0)
		detailed_error(EXIT_FAILURE, errno, "-");
	if (fclose(stdout) == EOF)
		detailed_error(EXIT_FAILURE, errno, "write error");
	exit(exit_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}


#endif
