/* md5sum.c - Compute MD5 checksum of files or strings according to the
 *            definition of MD5 in RFC 1321 from April 1992.
 * Copyright (C) 1995-1999 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu> */
/* Hacked to work with BusyBox by Alfred M. Szmidt <ams@trillian.itslinux.org> */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <sys/types.h>
#if defined HAVE_LIMITS_H
# include <limits.h>
#endif
#include "busybox.h"

/* For some silly reason, this file uses backwards TRUE and FALSE conventions */
#undef TRUE
#undef FALSE
#define FALSE   ((int) 1)
#define TRUE    ((int) 0)

//----------------------------------------------------------------------------
//--------md5.c
//----------------------------------------------------------------------------

/* md5.c - Functions to compute MD5 message digest of files or memory blocks
 *         according to the definition of MD5 in RFC 1321 from April 1992.
 * Copyright (C) 1995, 1996 Free Software Foundation, Inc.
 *
 * NOTE: The canonical source of this file is maintained with the GNU C
 * Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.  */

//----------------------------------------------------------------------------
//--------md5.h
//----------------------------------------------------------------------------

/* md5.h - Declaration of functions and data types used for MD5 sum
   computing library functions.
   Copyright (C) 1995, 1996 Free Software Foundation, Inc.
   NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _MD5_H
static const int _MD5_H = 1;

/* The following contortions are an attempt to use the C preprocessor
   to determine an unsigned integral type that is 32 bits wide.  An
   alternative approach is to use autoconf's AC_CHECK_SIZEOF macro, but
   doing that would require that the configure script compile and *run*
   the resulting executable.  Locally running cross-compiled executables
   is usually not possible.  */

typedef u_int32_t md5_uint32;

/* Structure to save state of computation between the single steps.  */
struct md5_ctx
{
  md5_uint32 A;
  md5_uint32 B;
  md5_uint32 C;
  md5_uint32 D;

  md5_uint32 total[2];
  md5_uint32 buflen;
  char buffer[128];
};

/*
 * The following three functions are build up the low level used in
 * the functions `md5_stream' and `md5_buffer'.
 */

/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
extern void md5_init_ctx __P ((struct md5_ctx *ctx));

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is necessary that LEN is a multiple of 64!!! */
extern void md5_process_block __P ((const void *buffer, size_t len,
				    struct md5_ctx *ctx));

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is NOT required that LEN is a multiple of 64.  */
extern void md5_process_bytes __P ((const void *buffer, size_t len,
				    struct md5_ctx *ctx));

/* Process the remaining bytes in the buffer and put result from CTX
   in first 16 bytes following RESBUF.  The result is always in little
   endian byte order, so that a byte-wise output yields to the wanted
   ASCII representation of the message digest.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
extern void *md5_finish_ctx __P ((struct md5_ctx *ctx, void *resbuf));


/* Put result from CTX in first 16 bytes following RESBUF.  The result is
   always in little endian byte order, so that a byte-wise output yields
   to the wanted ASCII representation of the message digest.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
extern void *md5_read_ctx __P ((const struct md5_ctx *ctx, void *resbuf));


/* Compute MD5 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
extern int md5_stream __P ((FILE *stream, void *resblock));

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
extern void *md5_buffer __P ((const char *buffer, size_t len, void *resblock));

#endif

//----------------------------------------------------------------------------
//--------end of md5.h
//----------------------------------------------------------------------------

/* Handle endian-ness */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	#define SWAP(n) (n)
#else
	#define SWAP(n) ((n << 24) | ((n&65280)<<8) | ((n&16711680)>>8) | (n>>24))
#endif



/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (RFC 1321, 3.1: Step 1)  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */  };

/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
void md5_init_ctx(struct md5_ctx *ctx)
{
  ctx->A = 0x67452301;
  ctx->B = 0xefcdab89;
  ctx->C = 0x98badcfe;
  ctx->D = 0x10325476;

  ctx->total[0] = ctx->total[1] = 0;
  ctx->buflen = 0;
}

