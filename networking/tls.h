/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* config tweaks */
#define HAVE_NATIVE_INT64 1
#undef  DISABLE_PSTM
#undef  USE_1024_KEY_SPEED_OPTIMIZATIONS
#undef  USE_2048_KEY_SPEED_OPTIMIZATIONS
//TODO: enable to use asm:
//#if defined(__GNUC__) && defined(__i386__)   -> #define PSTM_32BIT and PSTM_X86
//#if defined(__GNUC__) && defined(__x86_64__) -> #define PSTM_64BIT and PSTM_X86_64
//ARM and MIPS also have these


#define PS_SUCCESS              0
#define PS_FAILURE              -1
#define PS_ARG_FAIL             -6      /* Failure due to bad function param */
#define PS_PLATFORM_FAIL        -7      /* Failure as a result of system call error */
#define PS_MEM_FAIL             -8      /* Failure to allocate requested memory */
#define PS_LIMIT_FAIL           -9      /* Failure on sanity/limit tests */

#define PS_TRUE         1
#define PS_FALSE        0

#if BB_BIG_ENDIAN
# define ENDIAN_BIG     1
# undef  ENDIAN_LITTLE
//#????  ENDIAN_32BITWORD
// controls only STORE32L, which we don't use
#else
# define ENDIAN_LITTLE  1
# undef  ENDIAN_BIG
#endif

typedef uint64_t uint64;
typedef  int64_t  int64;
typedef uint32_t uint32;
typedef  int32_t  int32;
typedef uint16_t uint16;
typedef  int16_t  int16;

//FIXME
typedef char psPool_t;

//#ifdef PS_PUBKEY_OPTIMIZE_FOR_SMALLER_RAM
#define PS_EXPTMOD_WINSIZE   3
//#ifdef PS_PUBKEY_OPTIMIZE_FOR_FASTER_SPEED
//#define PS_EXPTMOD_WINSIZE 5

#define PUBKEY_TYPE     0x01
#define PRIVKEY_TYPE    0x02

void tls_get_random(void *buf, unsigned len);

#define matrixCryptoGetPrngData(buf, len, userPtr) (tls_get_random(buf, len), PS_SUCCESS)

#define psFree(p, pool)    free(p)
#define psTraceCrypto(msg) bb_error_msg_and_die(msg)

/* Secure zerofill */
#define memset_s(A,B,C,D) memset((A),(C),(D))
/* Constant time memory comparison */
#define memcmpct(s1, s2, len) memcmp((s1), (s2), (len))
#undef min
#define min(x, y) ((x) < (y) ? (x) : (y))


#include "tls_pstm.h"
#include "tls_rsa.h"
