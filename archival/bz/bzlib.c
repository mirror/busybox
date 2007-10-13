/*
 * bzip2 is written by Julian Seward <jseward@bzip.org>.
 * Adapted for busybox by Denys Vlasenko <vda.linux@googlemail.com>.
 * See README and LICENSE files in this directory for more information.
 */

/*-------------------------------------------------------------*/
/*--- Library top-level functions.                          ---*/
/*---                                               bzlib.c ---*/
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

/* CHANGES
 * 0.9.0    -- original version.
 * 0.9.0a/b -- no changes in this file.
 * 0.9.0c   -- made zero-length BZ_FLUSH work correctly in bzCompress().
 *             fixed bzWrite/bzRead to ignore zero-length requests.
 *	       fixed bzread to correctly handle read requests after EOF.
 *             wrong parameter order in call to bzDecompressInit in
 *             bzBuffToBuffDecompress.  Fixed.
 */

/* #include "bzlib_private.h" */

/*---------------------------------------------------*/
/*--- Compression stuff                           ---*/
/*---------------------------------------------------*/

/*---------------------------------------------------*/
#ifndef BZ_NO_STDIO
static void bz_assert_fail(int errcode)
{
	/* if (errcode == 1007) bb_error_msg_and_die("probably bad RAM"); */
	bb_error_msg_and_die("bzip2 internal error %d", errcode);
}
#endif


/*---------------------------------------------------*/
static
void prepare_new_block(EState* s)
{
	int32_t i;
	s->nblock = 0;
	s->numZ = 0;
	s->state_out_pos = 0;
	BZ_INITIALISE_CRC(s->blockCRC);
	for (i = 0; i < 256; i++) s->inUse[i] = False;
	s->blockNo++;
}


/*---------------------------------------------------*/
static
ALWAYS_INLINE
void init_RL(EState* s)
{
	s->state_in_ch = 256;
	s->state_in_len = 0;
}


static
Bool isempty_RL(EState* s)
{
	if (s->state_in_ch < 256 && s->state_in_len > 0)
		return False;
	return True;
}


/*---------------------------------------------------*/
static
void BZ2_bzCompressInit(bz_stream *strm, int blockSize100k)
{
	int32_t n;
	EState* s;

	s = xzalloc(sizeof(EState));
	s->strm = strm;

	n        = 100000 * blockSize100k;
	s->arr1  = xmalloc(n                    * sizeof(uint32_t));
	s->mtfv  = (uint16_t*)s->arr1;
	s->ptr   = (uint32_t*)s->arr1;
	s->arr2  = xmalloc((n + BZ_N_OVERSHOOT) * sizeof(uint32_t));
	s->block = (UChar*)s->arr2;
	s->ftab  = xmalloc(65537                * sizeof(uint32_t));

	s->state             = BZ_S_INPUT;
	s->mode              = BZ_M_RUNNING;
	s->blockSize100k     = blockSize100k;
	s->nblockMAX         = n - 19;

	strm->state          = s;
	/*strm->total_in     = 0;*/
	strm->total_out = 0;
	init_RL(s);
	prepare_new_block(s);
}


/*---------------------------------------------------*/
static
void add_pair_to_block(EState* s)
{
	int32_t i;
	UChar ch = (UChar)(s->state_in_ch);
	for (i = 0; i < s->state_in_len; i++) {
		BZ_UPDATE_CRC(s->blockCRC, ch);
	}
	s->inUse[s->state_in_ch] = True;
	switch (s->state_in_len) {
		case 1:
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			break;
		case 2:
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			break;
		case 3:
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			break;
		default:
			s->inUse[s->state_in_len-4] = True;
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			s->block[s->nblock] = (UChar)ch; s->nblock++;
			s->block[s->nblock] = ((UChar)(s->state_in_len-4));
			s->nblock++;
			break;
	}
}


/*---------------------------------------------------*/
static
void flush_RL(EState* s)
{
	if (s->state_in_ch < 256) add_pair_to_block(s);
	init_RL(s);
}


