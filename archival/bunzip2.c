/* Modified for busybox by Glenn McGrath <bug1@optushome.com.au> */
/* Added support output to stdout by Thomas Lundquist <thomasez@zelow.no> */ 
/*--
  This file is a part of bzip2 and/or libbzip2, a program and
  library for lossless, block-sorting data compression.

  Copyright (C) 1996-2000 Julian R Seward.  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. The origin of this software must not be misrepresented; you must 
     not claim that you wrote the original software.  If you use this 
     software in a product, an acknowledgment in the product 
     documentation would be appreciated but is not required.

  3. Altered source versions must be plainly marked as such, and must
     not be misrepresented as being the original software.

  4. The name of the author may not be used to endorse or promote 
     products derived from this software without specific prior written 
     permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Julian Seward, Cambridge, UK.
  jseward@acm.org
  bzip2/libbzip2 version 1.0 of 21 March 2000

  This program is based on (at least) the work of:
     Mike Burrows
     David Wheeler
     Peter Fenwick
     Alistair Moffat
     Radford Neal
     Ian H. Witten
     Robert Sedgewick
     Jon L. Bentley

  For more information on these sources, see the manual.
--*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <busybox.h>

//#define TRUE 1
//#define FALSE 0

#define MTFA_SIZE 4096
#define MTFL_SIZE 16
#define BZ_N_GROUPS 6
#define BZ_G_SIZE   50
#define BZ_MAX_ALPHA_SIZE 258

#define BZ_OK                0
#define BZ_STREAM_END        4
#define BZ_SEQUENCE_ERROR    (-1)
#define BZ_DATA_ERROR        (-4)
#define BZ_DATA_ERROR_MAGIC  (-5)
#define BZ_IO_ERROR          (-6)
#define BZ_UNEXPECTED_EOF    (-7)

#define BZ_RUNA 0
#define BZ_RUNB 1

#define BZ_MAX_UNUSED 5000
#define FILE_NAME_LEN 1034
/*-- states for decompression. --*/

#define BZ_X_IDLE        1
#define BZ_X_OUTPUT      2

#define BZ_X_MAGIC_1     10
#define BZ_X_MAGIC_2     11
#define BZ_X_MAGIC_3     12
#define BZ_X_MAGIC_4     13
#define BZ_X_BLKHDR_1    14
#define BZ_X_BLKHDR_2    15
#define BZ_X_BLKHDR_3    16
#define BZ_X_BLKHDR_4    17
#define BZ_X_BLKHDR_5    18
#define BZ_X_BLKHDR_6    19
#define BZ_X_BCRC_1      20
#define BZ_X_BCRC_2      21
#define BZ_X_BCRC_3      22
#define BZ_X_BCRC_4      23
#define BZ_X_RANDBIT     24
#define BZ_X_ORIGPTR_1   25
#define BZ_X_ORIGPTR_2   26
#define BZ_X_ORIGPTR_3   27
#define BZ_X_MAPPING_1   28
#define BZ_X_MAPPING_2   29
#define BZ_X_SELECTOR_1  30
#define BZ_X_SELECTOR_2  31
#define BZ_X_SELECTOR_3  32
#define BZ_X_CODING_1    33
#define BZ_X_CODING_2    34
#define BZ_X_CODING_3    35
#define BZ_X_MTF_1       36
#define BZ_X_MTF_2       37
#define BZ_X_MTF_3       38
#define BZ_X_MTF_4       39
#define BZ_X_MTF_5       40
#define BZ_X_MTF_6       41
#define BZ_X_ENDHDR_2    42
#define BZ_X_ENDHDR_3    43
#define BZ_X_ENDHDR_4    44
#define BZ_X_ENDHDR_5    45
#define BZ_X_ENDHDR_6    46
#define BZ_X_CCRC_1      47
#define BZ_X_CCRC_2      48
#define BZ_X_CCRC_3      49
#define BZ_X_CCRC_4      50

#define BZ_MAX_CODE_LEN    23
#define OM_TEST          3

typedef struct {
	char *next_in;
	unsigned int avail_in;

	char *next_out;
	unsigned int avail_out;

	void *state;

} bz_stream;

typedef struct {
	bz_stream	strm;
	FILE	*handle;
    unsigned char	initialisedOk;
	char	buf[BZ_MAX_UNUSED];
	int		lastErr;
	int		bufN;
} bzFile;

