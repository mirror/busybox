/* vi: set sw=4 ts=4: */
/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Charles P. Wright <cpw@unix.asb.com>
 * "this is a stripped down version of gzip I put into busybox, it does
 * only standard in to standard out with -9 compression.  It also requires
 * the zcat module for some important functions."
 *
 * Adjusted further by Erik Andersen <andersen@codepoet.org> to support
 * files as well as stdin/stdout, and to generally behave itself wrt
 * command line handling.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* big objects in bss:
 * 00000020 b bl_count
 * 00000074 b base_length
 * 00000078 b base_dist
 * 00000078 b static_dtree
 * 0000009c b bl_tree
 * 000000f4 b dyn_dtree
 * 00000100 b length_code
 * 00000200 b dist_code
 * 0000023d b depth
 * 00000400 b flag_buf
 * 0000047a b heap
 * 00000480 b static_ltree
 * 000008f4 b dyn_ltree
 */

/* TODO: full support for -v for DESKTOP
 * "/usr/bin/gzip -v a bogus aa" should say:
a:       85.1% -- replaced with a.gz
gzip: bogus: No such file or directory
aa:      85.1% -- replaced with aa.gz
*/

#include "busybox.h"


/* ===========================================================================
 */
//#define DEBUG 1
/* Diagnostic functions */
#ifdef DEBUG
#  define Assert(cond,msg) {if(!(cond)) bb_error_msg(msg);}
#  define Trace(x) fprintf x
#  define Tracev(x) {if (verbose) fprintf x ;}
#  define Tracevv(x) {if (verbose > 1) fprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (verbose > 1 && (c)) fprintf x ;}
#else
#  define Assert(cond,msg)
#  define Trace(x)
#  define Tracev(x)
#  define Tracevv(x)
#  define Tracec(c,x)
#  define Tracecv(c,x)
#endif


/* ===========================================================================
 */
#define SMALL_MEM

/* Compression methods (see algorithm.doc) */
/* Only STORED and DEFLATED are supported by this BusyBox module */
#define STORED      0
/* methods 4 to 7 reserved */
#define DEFLATED    8

#ifndef	INBUFSIZ
#  ifdef SMALL_MEM
#    define INBUFSIZ  0x2000	/* input buffer size */
#  else
#    define INBUFSIZ  0x8000	/* input buffer size */
#  endif
#endif

#ifndef	OUTBUFSIZ
#  ifdef SMALL_MEM
#    define OUTBUFSIZ   8192	/* output buffer size */
#  else
#    define OUTBUFSIZ  16384	/* output buffer size */
#  endif
#endif

#ifndef DIST_BUFSIZE
#  ifdef SMALL_MEM
#    define DIST_BUFSIZE 0x2000	/* buffer for distances, see trees.c */
#  else
#    define DIST_BUFSIZE 0x8000	/* buffer for distances, see trees.c */
#  endif
#endif

/* gzip flag byte */
#define ASCII_FLAG   0x01	/* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02	/* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04	/* bit 2 set: extra field present */
#define ORIG_NAME    0x08	/* bit 3 set: original file name present */
#define COMMENT      0x10	/* bit 4 set: file comment present */
#define RESERVED     0xC0	/* bit 6,7:   reserved */

/* internal file attribute */
#define UNKNOWN 0xffff
#define BINARY  0
#define ASCII   1

#ifndef WSIZE
#  define WSIZE 0x8000  /* window size--must be a power of two, and */
#endif                  /*  at least 32K for zip's deflate method */

#define MIN_MATCH  3
#define MAX_MATCH  258
/* The minimum and maximum match lengths */

#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */

#define MAX_DIST  (WSIZE-MIN_LOOKAHEAD)
/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */

#ifndef MAX_PATH_LEN
#  define MAX_PATH_LEN   1024	/* max pathname length */
#endif

#define seekable()    0	/* force sequential output */
#define translate_eol 0	/* no option -a yet */

#ifndef BITS
#  define BITS 16
#endif
#define INIT_BITS 9		/* Initial number of bits per code */

#define BIT_MASK    0x1f	/* Mask for 'number of compression bits' */
/* Mask 0x20 is reserved to mean a fourth header byte, and 0x40 is free.
 * It's a pity that old uncompress does not check bit 0x20. That makes
 * extension of the format actually undesirable because old compress
 * would just crash on the new format instead of giving a meaningful
 * error message. It does check the number of bits, but it's more
 * helpful to say "unsupported format, get a new version" than
 * "can only handle 16 bits".
 */

#ifdef MAX_EXT_CHARS
#  define MAX_SUFFIX  MAX_EXT_CHARS
#else
#  define MAX_SUFFIX  30
#endif


/* ===========================================================================
 * Compile with MEDIUM_MEM to reduce the memory requirements or
 * with SMALL_MEM to use as little memory as possible. Use BIG_MEM if the
 * entire input file can be held in memory (not possible on 16 bit systems).
 * Warning: defining these symbols affects HASH_BITS (see below) and thus
 * affects the compression ratio. The compressed output
 * is still correct, and might even be smaller in some cases.
 */

#ifdef SMALL_MEM
#   define HASH_BITS  13	/* Number of bits used to hash strings */
#endif
#ifdef MEDIUM_MEM
#   define HASH_BITS  14
#endif
#ifndef HASH_BITS
#   define HASH_BITS  15
   /* For portability to 16 bit machines, do not use values above 15. */
#endif

#define HASH_SIZE (unsigned)(1<<HASH_BITS)
#define HASH_MASK (HASH_SIZE-1)
#define WMASK     (WSIZE-1)
/* HASH_SIZE and WSIZE must be powers of two */
#ifndef TOO_FAR
#  define TOO_FAR 4096
#endif
/* Matches of length 3 are discarded if their distance exceeds TOO_FAR */


/* ===========================================================================
 * These types are not really 'char', 'short' and 'long'
 */
typedef uint8_t uch;
typedef uint16_t ush;
typedef uint32_t ulg;
typedef int32_t lng;


/* ===========================================================================
 */
typedef ush Pos;
typedef unsigned IPos;

/* A Pos is an index in the character window. We use short instead of int to
 * save space in the various tables. IPos is used only for parameter passing.
 */

static lng block_start;

/* window position at the beginning of the current output block. Gets
 * negative when the window is moved backwards.
 */

static unsigned ins_h;	/* hash index of string to be inserted */

#define H_SHIFT  ((HASH_BITS+MIN_MATCH-1) / MIN_MATCH)
/* Number of bits by which ins_h and del_h must be shifted at each
 * input step. It must be such that after MIN_MATCH steps, the oldest
 * byte no longer takes part in the hash key, that is:
 * H_SHIFT * MIN_MATCH >= HASH_BITS
 */

static unsigned int prev_length;

/* Length of the best match at previous step. Matches not greater than this
 * are discarded. This is used in the lazy match evaluation.
 */

static unsigned strstart;	/* start of string to insert */
static unsigned match_start;	/* start of matching string */
static int eofile;		/* flag set at end of input file */
static unsigned lookahead;	/* number of valid bytes ahead in window */

enum {
	WINDOW_SIZE = 2 * WSIZE,
/* window size, 2*WSIZE except for MMAP or BIG_MEM, where it is the
 * input file length plus MIN_LOOKAHEAD.
 */

	max_chain_length = 4096,
/* To speed up deflation, hash chains are never searched beyond this length.
 * A higher limit improves compression ratio but degrades the speed.
 */

	max_lazy_match = 258,
/* Attempt to find a better match only when the current match is strictly
 * smaller than this value. This mechanism is used only for compression
 * levels >= 4.
 */

	max_insert_length = max_lazy_match,
/* Insert new strings in the hash table only if the match length
 * is not greater than this length. This saves time but degrades compression.
 * max_insert_length is used only for compression levels <= 3.
 */

	good_match = 32,
/* Use a faster search when the previous match is longer than this */

/* Values for max_lazy_match, good_match and max_chain_length, depending on
 * the desired pack level (0..9). The values given below have been tuned to
 * exclude worst case performance for pathological files. Better values may be
 * found for specific files.
 */

	nice_match = 258,	/* Stop searching when current match exceeds this */
/* Note: the deflate() code requires max_lazy >= MIN_MATCH and max_chain >= 4
 * For deflate_fast() (levels <= 3) good is ignored and lazy has a different
 * meaning.
 */
};


/* ===========================================================================
 */
#define DECLARE(type, array, size) \
	static type * array

#define ALLOC(type, array, size) \
{ \
	array = xzalloc((size_t)(((size)+1L)/2) * 2*sizeof(type)); \
}

#define FREE(array) \
{ \
	free(array); \
	array = NULL; \
}

/* global buffers */

/* buffer for literals or lengths */
/* DECLARE(uch, l_buf, LIT_BUFSIZE); */
DECLARE(uch, l_buf, INBUFSIZ);

DECLARE(ush, d_buf, DIST_BUFSIZE);
DECLARE(uch, outbuf, OUTBUFSIZ);

