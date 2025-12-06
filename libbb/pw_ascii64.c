/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* Returns >=64 for invalid chars */
int FAST_FUNC a2i64(char c)
{
	unsigned char ch = c;
	if (ch >= 'a')
		/* "a..z" to 38..63 */
		/* anything after "z": positive int >= 64 */
		return (ch - 'a' + 38);

	if (ch > 'Z')
		/* after "Z" but before "a": positive byte >= 64 */
		return ch;

	if (ch >= 'A')
		/* "A..Z" to 12..37 */
		return (ch - 'A' + 12);

	if (ch > '9')
		return 64;

	/* "./0123456789" to 0,1,2..11 */
	/* anything before "." becomes positive byte >= 64 */
	return (unsigned char)(ch - '.');
}

/* 0..63 ->
 * "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
 */
int FAST_FUNC i2a64(int i)
{
	i &= 0x3f;

	i += '.';
	/* the above maps 0..11 to "./0123456789":
	 * ACSII codes of "./" are ('0'-2) and ('0'-1) */

	if (i > '9')
		i += ('A' - '9' - 1);
	if (i > 'Z')
		i += ('a' - 'Z' - 1);
	return i;
}

char* FAST_FUNC
num2str64_lsb_first(char *s, unsigned v, int n)
{
	while (--n >= 0) {
		*s++ = i2a64(v);
		v >>= 6;
	}
	return s;
}

static void
num2str64_4chars_msb_first(char *s, unsigned v)
{
	*s++ = i2a64(v >> 18); /* bits 23..18 */
	*s++ = i2a64(v >> 12); /* bits 17..12 */
	*s++ = i2a64(v >> 6); /* bits 11..6 */
	*s   = i2a64(v); /* bits 5..0 */
}

int FAST_FUNC crypt_make_rand64encoded(char *p, int cnt /*, int x */)
{
	/* was: x += ... */
	unsigned x = getpid() + monotonic_us();
	do {
		/* x = (x*1664525 + 1013904223) % 2^32 generator is lame
		 * (low-order bit is not "random", etc...),
		 * but for our purposes it is good enough */
		x = x*1664525 + 1013904223;
		/* BTW, Park and Miller's "minimal standard generator" is
		 * x = x*16807 % ((2^31)-1)
		 * It has no problem with visibly alternating lowest bit
		 * but is also weak in cryptographic sense + needs div,
		 * which needs more code (and slower) on many CPUs */
		*p++ = i2a64(x >> 16);
		*p++ = i2a64(x >> 22);
	} while (--cnt);
	*p = '\0';
	return x;
}