/*-- Structure holding all the decompression-side stuff. --*/
typedef struct {
	/* pointer back to the struct bz_stream */
	bz_stream* strm;

	/* state indicator for this stream */
	int	state;

	/* for doing the final run-length decoding */
	unsigned char    state_out_ch;
	int    state_out_len;
	unsigned char     blockRandomised;
	int rNToGo;
	int rTPos;

	/* the buffer for bit stream reading */
	unsigned int   bsBuff;
	int    bsLive;

	/* misc administratium */
	int    blockSize100k;
	int    currBlockNo;

	/* for undoing the Burrows-Wheeler transform */
	int    origPtr;
	unsigned int   tPos;
	int    k0;
	int    unzftab[256];
	int    nblock_used;
	int    cftab[257];
	int    cftabCopy[257];

	/* for undoing the Burrows-Wheeler transform (FAST) */
	unsigned int *tt;

	/* stored and calculated CRCs */
	unsigned int   storedBlockCRC;
	unsigned int   storedCombinedCRC;
	unsigned int   calculatedBlockCRC;
	unsigned int   calculatedCombinedCRC;

	/* map of bytes used in block */
	int    nInUse;
	unsigned char     inUse[256];
	unsigned char     inUse16[16];
	unsigned char    seqToUnseq[256];

	/* for decoding the MTF values */
	unsigned char    mtfa   [MTFA_SIZE];
	unsigned char    selector   [2 + (900000 / BZ_G_SIZE)];
	unsigned char    selectorMtf[2 + (900000 / BZ_G_SIZE)];
	unsigned char    len  [BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	int    mtfbase[256 / MTFL_SIZE];

	int    limit  [BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	int    base   [BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	int    perm   [BZ_N_GROUPS][BZ_MAX_ALPHA_SIZE];
	int    minLens[BZ_N_GROUPS];

	/* save area for scalars in the main decompress code */
	int    save_i;
	int    save_j;
	int    save_t;
	int    save_alphaSize;
	int    save_nGroups;
	int    save_nSelectors;
	int    save_EOB;
	int    save_groupNo;
	int    save_groupPos;
	int    save_nextSym;
	int    save_nblockMAX;
	int    save_nblock;
	int    save_es;
	int    save_N;
	int    save_curr;
	int    save_zt;
	int    save_zn; 
	int    save_zvec;
	int    save_zj;
	int    save_gSel;
	int    save_gMinlen;
	int	*save_gLimit;
	int	*save_gBase;
	int	*save_gPerm;
} DState;

int BZ2_rNums[512];
char inName[FILE_NAME_LEN];
char outName[FILE_NAME_LEN];
int srcMode;
int opMode;
unsigned char deleteOutputOnInterrupt;
FILE *outputHandleJustInCase;
int numFileNames;
int numFilesProcessed;
int exitValue;

const unsigned int BZ2_crc32Table[256] = {

   /*-- Ugly, innit? --*/

   0x00000000L, 0x04c11db7L, 0x09823b6eL, 0x0d4326d9L,
   0x130476dcL, 0x17c56b6bL, 0x1a864db2L, 0x1e475005L,
   0x2608edb8L, 0x22c9f00fL, 0x2f8ad6d6L, 0x2b4bcb61L,
   0x350c9b64L, 0x31cd86d3L, 0x3c8ea00aL, 0x384fbdbdL,
   0x4c11db70L, 0x48d0c6c7L, 0x4593e01eL, 0x4152fda9L,
   0x5f15adacL, 0x5bd4b01bL, 0x569796c2L, 0x52568b75L,
   0x6a1936c8L, 0x6ed82b7fL, 0x639b0da6L, 0x675a1011L,
   0x791d4014L, 0x7ddc5da3L, 0x709f7b7aL, 0x745e66cdL,
   0x9823b6e0L, 0x9ce2ab57L, 0x91a18d8eL, 0x95609039L,
   0x8b27c03cL, 0x8fe6dd8bL, 0x82a5fb52L, 0x8664e6e5L,
   0xbe2b5b58L, 0xbaea46efL, 0xb7a96036L, 0xb3687d81L,
   0xad2f2d84L, 0xa9ee3033L, 0xa4ad16eaL, 0xa06c0b5dL,
   0xd4326d90L, 0xd0f37027L, 0xddb056feL, 0xd9714b49L,
   0xc7361b4cL, 0xc3f706fbL, 0xceb42022L, 0xca753d95L,
   0xf23a8028L, 0xf6fb9d9fL, 0xfbb8bb46L, 0xff79a6f1L,
   0xe13ef6f4L, 0xe5ffeb43L, 0xe8bccd9aL, 0xec7dd02dL,
   0x34867077L, 0x30476dc0L, 0x3d044b19L, 0x39c556aeL,
   0x278206abL, 0x23431b1cL, 0x2e003dc5L, 0x2ac12072L,
   0x128e9dcfL, 0x164f8078L, 0x1b0ca6a1L, 0x1fcdbb16L,
   0x018aeb13L, 0x054bf6a4L, 0x0808d07dL, 0x0cc9cdcaL,
   0x7897ab07L, 0x7c56b6b0L, 0x71159069L, 0x75d48ddeL,
   0x6b93dddbL, 0x6f52c06cL, 0x6211e6b5L, 0x66d0fb02L,
   0x5e9f46bfL, 0x5a5e5b08L, 0x571d7dd1L, 0x53dc6066L,
   0x4d9b3063L, 0x495a2dd4L, 0x44190b0dL, 0x40d816baL,
   0xaca5c697L, 0xa864db20L, 0xa527fdf9L, 0xa1e6e04eL,
   0xbfa1b04bL, 0xbb60adfcL, 0xb6238b25L, 0xb2e29692L,
   0x8aad2b2fL, 0x8e6c3698L, 0x832f1041L, 0x87ee0df6L,
   0x99a95df3L, 0x9d684044L, 0x902b669dL, 0x94ea7b2aL,
   0xe0b41de7L, 0xe4750050L, 0xe9362689L, 0xedf73b3eL,
   0xf3b06b3bL, 0xf771768cL, 0xfa325055L, 0xfef34de2L,
   0xc6bcf05fL, 0xc27dede8L, 0xcf3ecb31L, 0xcbffd686L,
   0xd5b88683L, 0xd1799b34L, 0xdc3abdedL, 0xd8fba05aL,
   0x690ce0eeL, 0x6dcdfd59L, 0x608edb80L, 0x644fc637L,
   0x7a089632L, 0x7ec98b85L, 0x738aad5cL, 0x774bb0ebL,
   0x4f040d56L, 0x4bc510e1L, 0x46863638L, 0x42472b8fL,
   0x5c007b8aL, 0x58c1663dL, 0x558240e4L, 0x51435d53L,
   0x251d3b9eL, 0x21dc2629L, 0x2c9f00f0L, 0x285e1d47L,
   0x36194d42L, 0x32d850f5L, 0x3f9b762cL, 0x3b5a6b9bL,
   0x0315d626L, 0x07d4cb91L, 0x0a97ed48L, 0x0e56f0ffL,
   0x1011a0faL, 0x14d0bd4dL, 0x19939b94L, 0x1d528623L,
   0xf12f560eL, 0xf5ee4bb9L, 0xf8ad6d60L, 0xfc6c70d7L,
   0xe22b20d2L, 0xe6ea3d65L, 0xeba91bbcL, 0xef68060bL,
   0xd727bbb6L, 0xd3e6a601L, 0xdea580d8L, 0xda649d6fL,
   0xc423cd6aL, 0xc0e2d0ddL, 0xcda1f604L, 0xc960ebb3L,
   0xbd3e8d7eL, 0xb9ff90c9L, 0xb4bcb610L, 0xb07daba7L,
   0xae3afba2L, 0xaafbe615L, 0xa7b8c0ccL, 0xa379dd7bL,
   0x9b3660c6L, 0x9ff77d71L, 0x92b45ba8L, 0x9675461fL,
   0x8832161aL, 0x8cf30badL, 0x81b02d74L, 0x857130c3L,
   0x5d8a9099L, 0x594b8d2eL, 0x5408abf7L, 0x50c9b640L,
   0x4e8ee645L, 0x4a4ffbf2L, 0x470cdd2bL, 0x43cdc09cL,
   0x7b827d21L, 0x7f436096L, 0x7200464fL, 0x76c15bf8L,
   0x68860bfdL, 0x6c47164aL, 0x61043093L, 0x65c52d24L,
   0x119b4be9L, 0x155a565eL, 0x18197087L, 0x1cd86d30L,
   0x029f3d35L, 0x065e2082L, 0x0b1d065bL, 0x0fdc1becL,
   0x3793a651L, 0x3352bbe6L, 0x3e119d3fL, 0x3ad08088L,
   0x2497d08dL, 0x2056cd3aL, 0x2d15ebe3L, 0x29d4f654L,
   0xc5a92679L, 0xc1683bceL, 0xcc2b1d17L, 0xc8ea00a0L,
   0xd6ad50a5L, 0xd26c4d12L, 0xdf2f6bcbL, 0xdbee767cL,
   0xe3a1cbc1L, 0xe760d676L, 0xea23f0afL, 0xeee2ed18L,
   0xf0a5bd1dL, 0xf464a0aaL, 0xf9278673L, 0xfde69bc4L,
   0x89b8fd09L, 0x8d79e0beL, 0x803ac667L, 0x84fbdbd0L,
   0x9abc8bd5L, 0x9e7d9662L, 0x933eb0bbL, 0x97ffad0cL,
   0xafb010b1L, 0xab710d06L, 0xa6322bdfL, 0xa2f33668L,
   0xbcb4666dL, 0xb8757bdaL, 0xb5365d03L, 0xb1f740b4L
};

static void bz_rand_udp_mask(DState *s)
{
	if (s->rNToGo == 0) {
		s->rNToGo = BZ2_rNums[s->rTPos];
		s->rTPos++;
		if (s->rTPos == 512) {
			s->rTPos = 0;
		}
	}
	s->rNToGo--;
}

static unsigned char myfeof(FILE *f)
{
	int c = fgetc(f);
	if (c == EOF) {
		return(TRUE);
	}
	ungetc(c, f);
	return(FALSE);
}

static void BZ2_hbCreateDecodeTables(int *limit, int *base, int *perm, unsigned char *length, int minLen, int maxLen, int alphaSize )
{
	int pp, i, j, vec;

	pp = 0;
	for (i = minLen; i <= maxLen; i++) {
		for (j = 0; j < alphaSize; j++) {
			if (length[j] == i) {
				perm[pp] = j;
				pp++;
			}
		}
	}

	for (i = 0; i < BZ_MAX_CODE_LEN; i++) {
		base[i] = 0;
	}

	for (i = 0; i < alphaSize; i++) {
		base[length[i]+1]++;
	}

	for (i = 1; i < BZ_MAX_CODE_LEN; i++) {
		base[i] += base[i-1];
	}

	for (i = 0; i < BZ_MAX_CODE_LEN; i++) {
		limit[i] = 0;
	}
	vec = 0;

	for (i = minLen; i <= maxLen; i++) {
		vec += (base[i+1] - base[i]);
		limit[i] = vec-1;
		vec <<= 1;
	}
	for (i = minLen + 1; i <= maxLen; i++) {
		base[i] = ((limit[i-1] + 1) << 1) - base[i];
	}
}


static int get_bits(DState *s, int *vvv, char nnn)
{
	while (1) {
		if (s->bsLive >= nnn) {
			*vvv = (s->bsBuff >> (s->bsLive-nnn)) & ((1 << nnn)-1);
			s->bsLive -= nnn;
			break;
		}
		if (s->strm->avail_in == 0) {
			return(FALSE);
		}
		s->bsBuff = (s->bsBuff << 8) | ((unsigned int) (*((unsigned char*)(s->strm->next_in))));
		s->bsLive += 8;
		s->strm->next_in++;
		s->strm->avail_in--;
	}
	return(TRUE);
}

static int bz_get_fast(DState *s)
{
	int cccc;
	s->tPos = s->tt[s->tPos];
	cccc = (unsigned char)(s->tPos & 0xff);
	s->tPos >>= 8;
	return(cccc);
}

/*---------------------------------------------------*/
static inline int BZ2_decompress(DState *s)
{
	int uc = 0;
	int	retVal;
	int	minLen,	maxLen;

	/* stuff that needs to be saved/restored */
	int  i;
	int  j;
	int  t;
	int  alphaSize;
	int  nGroups;
	int  nSelectors;
	int  EOB;
	int  groupNo;
	int  groupPos;
	int  nextSym;
	int  nblockMAX;
	int  nblock;
	int  es;
	int  N;
	int  curr;
	int  zt;
	int  zn; 
	int  zvec;
	int  zj;
	int  gSel;
	int  gMinlen;
	int *gLimit;
	int *gBase;
	int *gPerm;
	int switch_val;

	int get_mtf_val_init(void)
	{
		if (groupPos == 0) {
			groupNo++;
			if (groupNo >= nSelectors) {
				retVal = BZ_DATA_ERROR;
				return(FALSE);
			}
			groupPos = BZ_G_SIZE;
			gSel = s->selector[groupNo];
			gMinlen = s->minLens[gSel];
			gLimit = &(s->limit[gSel][0]);
			gPerm = &(s->perm[gSel][0]);
			gBase = &(s->base[gSel][0]);
		}
		groupPos--;
		zn = gMinlen;
		return(TRUE);
	}

	if (s->state == BZ_X_MAGIC_1) {
		/*initialise the save area*/
		s->save_i           = 0;
		s->save_j           = 0;
		s->save_t           = 0;
		s->save_alphaSize   = 0;
		s->save_nGroups     = 0;
		s->save_nSelectors  = 0;
		s->save_EOB         = 0;
		s->save_groupNo     = 0;
		s->save_groupPos    = 0;
		s->save_nextSym     = 0;
		s->save_nblockMAX   = 0;
		s->save_nblock      = 0;
		s->save_es          = 0;
		s->save_N           = 0;
		s->save_curr        = 0;
		s->save_zt          = 0;
		s->save_zn          = 0;
		s->save_zvec        = 0;
		s->save_zj          = 0;
		s->save_gSel        = 0;
		s->save_gMinlen     = 0;
		s->save_gLimit      = NULL;
		s->save_gBase       = NULL;
		s->save_gPerm       = NULL;
	}

	/*restore from the save area*/
	i           = s->save_i;
	j           = s->save_j;
	t           = s->save_t;
	alphaSize   = s->save_alphaSize;
	nGroups     = s->save_nGroups;
	nSelectors  = s->save_nSelectors;
	EOB         = s->save_EOB;
	groupNo     = s->save_groupNo;
	groupPos    = s->save_groupPos;
	nextSym     = s->save_nextSym;
	nblockMAX   = s->save_nblockMAX;
	nblock      = s->save_nblock;
	es          = s->save_es;
	N           = s->save_N;
	curr        = s->save_curr;
	zt          = s->save_zt;
	zn          = s->save_zn; 
	zvec        = s->save_zvec;
	zj          = s->save_zj;
	gSel        = s->save_gSel;
	gMinlen     = s->save_gMinlen;
	gLimit      = s->save_gLimit;
	gBase       = s->save_gBase;
	gPerm       = s->save_gPerm;

	retVal = BZ_OK;
	switch_val = s->state;
	switch (switch_val) {
		case BZ_X_MAGIC_1:
			s->state = BZ_X_MAGIC_1;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 'B') {
				retVal = BZ_DATA_ERROR_MAGIC;
				goto save_state_and_return;
			}

		case BZ_X_MAGIC_2:
			s->state = BZ_X_MAGIC_2;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 'Z') {
				retVal = BZ_DATA_ERROR_MAGIC;
				goto save_state_and_return;
			}

		case BZ_X_MAGIC_3:
			s->state = BZ_X_MAGIC_3;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 'h') {
				retVal = BZ_DATA_ERROR_MAGIC;
				goto save_state_and_return;
			}

		case BZ_X_MAGIC_4:
			s->state = BZ_X_MAGIC_4;
			if (! get_bits(s, &s->blockSize100k, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if ((s->blockSize100k < '1') || (s->blockSize100k > '9')) {
				retVal = BZ_DATA_ERROR_MAGIC;
				goto save_state_and_return;
			}
			s->blockSize100k -= '0';

			s->tt = xmalloc(s->blockSize100k * 100000 * sizeof(int));

		case BZ_X_BLKHDR_1:
			s->state = BZ_X_BLKHDR_1;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}

			if (uc == 0x17) {
				goto endhdr_2;
			}
			if (uc != 0x31) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_BLKHDR_2:
			s->state = BZ_X_BLKHDR_2;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x41) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_BLKHDR_3:
			s->state = BZ_X_BLKHDR_3;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x59) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_BLKHDR_4:
			s->state = BZ_X_BLKHDR_4;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x26) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_BLKHDR_5:
			s->state = BZ_X_BLKHDR_5;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x53) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_BLKHDR_6:
			s->state = BZ_X_BLKHDR_6;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x59) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		s->currBlockNo++;
		s->storedBlockCRC = 0;

		case BZ_X_BCRC_1:
			s->state = BZ_X_BCRC_1;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedBlockCRC = (s->storedBlockCRC << 8) | ((unsigned int)uc);

		case BZ_X_BCRC_2:
			s->state = BZ_X_BCRC_2;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedBlockCRC = (s->storedBlockCRC << 8) | ((unsigned int)uc);

		case BZ_X_BCRC_3:
			s->state = BZ_X_BCRC_3;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedBlockCRC = (s->storedBlockCRC << 8) | ((unsigned int)uc);

		case BZ_X_BCRC_4:
			s->state = BZ_X_BCRC_4;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedBlockCRC = (s->storedBlockCRC << 8) | ((unsigned int)uc);

		case BZ_X_RANDBIT:
			s->state = BZ_X_RANDBIT;
			{
				int tmp = s->blockRandomised;
				const int ret = get_bits(s, &tmp, 1);
				s->blockRandomised = tmp;
				if (! ret) {
					retVal = BZ_OK;
					goto save_state_and_return;
				}
			}

			s->origPtr = 0;

		case BZ_X_ORIGPTR_1:
			s->state = BZ_X_ORIGPTR_1;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->origPtr = (s->origPtr << 8) | ((int)uc);

		case BZ_X_ORIGPTR_2:
			s->state = BZ_X_ORIGPTR_2;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->origPtr = (s->origPtr << 8) | ((int)uc);

		case BZ_X_ORIGPTR_3:
			s->state = BZ_X_ORIGPTR_3;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->origPtr = (s->origPtr << 8) | ((int)uc);

			if (s->origPtr < 0) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}
			if (s->origPtr > 10 + 100000*s->blockSize100k) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

			/*--- Receive the mapping table ---*/
		case BZ_X_MAPPING_1:
			for (i = 0; i < 16; i++) {
				s->state = BZ_X_MAPPING_1;
				if (! get_bits(s, &uc, 1)) {
					retVal = BZ_OK;
					goto save_state_and_return;
				}
				if (uc == 1) {
					s->inUse16[i] = TRUE;
				} else {
					s->inUse16[i] = FALSE;
				}
			}

			for (i = 0; i < 256; i++) {
				s->inUse[i] = FALSE;
			}

			for (i = 0; i < 16; i++) {
				if (s->inUse16[i]) {
					for (j = 0; j < 16; j++) {
					case BZ_X_MAPPING_2:
						s->state = BZ_X_MAPPING_2;
						if (! get_bits(s, &uc, 1)) {
							retVal = BZ_OK;
							goto save_state_and_return;
						}
						if (uc == 1) {
							s->inUse[i * 16 + j] = TRUE;
						}
					}
				}
			}

			s->nInUse = 0;
			for (i = 0; i < 256; i++) {
				if (s->inUse[i]) {
					s->seqToUnseq[s->nInUse] = i;
					s->nInUse++;
				}
			}
			if (s->nInUse == 0) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}
			alphaSize = s->nInUse+2;

		/*--- Now the selectors ---*/
		case BZ_X_SELECTOR_1:
			s->state = BZ_X_SELECTOR_1;
			if (! get_bits(s, &nGroups, 3)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (nGroups < 2 || nGroups > 6) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_SELECTOR_2:
			s->state = BZ_X_SELECTOR_2;
			if (! get_bits(s, &nSelectors, 15)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (nSelectors < 1) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}



			for (i = 0; i < nSelectors; i++) {
				j = 0;
				while (1) {
					case BZ_X_SELECTOR_3:
					s->state = BZ_X_SELECTOR_3;
					if (! get_bits(s, &uc, 1)) {
						retVal = BZ_OK;
						goto save_state_and_return;
					}
					if (uc == 0) {
						break;
					}
					j++;
					if (j >= nGroups) {
						retVal = BZ_DATA_ERROR;
						goto save_state_and_return;
					}
				}
				s->selectorMtf[i] = j;
			}

			/*--- Undo the MTF values for the selectors. ---*/
			{
				unsigned char pos[BZ_N_GROUPS], tmp, v;
				for (v = 0; v < nGroups; v++) {
					pos[v] = v;
				}
				for (i = 0; i < nSelectors; i++) {
					v = s->selectorMtf[i];
					tmp = pos[v];
					while (v > 0) {
						pos[v] = pos[v-1];
						v--;
					}
					pos[0] = tmp;
					s->selector[i] = tmp;
				}
			}

			/*--- Now the coding tables ---*/
			for (t = 0; t < nGroups; t++) {
			case BZ_X_CODING_1:
				s->state = BZ_X_CODING_1;
				if (! get_bits(s, &curr, 5)) {
					retVal = BZ_OK;
					goto save_state_and_return;
				}
			for (i = 0; i < alphaSize; i++) {
				while (TRUE) {
					if (curr < 1 || curr > 20) {
						retVal = BZ_DATA_ERROR;
						goto save_state_and_return;
					}

					case BZ_X_CODING_2:
						s->state = BZ_X_CODING_2;
						if (! get_bits(s, &uc, 1)) {
							retVal = BZ_OK;
							goto save_state_and_return;
						}
						if (uc == 0) {
							break;
						}

					case BZ_X_CODING_3:
						s->state = BZ_X_CODING_3;
						if (! get_bits(s, &uc, 1)) {
							retVal = BZ_OK;
							goto save_state_and_return;
						}
						if (uc == 0) {
							curr++;
						} else {
							curr--;
						}
				}
				s->len[t][i] = curr;
			}
		}

		/*--- Create the Huffman decoding tables ---*/
		for (t = 0; t < nGroups; t++) {
			minLen = 32;
			maxLen = 0;
			for (i = 0; i < alphaSize; i++) {
				if (s->len[t][i] > maxLen) {
					maxLen = s->len[t][i];
				}
				if (s->len[t][i] < minLen) {
					minLen = s->len[t][i];
				}
			}

			BZ2_hbCreateDecodeTables ( 
				&(s->limit[t][0]), 
				&(s->base[t][0]), 
				&(s->perm[t][0]), 
				&(s->len[t][0]),
				minLen, maxLen, alphaSize
				);


			s->minLens[t] = minLen;
		}

		/*--- Now the MTF values ---*/

		EOB      = s->nInUse+1;
		nblockMAX = 100000 * s->blockSize100k;
		groupNo  = -1;
		groupPos = 0;

		for (i = 0; i <= 255; i++) {
			s->unzftab[i] = 0;
		}
		/*-- MTF init --*/
		{
			int ii, jj, kk;
			kk = MTFA_SIZE-1;
			for (ii = 256 / MTFL_SIZE - 1; ii >= 0; ii--) {
				for (jj = MTFL_SIZE-1; jj >= 0; jj--) {
					s->mtfa[kk] = (unsigned char)(ii * MTFL_SIZE + jj);
					kk--;
				}
				s->mtfbase[ii] = kk + 1;
			}
		}
		/*-- end MTF init --*/

		nblock = 0;

		if (! get_mtf_val_init()) {
			goto save_state_and_return;
		}
		case BZ_X_MTF_1:
			s->state = BZ_X_MTF_1;
			if (! get_bits(s, &zvec, zn)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			while (1) {
				if (zn > 20 /* the longest code */) {
					retVal = BZ_DATA_ERROR;
					goto save_state_and_return;
				}
				if (zvec <= gLimit[zn]) {
					break;
				}
				zn++;

				case BZ_X_MTF_2:
					s->state = BZ_X_MTF_2;
					if (! get_bits(s, &zj, 1)) {
						retVal = BZ_OK;
						goto save_state_and_return;
					}
					zvec = (zvec << 1) | zj;
			}
			if (zvec - gBase[zn] < 0 || zvec - gBase[zn] >= BZ_MAX_ALPHA_SIZE) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}
			nextSym = gPerm[zvec - gBase[zn]];

		while (1) {
			if (nextSym == EOB) {
				break;
			}

		if (nextSym == BZ_RUNA || nextSym == BZ_RUNB) {
			es = -1;
			N = 1;
			do {
				if (nextSym == BZ_RUNA) {
					es = es + (0+1) * N;
				} else {
					if (nextSym == BZ_RUNB) {
						es = es + (1+1) * N;
					}
				}
				N = N * 2;
				if (! get_mtf_val_init()) {
					goto save_state_and_return;
				}
				case BZ_X_MTF_3:
					s->state = BZ_X_MTF_3;
					if (! get_bits(s, &zvec, zn)) {
						retVal = BZ_OK;
						goto save_state_and_return;
					}
					while (1) {
						if (zn > 20 /* the longest code */) {
							retVal = BZ_DATA_ERROR;
							goto save_state_and_return;
						}
						if (zvec <= gLimit[zn]) {
							break;
						}
						zn++;

						case BZ_X_MTF_4:
							s->state = BZ_X_MTF_4;
							if (! get_bits(s, &zj, 1)) {
								retVal = BZ_OK;
								goto save_state_and_return;
							}
							zvec = (zvec << 1) | zj;
					}
					if (zvec - gBase[zn] < 0 || zvec - gBase[zn] >= BZ_MAX_ALPHA_SIZE) {
						retVal = BZ_DATA_ERROR;
						goto save_state_and_return;

					}
					nextSym = gPerm[zvec - gBase[zn]];
			}
			while (nextSym == BZ_RUNA || nextSym == BZ_RUNB);

			es++;
			uc = s->seqToUnseq[ s->mtfa[s->mtfbase[0]] ];
			s->unzftab[uc] += es;

			while (es > 0) {
				if (nblock >= nblockMAX) {
					retVal = BZ_DATA_ERROR;
					goto save_state_and_return;
				}
				s->tt[nblock] = (unsigned int)uc;
				nblock++;
				es--;
			}
			continue;
		} else {
			if (nblock >= nblockMAX) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}
			/*-- uc = MTF ( nextSym-1 ) --*/
			{
				int ii, jj, kk, pp, lno, off;
				unsigned int nn;
				nn = (unsigned int)(nextSym - 1);

				if (nn < MTFL_SIZE) {
					/* avoid general-case expense */
					pp = s->mtfbase[0];
					uc = s->mtfa[pp+nn];
					while (nn > 3) {
						int z = pp+nn;
						s->mtfa[(z)  ] = s->mtfa[(z)-1];
						s->mtfa[(z)-1] = s->mtfa[(z)-2];
						s->mtfa[(z)-2] = s->mtfa[(z)-3];
						s->mtfa[(z)-3] = s->mtfa[(z)-4];
						nn -= 4;
					}
					while (nn > 0) { 
						s->mtfa[(pp+nn)] = s->mtfa[(pp+nn)-1]; nn--; 
					}
					s->mtfa[pp] = uc;
				} else { 
					/* general case */
					lno = nn / MTFL_SIZE;
					off = nn % MTFL_SIZE;
					pp = s->mtfbase[lno] + off;
					uc = s->mtfa[pp];
					while (pp > s->mtfbase[lno]) { 
						s->mtfa[pp] = s->mtfa[pp-1];
						pp--; 
					}
					s->mtfbase[lno]++;
					while (lno > 0) {
						s->mtfbase[lno]--;
						s->mtfa[s->mtfbase[lno]] = s->mtfa[s->mtfbase[lno-1] + MTFL_SIZE - 1];
						lno--;
					}
					s->mtfbase[0]--;
					s->mtfa[s->mtfbase[0]] = uc;
					if (s->mtfbase[0] == 0) {
						kk = MTFA_SIZE-1;
						for (ii = 256 / MTFL_SIZE-1; ii >= 0; ii--) {
							for (jj = MTFL_SIZE-1; jj >= 0; jj--) {
								s->mtfa[kk] = s->mtfa[s->mtfbase[ii] + jj];
								kk--;
							}
							s->mtfbase[ii] = kk + 1;
						}
					}
				}
			}
			/*-- end uc = MTF ( nextSym-1 ) --*/

			s->unzftab[s->seqToUnseq[uc]]++;
			s->tt[nblock]   = (unsigned int)(s->seqToUnseq[uc]);
			nblock++;

			if (! get_mtf_val_init()) {
				goto save_state_and_return;
			}
			case BZ_X_MTF_5:
				s->state = BZ_X_MTF_5;
				if (! get_bits(s, &zvec, zn)) {
					retVal = BZ_OK;
					goto save_state_and_return;
				}
				while (1) {
					if (zn > 20 /* the longest code */) {
						retVal = BZ_DATA_ERROR;
						goto save_state_and_return;
					}
					if (zvec <= gLimit[zn]) {
						break;
					}
					zn++;

					case BZ_X_MTF_6:
						s->state = BZ_X_MTF_6;
						if (! get_bits(s, &zj, 1)) {
							retVal = BZ_OK;
							goto save_state_and_return;
						}
						zvec = (zvec << 1) | zj;
				}
			if (zvec - gBase[zn] < 0 || zvec - gBase[zn] >= BZ_MAX_ALPHA_SIZE) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}
			nextSym = gPerm[zvec - gBase[zn]];
			continue;
		}
	}

	/* Now we know what nblock is, we can do a better sanity
		check on s->origPtr.
	*/
	if (s->origPtr < 0 || s->origPtr >= nblock) {
		retVal = BZ_DATA_ERROR;
		goto save_state_and_return;
	}
	s->state_out_len = 0;
	s->state_out_ch  = 0;
	s->calculatedBlockCRC = 0xffffffffL;
	s->state = BZ_X_OUTPUT;

	/*-- Set up cftab to facilitate generation of T^(-1) --*/
	s->cftab[0] = 0;
	for (i = 1; i <= 256; i++) {
		s->cftab[i] = s->unzftab[i-1];
	}
	for (i = 1; i <= 256; i++) {
		s->cftab[i] += s->cftab[i-1];
	}

	/*-- compute the T^(-1) vector --*/
	for (i = 0; i < nblock; i++) {
		uc = (unsigned char)(s->tt[i] & 0xff);
		s->tt[s->cftab[uc]] |= (i << 8);
		s->cftab[uc]++;
	}

	s->tPos = s->tt[s->origPtr] >> 8;
	s->nblock_used = 0;
	if (s->blockRandomised) {
		s->rNToGo = 0;
		s->rTPos  = 0;
		s->k0 = bz_get_fast(s);

		s->nblock_used++;
		bz_rand_udp_mask(s);
		s->k0 ^= ((s->rNToGo == 1) ? 1 : 0);
	} else {
		s->k0 = bz_get_fast(s);
		s->nblock_used++;
	}

		retVal = BZ_OK;
		goto save_state_and_return;

