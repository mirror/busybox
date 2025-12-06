/*-
 * Copyright 2013-2018 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if RESTRICTED_PARAMS

#define decode64_uint32(dst, src, min) \
({ \
	uint32_t d32 = a2i64(*(src)); \
	if (d32 > 47) \
		goto fail; \
	*(dst) = d32 + (min); \
	++src; \
})
#define test_decode64_uint32() ((void)0)
#define FULL_PARAMS(...)

#else

#define FULL_PARAMS(...) __VA_ARGS__

/* Not inlining:
 * de/encode64 functions are only used to read
 * yescrypt_params_t field, and convert salt to binary -
 * both of these are negligible compared to main hashing operation
 */
static NOINLINE const uint8_t *decode64_uint32(
		uint32_t *dst,
		const uint8_t *src, uint32_t val)
{
	uint32_t start = 0, end = 47, bits = 0;
	uint32_t c;

	if (!src) /* previous decode failed already? */
		goto fail;

	c = a2i64(*src++);
	if (c > 63)
		goto fail;

// The encoding of number N:
// start = 0 end = 47
// If N < 48, it is encoded verbatim, else
// N -= 48
// start = end+1 = 48
// end += (64-end)/2 = 55
// If N < (end+1-start)<<6 = 8<<6, it is encoded as 48+(N>>6)|low6bits (that is, 48...55|<6bit>), else
// N -= 8<<6
// start = end+1 = 56
// end += (64-end)/2 = 59
// If N < (end+1-start)<<2*6 = 4<<12, it is encoded as 56+(N>>2*6)|low12bits (that is, 56...59|<6bit>|<6bit>), else
// ...same for 60..61|<6bit>|<6bit>|<6bit>
// .......same for 62|<6bit>|<6bit>|<6bit>|<6bit>
// .......same for 63|<6bit>|<6bit>|<6bit>|<6bit>|<6bit>
	dbg_dec64("c:%d val:0x%08x", (int)c, (unsigned)val);
	while (c > end) {
		dbg_dec64("c:%d > end:%d", (int)c, (int)end);
		val += (end + 1 - start) << bits;
		dbg_dec64("val+=0x%08x", (int)((end + 1 - start) << bits));
		dbg_dec64(" val:0x%08x", (unsigned)val);
		start = end + 1;
		end += (64 - end) / 2;
		bits += 6;
		dbg_dec64("start=%d", (int)start);
		dbg_dec64("end=%d", (int)end);
		dbg_dec64("bits=%d", (int)bits);
	}

	val += (c - start) << bits;
	dbg_dec64("final val+=0x%08x", (int)((c - start) << bits));
	dbg_dec64("       val:0x%08x", (unsigned)val);

	while (bits != 0) {
		c = a2i64(*src++);
		if (c > 63)
			goto fail;
		bits -= 6;
		val += c << bits;
		dbg_dec64("low bits val+=0x%08x", (int)(c << bits));
		dbg_dec64("          val:0x%08x", (unsigned)val);
	}
 ret:
	*dst = val;
	return src;
 fail:
	val = 0;
	src = NULL;
	goto ret;
}

#if TEST_DECODE64
static void test_decode64_uint32(void)
{
	const uint8_t *src, *end;
	uint32_t u32;
	int a = 48;
	int b = 8<<6;  // 0x0200
	int c = 4<<12; // 0x04000
	int d = 2<<18; // 0x080000
	int e = 1<<24; // 0x1000000

	src = (void*)"wzzz";
	end = decode64_uint32(&u32, src, 0);
	if (u32 != 0x0003ffff+c+b+a) bb_error_msg_and_die("Incorrect decode '%s':0x%08x", src, (unsigned)u32);
	if (end != src + 4) bb_error_msg_and_die("Incorrect decode '%s': %p end:%p", src, src, end);
	src = (void*)"xzzz";
	end = decode64_uint32(&u32, src, 0);
	if (u32 != 0x0007ffff+c+b+a) bb_error_msg_and_die("Incorrect decode '%s':0x%08x", src, (unsigned)u32);
	if (end != src + 4) bb_error_msg_and_die("Incorrect decode '%s': %p end:%p", src, src, end);
	// Note how the last representable "x---" encoding, 0x7ffff, is exactly d-1!
	// And if we now increment it, we get:
	src = (void*)"y....";
	end = decode64_uint32(&u32, src, 0);
	if (u32 != 0x00000000+d+c+b+a) bb_error_msg_and_die("Incorrect decode '%s':0x%08x", src, (unsigned)u32);
	if (end != src + 5) bb_error_msg_and_die("Incorrect decode '%s': %p end:%p", src, src, end);
	src = (void*)"yzzzz";
	end = decode64_uint32(&u32, src, 0);
	if (u32 != 0x00ffffff+d+c+b+a) bb_error_msg_and_die("Incorrect decode '%s':0x%08x", src, (unsigned)u32);
	if (end != src + 5) bb_error_msg_and_die("Incorrect decode '%s': %p end:%p", src, src, end);

	src = (void*)"zzzzzz";
	end = decode64_uint32(&u32, src, 0);
	if (u32 != 0x3fffffff+e+d+c+b+a) bb_error_msg_and_die("Incorrect decode '%s':0x%08x", src, (unsigned)u32);
	if (end != src + 6) bb_error_msg_and_die("Incorrect decode '%s': %p end:%p", src, src, end);

	bb_error_msg("test_decode64_uint32() OK");
}
#else
# define test_decode64_uint32() ((void)0)
#endif

