/* vi: set sw=4 ts=4: */
/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Charles P. Wright <cpw@unix.asb.com>
 *		"this is a stripped down version of gzip I put into busybox, it does
 *		only standard in to standard out with -9 compression.  It also requires
 *		the zcat module for some important functions."
 *
 * Adjusted further by Erik Andersen <andersen@codepoet.org> to support 
 * files as well as stdin/stdout, and to generally behave itself wrt 
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
 */

/* These defines are very important for BusyBox.  Without these,
 * huge chunks of ram are pre-allocated making the BusyBox bss 
 * size Freaking Huge(tm), which is a bad thing.*/
#define SMALL_MEM
#define DYN_ALLOC

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <utime.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include "busybox.h"

#define memzero(s, n)     memset ((void *)(s), 0, (n))

#ifndef RETSIGTYPE
#  define RETSIGTYPE void
#endif

typedef unsigned char uch;
typedef unsigned short ush;
typedef unsigned long ulg;

/* Return codes from gzip */
#define OK      0
#define ERROR   1
#define WARNING 2

/* Compression methods (see algorithm.doc) */
/* Only STORED and DEFLATED are supported by this BusyBox module */
#define STORED      0
/* methods 4 to 7 reserved */
#define DEFLATED    8

/* To save memory for 16 bit systems, some arrays are overlaid between
 * the various modules:
 * deflate:  prev+head   window      d_buf  l_buf  outbuf
 * unlzw:    tab_prefix  tab_suffix  stack  inbuf  outbuf
 * For compression, input is done in window[]. For decompression, output
 * is done in window except for unlzw.
 */

#ifndef	INBUFSIZ
#  ifdef SMALL_MEM
#    define INBUFSIZ  0x2000	/* input buffer size */
#  else
#    define INBUFSIZ  0x8000	/* input buffer size */
#  endif
#endif
#define INBUF_EXTRA  64	/* required by unlzw() */

#ifndef	OUTBUFSIZ
#  ifdef SMALL_MEM
#    define OUTBUFSIZ   8192	/* output buffer size */
#  else
#    define OUTBUFSIZ  16384	/* output buffer size */
#  endif
#endif
#define OUTBUF_EXTRA 2048	/* required by unlzw() */

#ifndef DIST_BUFSIZE
#  ifdef SMALL_MEM
#    define DIST_BUFSIZE 0x2000	/* buffer for distances, see trees.c */
#  else
#    define DIST_BUFSIZE 0x8000	/* buffer for distances, see trees.c */
#  endif
#endif

#ifdef DYN_ALLOC
#  define DECLARE(type, array, size)  static type * array
#  define ALLOC(type, array, size) { \
      array = (type*)xcalloc((size_t)(((size)+1L)/2), 2*sizeof(type)); \
   }
#  define FREE(array) {free(array), array=NULL;}
#else
#  define DECLARE(type, array, size)  static type array[size]
#  define ALLOC(type, array, size)
#  define FREE(array)
#endif

#define tab_suffix window
#define tab_prefix prev	/* hash link (see deflate.c) */
#define head (prev+WSIZE)	/* hash head (see deflate.c) */

static long bytes_in;	/* number of input bytes */

#define isize bytes_in
/* for compatibility with old zip sources (to be cleaned) */

typedef int file_t;		/* Do not use stdio */

#define NO_FILE  (-1)	/* in memory compression */


#define	PACK_MAGIC     "\037\036"	/* Magic header for packed files */
#define	GZIP_MAGIC     "\037\213"	/* Magic header for gzip files, 1F 8B */
#define	OLD_GZIP_MAGIC "\037\236"	/* Magic header for gzip 0.5 = freeze 1.x */
#define	LZH_MAGIC      "\037\240"	/* Magic header for SCO LZH Compress files */
#define PKZIP_MAGIC    "\120\113\003\004"	/* Magic header for pkzip files */

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
#  define WSIZE 0x8000	/* window size--must be a power of two, and */
#endif							/*  at least 32K for zip's deflate method */

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

/* put_byte is used for the compressed output */
#define put_byte(c) {outbuf[outcnt++]=(uch)(c); if (outcnt==OUTBUFSIZ)\
   flush_outbuf();}


/* Output a 32 bit value to the bit stream, lsb first */
#if 0
#define put_long(n) { \
    put_short((n) & 0xffff); \
    put_short(((ulg)(n)) >> 16); \
}
#endif

#define seekable()    0	/* force sequential output */
#define translate_eol 0	/* no option -a yet */

/* Diagnostic functions */
#ifdef DEBUG
#  define Assert(cond,msg) {if(!(cond)) bb_error_msg(msg);}
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

#define WARN(msg) {if (!quiet) fprintf msg ; \
		   if (exit_code == OK) exit_code = WARNING;}

#ifndef MAX_PATH_LEN
#  define MAX_PATH_LEN   1024	/* max pathname length */
#endif


	/* from zip.c: */
static int zip(int in, int out);
static int file_read(char *buf, unsigned size);

	/* from gzip.c */
static RETSIGTYPE abort_gzip(void);

		/* from deflate.c */
static void lm_init(ush * flags);
static ulg deflate(void);

		/* from trees.c */
static void ct_init(ush * attr, int *methodp);
static int ct_tally(int dist, int lc);
static ulg flush_block(char *buf, ulg stored_len, int eof);

		/* from bits.c */
static void bi_init(file_t zipfile);
static void send_bits(int value, int length);
static unsigned bi_reverse(unsigned value, int length);
static void bi_windup(void);
static void copy_block(char *buf, unsigned len, int header);
static int (*read_buf) (char *buf, unsigned size);

	/* from util.c: */
static void flush_outbuf(void);

/* lzw.h -- define the lzw functions.
 * Copyright (C) 1992-1993 Jean-loup Gailly.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

#if !defined(OF) && defined(lint)
#  include "gzip.h"
#endif

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

/* tailor.h -- target dependent definitions
 * Copyright (C) 1992-1993 Jean-loup Gailly.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

/* The target dependent definitions should be defined here only.
 * The target dependent functions should be defined in tailor.c.
 */


	/* Common defaults */

#ifndef OS_CODE
#  define OS_CODE  0x03	/* assume Unix */
#endif

#ifndef PATH_SEP
#  define PATH_SEP '/'
#endif

#ifndef OPTIONS_VAR
#  define OPTIONS_VAR "GZIP"
#endif

