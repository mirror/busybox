/* vi: set sw=4 ts=4: */
/*
 * gunzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersen@codepoet.org> to support
 * files as well as stdin/stdout, and to generally behave itself wrt
 * command line handling.
 *
 * General cleanup to better adhere to the style guide and make use of standard
 * busybox functions by Glenn McGrath <bug1@optushome.com.au>
 *
 * read_gz interface + associated hacking by Laurence Anderson
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

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "config.h"
#include "busybox.h"
#include "unarchive.h"

typedef struct huft_s {
	unsigned char e;	/* number of extra bits or operation */
	unsigned char b;	/* number of bits in this code or subcode */
	union {
		unsigned short n;	/* literal, length base, or distance base */
		struct huft_s *t;	/* pointer to next level of table */
	} v;
} huft_t;

static int gunzip_src_fd;
unsigned int gunzip_bytes_out;	/* number of output bytes */
static unsigned int gunzip_outbuf_count;	/* bytes in output buffer */

/* gunzip_window size--must be a power of two, and
 *  at least 32K for zip's deflate method */
static const int gunzip_wsize = 0x8000;
static unsigned char *gunzip_window;

static unsigned int *gunzip_crc_table;
unsigned int gunzip_crc;

/* If BMAX needs to be larger than 16, then h and x[] should be ulg. */
#define BMAX 16	/* maximum bit length of any code (16 for explode) */
#define N_MAX 288	/* maximum number of codes in any set */

/* bitbuffer */
static unsigned int gunzip_bb;	/* bit buffer */
static unsigned char gunzip_bk;	/* bits in bit buffer */

/* These control the size of the bytebuffer */
static unsigned int bytebuffer_max = 0x8000;
static unsigned char *bytebuffer = NULL;
static unsigned int bytebuffer_offset = 0;
static unsigned int bytebuffer_size = 0;

static const unsigned short mask_bits[] = {
	0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
	0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

/* Copy lengths for literal codes 257..285 */
static const unsigned short cplens[] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
		67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

/* note: see note #13 above about the 258 in this list. */
/* Extra bits for literal codes 257..285 */
static const unsigned char cplext[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5,
		5, 5, 5, 0, 99, 99
};						/* 99==invalid */

/* Copy offsets for distance codes 0..29 */
static const unsigned short cpdist[] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
		769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

/* Extra bits for distance codes */
static const unsigned char cpdext[] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
		11, 11, 12, 12, 13, 13
};

/* Tables for deflate from PKZIP's appnote.txt. */
/* Order of the bit length code lengths */
static const unsigned char border[] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static unsigned int fill_bitbuffer(unsigned int bitbuffer, unsigned int *current, const unsigned int required)
{
	while (*current < required) {
		if (bytebuffer_offset >= bytebuffer_size) {
			/* Leave the first 4 bytes empty so we can always unwind the bitbuffer
			 * to the front of the bytebuffer, leave 4 bytes free at end of tail
			 * so we can easily top up buffer in check_trailer_gzip() */
			bytebuffer_size = 4 + bb_xread(gunzip_src_fd, &bytebuffer[4], bytebuffer_max - 8);
			bytebuffer_offset = 4;
		}
		bitbuffer |= ((unsigned int) bytebuffer[bytebuffer_offset]) << *current;
		bytebuffer_offset++;
		*current += 8;
	}
	return(bitbuffer);
}

static void make_gunzip_crc_table(void)
{
	const unsigned int poly = 0xedb88320;	/* polynomial exclusive-or pattern */
	unsigned short i;	/* counter for all possible eight bit values */

	/* initial shift register value */
	gunzip_crc = 0xffffffffL;
	gunzip_crc_table = (unsigned int *) malloc(256 * sizeof(unsigned int));

	/* Compute and print table of CRC's, five per line */
	for (i = 0; i < 256; i++) {
		unsigned int table_entry;	/* crc shift register */
		unsigned char k;	/* byte being shifted into crc apparatus */

		table_entry = i;
		/* The idea to initialize the register with the byte instead of
		   * zero was stolen from Haruhiko Okumura's ar002
		 */
		for (k = 8; k; k--) {
			if (table_entry & 1) {
				table_entry = (table_entry >> 1) ^ poly;
			} else {
				table_entry >>= 1;
	}
	}
		gunzip_crc_table[i] = table_entry;
	}
}