/* Sliding window. Input bytes are read into the second half of the window,
 * and move to the first half later to keep a dictionary of at least WSIZE
 * bytes. With this organization, matches are limited to a distance of
 * WSIZE-MAX_MATCH bytes, but this ensures that IO is always
 * performed with a length multiple of the block size. Also, it limits
 * the window size to 64K, which is quite useful on MSDOS.
 * To do: limit the window size to WSIZE+BSZ if SMALL_MEM (the code would
 * be less efficient).
 */
DECLARE(uch, window, 2L * WSIZE);

/* Link to older string with same hash index. To limit the size of this
 * array to 64K, this link is maintained only for the last 32K strings.
 * An index in this array is thus a window index modulo 32K.
 */
/* DECLARE(Pos, prev, WSIZE); */
DECLARE(ush, prev, 1L << BITS);

/* Heads of the hash chains or 0. */
/* DECLARE(Pos, head, 1<<HASH_BITS); */
#define head (prev+WSIZE) /* hash head (see deflate.c) */


/* number of input bytes */
static ulg isize;		/* only 32 bits stored in .gz file */

static int foreground;		/* set if program run in foreground */
static int method = DEFLATED;	/* compression method */
static int exit_code;		/* program exit code */

/* original time stamp (modification time) */
static ulg time_stamp;		/* only 32 bits stored in .gz file */

static int ifd;			/* input file descriptor */
static int ofd;			/* output file descriptor */
#ifdef DEBUG
static unsigned insize;	/* valid bytes in l_buf */
#endif
static unsigned outcnt;	/* bytes in output buffer */

static uint32_t *crc_32_tab;


/* ===========================================================================
 * Local data used by the "bit string" routines.
 */

//// static int zfile;	/* output gzip file */

static unsigned short bi_buf;

/* Output buffer. bits are inserted starting at the bottom (least significant
 * bits).
 */

#undef BUF_SIZE
#define BUF_SIZE (8 * sizeof(bi_buf))
/* Number of bits used within bi_buf. (bi_buf might be implemented on
 * more than 16 bits on some systems.)
 */

static int bi_valid;

/* Current input function. Set to mem_read for in-memory compression */

#ifdef DEBUG
static ulg bits_sent;			/* bit length of the compressed data */
#endif


/* ===========================================================================
 * Write the output buffer outbuf[0..outcnt-1] and update bytes_out.
 * (used for the compressed data only)
 */
static void flush_outbuf(void)
{
	if (outcnt == 0)
		return;

	xwrite(ofd, (char *) outbuf, outcnt);
	outcnt = 0;
}


/* ===========================================================================
 */
/* put_8bit is used for the compressed output */
#define put_8bit(c) \
{ \
	outbuf[outcnt++] = (c); \
	if (outcnt == OUTBUFSIZ) flush_outbuf(); \
}

/* Output a 16 bit value, lsb first */
static void put_16bit(ush w)
{
	if (outcnt < OUTBUFSIZ - 2) {
		outbuf[outcnt++] = w;
		outbuf[outcnt++] = w >> 8;
	} else {
		put_8bit(w);
		put_8bit(w >> 8);
	}
}

static void put_32bit(ulg n)
{
	put_16bit(n);
	put_16bit(n >> 16);
}

/* ===========================================================================
 * Clear input and output buffers
 */
static void clear_bufs(void)
{
	outcnt = 0;
#ifdef DEBUG
	insize = 0;
#endif
	isize = 0;
}


/* ===========================================================================
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
static uint32_t crc;	/* shift register contents */
static uint32_t updcrc(uch * s, unsigned n)
{
	uint32_t c = crc;
	while (n) {
		c = crc_32_tab[(uch)(c ^ *s++)] ^ (c >> 8);
		n--;
	}
	crc = c;
	return c;
}


/* ===========================================================================
 * Read a new buffer from the current input file, perform end-of-line
 * translation, and update the crc and input file size.
 * IN assertion: size >= 2 (for end-of-line translation)
 */
static unsigned file_read(void *buf, unsigned size)
{
	unsigned len;

	Assert(insize == 0, "l_buf not empty");

	len = safe_read(ifd, buf, size);
	if (len == (unsigned)(-1) || len == 0)
		return len;

	updcrc(buf, len);
	isize += len;
	return len;
}


/* ===========================================================================
 * Send a value on a given number of bits.
 * IN assertion: length <= 16 and value fits in length bits.
 */
static void send_bits(int value, int length)
{
#ifdef DEBUG
	Tracev((stderr, " l %2d v %4x ", length, value));
	Assert(length > 0 && length <= 15, "invalid length");
	bits_sent += length;
#endif
	/* If not enough room in bi_buf, use (valid) bits from bi_buf and
	 * (16 - bi_valid) bits from value, leaving (width - (16-bi_valid))
	 * unused bits in value.
	 */
	if (bi_valid > (int) BUF_SIZE - length) {
		bi_buf |= (value << bi_valid);
		put_16bit(bi_buf);
		bi_buf = (ush) value >> (BUF_SIZE - bi_valid);
		bi_valid += length - BUF_SIZE;
	} else {
		bi_buf |= value << bi_valid;
		bi_valid += length;
	}
}


/* ===========================================================================
 * Reverse the first len bits of a code, using straightforward code (a faster
 * method would use a table)
 * IN assertion: 1 <= len <= 15
 */
static unsigned bi_reverse(unsigned code, int len)
{
	unsigned res = 0;

	while (1) {
		res |= code & 1;
		if (--len <= 0) return res;
		code >>= 1;
		res <<= 1;
	}
}


/* ===========================================================================
 * Write out any remaining bits in an incomplete byte.
 */
static void bi_windup(void)
{
	if (bi_valid > 8) {
		put_16bit(bi_buf);
	} else if (bi_valid > 0) {
		put_8bit(bi_buf);
	}
	bi_buf = 0;
	bi_valid = 0;
#ifdef DEBUG
	bits_sent = (bits_sent + 7) & ~7;
#endif
}


/* ===========================================================================
 * Copy a stored block to the zip file, storing first the length and its
 * one's complement if requested.
 */
static void copy_block(char *buf, unsigned len, int header)
{
	bi_windup();		/* align on byte boundary */

	if (header) {
		put_16bit(len);
		put_16bit(~len);
#ifdef DEBUG
		bits_sent += 2 * 16;
#endif
	}
#ifdef DEBUG
	bits_sent += (ulg) len << 3;
#endif
	while (len--) {
		put_8bit(*buf++);
	}
}


/* ===========================================================================
 * Fill the window when the lookahead becomes insufficient.
 * Updates strstart and lookahead, and sets eofile if end of input file.
 * IN assertion: lookahead < MIN_LOOKAHEAD && strstart + lookahead > 0
 * OUT assertions: at least one byte has been read, or eofile is set;
 *    file reads are performed for at least two bytes (required for the
 *    translate_eol option).
 */
static void fill_window(void)
{
	unsigned n, m;
	unsigned more =	WINDOW_SIZE - lookahead - strstart;
	/* Amount of free space at the end of the window. */

	/* If the window is almost full and there is insufficient lookahead,
	 * move the upper half to the lower one to make room in the upper half.
	 */
	if (more == (unsigned) -1) {
		/* Very unlikely, but possible on 16 bit machine if strstart == 0
		 * and lookahead == 1 (input done one byte at time)
		 */
		more--;
	} else if (strstart >= WSIZE + MAX_DIST) {
		/* By the IN assertion, the window is not empty so we can't confuse
		 * more == 0 with more == 64K on a 16 bit machine.
		 */
		Assert(WINDOW_SIZE == 2 * WSIZE, "no sliding with BIG_MEM");

		memcpy(window, window + WSIZE, WSIZE);
		match_start -= WSIZE;
		strstart -= WSIZE;	/* we now have strstart >= MAX_DIST: */

		block_start -= WSIZE;

		for (n = 0; n < HASH_SIZE; n++) {
			m = head[n];
			head[n] = (Pos) (m >= WSIZE ? m - WSIZE : 0);
		}
		for (n = 0; n < WSIZE; n++) {
			m = prev[n];
			prev[n] = (Pos) (m >= WSIZE ? m - WSIZE : 0);
			/* If n is not on any hash chain, prev[n] is garbage but
			 * its value will never be used.
			 */
		}
		more += WSIZE;
	}
	/* At this point, more >= 2 */
	if (!eofile) {
		n = file_read(window + strstart + lookahead, more);
		if (n == 0 || n == (unsigned) -1) {
			eofile = 1;
		} else {
			lookahead += n;
		}
	}
}


/* ===========================================================================
 * Set match_start to the longest match starting at the given string and
 * return its length. Matches shorter or equal to prev_length are discarded,
 * in which case the result is equal to prev_length and match_start is
 * garbage.
 * IN assertions: cur_match is the head of the hash chain for the current
 *   string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
 */

/* For MSDOS, OS/2 and 386 Unix, an optimized version is in match.asm or
 * match.s. The code is functionally equivalent, so you can use the C version
 * if desired.
 */
