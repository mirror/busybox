/* vi: set sw=4 ts=4: */
/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 * to support files as well as stdin/stdout, and to generally behave itself wrt
 * command line handling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *
 * gzip (GNU zip) -- compress files with zip algorithm and 'compress' interface
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * The unzip code was written and put in the public domain by Mark Adler.
 * Portions of the lzw code are derived from the public domain 'compress'
 * written by Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
 *
 * See the license_msg below and the file COPYING for the software license.
 * See the file algorithm.doc for the compression algorithms and file formats.
 */

#if 0
static char *license_msg[] = {
	"   Copyright (C) 1992-1993 Jean-loup Gailly",
	"   This program is free software; you can redistribute it and/or modify",
	"   it under the terms of the GNU General Public License as published by",
	"   the Free Software Foundation; either version 2, or (at your option)",
	"   any later version.",
	"",
	"   This program is distributed in the hope that it will be useful,",
	"   but WITHOUT ANY WARRANTY; without even the implied warranty of",
	"   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
	"   GNU General Public License for more details.",
	"",
	"   You should have received a copy of the GNU General Public License",
	"   along with this program; if not, write to the Free Software",
	"   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.",
	0
};
#endif

#include "busybox.h"
#include <getopt.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#define BB_DECLARE_EXTERN
#define bb_need_memory_exhausted
#define bb_need_name_too_long
#include "messages.c"

static const int RECORD_IO = 0;

/* Return codes from gzip */
static const int OK = 0;
static const int ERROR = 1;
static const int WARNING = 2;

static const int DEFLATED = 8;
static const int INBUFSIZ = 0x2000;	/* input buffer size */
static const int INBUF_EXTRA = 64;		/* required by unlzw() */
static const int OUTBUFSIZ = 8192;	/* output buffer size */
static const int OUTBUF_EXTRA = 2048;	/* required by unlzw() */
static const int DIST_BUFSIZE = 0x2000;	/* buffer for distances, see trees.c */

#define	GZIP_MAGIC  "\037\213"	/* Magic header for gzip files, 1F 8B */

/* gzip flag byte */
static const int EXTRA_FIELD = 0x04;	/* bit 2 set: extra field present */
static const int ORIG_NAME = 0x08;	/* bit 3 set: original file name present */
static const int COMMENT = 0x10;	/* bit 4 set: file comment present */
static const int WSIZE = 0x8000;		/* window size--must be a power of two, and */
				/*  at least 32K for zip's deflate method */

/* If BMAX needs to be larger than 16, then h and x[] should be ulg. */
static const int BMAX = 16;		/* maximum bit length of any code (16 for explode) */
static const int N_MAX = 288;		/* maximum number of codes in any set */

/* PKZIP header definitions */
static const int LOCSIG = 0x04034b50L;	/* four-byte lead-in (lsb first) */
static const int LOCCRC = 14;		/* offset of crc */
static const int LOCLEN = 22;		/* offset of uncompressed length */
static const int EXTHDR = 16;		/* size of extended local header, inc sig */

static const int BITS = 16;

/* Diagnostic functions */
#ifdef DEBUG
#  define Assert(cond,msg) {if(!(cond)) error_msg(msg);}
#  define Trace(x) fprintf x
#  define Tracev(x) {if (verbose) fprintf x ;}
#  define Tracevv(x) {if (verbose>1) fprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (verbose>1 && (c)) fprintf x ;}
#else
#  define Assert(cond,msg)
#  define Trace(x)
#  define Tracev(x)
#  define Tracevv(x)
#  define Tracec(c,x)
#  define Tracecv(c,x)
#endif

#ifndef MAX_PATH_LEN			/* max pathname length */
#  ifdef BUFSIZ
#    define MAX_PATH_LEN   BUFSIZ
#  else
static const int MAX_PATH_LEN = 1024;
#  endif
#endif

#define NEXTBYTE()  (uch)get_byte()
#define NEEDBITS(n) {while(k<(n)){b|=((ulg)NEXTBYTE())<<k;k+=8;}}
#define DUMPBITS(n) {b>>=(n);k-=(n);}

#define get_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf(0))

/* Macros for getting two-byte and four-byte header values */
#define SH(p) ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8))
#define LG(p) ((ulg)(SH(p)) | ((ulg)(SH((p)+2)) << 16))

	/* in gzip.c */
void abort_gzip (void);
typedef void (*sig_type) (int);

typedef unsigned char uch;
typedef unsigned short ush;
typedef unsigned long ulg;
typedef int file_t;				/* Do not use stdio */

static uch *inbuf;
static uch *outbuf;
static ush *d_buf;
static uch *window;
static ush *tab_prefix0;
static ush *tab_prefix1;

/* local variables */
static int test_mode = 0;	/* check file integrity option */
static int foreground;		/* set if program run in foreground */
static int method;	/* compression method */
static int exit_code;	/* program exit code */
static int last_member;	/* set for .zip and .Z files */
static int part_nb;		/* number of parts in .gz file */
static long ifile_size;	/* input file size, -1 for devices (debug only) */
static long bytes_in;		/* number of input bytes */
static long bytes_out;		/* number of output bytes */
static int ifd;		/* input file descriptor */
static int ofd;		/* output file descriptor */
static unsigned insize;	/* valid bytes in inbuf */
static unsigned inptr;		/* index of next byte to be processed in inbuf */
static unsigned outcnt;	/* bytes in output buffer */