#endif /* !RESTRICTED_PARAMS */

#if 1
static const uint8_t *decode64(
		uint8_t *dst, size_t *dstlen,
		const uint8_t *src)
{
	unsigned dstpos = 0;

	dbg_dec64("src:'%s'", src);
	for (;;) {
		uint32_t c, value = 0;
		int bits = 0;
		while (*src != '\0' && *src != '$') {
			c = a2i64(*src);
			if (c > 63) { /* bad ascii64 char, stop decoding at it */
				break;
			}
			src++;
			value |= c << bits;
			bits += 6;
			if (bits == 24) /* got 4 chars */
				goto store;
		}
		/* we read entire src, or met a non-ascii64 char (such as "$") */
		if (bits == 0)
			break;
		/* else: we got last, partial bit block - store it */
 store:
		dbg_dec64(" storing bits:%d dstpos:%u v:%08x", bits, dstpos, (int)SWAP_BE32(value)); //BE to see lsb first
		for (;;) {
			if ((*src == '\0' || *src == '$')
			 && value == 0 && bits < 8
			) {
				/* Example: mkpasswd PWD '$y$j9T$123':
				 * the "123" is bits:18 value:03,51,00
				 * is considered to be 2 bytes, not 3!
				 *
				 * '$y$j9T$zzz' in upstream fails outright (3rd byte isn't zero).
				 * IOW: for upstream, validity of salt depends on VALUE,
				 * not just size of salt. Which is a bug.
				 * The '$y$j9T$zzz.' salt is the same
				 * (it adds 6 zero msbits) but upstream works with it,
				 * thus '$y$j9T$zzz' should work too and give the same result.
				 */
				goto end;
			}
			if (dstpos >= *dstlen) {
				dbg_dec64(" ERR: bits:%d dstpos:%u dst[] is too small", bits, dstpos);
				goto fail;
			}
			*dst++ = value;
			dstpos++;
			value >>= 8;
			bits -= 8;
			if (bits <= 0) /* can get negative, if we e.g. had 6 bits */
				break;
		}
		if (*src == '\0' || *src == '$')
			break;
	}
 end:
	*dstlen = dstpos;
	dbg_dec64("dec64: OK, dst[%d]", (int)dstpos);
	return src;
 fail:
	/* *dstlen = 0; - not needed, caller detects error by seeing NULL */
	return NULL;
}
#else
/* Buggy (and larger) original code */
static const uint8_t *decode64(
		uint8_t *dst, size_t *dstlen,
		const uint8_t *src, size_t srclen)
{
	size_t dstpos = 0;

	while (dstpos <= *dstlen && srclen) {
		uint32_t value = 0, bits = 0;
		while (srclen--) {
			uint32_t c = a2i64(*src);
			if (c > 63) {
				srclen = 0;
				break;
			}
			src++;
			value |= c << bits;
			bits += 6;
			if (bits >= 24)
				break;
		}
		if (!bits)
			break;
		if (bits < 12) /* must have at least one full byte */
			goto fail;
		dbg_dec64(" storing bits:%d v:%08x", (int)bits, (int)SWAP_BE32(value)); //BE to see lsb first
		while (dstpos++ < *dstlen) {
			*dst++ = value;
			value >>= 8;
			bits -= 8;
			if (bits < 8) { /* 2 or 4 */
				if (value) /* must be 0 */
					goto fail;
				bits = 0;
				break;
			}
		}
		if (bits)
			goto fail;
	}

	if (!srclen && dstpos <= *dstlen) {
		*dstlen = dstpos;
		dbg_dec64("dec64: OK, dst[%d]", (int)dstpos);
		return src;
	}
 fail:
	/* *dstlen = 0; - not needed, caller detects error by seeing NULL */
	return NULL;
}
#endif

static char *encode64(
		char *dst, size_t dstlen,
		const uint8_t *src, size_t srclen)
{
	while (srclen) {
		uint32_t value = 0, b = 0;
		do {
			value |= (uint32_t)(*src++ << b);
			b += 8;
			srclen--;
		} while (srclen && b < 24);

		b >>= 3; /* number of bits to number of bytes */
		b++; /* 1, 2 or 3 bytes will become 2, 3 or 4 ascii64 chars */
		dstlen -= b;
		if ((ssize_t)dstlen <= 0)
			return NULL;
		dst = num2str64_lsb_first(dst, value, b);
	}
	*dst = '\0';
	return dst;
}