endhdr_2:
		case BZ_X_ENDHDR_2:
			s->state = BZ_X_ENDHDR_2;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x72) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_ENDHDR_3:
			s->state = BZ_X_ENDHDR_3;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x45) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_ENDHDR_4:
			s->state = BZ_X_ENDHDR_4;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x38) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_ENDHDR_5:
			s->state = BZ_X_ENDHDR_5;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x50) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}

		case BZ_X_ENDHDR_6:
			s->state = BZ_X_ENDHDR_6;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			if (uc != 0x90) {
				retVal = BZ_DATA_ERROR;
				goto save_state_and_return;
			}
			s->storedCombinedCRC = 0;

		case BZ_X_CCRC_1:
			s->state = BZ_X_CCRC_1;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedCombinedCRC = (s->storedCombinedCRC << 8) | ((unsigned int)uc);
		case BZ_X_CCRC_2:
			s->state = BZ_X_CCRC_2;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedCombinedCRC = (s->storedCombinedCRC << 8) | ((unsigned int)uc);

		case BZ_X_CCRC_3:
			s->state = BZ_X_CCRC_3;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedCombinedCRC = (s->storedCombinedCRC << 8) | ((unsigned int)uc);

		case BZ_X_CCRC_4:
			s->state = BZ_X_CCRC_4;
			if (! get_bits(s, &uc, 8)) {
				retVal = BZ_OK;
				goto save_state_and_return;
			}
			s->storedCombinedCRC = (s->storedCombinedCRC << 8) | ((unsigned int)uc);

		s->state = BZ_X_IDLE;
		retVal = BZ_STREAM_END;
		goto save_state_and_return;

	}