/* Put result from CTX in first 16 bytes following RESBUF.  The result
   must be in little endian byte order.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *md5_read_ctx(const struct md5_ctx *ctx, void *resbuf)
{
  ((md5_uint32 *) resbuf)[0] = SWAP(ctx->A);
  ((md5_uint32 *) resbuf)[1] = SWAP(ctx->B);
  ((md5_uint32 *) resbuf)[2] = SWAP(ctx->C);
  ((md5_uint32 *) resbuf)[3] = SWAP(ctx->D);

  return resbuf;
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *md5_finish_ctx(struct md5_ctx *ctx, void *resbuf)
{
  /* Take yet unprocessed bytes into account.  */
  md5_uint32 bytes = ctx->buflen;
  size_t pad;

  /* Now count remaining bytes.  */
  ctx->total[0] += bytes;
  if (ctx->total[0] < bytes)
    ++ctx->total[1];

  pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
  memcpy(&ctx->buffer[bytes], fillbuf, pad);

  /* Put the 64-bit file length in *bits* at the end of the buffer.  */
  *(md5_uint32 *) & ctx->buffer[bytes + pad] = SWAP(ctx->total[0] << 3);
  *(md5_uint32 *) & ctx->buffer[bytes + pad + 4] =
    SWAP( ((ctx->total[1] << 3) | (ctx->total[0] >> 29)) );

  /* Process last bytes.  */
  md5_process_block(ctx->buffer, bytes + pad + 8, ctx);

  return md5_read_ctx(ctx, resbuf);
}

/* Compute MD5 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
int md5_stream(FILE *stream, void *resblock)
{
  /* Important: BLOCKSIZE must be a multiple of 64.  */
static const int BLOCKSIZE = 4096;
  struct md5_ctx ctx;
  char buffer[BLOCKSIZE + 72];
  size_t sum;

  /* Initialize the computation context.  */
  md5_init_ctx(&ctx);

  /* Iterate over full file contents.  */
  while (1) {
    /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
       computation function processes the whole buffer so that with the
       next round of the loop another block can be read.  */
    size_t n;
    sum = 0;

    /* Read block.  Take care for partial reads.  */
    do {
      n = fread(buffer + sum, 1, BLOCKSIZE - sum, stream);

      sum += n;
    }
    while (sum < BLOCKSIZE && n != 0);
    if (n == 0 && ferror(stream))
      return 1;

    /* If end of file is reached, end the loop.  */
    if (n == 0)
      break;

    /* Process buffer with BLOCKSIZE bytes.  Note that
       BLOCKSIZE % 64 == 0
    */
    md5_process_block(buffer, BLOCKSIZE, &ctx);
  }

  /* Add the last bytes if necessary.  */
  if (sum > 0)
    md5_process_bytes(buffer, sum, &ctx);

  /* Construct result in desired memory.  */
  md5_finish_ctx(&ctx, resblock);
  return 0;
}

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
void *md5_buffer(const char *buffer, size_t len, void *resblock)
{
  struct md5_ctx ctx;

  /* Initialize the computation context.  */
  md5_init_ctx(&ctx);

  /* Process whole buffer but last len % 64 bytes.  */
  md5_process_bytes(buffer, len, &ctx);

  /* Put result in desired memory area.  */
  return md5_finish_ctx(&ctx, resblock);
}

void md5_process_bytes(const void *buffer, size_t len, struct md5_ctx *ctx)
{
  /* When we already have some bits in our internal buffer concatenate
     both inputs first.  */
  if (ctx->buflen != 0) {
    size_t left_over = ctx->buflen;
    size_t add = 128 - left_over > len ? len : 128 - left_over;

    memcpy(&ctx->buffer[left_over], buffer, add);
    ctx->buflen += add;

    if (left_over + add > 64) {
      md5_process_block(ctx->buffer, (left_over + add) & ~63, ctx);
      /* The regions in the following copy operation cannot overlap.  */
      memcpy(ctx->buffer, &ctx->buffer[(left_over + add) & ~63],
	     (left_over + add) & 63);
      ctx->buflen = (left_over + add) & 63;
    }

    buffer = (const char *) buffer + add;
    len -= add;
  }

  /* Process available complete blocks.  */
  if (len > 64) {
    md5_process_block(buffer, len & ~63, ctx);
    buffer = (const char *) buffer + (len & ~63);
    len &= 63;
  }

  /* Move remaining bytes in internal buffer.  */
  if (len > 0) {
    memcpy(ctx->buffer, buffer, len);
    ctx->buflen = len;
  }
}