static int longest_match(IPos cur_match)
{
	unsigned chain_length = max_chain_length;	/* max hash chain length */
	uch *scan = window + strstart;	/* current string */
	uch *match;	/* matched string */
	int len;	/* length of current match */
	int best_len = prev_length;	/* best match length so far */
	IPos limit = strstart > (IPos) MAX_DIST ? strstart - (IPos) MAX_DIST : 0;
	/* Stop when cur_match becomes <= limit. To simplify the code,
	 * we prevent matches with the string of window index 0.
	 */

/* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
 * It is easy to get rid of this optimization if necessary.
 */
#if HASH_BITS < 8 || MAX_MATCH != 258
#  error Code too clever
#endif
	uch *strend = window + strstart + MAX_MATCH;
	uch scan_end1 = scan[best_len - 1];
	uch scan_end = scan[best_len];

	/* Do not waste too much time if we already have a good match: */
	if (prev_length >= good_match) {
		chain_length >>= 2;
	}
	Assert(strstart <= WINDOW_SIZE - MIN_LOOKAHEAD, "insufficient lookahead");

	do {
		Assert(cur_match < strstart, "no future");
		match = window + cur_match;

		/* Skip to next match if the match length cannot increase
		 * or if the match length is less than 2:
		 */
		if (match[best_len] != scan_end ||
			match[best_len - 1] != scan_end1 ||
			*match != *scan || *++match != scan[1])
			continue;

		/* The check at best_len-1 can be removed because it will be made
		 * again later. (This heuristic is not always a win.)
		 * It is not necessary to compare scan[2] and match[2] since they
		 * are always equal when the other bytes match, given that
		 * the hash keys are equal and that HASH_BITS >= 8.
		 */
		scan += 2, match++;

		/* We check for insufficient lookahead only every 8th comparison;
		 * the 256th check will be made at strstart+258.
		 */
		do {
		} while (*++scan == *++match && *++scan == *++match &&
				 *++scan == *++match && *++scan == *++match &&
				 *++scan == *++match && *++scan == *++match &&
				 *++scan == *++match && *++scan == *++match && scan < strend);

		len = MAX_MATCH - (int) (strend - scan);
		scan = strend - MAX_MATCH;

		if (len > best_len) {
			match_start = cur_match;
			best_len = len;
			if (len >= nice_match)
				break;
			scan_end1 = scan[best_len - 1];
			scan_end = scan[best_len];
		}
	} while ((cur_match = prev[cur_match & WMASK]) > limit
			 && --chain_length != 0);

	return best_len;
}


#ifdef DEBUG
/* ===========================================================================
 * Check that the match at match_start is indeed a match.
 */
static void check_match(IPos start, IPos match, int length)
{
	/* check that the match is indeed a match */
	if (memcmp(window + match, window + start, length) != 0) {
		bb_error_msg(" start %d, match %d, length %d", start, match, length);
		bb_error_msg("invalid match");
	}
	if (verbose > 1) {
		bb_error_msg("\\[%d,%d]", start - match, length);
		do {
			putc(window[start++], stderr);
		} while (--length != 0);
	}
}
#else
#  define check_match(start, match, length) ((void)0)
#endif


/* trees.c -- output deflated data using Huffman coding
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

/*  PURPOSE
 *      Encode various sets of source values using variable-length
 *      binary code trees.
 *
 *  DISCUSSION
 *      The PKZIP "deflation" process uses several Huffman trees. The more
 *      common source values are represented by shorter bit sequences.
 *
 *      Each code tree is stored in the ZIP file in a compressed form
 *      which is itself a Huffman encoding of the lengths of
 *      all the code strings (in ascending order by source values).
 *      The actual code strings are reconstructed from the lengths in
 *      the UNZIP process, as described in the "application note"
 *      (APPNOTE.TXT) distributed as part of PKWARE's PKZIP program.
 *
 *  REFERENCES
 *      Lynch, Thomas J.
 *          Data Compression:  Techniques and Applications, pp. 53-55.
 *          Lifetime Learning Publications, 1985.  ISBN 0-534-03418-7.
 *
 *      Storer, James A.
 *          Data Compression:  Methods and Theory, pp. 49-50.
 *          Computer Science Press, 1988.  ISBN 0-7167-8156-5.
 *
 *      Sedgewick, R.
 *          Algorithms, p290.
 *          Addison-Wesley, 1983. ISBN 0-201-06672-6.
 *
 *  INTERFACE
 *      void ct_init(ush *attr, int *methodp)
 *          Allocate the match buffer, initialize the various tables and save
 *          the location of the internal file attribute (ascii/binary) and
 *          method (DEFLATE/STORE)
 *
 *      void ct_tally(int dist, int lc);
 *          Save the match info and tally the frequency counts.
 *
 *      ulg flush_block(char *buf, ulg stored_len, int eof)
 *          Determine the best encoding for the current block: dynamic trees,
 *          static trees or store, and output the encoded block to the zip
 *          file. Returns the total compressed length for the file so far.
 */

#define MAX_BITS 15
/* All codes must not exceed MAX_BITS bits */

#define MAX_BL_BITS 7
/* Bit length codes must not exceed MAX_BL_BITS bits */

#define LENGTH_CODES 29
/* number of length codes, not counting the special END_BLOCK code */

#define LITERALS  256
/* number of literal bytes 0..255 */

#define END_BLOCK 256
/* end of block literal code */

#define L_CODES (LITERALS+1+LENGTH_CODES)
/* number of Literal or Length codes, including the END_BLOCK code */

#define D_CODES   30
/* number of distance codes */

#define BL_CODES  19
/* number of codes used to transfer the bit lengths */

typedef uch extra_bits_t;

/* extra bits for each length code */
static const extra_bits_t extra_lbits[LENGTH_CODES]= {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
	4, 4, 5, 5, 5, 5, 0
};

/* extra bits for each distance code */
static const extra_bits_t extra_dbits[D_CODES] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
	10, 10, 11, 11, 12, 12, 13, 13
};

/* extra bits for each bit length code */
static const extra_bits_t extra_blbits[BL_CODES] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7 };

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
/* The three kinds of block type */

#ifndef LIT_BUFSIZE
#  ifdef SMALL_MEM
#    define LIT_BUFSIZE  0x2000
#  else
#  ifdef MEDIUM_MEM
#    define LIT_BUFSIZE  0x4000
#  else
#    define LIT_BUFSIZE  0x8000
#  endif
#  endif
#endif
#ifndef DIST_BUFSIZE
#  define DIST_BUFSIZE  LIT_BUFSIZE
#endif
/* Sizes of match buffers for literals/lengths and distances.  There are
 * 4 reasons for limiting LIT_BUFSIZE to 64K:
 *   - frequencies can be kept in 16 bit counters
 *   - if compression is not successful for the first block, all input data is
 *     still in the window so we can still emit a stored block even when input
 *     comes from standard input.  (This can also be done for all blocks if
 *     LIT_BUFSIZE is not greater than 32K.)
 *   - if compression is not successful for a file smaller than 64K, we can
 *     even emit a stored file instead of a stored block (saving 5 bytes).
 *   - creating new Huffman trees less frequently may not provide fast
 *     adaptation to changes in the input data statistics. (Take for
 *     example a binary file with poorly compressible code followed by
 *     a highly compressible string table.) Smaller buffer sizes give
 *     fast adaptation but have of course the overhead of transmitting trees
 *     more frequently.
 *   - I can't count above 4
 * The current code is general and allows DIST_BUFSIZE < LIT_BUFSIZE (to save
 * memory at the expense of compression). Some optimizations would be possible
 * if we rely on DIST_BUFSIZE == LIT_BUFSIZE.
 */
#define REP_3_6      16
/* repeat previous bit length 3-6 times (2 bits of repeat count) */
#define REPZ_3_10    17
/* repeat a zero length 3-10 times  (3 bits of repeat count) */
#define REPZ_11_138  18
/* repeat a zero length 11-138 times  (7 bits of repeat count) */

/* ===========================================================================
*/
/* Data structure describing a single value and its code string. */
typedef struct ct_data {
	union {
		ush freq;		/* frequency count */
		ush code;		/* bit string */
	} fc;
	union {
		ush dad;		/* father node in Huffman tree */
		ush len;		/* length of bit string */
	} dl;
} ct_data;

#define Freq fc.freq
#define Code fc.code
#define Dad  dl.dad
#define Len  dl.len

#define HEAP_SIZE (2*L_CODES + 1)
/* maximum heap size */

////static int heap[HEAP_SIZE];     /* heap used to build the Huffman trees */
////let's try this
static ush heap[HEAP_SIZE];     /* heap used to build the Huffman trees */
static int heap_len;            /* number of elements in the heap */
static int heap_max;            /* element of largest frequency */

/* The sons of heap[n] are heap[2*n] and heap[2*n+1]. heap[0] is not used.
 * The same heap array is used to build all trees.
 */