/*
 * Free the malloc'ed tables built by huft_build(), which makes a linked
 * list of the tables it made, with the links in a dummy first entry of
 * each table.
 * t: table to free
 */
static int huft_free(huft_t * t)
{
	huft_t *p;
	huft_t *q;

	/* Go through linked list, freeing from the malloced (t[-1]) address. */
	p = t;
	while (p != (huft_t *) NULL) {
		q = (--p)->v.t;
		free((char *) p);
		p = q;
	}
	return 0;
}

/* Given a list of code lengths and a maximum table size, make a set of
 * tables to decode that set of codes.  Return zero on success, one if
 * the given code set is incomplete (the tables are still built in this
 * case), two if the input is invalid (all zero length codes or an
 * oversubscribed set of lengths), and three if not enough memory.
 *
 * b:	code lengths in bits (all assumed <= BMAX)
 * n:	number of codes (assumed <= N_MAX)
 * s:	number of simple-valued codes (0..s-1)
 * d:	list of base values for non-simple codes
 * e:	list of extra bits for non-simple codes
 * t:	result: starting table
 * m:	maximum lookup bits, returns actual
 */
static int huft_build(unsigned int *b, const unsigned int n,
					  const unsigned int s, const unsigned short *d,
					  const unsigned char *e, huft_t ** t, int *m)
{
	unsigned a;			/* counter for codes of length k */
	unsigned c[BMAX + 1];	/* bit length count table */
	unsigned f;			/* i repeats in table every f entries */
	int g;				/* maximum code length */
	int h;				/* table level */
	register unsigned i;	/* counter, current code */
	register unsigned j;	/* counter */
	register int k;		/* number of bits in current code */
	int l;				/* bits per table (returned in m) */
	register unsigned *p;	/* pointer into c[], b[], or v[] */
	register huft_t *q;	/* points to current table */
	huft_t r;			/* table entry for structure assignment */
	huft_t *u[BMAX];	/* table stack */
	unsigned v[N_MAX];	/* values in order of bit length */
	register int w;		/* bits before this table == (l * h) */
	unsigned x[BMAX + 1];	/* bit offsets, then code stack */
	unsigned *xp;		/* pointer into x */
	int y;				/* number of dummy codes added */
	unsigned z;			/* number of entries in current table */

	/* Generate counts for each bit length */
	memset((void *) (c), 0, sizeof(c));
	p = b;
	i = n;
	do {
		c[*p]++;		/* assume all entries <= BMAX */
		p++;			/* Can't combine with above line (Solaris bug) */
	} while (--i);
	if (c[0] == n) {	/* null input--all zero length codes */
		*t = (huft_t *) NULL;
		*m = 0;
		return 0;
	}

	/* Find minimum and maximum length, bound *m by those */
	l = *m;
	for (j = 1; j <= BMAX; j++) {
		if (c[j]) {
			break;
		}
	}
	k = j;				/* minimum code length */
	if ((unsigned) l < j) {
		l = j;
	}
	for (i = BMAX; i; i--) {
		if (c[i]) {
			break;
		}
	}
	g = i;				/* maximum code length */
	if ((unsigned) l > i) {
		l = i;
	}
	*m = l;

	/* Adjust last length count to fill out codes, if needed */
	for (y = 1 << j; j < i; j++, y <<= 1) {
		if ((y -= c[j]) < 0) {
			return 2;	/* bad input: more codes than bits */
		}
	}
	if ((y -= c[i]) < 0) {
		return 2;
	}
	c[i] += y;

	/* Generate starting offsets into the value table for each length */
	x[1] = j = 0;
	p = c + 1;
	xp = x + 2;
	while (--i) {		/* note that i == g from above */
		*xp++ = (j += *p++);
	}

	/* Make a table of values in order of bit lengths */
	p = b;
	i = 0;
	do {
		if ((j = *p++) != 0) {
			v[x[j]++] = i;
		}
	} while (++i < n);

	/* Generate the Huffman codes and for each, make the table entries */
	x[0] = i = 0;		/* first Huffman code is zero */
	p = v;				/* grab values in bit order */
	h = -1;				/* no tables yet--level -1 */
	w = -l;				/* bits decoded == (l * h) */
	u[0] = (huft_t *) NULL;	/* just to keep compilers happy */
	q = (huft_t *) NULL;	/* ditto */
	z = 0;				/* ditto */

	/* go through the bit lengths (k already is bits in shortest code) */
	for (; k <= g; k++) {
		a = c[k];
		while (a--) {
			/* here i is the Huffman code of length k bits for value *p */
			/* make tables up to required level */
			while (k > w + l) {
				h++;
				w += l;	/* previous table always l bits */

				/* compute minimum size table less than or equal to l bits */
				z = (z = g - w) > (unsigned) l ? l : z;	/* upper limit on table size */
				if ((f = 1 << (j = k - w)) > a + 1) {	/* try a k-w bit table *//* too few codes for k-w bit table */
					f -= a + 1;	/* deduct codes from patterns left */
					xp = c + k;
					while (++j < z) {	/* try smaller tables up to z bits */
						if ((f <<= 1) <= *++xp) {
							break;	/* enough codes to use up j bits */
						}
						f -= *xp;	/* else deduct codes from patterns */
					}
				}
				z = 1 << j;	/* table entries for j-bit table */

				/* allocate and link in new table */
				q = (huft_t *) xmalloc((z + 1) * sizeof(huft_t));

				*t = q + 1;	/* link to list for huft_free() */
				*(t = &(q->v.t)) = NULL;
				u[h] = ++q;	/* table starts after link */

				/* connect to last table, if there is one */
				if (h) {
					x[h] = i;	/* save pattern for backing up */
					r.b = (unsigned char) l;	/* bits to dump before this table */
					r.e = (unsigned char) (16 + j);	/* bits in this table */
					r.v.t = q;	/* pointer to this table */
					j = i >> (w - l);	/* (get around Turbo C bug) */
					u[h - 1][j] = r;	/* connect to last table */
				}
			}

			/* set up table entry in r */
			r.b = (unsigned char) (k - w);
			if (p >= v + n) {
				r.e = 99;	/* out of values--invalid code */
			} else if (*p < s) {
				r.e = (unsigned char) (*p < 256 ? 16 : 15);	/* 256 is end-of-block code */
				r.v.n = (unsigned short) (*p);	/* simple code is just the value */
				p++;	/* one compiler does not like *p++ */
			} else {
				r.e = (unsigned char) e[*p - s];	/* non-simple--look up in lists */
				r.v.n = d[*p++ - s];
			}

			/* fill code-like entries with r */
			f = 1 << (k - w);
			for (j = i >> w; j < z; j += f) {
				q[j] = r;
			}

			/* backwards increment the k-bit code i */
			for (j = 1 << (k - 1); i & j; j >>= 1) {
				i ^= j;
			}
			i ^= j;

			/* backup over finished tables */
			while ((i & ((1 << w) - 1)) != x[h]) {
				h--;	/* don't need to update q */
				w -= l;
			}
		}
	}
	/* Return true (1) if we were given an incomplete table */
	return y != 0 && g != 1;
}

