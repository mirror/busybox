/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright 2003, Glenn McGrath
 * Copyright 2010, Denys Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

//kbuild:lib-y += base64.o

void FAST_FUNC read_base64(FILE *src_stream, FILE *dst_stream, int flags)
{
/* Note that EOF _can_ be passed as exit_char too */
#define exit_char    ((int)(signed char)flags)
#define uu_style_end (flags & BASE64_FLAG_UUEND)

	int term_count = 0;

	while (1) {
		unsigned char translated[4];
		int count = 0;

		/* Process one group of 4 chars */
		while (count < 4) {
			char *table_ptr;
			int ch;

			/* Get next _valid_ character.
			 * bb_uuenc_tbl_base64[] contains this string:
			 *  0         1         2         3         4         5         6
			 *  012345678901234567890123456789012345678901234567890123456789012345
			 * "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=\n"
			 */
			do {
				ch = fgetc(src_stream);
				if (ch == exit_char && count == 0)
					return;
				if (ch == EOF)
					bb_error_msg_and_die("truncated base64 input");
				table_ptr = strchr(bb_uuenc_tbl_base64, ch);
//TODO: add BASE64_FLAG_foo to die on bad char?
//Note that then we may need to still allow '\r' (for mail processing)
			} while (!table_ptr);

			/* Convert encoded character to decimal */
			ch = table_ptr - bb_uuenc_tbl_base64;

			if (ch == 65 /* '\n' */) {
				/* Terminating "====" line? */
				if (uu_style_end && term_count == 4)
					return; /* yes */
				continue;
			}
			/* ch is 64 if char was '=', otherwise 0..63 */
			translated[count] = ch & 63; /* 64 -> 0 */
			if (ch == 64) {
				term_count++;
				break;
			}
			term_count = 0;
			count++;
		}

		/* Merge 6 bit chars to 8 bit.
		 * count can be < 4 when we decode the tail:
		 * "eQ==" -> "y", not "y NUL NUL"
		 */
		if (count > 1)
			fputc(translated[0] << 2 | translated[1] >> 4, dst_stream);
		if (count > 2)
			fputc(translated[1] << 4 | translated[2] >> 2, dst_stream);
		if (count > 3)
			fputc(translated[2] << 6 | translated[3], dst_stream);
	} /* while (1) */
}