char *yescrypt_r(
		const uint8_t *passwd, size_t passwdlen,
		const uint8_t *setting,
		char *buf, size_t buflen)
{
	struct {
		yescrypt_ctx_t yctx[1];
		unsigned char hashbin32[32];
	} u;
#define yctx      u.yctx
#define hashbin32 u.hashbin32
	char *dst;
	const uint8_t *src, *saltend;
	size_t need, prefixlen;
	uint32_t u32;

	test_decode64_uint32();

	memset(yctx, 0, sizeof(yctx));
	FULL_PARAMS(yctx->param.p = 1;)

	/* we assume setting starts with "$y$" (caller must ensure this) */
	src = setting + 3;

	src = decode64_uint32(&yctx->param.flags, src, 0);
	/* "j9T" returns: 0x2f */
	//if (!src)
	//	goto fail;

	if (yctx->param.flags < YESCRYPT_RW) {
		dbg("yctx->param.flags=0x%x", (unsigned)yctx->param.flags);
		goto fail; // bbox: we don't support scrypt - only yescrypt
	} else if (yctx->param.flags <= YESCRYPT_RW + (YESCRYPT_RW_FLAVOR_MASK >> 2)) {
		/* "j9T" sets flags to 0xb6 */
		yctx->param.flags = YESCRYPT_RW + ((yctx->param.flags - YESCRYPT_RW) << 2);
		dbg("yctx->param.flags=0x%x", (unsigned)yctx->param.flags);
		dbg(" YESCRYPT_RW:%u", !!(yctx->param.flags & YESCRYPT_RW));
                dbg((yctx->param.flags & YESCRYPT_RW_FLAVOR_MASK) ==
                       (YESCRYPT_ROUNDS_6 | YESCRYPT_GATHER_4 | YESCRYPT_SIMPLE_2 | YESCRYPT_SBOX_12K)
		    ? " YESCRYPT_ROUNDS_6 | YESCRYPT_GATHER_4 | YESCRYPT_SIMPLE_2 | YESCRYPT_SBOX_12K"
		    : " flags are not standard"
		);
	} else {
		goto fail;
	}

	src = decode64_uint32(&u32, src, 1);
	if (/*!src ||*/ u32 > 63)
		goto fail;
	yctx->param.N = (uint64_t)1 << u32;
	/* "j9T" sets to 4096 (1<<12) */
	dbg("yctx->param.N=%llu (1<<%u)", (unsigned long long)yctx->param.N, (unsigned)u32);

	src = decode64_uint32(&yctx->param.r, src, 1);
	/* "j9T" sets to 32 */
	dbg("yctx->param.r=%u", yctx->param.r);

	if (!src)
		goto fail;
	if (*src != '$') {
#if RESTRICTED_PARAMS
		goto fail;
#else
		src = decode64_uint32(&u32, src, 1);
		dbg("yescrypt has extended params:0x%x", (unsigned)u32);
		if (u32 & 1)
			src = decode64_uint32(&yctx->param.p, src, 2);
		if (u32 & 2)
			src = decode64_uint32(&yctx->param.t, src, 1);
		if (u32 & 4)
			src = decode64_uint32(&yctx->param.g, src, 1);
		if (u32 & 8) {
			src = decode64_uint32(&u32, src, 1);
			if (/*!src ||*/ u32 > 63)
				goto fail;
			yctx->param.NROM = (uint64_t)1 << u32;
		}
		if (!src)
			goto fail;
		if (*src != '$')
			goto fail;
#endif
	}

	yctx->saltlen = sizeof(yctx->salt);
	src++; /* now points to salt */
	saltend = decode64(yctx->salt, &yctx->saltlen, src);
	if (!saltend || (*saltend != '\0' && *saltend != '$'))
		goto fail; /* salt[] is too small, or bad char during decode */
	dbg_dec64("salt is %d ascii64 chars -> %d bytes (in binary)", (int)(saltend - src), (int)yctx->saltlen);

	prefixlen = saltend - setting;
	need = prefixlen + 1 + YESCRYPT_HASH_LEN + 1;
	if (need > buflen /*overflow is quite unlikely: || need < prefixlen*/)
		goto fail;

	if (yescrypt_kdf32(yctx, passwd, passwdlen, hashbin32)) {
		dbg("error in yescrypt_kdf32");
		goto fail;
	}

	dst = mempcpy(buf, setting, prefixlen);
	*dst++ = '$';
	dst = encode64(dst, buflen - (dst - buf), hashbin32, sizeof(hashbin32));
	if (!dst)
		goto fail;
 ret:
	free_region(yctx->local);
	explicit_bzero(&u, sizeof(u));
	return buf;
 fail:
	buf = NULL;
	goto ret;
#undef yctx
#undef hashbin32
}