save_state_and_return:
	s->save_i           = i;
	s->save_j           = j;
	s->save_t           = t;
	s->save_alphaSize   = alphaSize;
	s->save_nGroups     = nGroups;
	s->save_nSelectors  = nSelectors;
	s->save_EOB         = EOB;
	s->save_groupNo     = groupNo;
	s->save_groupPos    = groupPos;
	s->save_nextSym     = nextSym;
	s->save_nblockMAX   = nblockMAX;
	s->save_nblock      = nblock;
	s->save_es          = es;
	s->save_N           = N;
	s->save_curr        = curr;
	s->save_zt          = zt;
	s->save_zn          = zn;
	s->save_zvec        = zvec;
	s->save_zj          = zj;
	s->save_gSel        = gSel;
	s->save_gMinlen     = gMinlen;
	s->save_gLimit      = gLimit;
	s->save_gBase       = gBase;
	s->save_gPerm       = gPerm;

	return retVal;   
}

//int BZ2_bzDecompressInit(bz_stream* strm, int verbosity_level, int small)
static inline int BZ2_bzDecompressInit(bz_stream* strm)
{
	DState* s;

//	if (verbosity_level < 0 || verbosity_level > 4) {
//		return BZ_PARAM_ERROR;
//	}
	s = xmalloc(sizeof(DState));
	s->strm                  = strm;
	strm->state              = s;
	s->state                 = BZ_X_MAGIC_1;
	s->bsLive                = 0;
	s->bsBuff                = 0;
	s->calculatedCombinedCRC = 0;
	s->tt                    = NULL;
	s->currBlockNo           = 0;

	return BZ_OK;
}

