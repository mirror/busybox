/*
 * Utility routines.
 *
 * Copyright (C) 2025 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-y += bitops.o

#include "libbb.h"

void FAST_FUNC xorbuf_3(void *dst, const void *src1, const void *src2, unsigned count)
{
	uint8_t *d = dst;
	const uint8_t *s1 = src1;
	const uint8_t *s2 = src2;
#if BB_UNALIGNED_MEMACCESS_OK
	while (count >= sizeof(long)) {
		*(long*)d = *(long*)s1 ^ *(long*)s2;
		count -= sizeof(long);
		d += sizeof(long);
		s1 += sizeof(long);
		s2 += sizeof(long);
	}
#endif
	while (count--)
		*d++ = *s1++ ^ *s2++;
}

void FAST_FUNC xorbuf(void *dst, const void *src, unsigned count)
{
	xorbuf_3(dst, dst, src, count);
}

void FAST_FUNC xorbuf16_aligned_long(void *dst, const void *src)
{
#if defined(__SSE__) /* any x86_64 has it */
	asm volatile(
"\n		movups	(%0),%%xmm0"
"\n		movups	(%1),%%xmm1"   // can't just xorps(%1),%%xmm0:
"\n		xorps	%%xmm1,%%xmm0" // SSE requires 16-byte alignment
"\n		movups	%%xmm0,(%0)"
"\n"
		: "=r" (dst), "=r" (src)
		: "0" (dst), "1" (src)
		: "xmm0", "xmm1", "memory"
	);
#else
	unsigned long *d = dst;
	const unsigned long *s = src;
	d[0] ^= s[0];
# if LONG_MAX <= 0x7fffffffffffffff
	d[1] ^= s[1];
#  if LONG_MAX == 0x7fffffff
	d[2] ^= s[2];
	d[3] ^= s[3];
#  endif
# endif
#endif
}
// The above can be inlined in libbb.h, in a way where compiler
// is even free to use better addressing modes than (%reg), and
// to keep the result in a register
// (to not store it to memory after each XOR):
//#if defined(__SSE__)
//#include <xmmintrin.h>
//^^^ or just: typedef float __m128_u attribute((__vector_size__(16),__may_alias__,__aligned__(1)));
//static ALWAYS_INLINE void xorbuf16_aligned_long(void *dst, const void *src)
//{
//	__m128_u xmm0, xmm1;
//	asm volatile(
//"\n		xorps	%1,%0"
//		: "=x" (xmm0), "=x" (xmm1)
//		: "0" (*(__m128_u*)dst), "1" (*(__m128_u*)src)
//	);
//	*(__m128_u*)dst = xmm0; // this store may be optimized out!
//}
//#endif
// but I don't trust gcc optimizer enough to not generate some monstrosity.
// See GMULT() function in TLS code as an example.

void FAST_FUNC xorbuf64_3_aligned64(void *dst, const void *src1, const void *src2)
{
#if defined(__SSE__) /* any x86_64 has it */
	asm volatile(
"\n		movups	0*16(%1),%%xmm0"
"\n		movups	0*16(%2),%%xmm1" // can't just xorps(%2),%%xmm0:
"\n		xorps	%%xmm1,%%xmm0"   // SSE requires 16-byte alignment, we have only 8-byte
"\n		movups	%%xmm0,0*16(%0)"
"\n		movups	1*16(%1),%%xmm0"
"\n		movups	1*16(%2),%%xmm1"
"\n		xorps	%%xmm1,%%xmm0"
"\n		movups	%%xmm0,1*16(%0)"
"\n		movups	2*16(%1),%%xmm0"
"\n		movups	2*16(%2),%%xmm1"
"\n		xorps	%%xmm1,%%xmm0"
"\n		movups	%%xmm0,2*16(%0)"
"\n		movups	3*16(%1),%%xmm0"
"\n		movups	3*16(%2),%%xmm1"
"\n		xorps	%%xmm1,%%xmm0"
"\n		movups	%%xmm0,3*16(%0)"
"\n"
		: "=r" (dst), "=r" (src1), "=r" (src2)
		: "0" (dst), "1" (src1), "2" (src2)
		: "xmm0", "xmm1", "memory"
	);
#else
	long *d = dst;
	const long *s1 = src1;
	const long *s2 = src2;
	unsigned count = 64 / sizeof(long);
	do {
		*d++ = *s1++ ^ *s2++;
	} while (--count != 0);
#endif
}

#if !BB_UNALIGNED_MEMACCESS_OK
void FAST_FUNC xorbuf16(void *dst, const void *src)
{
#define p_aligned(a) (((uintptr_t)(a) & (sizeof(long)-1)) == 0)
	if (p_aligned(src) && p_aligned(dst)) {
		xorbuf16_aligned_long(dst, src);
		return;
	}
	xorbuf_3(dst, dst, src, 16);
}
#endif