/*
 * inflate (decompress) the codes in a deflated (compressed) block.
 * Return an error code or zero if it all goes ok.
 *
 * tl, td: literal/length and distance decoder tables
 * bl, bd: number of bits decoded by tl[] and td[]
 */
static int inflate_codes(huft_t * my_tl, huft_t * my_td, const unsigned int my_bl, const unsigned int my_bd, int setup)
{
	static unsigned int e;	/* table entry flag/number of extra bits */
	static unsigned int n, d;	/* length and index for copy */
	static unsigned int w;	/* current gunzip_window position */
	static huft_t *t;			/* pointer to table entry */
	static unsigned int ml, md;	/* masks for bl and bd bits */
	static unsigned int b;	/* bit buffer */
	static unsigned int k;			/* number of bits in bit buffer */
	static huft_t *tl, *td;
	static unsigned int bl, bd;
	static int resumeCopy = 0;

	if (setup) { // 1st time we are called, copy in variables
		tl = my_tl;
		td = my_td;
		bl = my_bl;
		bd = my_bd;
		/* make local copies of globals */
		b = gunzip_bb;				/* initialize bit buffer */
		k = gunzip_bk;
		w = gunzip_outbuf_count;			/* initialize gunzip_window position */

		/* inflate the coded data */
		ml = mask_bits[bl];	/* precompute masks for speed */
		md = mask_bits[bd];
		return 0; // Don't actually do anything the first time
	}

	if (resumeCopy) goto do_copy;

	while (1) {			/* do until end of block */
		b = fill_bitbuffer(b, &k, bl);
		if ((e = (t = tl + ((unsigned) b & ml))->e) > 16)
			do {
				if (e == 99) {
					bb_error_msg_and_die("inflate_codes error 1");;
				}
				b >>= t->b;
				k -= t->b;
				e -= 16;
				b = fill_bitbuffer(b, &k, e);
			} while ((e =
					  (t = t->v.t + ((unsigned) b & mask_bits[e]))->e) > 16);
		b >>= t->b;
		k -= t->b;
		if (e == 16) {	/* then it's a literal */
			gunzip_window[w++] = (unsigned char) t->v.n;
			if (w == gunzip_wsize) {
				gunzip_outbuf_count = (w);
				//flush_gunzip_window();
				w = 0;
				return 1; // We have a block to read
			}
		} else {		/* it's an EOB or a length */

			/* exit if end of block */
			if (e == 15) {
				break;
			}

			/* get length of block to copy */
			b = fill_bitbuffer(b, &k, e);
			n = t->v.n + ((unsigned) b & mask_bits[e]);
			b >>= e;
			k -= e;

			/* decode distance of block to copy */
			b = fill_bitbuffer(b, &k, bd);
			if ((e = (t = td + ((unsigned) b & md))->e) > 16)
				do {
					if (e == 99)
						bb_error_msg_and_die("inflate_codes error 2");;
					b >>= t->b;
					k -= t->b;
					e -= 16;
					b = fill_bitbuffer(b, &k, e);
				} while ((e =
						  (t =
						   t->v.t + ((unsigned) b & mask_bits[e]))->e) > 16);
			b >>= t->b;
			k -= t->b;
			b = fill_bitbuffer(b, &k, e);
			d = w - t->v.n - ((unsigned) b & mask_bits[e]);
			b >>= e;
			k -= e;

			/* do the copy */
do_copy:		do {
				n -= (e =
					  (e =
					   gunzip_wsize - ((d &= gunzip_wsize - 1) > w ? d : w)) > n ? n : e);
			   /* copy to new buffer to prevent possible overwrite */
				if (w - d >= e) {	/* (this test assumes unsigned comparison) */
					memcpy(gunzip_window + w, gunzip_window + d, e);
					w += e;
					d += e;
				} else {
				   /* do it slow to avoid memcpy() overlap */
				   /* !NOMEMCPY */
					do {
						gunzip_window[w++] = gunzip_window[d++];
					} while (--e);
				}
				if (w == gunzip_wsize) {
					gunzip_outbuf_count = (w);
					if (n) resumeCopy = 1;
					else resumeCopy = 0;
					//flush_gunzip_window();
					w = 0;
					return 1;
				}
			} while (n);
			resumeCopy = 0;
		}
	}

	/* restore the globals from the locals */
	gunzip_outbuf_count = w;			/* restore global gunzip_window pointer */
	gunzip_bb = b;				/* restore global bit buffer */
	gunzip_bk = k;

	/* normally just after call to inflate_codes, but save code by putting it here */
	/* free the decoding tables, return */
	huft_free(tl);
	huft_free(td);

	/* done */
	return 0;
}