/* These are the four functions used in the four steps of the MD5 algorithm
   and defined in the RFC 1321.  The first function is a little bit optimized
   (as found in Colin Plumbs public domain implementation).  */
/* #define FF(b, c, d) ((b & c) | (~b & d)) */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF (d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.  */
void md5_process_block(const void *buffer, size_t len, struct md5_ctx *ctx)
{
  md5_uint32 correct_words[16];
  const md5_uint32 *words = buffer;
  size_t nwords = len / sizeof(md5_uint32);
  const md5_uint32 *endp = words + nwords;
  md5_uint32 A = ctx->A;
  md5_uint32 B = ctx->B;
  md5_uint32 C = ctx->C;
  md5_uint32 D = ctx->D;

  /* First increment the byte count.  RFC 1321 specifies the possible
     length of the file up to 2^64 bits.  Here we only compute the
     number of bytes.  Do a double word increment.  */
  ctx->total[0] += len;
  if (ctx->total[0] < len)
    ++ctx->total[1];

  /* Process all bytes in the buffer with 64 bytes in each round of
     the loop.  */
  while (words < endp) {
    md5_uint32 *cwp = correct_words;
    md5_uint32 A_save = A;
    md5_uint32 B_save = B;
    md5_uint32 C_save = C;
    md5_uint32 D_save = D;

    /* First round: using the given function, the context and a constant
       the next context is computed.  Because the algorithms processing
       unit is a 32-bit word and it is determined to work on words in
       little endian byte order we perhaps have to change the byte order
       before the computation.  To reduce the work for the next steps
       we store the swapped words in the array CORRECT_WORDS.  */

#define OP(a, b, c, d, s, T)						\
      do								\
        {								\
	  a += FF (b, c, d) + (*cwp++ = SWAP (*words)) + T;		\
	  ++words;							\
	  CYCLIC (a, s);						\
	  a += b;							\
        }								\
      while (0)

    /* It is unfortunate that C does not provide an operator for
       cyclic rotation.  Hope the C compiler is smart enough.  */
#define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

    /* Before we start, one word to the strange constants.
       They are defined in RFC 1321 as

       T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64
    */

    /* Round 1.  */
    OP(A, B, C, D, 7, 0xd76aa478);
    OP(D, A, B, C, 12, 0xe8c7b756);
    OP(C, D, A, B, 17, 0x242070db);
    OP(B, C, D, A, 22, 0xc1bdceee);
    OP(A, B, C, D, 7, 0xf57c0faf);
    OP(D, A, B, C, 12, 0x4787c62a);
    OP(C, D, A, B, 17, 0xa8304613);
    OP(B, C, D, A, 22, 0xfd469501);
    OP(A, B, C, D, 7, 0x698098d8);
    OP(D, A, B, C, 12, 0x8b44f7af);
    OP(C, D, A, B, 17, 0xffff5bb1);
    OP(B, C, D, A, 22, 0x895cd7be);
    OP(A, B, C, D, 7, 0x6b901122);
    OP(D, A, B, C, 12, 0xfd987193);
    OP(C, D, A, B, 17, 0xa679438e);
    OP(B, C, D, A, 22, 0x49b40821);

    /* For the second to fourth round we have the possibly swapped words
       in CORRECT_WORDS.  Redefine the macro to take an additional first
       argument specifying the function to use.  */
#undef OP
#define OP(f, a, b, c, d, k, s, T)					\
      do 								\
	{								\
	  a += f (b, c, d) + correct_words[k] + T;			\
	  CYCLIC (a, s);						\
	  a += b;							\
	}								\
      while (0)

    /* Round 2.  */
    OP(FG, A, B, C, D, 1, 5, 0xf61e2562);
    OP(FG, D, A, B, C, 6, 9, 0xc040b340);
    OP(FG, C, D, A, B, 11, 14, 0x265e5a51);
    OP(FG, B, C, D, A, 0, 20, 0xe9b6c7aa);
    OP(FG, A, B, C, D, 5, 5, 0xd62f105d);
    OP(FG, D, A, B, C, 10, 9, 0x02441453);
    OP(FG, C, D, A, B, 15, 14, 0xd8a1e681);
    OP(FG, B, C, D, A, 4, 20, 0xe7d3fbc8);
    OP(FG, A, B, C, D, 9, 5, 0x21e1cde6);
    OP(FG, D, A, B, C, 14, 9, 0xc33707d6);
    OP(FG, C, D, A, B, 3, 14, 0xf4d50d87);
    OP(FG, B, C, D, A, 8, 20, 0x455a14ed);
    OP(FG, A, B, C, D, 13, 5, 0xa9e3e905);
    OP(FG, D, A, B, C, 2, 9, 0xfcefa3f8);
    OP(FG, C, D, A, B, 7, 14, 0x676f02d9);
    OP(FG, B, C, D, A, 12, 20, 0x8d2a4c8a);

    /* Round 3.  */
    OP(FH, A, B, C, D, 5, 4, 0xfffa3942);
    OP(FH, D, A, B, C, 8, 11, 0x8771f681);
    OP(FH, C, D, A, B, 11, 16, 0x6d9d6122);
    OP(FH, B, C, D, A, 14, 23, 0xfde5380c);
    OP(FH, A, B, C, D, 1, 4, 0xa4beea44);
    OP(FH, D, A, B, C, 4, 11, 0x4bdecfa9);
    OP(FH, C, D, A, B, 7, 16, 0xf6bb4b60);
    OP(FH, B, C, D, A, 10, 23, 0xbebfbc70);
    OP(FH, A, B, C, D, 13, 4, 0x289b7ec6);
    OP(FH, D, A, B, C, 0, 11, 0xeaa127fa);
    OP(FH, C, D, A, B, 3, 16, 0xd4ef3085);
    OP(FH, B, C, D, A, 6, 23, 0x04881d05);
    OP(FH, A, B, C, D, 9, 4, 0xd9d4d039);
    OP(FH, D, A, B, C, 12, 11, 0xe6db99e5);
    OP(FH, C, D, A, B, 15, 16, 0x1fa27cf8);
    OP(FH, B, C, D, A, 2, 23, 0xc4ac5665);

    /* Round 4.  */
    OP(FI, A, B, C, D, 0, 6, 0xf4292244);
    OP(FI, D, A, B, C, 7, 10, 0x432aff97);
    OP(FI, C, D, A, B, 14, 15, 0xab9423a7);
    OP(FI, B, C, D, A, 5, 21, 0xfc93a039);
    OP(FI, A, B, C, D, 12, 6, 0x655b59c3);
    OP(FI, D, A, B, C, 3, 10, 0x8f0ccc92);
    OP(FI, C, D, A, B, 10, 15, 0xffeff47d);
    OP(FI, B, C, D, A, 1, 21, 0x85845dd1);
    OP(FI, A, B, C, D, 8, 6, 0x6fa87e4f);
    OP(FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
    OP(FI, C, D, A, B, 6, 15, 0xa3014314);
    OP(FI, B, C, D, A, 13, 21, 0x4e0811a1);
    OP(FI, A, B, C, D, 4, 6, 0xf7537e82);
    OP(FI, D, A, B, C, 11, 10, 0xbd3af235);
    OP(FI, C, D, A, B, 2, 15, 0x2ad7d2bb);
    OP(FI, B, C, D, A, 9, 21, 0xeb86d391);

    /* Add the starting values of the context.  */
    A += A_save;
    B += B_save;
    C += C_save;
    D += D_save;
  }

  /* Put checksum in context given as argument.  */
  ctx->A = A;
  ctx->B = B;
  ctx->C = C;
  ctx->D = D;
}

//----------------------------------------------------------------------------
//--------end of md5.c
//----------------------------------------------------------------------------

#define ISWHITE(c) ((c) == ' ' || (c) == '\t')
#define IN_CTYPE_DOMAIN(c) 1
#define ISXDIGIT(c) (IN_CTYPE_DOMAIN (c) && isxdigit (c))
#define STREQ(a, b) (strcmp ((a), (b)) == 0)
#define TOLOWER(Ch) tolower (Ch)
#define OPENOPTS(BINARY) "r"

/* The minimum length of a valid digest line in a file produced
   by `md5sum FILE' and read by `md5sum -c'.  This length does
   not include any newline character at the end of a line.  */
static const int MIN_DIGEST_LINE_LENGTH = 35; /* 32 - message digest length
                                      2 - blank and binary indicator
                                      1 - minimum filename length */

static int have_read_stdin; /* Nonzero if any of the files read were
                               the standard input. */

static int status_only = 0; /* With -c, don't generate any output.
                               The exit code indicates success or failure */
static int warn = 0; /* With -w, print a message to standard error warning
                        about each improperly formatted MD5 checksum line */

static int split_3(char *s,
                   size_t s_len,
                   unsigned char **u,
                   int *binary,
                   char **w)
{
  size_t i = 0;
  int escaped_filename = 0;

  while (ISWHITE(s[i]))
    ++i;

  /* The line must have at least 35 (36 if the first is a backslash)
     more characters to contain correct message digest information.
     Ignore this line if it is too short.  */
  if (!(s_len - i >= MIN_DIGEST_LINE_LENGTH
        || (s[i] == '\\' && s_len - i >= 1 + MIN_DIGEST_LINE_LENGTH)))
    return FALSE;

  if (s[i] == '\\') {
    ++i;
    escaped_filename = 1;
  }
  *u = (unsigned char *) &s[i];

  /* The first field has to be the 32-character hexadecimal
     representation of the message digest.  If it is not followed
     immediately by a white space it's an error.  */
  i += 32;
  if (!ISWHITE(s[i]))
    return FALSE;

  s[i++] = '\0';

  if (s[i] != ' ' && s[i] != '*')
    return FALSE;
  *binary = (s[i++] == '*');

  /* All characters between the type indicator and end of line are
     significant -- that includes leading and trailing white space.  */
  *w = &s[i];

  if (escaped_filename) {
    /* Translate each `\n' string in the file name to a NEWLINE,
       and each `\\' string to a backslash.  */

    char *dst = &s[i];

    while (i < s_len) {
      switch (s[i]) {
       case '\\':
        if (i == s_len - 1) {
          /* A valid line does not end with a backslash.  */
          return FALSE;
        }
        ++i;
        switch (s[i++]) {
         case 'n':
          *dst++ = '\n';
          break;
         case '\\':
          *dst++ = '\\';
          break;
         default:
          /* Only `\' or `n' may follow a backslash.  */
          return FALSE;
        }
        break;

       case '\0':
        /* The file name may not contain a NUL.  */
        return FALSE;
        break;

       default:
        *dst++ = s[i++];
        break;
      }
    }
    *dst = '\0';
  }
  return TRUE;
}

static int hex_digits(unsigned char const *s)
{
  while (*s) {
    if (!ISXDIGIT(*s))
      return TRUE;
    ++s;
  }
  return FALSE;
}

/* An interface to md5_stream.  Operate on FILENAME (it may be "-") and
   put the result in *MD5_RESULT.  Return non-zero upon failure, zero
   to indicate success.  */
static int md5_file(const char *filename,
                    int binary,
                    unsigned char *md5_result)
{
  FILE *fp;

  if (STREQ(filename, "-")) {
    have_read_stdin = 1;
    fp = stdin;
  } else {
    fp = fopen(filename, OPENOPTS(binary));
    if (fp == NULL) {
      perror_msg("%s", filename);
      return FALSE;
    }
  }

  if (md5_stream(fp, md5_result)) {
    perror_msg("%s", filename);

    if (fp != stdin)
      fclose(fp);
    return FALSE;
  }

  if (fp != stdin && fclose(fp) == EOF) {
    perror_msg("%s", filename);
    return FALSE;
  }

  return TRUE;
}

static int md5_check(const char *checkfile_name)
{
  FILE *checkfile_stream;
  int n_properly_formated_lines = 0;
  int n_mismatched_checksums = 0;
  int n_open_or_read_failures = 0;
  unsigned char md5buffer[16];
  size_t line_number;
  char line[BUFSIZ];

  if (STREQ(checkfile_name, "-")) {
    have_read_stdin = 1;
    checkfile_stream = stdin;
  } else {
    checkfile_stream = fopen(checkfile_name, "r");
    if (checkfile_stream == NULL) {
      perror_msg("%s", checkfile_name);
      return FALSE;
    }
  }

  line_number = 0;

  do {
    char *filename;
    int binary;
    unsigned char *md5num;
    int line_length;

    ++line_number;

    fgets(line, BUFSIZ-1, checkfile_stream);
    line_length = strlen(line);

    if (line_length <= 0 || line==NULL)
      break;

    /* Ignore comment lines, which begin with a '#' character.  */
    if (line[0] == '#')
      continue;

    /* Remove any trailing newline.  */
    if (line[line_length - 1] == '\n')
      line[--line_length] = '\0';

    if (split_3(line, line_length, &md5num, &binary, &filename)
        || !hex_digits(md5num)) {
      if (warn) {
        error_msg("%s: %lu: improperly formatted MD5 checksum line",
                 checkfile_name, (unsigned long) line_number);
      }
    } else {
      static const char bin2hex[] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
      };

      ++n_properly_formated_lines;

      if (md5_file(filename, binary, md5buffer)) {
        ++n_open_or_read_failures;
        if (!status_only) {
          printf("%s: FAILED open or read\n", filename);
          fflush(stdout);
        }
      } else {
        size_t cnt;
        /* Compare generated binary number with text representation
           in check file.  Ignore case of hex digits.  */
        for (cnt = 0; cnt < 16; ++cnt) {
          if (TOLOWER(md5num[2 * cnt])
              != bin2hex[md5buffer[cnt] >> 4]
              || (TOLOWER(md5num[2 * cnt + 1])
                  != (bin2hex[md5buffer[cnt] & 0xf])))
            break;
        }
        if (cnt != 16)
          ++n_mismatched_checksums;

        if (!status_only) {
          printf("%s: %s\n", filename,
                 (cnt != 16 ? "FAILED" : "OK"));
          fflush(stdout);
        }
      }
    }
  }

  while (!feof(checkfile_stream) && !ferror(checkfile_stream));

  if (ferror(checkfile_stream)) {
    error_msg("%s: read error", checkfile_name);
    return FALSE;
  }

  if (checkfile_stream != stdin && fclose(checkfile_stream) == EOF) {
    perror_msg("md5sum: %s", checkfile_name);
    return FALSE;
  }

  if (n_properly_formated_lines == 0) {
    /* Warn if no tests are found.  */
    error_msg("%s: no properly formatted MD5 checksum lines found",
             checkfile_name);
    return FALSE;
  } else {
    if (!status_only) {
      int n_computed_checkums = (n_properly_formated_lines
                                 - n_open_or_read_failures);

      if (n_open_or_read_failures > 0) {
        error_msg("WARNING: %d of %d listed files could not be read",
                 n_open_or_read_failures, n_properly_formated_lines);
        return FALSE;
      }

      if (n_mismatched_checksums > 0) {
        error_msg("WARNING: %d of %d computed checksums did NOT match",
                 n_mismatched_checksums, n_computed_checkums);
        return FALSE;
      }
    }
  }

  return ((n_properly_formated_lines > 0 && n_mismatched_checksums == 0
           && n_open_or_read_failures == 0) ? 0 : 1);
}

int md5sum_main(int argc,
                char **argv)
{
  unsigned char md5buffer[16];
  int do_check = 0;
  int opt;
  char **string = NULL;
  size_t n_strings = 0;
  size_t err = 0;
  int file_type_specified = 0;
  int binary = 0;

  while ((opt = getopt(argc, argv, "g:bcstw")) != -1) {
    switch (opt) {
     case 'g': { /* read a string */
       if (string == NULL)
         string = (char **) xmalloc ((argc - 1) * sizeof (char *));

       string[n_strings++] = optarg;
       break;
     }

     case 'b': /* read files in binary mode */
      file_type_specified = 1;
      binary = 1;
      break;

     case 'c': /* check MD5 sums against given list */
      do_check = 1;
      break;

     case 's':  /* don't output anything, status code shows success */
      status_only = 1;
      warn = 0;
      break;

     case 't': /* read files in text mode (default) */
      file_type_specified = 1;
      binary = 0;
      break;

     case 'w': /* warn about improperly formated MD5 checksum lines */
      status_only = 0;
      warn = 1;
      break;

     default:
      show_usage();
    }
  }

  if (file_type_specified && do_check) {
    error_msg_and_die("the -b and -t options are meaningless when verifying checksums");
  }

  if (n_strings > 0 && do_check) {
    error_msg_and_die("the -g and -c options are mutually exclusive");
  }

  if (status_only && !do_check) {
    error_msg_and_die("the -s option is meaningful only when verifying checksums");
  }

  if (warn && !do_check) {
    error_msg_and_die("the -w option is meaningful only when verifying checksums");
  }

  if (n_strings > 0) {
    size_t i;

    if (optind < argc) {
      error_msg_and_die("no files may be specified when using -g");
    }
    for (i = 0; i < n_strings; ++i) {
      size_t cnt;
      md5_buffer (string[i], strlen (string[i]), md5buffer);

      for (cnt = 0; cnt < 16; ++cnt)
        printf ("%02x", md5buffer[cnt]);

      printf ("  \"%s\"\n", string[i]);
    }
  } else if (do_check) {
    if (optind + 1 < argc) {
      error_msg("only one argument may be specified when using -c");
    }

    err = md5_check ((optind == argc) ? "-" : argv[optind]);
  } else {
    if (optind == argc)
      argv[argc++] = "-";

    for (; optind < argc; ++optind) {
      int fail;
      char *file = argv[optind];

      fail = md5_file (file, binary, md5buffer);
      err |= fail;
      if (!fail && STREQ(file, "-")) {
	  size_t i;
	  for (i = 0; i < 16; ++i)
	      printf ("%02x", md5buffer[i]);
	  putchar ('\n');
      } else if (!fail) {
        size_t i;
        /* Output a leading backslash if the file name contains
           a newline or backslash.  */
        if (strchr (file, '\n') || strchr (file, '\\'))
          putchar ('\\');

        for (i = 0; i < 16; ++i)
          printf ("%02x", md5buffer[i]);

        putchar (' ');
        if (binary)
          putchar ('*');
        else
          putchar (' ');

        /* Translate each NEWLINE byte to the string, "\\n",
           and each backslash to "\\\\".  */
        for (i = 0; i < strlen (file); ++i) {
          switch (file[i]) {
           case '\n':
            fputs ("\\n", stdout);
            break;

           case '\\':
            fputs ("\\\\", stdout);
            break;

           default:
            putchar (file[i]);
            break;
          }
        }
        putchar ('\n');
      }
    }
  }

  if (fclose (stdout) == EOF) {
    error_msg_and_die("write error");
  }

  if (have_read_stdin && fclose (stdin) == EOF) {
    error_msg_and_die("standard input");
  }

  if (err == 0)
	  return EXIT_SUCCESS;
  else
	  return EXIT_FAILURE;
}