static ct_data dyn_ltree[HEAP_SIZE];	/* literal and length tree */
static ct_data dyn_dtree[2 * D_CODES + 1];	/* distance tree */

static ct_data static_ltree[L_CODES + 2];

/* The static literal tree. Since the bit lengths are imposed, there is no
 * need for the L_CODES extra codes used during heap construction. However
 * The codes 286 and 287 are needed to build a canonical tree (see ct_init
 * below).
 */

static ct_data static_dtree[D_CODES];

/* The static distance tree. (Actually a trivial tree since all codes use
 * 5 bits.)
 */

static ct_data bl_tree[2 * BL_CODES + 1];

/* Huffman tree for the bit lengths */

typedef struct tree_desc {
	ct_data *dyn_tree;	/* the dynamic tree */
	ct_data *static_tree;	/* corresponding static tree or NULL */
	const extra_bits_t *extra_bits;	/* extra bits for each code or NULL */
	int extra_base;		/* base index for extra_bits */
	int elems;			/* max number of elements in the tree */
	int max_length;		/* max bit length for the codes */
	int max_code;		/* largest code with non zero frequency */
} tree_desc;

static tree_desc l_desc = {
	dyn_ltree, static_ltree, extra_lbits,
	LITERALS + 1, L_CODES, MAX_BITS, 0
};

static tree_desc d_desc = {
	dyn_dtree, static_dtree, extra_dbits, 0, D_CODES, MAX_BITS, 0
};

static tree_desc bl_desc = {
	bl_tree, NULL, extra_blbits, 0, BL_CODES, MAX_BL_BITS,	0
};


static ush bl_count[MAX_BITS + 1];

/* number of codes at each bit length for an optimal tree */

static const uch bl_order[BL_CODES] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

/* The lengths of the bit length codes are sent in order of decreasing
 * probability, to avoid transmitting the lengths for unused bit length codes.
 */

static uch depth[2 * L_CODES + 1];

/* Depth of each subtree used as tie breaker for trees of equal frequency */

static uch length_code[MAX_MATCH - MIN_MATCH + 1];

/* length code for each normalized match length (0 == MIN_MATCH) */

static uch dist_code[512];

/* distance codes. The first 256 values correspond to the distances
 * 3 .. 258, the last 256 values correspond to the top 8 bits of
 * the 15 bit distances.
 */

static int base_length[LENGTH_CODES];

/* First normalized length for each code (0 = MIN_MATCH) */

static int base_dist[D_CODES];

/* First normalized distance for each code (0 = distance of 1) */

static uch flag_buf[LIT_BUFSIZE / 8];

/* flag_buf is a bit array distinguishing literals from lengths in
 * l_buf, thus indicating the presence or absence of a distance.
 */

static unsigned last_lit;       /* running index in l_buf */
static unsigned last_dist;      /* running index in d_buf */
static unsigned last_flags;     /* running index in flag_buf */
static uch flags;               /* current flags not yet saved in flag_buf */
static uch flag_bit;            /* current bit used in flags */

/* bits are filled in flags starting at bit 0 (least significant).
 * Note: these flags are overkill in the current code since we don't
 * take advantage of DIST_BUFSIZE == LIT_BUFSIZE.
 */

static ulg opt_len;             /* bit length of current block with optimal trees */
static ulg static_len;          /* bit length of current block with static trees */

static ulg compressed_len;      /* total bit length of compressed file */

static ush *file_type;          /* pointer to UNKNOWN, BINARY or ASCII */
static int *file_method;        /* pointer to DEFLATE or STORE */

/* ===========================================================================
 */
static void gen_codes(ct_data * tree, int max_code);
static void build_tree(tree_desc * desc);
static void scan_tree(ct_data * tree, int max_code);
static void send_tree(ct_data * tree, int max_code);
static int build_bl_tree(void);
static void send_all_trees(int lcodes, int dcodes, int blcodes);
static void compress_block(ct_data * ltree, ct_data * dtree);


#ifndef DEBUG
/* Send a code of the given tree. c and tree must not have side effects */
#  define SEND_CODE(c, tree) send_bits(tree[c].Code, tree[c].Len)
#else
#  define SEND_CODE(c, tree) \
{ \
	if (verbose > 1) bb_error_msg("\ncd %3d ",(c)); \
	send_bits(tree[c].Code, tree[c].Len); \
}
#endif

#define D_CODE(dist) \
	((dist) < 256 ? dist_code[dist] : dist_code[256 + ((dist)>>7)])
/* Mapping from a distance to a distance code. dist is the distance - 1 and
 * must not have side effects. dist_code[256] and dist_code[257] are never
 * used.
 * The arguments must not have side effects.
 */


/* ===========================================================================
 * Initialize a new block.
 */
static void init_block(void)
{
	int n; /* iterates over tree elements */

	/* Initialize the trees. */
	for (n = 0; n < L_CODES; n++)
		dyn_ltree[n].Freq = 0;
	for (n = 0; n < D_CODES; n++)
		dyn_dtree[n].Freq = 0;
	for (n = 0; n < BL_CODES; n++)
		bl_tree[n].Freq = 0;

	dyn_ltree[END_BLOCK].Freq = 1;
	opt_len = static_len = 0;
	last_lit = last_dist = last_flags = 0;
	flags = 0;
	flag_bit = 1;
}


/* ===========================================================================
 * Restore the heap property by moving down the tree starting at node k,
 * exchanging a node with the smallest of its two sons if necessary, stopping
 * when the heap property is re-established (each father smaller than its
 * two sons).
 */

/* Compares to subtrees, using the tree depth as tie breaker when
 * the subtrees have equal frequency. This minimizes the worst case length. */
#define SMALLER(tree, n, m) \
	(tree[n].Freq < tree[m].Freq \
	|| (tree[n].Freq == tree[m].Freq && depth[n] <= depth[m]))

static void pqdownheap(ct_data * tree, int k)
{
	int v = heap[k];
	int j = k << 1;		/* left son of k */

	while (j <= heap_len) {
		/* Set j to the smallest of the two sons: */
		if (j < heap_len && SMALLER(tree, heap[j + 1], heap[j]))
			j++;

		/* Exit if v is smaller than both sons */
		if (SMALLER(tree, v, heap[j]))
			break;

		/* Exchange v with the smallest son */
		heap[k] = heap[j];
		k = j;

		/* And continue down the tree, setting j to the left son of k */
		j <<= 1;
	}
	heap[k] = v;
}


/* ===========================================================================
 * Compute the optimal bit lengths for a tree and update the total bit length
 * for the current block.
 * IN assertion: the fields freq and dad are set, heap[heap_max] and
 *    above are the tree nodes sorted by increasing frequency.
 * OUT assertions: the field len is set to the optimal bit length, the
 *     array bl_count contains the frequencies for each bit length.
 *     The length opt_len is updated; static_len is also updated if stree is
 *     not null.
 */
static void gen_bitlen(tree_desc * desc)
{
	ct_data *tree = desc->dyn_tree;
	const extra_bits_t *extra = desc->extra_bits;
	int base = desc->extra_base;
	int max_code = desc->max_code;
	int max_length = desc->max_length;
	ct_data *stree = desc->static_tree;
	int h;				/* heap index */
	int n, m;			/* iterate over the tree elements */
	int bits;			/* bit length */
	int xbits;			/* extra bits */
	ush f;				/* frequency */
	int overflow = 0;	/* number of elements with bit length too large */

	for (bits = 0; bits <= MAX_BITS; bits++)
		bl_count[bits] = 0;

	/* In a first pass, compute the optimal bit lengths (which may
	 * overflow in the case of the bit length tree).
	 */
	tree[heap[heap_max]].Len = 0;	/* root of the heap */

	for (h = heap_max + 1; h < HEAP_SIZE; h++) {
		n = heap[h];
		bits = tree[tree[n].Dad].Len + 1;
		if (bits > max_length) {
			bits = max_length;
			overflow++;
		}
		tree[n].Len = (ush) bits;
		/* We overwrite tree[n].Dad which is no longer needed */

		if (n > max_code)
			continue;	/* not a leaf node */

		bl_count[bits]++;
		xbits = 0;
		if (n >= base)
			xbits = extra[n - base];
		f = tree[n].Freq;
		opt_len += (ulg) f *(bits + xbits);

		if (stree)
			static_len += (ulg) f * (stree[n].Len + xbits);
	}
	if (overflow == 0)
		return;

	Trace((stderr, "\nbit length overflow\n"));
	/* This happens for example on obj2 and pic of the Calgary corpus */

	/* Find the first bit length which could increase: */
	do {
		bits = max_length - 1;
		while (bl_count[bits] == 0)
			bits--;
		bl_count[bits]--;	/* move one leaf down the tree */
		bl_count[bits + 1] += 2;	/* move one overflow item as its brother */
		bl_count[max_length]--;
		/* The brother of the overflow item also moves one step up,
		 * but this does not affect bl_count[max_length]
		 */
		overflow -= 2;
	} while (overflow > 0);

	/* Now recompute all bit lengths, scanning in increasing frequency.
	 * h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
	 * lengths instead of fixing only the wrong ones. This idea is taken
	 * from 'ar' written by Haruhiko Okumura.)
	 */
	for (bits = max_length; bits != 0; bits--) {
		n = bl_count[bits];
		while (n != 0) {
			m = heap[--h];
			if (m > max_code)
				continue;
			if (tree[m].Len != (unsigned) bits) {
				Trace((stderr, "code %d bits %d->%d\n", m, tree[m].Len, bits));
				opt_len += ((int32_t) bits - tree[m].Len) * tree[m].Freq;
				tree[m].Len = bits;
			}
			n--;
		}
	}
}