unsigned hufts;		/* track memory usage */
ulg bb;			/* bit buffer */
unsigned bk;		/* bits in bit buffer */
int crc_table_empty = 1;

struct huft {
	uch e;		/* number of extra bits or operation */
	uch b;		/* number of bits in this code or subcode */
	union {
		ush n;		/* literal, length base, or distance base */
		struct huft *t;	/* pointer to next level of table */
	} v;
};

/* Tables for deflate from PKZIP's appnote.txt. */
static unsigned border[] = {	/* Order of the bit length code lengths */
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};
static ush cplens[] = {		/* Copy lengths for literal codes 257..285 */
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

/* note: see note #13 above about the 258 in this list. */
static ush cplext[] = {		/* Extra bits for literal codes 257..285 */
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99
};				/* 99==invalid */

static ush cpdist[] = {		/* Copy offsets for distance codes 0..29 */
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
	257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
	8193, 12289, 16385, 24577
};

static ush cpdext[] = {		/* Extra bits for distance codes */
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
	7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
	12, 12, 13, 13
};

ush mask_bits[] = {
	0x0000,
	0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
	0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

/* ========================================================================
 * Error handlers.
 */
void read_error_msg()
{
	fprintf(stderr, "\n");
	if (errno != 0) {
		perror("");
	} else {
		fprintf(stderr, "unexpected end of file\n");
	}
	abort_gzip();
}

/* ===========================================================================
 * Fill the input buffer. This is called only when the buffer is empty.
 */
int fill_inbuf(eof_ok)
int eof_ok;		/* set if EOF acceptable as a result */
{
	int len;

	/* Read as much as possible */
	insize = 0;
	errno = 0;
	do {
		len = read(ifd, (char *) inbuf + insize, INBUFSIZ - insize);
		if (len == 0 || len == EOF)
			break;
		insize += len;
	} while (insize < INBUFSIZ);

	if (insize == 0) {
		if (eof_ok)
			return EOF;
		read_error_msg();
	}
	bytes_in += (ulg) insize;
	inptr = 1;
	return inbuf[0];
}


/* ========================================================================
 * Check the magic number of the input file and update ofname if an
 * original name was given and tostdout is not set.
 * Return the compression method, -1 for error, -2 for warning.
 * Set inptr to the offset of the next byte to be processed.
 * Updates time_stamp if there is one and --no-time is not used.
 * This function may be called repeatedly for an input file consisting
 * of several contiguous gzip'ed members.
 * IN assertions: there is at least one remaining compressed member.
 *   If the member is a zip file, it must be the only one.
 */
static int get_method(in)
int in;					/* input file descriptor */
{
	uch flags;			/* compression flags */
	char magic[2];			/* magic header */
	long header_bytes = 0;		/* number of bytes in gzip header */

	magic[0] = (char) get_byte();
	magic[1] = (char) get_byte();
	method = -1;			/* unknown yet */
	part_nb++;			/* number of parts in gzip file */
	last_member = RECORD_IO;
	/* assume multiple members in gzip file except for record oriented I/O */

	if (memcmp(magic, GZIP_MAGIC, 2) == 0) {

		method = (int) get_byte();
		if (method != DEFLATED) {
			error_msg("unknown method %d -- get newer version of gzip", method);
			exit_code = ERROR;
			return -1;
		}
		flags = (uch) get_byte();

		(ulg) get_byte();		/* Ignore time stamp */
		(ulg) get_byte();
		(ulg) get_byte();
		(ulg) get_byte();

		(void) get_byte();		/* Ignore extra flags for the moment */
		(void) get_byte();		/* Ignore OS type for the moment */

		if ((flags & EXTRA_FIELD) != 0) {
			unsigned len = (unsigned) get_byte();
			len |= ((unsigned) get_byte()) << 8;
			while (len--)
				(void) get_byte();
		}

		/* Discard original name if any */
		if ((flags & ORIG_NAME) != 0) {
			while (get_byte() != 0);	/* null */
		}

		/* Discard file comment if any */
		if ((flags & COMMENT) != 0) {
			while (get_byte() != 0)	/* null */
				;
		}
		if (part_nb == 1) {
			header_bytes = inptr + 2 * sizeof(long);	/* include crc and size */
		}

	}

	if (method >= 0)
		return method;

	if (part_nb == 1) {
		fprintf(stderr, "\nnot in gzip format\n");
		exit_code = ERROR;
		return -1;
	} else {
		fprintf(stderr, "\ndecompression OK, trailing garbage ignored\n");
		if (exit_code == OK)
			exit_code = WARNING;
		return -2;
	}
}

/* ========================================================================
 * Signal and error handler.
 */
void abort_gzip()
{
	exit(ERROR);
}

/* ===========================================================================
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
ulg updcrc(s, n)
uch *s;					/* pointer to bytes to pump through */
unsigned n;				/* number of bytes in s[] */
{
	static ulg crc = (ulg) 0xffffffffL;	/* shift register contents */
	register ulg c;				/* temporary variable */
	static unsigned long crc_32_tab[256];
	if (crc_table_empty) {
		unsigned long c;      /* crc shift register */
		unsigned long e;      /* polynomial exclusive-or pattern */
		int i;                /* counter for all possible eight bit values */
		int k;                /* byte being shifted into crc apparatus */

		/* terms of polynomial defining this crc (except x^32): */
		static int p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

		/* Make exclusive-or pattern from polynomial (0xedb88320) */
		e = 0;
		for (i = 0; i < sizeof(p)/sizeof(int); i++)
			e |= 1L << (31 - p[i]);

		/* Compute and print table of CRC's, five per line */
		crc_32_tab[0] = 0x00000000L;
		for (i = 1; i < 256; i++) {
			c = i;
 		   /* The idea to initialize the register with the byte instead of
		     * zero was stolen from Haruhiko Okumura's ar002
		     */
			for (k = 8; k; k--)
				c = c & 1 ? (c >> 1) ^ e : c >> 1;
			crc_32_tab[i]=c;
		}
	}

	if (s == NULL) {
		c = 0xffffffffL;
	} else {
		c = crc;
		if (n)
			do {
				c = crc_32_tab[((int) c ^ (*s++)) & 0xff] ^ (c >> 8);
			} while (--n);
	}
	crc = c;
	return c ^ 0xffffffffL;		/* (instead of ~c for 64-bit machines) */
}

void write_error_msg()
{
	fprintf(stderr, "\n");
	perror("");
	abort_gzip();
}

/* ===========================================================================
 * Does the same as write(), but also handles partial pipe writes and checks
 * for error return.
 */
void write_buf(fd, buf, cnt)
int fd;
void * buf;
unsigned cnt;
{
	unsigned n;

	while ((n = write(fd, buf, cnt)) != cnt) {
		if (n == (unsigned) (-1)) {
			write_error_msg();
		}
		cnt -= n;
		buf = (void *) ((char *) buf + n);
	}
}

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
void flush_window()
{
	if (outcnt == 0)
		return;
	updcrc(window, outcnt);

	if (!test_mode)
		write_buf(ofd, (char *) window, outcnt);
	bytes_out += (ulg) outcnt;
	outcnt = 0;
}

int inflate_stored()
/* "decompress" an inflated type 0 (stored) block. */
{
	unsigned n;			/* number of bytes in block */
	unsigned w;			/* current window position */
	register ulg b;			/* bit buffer */
	register unsigned k;		/* number of bits in bit buffer */

	/* make local copies of globals */
	b = bb;				/* initialize bit buffer */
	k = bk;
	w = outcnt;			/* initialize window position */

	/* go to byte boundary */
	n = k & 7;
	DUMPBITS(n);

	/* get the length and its complement */
	NEEDBITS(16)
		n = ((unsigned) b & 0xffff);
	DUMPBITS(16)
		NEEDBITS(16)
		if (n != (unsigned) ((~b) & 0xffff))
		return 1;		/* error in compressed data */
	DUMPBITS(16)

		/* read and output the compressed data */
		while (n--) {
		NEEDBITS(8)
			window[w++] = (uch) b;
		if (w == WSIZE) {
//			flush_output(w);
				outcnt=(w),
				flush_window();
			w = 0;
		}
		DUMPBITS(8)
	}

	/* restore the globals from the locals */
	outcnt = w;			/* restore global window pointer */
	bb = b;				/* restore global bit buffer */
	bk = k;
	return 0;
}

int huft_free(t)
struct huft *t;				/* table to free */

/* Free the malloc'ed tables built by huft_build(), which makes a linked
   list of the tables it made, with the links in a dummy first entry of
   each table. */
{
	register struct huft *p, *q;

	/* Go through linked list, freeing from the malloced (t[-1]) address. */
	p = t;
	while (p != (struct huft *) NULL) {
		q = (--p)->v.t;
		free((char *) p);
		p = q;
	}
	return 0;
}


int huft_build(b, n, s, d, e, t, m)
unsigned *b;			/* code lengths in bits (all assumed <= BMAX) */
unsigned n;			/* number of codes (assumed <= N_MAX) */
unsigned s;			/* number of simple-valued codes (0..s-1) */
ush *d;				/* list of base values for non-simple codes */
ush *e;				/* list of extra bits for non-simple codes */
struct huft **t;		/* result: starting table */
int *m;				/* maximum lookup bits, returns actual */

/* Given a list of code lengths and a maximum table size, make a set of
   tables to decode that set of codes.  Return zero on success, one if
   the given code set is incomplete (the tables are still built in this
   case), two if the input is invalid (all zero length codes or an
   oversubscribed set of lengths), and three if not enough memory. */
{
	unsigned a;		/* counter for codes of length k */
	unsigned c[BMAX + 1];	/* bit length count table */
	unsigned f;		/* i repeats in table every f entries */
	int g;			/* maximum code length */
	int h;			/* table level */
	register unsigned i;	/* counter, current code */
	register unsigned j;	/* counter */
	register int k;		/* number of bits in current code */
	int l;			/* bits per table (returned in m) */
	register unsigned *p;		/* pointer into c[], b[], or v[] */
	register struct huft *q;	/* points to current table */
	struct huft r;		/* table entry for structure assignment */
	struct huft *u[BMAX];	/* table stack */
	unsigned v[N_MAX];	/* values in order of bit length */
	register int w;		/* bits before this table == (l * h) */
	unsigned x[BMAX + 1];	/* bit offsets, then code stack */
	unsigned *xp;		/* pointer into x */
	int y;			/* number of dummy codes added */
	unsigned z;		/* number of entries in current table */

	/* Generate counts for each bit length */
	memset ((void *)(c), 0, sizeof(c));
	p = b;
	i = n;
	do {
		Tracecv(*p,(stderr, (n - i >= ' ' && n - i <= '~' ? "%c %d\n" : "0x%x %d\n"), n - i, *p));
		c[*p]++;	/* assume all entries <= BMAX */
		p++;		/* Can't combine with above line (Solaris bug) */
	} while (--i);
	if (c[0] == n) {	/* null input--all zero length codes */
		*t = (struct huft *) NULL;
		*m = 0;
		return 0;
	}

	/* Find minimum and maximum length, bound *m by those */
	l = *m;
	for (j = 1; j <= BMAX; j++)
		if (c[j])
			break;
	k = j;				/* minimum code length */
	if ((unsigned) l < j)
		l = j;
	for (i = BMAX; i; i--)
		if (c[i])
			break;
	g = i;				/* maximum code length */
	if ((unsigned) l > i)
		l = i;
	*m = l;

	/* Adjust last length count to fill out codes, if needed */
	for (y = 1 << j; j < i; j++, y <<= 1)
		if ((y -= c[j]) < 0)
			return 2;	/* bad input: more codes than bits */
	if ((y -= c[i]) < 0)
		return 2;
	c[i] += y;

	/* Generate starting offsets into the value table for each length */
	x[1] = j = 0;
	p = c + 1;
	xp = x + 2;
	while (--i) {			/* note that i == g from above */
		*xp++ = (j += *p++);
	}

	/* Make a table of values in order of bit lengths */
	p = b;
	i = 0;
	do {
		if ((j = *p++) != 0)
			v[x[j]++] = i;
	} while (++i < n);

	/* Generate the Huffman codes and for each, make the table entries */
	x[0] = i = 0;			/* first Huffman code is zero */
	p = v;				/* grab values in bit order */
	h = -1;				/* no tables yet--level -1 */
	w = -l;				/* bits decoded == (l * h) */
	u[0] = (struct huft *) NULL;	/* just to keep compilers happy */
	q = (struct huft *) NULL;	/* ditto */
	z = 0;				/* ditto */

	/* go through the bit lengths (k already is bits in shortest code) */
	for (; k <= g; k++) {
		a = c[k];
		while (a--) {
			/* here i is the Huffman code of length k bits for value *p */
			/* make tables up to required level */
			while (k > w + l) {
				h++;
				w += l;		/* previous table always l bits */

				/* compute minimum size table less than or equal to l bits */
				z = (z = g - w) > (unsigned) l ? l : z;	/* upper limit on table size */
				if ((f = 1 << (j = k - w)) > a + 1) {	/* try a k-w bit table *//* too few codes for k-w bit table */
					f -= a + 1;	/* deduct codes from patterns left */
					xp = c + k;
					while (++j < z) {	/* try smaller tables up to z bits */
						if ((f <<= 1) <= *++xp)
							break;	/* enough codes to use up j bits */
						f -= *xp;	/* else deduct codes from patterns */
					}
				}
				z = 1 << j;		/* table entries for j-bit table */

				/* allocate and link in new table */
				if (
					(q =
					 (struct huft *) malloc((z + 1) *
											sizeof(struct huft))) ==
					(struct huft *) NULL) {
					if (h)
						huft_free(u[0]);
					return 3;	/* not enough memory */
				}
				hufts += z + 1;	/* track memory usage */
				*t = q + 1;		/* link to list for huft_free() */
				*(t = &(q->v.t)) = (struct huft *) NULL;
				u[h] = ++q;		/* table starts after link */

				/* connect to last table, if there is one */
				if (h) {
					x[h] = i;	/* save pattern for backing up */
					r.b = (uch) l;	/* bits to dump before this table */
					r.e = (uch) (16 + j);	/* bits in this table */
					r.v.t = q;	/* pointer to this table */
					j = i >> (w - l);	/* (get around Turbo C bug) */
					u[h - 1][j] = r;	/* connect to last table */
				}
			}

			/* set up table entry in r */
			r.b = (uch) (k - w);
			if (p >= v + n)
				r.e = 99;		/* out of values--invalid code */
			else if (*p < s) {
				r.e = (uch) (*p < 256 ? 16 : 15);	/* 256 is end-of-block code */
				r.v.n = (ush) (*p);	/* simple code is just the value */
				p++;			/* one compiler does not like *p++ */
			} else {
				r.e = (uch) e[*p - s];	/* non-simple--look up in lists */
				r.v.n = d[*p++ - s];
			}

			/* fill code-like entries with r */
			f = 1 << (k - w);
			for (j = i >> w; j < z; j += f)
				q[j] = r;

			/* backwards increment the k-bit code i */
			for (j = 1 << (k - 1); i & j; j >>= 1)
				i ^= j;
			i ^= j;

			/* backup over finished tables */
			while ((i & ((1 << w) - 1)) != x[h]) {
				h--;			/* don't need to update q */
				w -= l;
			}
		}
	}
	/* Return true (1) if we were given an incomplete table */
	return y != 0 && g != 1;
}


int inflate_codes(tl, td, bl, bd)
struct huft *tl, *td;			/* literal/length and distance decoder tables */
int bl, bd;						/* number of bits decoded by tl[] and td[] */

/* inflate (decompress) the codes in a deflated (compressed) block.
   Return an error code or zero if it all goes ok. */
{
	register unsigned e;		/* table entry flag/number of extra bits */
	unsigned n, d;				/* length and index for copy */
	unsigned w;				/* current window position */
	struct huft *t;				/* pointer to table entry */
	unsigned ml, md;			/* masks for bl and bd bits */
	register ulg b;				/* bit buffer */
	register unsigned k;		/* number of bits in bit buffer */

	/* make local copies of globals */
	b = bb;					/* initialize bit buffer */
	k = bk;
	w = outcnt;				/* initialize window position */

	/* inflate the coded data */
	ml = mask_bits[bl];			/* precompute masks for speed */
	md = mask_bits[bd];
	for (;;) {				/* do until end of block */
		NEEDBITS((unsigned) bl)
			if ((e = (t = tl + ((unsigned) b & ml))->e) > 16)
			do {
				if (e == 99)
					return 1;
				DUMPBITS(t->b)
					e -= 16;
				NEEDBITS(e)
			} while ((e = (t = t->v.t + ((unsigned) b & mask_bits[e]))->e)
					 > 16);
		DUMPBITS(t->b)
			if (e == 16) {		/* then it's a literal */
			window[w++] = (uch) t->v.n;
			Tracevv((stderr, "%c", window[w - 1]));
			if (w == WSIZE) {
//				flush_output(w);
				outcnt=(w),
				flush_window();
				w = 0;
			}
		} else {				/* it's an EOB or a length */

			/* exit if end of block */
			if (e == 15)
				break;

			/* get length of block to copy */
			NEEDBITS(e)
				n = t->v.n + ((unsigned) b & mask_bits[e]);
			DUMPBITS(e);

			/* decode distance of block to copy */
			NEEDBITS((unsigned) bd)
				if ((e = (t = td + ((unsigned) b & md))->e) > 16)
				do {
					if (e == 99)
						return 1;
					DUMPBITS(t->b)
						e -= 16;
					NEEDBITS(e)
				}
					while (
						   (e =
							(t =
							 t->v.t + ((unsigned) b & mask_bits[e]))->e) >
						   16);
			DUMPBITS(t->b)
				NEEDBITS(e)
				d = w - t->v.n - ((unsigned) b & mask_bits[e]);
			DUMPBITS(e)
				Tracevv((stderr, "\\[%d,%d]", w - d, n));

			/* do the copy */
			do {
				n -= (e =
					  (e =
					   WSIZE - ((d &= WSIZE - 1) > w ? d : w)) >
					  n ? n : e);
#if !defined(NOMEMCPY) && !defined(DEBUG)
				if (w - d >= e) {	/* (this test assumes unsigned comparison) */
					memcpy(window + w, window + d, e);
					w += e;
					d += e;
				} else			/* do it slow to avoid memcpy() overlap */
#endif							/* !NOMEMCPY */
					do {
						window[w++] = window[d++];
						Tracevv((stderr, "%c", window[w - 1]));
					} while (--e);
				if (w == WSIZE) {
//					flush_output(w);
				outcnt=(w),
				flush_window();
					w = 0;
				}
			} while (n);
		}
	}

	/* restore the globals from the locals */
	outcnt = w;			/* restore global window pointer */
	bb = b;				/* restore global bit buffer */
	bk = k;

	/* done */
	return 0;
}

int inflate_fixed()
/* decompress an inflated type 1 (fixed Huffman codes) block.  We should
   either replace this with a custom decoder, or at least precompute the
   Huffman tables. */
{
	int i;						/* temporary variable */
	struct huft *tl;			/* literal/length code table */
	struct huft *td;			/* distance code table */
	int bl;						/* lookup bits for tl */
	int bd;						/* lookup bits for td */
	unsigned l[288];			/* length list for huft_build */

	/* set up literal table */
	for (i = 0; i < 144; i++)
		l[i] = 8;
	for (; i < 256; i++)
		l[i] = 9;
	for (; i < 280; i++)
		l[i] = 7;
	for (; i < 288; i++)		/* make a complete, but wrong code set */
		l[i] = 8;
	bl = 7;
	if ((i = huft_build(l, 288, 257, cplens, cplext, &tl, &bl)) != 0)
		return i;

	/* set up distance table */
	for (i = 0; i < 30; i++)	/* make an incomplete code set */
		l[i] = 5;
	bd = 5;
	if ((i = huft_build(l, 30, 0, cpdist, cpdext, &td, &bd)) > 1) {
		huft_free(tl);
		return i;
	}

	/* decompress until an end-of-block code */
	if (inflate_codes(tl, td, bl, bd))
		return 1;

	/* free the decoding tables, return */
	huft_free(tl);
	huft_free(td);
	return 0;
}

int inflate_dynamic()
/* decompress an inflated type 2 (dynamic Huffman codes) block. */
{
	int dbits = 6;					/* bits in base distance lookup table */
	int lbits = 9;					/* bits in base literal/length lookup table */

	int i;						/* temporary variables */
	unsigned j;
	unsigned l;					/* last length */
	unsigned m;					/* mask for bit lengths table */
	unsigned n;					/* number of lengths to get */
	struct huft *tl;			/* literal/length code table */
	struct huft *td;			/* distance code table */
	int bl;						/* lookup bits for tl */
	int bd;						/* lookup bits for td */
	unsigned nb;				/* number of bit length codes */
	unsigned nl;				/* number of literal/length codes */
	unsigned nd;				/* number of distance codes */

#ifdef PKZIP_BUG_WORKAROUND
	unsigned ll[288 + 32];		/* literal/length and distance code lengths */
#else
	unsigned ll[286 + 30];		/* literal/length and distance code lengths */
#endif
	register ulg b;				/* bit buffer */
	register unsigned k;		/* number of bits in bit buffer */

	/* make local bit buffer */
	b = bb;
	k = bk;

	/* read in table lengths */
	NEEDBITS(5)
		nl = 257 + ((unsigned) b & 0x1f);	/* number of literal/length codes */
	DUMPBITS(5)
		NEEDBITS(5)
		nd = 1 + ((unsigned) b & 0x1f);	/* number of distance codes */
	DUMPBITS(5)
		NEEDBITS(4)
		nb = 4 + ((unsigned) b & 0xf);	/* number of bit length codes */
	DUMPBITS(4)
#ifdef PKZIP_BUG_WORKAROUND
		if (nl > 288 || nd > 32)
#else
		if (nl > 286 || nd > 30)
#endif
		return 1;				/* bad lengths */

	/* read in bit-length-code lengths */
	for (j = 0; j < nb; j++) {
		NEEDBITS(3)
			ll[border[j]] = (unsigned) b & 7;
		DUMPBITS(3)
	}
	for (; j < 19; j++)
		ll[border[j]] = 0;

	/* build decoding table for trees--single level, 7 bit lookup */
	bl = 7;
	if ((i = huft_build(ll, 19, 19, NULL, NULL, &tl, &bl)) != 0) {
		if (i == 1)
			huft_free(tl);
		return i;			/* incomplete code set */
	}

	/* read in literal and distance code lengths */
	n = nl + nd;
	m = mask_bits[bl];
	i = l = 0;
	while ((unsigned) i < n) {
		NEEDBITS((unsigned) bl)
			j = (td = tl + ((unsigned) b & m))->b;
		DUMPBITS(j)
			j = td->v.n;
		if (j < 16)			/* length of code in bits (0..15) */
			ll[i++] = l = j;	/* save last length in l */
		else if (j == 16) {		/* repeat last length 3 to 6 times */
			NEEDBITS(2)
				j = 3 + ((unsigned) b & 3);
			DUMPBITS(2)
				if ((unsigned) i + j > n)
				return 1;
			while (j--)
				ll[i++] = l;
		} else if (j == 17) {	/* 3 to 10 zero length codes */
			NEEDBITS(3)
				j = 3 + ((unsigned) b & 7);
			DUMPBITS(3)
				if ((unsigned) i + j > n)
				return 1;
			while (j--)
				ll[i++] = 0;
			l = 0;
		} else {		/* j == 18: 11 to 138 zero length codes */

			NEEDBITS(7)
				j = 11 + ((unsigned) b & 0x7f);
			DUMPBITS(7)
				if ((unsigned) i + j > n)
				return 1;
			while (j--)
				ll[i++] = 0;
			l = 0;
		}
	}

	/* free decoding table for trees */
	huft_free(tl);

	/* restore the global bit buffer */
	bb = b;
	bk = k;

	/* build the decoding tables for literal/length and distance codes */
	bl = lbits;
	if ((i = huft_build(ll, nl, 257, cplens, cplext, &tl, &bl)) != 0) {
		if (i == 1) {
			fprintf(stderr, " incomplete literal tree\n");
			huft_free(tl);
		}
		return i;			/* incomplete code set */
	}
	bd = dbits;
	if ((i = huft_build(ll + nl, nd, 0, cpdist, cpdext, &td, &bd)) != 0) {
		if (i == 1) {
			fprintf(stderr, " incomplete distance tree\n");
			huft_free(td);
		}
		huft_free(tl);
		return i;			/* incomplete code set */
	}

	/* decompress until an end-of-block code */
	if (inflate_codes(tl, td, bl, bd))
		return 1;

	/* free the decoding tables, return */
	huft_free(tl);
	huft_free(td);
	return 0;
}

/* decompress an inflated block */
int inflate_block(e)
int *e;					/* last block flag */
{
	unsigned t;			/* block type */
	register ulg b;			/* bit buffer */
	register unsigned k;		/* number of bits in bit buffer */

	/* make local bit buffer */
	b = bb;
	k = bk;

	/* read in last block bit */
	NEEDBITS(1)
		* e = (int) b & 1;
	DUMPBITS(1)

		/* read in block type */
		NEEDBITS(2)
		t = (unsigned) b & 3;
	DUMPBITS(2)

		/* restore the global bit buffer */
		bb = b;
	bk = k;

	/* inflate that block type */
	if (t == 2)
		return inflate_dynamic();
	if (t == 0)
		return inflate_stored();
	if (t == 1)
		return inflate_fixed();

	/* bad block type */
	return 2;
}

int inflate()
/* decompress an inflated entry */
{
	int e;				/* last block flag */
	int r;				/* result code */
	unsigned h;			/* maximum struct huft's malloc'ed */

	/* initialize window, bit buffer */
	outcnt = 0;
	bk = 0;
	bb = 0;

	/* decompress until the last block */
	h = 0;
	do {
		hufts = 0;
		if ((r = inflate_block(&e)) != 0)
			return r;
		if (hufts > h)
			h = hufts;
	} while (!e);

	/* Undo too much lookahead. The next read will be byte aligned so we
	 * can discard unused bits in the last meaningful byte.
	 */
	while (bk >= 8) {
		bk -= 8;
		inptr--;
	}

	/* flush out window */
	outcnt=(outcnt),
	flush_window();
	/* return success */
#ifdef DEBUG
	fprintf(stderr, "<%u> ", h);
#endif							/* DEBUG */
	return 0;
}

/* ===========================================================================
 * Unzip in to out.  This routine works on both gzip and pkzip files.
 *
 * IN assertions: the buffer inbuf contains already the beginning of
 *   the compressed data, from offsets inptr to insize-1 included.
 *   The magic header has already been checked. The output buffer is cleared.
 */
extern int unzip(in, out)
int in, out;					/* input and output file descriptors */
{
	int ext_header = 0;				/* set if extended local header */
	int pkzip = 0;					/* set for a pkzip file */
	ulg orig_crc = 0;			/* original crc */
	ulg orig_len = 0;			/* original uncompressed length */
	int n;
	uch buf[EXTHDR];			/* extended local header */

	ifd = in;
	ofd = out;
	method = get_method(ifd);
	if (method < 0) {
		exit(exit_code);		/* error message already emitted */
	}

	updcrc(NULL, 0);			/* initialize crc */

	if (pkzip && !ext_header) {	/* crc and length at the end otherwise */
		orig_crc = LG(inbuf + LOCCRC);
		orig_len = LG(inbuf + LOCLEN);
	}

	/* Decompress */
	if (method == DEFLATED) {

		int res = inflate();

		if (res == 3) {
			error_msg(memory_exhausted);
		} else if (res != 0) {
			error_msg("invalid compressed data--format violated");
		}

	} else {
		error_msg("internal error, invalid method");
	}

	/* Get the crc and original length */
	if (!pkzip) {
		/* crc32  (see algorithm.doc)
		   * uncompressed input size modulo 2^32
		 */
		for (n = 0; n < 8; n++) {
			buf[n] = (uch) get_byte();	/* may cause an error if EOF */
		}
		orig_crc = LG(buf);
		orig_len = LG(buf + 4);

	} else if (ext_header) {	/* If extended header, check it */
		/* signature - 4bytes: 0x50 0x4b 0x07 0x08
		 * CRC-32 value
		 * compressed size 4-bytes
		 * uncompressed size 4-bytes
		 */
		for (n = 0; n < EXTHDR; n++) {
			buf[n] = (uch) get_byte();	/* may cause an error if EOF */
		}
		orig_crc = LG(buf + 4);
		orig_len = LG(buf + 12);
	}

	/* Validate decompression */
	if (orig_crc != updcrc(outbuf, 0)) {
		error_msg("invalid compressed data--crc error");
	}
	if (orig_len != (ulg) bytes_out) {
		error_msg("invalid compressed data--length error");
	}

	/* Check if there are more entries in a pkzip file */
	if (pkzip && inptr + 4 < insize && LG(inbuf + inptr) == LOCSIG) {
		error_msg("has more than one entry--rest ignored");
		if (exit_code == OK)
			exit_code = WARNING;
	}
	ext_header = pkzip = 0;		/* for next file */
	return OK;
}


/* ===========================================================================
 * Clear input and output buffers
 */
void clear_bufs(void)
{
	outcnt = 0;
	insize = inptr = 0;
	bytes_in = bytes_out = 0L;
}

/* ===========================================================================
 * Initialize gunzip buffers and signals
 */
extern int gunzip_init()
{
	foreground = signal(SIGINT, SIG_IGN) != SIG_IGN;
	if (foreground) {
		(void) signal(SIGINT, (sig_type) abort_gzip);
	}
#ifdef SIGTERM
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
		(void) signal(SIGTERM, (sig_type) abort_gzip);
	}
#endif
#ifdef SIGHUP
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
		(void) signal(SIGHUP, (sig_type) abort_gzip);
	}
#endif

	/* Allocate all global buffers (for DYN_ALLOC option) */
	inbuf = xmalloc((size_t)((INBUFSIZ+INBUF_EXTRA+1L)*sizeof(uch)));
	outbuf = xmalloc((size_t)((OUTBUFSIZ+OUTBUF_EXTRA+1L)*sizeof(uch)));
	d_buf = xmalloc((size_t)((DIST_BUFSIZE+1L)*sizeof(ush)));
	window = xmalloc((size_t)(((2L*WSIZE)+1L)*sizeof(uch)));
	tab_prefix0 = xmalloc((size_t)(((1L<<(BITS-1))+1L)*sizeof(ush)));	
	tab_prefix1 = xmalloc((size_t)(((1L<<(BITS-1))+1L)*sizeof(ush)));
	
	clear_bufs();			/* clear input and output buffers */
	part_nb = 0;
	return(EXIT_SUCCESS);	
}


