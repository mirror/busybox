/* vi: set sw=4 ts=4: */
/*
 * Trick to assign a const ptr with barrier for clang
 *
 * Copyright (C) 2021 by YU Jincheng <shana@zju.edu.cn>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

#if defined(__clang_major__) && __clang_major__ >= 9
/* Clang/llvm drops assignment to "constant" storage. Silently.
 * Needs serious convincing to not eliminate the store.
 */
static ALWAYS_INLINE void* not_const_pp(const void *p)
{
	void *pp;
	asm volatile (
		"# forget that p points to const"
		: /*outputs*/ "=r" (pp)
		: /*inputs*/ "0" (p)
	);
	return pp;
}
void FAST_FUNC ASSIGN_CONST_PTR(const void *pptr, void *v)
{
	*(void**)not_const_pp(pptr) = v;
	barrier();
}
void FAST_FUNC XZALLOC_CONST_PTR(const void *pptr, size_t size)
{
	*(void**)not_const_pp(pptr) = xzalloc(size);
	barrier();
}
#endif