/* ===========================================================================
 * Generate the codes for a given tree and bit counts (which need not be
 * optimal).
 * IN assertion: the array bl_count contains the bit length statistics for
 * the given tree and the field len is set for all tree elements.
 * OUT assertion: the field code is set for all tree elements of non
 *     zero code length.
 */
static void gen_codes(ct_data * tree, int max_code)
{
	ush next_code[MAX_BITS + 1];	/* next code value for each bit length */
	ush code = 0;		/* running code value */
	int bits;			/* bit index */
	int n;				/* code index */

	/* The distribution counts are first used to generate the code values
	 * without bit reversal.
	 */
	for (bits = 1; bits <= MAX_BITS; bits++) {
		next_code[bits] = code = (code + bl_count[bits - 1]) << 1;
	}
	/* Check that the bit counts in bl_count are consistent. The last code
	 * must be all ones.
	 */
	Assert(code + bl_count[MAX_BITS] - 1 == (1 << MAX_BITS) - 1,
		   "inconsistent bit counts");
	Tracev((stderr, "\ngen_codes: max_code %d ", max_code));

	for (n = 0; n <= max_code; n++) {
		int len = tree[n].Len;

		if (len == 0)
			continue;
		/* Now reverse the bits */
		tree[n].Code = bi_reverse(next_code[len]++, len);

		Tracec(tree != static_ltree,
			   (stderr, "\nn %3d %c l %2d c %4x (%x) ", n,
				(isgraph(n) ? n : ' '), len, tree[n].Code,
				next_code[len] - 1));
	}
}


/* ===========================================================================
 * Construct one Huffman tree and assigns the code bit strings and lengths.
 * Update the total bit length for the current block.
 * IN assertion: the field freq is set for all tree elements.
 * OUT assertions: the fields len and code are set to the optimal bit length
 *     and corresponding code. The length opt_len is updated; static_len is
 *     also updated if stree is not null. The field max_code is set.
 */

/* Remove the smallest element from the heap and recreate the heap with
 * one less element. Updates heap and heap_len. */

#define SMALLEST 1
/* Index within the heap array of least frequent node in the Huffman tree */

#define PQREMOVE(tree, top) \
{ \
	top = heap[SMALLEST]; \
	heap[SMALLEST] = heap[heap_len--]; \
	pqdownheap(tree, SMALLEST); \
}

static void build_tree(tree_desc * desc)
{
	ct_data *tree = desc->dyn_tree;
	ct_data *stree = desc->static_tree;
	int elems = desc->elems;
	int n, m;			/* iterate over heap elements */
	int max_code = -1;	/* largest code with non zero frequency */
	int node = elems;	/* next internal node of the tree */

	/* Construct the initial heap, with least frequent element in
	 * heap[SMALLEST]. The sons of heap[n] are heap[2*n] and heap[2*n+1].
	 * heap[0] is not used.
	 */
	heap_len = 0, heap_max = HEAP_SIZE;

	for (n = 0; n < elems; n++) {
		if (tree[n].Freq != 0) {
			heap[++heap_len] = max_code = n;
			depth[n] = 0;
		} else {
			tree[n].Len = 0;
		}
	}

	/* The pkzip format requires that at least one distance code exists,
	 * and that at least one bit should be sent even if there is only one
	 * possible code. So to avoid special checks later on we force at least
	 * two codes of non zero frequency.
	 */
	while (heap_len < 2) {
		int new = heap[++heap_len] = (max_code < 2 ? ++max_code : 0);

		tree[new].Freq = 1;
		depth[new] = 0;
		opt_len--;
		if (stree)
			static_len -= stree[new].Len;
		/* new is 0 or 1 so it does not have extra bits */
	}
	desc->max_code = max_code;

	/* The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree,
	 * establish sub-heaps of increasing lengths:
	 */
	for (n = heap_len / 2; n >= 1; n--)
		pqdownheap(tree, n);

	/* Construct the Huffman tree by repeatedly combining the least two
	 * frequent nodes.
	 */
	do {
		PQREMOVE(tree, n);	/* n = node of least frequency */
		m = heap[SMALLEST];	/* m = node of next least frequency */

		heap[--heap_max] = n;	/* keep the nodes sorted by frequency */
		heap[--heap_max] = m;

		/* Create a new node father of n and m */
		tree[node].Freq = tree[n].Freq + tree[m].Freq;
		depth[node] = MAX(depth[n], depth[m]) + 1;
		tree[n].Dad = tree[m].Dad = (ush) node;
#ifdef DUMP_BL_TREE
		if (tree == bl_tree) {
			bb_error_msg("\nnode %d(%d), sons %d(%d) %d(%d)",
					node, tree[node].Freq, n, tree[n].Freq, m, tree[m].Freq);
		}
#endif
		/* and insert the new node in the heap */
		heap[SMALLEST] = node++;
		pqdownheap(tree, SMALLEST);

	} while (heap_len >= 2);

	heap[--heap_max] = heap[SMALLEST];

	/* At this point, the fields freq and dad are set. We can now
	 * generate the bit lengths.
	 */
	gen_bitlen((tree_desc *) desc);

	/* The field len is now set, we can generate the bit codes */
	gen_codes((ct_data *) tree, max_code);
}


/* ===========================================================================
 * Scan a literal or distance tree to determine the frequencies of the codes
 * in the bit length tree. Updates opt_len to take into account the repeat
 * counts. (The contribution of the bit length codes will be added later
 * during the construction of bl_tree.)
 */
static void scan_tree(ct_data * tree, int max_code)
{
	int n;				/* iterates over all tree elements */
	int prevlen = -1;	/* last emitted length */
	int curlen;			/* length of current code */
	int nextlen = tree[0].Len;	/* length of next code */
	int count = 0;		/* repeat count of the current code */
	int max_count = 7;	/* max repeat count */
	int min_count = 4;	/* min repeat count */

	if (nextlen == 0) {
		max_count = 138;
		min_count = 3;
	}
	tree[max_code + 1].Len = 0xffff; /* guard */

	for (n = 0; n <= max_code; n++) {
		curlen = nextlen;
		nextlen = tree[n + 1].Len;
		if (++count < max_count && curlen == nextlen)
			continue;

		if (count < min_count) {
			bl_tree[curlen].Freq += count;
		} else if (curlen != 0) {
			if (curlen != prevlen)
				bl_tree[curlen].Freq++;
			bl_tree[REP_3_6].Freq++;
		} else if (count <= 10) {
			bl_tree[REPZ_3_10].Freq++;
		} else {
			bl_tree[REPZ_11_138].Freq++;
		}
		count = 0;
		prevlen = curlen;

		max_count = 7;
		min_count = 4;
		if (nextlen == 0) {
			max_count = 138;
			min_count = 3;
		} else if (curlen == nextlen) {
			max_count = 6;
			min_count = 3;
		}
	}
}


/* ===========================================================================
 * Send a literal or distance tree in compressed form, using the codes in
 * bl_tree.
 */
static void send_tree(ct_data * tree, int max_code)
{
	int n;				/* iterates over all tree elements */
	int prevlen = -1;	/* last emitted length */
	int curlen;			/* length of current code */
	int nextlen = tree[0].Len;	/* length of next code */
	int count = 0;		/* repeat count of the current code */
	int max_count = 7;	/* max repeat count */
	int min_count = 4;	/* min repeat count */

/* tree[max_code+1].Len = -1; *//* guard already set */
	if (nextlen == 0)
		max_count = 138, min_count = 3;

	for (n = 0; n <= max_code; n++) {
		curlen = nextlen;
		nextlen = tree[n + 1].Len;
		if (++count < max_count && curlen == nextlen) {
			continue;
		} else if (count < min_count) {
			do {
				SEND_CODE(curlen, bl_tree);
			} while (--count);
		} else if (curlen != 0) {
			if (curlen != prevlen) {
				SEND_CODE(curlen, bl_tree);
				count--;
			}
			Assert(count >= 3 && count <= 6, " 3_6?");
			SEND_CODE(REP_3_6, bl_tree);
			send_bits(count - 3, 2);
		} else if (count <= 10) {
			SEND_CODE(REPZ_3_10, bl_tree);
			send_bits(count - 3, 3);
		} else {
			SEND_CODE(REPZ_11_138, bl_tree);
			send_bits(count - 11, 7);
		}
		count = 0;
		prevlen = curlen;
		if (nextlen == 0) {
			max_count = 138;
			min_count = 3;
		} else if (curlen == nextlen) {
			max_count = 6;
			min_count = 3;
		} else {
			max_count = 7;
			min_count = 4;
		}
	}
}