static void bz_seterr(int eee, int *bzerror, bzFile **bzf)
{
	if (bzerror != NULL) {
		*bzerror = eee;
	}
	if (*bzf != NULL) {
		(*bzf)->lastErr = eee;
	}
}

static void BZ2_bzReadClose(int *bzerror, void *b)
{
	bzFile* bzf = (bzFile*)b;

	bz_seterr(BZ_OK, bzerror, &bzf);

	if (bzf->initialisedOk) {
		bz_stream *strm = &(bzf->strm);
		DState *s;
		if (strm == NULL) {
			return;
		}
		s = strm->state;
		if ((s == NULL) || (s->strm != strm)) {
			return;
		}
		free(s->tt);
		free(strm->state);
		strm->state = NULL;
		return;
	}
	free(bzf);
}

static void unRLE_obuf_to_output_FAST(DState *s)
{
	unsigned char k1;

	if (s->blockRandomised) {
		while (1) {
			/* try to finish existing run */
			while (1) {
				if (s->strm->avail_out == 0) {
					return;
				}
				if (s->state_out_len == 0) {
					break;
				}
				*((unsigned char *)(s->strm->next_out)) = s->state_out_ch;
				s->calculatedBlockCRC = (s->calculatedBlockCRC << 8) ^
					BZ2_crc32Table[(s->calculatedBlockCRC >> 24) ^
					((unsigned char)s->state_out_ch)];
				s->state_out_len--;
				s->strm->next_out++;
				s->strm->avail_out--;
			}
   
			/* can a new run be started? */
			if (s->nblock_used == s->save_nblock+1) {
				return;
			}
			s->state_out_len = 1;
			s->state_out_ch = s->k0;
			k1 = bz_get_fast(s);
			bz_rand_udp_mask(s);
			k1 ^= ((s->rNToGo == 1) ? 1 : 0);
			s->nblock_used++;
			if (s->nblock_used == s->save_nblock+1) {
				continue;
			}
			if (k1 != s->k0) {
				s->k0 = k1;
				continue;
			}

			s->state_out_len = 2;
			k1 = bz_get_fast(s);
			bz_rand_udp_mask(s);
			k1 ^= ((s->rNToGo == 1) ? 1 : 0);
			s->nblock_used++;
			if (s->nblock_used == s->save_nblock+1) {
				continue;
			}
			if (k1 != s->k0) {
				s->k0 = k1;
				continue;
			}
			s->state_out_len = 3;
			k1 = bz_get_fast(s);
			bz_rand_udp_mask(s);
			k1 ^= ((s->rNToGo == 1) ? 1 : 0);
			s->nblock_used++;
			if (s->nblock_used == s->save_nblock+1) {
				continue;
			}
			if (k1 != s->k0) {
				s->k0 = k1;
				continue;
			}

			k1 = bz_get_fast(s);
			bz_rand_udp_mask(s);
			k1 ^= ((s->rNToGo == 1) ? 1 : 0);
			s->nblock_used++;
			s->state_out_len = ((int)k1) + 4;
			s->k0 = bz_get_fast(s);
			bz_rand_udp_mask(s);
			s->k0 ^= ((s->rNToGo == 1) ? 1 : 0);
			s->nblock_used++;
		}
	} else {
		/* restore */
		unsigned int c_calculatedBlockCRC = s->calculatedBlockCRC;
		unsigned char c_state_out_ch       = s->state_out_ch;
		int c_state_out_len      = s->state_out_len;
		int c_nblock_used        = s->nblock_used;
		int c_k0                 = s->k0;
		unsigned int	*c_tt                 = s->tt;
		unsigned int	c_tPos               = s->tPos;
		char	*cs_next_out          = s->strm->next_out;
		unsigned int  cs_avail_out         = s->strm->avail_out;
		/* end restore */

		int        s_save_nblockPP = s->save_nblock+1;

		while (1) {
			/* try to finish existing run */
			if (c_state_out_len > 0) {
				while (TRUE) {
					if (cs_avail_out == 0) {
						goto return_notr;
					}
					if (c_state_out_len == 1) {
						break;
					}
					*((unsigned char *)(cs_next_out)) = c_state_out_ch;
					c_calculatedBlockCRC = (c_calculatedBlockCRC << 8) ^
						BZ2_crc32Table[(c_calculatedBlockCRC >> 24) ^
						((unsigned char)c_state_out_ch)];
					c_state_out_len--;
					cs_next_out++;
					cs_avail_out--;
				}
s_state_out_len_eq_one:
				{
					if (cs_avail_out == 0) { 
						c_state_out_len = 1;
						goto return_notr;
					}
					*((unsigned char *)(cs_next_out)) = c_state_out_ch;
					c_calculatedBlockCRC = (c_calculatedBlockCRC << 8) ^
						BZ2_crc32Table[(c_calculatedBlockCRC >> 24) ^
						((unsigned char)c_state_out_ch)];
					cs_next_out++;
					cs_avail_out--;
				}
			}   
			/* can a new run be started? */
			if (c_nblock_used == s_save_nblockPP) {
				c_state_out_len = 0; goto return_notr;
			}
			c_state_out_ch = c_k0;
			c_tPos = c_tt[c_tPos];
			k1 = (unsigned char)(c_tPos & 0xff);
			c_tPos >>= 8;

			c_nblock_used++;

			if (k1 != c_k0) { 
				c_k0 = k1;
				goto s_state_out_len_eq_one; 
			}

			if (c_nblock_used == s_save_nblockPP) {
				goto s_state_out_len_eq_one;
			}

			c_state_out_len = 2;
			c_tPos = c_tt[c_tPos];
			k1 = (unsigned char)(c_tPos & 0xff);
			c_tPos >>= 8;

			c_nblock_used++;
			if (c_nblock_used == s_save_nblockPP) {
				continue;
			}
			if (k1 != c_k0) {
				c_k0 = k1;
				continue;
			}

			c_state_out_len = 3;
			c_tPos = c_tt[c_tPos];
			k1 = (unsigned char)(c_tPos & 0xff);
			c_tPos >>= 8;

			c_nblock_used++;
			if (c_nblock_used == s_save_nblockPP) {
				continue;
			}
			if (k1 != c_k0) {
				c_k0 = k1;
				continue;
			}
   
			c_tPos = c_tt[c_tPos];
			k1 = (unsigned char)(c_tPos & 0xff);
			c_tPos >>= 8;

			c_nblock_used++;
			c_state_out_len = ((int)k1) + 4;

			c_tPos = c_tt[c_tPos];
			c_k0 = (unsigned char)(c_tPos & 0xff);
			c_tPos >>= 8;

			c_nblock_used++;
		}

return_notr:

		/* save */
		s->calculatedBlockCRC = c_calculatedBlockCRC;
		s->state_out_ch       = c_state_out_ch;
		s->state_out_len      = c_state_out_len;
		s->nblock_used        = c_nblock_used;
		s->k0                 = c_k0;
		s->tt                 = c_tt;
		s->tPos               = c_tPos;
		s->strm->next_out     = cs_next_out;
		s->strm->avail_out    = cs_avail_out;
		/* end save */
	}
}
static inline
int BZ2_bzDecompress(bz_stream *strm)
{
	DState* s;
	s = strm->state;

	while (1) {
		if (s->state == BZ_X_IDLE) {
			return BZ_SEQUENCE_ERROR;
		}
		if (s->state == BZ_X_OUTPUT) {
			unRLE_obuf_to_output_FAST(s);
			if (s->nblock_used == s->save_nblock+1 && s->state_out_len == 0) {
				s->calculatedBlockCRC = ~(s->calculatedBlockCRC);
				if (s->calculatedBlockCRC != s->storedBlockCRC) {
					return BZ_DATA_ERROR;
				}
				s->calculatedCombinedCRC = (s->calculatedCombinedCRC << 1) | (s->calculatedCombinedCRC >> 31);
				s->calculatedCombinedCRC ^= s->calculatedBlockCRC;
				s->state = BZ_X_BLKHDR_1;
			} else {
				return BZ_OK;
			}
		}
		if (s->state >= BZ_X_MAGIC_1) {
			int r = BZ2_decompress(s);
			if (r == BZ_STREAM_END) {
				if (s->calculatedCombinedCRC != s->storedCombinedCRC) {
					return BZ_DATA_ERROR;
				}
				return r;
			}
			if (s->state != BZ_X_OUTPUT) {
				return r;
			}
		}
	}

	return(0);  /*NOTREACHED*/
}

