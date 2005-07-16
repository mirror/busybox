/* vi: set sw=4 ts=4: */
/*
 * gunzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersee@debian.org> to support
 * files as well as stdin/stdout, and to generally behave itself wrt
 * command line handling.
 *
 * General cleanup to better adhere to the style guide and make use of
 * standard busybox functions by Glenn McGrath <bug1@optushome.com.au>
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
#include "libbb.h"

static FILE *in_file, *out_file;

/* these are freed by gz_close */
static unsigned char *window;
static unsigned long *crc_table;

static unsigned long crc; /* shift register contents */

/* Return codes from gzip */
static const int ERROR = 1;

/*
 * window size--must be a power of two, and
 *  at least 32K for zip's deflate method 
 */
static const int WSIZE = 0x8000;

/* If BMAX needs to be larger than 16, then h and x[] should be ulg. */
static const int BMAX = 16;		/* maximum bit length of any code (16 for explode) */
static const int N_MAX = 288;		/* maximum number of codes in any set */

static long bytes_out;		/* number of output bytes */
static unsigned long outcnt;	/* bytes in output buffer */

static unsigned hufts;		/* track memory usage */
static unsigned long bb;			/* bit buffer */
static unsigned bk;		/* bits in bit buffer */

typedef struct huft_s {
	unsigned char e;		/* number of extra bits or operation */
	unsigned char b;		/* number of bits in this code or subcode */
	union {
		unsigned short n;		/* literal, length base, or distance base */
		struct huft_s *t;	/* pointer to next level of table */
	} v;
} huft_t;