/* ===========================================================================
 * Construct the Huffman tree for the bit lengths and return the index in
 * bl_order of the last bit length code to send.
 */
static int build_bl_tree(void)
{
	int max_blindex;	/* index of last bit length code of non zero freq */

	/* Determine the bit length frequencies for literal and distance trees */
	scan_tree((ct_data *) dyn_ltree, l_desc.max_code);
	scan_tree((ct_data *) dyn_dtree, d_desc.max_code);

	/* Build the bit length tree: */
	build_tree((tree_desc *) &bl_desc);
	/* opt_len now includes the length of the tree representations, except
	 * the lengths of the bit lengths codes and the 5+5+4 bits for the counts.
	 */

	/* Determine the number of bit length codes to send. The pkzip format
	 * requires that at least 4 bit length codes be sent. (appnote.txt says
	 * 3 but the actual value used is 4.)
	 */
	for (max_blindex = BL_CODES - 1; max_blindex >= 3; max_blindex--) {
		if (bl_tree[bl_order[max_blindex]].Len != 0)
			break;
	}
	/* Update opt_len to include the bit length tree and counts */
	opt_len += 3 * (max_blindex + 1) + 5 + 5 + 4;
	Tracev((stderr, "\ndyn trees: dyn %ld, stat %ld", opt_len, static_len));

	return max_blindex;
}


/* ===========================================================================
 * Send the header for a block using dynamic Huffman trees: the counts, the
 * lengths of the bit length codes, the literal tree and the distance tree.
 * IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
 */
static void send_all_trees(int lcodes, int dcodes, int blcodes)
{
	int rank;			/* index in bl_order */

	Assert(lcodes >= 257 && dcodes >= 1 && blcodes >= 4, "not enough codes");
	Assert(lcodes <= L_CODES && dcodes <= D_CODES
		   && blcodes <= BL_CODES, "too many codes");
	Tracev((stderr, "\nbl counts: "));
	send_bits(lcodes - 257, 5);	/* not +255 as stated in appnote.txt */
	send_bits(dcodes - 1, 5);
	send_bits(blcodes - 4, 4);	/* not -3 as stated in appnote.txt */
	for (rank = 0; rank < blcodes; rank++) {
		Tracev((stderr, "\nbl code %2d ", bl_order[rank]));
		send_bits(bl_tree[bl_order[rank]].Len, 3);
	}
	Tracev((stderr, "\nbl tree: sent %ld", bits_sent));

	send_tree((ct_data *) dyn_ltree, lcodes - 1);	/* send the literal tree */
	Tracev((stderr, "\nlit tree: sent %ld", bits_sent));

	send_tree((ct_data *) dyn_dtree, dcodes - 1);	/* send the distance tree */
	Tracev((stderr, "\ndist tree: sent %ld", bits_sent));
}


/* ===========================================================================
 * Set the file type to ASCII or BINARY, using a crude approximation:
 * binary if more than 20% of the bytes are <= 6 or >= 128, ascii otherwise.
 * IN assertion: the fields freq of dyn_ltree are set and the total of all
 * frequencies does not exceed 64K (to fit in an int on 16 bit machines).
 */
static void set_file_type(void)
{
	int n = 0;
	unsigned ascii_freq = 0;
	unsigned bin_freq = 0;

	while (n < 7)
		bin_freq += dyn_ltree[n++].Freq;
	while (n < 128)
		ascii_freq += dyn_ltree[n++].Freq;
	while (n < LITERALS)
		bin_freq += dyn_ltree[n++].Freq;
	*file_type = (bin_freq > (ascii_freq >> 2)) ? BINARY : ASCII;
	if (*file_type == BINARY && translate_eol) {
		bb_error_msg("-l used on binary file");
	}
}


/* ===========================================================================
 * Save the match info and tally the frequency counts. Return true if
 * the current block must be flushed.
 */
static int ct_tally(int dist, int lc)
{
	l_buf[last_lit++] = lc;
	if (dist == 0) {
		/* lc is the unmatched char */
		dyn_ltree[lc].Freq++;
	} else {
		/* Here, lc is the match length - MIN_MATCH */
		dist--;			/* dist = match distance - 1 */
		Assert((ush) dist < (ush) MAX_DIST
		 && (ush) lc <= (ush) (MAX_MATCH - MIN_MATCH)
		 && (ush) D_CODE(dist) < (ush) D_CODES, "ct_tally: bad match"
		);

		dyn_ltree[length_code[lc] + LITERALS + 1].Freq++;
		dyn_dtree[D_CODE(dist)].Freq++;

		d_buf[last_dist++] = dist;
		flags |= flag_bit;
	}
	flag_bit <<= 1;

	/* Output the flags if they fill a byte: */
	if ((last_lit & 7) == 0) {
		flag_buf[last_flags++] = flags;
		flags = 0, flag_bit = 1;
	}
	/* Try to guess if it is profitable to stop the current block here */
	if ((last_lit & 0xfff) == 0) {
		/* Compute an upper bound for the compressed length */
		ulg out_length = last_lit * 8L;
		ulg in_length = (ulg) strstart - block_start;
		int dcode;

		for (dcode = 0; dcode < D_CODES; dcode++) {
			out_length += dyn_dtree[dcode].Freq * (5L + extra_dbits[dcode]);
		}
		out_length >>= 3;
		Trace((stderr,
			   "\nlast_lit %u, last_dist %u, in %ld, out ~%ld(%ld%%) ",
			   last_lit, last_dist, in_length, out_length,
			   100L - out_length * 100L / in_length));
		if (last_dist < last_lit / 2 && out_length < in_length / 2)
			return 1;
	}
	return (last_lit == LIT_BUFSIZE - 1 || last_dist == DIST_BUFSIZE);
	/* We avoid equality with LIT_BUFSIZE because of wraparound at 64K
	 * on 16 bit machines and because stored blocks are restricted to
	 * 64K-1 bytes.
	 */
}

/* ===========================================================================
 * Send the block data compressed using the given Huffman trees
 */
static void compress_block(ct_data * ltree, ct_data * dtree)
{
	unsigned dist;          /* distance of matched string */
	int lc;                 /* match length or unmatched char (if dist == 0) */
	unsigned lx = 0;        /* running index in l_buf */
	unsigned dx = 0;        /* running index in d_buf */
	unsigned fx = 0;        /* running index in flag_buf */
	uch flag = 0;           /* current flags */
	unsigned code;          /* the code to send */
	int extra;              /* number of extra bits to send */

	if (last_lit != 0) do {
		if ((lx & 7) == 0)
			flag = flag_buf[fx++];
		lc = l_buf[lx++];
		if ((flag & 1) == 0) {
			SEND_CODE(lc, ltree);	/* send a literal byte */
			Tracecv(isgraph(lc), (stderr, " '%c' ", lc));
		} else {
			/* Here, lc is the match length - MIN_MATCH */
			code = length_code[lc];
			SEND_CODE(code + LITERALS + 1, ltree);	/* send the length code */
			extra = extra_lbits[code];
			if (extra != 0) {
				lc -= base_length[code];
				send_bits(lc, extra);	/* send the extra length bits */
			}
			dist = d_buf[dx++];
			/* Here, dist is the match distance - 1 */
			code = D_CODE(dist);
			Assert(code < D_CODES, "bad d_code");

			SEND_CODE(code, dtree);	/* send the distance code */
			extra = extra_dbits[code];
			if (extra != 0) {
				dist -= base_dist[code];
				send_bits(dist, extra);	/* send the extra distance bits */
			}
		}			/* literal or match pair ? */
		flag >>= 1;
	} while (lx < last_lit);

	SEND_CODE(END_BLOCK, ltree);
}


/* ===========================================================================
 * Determine the best encoding for the current block: dynamic trees, static
 * trees or store, and output the encoded block to the zip file. This function
 * returns the total compressed length for the file so far.
 */