static inline int BZ2_bzRead(int *bzerror, void *b, void *buf, int len)
{
	int n, ret;
	bzFile *bzf = (bzFile*)b;

	bz_seterr(BZ_OK, bzerror, &bzf);

	if (len == 0) {
		bz_seterr(BZ_OK, bzerror, &bzf);
		return 0;
	}

	bzf->strm.avail_out = len;
	bzf->strm.next_out = buf;

	while (1) {
		if (ferror(bzf->handle)) {
			bz_seterr(BZ_IO_ERROR, bzerror, &bzf);
			return 0;
		}
		if ((bzf->strm.avail_in == 0) && !myfeof(bzf->handle)) {
			n = fread(bzf->buf, sizeof(unsigned char), BZ_MAX_UNUSED, bzf->handle);
			if (ferror(bzf->handle)) {
				bz_seterr(BZ_IO_ERROR, bzerror, &bzf);
				return 0;
			}
			bzf->bufN = n;
			bzf->strm.avail_in = bzf->bufN;
			bzf->strm.next_in = bzf->buf;
		}

		ret = BZ2_bzDecompress(&(bzf->strm));

		if ((ret != BZ_OK) && (ret != BZ_STREAM_END)) {
			bz_seterr(ret, bzerror, &bzf);
			return 0;
		}

		if ((ret == BZ_OK) && myfeof(bzf->handle) &&
			(bzf->strm.avail_in == 0) && (bzf->strm.avail_out > 0)) {
			bz_seterr(BZ_UNEXPECTED_EOF, bzerror, &bzf);
			return(0);
		}

		if (ret == BZ_STREAM_END) {
			bz_seterr(BZ_STREAM_END, bzerror, &bzf);
			return(len - bzf->strm.avail_out);
		}
		if (bzf->strm.avail_out == 0) {
			bz_seterr(BZ_OK, bzerror, &bzf);
			return(len);
		}
	}
	return(0); /*not reached*/
}