static const unsigned short mask_bits[] = {
	0x0000,
	0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
	0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

//static int error_number = 0;
/* ========================================================================
 * Signal and error handler.
 */
 
static void abort_gzip()
{
	error_msg("gzip aborted\n");
	exit(ERROR);
}

static void make_crc_table()
{
	unsigned long table_entry;      /* crc shift register */
	unsigned long poly = 0;      /* polynomial exclusive-or pattern */
	int i;                /* counter for all possible eight bit values */
	int k;                /* byte being shifted into crc apparatus */

	/* terms of polynomial defining this crc (except x^32): */
	static int p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

	/* initial shift register value */
	crc = 0xffffffffL;	
	crc_table = (unsigned long *) malloc(256 * sizeof(unsigned long));

	/* Make exclusive-or pattern from polynomial (0xedb88320) */
	for (i = 0; i < sizeof(p)/sizeof(int); i++)
		poly |= 1L << (31 - p[i]);

	/* Compute and print table of CRC's, five per line */
	for (i = 0; i < 256; i++) {
		table_entry = i;
	   /* The idea to initialize the register with the byte instead of
	     * zero was stolen from Haruhiko Okumura's ar002
	     */
		for (k = 8; k; k--) {
			table_entry = table_entry & 1 ? (table_entry >> 1) ^ poly : table_entry >> 1;
		}
		crc_table[i]=table_entry;
	}
}

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
static void flush_window(void)
{
	int n;

	if (outcnt == 0)
		return;

	for (n = 0; n < outcnt; n++) {
		crc = crc_table[((int) crc ^ (window[n])) & 0xff] ^ (crc >> 8);
	}

	if (fwrite(window, 1, outcnt, out_file) != outcnt) {
		error_msg_and_die("Couldnt write");
	}
	bytes_out += (unsigned long) outcnt;
	outcnt = 0;
}

/*
 * Free the malloc'ed tables built by huft_build(), which makes a linked
 * list of the tables it made, with the links in a dummy first entry of
 * each table. 
 * t: table to free
 */
static int huft_free(huft_t *t)
{
	huft_t *p, *q;

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
static int huft_build(unsigned int *b, const unsigned int n, const unsigned int s, 
	const unsigned short *d, const unsigned short *e, huft_t **t, int *m)
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
	register huft_t *q;	/* points to current table */
	huft_t r;		/* table entry for structure assignment */
	huft_t *u[BMAX];	/* table stack */
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
		c[*p]++;	/* assume all entries <= BMAX */
		p++;		/* Can't combine with above line (Solaris bug) */
	} while (--i);
	if (c[0] == n) {	/* null input--all zero length codes */
		*t = (huft_t *) NULL;
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
				if ((q = (huft_t *) xmalloc((z + 1) * sizeof(huft_t))) == NULL) {
					if (h) {
						huft_free(u[0]);
					}
					return 3;	/* not enough memory */
				}
				hufts += z + 1;	/* track memory usage */
				*t = q + 1;		/* link to list for huft_free() */
				*(t = &(q->v.t)) = NULL;
				u[h] = ++q;		/* table starts after link */

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
			if (p >= v + n)
				r.e = 99;		/* out of values--invalid code */
			else if (*p < s) {
				r.e = (unsigned char) (*p < 256 ? 16 : 15);	/* 256 is end-of-block code */
				r.v.n = (unsigned short) (*p);	/* simple code is just the value */
				p++;			/* one compiler does not like *p++ */
			} else {
				r.e = (unsigned char) e[*p - s];	/* non-simple--look up in lists */
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

/*
 * inflate (decompress) the codes in a deflated (compressed) block.
 * Return an error code or zero if it all goes ok.
 *
 * tl, td: literal/length and distance decoder tables
 * bl, bd: number of bits decoded by tl[] and td[]
 */
static int inflate_codes(huft_t *tl, huft_t *td, int bl, int bd)
{
	register unsigned long e;		/* table entry flag/number of extra bits */
	unsigned long n, d;				/* length and index for copy */
	unsigned long w;				/* current window position */
	huft_t *t;				/* pointer to table entry */
	unsigned ml, md;			/* masks for bl and bd bits */
	register unsigned long b;				/* bit buffer */
	register unsigned k;		/* number of bits in bit buffer */
	register int input_char;

	/* make local copies of globals */
	b = bb;					/* initialize bit buffer */
	k = bk;
	w = outcnt;				/* initialize window position */

	/* inflate the coded data */
	ml = mask_bits[bl];			/* precompute masks for speed */
	md = mask_bits[bd];
	for (;;) {				/* do until end of block */
		while (k < (unsigned) bl) {
			input_char = fgetc(in_file);
			if (input_char == EOF) return 1;
			b |= ((unsigned long)input_char) << k;
			k += 8;
		}
		if ((e = (t = tl + ((unsigned) b & ml))->e) > 16)
		do {
			if (e == 99) {
				return 1;
			}
			b >>= t->b;
			k -= t->b;
			e -= 16;
			while (k < e) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b |= ((unsigned long)input_char) << k;
				k += 8;
			}
		} while ((e = (t = t->v.t + ((unsigned) b & mask_bits[e]))->e) > 16);
		b >>= t->b;
		k -= t->b;
		if (e == 16) {		/* then it's a literal */
			window[w++] = (unsigned char) t->v.n;
			if (w == WSIZE) {
				outcnt=(w),
				flush_window();
				w = 0;
			}
		} else {				/* it's an EOB or a length */

			/* exit if end of block */
			if (e == 15) {
				break;
			}

			/* get length of block to copy */
			while (k < e) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b |= ((unsigned long)input_char) << k;
				k += 8;
			}
			n = t->v.n + ((unsigned) b & mask_bits[e]);
			b >>= e;
			k -= e;

			/* decode distance of block to copy */
			while (k < (unsigned) bd) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b |= ((unsigned long)input_char) << k;
				k += 8;
			}

			if ((e = (t = td + ((unsigned) b & md))->e) > 16)
				do {
					if (e == 99)
						return 1;
					b >>= t->b;
					k -= t->b;
					e -= 16;
					while (k < e) {
						input_char = fgetc(in_file);
						if (input_char == EOF) return 1;
						b |= ((unsigned long)input_char) << k;
						k += 8;
					}
				} while ((e = (t = t->v.t + ((unsigned) b & mask_bits[e]))->e) > 16);
			b >>= t->b;
			k -= t->b;
			while (k < e) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b |= ((unsigned long)input_char) << k;
				k += 8;
			}
			d = w - t->v.n - ((unsigned) b & mask_bits[e]);
			b >>= e;
			k -= e;

			/* do the copy */
			do {
				n -= (e = (e = WSIZE - ((d &= WSIZE - 1) > w ? d : w)) > n ? n : e);
#if !defined(NOMEMCPY) && !defined(DEBUG)
				if (w - d >= e) {	/* (this test assumes unsigned comparison) */
					memcpy(window + w, window + d, e);
					w += e;
					d += e;
				} else			/* do it slow to avoid memcpy() overlap */
#endif							/* !NOMEMCPY */
					do {
						window[w++] = window[d++];
					} while (--e);
				if (w == WSIZE) {
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

/*
 * decompress an inflated block
 * e: last block flag
 *
 * GLOBAL VARIABLES: bb, kk,
 */
static int inflate_block(int *e)
{
	unsigned t;			/* block type */
	register unsigned long b;			/* bit buffer */
	register unsigned k;		/* number of bits in bit buffer */
	static unsigned short cplens[] = {		/* Copy lengths for literal codes 257..285 */
		3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
		35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
	};
	/* note: see note #13 above about the 258 in this list. */
	static unsigned short cplext[] = {		/* Extra bits for literal codes 257..285 */
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
		3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99
	};				/* 99==invalid */
	static unsigned short cpdist[] = {		/* Copy offsets for distance codes 0..29 */
		1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
		257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
		8193, 12289, 16385, 24577
	};
	static unsigned short cpdext[] = {		/* Extra bits for distance codes */
		0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
		7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
		12, 12, 13, 13
	};
	int input_char;

	/* make local bit buffer */
	b = bb;
	k = bk;

	/* read in last block bit */
	while (k < 1) {
		input_char = fgetc(in_file);
		if (input_char == EOF) return 1;
		b |= ((unsigned long)input_char) << k;
		k += 8;
	}
	*e = (int) b & 1;
	b >>= 1;
	k -= 1;

	/* read in block type */
	while (k < 2) {
		input_char = fgetc(in_file);
		if (input_char == EOF) return 1;
		b |= ((unsigned long)input_char) << k;
		k += 8;
	}
	t = (unsigned) b & 3;
	b >>= 2;
	k -= 2;

	/* restore the global bit buffer */
	bb = b;
	bk = k;

	/* inflate that block type */
	switch (t) {
	case 0:	/* Inflate stored */
		{
			unsigned long n;			/* number of bytes in block */
			unsigned long w;			/* current window position */
			register unsigned long b_stored;			/* bit buffer */
			register unsigned long k_stored;		/* number of bits in bit buffer */

			/* make local copies of globals */
			b_stored = bb;				/* initialize bit buffer */
			k_stored = bk;
			w = outcnt;			/* initialize window position */

			/* go to byte boundary */
			n = k_stored & 7;
			b_stored >>= n;
			k_stored -= n;

			/* get the length and its complement */
			while (k_stored < 16) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b_stored |= ((unsigned long)input_char) << k_stored;
				k_stored += 8;
			}
			n = ((unsigned) b_stored & 0xffff);
			b_stored >>= 16;
			k_stored -= 16;
			while (k_stored < 16) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b_stored |= ((unsigned long)input_char) << k_stored;
				k_stored += 8;
			}
			if (n != (unsigned) ((~b_stored) & 0xffff)) {
				return 1;		/* error in compressed data */
			}
			b_stored >>= 16;
			k_stored -= 16;

			/* read and output the compressed data */
			while (n--) {
				while (k_stored < 8) {
					input_char = fgetc(in_file);
					if (input_char == EOF) return 1;
					b_stored |= ((unsigned long)input_char) << k_stored;
					k_stored += 8;
				}
				window[w++] = (unsigned char) b_stored;
				if (w == (unsigned long)WSIZE) {
					outcnt=(w),
					flush_window();
					w = 0;
				}
				b_stored >>= 8;
				k_stored -= 8;
			}

			/* restore the globals from the locals */
			outcnt = w;			/* restore global window pointer */
			bb = b_stored;				/* restore global bit buffer */
			bk = k_stored;
			return 0;
		}
	case 1:	/* Inflate fixed 
			 * decompress an inflated type 1 (fixed Huffman codes) block.  We should
			 * either replace this with a custom decoder, or at least precompute the
			 * Huffman tables.
			 */
		{
			int i;					/* temporary variable */
			huft_t *tl;				/* literal/length code table */
			huft_t *td;				/* distance code table */
			int bl;					/* lookup bits for tl */
			int bd;					/* lookup bits for td */
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
			if (inflate_codes(tl, td, bl, bd))
				return 1;

			/* free the decoding tables, return */
			huft_free(tl);
			huft_free(td);
			return 0;
		}
	case 2:	/* Inflate dynamic */
		{
			/* Tables for deflate from PKZIP's appnote.txt. */
			static unsigned border[] = {	/* Order of the bit length code lengths */
				16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
			};
			int dbits = 6;					/* bits in base distance lookup table */
			int lbits = 9;					/* bits in base literal/length lookup table */

			int i;						/* temporary variables */
			unsigned j;
			unsigned l;					/* last length */
			unsigned m;					/* mask for bit lengths table */
			unsigned n;					/* number of lengths to get */
			huft_t *tl;			/* literal/length code table */
			huft_t *td;			/* distance code table */
			int bl;						/* lookup bits for tl */
			int bd;						/* lookup bits for td */
			unsigned nb;				/* number of bit length codes */
			unsigned nl;				/* number of literal/length codes */
			unsigned nd;				/* number of distance codes */

			unsigned ll[286 + 30];		/* literal/length and distance code lengths */
			register unsigned long b_dynamic;	/* bit buffer */
			register unsigned k_dynamic;		/* number of bits in bit buffer */

			/* make local bit buffer */
			b_dynamic = bb;
			k_dynamic = bk;

			/* read in table lengths */
			while (k_dynamic < 5) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b_dynamic |= ((unsigned long)input_char) << k_dynamic;
				k_dynamic += 8;
			}
			nl = 257 + ((unsigned) b_dynamic & 0x1f);	/* number of literal/length codes */
			b_dynamic >>= 5;
			k_dynamic -= 5;
			while (k_dynamic < 5) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b_dynamic |= ((unsigned long)input_char) << k_dynamic;
				k_dynamic += 8;
			}
			nd = 1 + ((unsigned) b_dynamic & 0x1f);	/* number of distance codes */
			b_dynamic >>= 5;
			k_dynamic -= 5;
			while (k_dynamic < 4) {
				input_char = fgetc(in_file);
				if (input_char == EOF) return 1;
				b_dynamic |= ((unsigned long)input_char) << k_dynamic;
				k_dynamic += 8;
			}
			nb = 4 + ((unsigned) b_dynamic & 0xf);	/* number of bit length codes */
			b_dynamic >>= 4;
			k_dynamic -= 4;
			if (nl > 286 || nd > 30) {
				return 1;	/* bad lengths */
			}

			/* read in bit-length-code lengths */
			for (j = 0; j < nb; j++) {
				while (k_dynamic < 3) {
					input_char = fgetc(in_file);
					if (input_char == EOF) return 1;
					b_dynamic |= ((unsigned long)input_char) << k_dynamic;
					k_dynamic += 8;
				}
				ll[border[j]] = (unsigned) b_dynamic & 7;
				b_dynamic >>= 3;
				k_dynamic -= 3;
			}
			for (; j < 19; j++) {
				ll[border[j]] = 0;
			}

			/* build decoding table for trees--single level, 7 bit lookup */
			bl = 7;
			if ((i = huft_build(ll, 19, 19, NULL, NULL, &tl, &bl)) != 0) {
				if (i == 1) {
					huft_free(tl);
				}
				return i;			/* incomplete code set */
			}

			/* read in literal and distance code lengths */
			n = nl + nd;
			m = mask_bits[bl];
			i = l = 0;
			while ((unsigned) i < n) {
				while (k_dynamic < (unsigned) bl) {
					input_char = fgetc(in_file);
					if (input_char == EOF) return 1;
					b_dynamic |= ((unsigned long)input_char) << k_dynamic;
					k_dynamic += 8;
				}
				j = (td = tl + ((unsigned) b_dynamic & m))->b;
				b_dynamic >>= j;
				k_dynamic -= j;
				j = td->v.n;
				if (j < 16) {			/* length of code in bits (0..15) */
					ll[i++] = l = j;	/* save last length in l */
				}
				else if (j == 16) {		/* repeat last length 3 to 6 times */
					while (k_dynamic < 2) {
						input_char = fgetc(in_file);
						if (input_char == EOF) return 1;
						b_dynamic |= ((unsigned long)input_char) << k_dynamic;
						k_dynamic += 8;
					}
					j = 3 + ((unsigned) b_dynamic & 3);
					b_dynamic >>= 2;
					k_dynamic -= 2;
					if ((unsigned) i + j > n) {
						return 1;
					}
					while (j--) {
						ll[i++] = l;
					}
				} else if (j == 17) {	/* 3 to 10 zero length codes */
					while (k_dynamic < 3) {
						input_char = fgetc(in_file);
						if (input_char == EOF) return 1;
						b_dynamic |= ((unsigned long)input_char) << k_dynamic;
						k_dynamic += 8;
					}
					j = 3 + ((unsigned) b_dynamic & 7);
					b_dynamic >>= 3;
					k_dynamic -= 3;
					if ((unsigned) i + j > n) {
						return 1;
					}
					while (j--) {
						ll[i++] = 0;
					}
					l = 0;
				} else {		/* j == 18: 11 to 138 zero length codes */
					while (k_dynamic < 7) {
						input_char = fgetc(in_file);
						if (input_char == EOF) return 1;
						b_dynamic |= ((unsigned long)input_char) << k_dynamic;
						k_dynamic += 8;
					}
					j = 11 + ((unsigned) b_dynamic & 0x7f);
					b_dynamic >>= 7;
					k_dynamic -= 7;
					if ((unsigned) i + j > n) {
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
			bb = b_dynamic;
			bk = k_dynamic;

			/* build the decoding tables for literal/length and distance codes */
			bl = lbits;
			if ((i = huft_build(ll, nl, 257, cplens, cplext, &tl, &bl)) != 0) {
				if (i == 1) {
					error_msg("Incomplete literal tree");
					huft_free(tl);
				}
				return i;			/* incomplete code set */
			}
			bd = dbits;
			if ((i = huft_build(ll + nl, nd, 0, cpdist, cpdext, &td, &bd)) != 0) {
				if (i == 1) {
					error_msg("incomplete distance tree");
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
	default:
		/* bad block type */
		return 2;
	}
}

/*
 * decompress an inflated entry
 *
 * GLOBAL VARIABLES: outcnt, bk, bb, hufts, inptr
 */
static int inflate()
{
	int e;				/* last block flag */
	int r;				/* result code */
	unsigned h = 0;		/* maximum struct huft's malloc'ed */

	/* initialize window, bit buffer */
	outcnt = 0;
	bk = 0;
	bb = 0;

	/* decompress until the last block */
	do {
		hufts = 0;
		if ((r = inflate_block(&e)) != 0) {
			return r;
		}
		if (hufts > h) {
			h = hufts;
		}
	} while (!e);

	/* Undo too much lookahead.  The next read will be byte aligned so we
	 * can discard unused bits in the last meaningful byte.  */
	while (bk >= 8) {
		bk -= 8;
		ungetc((bb << bk), in_file);
	}

	/* flush out window */
	flush_window();

	/* return success */
	return 0;
}

/* ===========================================================================
 * Unzip in to out.  This routine works on both gzip and pkzip files.
 *
 * IN assertions: the buffer inbuf contains already the beginning of
 *   the compressed data, from offsets inptr to insize-1 included.
 *   The magic header has already been checked. The output buffer is cleared.
 * in, out: input and output file descriptors
 */
extern int unzip(FILE *l_in_file, FILE *l_out_file)
{
	const int extra_field = 0x04;	/* bit 2 set: extra field present */
	const int orig_name = 0x08;	/* bit 3 set: original file name present */
	const int comment = 0x10;	/* bit 4 set: file comment present */
	unsigned char buf[8];	/* extended local header */
	unsigned char flags;	/* compression flags */
	char magic[2];			/* magic header */
	int method;
	typedef void (*sig_type) (int);
	int exit_code=0;	/* program exit code */
	int i;

	in_file = l_in_file;
	out_file = l_out_file;

	if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
		(void) signal(SIGINT, (sig_type) abort_gzip);
	}
#ifdef SIGTERM
//	if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
//		(void) signal(SIGTERM, (sig_type) abort_gzip);
//	}
#endif
#ifdef SIGHUP
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
		(void) signal(SIGHUP, (sig_type) abort_gzip);
	}
#endif

	/* Allocate all global buffers (for DYN_ALLOC option) */
	window = xmalloc((size_t)(((2L*WSIZE)+1L)*sizeof(unsigned char)));
	outcnt = 0;
	bytes_out = 0L;

	magic[0] = fgetc(in_file);
	magic[1] = fgetc(in_file);

	/* Magic header for gzip files, 1F 8B = \037\213 */
	if (memcmp(magic, "\037\213", 2) != 0) {
		error_msg("Invalid gzip magic");
		return EXIT_FAILURE;
	}

	method = (int) fgetc(in_file);
	if (method != 8) {                   /* also catches EOF */
		error_msg("unknown method %d -- get newer version of gzip", method);
		exit_code = 1;
		return -1;
	}

	flags = (unsigned char) fgetc(in_file);

	/* Ignore time stamp(4), extra flags(1), OS type(1) */
	for (i = 0; i < 6; i++)
		fgetc(in_file);

	if ((flags & extra_field) != 0) {
		size_t extra;
		extra = fgetc(in_file);
		extra += fgetc(in_file) << 8;
		if (feof(in_file)) return 1;

		for (i = 0; i < extra; i++)
			fgetc(in_file);
	}

	/* Discard original name if any */
	if ((flags & orig_name) != 0) {
		while (fgetc(in_file) != 0 && !feof(in_file));	/* null */
	}

	/* Discard file comment if any */
	if ((flags & comment) != 0) {
		while (fgetc(in_file) != 0 && !feof(in_file));	/* null */
	}

	if (method < 0) {
		printf("it failed\n");
		return(exit_code);		/* error message already emitted */
	}

	make_crc_table();

	/* Decompress */
	if (method == 8) {

		int res = inflate();

		if (res == 3) {
			error_msg(memory_exhausted);
			exit_code = 1;
		} else if (res != 0) {
			error_msg("invalid compressed data--format violated");
			exit_code = 1;
		}

	} else {
		error_msg("internal error, invalid method");
		exit_code = 1;
	}

	/* Get the crc and original length
	 * crc32  (see algorithm.doc)
	 * uncompressed input size modulo 2^32
	 */
	fread(buf, 1, 8, in_file);

	/* Validate decompression - crc */
	if (!exit_code && (unsigned int)((buf[0] | (buf[1] << 8)) |((buf[2] | (buf[3] << 8)) << 16)) != (crc ^ 0xffffffffL)) {
		error_msg("invalid compressed data--crc error");
		exit_code = 1;
	}
	/* Validate decompression - size */
	if (!exit_code && ((buf[4] | (buf[5] << 8)) |((buf[6] | (buf[7] << 8)) << 16)) != (unsigned long) bytes_out) {
		error_msg("invalid compressed data--length error");
		exit_code = 1;
	}

	free(window);
	free(crc_table);

	return exit_code;
}

/*
 * This needs access to global variables wondow and crc_table, so its not in its own file.
 */
extern void gz_close(int gunzip_pid)
{
	if (kill(gunzip_pid, SIGTERM) == -1) {
		error_msg_and_die("***  Couldnt kill old gunzip process *** aborting");
	}

	if (waitpid(gunzip_pid, NULL, 0) == -1) {
		printf("Couldnt wait ?");
	}
		free(window);
		free(crc_table);
}
