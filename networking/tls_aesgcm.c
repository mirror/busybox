/*
 * Copyright (C) 2018 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

#include "tls.h"

typedef uint8_t byte;
typedef uint32_t word32;
#define XMEMSET memset
#define XMEMCPY memcpy

void FAST_FUNC xorbuf(void* buf, const void* mask, unsigned count)
{
    word32 i;
    byte*       b = (byte*)buf;
    const byte* m = (const byte*)mask;
    for (i = 0; i < count; i++)
        b[i] ^= m[i];
}

/* from wolfssl-3.15.3/wolfcrypt/src/aes.c */

static ALWAYS_INLINE void FlattenSzInBits(byte* buf, word32 sz)
{
    /* Multiply the sz by 8 */
//bbox: these sizes are never even close to 2^32/8
//    word32 szHi = (sz >> (8*sizeof(sz) - 3));
    sz <<= 3;

    /* copy over the words of the sz into the destination buffer */
//    buf[0] = (szHi >> 24) & 0xff;
//    buf[1] = (szHi >> 16) & 0xff;
//    buf[2] = (szHi >>  8) & 0xff;
//    buf[3] = szHi & 0xff;
    *(uint32_t*)(buf + 0) = 0;
//    buf[4] = (sz >> 24) & 0xff;
//    buf[5] = (sz >> 16) & 0xff;
//    buf[6] = (sz >>  8) & 0xff;
//    buf[7] = sz & 0xff;
    *(uint32_t*)(buf + 4) = SWAP_BE32(sz);
}

static void RIGHTSHIFTX(byte* x)
{
    int i;
    int carryOut = 0;
    int carryIn = 0;
    int borrow = x[15] & 0x01;

    for (i = 0; i < AES_BLOCK_SIZE; i++) {
        carryOut = x[i] & 0x01;
        x[i] = (x[i] >> 1) | (carryIn ? 0x80 : 0);
        carryIn = carryOut;
    }
    if (borrow) x[0] ^= 0xE1;
}

static void GMULT(byte* X, byte* Y)
{
    byte Z[AES_BLOCK_SIZE];
    byte V[AES_BLOCK_SIZE];
    int i, j;

    XMEMSET(Z, 0, AES_BLOCK_SIZE);
    XMEMCPY(V, X, AES_BLOCK_SIZE);
    for (i = 0; i < AES_BLOCK_SIZE; i++)
    {
        byte y = Y[i];
        for (j = 0; j < 8; j++)
        {
            if (y & 0x80) {
                xorbuf(Z, V, AES_BLOCK_SIZE);
            }

            RIGHTSHIFTX(V);
            y = y << 1;
        }
    }
    XMEMCPY(X, Z, AES_BLOCK_SIZE);
}

//bbox:
// for TLS AES-GCM, a (which as AAD) is always 13 bytes long, and bbox code provides
// extra 3 zeroed bytes, making it a[16], or a[AES_BLOCK_SIZE].
// Resulting auth tag in s is also always AES_BLOCK_SIZE bytes.
//
// This allows some simplifications.
#define aSz AES_BLOCK_SIZE
#define sSz AES_BLOCK_SIZE
void FAST_FUNC aesgcm_GHASH(byte* h,
    const byte* a, //unsigned aSz,
    const byte* c, unsigned cSz,
    byte* s //, unsigned sSz
)
{
    byte x[AES_BLOCK_SIZE] ALIGNED(4);
    byte scratch[AES_BLOCK_SIZE] ALIGNED(4);
    word32 blocks, partial;
    //was: byte* h = aes->H;

    //XMEMSET(x, 0, AES_BLOCK_SIZE);

    /* Hash in A, the Additional Authentication Data */
//    if (aSz != 0 && a != NULL) {
//        blocks = aSz / AES_BLOCK_SIZE;
//        partial = aSz % AES_BLOCK_SIZE;
//        while (blocks--) {
            //xorbuf(x, a, AES_BLOCK_SIZE);
            XMEMCPY(x, a, AES_BLOCK_SIZE);// memcpy(x,a) = memset(x,0)+xorbuf(x,a)
            GMULT(x, h);
//            a += AES_BLOCK_SIZE;
//        }
//        if (partial != 0) {
//            XMEMSET(scratch, 0, AES_BLOCK_SIZE);
//            XMEMCPY(scratch, a, partial);
//            xorbuf(x, scratch, AES_BLOCK_SIZE);
//            GMULT(x, h);
//        }
//    }

    /* Hash in C, the Ciphertext */
    if (cSz != 0 /*&& c != NULL*/) {
        blocks = cSz / AES_BLOCK_SIZE;
        partial = cSz % AES_BLOCK_SIZE;
        while (blocks--) {
            xorbuf(x, c, AES_BLOCK_SIZE);
            GMULT(x, h);
            c += AES_BLOCK_SIZE;
        }
        if (partial != 0) {
            XMEMSET(scratch, 0, AES_BLOCK_SIZE);
            XMEMCPY(scratch, c, partial);
            xorbuf(x, scratch, AES_BLOCK_SIZE);
            GMULT(x, h);
        }
    }

    /* Hash in the lengths of A and C in bits */
    FlattenSzInBits(&scratch[0], aSz);
    FlattenSzInBits(&scratch[8], cSz);
    xorbuf(x, scratch, AES_BLOCK_SIZE);
    GMULT(x, h);

    /* Copy the result into s. */
    XMEMCPY(s, x, sSz);
}