static int inflate_stored(int my_n, int my_b_stored, int my_k_stored, int setup)
{
	static int n, b_stored, k_stored, w;
	if (setup) {
		n = my_n;
		b_stored = my_b_stored;
		k_stored = my_k_stored;
		w = gunzip_outbuf_count;		/* initialize gunzip_window position */
		return 0; // Don't do anything first time
	}

	/* read and output the compressed data */
	while (n--) {
		b_stored = fill_bitbuffer(b_stored, &k_stored, 8);
		gunzip_window[w++] = (unsigned char) b_stored;
		if (w == (unsigned int) gunzip_wsize) {
			gunzip_outbuf_count = (w);
			//flush_gunzip_window();
			w = 0;
			b_stored >>= 8;
			k_stored -= 8;
			return 1; // We have a block
		}
		b_stored >>= 8;
		k_stored -= 8;
	}

	/* restore the globals from the locals */
	gunzip_outbuf_count = w;		/* restore global gunzip_window pointer */
	gunzip_bb = b_stored;	/* restore global bit buffer */
	gunzip_bk = k_stored;
	return 0; // Finished
}

/*
 * decompress an inflated block
 * e: last block flag
 *
 * GLOBAL VARIABLES: bb, kk,
 */
 // Return values: -1 = inflate_stored, -2 = inflate_codes