static ulg flush_block(char *buf, ulg stored_len, int eof)
{
	ulg opt_lenb, static_lenb;      /* opt_len and static_len in bytes */
	int max_blindex;                /* index of last bit length code of non zero freq */

	flag_buf[last_flags] = flags;   /* Save the flags for the last 8 items */

	/* Check if the file is ascii or binary */
	if (*file_type == (ush) UNKNOWN)
		set_file_type();

	/* Construct the literal and distance trees */
	build_tree((tree_desc *) &l_desc);
	Tracev((stderr, "\nlit data: dyn %ld, stat %ld", opt_len, static_len));

	build_tree((tree_desc *) &d_desc);
	Tracev((stderr, "\ndist data: dyn %ld, stat %ld", opt_len, static_len));
	/* At this point, opt_len and static_len are the total bit lengths of
	 * the compressed block data, excluding the tree representations.
	 */

	/* Build the bit length tree for the above two trees, and get the index
	 * in bl_order of the last bit length code to send.
	 */
	max_blindex = build_bl_tree();

	/* Determine the best encoding. Compute first the block length in bytes */
	opt_lenb = (opt_len + 3 + 7) >> 3;
	static_lenb = (static_len + 3 + 7) >> 3;

	Trace((stderr,
		   "\nopt %lu(%lu) stat %lu(%lu) stored %lu lit %u dist %u ",
		   opt_lenb, opt_len, static_lenb, static_len, stored_len,
		   last_lit, last_dist));

	if (static_lenb <= opt_lenb)
		opt_lenb = static_lenb;

	/* If compression failed and this is the first and last block,
	 * and if the zip file can be seeked (to rewrite the local header),
	 * the whole file is transformed into a stored file:
	 */
	if (stored_len <= opt_lenb && eof && compressed_len == 0L && seekable()) {
		/* Since LIT_BUFSIZE <= 2*WSIZE, the input data must be there: */
		if (buf == NULL)
			bb_error_msg("block vanished");

		copy_block(buf, (unsigned) stored_len, 0);	/* without header */
		compressed_len = stored_len << 3;
		*file_method = STORED;

	} else if (stored_len + 4 <= opt_lenb && buf != NULL) {
		/* 4: two words for the lengths */
		/* The test buf != NULL is only necessary if LIT_BUFSIZE > WSIZE.
		 * Otherwise we can't have processed more than WSIZE input bytes since
		 * the last block flush, because compression would have been
		 * successful. If LIT_BUFSIZE <= WSIZE, it is never too late to
		 * transform a block into a stored block.
		 */
		send_bits((STORED_BLOCK << 1) + eof, 3);	/* send block type */
		compressed_len = (compressed_len + 3 + 7) & ~7L;
		compressed_len += (stored_len + 4) << 3;

		copy_block(buf, (unsigned) stored_len, 1);	/* with header */

	} else if (static_lenb == opt_lenb) {
		send_bits((STATIC_TREES << 1) + eof, 3);
		compress_block((ct_data *) static_ltree, (ct_data *) static_dtree);
		compressed_len += 3 + static_len;
	} else {
		send_bits((DYN_TREES << 1) + eof, 3);
		send_all_trees(l_desc.max_code + 1, d_desc.max_code + 1,
					   max_blindex + 1);
		compress_block((ct_data *) dyn_ltree, (ct_data *) dyn_dtree);
		compressed_len += 3 + opt_len;
	}
	Assert(compressed_len == bits_sent, "bad compressed size");
	init_block();

	if (eof) {
		bi_windup();
		compressed_len += 7;	/* align on byte boundary */
	}
	Tracev((stderr, "\ncomprlen %lu(%lu) ", compressed_len >> 3,
			compressed_len - 7 * eof));

	return compressed_len >> 3;
}


/* ===========================================================================
 * Update a hash value with the given input byte
 * IN  assertion: all calls to to UPDATE_HASH are made with consecutive
 *    input characters, so that a running hash key can be computed from the
 *    previous key instead of complete recalculation each time.
 */
#define UPDATE_HASH(h, c) (h = (((h)<<H_SHIFT) ^ (c)) & HASH_MASK)


/* ===========================================================================
 * Same as above, but achieves better compression. We use a lazy
 * evaluation for matches: a match is finally adopted only if there is
 * no better match at the next window position.
 *
 * Processes a new input file and return its compressed length. Sets
 * the compressed length, crc, deflate flags and internal file
 * attributes.
 */

/* Flush the current block, with given end-of-file flag.
 * IN assertion: strstart is set to the end of the current match. */
#define FLUSH_BLOCK(eof) \
	flush_block( \
		block_start >= 0L \
			? (char*)&window[(unsigned)block_start] \
			: (char*)NULL, \
		(ulg)strstart - block_start, \
		(eof) \
	)

/* Insert string s in the dictionary and set match_head to the previous head
 * of the hash chain (the most recent string with same hash key). Return
 * the previous length of the hash chain.
 * IN  assertion: all calls to to INSERT_STRING are made with consecutive
 *    input characters and the first MIN_MATCH bytes of s are valid
 *    (except for the last MIN_MATCH-1 bytes of the input file). */
#define INSERT_STRING(s, match_head) \
{ \
	UPDATE_HASH(ins_h, window[(s) + MIN_MATCH-1]); \
	prev[(s) & WMASK] = match_head = head[ins_h]; \
	head[ins_h] = (s); \
}

static ulg deflate(void)
{
	IPos hash_head;		/* head of hash chain */
	IPos prev_match;	/* previous match */
	int flush;			/* set if current block must be flushed */
	int match_available = 0;	/* set if previous match exists */
	unsigned match_length = MIN_MATCH - 1;	/* length of best match */

	/* Process the input block. */
	while (lookahead != 0) {
		/* Insert the string window[strstart .. strstart+2] in the
		 * dictionary, and set hash_head to the head of the hash chain:
		 */
		INSERT_STRING(strstart, hash_head);

		/* Find the longest match, discarding those <= prev_length.
		 */
		prev_length = match_length, prev_match = match_start;
		match_length = MIN_MATCH - 1;

		if (hash_head != 0 && prev_length < max_lazy_match
		 && strstart - hash_head <= MAX_DIST
		) {
			/* To simplify the code, we prevent matches with the string
			 * of window index 0 (in particular we have to avoid a match
			 * of the string with itself at the start of the input file).
			 */
			match_length = longest_match(hash_head);
			/* longest_match() sets match_start */
			if (match_length > lookahead)
				match_length = lookahead;

			/* Ignore a length 3 match if it is too distant: */
			if (match_length == MIN_MATCH && strstart - match_start > TOO_FAR) {
				/* If prev_match is also MIN_MATCH, match_start is garbage
				 * but we will ignore the current match anyway.
				 */
				match_length--;
			}
		}
		/* If there was a match at the previous step and the current
		 * match is not better, output the previous match:
		 */
		if (prev_length >= MIN_MATCH && match_length <= prev_length) {
			check_match(strstart - 1, prev_match, prev_length);
			flush = ct_tally(strstart - 1 - prev_match, prev_length - MIN_MATCH);

			/* Insert in hash table all strings up to the end of the match.
			 * strstart-1 and strstart are already inserted.
			 */
			lookahead -= prev_length - 1;
			prev_length -= 2;
			do {
				strstart++;
				INSERT_STRING(strstart, hash_head);
				/* strstart never exceeds WSIZE-MAX_MATCH, so there are
				 * always MIN_MATCH bytes ahead. If lookahead < MIN_MATCH
				 * these bytes are garbage, but it does not matter since the
				 * next lookahead bytes will always be emitted as literals.
				 */
			} while (--prev_length != 0);
			match_available = 0;
			match_length = MIN_MATCH - 1;
			strstart++;
			if (flush) {
				FLUSH_BLOCK(0);
				block_start = strstart;
			}
		} else if (match_available) {
			/* If there was no match at the previous position, output a
			 * single literal. If there was a match but the current match
			 * is longer, truncate the previous match to a single literal.
			 */
			Tracevv((stderr, "%c", window[strstart - 1]));
			if (ct_tally(0, window[strstart - 1])) {
				FLUSH_BLOCK(0);
				block_start = strstart;
			}
			strstart++;
			lookahead--;
		} else {
			/* There is no previous match to compare with, wait for
			 * the next step to decide.
			 */
			match_available = 1;
			strstart++;
			lookahead--;
		}
		Assert(strstart <= isize && lookahead <= isize, "a bit too far");

		/* Make sure that we always have enough lookahead, except
		 * at the end of the input file. We need MAX_MATCH bytes
		 * for the next match, plus MIN_MATCH bytes to insert the
		 * string following the next match.
		 */
		while (lookahead < MIN_LOOKAHEAD && !eofile)
			fill_window();
	}
	if (match_available)
		ct_tally(0, window[strstart - 1]);

	return FLUSH_BLOCK(1);	/* eof */
}


/* ===========================================================================
 * Initialize the bit string routines.
 */
static void bi_init(void) //// int zipfile)
{
////	zfile = zipfile;
	bi_buf = 0;
	bi_valid = 0;
#ifdef DEBUG
	bits_sent = 0L;
#endif
}


/* ===========================================================================
 * Initialize the "longest match" routines for a new file
 */
static void lm_init(ush * flagsp)
{
	unsigned j;

	/* Initialize the hash table. */
	memset(head, 0, HASH_SIZE * sizeof(*head));
	/* prev will be initialized on the fly */

	/* speed options for the general purpose bit flag */
	*flagsp |= 2;	/* FAST 4, SLOW 2 */
	/* ??? reduce max_chain_length for binary files */

	strstart = 0;
	block_start = 0L;

	lookahead = file_read(window,
			sizeof(int) <= 2 ? (unsigned) WSIZE : 2 * WSIZE);

	if (lookahead == 0 || lookahead == (unsigned) -1) {
		eofile = 1;
		lookahead = 0;
		return;
	}
	eofile = 0;
	/* Make sure that we always have enough lookahead. This is important
	 * if input comes from a device such as a tty.
	 */
	while (lookahead < MIN_LOOKAHEAD && !eofile)
		fill_window();

	ins_h = 0;
	for (j = 0; j < MIN_MATCH - 1; j++)
		UPDATE_HASH(ins_h, window[j]);
	/* If lookahead < MIN_MATCH, ins_h is garbage, but this is
	 * not important since only literal bytes will be emitted.
	 */
}


