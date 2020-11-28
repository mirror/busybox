/* vi: set sw=4 ts=4: */
/*
 * Copyright 2003, Glenn McGrath
 * Copyright 2006, Rob Landley <rob@landley.net>
 * Copyright 2010, Denys Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* Conversion tables */
/* for base 64 */
const char bb_uuenc_tbl_base64[65 + 1] ALIGN1 = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
	'=' /* termination character */,
	'\0' /* needed for uudecode.c only */
};
#if ENABLE_BASE32
const char bb_uuenc_tbl_base32[33 + 1] ALIGN1 = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', '2', '3', '4', '5', '6', '7',
	'=',
	'\0'
};
#endif
const char bb_uuenc_tbl_std[65] ALIGN1 = {
	'`', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`' /* termination character */
};

/*
 * Encode bytes at S of length LENGTH to uuencode or base64 format and place it
 * to STORE.  STORE will be 0-terminated, and must point to a writable
 * buffer of at least 1+BASE64_LENGTH(length) bytes.
 * where BASE64_LENGTH(len) = (4 * ((LENGTH + 2) / 3))
 */
void FAST_FUNC bb_uuencode(char *p, const void *src, int length, const char *tbl)
{
	const unsigned char *s = src;

	/* Transform the 3x8 bits to 4x6 bits */
	while (length > 0) {
		unsigned s1, s2;

		/* Are s[1], s[2] valid or should be assumed 0? */
		s1 = s2 = 0;
		length -= 3; /* can be >=0, -1, -2 */
		if (length >= -1) {
			s1 = s[1];
			if (length >= 0)
				s2 = s[2];
		}
		*p++ = tbl[s[0] >> 2];
		*p++ = tbl[((s[0] & 3) << 4) + (s1 >> 4)];
		*p++ = tbl[((s1 & 0xf) << 2) + (s2 >> 6)];
		*p++ = tbl[s2 & 0x3f];
		s += 3;
	}
	/* Zero-terminate */
	*p = '\0';
	/* If length is -2 or -1, pad last char or two */
	while (length) {
		*--p = tbl[64];
		length++;
	}
}

/*
 * Decode base64 encoded string.
 *
 * Returns: pointer to the undecoded part of source.
 * If points to '\0', then the source was fully decoded.
 * (*pp_dst): advanced past the last written byte.
 */
const char* FAST_FUNC decode_base64(char **pp_dst, const char *src)
{
	char *dst = *pp_dst;
	unsigned ch = 0;
	int i = 0;
	int t;

	while ((t = (unsigned char)*src) != '\0') {
		src++;

		/* "if" forest is faster than strchr(bb_uuenc_tbl_base64, t) */
		if (t >= '0' && t <= '9')
			t = t - '0' + 52;
		else if (t >= 'A' && t <= 'Z')
			t = t - 'A';
		else if (t >= 'a' && t <= 'z')
			t = t - 'a' + 26;
		else if (t == '+')
			t = 62;
		else if (t == '/')
			t = 63;
		else if (t == '=' && (i == 3 || (i == 2 && *src == '=')))
			/* the above disallows "==AA", "A===", "AA=A" etc */
			t = 0x1000000;
		else
//TODO: add BASE64_FLAG_foo to die on bad char?
			continue;

		ch = (ch << 6) | t;
		if (++i == 4) {
			*dst++ = (char) (ch >> 16);
			*dst++ = (char) (ch >> 8);
			*dst++ = (char) ch;
			i = 0;
			if (ch & 0x1000000) { /* was last input char '='? */
				dst--;
				if (ch & (0x1000000 << 6)) /* was it "=="? */
					dst--;
				break;
			}
			ch = 0;
		}
	}
	*pp_dst = dst;
	/* i is zero here if full 4-char block was decoded */
	return src - i; /* -i rejects truncations: e.g. "MQ" and "MQ=" (correct encoding is "MQ==" -> "1") */
}

#if ENABLE_BASE32
const char* FAST_FUNC decode_base32(char **pp_dst, const char *src)
{
	char *dst = *pp_dst;
	uint64_t ch = 0;
	int i = 0;
	int t;

	while ((t = (unsigned char)*src) != '\0') {
		src++;

		/* "if" forest is faster than strchr(bb_uuenc_tbl_base32, t) */
		if (t >= '2' && t <= '7')
			t = t - '2' + 26;
		else if ((t|0x20) >= 'a' && (t|0x20) <= 'z')
			t = (t|0x20) - 'a';
		else if (t == '=' && i > 1)
			t = 0;
		else
//TODO: add BASE64_FLAG_foo to die on bad char?
			continue;

		ch = (ch << 5) | (unsigned)t; /* cast prevents pointless sign-extension of t */
		if (++i == 8) {
			/* testcase:
			 * echo ' 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18' | base32 | base32 -d
			 * IOW, decoding of
			 * EAYSAMRAGMQDIIBVEA3CANZAHAQDSIBRGAQDCMJAGEZCAMJTEAYTIIBRGUQDCNRAGE3SAMJYBI==
			 * ====
			 * should correctly stitch together the tail.
			 */
			if (t == 0) {
				const char *s = src;
				while (*--s == '=')
					t--;
			}
			*dst++ = (char) (ch >> 32);
			*dst++ = (char) (ch >> 24);
			*dst++ = (char) (ch >> 16);
			*dst++ = (char) (ch >> 8);
			*dst++ = (char) ch;
			i = 0;
			if (t < 0) /* was last input char '='? */
				break;
		}
	}
	if (t < 0) /* was last input char '='? */
		/* -t is the count of =, must be 1, 3, 4 or 6 */
		dst -= (-t + 1) * 2 / 3; /* discard last 1, 2, 3 or 4 bytes */
	*pp_dst = dst;
	/* i is zero here if full 8-char block was decoded */
	return src - i;
}
#endif

/*
 * Decode base64 encoded stream.
 * Can stop on EOF, specified char, or on uuencode-style "====" line:
 * flags argument controls it.
 */
void FAST_FUNC read_base64(FILE *src_stream, FILE *dst_stream, int flags)
{
/* Note that EOF _can_ be passed as exit_char too */
#define exit_char    ((int)(signed char)flags)
#define uu_style_end (flags & BASE64_FLAG_UU_STOP)
#define base32       (flags & BASE64_32)

	/* uuencoded files have 61 byte lines.
	 * base32/64 have 76 byte lines by default.
	 * Use 80 byte buffer to process one line at a time.
	 */
	enum { BUFFER_SIZE = 80 };
	/* decoded data is shorter than input, can use single buffer for both */
	char buf[BUFFER_SIZE + 2];
	int term_seen = 0;
	int in_count = 0;

	while (1) {
		char *out_tail;
		const char *in_tail;

		while (in_count < BUFFER_SIZE) {
			int ch = fgetc(src_stream);
			if (ch == exit_char) {
				if (in_count == 0)
					return;
				term_seen = 1;
				break;
			}
			if (ch == EOF) {
				term_seen = 1;
				break;
			}
			/* Prevent "====" line to be split: stop if we see '\n'.
			 * We can also skip other whitespace and skirt the problem
			 * of files with NULs by stopping on any control char or space:
			 */
			if (ch <= ' ')
				break;
			buf[in_count++] = ch;
		}
		buf[in_count] = '\0';

		/* Did we encounter "====" line? */
		if (uu_style_end && strcmp(buf, "====") == 0)
			return;

		out_tail = buf;
#if ENABLE_BASE32
		if (base32)
			in_tail = decode_base32(&out_tail, buf);
		else
#endif
			in_tail = decode_base64(&out_tail, buf);

		fwrite(buf, (out_tail - buf), 1, dst_stream);

		if (term_seen) {
			/* Did we consume ALL characters? */
			if (*in_tail == '\0')
				return;
			/* No */
			bb_simple_error_msg_and_die("truncated base64 input");
		}

		/* It was partial decode */
		in_count = strlen(in_tail);
		memmove(buf, in_tail, in_count);
	}
}