static inline void *BZ2_bzReadOpen(int *bzerror, FILE *f, void *unused, int nUnused)
{
	bzFile *bzf = xmalloc(sizeof(bzFile));
	int ret;

	bz_seterr(BZ_OK, bzerror, &bzf);

	bzf->initialisedOk = FALSE;
	bzf->handle        = f;
	bzf->bufN          = 0;

	ret = BZ2_bzDecompressInit(&(bzf->strm));
	if (ret != BZ_OK) {
		bz_seterr(ret, bzerror, &bzf);
		free(bzf);
		return NULL;
	}

	while (nUnused > 0) {
		bzf->buf[bzf->bufN] = *((unsigned char *)(unused)); bzf->bufN++;
		unused = ((void *)( 1 + ((unsigned char *)(unused))  ));
		nUnused--;
	}
	bzf->strm.avail_in = bzf->bufN;
	bzf->strm.next_in  = bzf->buf;

	bzf->initialisedOk = TRUE;
	return bzf;   
}

static inline unsigned char uncompressStream(FILE *zStream, FILE *stream)
{
	unsigned char unused[BZ_MAX_UNUSED];
	unsigned char *unusedTmp;
	unsigned char obuf[5000];
	bzFile *bzf = NULL;
	int bzerr_dummy;
	int bzerr;
	int nread;
	int nUnused;
	int streamNo;
	int ret;
	int i;

	nUnused = 0;
	streamNo = 0;

	if (ferror(stream)) {
		goto errhandler_io;
	}
	if (ferror(zStream)) {
		goto errhandler_io;
	}

	while(1) {
		bzf = BZ2_bzReadOpen(&bzerr, zStream, unused, nUnused);
		if (bzf == NULL || bzerr != BZ_OK) {
			goto errhandler;
		}
		streamNo++;

		while (bzerr == BZ_OK) {
			nread = BZ2_bzRead(&bzerr, bzf, obuf, 5000);
			if (bzerr == BZ_DATA_ERROR_MAGIC) {
				goto errhandler;
			}
			if ((bzerr == BZ_OK || bzerr == BZ_STREAM_END) && nread > 0) {
				fwrite(obuf, sizeof(unsigned char), nread, stream);
			}
			if (ferror(stream)) {
				goto errhandler_io;
			}
		}
		if (bzerr != BZ_STREAM_END) {
			goto errhandler;
		}
		nUnused = bzf->strm.avail_in;
		unusedTmp = bzf->strm.next_in;
		bz_seterr(BZ_OK, &bzerr, &bzf);
		for (i = 0; i < nUnused; i++) {
			unused[i] = unusedTmp[i];
		}
		BZ2_bzReadClose(&bzerr, bzf);
		if ((nUnused == 0) && myfeof(zStream)) {
			break;
		}
	}

	if (ferror(zStream)) {
		goto errhandler_io;
	}
	ret = fclose(zStream);
	if (ret == EOF) {
		goto errhandler_io;
	}
	if (ferror(stream)) {
		goto errhandler_io;
	}
	ret = fflush(stream);
	if (ret != 0) {
		goto errhandler_io;
	}
	if (stream != stdout) {
		ret = fclose(stream);
		if (ret == EOF) {
			goto errhandler_io;
		}
	}
	return TRUE;

errhandler:
	BZ2_bzReadClose ( &bzerr_dummy, bzf );
	switch (bzerr) {
		case BZ_IO_ERROR:
errhandler_io:
			error_msg("\n%s: I/O or other error, bailing out.  "
				"Possible reason follows.\n", applet_name);
			perror(applet_name);
			exit(1);
		case BZ_DATA_ERROR:
			error_msg("\n%s: Data integrity error when decompressing.\n", applet_name);
			exit(2);
		case BZ_UNEXPECTED_EOF:
			error_msg("\n%s: Compressed file ends unexpectedly;\n\t"
				"perhaps it is corrupted?  *Possible* reason follows.\n", applet_name);
			perror(applet_name);
			exit(2);
		case BZ_DATA_ERROR_MAGIC:
			if (zStream != stdin) {
				fclose(zStream);
			}
			if (stream != stdout) {
				fclose(stream);
			}
			if (streamNo == 1) {
				return FALSE;
			} else {
				return TRUE;
			}
	}

	return(TRUE); /*notreached*/
}