/* ===========================================================================
 * Allocate the match buffer, initialize the various tables and save the
 * location of the internal file attribute (ascii/binary) and method
 * (DEFLATE/STORE).
 * One callsite in zip()
 */
static void ct_init(ush * attr, int *methodp)
{
	int n;				/* iterates over tree elements */
	int length;			/* length value */
	int code;			/* code value */
	int dist;			/* distance index */

	file_type = attr;
	file_method = methodp;
	compressed_len = 0L;

#ifdef NOT_NEEDED
	if (static_dtree[0].Len != 0)
		return;			/* ct_init already called */
#endif

	/* Initialize the mapping length (0..255) -> length code (0..28) */
	length = 0;
	for (code = 0; code < LENGTH_CODES - 1; code++) {
		base_length[code] = length;
		for (n = 0; n < (1 << extra_lbits[code]); n++) {
			length_code[length++] = code;
		}
	}
	Assert(length == 256, "ct_init: length != 256");
	/* Note that the length 255 (match length 258) can be represented
	 * in two different ways: code 284 + 5 bits or code 285, so we
	 * overwrite length_code[255] to use the best encoding:
	 */
	length_code[length - 1] = code;

	/* Initialize the mapping dist (0..32K) -> dist code (0..29) */
	dist = 0;
	for (code = 0; code < 16; code++) {
		base_dist[code] = dist;
		for (n = 0; n < (1 << extra_dbits[code]); n++) {
			dist_code[dist++] = code;
		}
	}
	Assert(dist == 256, "ct_init: dist != 256");
	dist >>= 7;			/* from now on, all distances are divided by 128 */
	for (; code < D_CODES; code++) {
		base_dist[code] = dist << 7;
		for (n = 0; n < (1 << (extra_dbits[code] - 7)); n++) {
			dist_code[256 + dist++] = code;
		}
	}
	Assert(dist == 256, "ct_init: 256+dist != 512");

	/* Construct the codes of the static literal tree */
	/* already zeroed - it's in bss
	for (n = 0; n <= MAX_BITS; n++)
		bl_count[n] = 0; */

	n = 0;
	while (n <= 143) {
		static_ltree[n++].Len = 8;
		bl_count[8]++;
	}
	while (n <= 255) {
		static_ltree[n++].Len = 9;
		bl_count[9]++;
	}
	while (n <= 279) {
		static_ltree[n++].Len = 7;
		bl_count[7]++;
	}
	while (n <= 287) {
		static_ltree[n++].Len = 8;
		bl_count[8]++;
	}
	/* Codes 286 and 287 do not exist, but we must include them in the
	 * tree construction to get a canonical Huffman tree (longest code
	 * all ones)
	 */
	gen_codes((ct_data *) static_ltree, L_CODES + 1);

	/* The static distance tree is trivial: */
	for (n = 0; n < D_CODES; n++) {
		static_dtree[n].Len = 5;
		static_dtree[n].Code = bi_reverse(n, 5);
	}

	/* Initialize the first block of the first file: */
	init_block();
}


/* ===========================================================================
 * Deflate in to out.
 * IN assertions: the input and output buffers are cleared.
 *   The variables time_stamp and save_orig_name are initialized.
 */

/* put_header_byte is used for the compressed output
 * - for the initial 4 bytes that can't overflow the buffer. */
#define put_header_byte(c) outbuf[outcnt++] = (c)

static void zip(int in, int out)
{
	uch my_flags = 0;       /* general purpose bit flags */
	ush attr = 0;           /* ascii/binary flag */
	ush deflate_flags = 0;  /* pkzip -es, -en or -ex equivalent */

	ifd = in;
	ofd = out;
	outcnt = 0;

	/* Write the header to the gzip file. See algorithm.doc for the format */

	method = DEFLATED;
	put_header_byte(0x1f); /* magic header for gzip files, 1F 8B */
	put_header_byte(0x8b);
	put_header_byte(DEFLATED); /* compression method */
	put_header_byte(my_flags); /* general flags */
	put_32bit(time_stamp);

	/* Write deflated file to zip file */
	crc = ~0;

	bi_init(); //// (out);
	ct_init(&attr, &method);
	lm_init(&deflate_flags);

	put_8bit(deflate_flags);	/* extra flags */
	put_8bit(3);	/* OS identifier = 3 (Unix) */

	deflate();

	/* Write the crc and uncompressed size */
	put_32bit(~crc);
	put_32bit(isize);

	flush_outbuf();
}


/* ======================================================================== */
static void abort_gzip(int ATTRIBUTE_UNUSED ignored)
{
	exit(1);
}

int gzip_main(int argc, char **argv)
{
	enum {
		OPT_tostdout = 0x1,
		OPT_force = 0x2,
	};

	unsigned opt;
	int inFileNum;
	int outFileNum;
	int i;
	struct stat statBuf;

	opt = getopt32(argc, argv, "cf123456789qv" USE_GUNZIP("d"));
	//if (opt & 0x1) // -c
	//if (opt & 0x2) // -f
	/* Ignore 1-9 (compression level) options */
	//if (opt & 0x4) // -1
	//if (opt & 0x8) // -2
	//if (opt & 0x10) // -3
	//if (opt & 0x20) // -4
	//if (opt & 0x40) // -5
	//if (opt & 0x80) // -6
	//if (opt & 0x100) // -7
	//if (opt & 0x200) // -8
	//if (opt & 0x400) // -9
	//if (opt & 0x800) // -q
	//if (opt & 0x1000) // -v
#if ENABLE_GUNZIP /* gunzip_main may not be visible... */
	if (opt & 0x2000) { // -d
		/* FIXME: getopt32 should not depend on optind */
		optind = 1;
		return gunzip_main(argc, argv);
	}
#endif

	foreground = signal(SIGINT, SIG_IGN) != SIG_IGN;
	if (foreground) {
		signal(SIGINT, abort_gzip);
	}
#ifdef SIGTERM
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
		signal(SIGTERM, abort_gzip);
	}
#endif
#ifdef SIGHUP
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
		signal(SIGHUP, abort_gzip);
	}
#endif

	/* Allocate all global buffers (for DYN_ALLOC option) */
	ALLOC(uch, l_buf, INBUFSIZ);
	ALLOC(uch, outbuf, OUTBUFSIZ);
	ALLOC(ush, d_buf, DIST_BUFSIZE);
	ALLOC(uch, window, 2L * WSIZE);
	ALLOC(ush, prev, 1L << BITS);

	/* Initialise the CRC32 table */
	crc_32_tab = crc32_filltable(0);

	clear_bufs();

	if (optind == argc) {
		time_stamp = 0;
		zip(STDIN_FILENO, STDOUT_FILENO);
		return exit_code;
	}

	for (i = optind; i < argc; i++) {
		char *path = NULL;

		clear_bufs();
		if (LONE_DASH(argv[i])) {
			time_stamp = 0;
			inFileNum = STDIN_FILENO;
			outFileNum = STDOUT_FILENO;
		} else {
			inFileNum = xopen(argv[i], O_RDONLY);
			if (fstat(inFileNum, &statBuf) < 0)
				bb_perror_msg_and_die("%s", argv[i]);
			time_stamp = statBuf.st_ctime;

			if (!(opt & OPT_tostdout)) {
				path = xasprintf("%s.gz", argv[i]);

				/* Open output file */
#if defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1 && defined(O_NOFOLLOW)
				outFileNum = open(path, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW);
#else
				outFileNum = open(path, O_RDWR | O_CREAT | O_EXCL);
#endif
				if (outFileNum < 0) {
					bb_perror_msg("%s", path);
					free(path);
					continue;
				}

				/* Set permissions on the file */
				fchmod(outFileNum, statBuf.st_mode);
			} else
				outFileNum = STDOUT_FILENO;
		}

		if (path == NULL && isatty(outFileNum) && !(opt & OPT_force)) {
			bb_error_msg("compressed data not written "
				"to a terminal. Use -f to force compression.");
			free(path);
			continue;
		}

		zip(inFileNum, outFileNum);

		if (path != NULL) {
			char *delFileName;

			close(inFileNum);
			close(outFileNum);

			/* Delete the original file */
			// Pity we don't propagate zip failures to this place...
			//if (zip_is_ok)
				delFileName = argv[i];
			//else
			//	delFileName = path;
			if (unlink(delFileName) < 0)
				bb_perror_msg("%s", delFileName);
		}

		free(path);
	}

	return exit_code;
}
