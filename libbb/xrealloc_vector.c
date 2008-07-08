/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2008 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* Resize (grow) malloced vector.
 *
 *  #define magic packed two parameters into one:
 *  sizeof = sizeof_and_shift >> 8
 *  shift  = (sizeof_and_shift) & 0xff
 *  (TODO: encode "I want it zeroed" in lowest bit?)
 *
 * Lets say shift = 4. 1 << 4 == 0x10.
 * If idx == 0, 0x10, 0x20 etc, vector[] is resized to next higher
 * idx step, plus one: if idx == 0x20, vector[] is resized to 0x31,
 * thus last usable element is vector[0x30].
 *
 * In other words: after xrealloc_vector(v, 4, idx) it's ok to use
 * at least v[idx] and v[idx+1], for all idx values.
 */
void* FAST_FUNC xrealloc_vector_helper(void *vector, unsigned sizeof_and_shift, int idx)
{
	int mask = 1 << (uint8_t)sizeof_and_shift;

	if (!(idx & (mask - 1))) {
		sizeof_and_shift >>= 8;
		vector = xrealloc(vector, sizeof_and_shift * (idx + mask + 1));
	}
	return vector;
}
