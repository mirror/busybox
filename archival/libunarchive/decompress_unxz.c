/*
 * This file uses XZ Embedded library code which is written
 * by Lasse Collin <lasse.collin@tukaani.org>
 * and Igor Pavlov <http://7-zip.org/>
 *
 * See README file in unxz/ directory for more information.
 *
 * This file is:
 * Copyright (C) 2010 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include "unarchive.h"

//#define XZ_DEBUG_MSG(...) bb_error_msg(__VA_ARGS)
#define XZ_REALLOC_DICT_BUF(ptr, size) xrealloc(ptr, size)
#define XZ_FUNC FAST_FUNC
#define XZ_EXTERN static

#define xz_crc32_init(table) crc32_filltable(table, /*endian:*/ 0)
static uint32_t xz_crc32(uint32_t *crc32_table,
		const uint8_t *buf, size_t size, uint32_t crc)
{
	crc = ~crc;

	while (size != 0) {
		crc = crc32_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
#define xz_crc32 xz_crc32

#define get_unaligned_le32(buf) ({ uint32_t v; move_from_unaligned32(v, buf); SWAP_LE32(v); })
#define get_unaligned_be32(buf) ({ uint32_t v; move_from_unaligned32(v, buf); SWAP_BE32(v); })
#define put_unaligned_le32(val, buf) move_to_unaligned16(buf, SWAP_LE32(val))
#define put_unaligned_be32(val, buf) move_to_unaligned16(buf, SWAP_BE32(val))

#include "unxz/xz.h"
#include "unxz/xz_config.h"

#include "unxz/xz_dec_bcj.c"
#include "unxz/xz_dec_lzma2.c"
#include "unxz/xz_dec_stream.c"
#include "unxz/xz_lzma2.h"
#include "unxz/xz_private.h"
#include "unxz/xz_stream.h"

IF_DESKTOP(long long) int FAST_FUNC
unpack_xz_stream(int src_fd, int dst_fd)
{
	struct xz_buf iobuf;
	struct xz_dec *state;
	unsigned char *membuf;
	IF_DESKTOP(long long) int total = 0;
	enum {
		IN_SIZE = 4 * 1024,
		OUT_SIZE = 60 * 1024,
	};

	membuf = xmalloc(IN_SIZE + OUT_SIZE);
	memset(&iobuf, 0, sizeof(iobuf));
	iobuf.in = membuf;
	iobuf.out = membuf + IN_SIZE;
	iobuf.out_size = OUT_SIZE;

	state = xz_dec_init(64*1024); /* initial dict of 64k */
	xz_crc32_init(state->crc32_table);

	while (1) {
		enum xz_ret r;
                int insz, rd, outpos;

		iobuf.in_size -= iobuf.in_pos;
		insz = iobuf.in_size;
		if (insz)
			memmove(membuf, membuf + iobuf.in_pos, insz);
		iobuf.in_pos = 0;
		rd = IN_SIZE - insz;
		if (rd) {
			rd = safe_read(src_fd, membuf + insz, rd);
			if (rd < 0) {
				bb_error_msg("read error");
				total = -1;
				break;
			}
			iobuf.in_size = insz + rd;
		}
//		bb_error_msg(">in pos:%d size:%d out pos:%d size:%d",
//				iobuf.in_pos, iobuf.in_size, iobuf.out_pos, iobuf.out_size);
		r = xz_dec_run(state, &iobuf);
//		bb_error_msg("<in pos:%d size:%d out pos:%d size:%d r:%d",
//				iobuf.in_pos, iobuf.in_size, iobuf.out_pos, iobuf.out_size, r);
		outpos = iobuf.out_pos;
		if (outpos) {
			xwrite(dst_fd, iobuf.out, outpos);
			IF_DESKTOP(total += outpos;)
		}
		if (r == XZ_STREAM_END
		/* this happens even with well-formed files: */
		 || (r == XZ_BUF_ERROR && insz == 0 && outpos == 0)
		) {
			break;
		}
		if (r != XZ_OK) {
			bb_error_msg("corrupted data");
			total = -1;
			break;
		}
		iobuf.out_pos = 0;
	}
	xz_dec_end(state);
	free(membuf);

	return total;
}
