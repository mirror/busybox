/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Based on LzmaDecode.c from the LZMA SDK 4.22 (http://www.7-zip.org/)
 * Copyright (C) 1999-2005  Igor Pavlov
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
#include <unistd.h>
#include <stdio.h>
#include <byteswap.h>

#include "libbb.h"

#include "rangecoder.h"


typedef struct {
	uint8_t pos;
	uint32_t dict_size;
	uint64_t dst_size;
} __attribute__ ((packed)) lzma_header_t;


#define LZMA_BASE_SIZE 1846
#define LZMA_LIT_SIZE 768

#define LZMA_NUM_POS_BITS_MAX 4

#define LZMA_LEN_NUM_LOW_BITS 3
#define LZMA_LEN_NUM_MID_BITS 3
#define LZMA_LEN_NUM_HIGH_BITS 8

#define LZMA_LEN_CHOICE 0
#define LZMA_LEN_CHOICE_2 (LZMA_LEN_CHOICE + 1)
#define LZMA_LEN_LOW (LZMA_LEN_CHOICE_2 + 1)
#define LZMA_LEN_MID (LZMA_LEN_LOW \
		      + (1 << (LZMA_NUM_POS_BITS_MAX + LZMA_LEN_NUM_LOW_BITS)))
#define LZMA_LEN_HIGH (LZMA_LEN_MID \
		       +(1 << (LZMA_NUM_POS_BITS_MAX + LZMA_LEN_NUM_MID_BITS)))
#define LZMA_NUM_LEN_PROBS (LZMA_LEN_HIGH + (1 << LZMA_LEN_NUM_HIGH_BITS))

#define LZMA_NUM_STATES 12
#define LZMA_NUM_LIT_STATES 7

#define LZMA_START_POS_MODEL_INDEX 4
#define LZMA_END_POS_MODEL_INDEX 14
#define LZMA_NUM_FULL_DISTANCES (1 << (LZMA_END_POS_MODEL_INDEX >> 1))

#define LZMA_NUM_POS_SLOT_BITS 6
#define LZMA_NUM_LEN_TO_POS_STATES 4

#define LZMA_NUM_ALIGN_BITS 4

#define LZMA_MATCH_MIN_LEN 2

#define LZMA_IS_MATCH 0
#define LZMA_IS_REP (LZMA_IS_MATCH + (LZMA_NUM_STATES <<LZMA_NUM_POS_BITS_MAX))
#define LZMA_IS_REP_G0 (LZMA_IS_REP + LZMA_NUM_STATES)
#define LZMA_IS_REP_G1 (LZMA_IS_REP_G0 + LZMA_NUM_STATES)
#define LZMA_IS_REP_G2 (LZMA_IS_REP_G1 + LZMA_NUM_STATES)
#define LZMA_IS_REP_0_LONG (LZMA_IS_REP_G2 + LZMA_NUM_STATES)
#define LZMA_POS_SLOT (LZMA_IS_REP_0_LONG \
		       + (LZMA_NUM_STATES << LZMA_NUM_POS_BITS_MAX))
#define LZMA_SPEC_POS (LZMA_POS_SLOT \
		       +(LZMA_NUM_LEN_TO_POS_STATES << LZMA_NUM_POS_SLOT_BITS))
#define LZMA_ALIGN (LZMA_SPEC_POS \
		    + LZMA_NUM_FULL_DISTANCES - LZMA_END_POS_MODEL_INDEX)
#define LZMA_LEN_CODER (LZMA_ALIGN + (1 << LZMA_NUM_ALIGN_BITS))
#define LZMA_REP_LEN_CODER (LZMA_LEN_CODER + LZMA_NUM_LEN_PROBS)
#define LZMA_LITERAL (LZMA_REP_LEN_CODER + LZMA_NUM_LEN_PROBS)


