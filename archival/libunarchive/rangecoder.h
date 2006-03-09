/*
 * Small range coder implementation for lzma.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Based on LzmaDecode.c from the LZMA SDK 4.22 (http://www.7-zip.org/)
 * Copyright (c) 1999-2005  Igor Pavlov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdint.h>

#include "libbb.h"

#ifndef always_inline
#  if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ >0)
#    define always_inline __attribute__((always_inline)) inline
#  else
#    define always_inline inline
#  endif
#endif

#ifdef CONFIG_FEATURE_LZMA_FAST
#  define speed_inline always_inline
#else
#  define speed_inline
#endif


typedef struct {
	int fd;
	uint8_t *ptr;
	uint8_t *buffer;
	uint8_t *buffer_end;
	int buffer_size;
	uint32_t code;
	uint32_t range;
	uint32_t bound;
} rc_t;


#define RC_TOP_BITS 24
#define RC_MOVE_BITS 5
#define RC_MODEL_TOTAL_BITS 11


/* Called twice: once at startup and once in rc_normalize() */
static void rc_read(rc_t * rc)
{
	rc->buffer_size = read(rc->fd, rc->buffer, rc->buffer_size);
	if (rc->buffer_size <= 0)
		bb_error_msg_and_die("unexpected EOF");
	rc->ptr = rc->buffer;
	rc->buffer_end = rc->buffer + rc->buffer_size;
}

/* Called once */
static always_inline void rc_init(rc_t * rc, int fd, int buffer_size)
{
	int i;

	rc->fd = fd;
	rc->buffer = malloc(buffer_size);
	rc->buffer_size = buffer_size;
	rc->buffer_end = rc->buffer + rc->buffer_size;
	rc->ptr = rc->buffer_end;

	rc->code = 0;
	rc->range = 0xFFFFFFFF;
	for (i = 0; i < 5; i++) {
		if (rc->ptr >= rc->buffer_end)
			rc_read(rc);
		rc->code = (rc->code << 8) | *rc->ptr++;
	}
}

/* Called once. TODO: bb_maybe_free() */
static always_inline void rc_free(rc_t * rc)
{
	if (ENABLE_FEATURE_CLEAN_UP)
		free(rc->buffer);
}

/* Called twice, but one callsite is in speed_inline'd rc_is_bit_0_helper() */
static void rc_do_normalize(rc_t * rc)
{
	if (rc->ptr >= rc->buffer_end)
		rc_read(rc);
	rc->range <<= 8;
	rc->code = (rc->code << 8) | *rc->ptr++;
}
static always_inline void rc_normalize(rc_t * rc)
{
	if (rc->range < (1 << RC_TOP_BITS)) {
		rc_do_normalize(rc);
	}
}

/* Called 9 times */
/* Why rc_is_bit_0_helper exists?
 * Because we want to always expose (rc->code < rc->bound) to optimizer
 */
static speed_inline uint32_t rc_is_bit_0_helper(rc_t * rc, uint16_t * p)
{
	rc_normalize(rc);
	rc->bound = *p * (rc->range >> RC_MODEL_TOTAL_BITS);
	return rc->bound;
}
static always_inline int rc_is_bit_0(rc_t * rc, uint16_t * p)
{
	uint32_t t = rc_is_bit_0_helper(rc, p);
	return rc->code < t;
}

/* Called ~10 times, but very small, thus inlined */
static speed_inline void rc_update_bit_0(rc_t * rc, uint16_t * p)
{
	rc->range = rc->bound;
	*p += ((1 << RC_MODEL_TOTAL_BITS) - *p) >> RC_MOVE_BITS;
}
static speed_inline void rc_update_bit_1(rc_t * rc, uint16_t * p)
{
	rc->range -= rc->bound;
	rc->code -= rc->bound;
	*p -= *p >> RC_MOVE_BITS;
}

/* Called 4 times in unlzma loop */
static int rc_get_bit(rc_t * rc, uint16_t * p, int *symbol)
{
	if (rc_is_bit_0(rc, p)) {
		rc_update_bit_0(rc, p);
		*symbol *= 2;
		return 0;
	} else {
		rc_update_bit_1(rc, p);
		*symbol = *symbol * 2 + 1;
		return 1;
	}
}

/* Called once */
static always_inline int rc_direct_bit(rc_t * rc)
{
	rc_normalize(rc);
	rc->range >>= 1;
	if (rc->code >= rc->range) {
		rc->code -= rc->range;
		return 1;
	}
	return 0;
}

/* Called twice */
static speed_inline void
rc_bit_tree_decode(rc_t * rc, uint16_t * p, int num_levels, int *symbol)
{
	int i = num_levels;

	*symbol = 1;
	while (i--)
		rc_get_bit(rc, p + *symbol, symbol);
	*symbol -= 1 << num_levels;
}

/* vi:set ts=4: */