static int inflate_block(int *e)
{
	unsigned t;			/* block type */
	register unsigned int b;	/* bit buffer */
	unsigned int k;	/* number of bits in bit buffer */

	/* make local bit buffer */

	b = gunzip_bb;
	k = gunzip_bk;

	/* read in last block bit */
	b = fill_bitbuffer(b, &k, 1);
	*e = (int) b & 1;
	b >>= 1;
	k -= 1;

	/* read in block type */
	b = fill_bitbuffer(b, &k, 2);
	t = (unsigned) b & 3;
	b >>= 2;
	k -= 2;

	/* restore the global bit buffer */
	gunzip_bb = b;
	gunzip_bk = k;

	/* inflate that block type */
	switch (t) {
	case 0:			/* Inflate stored */
	{
		unsigned int n;	/* number of bytes in block */
		unsigned int b_stored;	/* bit buffer */
		unsigned int k_stored;	/* number of bits in bit buffer */

		/* make local copies of globals */
		b_stored = gunzip_bb;	/* initialize bit buffer */
		k_stored = gunzip_bk;

		/* go to byte boundary */
		n = k_stored & 7;
		b_stored >>= n;
		k_stored -= n;

		/* get the length and its complement */
		b_stored = fill_bitbuffer(b_stored, &k_stored, 16);
		n = ((unsigned) b_stored & 0xffff);
		b_stored >>= 16;
		k_stored -= 16;

		b_stored = fill_bitbuffer(b_stored, &k_stored, 16);
		if (n != (unsigned) ((~b_stored) & 0xffff)) {
			return 1;	/* error in compressed data */
		}
		b_stored >>= 16;
		k_stored -= 16;

		inflate_stored(n, b_stored, k_stored, 1); // Setup inflate_stored
		return -1;
	}
	case 1:			/* Inflate fixed
						   * decompress an inflated type 1 (fixed Huffman codes) block.  We should
						   * either replace this with a custom decoder, or at least precompute the
						   * Huffman tables.
						 */
	{
		int i;			/* temporary variable */
		huft_t *tl;		/* literal/length code table */
		huft_t *td;		/* distance code table */
		unsigned int bl;			/* lookup bits for tl */
		unsigned int bd;			/* lookup bits for td */
		unsigned int l[288];	/* length list for huft_build */

		/* set up literal table */
		for (i = 0; i < 144; i++) {
			l[i] = 8;
		}
		for (; i < 256; i++) {
			l[i] = 9;
		}
		for (; i < 280; i++) {
			l[i] = 7;
		}
		for (; i < 288; i++) {	/* make a complete, but wrong code set */
			l[i] = 8;
		}
		bl = 7;
		if ((i = huft_build(l, 288, 257, cplens, cplext, &tl, &bl)) != 0) {
			return i;
		}

		/* set up distance table */
		for (i = 0; i < 30; i++) {	/* make an incomplete code set */
			l[i] = 5;
		}
		bd = 5;
		if ((i = huft_build(l, 30, 0, cpdist, cpdext, &td, &bd)) > 1) {
			huft_free(tl);
			return i;
		}

		/* decompress until an end-of-block code */
		inflate_codes(tl, td, bl, bd, 1); // Setup inflate_codes

		/* huft_free code moved into inflate_codes */

		return -2;
	}
	case 2:			/* Inflate dynamic */
	{
		const int dbits = 6;	/* bits in base distance lookup table */
		const int lbits = 9;	/* bits in base literal/length lookup table */

		huft_t *tl;		/* literal/length code table */
		huft_t *td;		/* distance code table */
		unsigned int i;			/* temporary variables */
		unsigned int j;
		unsigned int l;		/* last length */
		unsigned int m;		/* mask for bit lengths table */
		unsigned int n;		/* number of lengths to get */
		unsigned int bl;			/* lookup bits for tl */
		unsigned int bd;			/* lookup bits for td */
		unsigned int nb;	/* number of bit length codes */
		unsigned int nl;	/* number of literal/length codes */
		unsigned int nd;	/* number of distance codes */

		unsigned int ll[286 + 30];	/* literal/length and distance code lengths */
		unsigned int b_dynamic;	/* bit buffer */
		unsigned int k_dynamic;	/* number of bits in bit buffer */

		/* make local bit buffer */
		b_dynamic = gunzip_bb;
		k_dynamic = gunzip_bk;

		/* read in table lengths */
		b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, 5);
		nl = 257 + ((unsigned int) b_dynamic & 0x1f);	/* number of literal/length codes */

		b_dynamic >>= 5;
		k_dynamic -= 5;
		b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, 5);
		nd = 1 + ((unsigned int) b_dynamic & 0x1f);	/* number of distance codes */

		b_dynamic >>= 5;
		k_dynamic -= 5;
		b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, 4);
		nb = 4 + ((unsigned int) b_dynamic & 0xf);	/* number of bit length codes */

		b_dynamic >>= 4;
		k_dynamic -= 4;
		if (nl > 286 || nd > 30) {
			return 1;	/* bad lengths */
		}

		/* read in bit-length-code lengths */
		for (j = 0; j < nb; j++) {
			b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, 3);
			ll[border[j]] = (unsigned int) b_dynamic & 7;
			b_dynamic >>= 3;
			k_dynamic -= 3;
		}
		for (; j < 19; j++) {
			ll[border[j]] = 0;
		}

		/* build decoding table for trees--single level, 7 bit lookup */
		bl = 7;
		i = huft_build(ll, 19, 19, NULL, NULL, &tl, &bl);
		if (i != 0) {
			if (i == 1) {
				huft_free(tl);
			}
			return i;	/* incomplete code set */
		}

		/* read in literal and distance code lengths */
		n = nl + nd;
		m = mask_bits[bl];
		i = l = 0;
		while ((unsigned int) i < n) {
			b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, (unsigned int)bl);
			j = (td = tl + ((unsigned int) b_dynamic & m))->b;
			b_dynamic >>= j;
			k_dynamic -= j;
			j = td->v.n;
			if (j < 16) {	/* length of code in bits (0..15) */
				ll[i++] = l = j;	/* save last length in l */
			} else if (j == 16) {	/* repeat last length 3 to 6 times */
				b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, 2);
				j = 3 + ((unsigned int) b_dynamic & 3);
				b_dynamic >>= 2;
				k_dynamic -= 2;
				if ((unsigned int) i + j > n) {
					return 1;
				}
				while (j--) {
					ll[i++] = l;
				}
			} else if (j == 17) {	/* 3 to 10 zero length codes */
				b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, 3);
				j = 3 + ((unsigned int) b_dynamic & 7);
				b_dynamic >>= 3;
				k_dynamic -= 3;
				if ((unsigned int) i + j > n) {
					return 1;
				}
				while (j--) {
					ll[i++] = 0;
				}
				l = 0;
			} else {	/* j == 18: 11 to 138 zero length codes */
				b_dynamic = fill_bitbuffer(b_dynamic, &k_dynamic, 7);
				j = 11 + ((unsigned int) b_dynamic & 0x7f);
				b_dynamic >>= 7;
				k_dynamic -= 7;
				if ((unsigned int) i + j > n) {
					return 1;
				}
				while (j--) {
					ll[i++] = 0;
				}
				l = 0;
			}
		}

		/* free decoding table for trees */
		huft_free(tl);

		/* restore the global bit buffer */
		gunzip_bb = b_dynamic;
		gunzip_bk = k_dynamic;

		/* build the decoding tables for literal/length and distance codes */
		bl = lbits;

		if ((i = huft_build(ll, nl, 257, cplens, cplext, &tl, &bl)) != 0) {
			if (i == 1) {
				bb_error_msg_and_die("Incomplete literal tree");
				huft_free(tl);
			}
			return i;	/* incomplete code set */
		}

		bd = dbits;
		if ((i = huft_build(ll + nl, nd, 0, cpdist, cpdext, &td, &bd)) != 0) {
			if (i == 1) {
				bb_error_msg_and_die("incomplete distance tree");
				huft_free(td);
			}
			huft_free(tl);
			return i;	/* incomplete code set */
		}

		/* decompress until an end-of-block code */
		inflate_codes(tl, td, bl, bd, 1); // Setup inflate_codes

		/* huft_free code moved into inflate_codes */

		return -2;
	}
	default:
		/* bad block type */
		bb_error_msg_and_die("bad block type %d\n", t);
	}
}