int unlzma(int src_fd, int dst_fd)
{
	lzma_header_t header;
	int lc, pb, lp;
	uint32_t pos_state_mask;
	uint32_t literal_pos_mask;
	uint32_t pos;
	uint16_t *p;
	uint16_t *prob;
	uint16_t *prob_lit;
	int num_bits;
	int num_probs;
	rc_t rc;
	int i, mi;
	uint8_t *buffer;
	uint8_t previous_byte = 0;
	size_t buffer_pos = 0, global_pos = 0;
	int len = 0;
	int state = 0;
	uint32_t rep0 = 1, rep1 = 1, rep2 = 1, rep3 = 1;

	if (read(src_fd, &header, sizeof(header)) != sizeof(header))
		bb_error_msg_and_die("can't read header");

	if (header.pos >= (9 * 5 * 5))
		bb_error_msg_and_die("bad header");
	mi = header.pos / 9;
	lc = header.pos % 9;
	pb = mi / 5;
	lp = mi % 5;
	pos_state_mask = (1 << pb) - 1;
	literal_pos_mask = (1 << lp) - 1;

#if BB_BIG_ENDIAN
	header.dict_size = bswap_32(header.dict_size);
	header.dst_size = bswap_64(header.dst_size);
#endif

	if (header.dict_size == 0)
		header.dict_size = 1;

	buffer = xmalloc(MIN(header.dst_size, header.dict_size));

	num_probs = LZMA_BASE_SIZE + (LZMA_LIT_SIZE << (lc + lp));
	p = xmalloc(num_probs * sizeof(*p));
	num_probs = LZMA_LITERAL + (LZMA_LIT_SIZE << (lc + lp));
	for (i = 0; i < num_probs; i++)
		p[i] = (1 << RC_MODEL_TOTAL_BITS) >> 1;

	rc_init(&rc, src_fd, 0x10000);

	while (global_pos + buffer_pos < header.dst_size) {
		int pos_state = (buffer_pos + global_pos) & pos_state_mask;

		prob =
			p + LZMA_IS_MATCH + (state << LZMA_NUM_POS_BITS_MAX) + pos_state;
		if (rc_is_bit_0(&rc, prob)) {
			mi = 1;
			rc_update_bit_0(&rc, prob);
			prob = (p + LZMA_LITERAL + (LZMA_LIT_SIZE
					* ((((buffer_pos + global_pos) & literal_pos_mask) << lc)
					+ (previous_byte >> (8 - lc)))));

			if (state >= LZMA_NUM_LIT_STATES) {
				int match_byte;

				pos = buffer_pos - rep0;
				while (pos >= header.dict_size)
					pos += header.dict_size;
				match_byte = buffer[pos];
				do {
					int bit;

					match_byte <<= 1;
					bit = match_byte & 0x100;
					prob_lit = prob + 0x100 + bit + mi;
					if (rc_get_bit(&rc, prob_lit, &mi)) {
						if (!bit)
							break;
					} else {
						if (bit)
							break;
					}
				} while (mi < 0x100);
			}
			while (mi < 0x100) {
				prob_lit = prob + mi;
				rc_get_bit(&rc, prob_lit, &mi);
			}
			previous_byte = (uint8_t) mi;

			buffer[buffer_pos++] = previous_byte;
			if (buffer_pos == header.dict_size) {
				buffer_pos = 0;
				global_pos += header.dict_size;
				write(dst_fd, buffer, header.dict_size);
			}
			if (state < 4)
				state = 0;
			else if (state < 10)
				state -= 3;
			else
				state -= 6;
		} else {
			int offset;
			uint16_t *prob_len;

			rc_update_bit_1(&rc, prob);
			prob = p + LZMA_IS_REP + state;
			if (rc_is_bit_0(&rc, prob)) {
				rc_update_bit_0(&rc, prob);
				rep3 = rep2;
				rep2 = rep1;
				rep1 = rep0;
				state = state < LZMA_NUM_LIT_STATES ? 0 : 3;
				prob = p + LZMA_LEN_CODER;
			} else {
				rc_update_bit_1(&rc, prob);
				prob = p + LZMA_IS_REP_G0 + state;
				if (rc_is_bit_0(&rc, prob)) {
					rc_update_bit_0(&rc, prob);
					prob = (p + LZMA_IS_REP_0_LONG
							+ (state << LZMA_NUM_POS_BITS_MAX) + pos_state);
					if (rc_is_bit_0(&rc, prob)) {
						rc_update_bit_0(&rc, prob);

						state = state < LZMA_NUM_LIT_STATES ? 9 : 11;
						pos = buffer_pos - rep0;
						while (pos >= header.dict_size)
							pos += header.dict_size;
						previous_byte = buffer[pos];
						buffer[buffer_pos++] = previous_byte;
						if (buffer_pos == header.dict_size) {
							buffer_pos = 0;
							global_pos += header.dict_size;
							write(dst_fd, buffer, header.dict_size);
						}
						continue;
					} else {
						rc_update_bit_1(&rc, prob);
					}
				} else {
					uint32_t distance;

					rc_update_bit_1(&rc, prob);
					prob = p + LZMA_IS_REP_G1 + state;
					if (rc_is_bit_0(&rc, prob)) {
						rc_update_bit_0(&rc, prob);
						distance = rep1;
					} else {
						rc_update_bit_1(&rc, prob);
						prob = p + LZMA_IS_REP_G2 + state;
						if (rc_is_bit_0(&rc, prob)) {
							rc_update_bit_0(&rc, prob);
							distance = rep2;
						} else {
							rc_update_bit_1(&rc, prob);
							distance = rep3;
							rep3 = rep2;
						}
						rep2 = rep1;
					}
					rep1 = rep0;
					rep0 = distance;
				}
				state = state < LZMA_NUM_LIT_STATES ? 8 : 11;
				prob = p + LZMA_REP_LEN_CODER;
			}

			prob_len = prob + LZMA_LEN_CHOICE;
			if (rc_is_bit_0(&rc, prob_len)) {
				rc_update_bit_0(&rc, prob_len);
				prob_len = (prob + LZMA_LEN_LOW
							+ (pos_state << LZMA_LEN_NUM_LOW_BITS));
				offset = 0;
				num_bits = LZMA_LEN_NUM_LOW_BITS;
			} else {
				rc_update_bit_1(&rc, prob_len);
				prob_len = prob + LZMA_LEN_CHOICE_2;
				if (rc_is_bit_0(&rc, prob_len)) {
					rc_update_bit_0(&rc, prob_len);
					prob_len = (prob + LZMA_LEN_MID
								+ (pos_state << LZMA_LEN_NUM_MID_BITS));
					offset = 1 << LZMA_LEN_NUM_LOW_BITS;
					num_bits = LZMA_LEN_NUM_MID_BITS;
				} else {
					rc_update_bit_1(&rc, prob_len);
					prob_len = prob + LZMA_LEN_HIGH;
					offset = ((1 << LZMA_LEN_NUM_LOW_BITS)
							  + (1 << LZMA_LEN_NUM_MID_BITS));
					num_bits = LZMA_LEN_NUM_HIGH_BITS;
				}
			}
			rc_bit_tree_decode(&rc, prob_len, num_bits, &len);
			len += offset;

			if (state < 4) {
				int pos_slot;

				state += LZMA_NUM_LIT_STATES;
				prob =
					p + LZMA_POS_SLOT +
					((len <
					  LZMA_NUM_LEN_TO_POS_STATES ? len :
					  LZMA_NUM_LEN_TO_POS_STATES - 1)
					 << LZMA_NUM_POS_SLOT_BITS);
				rc_bit_tree_decode(&rc, prob, LZMA_NUM_POS_SLOT_BITS,
								   &pos_slot);
				if (pos_slot >= LZMA_START_POS_MODEL_INDEX) {
					num_bits = (pos_slot >> 1) - 1;
					rep0 = 2 | (pos_slot & 1);
					if (pos_slot < LZMA_END_POS_MODEL_INDEX) {
						rep0 <<= num_bits;
						prob = p + LZMA_SPEC_POS + rep0 - pos_slot - 1;
					} else {
						num_bits -= LZMA_NUM_ALIGN_BITS;
						while (num_bits--)
							rep0 = (rep0 << 1) | rc_direct_bit(&rc);
						prob = p + LZMA_ALIGN;
						rep0 <<= LZMA_NUM_ALIGN_BITS;
						num_bits = LZMA_NUM_ALIGN_BITS;
					}
					i = 1;
					mi = 1;
					while (num_bits--) {
						if (rc_get_bit(&rc, prob + mi, &mi))
							rep0 |= i;
						i <<= 1;
					}
				} else
					rep0 = pos_slot;
				if (++rep0 == 0)
					break;
			}

			len += LZMA_MATCH_MIN_LEN;

			do {
				pos = buffer_pos - rep0;
				while (pos >= header.dict_size)
					pos += header.dict_size;
				previous_byte = buffer[pos];
				buffer[buffer_pos++] = previous_byte;
				if (buffer_pos == header.dict_size) {
					buffer_pos = 0;
					global_pos += header.dict_size;
					write(dst_fd, buffer, header.dict_size);
				}
				len--;
			} while (len != 0 && buffer_pos < header.dst_size);
		}
	}

	write(dst_fd, buffer, buffer_pos);
	rc_free(&rc);
	return 0;
}

/* vi:set ts=4: */
