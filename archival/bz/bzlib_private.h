/*
 * bzip2 is written by Julian Seward <jseward@bzip.org>.
 * Adapted for busybox by Denys Vlasenko <vda.linux@googlemail.com>.
 * See README and LICENSE files in this directory for more information.
 */

/*-------------------------------------------------------------*/
/*--- Private header file for the library.                  ---*/
/*---                                       bzlib_private.h ---*/
/*-------------------------------------------------------------*/

/* ------------------------------------------------------------------
This file is part of bzip2/libbzip2, a program and library for
lossless, block-sorting data compression.

bzip2/libbzip2 version 1.0.4 of 20 December 2006
Copyright (C) 1996-2006 Julian Seward <jseward@bzip.org>

Please read the WARNING, DISCLAIMER and PATENTS sections in the
README file.

This program is released under the terms of the license contained
in the file LICENSE.
------------------------------------------------------------------ */

/* #include "bzlib.h" */

#define BZ_DEBUG 0
//#define BZ_NO_STDIO 1 - does not work


/*-- General stuff. --*/

typedef unsigned char Bool;
typedef unsigned char UChar;

#define True  ((Bool)1)
#define False ((Bool)0)

static void bz_assert_fail(int errcode) ATTRIBUTE_NORETURN;
#define AssertH(cond, errcode) \
{ \
	if (!(cond)) \
		bz_assert_fail(errcode); \
}

#if BZ_DEBUG
#define AssertD(cond, msg) \
{ \
	if (!(cond)) \
		bb_error_msg_and_die("(debug build): internal error %s", msg); \
}
#else
#define AssertD(cond, msg) do { } while (0)
#endif


/*-- Header bytes. --*/

#define BZ_HDR_B 0x42   /* 'B' */
#define BZ_HDR_Z 0x5a   /* 'Z' */
#define BZ_HDR_h 0x68   /* 'h' */
#define BZ_HDR_0 0x30   /* '0' */

#define BZ_HDR_BZh0 0x425a6830

/*-- Constants for the back end. --*/

#define BZ_MAX_ALPHA_SIZE 258
#define BZ_MAX_CODE_LEN    23

#define BZ_RUNA 0
#define BZ_RUNB 1

#define BZ_N_GROUPS 6
#define BZ_G_SIZE   50
#define BZ_N_ITERS  4

#define BZ_MAX_SELECTORS (2 + (900000 / BZ_G_SIZE))


/*-- Stuff for randomising repetitive blocks. --*/

static const int32_t BZ2_rNums[512];

#define BZ_RAND_DECLS \
	int32_t rNToGo; \
	int32_t rTPos \

#define BZ_RAND_INIT_MASK \
	s->rNToGo = 0; \
	s->rTPos  = 0 \

#define BZ_RAND_MASK ((s->rNToGo == 1) ? 1 : 0)

#define BZ_RAND_UPD_MASK \
{ \
	if (s->rNToGo == 0) { \
		s->rNToGo = BZ2_rNums[s->rTPos]; \
		s->rTPos++; \
		if (s->rTPos == 512) s->rTPos = 0; \
	} \
	s->rNToGo--; \
}


/*-- Stuff for doing CRCs. --*/

static const uint32_t BZ2_crc32Table[256];

#define BZ_INITIALISE_CRC(crcVar) \
{ \
	crcVar = 0xffffffffL; \
}

#define BZ_FINALISE_CRC(crcVar) \
{ \
	crcVar = ~(crcVar); \
}

#define BZ_UPDATE_CRC(crcVar,cha) \
{ \
	crcVar = (crcVar << 8) ^ BZ2_crc32Table[(crcVar >> 24) ^ ((UChar)cha)]; \
}


/*-- States and modes for compression. --*/

#define BZ_M_IDLE      1
#define BZ_M_RUNNING   2
#define BZ_M_FLUSHING  3
#define BZ_M_FINISHING 4

#define BZ_S_OUTPUT    1
#define BZ_S_INPUT     2

#define BZ_N_RADIX 2
#define BZ_N_QSORT 12
#define BZ_N_SHELL 18
#define BZ_N_OVERSHOOT (BZ_N_RADIX + BZ_N_QSORT + BZ_N_SHELL + 2)


/*-- Structure holding all the compression-side stuff. --*/

typedef struct EState {
	/* pointer back to the struct bz_stream */
	bz_stream *strm;

	/* mode this stream is in, and whether inputting */
	/* or outputting data */
	int32_t  mode;
	int32_t  state;

	/* remembers avail_in when flush/finish requested */
	uint32_t avail_in_expect; //vda: do we need this?

	/* for doing the block sorting */
	uint32_t *arr1;
	uint32_t *arr2;
	uint32_t *ftab;
	int32_t  origPtr;

	/* aliases for arr1 and arr2 */
	uint32_t *ptr;
	UChar    *block;
	uint16_t *mtfv;
	UChar    *zbits;

	/* run-length-encoding of the input */
	uint32_t state_in_ch;
	int32_t  state_in_len;
	BZ_RAND_DECLS;

	/* input and output limits and current posns */
	int32_t  nblock;
	int32_t  nblockMAX;
	int32_t  numZ;
	int32_t  state_out_pos;

	/* the buffer for bit stream creation */
	uint32_t bsBuff;
	int32_t  bsLive;

	/* block and combined CRCs */
	uint32_t blockCRC;
	uint32_t combinedCRC;

	/* misc administratium */
	int32_t  blockNo;
	int32_t  blockSize100k;

	/* stuff for coding the MTF values */
	int32_t  nMTF;

	/* map of bytes used in block */
	int32_t  nInUse;
	Bool     inUse[256];
	UChar    unseqToSeq[256];

	/* stuff for coding the MTF values */
	int32_t  mtfFreq    [BZ_MAX_ALPHA_SIZE];
	UChar    selector   [BZ_MAX_SELECTORS];
	UChar    selectorMtf[BZ_MAX_SELECTORS];

	UChar    len     [BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	int32_t  code    [BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	int32_t  rfreq   [BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
#ifdef FAST_GROUP6
	/* second dimension: only 3 needed; 4 makes index calculations faster */
	uint32_t len_pack[BZ_MAX_ALPHA_SIZE][4];
#endif
} EState;


/*-- compression. --*/

static void
BZ2_blockSort(EState*);

static void
BZ2_compressBlock(EState*, Bool);

static void
BZ2_bsInitWrite(EState*);

static void
BZ2_hbAssignCodes(int32_t*, UChar*, int32_t, int32_t, int32_t);

static void
BZ2_hbMakeCodeLengths(UChar*, int32_t*, int32_t, int32_t);

/*-------------------------------------------------------------*/
/*--- end                                   bzlib_private.h ---*/
/*-------------------------------------------------------------*/