static void calculate_gunzip_crc(void)
{
	int n;
	for (n = 0; n < gunzip_outbuf_count; n++) {
		gunzip_crc = gunzip_crc_table[((int) gunzip_crc ^ (gunzip_window[n])) & 0xff] ^ (gunzip_crc >> 8);
	}
	gunzip_bytes_out += gunzip_outbuf_count;
}

static int inflate_get_next_window(void)
{
	static int method = -1; // Method == -1 for stored, -2 for codes
	static int e = 0;
	static int needAnotherBlock = 1;

	gunzip_outbuf_count = 0;

	while(1) {
		int ret;

		if (needAnotherBlock) {
			if(e) {
				calculate_gunzip_crc();
				e = 0;
				needAnotherBlock = 1;
				return 0;
			} // Last block
			method = inflate_block(&e);
			needAnotherBlock = 0;
		}

		switch (method) {
			case -1:	ret = inflate_stored(0,0,0,0);
					break;
			case -2:	ret = inflate_codes(0,0,0,0,0);
					break;
			default:	bb_error_msg_and_die("inflate error %d", method);
		}

		if (ret == 1) {
			calculate_gunzip_crc();
			return 1; // More data left
		} else needAnotherBlock = 1; // End of that block
	}
	/* Doesnt get here */
}