#ifndef Z_SUFFIX
#  define Z_SUFFIX ".gz"
#endif

#ifdef MAX_EXT_CHARS
#  define MAX_SUFFIX  MAX_EXT_CHARS
#else
#  define MAX_SUFFIX  30
#endif

		/* global buffers */

DECLARE(uch, inbuf, INBUFSIZ + INBUF_EXTRA);
DECLARE(uch, outbuf, OUTBUFSIZ + OUTBUF_EXTRA);
DECLARE(ush, d_buf, DIST_BUFSIZE);
DECLARE(uch, window, 2L * WSIZE);
DECLARE(ush, tab_prefix, 1L << BITS);

static int foreground;	/* set if program run in foreground */
static int method = DEFLATED;	/* compression method */
static int exit_code = OK;	/* program exit code */
static int part_nb;		/* number of parts in .gz file */
static long time_stamp;	/* original time stamp (modification time) */
static long ifile_size;	/* input file size, -1 for devices (debug only) */
static char z_suffix[MAX_SUFFIX + 1];	/* default suffix (can be set with --suffix) */
static int z_len;		/* strlen(z_suffix) */

static int ifd;			/* input file descriptor */
static int ofd;			/* output file descriptor */
static unsigned insize;	/* valid bytes in inbuf */
static unsigned outcnt;	/* bytes in output buffer */


/* Output a 16 bit value, lsb first */
static void put_short(ush w)
{
	if (outcnt < OUTBUFSIZ - 2) {
		outbuf[outcnt++] = (uch) ((w) & 0xff);
		outbuf[outcnt++] = (uch) ((ush) (w) >> 8);
	} else {
		put_byte((uch) ((w) & 0xff));
		put_byte((uch) ((ush) (w) >> 8));
	}
}

/* ========================================================================
 * Signal and error handler.
 */
static void abort_gzip()
{
	exit(ERROR);
}

/* ===========================================================================
 * Clear input and output buffers
 */
static void clear_bufs(void)
{
	outcnt = 0;
	insize = 0;
	bytes_in = 0L;
}

static void write_bb_error_msg(void)
{
	fputc('\n', stderr);
	bb_perror_nomsg();
	abort_gzip();
}

/* ===========================================================================
 * Does the same as write(), but also handles partial pipe writes and checks
 * for error return.
 */
static void write_buf(int fd, void *buf, unsigned cnt)
{
	unsigned n;

	while ((n = write(fd, buf, cnt)) != cnt) {
		if (n == (unsigned) (-1)) {
			write_bb_error_msg();
		}
		cnt -= n;
		buf = (void *) ((char *) buf + n);
	}
}