/*---------------------------------------------------*/
#define ADD_CHAR_TO_BLOCK(zs, zchh0) \
{ \
	uint32_t zchh = (uint32_t)(zchh0); \
	/*-- fast track the common case --*/ \
	if (zchh != zs->state_in_ch && zs->state_in_len == 1) { \
		UChar ch = (UChar)(zs->state_in_ch); \
		BZ_UPDATE_CRC(zs->blockCRC, ch); \
		zs->inUse[zs->state_in_ch] = True; \
		zs->block[zs->nblock] = (UChar)ch; \
		zs->nblock++; \
		zs->state_in_ch = zchh; \
	} \
	else \
	/*-- general, uncommon cases --*/ \
	if (zchh != zs->state_in_ch || \
		zs->state_in_len == 255) { \
		if (zs->state_in_ch < 256) \
			add_pair_to_block(zs); \
		zs->state_in_ch = zchh; \
		zs->state_in_len = 1; \
	} else { \
		zs->state_in_len++; \
	} \
}


/*---------------------------------------------------*/
static
Bool copy_input_until_stop(EState* s)
{
	Bool progress_in = False;

//vda: cannot simplify this until avail_in_expect is removed
	if (s->mode == BZ_M_RUNNING) {
		/*-- fast track the common case --*/
		while (1) {
			/*-- block full? --*/
			if (s->nblock >= s->nblockMAX) break;
			/*-- no input? --*/
			if (s->strm->avail_in == 0) break;
			progress_in = True;
			ADD_CHAR_TO_BLOCK(s, (uint32_t)(*((UChar*)(s->strm->next_in))));
			s->strm->next_in++;
			s->strm->avail_in--;
			/*s->strm->total_in++;*/
		}
	} else {
		/*-- general, uncommon case --*/
		while (1) {
			/*-- block full? --*/
			if (s->nblock >= s->nblockMAX) break;
			/*-- no input? --*/
			if (s->strm->avail_in == 0) break;
			/*-- flush/finish end? --*/
			if (s->avail_in_expect == 0) break;
			progress_in = True;
			ADD_CHAR_TO_BLOCK(s, (uint32_t)(*((UChar*)(s->strm->next_in))));
			s->strm->next_in++;
			s->strm->avail_in--;
			/*s->strm->total_in++;*/
			s->avail_in_expect--;
		}
	}
	return progress_in;
}


/*---------------------------------------------------*/
static
Bool copy_output_until_stop(EState* s)
{
	Bool progress_out = False;

	while (1) {

		/*-- no output space? --*/
		if (s->strm->avail_out == 0) break;

		/*-- block done? --*/
		if (s->state_out_pos >= s->numZ) break;

		progress_out = True;
		*(s->strm->next_out) = s->zbits[s->state_out_pos];
		s->state_out_pos++;
		s->strm->avail_out--;
		s->strm->next_out++;
		s->strm->total_out++;
	}

	return progress_out;
}


/*---------------------------------------------------*/
static
Bool handle_compress(bz_stream *strm)
{
	Bool progress_in  = False;
	Bool progress_out = False;
	EState* s = strm->state;
	
	while (1) {
		if (s->state == BZ_S_OUTPUT) {
			progress_out |= copy_output_until_stop(s);
			if (s->state_out_pos < s->numZ) break;
			if (s->mode == BZ_M_FINISHING
			 && s->avail_in_expect == 0
			 && isempty_RL(s))
				break;
			prepare_new_block(s);
			s->state = BZ_S_INPUT;
			if (s->mode == BZ_M_FLUSHING
			 && s->avail_in_expect == 0
			 && isempty_RL(s))
				break;
		}

		if (s->state == BZ_S_INPUT) {
			progress_in |= copy_input_until_stop(s);
			if (s->mode != BZ_M_RUNNING && s->avail_in_expect == 0) {
				flush_RL(s);
				BZ2_compressBlock(s, (Bool)(s->mode == BZ_M_FINISHING));
				s->state = BZ_S_OUTPUT;
			} else
			if (s->nblock >= s->nblockMAX) {
				BZ2_compressBlock(s, False);
				s->state = BZ_S_OUTPUT;
			} else
			if (s->strm->avail_in == 0) {
				break;
			}
		}

	}

	return progress_in || progress_out;
}