/* ======================================================================== */
int gunzip_main(int argc, char **argv)
{
	int tostdout = 0;
	int fromstdin = 0;
	int result;
	int inFileNum;
	int outFileNum;
	int delInputFile = 0;
	int force = 0;
	struct stat statBuf;
	char *delFileName;
	RESERVE_BB_BUFFER(ifname, MAX_PATH_LEN+1);  /* input file name */
	RESERVE_BB_BUFFER(ofname, MAX_PATH_LEN+1);  /* output file name */

	method = DEFLATED;	/* default compression method */
	exit_code = OK;	/* let's go out on a limb and assume everything will run fine (wink wink) */

	if (strcmp(applet_name, "zcat") == 0) {
		force = 1;
		tostdout = 1;
	}

	/* Parse any options */
	while (--argc > 0 && **(++argv) == '-') {
		if (*((*argv) + 1) == '\0') {
			tostdout = 1;
		}
		while (*(++(*argv))) {
			switch (**argv) {
			case 'c':
				tostdout = 1;
				break;
			case 't':
				test_mode = 1;
				break;
			case 'f':
				force = 1;
				break;
			default:
				usage(gunzip_usage);
			}
		}
	}

	if (argc <= 0) {
		tostdout = 1;
		fromstdin = 1;
	}

	if (isatty(fileno(stdin)) && fromstdin==1 && force==0)
		error_msg_and_die( "data not read from terminal. Use -f to force it.");
	if (isatty(fileno(stdout)) && tostdout==1 && force==0)
		error_msg_and_die( "data not written to terminal. Use -f to force it.");

	gunzip_init();

	if (fromstdin == 1) {
		strcpy(ofname, "stdin");

		inFileNum = fileno(stdin);
		ifile_size = -1L;		/* convention for unknown size */
	} else {
		/* Open up the input file */
		if (argc <= 0)
			usage(gunzip_usage);
		if (strlen(*argv) > MAX_PATH_LEN) {
			error_msg(name_too_long);
			exit(WARNING);
		}
		strcpy(ifname, *argv);

		/* Open input fille */
		inFileNum = open(ifname, O_RDONLY);
		if (inFileNum < 0) {
			perror(ifname);
			exit(WARNING);
		}
		/* Get the time stamp on the input file. */
		result = stat(ifname, &statBuf);
		if (result < 0) {
			perror(ifname);
			exit(WARNING);
		}
		ifile_size = statBuf.st_size;
	}

	if (tostdout == 1) {
		/* And get to work */
		strcpy(ofname, "stdout");
		outFileNum = fileno(stdout);

		/* Actually do the compression/decompression. */
		unzip(inFileNum, outFileNum);

	} else if (test_mode) {
		/* Actually do the compression/decompression. */
		unzip(inFileNum, 2);
	} else {
		char *pos;

		/* And get to work */
		if (strlen(ifname) > MAX_PATH_LEN - 4) {
			error_msg(name_too_long);
			exit(WARNING);
		}
		strcpy(ofname, ifname);
		pos = strstr(ofname, ".gz");
		if (pos != NULL) {
			*pos = '\0';
			delInputFile = 1;
		} else {
			pos = strstr(ofname, ".tgz");
			if (pos != NULL) {
				*pos = '\0';
				strcat(pos, ".tar");
				delInputFile = 1;
			}
		}

		/* Open output fille */
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
		outFileNum = open(ofname, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW);
#else
		outFileNum = open(ofname, O_RDWR | O_CREAT | O_EXCL);
#endif
		if (outFileNum < 0) {
			perror(ofname);
			exit(WARNING);
		}
		/* Set permissions on the file */
		fchmod(outFileNum, statBuf.st_mode);

		/* Actually do the compression/decompression. */
		result = unzip(inFileNum, outFileNum);

		close(outFileNum);
		close(inFileNum);
		/* Delete the original file */
		if (result == OK)
			delFileName = ifname;
		else
			delFileName = ofname;

		if (delInputFile == 1 && unlink(delFileName) < 0) {
			perror(delFileName);
			return EXIT_FAILURE;
		}
	}
	return(exit_code);
}