/* Initialise bytebuffer, be carefull not to overfill the buffer */
extern void inflate_init(unsigned int bufsize)
{
	/* Set the bytebuffer size, default is same as gunzip_wsize */
	bytebuffer_max = bufsize + 8;
	bytebuffer_offset = 4;
	bytebuffer_size = 0;
}

extern int inflate_unzip(int in, int out)
{
	ssize_t nwrote;
	typedef void (*sig_type) (int);

	/* Allocate all global buffers (for DYN_ALLOC option) */
	gunzip_window = xmalloc(gunzip_wsize);
	gunzip_outbuf_count = 0;
	gunzip_bytes_out = 0;
	gunzip_src_fd = in;

	/* initialize gunzip_window, bit buffer */
	gunzip_bk = 0;
	gunzip_bb = 0;

	/* Create the crc table */
	make_gunzip_crc_table();

	/* Allocate space for buffer */
	bytebuffer = xmalloc(bytebuffer_max);

	while(1) {
		int ret = inflate_get_next_window();
		nwrote = bb_full_write(out, gunzip_window, gunzip_outbuf_count);
		if (nwrote == -1) {
			bb_perror_msg("write");
			return -1;
		}
		if (ret == 0) break;
	}

	/* Cleanup */
	free(gunzip_window);
	free(gunzip_crc_table);

	/* Store unused bytes in a global buffer so calling applets can access it */
	if (gunzip_bk >= 8) {
		/* Undo too much lookahead. The next read will be byte aligned
		 * so we can discard unused bits in the last meaningful byte. */
		bytebuffer_offset--;
		bytebuffer[bytebuffer_offset] = gunzip_bb & 0xff;
		gunzip_bb >>= 8;
		gunzip_bk -= 8;
	}
	return 0;
}

extern int inflate_gunzip(int in, int out)
{
	unsigned int stored_crc = 0;
	unsigned char count;

	inflate_unzip(in, out);

	/* top up the input buffer with the rest of the trailer */
	count = bytebuffer_size - bytebuffer_offset;
	if (count < 8) {
		bb_xread_all(in, &bytebuffer[bytebuffer_size], 8 - count);
		bytebuffer_size += 8 - count;
	}
	for (count = 0; count != 4; count++) {
		stored_crc |= (bytebuffer[bytebuffer_offset] << (count * 8));
		bytebuffer_offset++;
	}

	/* Validate decompression - crc */
	if (stored_crc != (gunzip_crc ^ 0xffffffffL)) {
		bb_error_msg("crc error");
	}

	/* Validate decompression - size */
	if (gunzip_bytes_out !=
		(bytebuffer[bytebuffer_offset] | (bytebuffer[bytebuffer_offset+1] << 8) |
		(bytebuffer[bytebuffer_offset+2] << 16) | (bytebuffer[bytebuffer_offset+3] << 24))) {
		bb_error_msg("Incorrect length");
	}

	return 0;
}