/*---------------------------------------------------*/
static
int BZ2_bzCompress(bz_stream *strm, int action)
{
	Bool progress;
	EState* s;
	if (strm == NULL) return BZ_PARAM_ERROR;
	s = strm->state;
	if (s == NULL) return BZ_PARAM_ERROR;
	if (s->strm != strm) return BZ_PARAM_ERROR;

	preswitch:
	switch (s->mode) {

		case BZ_M_IDLE:
			return BZ_SEQUENCE_ERROR;

		case BZ_M_RUNNING:
			if (action == BZ_RUN) {
				progress = handle_compress(strm);
				return progress ? BZ_RUN_OK : BZ_PARAM_ERROR;
			}
			else
			if (action == BZ_FLUSH) {
				s->avail_in_expect = strm->avail_in;
				s->mode = BZ_M_FLUSHING;
				goto preswitch;
			}
			else
			if (action == BZ_FINISH) {
				s->avail_in_expect = strm->avail_in;
				s->mode = BZ_M_FINISHING;
				goto preswitch;
			}
			else
				return BZ_PARAM_ERROR;

		case BZ_M_FLUSHING:
			if (action != BZ_FLUSH) return BZ_SEQUENCE_ERROR;
			if (s->avail_in_expect != s->strm->avail_in)
				return BZ_SEQUENCE_ERROR;
			progress = handle_compress(strm);
			if (s->avail_in_expect > 0 || !isempty_RL(s) || s->state_out_pos < s->numZ)
				return BZ_FLUSH_OK;
			s->mode = BZ_M_RUNNING;
			return BZ_RUN_OK;

		case BZ_M_FINISHING:
			if (action != BZ_FINISH) return BZ_SEQUENCE_ERROR;
			if (s->avail_in_expect != s->strm->avail_in)
				return BZ_SEQUENCE_ERROR;
			progress = handle_compress(strm);
			if (!progress) return BZ_SEQUENCE_ERROR;
			if (s->avail_in_expect > 0 || !isempty_RL(s) || s->state_out_pos < s->numZ)
				return BZ_FINISH_OK;
			s->mode = BZ_M_IDLE;
			return BZ_STREAM_END;
	}
	return BZ_OK; /*--not reached--*/
}


/*---------------------------------------------------*/
static
int BZ2_bzCompressEnd(bz_stream *strm)
{
	EState* s;
	if (strm == NULL) return BZ_PARAM_ERROR;
	s = strm->state;
	if (s == NULL) return BZ_PARAM_ERROR;
	if (s->strm != strm) return BZ_PARAM_ERROR;

	if (s->arr1 != NULL) free(s->arr1);
	if (s->arr2 != NULL) free(s->arr2);
	if (s->ftab != NULL) free(s->ftab);
	free(strm->state);

	strm->state = NULL;

	return BZ_OK;
}


/*---------------------------------------------------*/
/*--- Misc convenience stuff                      ---*/
/*---------------------------------------------------*/

/*---------------------------------------------------*/
#ifdef EXAMPLE_CODE_FOR_MEM_TO_MEM_COMPRESSION
static
int BZ2_bzBuffToBuffCompress(char* dest,
		unsigned int* destLen,
		char*         source,
		unsigned int  sourceLen,
		int           blockSize100k)
{
	bz_stream strm;
	int ret;

	if (dest == NULL || destLen == NULL ||
		 source == NULL ||
		 blockSize100k < 1 || blockSize100k > 9)
		return BZ_PARAM_ERROR;

	BZ2_bzCompressInit(&strm, blockSize100k);

	strm.next_in = source;
	strm.next_out = dest;
	strm.avail_in = sourceLen;
	strm.avail_out = *destLen;

	ret = BZ2_bzCompress(&strm, BZ_FINISH);
	if (ret == BZ_FINISH_OK) goto output_overflow;
	if (ret != BZ_STREAM_END) goto errhandler;

	/* normal termination */
	*destLen -= strm.avail_out;
	BZ2_bzCompressEnd(&strm);
	return BZ_OK;

	output_overflow:
	BZ2_bzCompressEnd(&strm);
	return BZ_OUTBUFF_FULL;

	errhandler:
	BZ2_bzCompressEnd(&strm);
	return ret;
}
#endif

/*-------------------------------------------------------------*/
/*--- end                                           bzlib.c ---*/
/*-------------------------------------------------------------*/