int bunzip2_main(int argc, char **argv)
{
	const int bunzip_to_stdout = 1;
	const int bunzip_force = 2;
	int flags = 0;
	int opt = 0;
	int status;

	FILE *src_stream;
	FILE *dst_stream;
	char *save_name = NULL;
	char *delete_name = NULL;

	/* if called as bzcat */
	if (strcmp(applet_name, "bzcat") == 0)
		flags |= bunzip_to_stdout;

	while ((opt = getopt(argc, argv, "cfh")) != -1) {
		switch (opt) {
		case 'c':
			flags |= bunzip_to_stdout;
			break;
		case 'f':
			flags |= bunzip_force;
			break;
		case 'h':
		default:
			show_usage(); /* exit's inside usage */
		}
	}

	/* Set input filename and number */
	if (argv[optind] == NULL || strcmp(argv[optind], "-") == 0) {
		flags |= bunzip_to_stdout;
		src_stream = stdin;
	} else {
		/* Open input file */
		src_stream = xfopen(argv[optind], "r");

		save_name = xstrdup(argv[optind]);
		if (strcmp(save_name + strlen(save_name) - 4, ".bz2") != 0)
			error_msg_and_die("Invalid extension");
		save_name[strlen(save_name) - 4] = '\0';
	}

	/* Check that the input is sane.  */
	if (isatty(fileno(src_stream)) && (flags & bunzip_force) == 0)
		error_msg_and_die("compressed data not read from terminal.  Use -f to force it.");

	if (flags & bunzip_to_stdout) {
		dst_stream = stdout;
	} else {
		dst_stream = xfopen(save_name, "w");
	}

	if (uncompressStream(src_stream, dst_stream)) {
		if (!(flags & bunzip_to_stdout))
			delete_name = argv[optind];
		status = EXIT_SUCCESS;
	} else {
		if (!(flags & bunzip_to_stdout))
			delete_name = save_name;
		status = EXIT_FAILURE;
	}

	if (delete_name) {
		if (unlink(delete_name) < 0) {
			error_msg_and_die("Couldn't remove %s", delete_name);
		}
	}

	return status;
}