/* ===========================================================================
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
static ulg updcrc(uch * s, unsigned n)
{
	static ulg crc = (ulg) 0xffffffffL;	/* shift register contents */
	register ulg c;		/* temporary variable */
	static unsigned long crc_32_tab[256];

	if (crc_32_tab[1] == 0x00000000L) {
		unsigned long csr;	/* crc shift register */
		const unsigned long e = 0xedb88320L;	/* polynomial exclusive-or pattern */
		int i;			/* counter for all possible eight bit values */
		int k;			/* byte being shifted into crc apparatus */

		/* Compute table of CRC's. */
		for (i = 1; i < 256; i++) {
			csr = i;
			/* The idea to initialize the register with the byte instead of
			   * zero was stolen from Haruhiko Okumura's ar002
			 */
			for (k = 8; k; k--)
				csr = csr & 1 ? (csr >> 1) ^ e : csr >> 1;
			crc_32_tab[i] = csr;
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
	return c ^ 0xffffffffL;	/* (instead of ~c for 64-bit machines) */
}

/* bits.c -- output variable-length bit strings
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */


/*
 *  PURPOSE
 *
 *      Output variable-length bit strings. Compression can be done
 *      to a file or to memory. (The latter is not supported in this version.)
 *
 *  DISCUSSION
 *
 *      The PKZIP "deflate" file format interprets compressed file data
 *      as a sequence of bits.  Multi-bit strings in the file may cross
 *      byte boundaries without restriction.
 *
 *      The first bit of each byte is the low-order bit.
 *
 *      The routines in this file allow a variable-length bit value to
 *      be output right-to-left (useful for literal values). For
 *      left-to-right output (useful for code strings from the tree routines),
 *      the bits must have been reversed first with bi_reverse().
 *
 *      For in-memory compression, the compressed bit stream goes directly
 *      into the requested output buffer. The input data is read in blocks
 *      by the mem_read() function. The buffer is limited to 64K on 16 bit
 *      machines.
 *
 *  INTERFACE
 *
 *      void bi_init (FILE *zipfile)
 *          Initialize the bit string routines.
 *
 *      void send_bits (int value, int length)
 *          Write out a bit string, taking the source bits right to
 *          left.
 *
 *      int bi_reverse (int value, int length)
 *          Reverse the bits of a bit string, taking the source bits left to
 *          right and emitting them right to left.
 *
 *      void bi_windup (void)
 *          Write out any remaining bits in an incomplete byte.
 *
 *      void copy_block(char *buf, unsigned len, int header)
 *          Copy a stored block to the zip file, storing first the length and
 *          its one's complement if requested.
 *
 */

/* ===========================================================================
 * Local data used by the "bit string" routines.
 */

static file_t zfile;	/* output gzip file */

static unsigned short bi_buf;

/* Output buffer. bits are inserted starting at the bottom (least significant
 * bits).
 */

#define Buf_size (8 * 2*sizeof(char))
/* Number of bits used within bi_buf. (bi_buf might be implemented on
 * more than 16 bits on some systems.)
 */

static int bi_valid;

/* Current input function. Set to mem_read for in-memory compression */

#ifdef DEBUG
ulg bits_sent;			/* bit length of the compressed data */
#endif

/* ===========================================================================
 * Initialize the bit string routines.
 */
static void bi_init(file_t zipfile)
{
	zfile = zipfile;
	bi_buf = 0;
	bi_valid = 0;
#ifdef DEBUG
	bits_sent = 0L;
#endif

	/* Set the defaults for file compression. They are set by memcompress
	 * for in-memory compression.
	 */
	if (zfile != NO_FILE) {
		read_buf = file_read;
	}
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
	bits_sent += (ulg) length;
#endif
	/* If not enough room in bi_buf, use (valid) bits from bi_buf and
	 * (16 - bi_valid) bits from value, leaving (width - (16-bi_valid))
	 * unused bits in value.
	 */
	if (bi_valid > (int) Buf_size - length) {
		bi_buf |= (value << bi_valid);
		put_short(bi_buf);
		bi_buf = (ush) value >> (Buf_size - bi_valid);
		bi_valid += length - Buf_size;
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
	register unsigned res = 0;

	do {
		res |= code & 1;
		code >>= 1, res <<= 1;
	} while (--len > 0);
	return res >> 1;
}

/* ===========================================================================
 * Write out any remaining bits in an incomplete byte.
 */
static void bi_windup()
{
	if (bi_valid > 8) {
		put_short(bi_buf);
	} else if (bi_valid > 0) {
		put_byte(bi_buf);
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
		put_short((ush) len);
		put_short((ush) ~ len);
#ifdef DEBUG
		bits_sent += 2 * 16;
#endif
	}
#ifdef DEBUG
	bits_sent += (ulg) len << 3;
#endif
	while (len--) {
		put_byte(*buf++);
	}
}

/* deflate.c -- compress data using the deflation algorithm
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

/*
 *  PURPOSE
 *
 *      Identify new text as repetitions of old text within a fixed-
 *      length sliding window trailing behind the new text.
 *
 *  DISCUSSION
 *
 *      The "deflation" process depends on being able to identify portions
 *      of the input text which are identical to earlier input (within a
 *      sliding window trailing behind the input currently being processed).
 *
 *      The most straightforward technique turns out to be the fastest for
 *      most input files: try all possible matches and select the longest.
 *      The key feature of this algorithm is that insertions into the string
 *      dictionary are very simple and thus fast, and deletions are avoided
 *      completely. Insertions are performed at each input character, whereas
 *      string matches are performed only when the previous match ends. So it
 *      is preferable to spend more time in matches to allow very fast string
 *      insertions and avoid deletions. The matching algorithm for small
 *      strings is inspired from that of Rabin & Karp. A brute force approach
 *      is used to find longer strings when a small match has been found.
 *      A similar algorithm is used in comic (by Jan-Mark Wams) and freeze
 *      (by Leonid Broukhis).
 *         A previous version of this file used a more sophisticated algorithm
 *      (by Fiala and Greene) which is guaranteed to run in linear amortized
 *      time, but has a larger average cost, uses more memory and is patented.
 *      However the F&G algorithm may be faster for some highly redundant
 *      files if the parameter max_chain_length (described below) is too large.
 *
 *  ACKNOWLEDGEMENTS
 *
 *      The idea of lazy evaluation of matches is due to Jan-Mark Wams, and
 *      I found it in 'freeze' written by Leonid Broukhis.
 *      Thanks to many info-zippers for bug reports and testing.
 *
 *  REFERENCES
 *
 *      APPNOTE.TXT documentation file in PKZIP 1.93a distribution.
 *
 *      A description of the Rabin and Karp algorithm is given in the book
 *         "Algorithms" by R. Sedgewick, Addison-Wesley, p252.
 *
 *      Fiala,E.R., and Greene,D.H.
 *         Data Compression with Finite Windows, Comm.ACM, 32,4 (1989) 490-595
 *
 *  INTERFACE
 *
 *      void lm_init (int pack_level, ush *flags)
 *          Initialize the "longest match" routines for a new file
 *
 *      ulg deflate (void)
 *          Processes a new input file and return its compressed length. Sets
 *          the compressed length, crc, deflate flags and internal file
 *          attributes.
 */


/* ===========================================================================
 * Configuration parameters
 */

/* Compile with MEDIUM_MEM to reduce the memory requirements or
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

/* To save space (see unlzw.c), we overlay prev+head with tab_prefix and
 * window with tab_suffix. Check that we can do this:
 */
#if (WSIZE<<1) > (1<<BITS)
#  error cannot overlay window with tab_suffix and prev with tab_prefix0
#endif
#if HASH_BITS > BITS-1
#  error cannot overlay head with tab_prefix1
#endif
#define HASH_SIZE (unsigned)(1<<HASH_BITS)
#define HASH_MASK (HASH_SIZE-1)
#define WMASK     (WSIZE-1)
/* HASH_SIZE and WSIZE must be powers of two */
#define NIL 0
/* Tail of hash chains */
#define FAST 4
#define SLOW 2
/* speed options for the general purpose bit flag */
#ifndef TOO_FAR
#  define TOO_FAR 4096
#endif
/* Matches of length 3 are discarded if their distance exceeds TOO_FAR */
/* ===========================================================================
 * Local data used by the "longest match" routines.
 */
typedef ush Pos;
typedef unsigned IPos;

/* A Pos is an index in the character window. We use short instead of int to
 * save space in the various tables. IPos is used only for parameter passing.
 */

/* DECLARE(uch, window, 2L*WSIZE); */
/* Sliding window. Input bytes are read into the second half of the window,
 * and move to the first half later to keep a dictionary of at least WSIZE
 * bytes. With this organization, matches are limited to a distance of
 * WSIZE-MAX_MATCH bytes, but this ensures that IO is always
 * performed with a length multiple of the block size. Also, it limits
 * the window size to 64K, which is quite useful on MSDOS.
 * To do: limit the window size to WSIZE+BSZ if SMALL_MEM (the code would
 * be less efficient).
 */

/* DECLARE(Pos, prev, WSIZE); */
/* Link to older string with same hash index. To limit the size of this
 * array to 64K, this link is maintained only for the last 32K strings.
 * An index in this array is thus a window index modulo 32K.
 */

/* DECLARE(Pos, head, 1<<HASH_BITS); */
/* Heads of the hash chains or NIL. */

static const ulg window_size = (ulg) 2 * WSIZE;

/* window size, 2*WSIZE except for MMAP or BIG_MEM, where it is the
 * input file length plus MIN_LOOKAHEAD.
 */

static long block_start;

/* window position at the beginning of the current output block. Gets
 * negative when the window is moved backwards.
 */

static unsigned ins_h;	/* hash index of string to be inserted */

#define H_SHIFT  ((HASH_BITS+MIN_MATCH-1)/MIN_MATCH)
/* Number of bits by which ins_h and del_h must be shifted at each
 * input step. It must be such that after MIN_MATCH steps, the oldest
 * byte no longer takes part in the hash key, that is:
 *   H_SHIFT * MIN_MATCH >= HASH_BITS
 */

static unsigned int prev_length;

/* Length of the best match at previous step. Matches not greater than this
 * are discarded. This is used in the lazy match evaluation.
 */

static unsigned strstart;	/* start of string to insert */
static unsigned match_start;	/* start of matching string */
static int eofile;		/* flag set at end of input file */
static unsigned lookahead;	/* number of valid bytes ahead in window */

static const unsigned max_chain_length = 4096;

/* To speed up deflation, hash chains are never searched beyond this length.
 * A higher limit improves compression ratio but degrades the speed.
 */

static const unsigned int max_lazy_match = 258;

/* Attempt to find a better match only when the current match is strictly
 * smaller than this value. This mechanism is used only for compression
 * levels >= 4.
 */
#define max_insert_length  max_lazy_match
/* Insert new strings in the hash table only if the match length
 * is not greater than this length. This saves time but degrades compression.
 * max_insert_length is used only for compression levels <= 3.
 */

static const unsigned good_match = 32;

/* Use a faster search when the previous match is longer than this */


/* Values for max_lazy_match, good_match and max_chain_length, depending on
 * the desired pack level (0..9). The values given below have been tuned to
 * exclude worst case performance for pathological files. Better values may be
 * found for specific files.
 */

static const int nice_match = 258;	/* Stop searching when current match exceeds this */

/* Note: the deflate() code requires max_lazy >= MIN_MATCH and max_chain >= 4
 * For deflate_fast() (levels <= 3) good is ignored and lazy has a different
 * meaning.
 */

#define EQUAL 0
/* result of memcmp for equal strings */

/* ===========================================================================
 *  Prototypes for local functions.
 */
static void fill_window(void);

static int longest_match(IPos cur_match);

#ifdef DEBUG
static void check_match(IPos start, IPos match, int length);
#endif

/* ===========================================================================
 * Update a hash value with the given input byte
 * IN  assertion: all calls to to UPDATE_HASH are made with consecutive
 *    input characters, so that a running hash key can be computed from the
 *    previous key instead of complete recalculation each time.
 */
#define UPDATE_HASH(h,c) (h = (((h)<<H_SHIFT) ^ (c)) & HASH_MASK)

/* ===========================================================================
 * Insert string s in the dictionary and set match_head to the previous head
 * of the hash chain (the most recent string with same hash key). Return
 * the previous length of the hash chain.
 * IN  assertion: all calls to to INSERT_STRING are made with consecutive
 *    input characters and the first MIN_MATCH bytes of s are valid
 *    (except for the last MIN_MATCH-1 bytes of the input file).
 */
#define INSERT_STRING(s, match_head) \
   (UPDATE_HASH(ins_h, window[(s) + MIN_MATCH-1]), \
    prev[(s) & WMASK] = match_head = head[ins_h], \
    head[ins_h] = (s))

/* ===========================================================================
 * Initialize the "longest match" routines for a new file
 */
static void lm_init(ush * flags)
{
	register unsigned j;

	/* Initialize the hash table. */
	memzero((char *) head, HASH_SIZE * sizeof(*head));
	/* prev will be initialized on the fly */

	*flags |= SLOW;
	/* ??? reduce max_chain_length for binary files */

	strstart = 0;
	block_start = 0L;

	lookahead = read_buf((char *) window,
						 sizeof(int) <= 2 ? (unsigned) WSIZE : 2 * WSIZE);

	if (lookahead == 0 || lookahead == (unsigned) EOF) {
		eofile = 1, lookahead = 0;
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
	register uch *scan = window + strstart;	/* current string */
	register uch *match;	/* matched string */
	register int len;	/* length of current match */
	int best_len = prev_length;	/* best match length so far */
	IPos limit =
		strstart > (IPos) MAX_DIST ? strstart - (IPos) MAX_DIST : NIL;
	/* Stop when cur_match becomes <= limit. To simplify the code,
	 * we prevent matches with the string of window index 0.
	 */

/* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
 * It is easy to get rid of this optimization if necessary.
 */
#if HASH_BITS < 8 || MAX_MATCH != 258
#  error Code too clever
#endif
	register uch *strend = window + strstart + MAX_MATCH;
	register uch scan_end1 = scan[best_len - 1];
	register uch scan_end = scan[best_len];

	/* Do not waste too much time if we already have a good match: */
	if (prev_length >= good_match) {
		chain_length >>= 2;
	}
	Assert(strstart <= window_size - MIN_LOOKAHEAD, "insufficient lookahead");

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
	if (memcmp((char *) window + match,
			   (char *) window + start, length) != EQUAL) {
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
#  define check_match(start, match, length)
#endif

/* ===========================================================================
 * Fill the window when the lookahead becomes insufficient.
 * Updates strstart and lookahead, and sets eofile if end of input file.
 * IN assertion: lookahead < MIN_LOOKAHEAD && strstart + lookahead > 0
 * OUT assertions: at least one byte has been read, or eofile is set;
 *    file reads are performed for at least two bytes (required for the
 *    translate_eol option).
 */
static void fill_window()
{
	register unsigned n, m;
	unsigned more =
		(unsigned) (window_size - (ulg) lookahead - (ulg) strstart);
	/* Amount of free space at the end of the window. */

	/* If the window is almost full and there is insufficient lookahead,
	 * move the upper half to the lower one to make room in the upper half.
	 */
	if (more == (unsigned) EOF) {
		/* Very unlikely, but possible on 16 bit machine if strstart == 0
		 * and lookahead == 1 (input done one byte at time)
		 */
		more--;
	} else if (strstart >= WSIZE + MAX_DIST) {
		/* By the IN assertion, the window is not empty so we can't confuse
		 * more == 0 with more == 64K on a 16 bit machine.
		 */
		Assert(window_size == (ulg) 2 * WSIZE, "no sliding with BIG_MEM");

		memcpy((char *) window, (char *) window + WSIZE, (unsigned) WSIZE);
		match_start -= WSIZE;
		strstart -= WSIZE;	/* we now have strstart >= MAX_DIST: */

		block_start -= (long) WSIZE;

		for (n = 0; n < HASH_SIZE; n++) {
			m = head[n];
			head[n] = (Pos) (m >= WSIZE ? m - WSIZE : NIL);
		}
		for (n = 0; n < WSIZE; n++) {
			m = prev[n];
			prev[n] = (Pos) (m >= WSIZE ? m - WSIZE : NIL);
			/* If n is not on any hash chain, prev[n] is garbage but
			 * its value will never be used.
			 */
		}
		more += WSIZE;
	}
	/* At this point, more >= 2 */
	if (!eofile) {
		n = read_buf((char *) window + strstart + lookahead, more);
		if (n == 0 || n == (unsigned) EOF) {
			eofile = 1;
		} else {
			lookahead += n;
		}
	}
}

/* ===========================================================================
 * Flush the current block, with given end-of-file flag.
 * IN assertion: strstart is set to the end of the current match.
 */
#define FLUSH_BLOCK(eof) \
   flush_block(block_start >= 0L ? (char*)&window[(unsigned)block_start] : \
                (char*)NULL, (long)strstart - block_start, (eof))

/* ===========================================================================
 * Same as above, but achieves better compression. We use a lazy
 * evaluation for matches: a match is finally adopted only if there is
 * no better match at the next window position.
 */
static ulg deflate()
{
	IPos hash_head;		/* head of hash chain */
	IPos prev_match;	/* previous match */
	int flush;			/* set if current block must be flushed */
	int match_available = 0;	/* set if previous match exists */
	register unsigned match_length = MIN_MATCH - 1;	/* length of best match */

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

		if (hash_head != NIL && prev_length < max_lazy_match &&
			strstart - hash_head <= MAX_DIST) {
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

			flush =
				ct_tally(strstart - 1 - prev_match, prev_length - MIN_MATCH);

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
			if (flush)
				FLUSH_BLOCK(0), block_start = strstart;

		} else if (match_available) {
			/* If there was no match at the previous position, output a
			 * single literal. If there was a match but the current match
			 * is longer, truncate the previous match to a single literal.
			 */
			Tracevv((stderr, "%c", window[strstart - 1]));
			if (ct_tally(0, window[strstart - 1])) {
				FLUSH_BLOCK(0), block_start = strstart;
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

/* gzip (GNU zip) -- compress files with zip algorithm and 'compress' interface
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * The unzip code was written and put in the public domain by Mark Adler.
 * Portions of the lzw code are derived from the public domain 'compress'
 * written by Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
 *
 * See the license_msg below and the file COPYING for the software license.
 * See the file algorithm.doc for the compression algorithms and file formats.
 */

/* Compress files with zip algorithm and 'compress' interface.
 * See usage() and help() functions below for all options.
 * Outputs:
 *        file.gz:   compressed file with same mode, owner, and utimes
 *     or stdout with -c option or if stdin used as input.
 * If the output file name had to be truncated, the original name is kept
 * in the compressed file.
 */

		/* configuration */

typedef struct dirent dir_type;

typedef RETSIGTYPE(*sig_type) (int);

/* ======================================================================== */
int gzip_main(int argc, char **argv)
{
	int result;
	int inFileNum;
	int outFileNum;
	struct stat statBuf;
	char *delFileName;
	int tostdout = 0;
	int force = 0;
	int opt;

	while ((opt = getopt(argc, argv, "cf123456789dq")) != -1) {
		switch (opt) {
		case 'c':
			tostdout = 1;
			break;
		case 'f':
			force = 1;
			break;
			/* Ignore 1-9 (compression level) options */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			break;
		case 'q':
			break;
#ifdef CONFIG_GUNZIP
		case 'd':
			optind = 1;
			return gunzip_main(argc, argv);
#endif
		default:
			bb_show_usage();
		}
	}

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

	strncpy(z_suffix, Z_SUFFIX, sizeof(z_suffix) - 1);
	z_len = strlen(z_suffix);

	/* Allocate all global buffers (for DYN_ALLOC option) */
	ALLOC(uch, inbuf, INBUFSIZ + INBUF_EXTRA);
	ALLOC(uch, outbuf, OUTBUFSIZ + OUTBUF_EXTRA);
	ALLOC(ush, d_buf, DIST_BUFSIZE);
	ALLOC(uch, window, 2L * WSIZE);
	ALLOC(ush, tab_prefix, 1L << BITS);

	clear_bufs();
	part_nb = 0;

	if (optind == argc) {
		time_stamp = 0;
		ifile_size = -1L;
		zip(STDIN_FILENO, STDOUT_FILENO);
	} else {
		int i;

		for (i = optind; i < argc; i++) {
			char *path = NULL;

			if (strcmp(argv[i], "-") == 0) {
				time_stamp = 0;
				ifile_size = -1L;
				inFileNum = STDIN_FILENO;
				outFileNum = STDOUT_FILENO;
			} else {
				inFileNum = open(argv[i], O_RDONLY);
				if (inFileNum < 0 || fstat(inFileNum, &statBuf) < 0)
					bb_perror_msg_and_die("%s", argv[i]);
				time_stamp = statBuf.st_ctime;
				ifile_size = statBuf.st_size;

				if (!tostdout) {
					path = xmalloc(strlen(argv[i]) + 4);
					strcpy(path, argv[i]);
					strcat(path, ".gz");

					/* Open output file */
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
					outFileNum =
						open(path, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW);
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

			if (path == NULL && isatty(outFileNum) && force == 0) {
				bb_error_msg
					("compressed data not written to a terminal. Use -f to force compression.");
				free(path);
				continue;
			}

			result = zip(inFileNum, outFileNum);

			if (path != NULL) {
				close(inFileNum);
				close(outFileNum);

				/* Delete the original file */
				if (result == OK)
					delFileName = argv[i];
				else
					delFileName = path;

				if (unlink(delFileName) < 0)
					bb_perror_msg("%s", delFileName);
			}

			free(path);
		}
	}

	return (exit_code);
}

/* trees.c -- output deflated data using Huffman coding
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

/*
 *  PURPOSE
 *
 *      Encode various sets of source values using variable-length
 *      binary code trees.
 *
 *  DISCUSSION
 *
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
 *
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
 *
 *      void ct_init (ush *attr, int *methodp)
 *          Allocate the match buffer, initialize the various tables and save
 *          the location of the internal file attribute (ascii/binary) and
 *          method (DEFLATE/STORE)
 *
 *      void ct_tally (int dist, int lc);
 *          Save the match info and tally the frequency counts.
 *
 *      long flush_block (char *buf, ulg stored_len, int eof)
 *          Determine the best encoding for the current block: dynamic trees,
 *          static trees or store, and output the encoded block to the zip
 *          file. Returns the total compressed length for the file so far.
 *
 */

/* ===========================================================================
 * Constants
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
static const extra_bits_t extra_lbits[LENGTH_CODES]
	= { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
	4, 4, 5, 5, 5, 5, 0
};

/* extra bits for each distance code */
static const extra_bits_t extra_dbits[D_CODES]
	= { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
	10, 10, 11, 11, 12, 12, 13, 13
};

/* extra bits for each bit length code */
static const extra_bits_t extra_blbits[BL_CODES]
= { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7 };

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
#if LIT_BUFSIZE > INBUFSIZ
#error cannot overlay l_buf and inbuf
#endif
#define REP_3_6      16
/* repeat previous bit length 3-6 times (2 bits of repeat count) */
#define REPZ_3_10    17
/* repeat a zero length 3-10 times  (3 bits of repeat count) */
#define REPZ_11_138  18
/* repeat a zero length 11-138 times  (7 bits of repeat count) */

/* ===========================================================================
 * Local data
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

#define HEAP_SIZE (2*L_CODES+1)
/* maximum heap size */

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

static tree_desc l_desc =
	{ dyn_ltree, static_ltree, extra_lbits, LITERALS + 1, L_CODES,
	MAX_BITS, 0
};

static tree_desc d_desc =
	{ dyn_dtree, static_dtree, extra_dbits, 0, D_CODES, MAX_BITS, 0 };

static tree_desc bl_desc =
	{ bl_tree, (ct_data *) 0, extra_blbits, 0, BL_CODES, MAX_BL_BITS,
	0
};


static ush bl_count[MAX_BITS + 1];

/* number of codes at each bit length for an optimal tree */

static const uch bl_order[BL_CODES]
= { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

/* The lengths of the bit length codes are sent in order of decreasing
 * probability, to avoid transmitting the lengths for unused bit length codes.
 */

static int heap[2 * L_CODES + 1];	/* heap used to build the Huffman trees */
static int heap_len;	/* number of elements in the heap */
static int heap_max;	/* element of largest frequency */

/* The sons of heap[n] are heap[2*n] and heap[2*n+1]. heap[0] is not used.
 * The same heap array is used to build all trees.
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

#define l_buf inbuf
/* DECLARE(uch, l_buf, LIT_BUFSIZE);  buffer for literals or lengths */

/* DECLARE(ush, d_buf, DIST_BUFSIZE); buffer for distances */

static uch flag_buf[(LIT_BUFSIZE / 8)];

/* flag_buf is a bit array distinguishing literals from lengths in
 * l_buf, thus indicating the presence or absence of a distance.
 */

static unsigned last_lit;	/* running index in l_buf */
static unsigned last_dist;	/* running index in d_buf */
static unsigned last_flags;	/* running index in flag_buf */
static uch flags;		/* current flags not yet saved in flag_buf */
static uch flag_bit;	/* current bit used in flags */

/* bits are filled in flags starting at bit 0 (least significant).
 * Note: these flags are overkill in the current code since we don't
 * take advantage of DIST_BUFSIZE == LIT_BUFSIZE.
 */

static ulg opt_len;		/* bit length of current block with optimal trees */
static ulg static_len;	/* bit length of current block with static trees */

static ulg compressed_len;	/* total bit length of compressed file */


static ush *file_type;	/* pointer to UNKNOWN, BINARY or ASCII */
static int *file_method;	/* pointer to DEFLATE or STORE */

/* ===========================================================================
 * Local (static) routines in this file.
 */

static void init_block(void);
static void pqdownheap(ct_data * tree, int k);
static void gen_bitlen(tree_desc * desc);
static void gen_codes(ct_data * tree, int max_code);
static void build_tree(tree_desc * desc);
static void scan_tree(ct_data * tree, int max_code);
static void send_tree(ct_data * tree, int max_code);
static int build_bl_tree(void);
static void send_all_trees(int lcodes, int dcodes, int blcodes);
static void compress_block(ct_data * ltree, ct_data * dtree);
static void set_file_type(void);


#ifndef DEBUG
#  define send_code(c, tree) send_bits(tree[c].Code, tree[c].Len)
   /* Send a code of the given tree. c and tree must not have side effects */

#else							/* DEBUG */
#  define send_code(c, tree) \
     { if (verbose>1) bb_error_msg("\ncd %3d ",(c)); \
       send_bits(tree[c].Code, tree[c].Len); }
#endif

#define d_code(dist) \
   ((dist) < 256 ? dist_code[dist] : dist_code[256+((dist)>>7)])
/* Mapping from a distance to a distance code. dist is the distance - 1 and
 * must not have side effects. dist_code[256] and dist_code[257] are never
 * used.
 */

/* the arguments must not have side effects */

/* ===========================================================================
 * Allocate the match buffer, initialize the various tables and save the
 * location of the internal file attribute (ascii/binary) and method
 * (DEFLATE/STORE).
 */
static void ct_init(ush * attr, int *methodp)
{
	int n;				/* iterates over tree elements */
	int bits;			/* bit counter */
	int length;			/* length value */
	int code;			/* code value */
	int dist;			/* distance index */

	file_type = attr;
	file_method = methodp;
	compressed_len = 0L;

	if (static_dtree[0].Len != 0)
		return;			/* ct_init already called */

	/* Initialize the mapping length (0..255) -> length code (0..28) */
	length = 0;
	for (code = 0; code < LENGTH_CODES - 1; code++) {
		base_length[code] = length;
		for (n = 0; n < (1 << extra_lbits[code]); n++) {
			length_code[length++] = (uch) code;
		}
	}
	Assert(length == 256, "ct_init: length != 256");
	/* Note that the length 255 (match length 258) can be represented
	 * in two different ways: code 284 + 5 bits or code 285, so we
	 * overwrite length_code[255] to use the best encoding:
	 */
	length_code[length - 1] = (uch) code;

	/* Initialize the mapping dist (0..32K) -> dist code (0..29) */
	dist = 0;
	for (code = 0; code < 16; code++) {
		base_dist[code] = dist;
		for (n = 0; n < (1 << extra_dbits[code]); n++) {
			dist_code[dist++] = (uch) code;
		}
	}
	Assert(dist == 256, "ct_init: dist != 256");
	dist >>= 7;			/* from now on, all distances are divided by 128 */
	for (; code < D_CODES; code++) {
		base_dist[code] = dist << 7;
		for (n = 0; n < (1 << (extra_dbits[code] - 7)); n++) {
			dist_code[256 + dist++] = (uch) code;
		}
	}
	Assert(dist == 256, "ct_init: 256+dist != 512");

	/* Construct the codes of the static literal tree */
	for (bits = 0; bits <= MAX_BITS; bits++)
		bl_count[bits] = 0;
	n = 0;
	while (n <= 143)
		static_ltree[n++].Len = 8, bl_count[8]++;
	while (n <= 255)
		static_ltree[n++].Len = 9, bl_count[9]++;
	while (n <= 279)
		static_ltree[n++].Len = 7, bl_count[7]++;
	while (n <= 287)
		static_ltree[n++].Len = 8, bl_count[8]++;
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
 * Initialize a new block.
 */
static void init_block()
{
	int n;				/* iterates over tree elements */

	/* Initialize the trees. */
	for (n = 0; n < L_CODES; n++)
		dyn_ltree[n].Freq = 0;
	for (n = 0; n < D_CODES; n++)
		dyn_dtree[n].Freq = 0;
	for (n = 0; n < BL_CODES; n++)
		bl_tree[n].Freq = 0;

	dyn_ltree[END_BLOCK].Freq = 1;
	opt_len = static_len = 0L;
	last_lit = last_dist = last_flags = 0;
	flags = 0;
	flag_bit = 1;
}

#define SMALLEST 1
/* Index within the heap array of least frequent node in the Huffman tree */


/* ===========================================================================
 * Remove the smallest element from the heap and recreate the heap with
 * one less element. Updates heap and heap_len.
 */
#define pqremove(tree, top) \
{\
    top = heap[SMALLEST]; \
    heap[SMALLEST] = heap[heap_len--]; \
    pqdownheap(tree, SMALLEST); \
}

/* ===========================================================================
 * Compares to subtrees, using the tree depth as tie breaker when
 * the subtrees have equal frequency. This minimizes the worst case length.
 */
#define smaller(tree, n, m) \
   (tree[n].Freq < tree[m].Freq || \
   (tree[n].Freq == tree[m].Freq && depth[n] <= depth[m]))

/* ===========================================================================
 * Restore the heap property by moving down the tree starting at node k,
 * exchanging a node with the smallest of its two sons if necessary, stopping
 * when the heap property is re-established (each father smaller than its
 * two sons).
 */
static void pqdownheap(ct_data * tree, int k)
{
	int v = heap[k];
	int j = k << 1;		/* left son of k */

	while (j <= heap_len) {
		/* Set j to the smallest of the two sons: */
		if (j < heap_len && smaller(tree, heap[j + 1], heap[j]))
			j++;

		/* Exit if v is smaller than both sons */
		if (smaller(tree, v, heap[j]))
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
		if (bits > max_length)
			bits = max_length, overflow++;
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
			static_len += (ulg) f *(stree[n].Len + xbits);
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
				Trace((stderr, "code %d bits %d->%d\n", m, tree[m].Len,
					   bits));
				opt_len +=
					((long) bits - (long) tree[m].Len) * (long) tree[m].Freq;
				tree[m].Len = (ush) bits;
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
		pqremove(tree, n);	/* n = node of least frequency */
		m = heap[SMALLEST];	/* m = node of next least frequency */

		heap[--heap_max] = n;	/* keep the nodes sorted by frequency */
		heap[--heap_max] = m;

		/* Create a new node father of n and m */
		tree[node].Freq = tree[n].Freq + tree[m].Freq;
		depth[node] = (uch) (MAX(depth[n], depth[m]) + 1);
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

	if (nextlen == 0)
		max_count = 138, min_count = 3;
	tree[max_code + 1].Len = (ush) 0xffff;	/* guard */

	for (n = 0; n <= max_code; n++) {
		curlen = nextlen;
		nextlen = tree[n + 1].Len;
		if (++count < max_count && curlen == nextlen) {
			continue;
		} else if (count < min_count) {
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
		if (nextlen == 0) {
			max_count = 138, min_count = 3;
		} else if (curlen == nextlen) {
			max_count = 6, min_count = 3;
		} else {
			max_count = 7, min_count = 4;
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
				send_code(curlen, bl_tree);
			} while (--count != 0);

		} else if (curlen != 0) {
			if (curlen != prevlen) {
				send_code(curlen, bl_tree);
				count--;
			}
			Assert(count >= 3 && count <= 6, " 3_6?");
			send_code(REP_3_6, bl_tree);
			send_bits(count - 3, 2);

		} else if (count <= 10) {
			send_code(REPZ_3_10, bl_tree);
			send_bits(count - 3, 3);

		} else {
			send_code(REPZ_11_138, bl_tree);
			send_bits(count - 11, 7);
		}
		count = 0;
		prevlen = curlen;
		if (nextlen == 0) {
			max_count = 138, min_count = 3;
		} else if (curlen == nextlen) {
			max_count = 6, min_count = 3;
		} else {
			max_count = 7, min_count = 4;
		}
	}
}

/* ===========================================================================
 * Construct the Huffman tree for the bit lengths and return the index in
 * bl_order of the last bit length code to send.
 */
static const int build_bl_tree()
{
	int max_blindex;	/* index of last bit length code of non zero freq */

	/* Determine the bit length frequencies for literal and distance trees */
	scan_tree((ct_data *) dyn_ltree, l_desc.max_code);
	scan_tree((ct_data *) dyn_dtree, d_desc.max_code);

	/* Build the bit length tree: */
	build_tree((tree_desc *) (&bl_desc));
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
 * Determine the best encoding for the current block: dynamic trees, static
 * trees or store, and output the encoded block to the zip file. This function
 * returns the total compressed length for the file so far.
 */
static ulg flush_block(char *buf, ulg stored_len, int eof)
{
	ulg opt_lenb, static_lenb;	/* opt_len and static_len in bytes */
	int max_blindex;	/* index of last bit length code of non zero freq */

	flag_buf[last_flags] = flags;	/* Save the flags for the last 8 items */

	/* Check if the file is ascii or binary */
	if (*file_type == (ush) UNKNOWN)
		set_file_type();

	/* Construct the literal and distance trees */
	build_tree((tree_desc *) (&l_desc));
	Tracev((stderr, "\nlit data: dyn %ld, stat %ld", opt_len, static_len));

	build_tree((tree_desc *) (&d_desc));
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
		if (buf == (char *) 0)
			bb_error_msg("block vanished");

		copy_block(buf, (unsigned) stored_len, 0);	/* without header */
		compressed_len = stored_len << 3;
		*file_method = STORED;

	} else if (stored_len + 4 <= opt_lenb && buf != (char *) 0) {
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
 * Save the match info and tally the frequency counts. Return true if
 * the current block must be flushed.
 */
static int ct_tally(int dist, int lc)
{
	l_buf[last_lit++] = (uch) lc;
	if (dist == 0) {
		/* lc is the unmatched char */
		dyn_ltree[lc].Freq++;
	} else {
		/* Here, lc is the match length - MIN_MATCH */
		dist--;			/* dist = match distance - 1 */
		Assert((ush) dist < (ush) MAX_DIST &&
			   (ush) lc <= (ush) (MAX_MATCH - MIN_MATCH) &&
			   (ush) d_code(dist) < (ush) D_CODES, "ct_tally: bad match");

		dyn_ltree[length_code[lc] + LITERALS + 1].Freq++;
		dyn_dtree[d_code(dist)].Freq++;

		d_buf[last_dist++] = (ush) dist;
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
		ulg out_length = (ulg) last_lit * 8L;
		ulg in_length = (ulg) strstart - block_start;
		int dcode;

		for (dcode = 0; dcode < D_CODES; dcode++) {
			out_length +=
				(ulg) dyn_dtree[dcode].Freq * (5L + extra_dbits[dcode]);
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
	unsigned dist;		/* distance of matched string */
	int lc;				/* match length or unmatched char (if dist == 0) */
	unsigned lx = 0;	/* running index in l_buf */
	unsigned dx = 0;	/* running index in d_buf */
	unsigned fx = 0;	/* running index in flag_buf */
	uch flag = 0;		/* current flags */
	unsigned code;		/* the code to send */
	int extra;			/* number of extra bits to send */

	if (last_lit != 0)
		do {
			if ((lx & 7) == 0)
				flag = flag_buf[fx++];
			lc = l_buf[lx++];
			if ((flag & 1) == 0) {
				send_code(lc, ltree);	/* send a literal byte */
				Tracecv(isgraph(lc), (stderr, " '%c' ", lc));
			} else {
				/* Here, lc is the match length - MIN_MATCH */
				code = length_code[lc];
				send_code(code + LITERALS + 1, ltree);	/* send the length code */
				extra = extra_lbits[code];
				if (extra != 0) {
					lc -= base_length[code];
					send_bits(lc, extra);	/* send the extra length bits */
				}
				dist = d_buf[dx++];
				/* Here, dist is the match distance - 1 */
				code = d_code(dist);
				Assert(code < D_CODES, "bad d_code");

				send_code(code, dtree);	/* send the distance code */
				extra = extra_dbits[code];
				if (extra != 0) {
					dist -= base_dist[code];
					send_bits(dist, extra);	/* send the extra distance bits */
				}
			}			/* literal or match pair ? */
			flag >>= 1;
		} while (lx < last_lit);

	send_code(END_BLOCK, ltree);
}

/* ===========================================================================
 * Set the file type to ASCII or BINARY, using a crude approximation:
 * binary if more than 20% of the bytes are <= 6 or >= 128, ascii otherwise.
 * IN assertion: the fields freq of dyn_ltree are set and the total of all
 * frequencies does not exceed 64K (to fit in an int on 16 bit machines).
 */
static void set_file_type()
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
	*file_type = bin_freq > (ascii_freq >> 2) ? BINARY : ASCII;
	if (*file_type == BINARY && translate_eol) {
		bb_error_msg("-l used on binary file");
	}
}

/* zip.c -- compress files to the gzip or pkzip format
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */


static ulg crc;			/* crc on uncompressed file data */
static long header_bytes;	/* number of bytes in gzip header */

static void put_long(ulg n)
{
	put_short((n) & 0xffff);
	put_short(((ulg) (n)) >> 16);
}

/* put_header_byte is used for the compressed output
 * - for the initial 4 bytes that can't overflow the buffer.
 */
#define put_header_byte(c) {outbuf[outcnt++]=(uch)(c);}

/* ===========================================================================
 * Deflate in to out.
 * IN assertions: the input and output buffers are cleared.
 *   The variables time_stamp and save_orig_name are initialized.
 */
static int zip(int in, int out)
{
	uch my_flags = 0;	/* general purpose bit flags */
	ush attr = 0;		/* ascii/binary flag */
	ush deflate_flags = 0;	/* pkzip -es, -en or -ex equivalent */

	ifd = in;
	ofd = out;
	outcnt = 0;

	/* Write the header to the gzip file. See algorithm.doc for the format */


	method = DEFLATED;
	put_header_byte(GZIP_MAGIC[0]);	/* magic header */
	put_header_byte(GZIP_MAGIC[1]);
	put_header_byte(DEFLATED);	/* compression method */

	put_header_byte(my_flags);	/* general flags */
	put_long(time_stamp);

	/* Write deflated file to zip file */
	crc = updcrc(0, 0);

	bi_init(out);
	ct_init(&attr, &method);
	lm_init(&deflate_flags);

	put_byte((uch) deflate_flags);	/* extra flags */
	put_byte(OS_CODE);	/* OS identifier */

	header_bytes = (long) outcnt;

	(void) deflate();

	/* Write the crc and uncompressed size */
	put_long(crc);
	put_long(isize);
	header_bytes += 2 * sizeof(long);

	flush_outbuf();
	return OK;
}


/* ===========================================================================
 * Read a new buffer from the current input file, perform end-of-line
 * translation, and update the crc and input file size.
 * IN assertion: size >= 2 (for end-of-line translation)
 */
static int file_read(char *buf, unsigned size)
{
	unsigned len;

	Assert(insize == 0, "inbuf not empty");

	len = read(ifd, buf, size);
	if (len == (unsigned) (-1) || len == 0)
		return (int) len;

	crc = updcrc((uch *) buf, len);
	isize += (ulg) len;
	return (int) len;
}

/* ===========================================================================
 * Write the output buffer outbuf[0..outcnt-1] and update bytes_out.
 * (used for the compressed data only)
 */
static void flush_outbuf()
{
	if (outcnt == 0)
		return;

	write_buf(ofd, (char *) outbuf, outcnt);
	outcnt = 0;
}
