/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config TLS
//config:	bool "tls (debugging)"
//config:	default n

//applet:IF_TLS(APPLET(tls, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TLS) += tls.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_montgomery_reduce.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_mul_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_pstm_sqr_comba.o
//kbuild:lib-$(CONFIG_TLS) += tls_rsa.o
//kbuild:lib-$(CONFIG_TLS) += tls_aes.o
////kbuild:lib-$(CONFIG_TLS) += tls_aes_gcm.o

//usage:#define tls_trivial_usage
//usage:       "HOST[:PORT]"
//usage:#define tls_full_usage "\n\n"

#include "tls.h"
#include "common_bufsiz.h"

#define TLS_DEBUG      1
#define TLS_DEBUG_HASH 1
#define TLS_DEBUG_DER  0

#if TLS_DEBUG
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

#if TLS_DEBUG_DER
# define dbg_der(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg_der(...) ((void)0)
#endif

#define RECORD_TYPE_CHANGE_CIPHER_SPEC  20
#define RECORD_TYPE_ALERT               21
#define RECORD_TYPE_HANDSHAKE           22
#define RECORD_TYPE_APPLICATION_DATA    23

#define HANDSHAKE_HELLO_REQUEST         0
#define HANDSHAKE_CLIENT_HELLO          1
#define HANDSHAKE_SERVER_HELLO          2
#define HANDSHAKE_HELLO_VERIFY_REQUEST  3
#define HANDSHAKE_NEW_SESSION_TICKET    4
#define HANDSHAKE_CERTIFICATE           11
#define HANDSHAKE_SERVER_KEY_EXCHANGE   12
#define HANDSHAKE_CERTIFICATE_REQUEST   13
#define HANDSHAKE_SERVER_HELLO_DONE     14
#define HANDSHAKE_CERTIFICATE_VERIFY    15
#define HANDSHAKE_CLIENT_KEY_EXCHANGE   16
#define HANDSHAKE_FINISHED              20

#define SSL_HS_RANDOM_SIZE              32
#define SSL_HS_RSA_PREMASTER_SIZE       48

#define SSL_NULL_WITH_NULL_NULL                 0x0000
#define SSL_RSA_WITH_NULL_MD5                   0x0001
#define SSL_RSA_WITH_NULL_SHA                   0x0002
#define SSL_RSA_WITH_RC4_128_MD5                0x0004
#define SSL_RSA_WITH_RC4_128_SHA                0x0005
#define SSL_RSA_WITH_3DES_EDE_CBC_SHA           0x000A  /* 10 */
#define TLS_RSA_WITH_AES_128_CBC_SHA            0x002F  /* 47 */
#define TLS_RSA_WITH_AES_256_CBC_SHA            0x0035  /* 53 */
#define TLS_RSA_WITH_NULL_SHA256                0x003B  /* 59 */

#define TLS_EMPTY_RENEGOTIATION_INFO_SCSV       0x00FF

#define TLS_RSA_WITH_IDEA_CBC_SHA               0x0007  /* 7 */
#define SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA       0x0016  /* 22 */
#define SSL_DH_anon_WITH_RC4_128_MD5            0x0018  /* 24 */
#define SSL_DH_anon_WITH_3DES_EDE_CBC_SHA       0x001B  /* 27 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA        0x0033  /* 51 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA        0x0039  /* 57 */
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256     0x0067  /* 103 */
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256     0x006B  /* 107 */
#define TLS_DH_anon_WITH_AES_128_CBC_SHA        0x0034  /* 52 */
#define TLS_DH_anon_WITH_AES_256_CBC_SHA        0x003A  /* 58 */
#define TLS_RSA_WITH_AES_128_CBC_SHA256         0x003C  /* 60 */
#define TLS_RSA_WITH_AES_256_CBC_SHA256         0x003D  /* 61 */
#define TLS_RSA_WITH_SEED_CBC_SHA               0x0096  /* 150 */
#define TLS_PSK_WITH_AES_128_CBC_SHA            0x008C  /* 140 */
#define TLS_PSK_WITH_AES_128_CBC_SHA256         0x00AE  /* 174 */
#define TLS_PSK_WITH_AES_256_CBC_SHA384         0x00AF  /* 175 */
#define TLS_PSK_WITH_AES_256_CBC_SHA            0x008D  /* 141 */
#define TLS_DHE_PSK_WITH_AES_128_CBC_SHA        0x0090  /* 144 */
#define TLS_DHE_PSK_WITH_AES_256_CBC_SHA        0x0091  /* 145 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA     0xC004  /* 49156 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA     0xC005  /* 49157 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA    0xC009  /* 49161 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA    0xC00A  /* 49162 */
#define TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA     0xC012  /* 49170 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA      0xC013  /* 49171 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA      0xC014  /* 49172 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA       0xC00E  /* 49166 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA       0xC00F  /* 49167 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 0xC023  /* 49187 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 0xC024  /* 49188 */
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256  0xC025  /* 49189 */
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384  0xC026  /* 49190 */
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256   0xC027  /* 49191 */
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384   0xC028  /* 49192 */
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256    0xC029  /* 49193 */
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384    0xC02A  /* 49194 */

// RFC 5288 "AES Galois Counter Mode (GCM) Cipher Suites for TLS"
#define TLS_RSA_WITH_AES_128_GCM_SHA256         0x009C  /* 156 */
#define TLS_RSA_WITH_AES_256_GCM_SHA384         0x009D  /* 157 */
#define TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 0xC02B  /* 49195 */
#define TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 0xC02C  /* 49196 */
#define TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256  0xC02D  /* 49197 */
#define TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384  0xC02E  /* 49198 */
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256   0xC02F  /* 49199 */
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384   0xC030  /* 49200 */
#define TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256    0xC031  /* 49201 */
#define TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384    0xC032  /* 49202 */

//Tested against kernel.org:
//TLS 1.1
//#define TLS_MAJ 3
//#define TLS_MIN 2
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA // ok, recvs SERVER_KEY_EXCHANGE
//TLS 1.2
#define TLS_MAJ 3
#define TLS_MIN 3
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA // ok, recvs SERVER_KEY_EXCHANGE *** matrixssl uses this on my box
//#define CIPHER_ID TLS_RSA_WITH_AES_256_CBC_SHA256 // ok, no SERVER_KEY_EXCHANGE
// All GCMs:
//#define CIPHER_ID TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 // ok, recvs SERVER_KEY_EXCHANGE
//#define CIPHER_ID TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
//#define CIPHER_ID TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384
//#define CIPHER_ID TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384
//#define CIPHER_ID TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256 // SSL_ALERT_HANDSHAKE_FAILURE
//#define CIPHER_ID TLS_RSA_WITH_AES_256_GCM_SHA384 // ok, no SERVER_KEY_EXCHANGE
//#define CIPHER_ID TLS_RSA_WITH_AES_128_GCM_SHA256 // ok, no SERVER_KEY_EXCHANGE *** select this?
//#define CIPHER_ID TLS_DH_anon_WITH_AES_256_CBC_SHA // SSL_ALERT_HANDSHAKE_FAILURE
//^^^^^^^^^^^^^^^^^^^^^^^ (tested b/c this one doesn't req server certs... no luck)
//test TLS_RSA_WITH_AES_128_CBC_SHA, in TLS 1.2 it's mandated to be always supported

// works against "openssl s_server -cipher NULL"
// and against wolfssl-3.9.10-stable/examples/server/server.c:
//#define CIPHER_ID TLS_RSA_WITH_NULL_SHA256 // for testing (does everything except encrypting)
// works against wolfssl-3.9.10-stable/examples/server/server.c
// (getting back and decrypt ok first application data message)
#define CIPHER_ID TLS_RSA_WITH_AES_256_CBC_SHA256 // ok, no SERVER_KEY_EXCHANGE

enum {
	SHA256_INSIZE = 64,
	SHA256_OUTSIZE = 32,

	AES_BLOCKSIZE = 16,
	AES128_KEYSIZE = 16,
	AES256_KEYSIZE = 32,

	MAX_TLS_RECORD = (1 << 14),
	OUTBUF_PFX = 8 + AES_BLOCKSIZE, /* header + IV */
	OUTBUF_SFX = SHA256_OUTSIZE + AES_BLOCKSIZE, /* MAC + padding */
	MAX_OTBUF = MAX_TLS_RECORD - OUTBUF_PFX - OUTBUF_SFX,
};

struct record_hdr {
	uint8_t type;
	uint8_t proto_maj, proto_min;
	uint8_t len16_hi, len16_lo;
};

typedef struct tls_state {
	int fd;

	psRsaKey_t server_rsa_pub_key;

	sha256_ctx_t handshake_sha256_ctx;

	uint8_t client_and_server_rand32[2 * 32];
	uint8_t master_secret[48];

	uint8_t encrypt_on_write;
	int     min_encrypted_len_on_read;
	uint8_t client_write_MAC_key[SHA256_OUTSIZE];
	uint8_t server_write_MAC_key[SHA256_OUTSIZE];
	uint8_t client_write_key[AES256_KEYSIZE];
	uint8_t server_write_key[AES256_KEYSIZE];
// RFC 5246
// sequence number
// Each connection state contains a sequence number, which is
// maintained separately for read and write states.  The sequence
// number MUST be set to zero whenever a connection state is made the
// active state.  Sequence numbers are of type uint64 and may not
// exceed 2^64-1.
	uint64_t write_seq64_be;

	int outbuf_size;
	uint8_t *outbuf;

	// RFC 5246
	// |6.2.1. Fragmentation
	// |  The record layer fragments information blocks into TLSPlaintext
	// |  records carrying data in chunks of 2^14 bytes or less.  Client
	// |  message boundaries are not preserved in the record layer (i.e.,
	// |  multiple client messages of the same ContentType MAY be coalesced
	// |  into a single TLSPlaintext record, or a single message MAY be
	// |  fragmented across several records)
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSPlaintext.fragment.
	// |    The length MUST NOT exceed 2^14.
	// |...
	// | 6.2.2. Record Compression and Decompression
	// |...
	// |  Compression must be lossless and may not increase the content length
	// |  by more than 1024 bytes.  If the decompression function encounters a
	// |  TLSCompressed.fragment that would decompress to a length in excess of
	// |  2^14 bytes, it MUST report a fatal decompression failure error.
	// |...
	// |  length
	// |    The length (in bytes) of the following TLSCompressed.fragment.
	// |    The length MUST NOT exceed 2^14 + 1024.
	//
	// Since our buffer also contains 5-byte headers, make it a bit bigger:
	int insize;
	int tail;
//needed?
	uint64_t align____;
	uint8_t inbuf[20*1024];
} tls_state_t;


static unsigned get24be(const uint8_t *p)
{
	return 0x100*(0x100*p[0] + p[1]) + p[2];
}

#if TLS_DEBUG
static void dump_hex(const char *fmt, const void *vp, int len)
{
	char hexbuf[32 * 1024 + 4];
	const uint8_t *p = vp;

	bin2hex(hexbuf, (void*)p, len)[0] = '\0';
	dbg(fmt, hexbuf);
}

static void dump_tls_record(const void *vp, int len)
{
	const uint8_t *p = vp;

	while (len > 0) {
		unsigned xhdr_len;
		if (len < 5) {
			dump_hex("< |%s|\n", p, len);
			return;
		}
		xhdr_len = 0x100*p[3] + p[4];
		dbg("< hdr_type:%u ver:%u.%u len:%u", p[0], p[1], p[2], xhdr_len);
		p += 5;
		len -= 5;
		if (len >= 4 && p[-5] == RECORD_TYPE_HANDSHAKE) {
			unsigned len24 = get24be(p + 1);
			dbg(" type:%u len24:%u", p[0], len24);
		}
		if (xhdr_len > len)
			xhdr_len = len;
		dump_hex(" |%s|\n", p, xhdr_len);
		p += xhdr_len;
		len -= xhdr_len;
	}
}
#endif

void tls_get_random(void *buf, unsigned len)
{
	if (len != open_read_close("/dev/urandom", buf, len))
		xfunc_die();
}

//TODO rename this to sha256_hash, and sha256_hash -> sha256_update
static void hash_sha256(uint8_t out[SHA256_OUTSIZE], const void *data, unsigned size)
{
	sha256_ctx_t ctx;
	sha256_begin(&ctx);
	sha256_hash(&ctx, data, size);
	sha256_end(&ctx, out);
}

/* Nondestructively see the current hash value */
static void sha256_peek(sha256_ctx_t *ctx, void *buffer)
{
	sha256_ctx_t ctx_copy = *ctx;
        sha256_end(&ctx_copy, buffer);
}

#if TLS_DEBUG_HASH
static void sha256_hash_dbg(const char *fmt, sha256_ctx_t *ctx, const void *buffer, size_t len)
{
        uint8_t h[SHA256_OUTSIZE];

	sha256_hash(ctx, buffer, len);
	dump_hex(fmt, buffer, len);
	dbg(" (%u) ", (int)len);
	sha256_peek(ctx, h);
	dump_hex("%s\n", h, SHA256_OUTSIZE);
}
#else
# define sha256_hash_dbg(fmt, ctx, buffer, len) \
         sha256_hash(ctx, buffer, len)
#endif

// RFC 2104
// HMAC(key, text) based on a hash H (say, sha256) is:
// ipad = [0x36 x INSIZE]
// opad = [0x5c x INSIZE]
// HMAC(key, text) = H((key XOR opad) + H((key XOR ipad) + text))
//
// H(key XOR opad) and H(key XOR ipad) can be precomputed
// if we often need HMAC hmac with the same key.
//
// text is often given in disjoint pieces.
static void hmac_sha256_precomputed_v(uint8_t out[SHA256_OUTSIZE],
		sha256_ctx_t *hashed_key_xor_ipad,
		sha256_ctx_t *hashed_key_xor_opad,
		va_list va)
{
	uint8_t *text;

	/* hashed_key_xor_ipad contains unclosed "H((key XOR ipad) +" state */
	/* hashed_key_xor_opad contains unclosed "H((key XOR opad) +" state */

	/* calculate out = H((key XOR ipad) + text) */
	while ((text = va_arg(va, uint8_t*)) != NULL) {
		unsigned text_size = va_arg(va, unsigned);
		sha256_hash(hashed_key_xor_ipad, text, text_size);
	}
	sha256_end(hashed_key_xor_ipad, out);

	/* out = H((key XOR opad) + out) */
	sha256_hash(hashed_key_xor_opad, out, SHA256_OUTSIZE);
	sha256_end(hashed_key_xor_opad, out);
}

static void hmac_sha256(uint8_t out[SHA256_OUTSIZE], uint8_t *key, unsigned key_size, ...)
{
	sha256_ctx_t hashed_key_xor_ipad;
	sha256_ctx_t hashed_key_xor_opad;
	uint8_t key_xor_ipad[SHA256_INSIZE];
	uint8_t key_xor_opad[SHA256_INSIZE];
	uint8_t tempkey[SHA256_OUTSIZE];
	va_list va;
	int i;

	va_start(va, key_size);

	// "The authentication key can be of any length up to INSIZE, the
	// block length of the hash function.  Applications that use keys longer
	// than INSIZE bytes will first hash the key using H and then use the
	// resultant OUTSIZE byte string as the actual key to HMAC."
	if (key_size > SHA256_INSIZE) {
		hash_sha256(tempkey, key, key_size);
		key = tempkey;
		key_size = SHA256_OUTSIZE;
	}

	for (i = 0; i < key_size; i++) {
		key_xor_ipad[i] = key[i] ^ 0x36;
		key_xor_opad[i] = key[i] ^ 0x5c;
	}
	for (; i < SHA256_INSIZE; i++) {
		key_xor_ipad[i] = 0x36;
		key_xor_opad[i] = 0x5c;
	}
	sha256_begin(&hashed_key_xor_ipad);
	sha256_hash(&hashed_key_xor_ipad, key_xor_ipad, SHA256_INSIZE);
	sha256_begin(&hashed_key_xor_opad);
	sha256_hash(&hashed_key_xor_opad, key_xor_opad, SHA256_INSIZE);

	hmac_sha256_precomputed_v(out, &hashed_key_xor_ipad, &hashed_key_xor_opad, va);
	va_end(va);
}

// RFC 5246:
// 5.  HMAC and the Pseudorandom Function
//...
// In this section, we define one PRF, based on HMAC.  This PRF with the
// SHA-256 hash function is used for all cipher suites defined in this
// document and in TLS documents published prior to this document when
// TLS 1.2 is negotiated.
//...
//    P_hash(secret, seed) = HMAC_hash(secret, A(1) + seed) +
//                           HMAC_hash(secret, A(2) + seed) +
//                           HMAC_hash(secret, A(3) + seed) + ...
// where + indicates concatenation.
// A() is defined as:
//    A(0) = seed
//    A(1) = HMAC_hash(secret, A(0)) = HMAC_hash(secret, seed)
//    A(i) = HMAC_hash(secret, A(i-1))
// P_hash can be iterated as many times as necessary to produce the
// required quantity of data.  For example, if P_SHA256 is being used to
// create 80 bytes of data, it will have to be iterated three times
// (through A(3)), creating 96 bytes of output data; the last 16 bytes
// of the final iteration will then be discarded, leaving 80 bytes of
// output data.
//
// TLS's PRF is created by applying P_hash to the secret as:
//
//    PRF(secret, label, seed) = P_<hash>(secret, label + seed)
//
// The label is an ASCII string.
static void prf_hmac_sha256(
		uint8_t *outbuf, unsigned outbuf_size,
		uint8_t *secret, unsigned secret_size,
		const char *label,
		uint8_t *seed, unsigned seed_size)
{
	uint8_t a[SHA256_OUTSIZE];
	uint8_t *out_p = outbuf;
	unsigned label_size = strlen(label);

	/* In P_hash() calculation, "seed" is "label + seed": */
#define SEED   label, label_size, seed, seed_size
#define SECRET secret, secret_size
#define A      a, (int)(sizeof(a))

	/* A(1) = HMAC_hash(secret, seed) */
	hmac_sha256(a, SECRET, SEED, NULL);
//TODO: convert hmac_sha256 to precomputed

	for(;;) {
		/* HMAC_hash(secret, A(1) + seed) */
		if (outbuf_size <= SHA256_OUTSIZE) {
			/* Last, possibly incomplete, block */
			/* (use a[] as temp buffer) */
			hmac_sha256(a, SECRET, A, SEED, NULL);
			memcpy(out_p, a, outbuf_size);
			return;
		}
		/* Not last block. Store directly to result buffer */
		hmac_sha256(out_p, SECRET, A, SEED, NULL);
		out_p += SHA256_OUTSIZE;
		outbuf_size -= SHA256_OUTSIZE;
		/* A(2) = HMAC_hash(secret, A(1)) */
		hmac_sha256(a, SECRET, A, NULL);
	}
#undef A
#undef SECRET
#undef SEED
}

static tls_state_t *new_tls_state(void)
{
	tls_state_t *tls = xzalloc(sizeof(*tls));
	tls->fd = -1;
	sha256_begin(&tls->handshake_sha256_ctx);
	return tls;
}

static void tls_error_die(tls_state_t *tls)
{
	dump_tls_record(tls->inbuf, tls->insize + tls->tail);
	xfunc_die();
}

static void *tls_get_outbuf(tls_state_t *tls, int len)
{
	if (len > MAX_OTBUF)
		xfunc_die();
	if (tls->outbuf_size < len + OUTBUF_PFX + OUTBUF_SFX) {
		tls->outbuf_size = len + OUTBUF_PFX + OUTBUF_SFX;
		tls->outbuf = xrealloc(tls->outbuf, tls->outbuf_size);
	}
	return tls->outbuf + OUTBUF_PFX;
}

// RFC 5246
// 6.2.3.1.  Null or Standard Stream Cipher
//
// Stream ciphers (including BulkCipherAlgorithm.null; see Appendix A.6)
// convert TLSCompressed.fragment structures to and from stream
// TLSCiphertext.fragment structures.
//
//    stream-ciphered struct {
//        opaque content[TLSCompressed.length];
//        opaque MAC[SecurityParameters.mac_length];
//    } GenericStreamCipher;
//
// The MAC is generated as:
//    MAC(MAC_write_key, seq_num +
//                          TLSCompressed.type +
//                          TLSCompressed.version +
//                          TLSCompressed.length +
//                          TLSCompressed.fragment);
// where "+" denotes concatenation.
// seq_num
//    The sequence number for this record.
// MAC
//    The MAC algorithm specified by SecurityParameters.mac_algorithm.
//
// Note that the MAC is computed before encryption.  The stream cipher
// encrypts the entire block, including the MAC.
//...
// Appendix C.  Cipher Suite Definitions
//...
//                         Key      IV   Block
// Cipher        Type    Material  Size  Size
// ------------  ------  --------  ----  -----
// AES_128_CBC   Block      16      16     16
// AES_256_CBC   Block      32      16     16
//
// MAC       Algorithm    mac_length  mac_key_length
// --------  -----------  ----------  --------------
// SHA       HMAC-SHA1       20            20
// SHA256    HMAC-SHA256     32            32
static void xwrite_encrypted(tls_state_t *tls, unsigned size, unsigned type)
{
	uint8_t *buf = tls->outbuf + OUTBUF_PFX;
	struct record_hdr *xhdr;

	xhdr = (void*)(buf - sizeof(*xhdr));
	if (CIPHER_ID != TLS_RSA_WITH_NULL_SHA256)
		xhdr = (void*)(buf - sizeof(*xhdr) - AES_BLOCKSIZE); /* place for IV */

	xhdr->type = type;
	xhdr->proto_maj = TLS_MAJ;
	xhdr->proto_min = TLS_MIN;
	/* fake unencrypted record header len for MAC calculation */
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;

	/* Calculate MAC signature */
//TODO: convert hmac_sha256 to precomputed
	hmac_sha256(buf + size,
			tls->client_write_MAC_key, sizeof(tls->client_write_MAC_key),
			&tls->write_seq64_be, sizeof(tls->write_seq64_be),
			xhdr, sizeof(*xhdr),
			buf, size,
			NULL);
	tls->write_seq64_be = SWAP_BE64(1 + SWAP_BE64(tls->write_seq64_be));

	size += SHA256_OUTSIZE;

	if (CIPHER_ID == TLS_RSA_WITH_NULL_SHA256) {
		/* No encryption, only signing */
		xhdr->len16_hi = size >> 8;
		xhdr->len16_lo = size & 0xff;
		dump_hex(">> %s\n", xhdr, sizeof(*xhdr) + size);
		xwrite(tls->fd, xhdr, sizeof(*xhdr) + size);
		dbg("wrote %u bytes (NULL crypt, SHA256 hash)\n", size);
		return;
	}

	// RFC 5246
	// 6.2.3.2.  CBC Block Cipher
	// For block ciphers (such as 3DES or AES), the encryption and MAC
	// functions convert TLSCompressed.fragment structures to and from block
	// TLSCiphertext.fragment structures.
	//    struct {
	//        opaque IV[SecurityParameters.record_iv_length];
	//        block-ciphered struct {
	//            opaque content[TLSCompressed.length];
	//            opaque MAC[SecurityParameters.mac_length];
	//            uint8 padding[GenericBlockCipher.padding_length];
	//            uint8 padding_length;
	//        };
	//    } GenericBlockCipher;
	//...
	// IV
	//    The Initialization Vector (IV) SHOULD be chosen at random, and
	//    MUST be unpredictable.  Note that in versions of TLS prior to 1.1,
	//    there was no IV field (...).  For block ciphers, the IV length is
	//    of length SecurityParameters.record_iv_length, which is equal to the
	//    SecurityParameters.block_size.
	// padding
	//    Padding that is added to force the length of the plaintext to be
	//    an integral multiple of the block cipher's block length.
	// padding_length
	//    The padding length MUST be such that the total size of the
	//    GenericBlockCipher structure is a multiple of the cipher's block
	//    length.  Legal values range from zero to 255, inclusive.
	//...
	// Appendix C.  Cipher Suite Definitions
	//...
	//                         Key      IV   Block
	// Cipher        Type    Material  Size  Size
	// ------------  ------  --------  ----  -----
	// AES_128_CBC   Block      16      16     16
	// AES_256_CBC   Block      32      16     16
    {
	psCipherContext_t ctx;
	uint8_t *p;
	uint8_t padding_length;

	/* Build IV+content+MAC+padding in outbuf */
	tls_get_random(buf - AES_BLOCKSIZE, AES_BLOCKSIZE); /* IV */
	dbg("before crypt: 5 hdr + %u data + %u hash bytes\n", size, SHA256_OUTSIZE);
	// RFC is talking nonsense:
	//    Padding that is added to force the length of the plaintext to be
	//    an integral multiple of the block cipher's block length.
	// WRONG. _padding+padding_length_, not just _padding_,
	// pads the data.
	// IOW: padding_length is the last byte of padding[] array,
	// contrary to what RFC depicts.
	//
	// What actually happens is that there is always padding.
	// If you need one byte to reach BLOCKSIZE, this byte is 0x00.
	// If you need two bytes, they are both 0x01.
	// If you need three, they are 0x02,0x02,0x02. And so on.
	// If you need no bytes to reach BLOCKSIZE, you have to pad a full
	// BLOCKSIZE with bytes of value (BLOCKSIZE-1).
	// It's ok to have more than minimum padding, but we do minimum.
	p = buf + size;
	padding_length = (~size) & (AES_BLOCKSIZE - 1);
	do {
		*p++ = padding_length;              /* padding */
		size++;
	} while ((size & (AES_BLOCKSIZE - 1)) != 0);

	/* Encrypt content+MAC+padding in place */
	psAesInit(&ctx, buf - AES_BLOCKSIZE, /* IV */
			tls->client_write_key, sizeof(tls->client_write_key)
	);
	psAesEncrypt(&ctx,
			buf, /* plaintext */
			buf, /* ciphertext */
			size
	);

	/* Write out */
	dbg("writing 5 + %u IV + %u encrypted bytes, padding_length:0x%02x\n",
			AES_BLOCKSIZE, size, padding_length);
	size += AES_BLOCKSIZE;     /* + IV */
	xhdr->len16_hi = size >> 8;
	xhdr->len16_lo = size & 0xff;
	dump_hex(">> %s\n", xhdr, sizeof(*xhdr) + size);
	xwrite(tls->fd, xhdr, sizeof(*xhdr) + size);
	dbg("wrote %u bytes\n", (int)sizeof(*xhdr) + size);
    }
}

static void xwrite_and_update_handshake_hash(tls_state_t *tls, unsigned size)
{
	if (!tls->encrypt_on_write) {
		uint8_t *buf = tls->outbuf + OUTBUF_PFX;
		struct record_hdr *xhdr = (void*)(buf - sizeof(*xhdr));

		xhdr->type = RECORD_TYPE_HANDSHAKE;
		xhdr->proto_maj = TLS_MAJ;
		xhdr->proto_min = TLS_MIN;
		xhdr->len16_hi = size >> 8;
		xhdr->len16_lo = size & 0xff;
		dump_hex(">> %s\n", xhdr, sizeof(*xhdr) + size);
		xwrite(tls->fd, xhdr, sizeof(*xhdr) + size);
		dbg("wrote %u bytes\n", (int)sizeof(*xhdr) + size);
		/* Handshake hash does not include record headers */
		sha256_hash_dbg(">> sha256:%s", &tls->handshake_sha256_ctx, buf, size);
		return;
	}
	xwrite_encrypted(tls, size, RECORD_TYPE_HANDSHAKE);
}

static int xread_tls_block(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	int sz;
	int total;
	int target;

	dbg("insize:%u tail:%u\n", tls->insize, tls->tail);
	memmove(tls->inbuf, tls->inbuf + tls->insize, tls->tail);
	errno = 0;
	total = tls->tail;
	target = sizeof(tls->inbuf);
	for (;;) {
		if (total >= sizeof(*xhdr) && target == sizeof(tls->inbuf)) {
			xhdr = (void*)tls->inbuf;
			target = sizeof(*xhdr) + (0x100 * xhdr->len16_hi + xhdr->len16_lo);
			if (target >= sizeof(tls->inbuf)) {
				/* malformed input (too long): yell and die */
				tls->tail = 0;
				tls->insize = total;
				tls_error_die(tls);
			}
			// can also check type/proto_maj/proto_min here
		}
		/* if total >= target, we have a full packet (and possibly more)... */
		if (total - target >= 0)
			break;
		sz = safe_read(tls->fd, tls->inbuf + total, sizeof(tls->inbuf) - total);
		if (sz <= 0)
			bb_perror_msg_and_die("short read");
		total += sz;
	}
	tls->tail = total - target;
	tls->insize = target;
	sz = target - sizeof(*xhdr);

	/* Needs to be decrypted? */
	if (tls->min_encrypted_len_on_read > SHA256_OUTSIZE) {
		psCipherContext_t ctx;
		uint8_t *p = tls->inbuf + sizeof(*xhdr);
		int padding_len;

		if (sz & (AES_BLOCKSIZE-1)
		 || sz < tls->min_encrypted_len_on_read
		) {
			bb_error_msg_and_die("bad encrypted len:%u", sz);
		}
		/* Decrypt content+MAC+padding in place */
		psAesInit(&ctx, p, /* IV */
			tls->server_write_key, sizeof(tls->server_write_key)
		);
		psAesDecrypt(&ctx,
			p + AES_BLOCKSIZE, /* ciphertext */
			p + AES_BLOCKSIZE, /* plaintext */
			sz
		);
		padding_len = p[sz - 1];
		dbg("encrypted size:%u type:0x%02x padding_length:0x%02x\n", sz, p[AES_BLOCKSIZE], padding_len);
		padding_len++;
		sz -= AES_BLOCKSIZE + SHA256_OUTSIZE + padding_len;
		if (sz < 0) {
			bb_error_msg_and_die("bad padding size:%u", padding_len);
		}
		if (sz != 0) {
			/* Skip IV */
			memmove(tls->inbuf + 5, tls->inbuf + 5 + AES_BLOCKSIZE, sz);
		}
	} else {
		/* if nonzero, then it's TLS_RSA_WITH_NULL_SHA256: drop MAC */
		/* else: no encryption yet on input, subtract zero = NOP */
		sz -= tls->min_encrypted_len_on_read;
	}

	/* RFC 5246 is not saying it explicitly, but sha256 hash
	 * in our FINISHED record must include data of incoming packets too!
	 */
	if (tls->inbuf[0] == RECORD_TYPE_HANDSHAKE) {
		sha256_hash_dbg("<< sha256:%s", &tls->handshake_sha256_ctx, tls->inbuf + 5, sz);
	}

	dbg("got block len:%u\n", sz);
	return sz;
}

/*
 * DER parsing routines
 */
static unsigned get_der_len(uint8_t **bodyp, uint8_t *der, uint8_t *end)
{
	unsigned len, len1;

	if (end - der < 2)
		xfunc_die();
//	if ((der[0] & 0x1f) == 0x1f) /* not single-byte item code? */
//		xfunc_die();

	len = der[1]; /* maybe it's short len */
	if (len >= 0x80) {
		/* no, it's long */

		if (len == 0x80 || end - der < (int)(len - 0x7e)) {
			/* 0x80 is "0 bytes of len", invalid DER: must use short len if can */
			/* need 3 or 4 bytes for 81, 82 */
			xfunc_die();
		}

		len1 = der[2]; /* if (len == 0x81) it's "ii 81 xx", fetch xx */
		if (len > 0x82) {
			/* >0x82 is "3+ bytes of len", should not happen realistically */
			xfunc_die();
		}
		if (len == 0x82) { /* it's "ii 82 xx yy" */
			len1 = 0x100*len1 + der[3];
			der += 1; /* skip [yy] */
		}
		der += 1; /* skip [xx] */
		len = len1;
//		if (len < 0x80)
//			xfunc_die(); /* invalid DER: must use short len if can */
	}
	der += 2; /* skip [code]+[1byte] */

	if (end - der < (int)len)
		xfunc_die();
	*bodyp = der;

	return len;
}

static uint8_t *enter_der_item(uint8_t *der, uint8_t **endp)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, *endp);
	dbg_der("entered der @%p:0x%02x len:%u inner_byte @%p:0x%02x\n", der, der[0], len, new_der, new_der[0]);
	/* Move "end" position to cover only this item */
	*endp = new_der + len;
	return new_der;
}

static uint8_t *skip_der_item(uint8_t *der, uint8_t *end)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, end);
	/* Skip body */
	new_der += len;
	dbg_der("skipped der 0x%02x, next byte 0x%02x\n", der[0], new_der[0]);
	return new_der;
}

static void der_binary_to_pstm(pstm_int *pstm_n, uint8_t *der, uint8_t *end)
{
	uint8_t *bin_ptr;
	unsigned len = get_der_len(&bin_ptr, der, end);

	dbg_der("binary bytes:%u, first:0x%02x\n", len, bin_ptr[0]);
	pstm_init_for_read_unsigned_bin(/*pool:*/ NULL, pstm_n, len);
	pstm_read_unsigned_bin(pstm_n, bin_ptr, len);
	//return bin + len;
}

static void find_key_in_der_cert(tls_state_t *tls, uint8_t *der, int len)
{
/* Certificate is a DER-encoded data structure. Each DER element has a length,
 * which makes it easy to skip over large compound elements of any complexity
 * without parsing them. Example: partial decode of kernel.org certificate:
 *  SEQ 0x05ac/1452 bytes (Certificate): 308205ac
 *    SEQ 0x0494/1172 bytes (tbsCertificate): 30820494
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0] 3 bytes: a003
 *        INTEGER (version): 0201 02
 *      INTEGER 0x11 bytes (serialNumber): 0211 00 9f85bf664b0cddafca508679501b2be4
 *      //^^^^^^note: matrixSSL also allows [ASN_CONTEXT_SPECIFIC | ASN_PRIMITIVE | 2] = 0x82 type
 *      SEQ 0x0d bytes (signatureAlgo): 300d
 *        OID 9 bytes: 0609 2a864886f70d01010b (OID_SHA256_RSA_SIG 42.134.72.134.247.13.1.1.11)
 *        NULL: 0500
 *      SEQ 0x5f bytes (issuer): 305f
 *        SET 11 bytes: 310b
 *          SEQ 9 bytes: 3009
 *            OID 3 bytes: 0603 550406
 *            Printable string "FR": 1302 4652
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550408
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 550407
 *            Printable string "Paris": 1305 5061726973
 *        SET 14 bytes: 310e
 *          SEQ 12 bytes: 300c
 *            OID 3 bytes: 0603 55040a
 *            Printable string "Gandi": 1305 47616e6469
 *        SET 32 bytes: 3120
 *          SEQ 30 bytes: 301e
 *            OID 3 bytes: 0603 550403
 *            Printable string "Gandi Standard SSL CA 2": 1317 47616e6469205374616e646172642053534c2043412032
 *      SEQ 30 bytes (validity): 301e
 *        TIME "161011000000Z": 170d 3136313031313030303030305a
 *        TIME "191011235959Z": 170d 3139313031313233353935395a
 *      SEQ 0x5b/91 bytes (subject): 305b //I did not decode this
 *          3121301f060355040b1318446f6d61696e20436f
 *          6e74726f6c2056616c6964617465643121301f06
 *          0355040b1318506f73697469766553534c204d75
 *          6c74692d446f6d61696e31133011060355040313
 *          0a6b65726e656c2e6f7267
 *      SEQ 0x01a2/418 bytes (subjectPublicKeyInfo): 308201a2
 *        SEQ 13 bytes (algorithm): 300d
 *          OID 9 bytes: 0609 2a864886f70d010101 (OID_RSA_KEY_ALG 42.134.72.134.247.13.1.1.1)
 *          NULL: 0500
 *        BITSTRING 0x018f/399 bytes (publicKey): 0382018f
 *          ????: 00
 *          //after the zero byte, it appears key itself uses DER encoding:
 *          SEQ 0x018a/394 bytes: 3082018a
 *            INTEGER 0x0181/385 bytes (modulus): 02820181
 *                  00b1ab2fc727a3bef76780c9349bf3
 *                  ...24 more blocks of 15 bytes each...
 *                  90e895291c6bc8693b65
 *            INTEGER 3 bytes (exponent): 0203 010001
 *      [ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 0x3] 0x01e5 bytes (X509v3 extensions): a38201e5
 *        SEQ 0x01e1 bytes: 308201e1
 *        ...
 * Certificate is a sequence of three elements:
 *	tbsCertificate (SEQ)
 *	signatureAlgorithm (AlgorithmIdentifier)
 *	signatureValue (BIT STRING)
 *
 * In turn, tbsCertificate is a sequence of:
 *	version
 *	serialNumber
 *	signatureAlgo (AlgorithmIdentifier)
 *	issuer (Name, has complex structure)
 *	validity (Validity, SEQ of two Times)
 *	subject (Name)
 *	subjectPublicKeyInfo (SEQ)
 *	...
 *
 * subjectPublicKeyInfo is a sequence of:
 *	algorithm (AlgorithmIdentifier)
 *	publicKey (BIT STRING)
 *
 * We need Certificate.tbsCertificate.subjectPublicKeyInfo.publicKey
 */
	uint8_t *end = der + len;

	/* enter "Certificate" item: [der, end) will be only Cert */
	der = enter_der_item(der, &end);

	/* enter "tbsCertificate" item: [der, end) will be only tbsCert */
	der = enter_der_item(der, &end);

	/* skip up to subjectPublicKeyInfo */
	der = skip_der_item(der, end); /* version */
	der = skip_der_item(der, end); /* serialNumber */
	der = skip_der_item(der, end); /* signatureAlgo */
	der = skip_der_item(der, end); /* issuer */
	der = skip_der_item(der, end); /* validity */
	der = skip_der_item(der, end); /* subject */

	/* enter subjectPublicKeyInfo */
	der = enter_der_item(der, &end);
	{ /* check subjectPublicKeyInfo.algorithm */
		static const uint8_t expected[] = {
			0x30,0x0d, // SEQ 13 bytes
			0x06,0x09, 0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01, // OID RSA_KEY_ALG 42.134.72.134.247.13.1.1.1
			//0x05,0x00, // NULL
		};
		if (memcmp(der, expected, sizeof(expected)) != 0)
			bb_error_msg_and_die("not RSA key");
	}
	/* skip subjectPublicKeyInfo.algorithm */
	der = skip_der_item(der, end);
	/* enter subjectPublicKeyInfo.publicKey */
//	die_if_not_this_der_type(der, end, 0x03); /* must be BITSTRING */
	der = enter_der_item(der, &end);

	/* parse RSA key: */
//based on getAsnRsaPubKey(), pkcs1ParsePrivBin() is also of note
	dbg("key bytes:%u, first:0x%02x\n", (int)(end - der), der[0]);
	if (end - der < 14) xfunc_die();
	/* example format:
	 * ignore bits: 00
	 * SEQ 0x018a/394 bytes: 3082018a
	 *   INTEGER 0x0181/385 bytes (modulus): 02820181 XX...XXX
	 *   INTEGER 3 bytes (exponent): 0203 010001
	 */
	if (*der != 0) /* "ignore bits", should be 0 */
		xfunc_die();
	der++;
	der = enter_der_item(der, &end); /* enter SEQ */
	/* memset(tls->server_rsa_pub_key, 0, sizeof(tls->server_rsa_pub_key)); - already is */
	der_binary_to_pstm(&tls->server_rsa_pub_key.N, der, end); /* modulus */
	der = skip_der_item(der, end);
	der_binary_to_pstm(&tls->server_rsa_pub_key.e, der, end); /* exponent */
	tls->server_rsa_pub_key.size = pstm_unsigned_bin_size(&tls->server_rsa_pub_key.N);
	dbg("server_rsa_pub_key.size:%d\n", tls->server_rsa_pub_key.size);
}

/*
 * TLS Handshake routines
 */
static int xread_tls_handshake_block(tls_state_t *tls, int min_len)
{
	struct record_hdr *xhdr;
	int len = xread_tls_block(tls);

	xhdr = (void*)tls->inbuf;
	if (len < min_len
	 || xhdr->type != RECORD_TYPE_HANDSHAKE
	 || xhdr->proto_maj != TLS_MAJ
	 || xhdr->proto_min != TLS_MIN
	) {
		tls_error_die(tls);
	}
	dbg("got HANDSHAKE\n");
	return len;
}

static ALWAYS_INLINE void fill_handshake_record_hdr(void *buf, unsigned type, unsigned len)
{
	struct handshake_hdr {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
	} *h = buf;

	len -= 4;
	h->type = type;
	h->len24_hi  = len >> 16;
	h->len24_mid = len >> 8;
	h->len24_lo  = len & 0xff;
}

//TODO: implement RFC 5746 (Renegotiation Indication Extension) - some servers will refuse to work with us otherwise
static void send_client_hello(tls_state_t *tls)
{
	struct client_hello {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32];
		uint8_t session_id_len;
		/* uint8_t session_id[]; */
		uint8_t cipherid_len16_hi, cipherid_len16_lo;
		uint8_t cipherid[2 * 1]; /* actually variable */
		uint8_t comprtypes_len;
		uint8_t comprtypes[1]; /* actually variable */
	};
	struct client_hello *record = tls_get_outbuf(tls, sizeof(*record));

	fill_handshake_record_hdr(record, HANDSHAKE_CLIENT_HELLO, sizeof(*record));
	record->proto_maj = TLS_MAJ;	/* the "requested" version of the protocol, */
	record->proto_min = TLS_MIN;	/* can be higher than one in record headers */
	tls_get_random(record->rand32, sizeof(record->rand32));
memset(record->rand32, 0x11, sizeof(record->rand32));
	memcpy(tls->client_and_server_rand32, record->rand32, sizeof(record->rand32));
	record->session_id_len = 0;
	record->cipherid_len16_hi = 0;
	record->cipherid_len16_lo = 2 * 1;
	record->cipherid[0] = CIPHER_ID >> 8;
	record->cipherid[1] = CIPHER_ID & 0xff;
	record->comprtypes_len = 1;
	record->comprtypes[0] = 0;

	//dbg (make it repeatable): memset(record.rand32, 0x11, sizeof(record.rand32));
	dbg(">> CLIENT_HELLO\n");
	xwrite_and_update_handshake_hash(tls, sizeof(*record));
}

static void get_server_hello(tls_state_t *tls)
{
	struct server_hello {
		struct record_hdr xhdr;
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t proto_maj, proto_min;
		uint8_t rand32[32]; /* first 4 bytes are unix time in BE format */
		uint8_t session_id_len;
		uint8_t session_id[32];
		uint8_t cipherid_hi, cipherid_lo;
		uint8_t comprtype;
		/* extensions may follow, but only those which client offered in its Hello */
	};
	struct server_hello *hp;
	uint8_t *cipherid;

	xread_tls_handshake_block(tls, 74);

	hp = (void*)tls->inbuf;
	// 74 bytes:
	// 02  000046 03|03   58|78|cf|c1 50|a5|49|ee|7e|29|48|71|fe|97|fa|e8|2d|19|87|72|90|84|9d|37|a3|f0|cb|6f|5f|e3|3c|2f |20  |d8|1a|78|96|52|d6|91|01|24|b3|d6|5b|b7|d0|6c|b3|e1|78|4e|3c|95|de|74|a0|ba|eb|a7|3a|ff|bd|a2|bf |00|9c |00|
	//SvHl len=70 maj.min unixtime^^^ 28randbytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^ slen sid32bytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ cipSel comprSel
	if (hp->type != HANDSHAKE_SERVER_HELLO
	 || hp->len24_hi  != 0
	 || hp->len24_mid != 0
	 /* hp->len24_lo checked later */
	 || hp->proto_maj != TLS_MAJ
	 || hp->proto_min != TLS_MIN
	) {
		tls_error_die(tls);
	}

	cipherid = &hp->cipherid_hi;
	if (hp->session_id_len != 32) {
		if (hp->session_id_len != 0)
			tls_error_die(tls);

		// session_id_len == 0: no session id
		// "The server
		// may return an empty session_id to indicate that the session will
		// not be cached and therefore cannot be resumed."
		cipherid -= 32;
		hp->len24_lo += 32; /* what len would be if session id would be present */
	}

	if (hp->len24_lo < 70
	 || cipherid[0]  != (CIPHER_ID >> 8)
	 || cipherid[1]  != (CIPHER_ID & 0xff)
	 || cipherid[2]  != 0 /* comprtype */
	) {
		tls_error_die(tls);
	}

	dbg("<< SERVER_HELLO\n");
	memcpy(tls->client_and_server_rand32 + 32, hp->rand32, sizeof(hp->rand32));
}

static void get_server_cert(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	uint8_t *certbuf;
	int len, len1;

	len = xread_tls_handshake_block(tls, 10);

	xhdr = (void*)tls->inbuf;
	certbuf = (void*)(xhdr + 1);
	if (certbuf[0] != HANDSHAKE_CERTIFICATE)
		tls_error_die(tls);
	dbg("<< CERTIFICATE\n");
	// 4392 bytes:
	// 0b  00|11|24 00|11|21 00|05|b0 30|82|05|ac|30|82|04|94|a0|03|02|01|02|02|11|00|9f|85|bf|66|4b|0c|dd|af|ca|50|86|79|50|1b|2b|e4|30|0d...
	//Cert len=4388 ChainLen CertLen^ DER encoded X509 starts here. openssl x509 -in FILE -inform DER -noout -text
	len1 = get24be(certbuf + 1);
	if (len1 > len - 4) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 4);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;
	len1 = get24be(certbuf + 7);
	if (len1 > len - 3) tls_error_die(tls);
	len = len1;

	if (len)
		find_key_in_der_cert(tls, certbuf + 10, len);
}

static void send_client_key_exchange(tls_state_t *tls)
{
	struct client_key_exchange {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		/* keylen16 exists for RSA (in TLS, not in SSL), but not for some other key types */
		uint8_t keylen16_hi, keylen16_lo;
		uint8_t key[4 * 1024]; // size??
	};
//FIXME: better size estimate
	struct client_key_exchange *record = tls_get_outbuf(tls, sizeof(*record));
	uint8_t rsa_premaster[SSL_HS_RSA_PREMASTER_SIZE];
	int len;

	tls_get_random(rsa_premaster, sizeof(rsa_premaster));
memset(rsa_premaster, 0x44, sizeof(rsa_premaster));
	// RFC 5246
	// "Note: The version number in the PreMasterSecret is the version
	// offered by the client in the ClientHello.client_version, not the
	// version negotiated for the connection."
	rsa_premaster[0] = TLS_MAJ;
	rsa_premaster[1] = TLS_MIN;
	len = psRsaEncryptPub(/*pool:*/ NULL,
		/* psRsaKey_t* */ &tls->server_rsa_pub_key,
		rsa_premaster, /*inlen:*/ sizeof(rsa_premaster),
		record->key, sizeof(record->key),
		data_param_ignored
	);
	record->keylen16_hi = len >> 8;
	record->keylen16_lo = len & 0xff;
	len += 2;
	record->type = HANDSHAKE_CLIENT_KEY_EXCHANGE;
	record->len24_hi  = 0;
	record->len24_mid = len >> 8;
	record->len24_lo  = len & 0xff;
	len += 4;

	dbg(">> CLIENT_KEY_EXCHANGE\n");
	xwrite_and_update_handshake_hash(tls, len);

	// RFC 5246
	// For all key exchange methods, the same algorithm is used to convert
	// the pre_master_secret into the master_secret.  The pre_master_secret
	// should be deleted from memory once the master_secret has been
	// computed.
	//      master_secret = PRF(pre_master_secret, "master secret",
	//                          ClientHello.random + ServerHello.random)
	//                          [0..47];
	// The master secret is always exactly 48 bytes in length.  The length
	// of the premaster secret will vary depending on key exchange method.
	prf_hmac_sha256(
		tls->master_secret, sizeof(tls->master_secret),
		rsa_premaster, sizeof(rsa_premaster),
		"master secret",
		tls->client_and_server_rand32, sizeof(tls->client_and_server_rand32)
	);
	dump_hex("master secret:%s\n", tls->master_secret, sizeof(tls->master_secret));

	// RFC 5246
	// 6.3.  Key Calculation
	//
	// The Record Protocol requires an algorithm to generate keys required
	// by the current connection state (see Appendix A.6) from the security
	// parameters provided by the handshake protocol.
	//
	// The master secret is expanded into a sequence of secure bytes, which
	// is then split to a client write MAC key, a server write MAC key, a
	// client write encryption key, and a server write encryption key.  Each
	// of these is generated from the byte sequence in that order.  Unused
	// values are empty.  Some AEAD ciphers may additionally require a
	// client write IV and a server write IV (see Section 6.2.3.3).
	//
	// When keys and MAC keys are generated, the master secret is used as an
	// entropy source.
	//
	// To generate the key material, compute
	//
	//    key_block = PRF(SecurityParameters.master_secret,
	//                    "key expansion",
	//                    SecurityParameters.server_random +
	//                    SecurityParameters.client_random);
	//
	// until enough output has been generated.  Then, the key_block is
	// partitioned as follows:
	//
	//    client_write_MAC_key[SecurityParameters.mac_key_length]
	//    server_write_MAC_key[SecurityParameters.mac_key_length]
	//    client_write_key[SecurityParameters.enc_key_length]
	//    server_write_key[SecurityParameters.enc_key_length]
	//    client_write_IV[SecurityParameters.fixed_iv_length]
	//    server_write_IV[SecurityParameters.fixed_iv_length]
	{
		uint8_t tmp64[64];

		/* make "server_rand32 + client_rand32" */
		memcpy(&tmp64[0] , &tls->client_and_server_rand32[32], 32);
		memcpy(&tmp64[32], &tls->client_and_server_rand32[0] , 32);

		prf_hmac_sha256(
			tls->client_write_MAC_key, 2 * (SHA256_OUTSIZE + AES256_KEYSIZE),
			// also fills:
			// server_write_MAC_key[SHA256_OUTSIZE]
			// client_write_key[AES256_KEYSIZE]
			// server_write_key[AES256_KEYSIZE]
			tls->master_secret, sizeof(tls->master_secret),
			"key expansion",
			tmp64, 64
		);
		dump_hex("client_write_MAC_key:%s\n",
			tls->client_write_MAC_key, sizeof(tls->client_write_MAC_key)
		);
		dump_hex("client_write_key:%s\n",
			tls->client_write_key, sizeof(tls->client_write_key)
		);
	}
}

static const uint8_t rec_CHANGE_CIPHER_SPEC[] = {
	RECORD_TYPE_CHANGE_CIPHER_SPEC, TLS_MAJ, TLS_MIN, 00, 01,
	01
};

static void send_change_cipher_spec(tls_state_t *tls)
{
	dbg(">> CHANGE_CIPHER_SPEC\n");
	xwrite(tls->fd, rec_CHANGE_CIPHER_SPEC, sizeof(rec_CHANGE_CIPHER_SPEC));
}

// 7.4.9.  Finished
// A Finished message is always sent immediately after a change
// cipher spec message to verify that the key exchange and
// authentication processes were successful.  It is essential that a
// change cipher spec message be received between the other handshake
// messages and the Finished message.
//...
// The Finished message is the first one protected with the just
// negotiated algorithms, keys, and secrets.  Recipients of Finished
// messages MUST verify that the contents are correct.  Once a side
// has sent its Finished message and received and validated the
// Finished message from its peer, it may begin to send and receive
// application data over the connection.
//...
// struct {
//     opaque verify_data[verify_data_length];
// } Finished;
//
// verify_data
//    PRF(master_secret, finished_label, Hash(handshake_messages))
//       [0..verify_data_length-1];
//
// finished_label
//    For Finished messages sent by the client, the string
//    "client finished".  For Finished messages sent by the server,
//    the string "server finished".
//
// Hash denotes a Hash of the handshake messages.  For the PRF
// defined in Section 5, the Hash MUST be the Hash used as the basis
// for the PRF.  Any cipher suite which defines a different PRF MUST
// also define the Hash to use in the Finished computation.
//
// In previous versions of TLS, the verify_data was always 12 octets
// long.  In the current version of TLS, it depends on the cipher
// suite.  Any cipher suite which does not explicitly specify
// verify_data_length has a verify_data_length equal to 12.  This
// includes all existing cipher suites.
static void send_client_finished(tls_state_t *tls)
{
	struct finished {
		uint8_t type;
		uint8_t len24_hi, len24_mid, len24_lo;
		uint8_t prf_result[12];
	};
	struct finished *record = tls_get_outbuf(tls, sizeof(*record));
	uint8_t handshake_hash[SHA256_OUTSIZE];

	fill_handshake_record_hdr(record, HANDSHAKE_FINISHED, sizeof(*record));

	sha256_peek(&tls->handshake_sha256_ctx, handshake_hash);
	prf_hmac_sha256(record->prf_result, sizeof(record->prf_result),
			tls->master_secret, sizeof(tls->master_secret),
			"client finished",
			handshake_hash, sizeof(handshake_hash)
	);
	dump_hex("from secret: %s\n", tls->master_secret, sizeof(tls->master_secret));
	dump_hex("from labelSeed: %s", "client finished", sizeof("client finished")-1);
	dump_hex("%s\n", handshake_hash, sizeof(handshake_hash));
	dump_hex("=> digest: %s\n", record->prf_result, sizeof(record->prf_result));

	dbg(">> FINISHED\n");
	xwrite_encrypted(tls, sizeof(*record), RECORD_TYPE_HANDSHAKE);
}

static void tls_handshake(tls_state_t *tls)
{
	// Client              RFC 5246                Server
	// (*) - optional messages, not always sent
	//
	// ClientHello          ------->
	//                                        ServerHello
	//                                       Certificate*
	//                                 ServerKeyExchange*
	//                                CertificateRequest*
	//                      <-------      ServerHelloDone
	// Certificate*
	// ClientKeyExchange
	// CertificateVerify*
	// [ChangeCipherSpec]
	// Finished             ------->
	//                                 [ChangeCipherSpec]
	//                      <-------             Finished
	// Application Data     <------>     Application Data
	int len;

	send_client_hello(tls);
	get_server_hello(tls);

	//RFC 5246
	// The server MUST send a Certificate message whenever the agreed-
	// upon key exchange method uses certificates for authentication
	// (this includes all key exchange methods defined in this document
	// except DH_anon).  This message will always immediately follow the
	// ServerHello message.
	//
	// IOW: in practice, Certificate *always* follows.
	// (for example, kernel.org does not even accept DH_anon cipher id)
	get_server_cert(tls);

	len = xread_tls_handshake_block(tls, 4);
	if (tls->inbuf[5] == HANDSHAKE_SERVER_KEY_EXCHANGE) {
		// 459 bytes:
		// 0c   00|01|c7 03|00|17|41|04|87|94|2e|2f|68|d0|c9|f4|97|a8|2d|ef|ed|67|ea|c6|f3|b3|56|47|5d|27|b6|bd|ee|70|25|30|5e|b0|8e|f6|21|5a...
		//SvKey len=455^
		// with TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA: 461 bytes:
		// 0c   00|01|c9 03|00|17|41|04|cd|9b|b4|29|1f|f6|b0|c2|84|82|7f|29|6a|47|4e|ec|87|0b|c1|9c|69|e1|f8|c6|d0|53|e9|27|90|a5|c8|02|15|75...
		dbg("<< SERVER_KEY_EXCHANGE len:%u\n", len);
//probably need to save it
		xread_tls_handshake_block(tls, 4);
	}

//	if (tls->inbuf[5] == HANDSHAKE_CERTIFICATE_REQUEST) {
//		dbg("<< CERTIFICATE_REQUEST\n");
//RFC 5246: (in response to this,) "If no suitable certificate is available,
// the client MUST send a certificate message containing no
// certificates.  That is, the certificate_list structure has a
// length of zero. ...
// Client certificates are sent using the Certificate structure
// defined in Section 7.4.2."
// (i.e. the same format as server certs)
//		xread_tls_handshake_block(tls, 4);
//	}

	if (tls->inbuf[5] != HANDSHAKE_SERVER_HELLO_DONE)
		tls_error_die(tls);
	// 0e 000000 (len:0)
	dbg("<< SERVER_HELLO_DONE\n");

	send_client_key_exchange(tls);

	send_change_cipher_spec(tls);
	/* from now on we should send encrypted */
	/* tls->write_seq64_be = 0; - already is */
	tls->encrypt_on_write = 1;

	send_client_finished(tls);

	/* Get CHANGE_CIPHER_SPEC */
	len = xread_tls_block(tls);
	if (len != 1 || memcmp(tls->inbuf, rec_CHANGE_CIPHER_SPEC, 6) != 0)
		tls_error_die(tls);
	dbg("<< CHANGE_CIPHER_SPEC\n");
	if (CIPHER_ID == TLS_RSA_WITH_NULL_SHA256)
		tls->min_encrypted_len_on_read = SHA256_OUTSIZE;
	else
		/* all incoming packets now should be encrypted and have IV + MAC + padding */
		tls->min_encrypted_len_on_read = AES_BLOCKSIZE + SHA256_OUTSIZE + AES_BLOCKSIZE;

	/* Get (encrypted) FINISHED from the server */
	len = xread_tls_block(tls);
	if (len < 4 || tls->inbuf[5] != HANDSHAKE_FINISHED)
		tls_error_die(tls);
	dbg("<< FINISHED\n");
	/* application data can be sent/received */
}

static void tls_xwrite(tls_state_t *tls, int len)
{
	dbg(">> DATA\n");
	xwrite_encrypted(tls, len, RECORD_TYPE_APPLICATION_DATA);
}

// To run a test server using openssl:
// openssl req -x509 -newkey rsa:$((4096/4*3)) -keyout key.pem -out server.pem -nodes -days 99999 -subj '/CN=localhost'
// openssl s_server -key key.pem -cert server.pem -debug -tls1_2 -no_tls1 -no_tls1_1
//
// Unencryped SHA256 example:
// openssl req -x509 -newkey rsa:$((4096/4*3)) -keyout key.pem -out server.pem -nodes -days 99999 -subj '/CN=localhost'
// openssl s_server -key key.pem -cert server.pem -debug -tls1_2 -no_tls1 -no_tls1_1 -cipher NULL
// openssl s_client -connect 127.0.0.1:4433 -debug -tls1_2 -no_tls1 -no_tls1_1 -cipher NULL-SHA256

int tls_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tls_main(int argc UNUSED_PARAM, char **argv)
{
	tls_state_t *tls;
	fd_set readfds, testfds;
	int cfd;

	// INIT_G();
	// getopt32(argv, "myopts")

	if (!argv[1])
		bb_show_usage();

	cfd = create_and_connect_stream_or_die(argv[1], 443);

	tls = new_tls_state();
	tls->fd = cfd;
	tls_handshake(tls);

	/* Select loop copying stdin to cfd, and cfd to stdout */
	FD_ZERO(&readfds);
	FD_SET(cfd, &readfds);
	FD_SET(STDIN_FILENO, &readfds);

#define iobuf bb_common_bufsiz1
	setup_common_bufsiz();
	for (;;) {
		int nread;

		testfds = readfds;

		if (select(cfd + 1, &testfds, NULL, NULL, NULL) < 0)
			bb_perror_msg_and_die("select");

		if (FD_ISSET(STDIN_FILENO, &testfds)) {
			void *buf = tls_get_outbuf(tls, COMMON_BUFSIZE);
			nread = safe_read(STDIN_FILENO, buf, COMMON_BUFSIZE);
			if (nread < 1) {
//&& errno != EAGAIN
				/* Close outgoing half-connection so they get EOF,
				 * but leave incoming alone so we can see response */
//				shutdown(cfd, SHUT_WR);
				FD_CLR(STDIN_FILENO, &readfds);
			}
			tls_xwrite(tls, nread);
		}
		if (FD_ISSET(cfd, &testfds)) {
			nread = xread_tls_block(tls);
			if (nread < 1)
//if eof, just close stdout, but not exit!
				return EXIT_SUCCESS;
			xwrite(STDOUT_FILENO, tls->inbuf + 5, nread);
		}
	}

	return EXIT_SUCCESS;
}
/* Unencryped SHA256 example:
 * s_client says:

write to 0x1d750b0 [0x1e6f153] (99 bytes => 99 (0x63))
0000 - 16 03 01 005e  01 00005a   0303 [4d ef 5c 82 3e   ....^...Z..M.\.> >> ClHello
0010 - bf a6 ee f1 1e 04 d1 5c-99 20 86 13 e9 0a cf 58   .......\. .....X
0020 - 75 b1 bd 7a e6 d6 44 f3-d3 a1 52] 00 0004 003b    u..z..D...R....; 003b = TLS_RSA_WITH_NULL_SHA256
0030 - 00ff                                                                       TLS_EMPTY_RENEGOTIATION_INFO_SCSV
             0100                                                             compr=none
                   002d, 0023  0000, 000d  0020 [00 1e   .....-.#..... .. extlen, SessionTicketTLS 0 bytes, SignatureAlgorithms 32 bytes
0040 - 06 01 06 02 06 03 05 01-05 02 05 03 04 01 04 02   ................
0050 - 04 03 03 01 03 02 03 03-02 01 02 02 02 03] 000f   ................ Heart Beat 1 byte
0060 - 0001  01                                          ...

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 16 03 03 00 3a                                    ....:
read from 0x1d750b0 [0x1e6ac08] (58 bytes => 58 (0x3A))
0000 - 02 000036   0303  [f2 61-ae c8 58 e3 51 42 32 93   ...6...a..X.QB2. << SvHello
0010 - c5 62 e4 f5 06 93 81 65-aa f7 df 74 af 7c 98 b4   .b.....e...t.|..
0020 - 3e a7 35 c3 25 69] 00,003b,00..................   >.5.%i..;....... - no session id! "The server
									may return an empty session_id to indicate that the session will
									not be cached and therefore cannot be resumed."
									003b = TLS_RSA_WITH_NULL_SHA256 accepted, 00 - no compr
                                     000e  ff01  0001                 extlen, 0xff01=RenegotiationInfo 1 byte
0030 - 00, 0023 0000,                                                SessionTicketTLS 0 bytes
                       000f 0001 01                     ..#.......       Heart Beat 1 byte

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 16 03 03 04 0b                                    .....
read from 0x1d750b0 [0x1e6ac08] (1035 bytes => 1035 (0x40B))
0000 - 0b 00 04 07 00 04 04 00-04 01 30 82 03 fd 30 82   ..........0...0. << Cert
0010 - 02 65 a0 03 02 01 02 02-09 00 d9 d9 8d b8 94 ad   .e..............
0020 - 2e 2b 30 0d 06 09 2a 86-48 86 f7 0d 01 01 0b 05   .+0...*.H.......
0030 - 00 30 14 31 12 30 10 06-03 55 04 03 0c 09 6c 6f   .0.1.0...U....lo
0040 - 63 61 6c 68 6f 73 74 30-20 17 0d 31 37 30 31 31   calhost0 ..17011
...".......".......".......".......".......".......".......".......".....
03f0 - 11 8a cd c5 a3 0a 22 43-d5 13 f9 a5 8a 06 f9 00   ......"C........
0400 - 3c f7 86 4e e8 a5 d8 5b-92 37 f5                  <..N...[.7.
depth=0 CN = localhost
verify error:num=18:self signed certificate
verify return:1
depth=0 CN = localhost
verify return:1

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 16 03 03 00 04                                    .....

read from 0x1d750b0 [0x1e6ac08] (4 bytes => 4 (0x4))                      << SvDone
0000 - 0e                                                .
0004 - <SPACES/NULS>

write to 0x1d750b0 [0x1e74620] (395 bytes => 395 (0x18B))                 >> ClDone
0000 - 16 03 03 01 86 10 00 01-82 01 80 88 f0 87 5d b0   ..............].
0010 - ea df 3b 4d e2 35 f3 99-e6 d4 29 87 36 86 ea 30   ..;M.5....).6..0
0020 - 38 80 c7 37 66 7f 5b e7-23 38 7e 87 24 66 82 81   8..7f.[.#8~.$f..
0030 - e4 ba 6c 2a 0c 92 a8 b9-39 c1 55 16 32 88 14 cd   ..l*....9.U.2...
0040 - 95 8c 82 49 a1 c7 f9 9b-e5 8f f6 5e 7e ee 91 b3   ...I.......^~...
0050 - 2c 92 e7 a3 02 f8 9f 56-04 45 39 df a7 d6 1a 16   ,......V.E9.....
0060 - 67 5c a4 f8 87 8a c4 c8-6c 6f c6 f0 9b c9 b4 87   g\......lo......
0070 - 36 43 c1 67 9f b3 aa 11-34 b0 c2 fc 1f d9 e1 ff   6C.g....4.......
0080 - fb e1 89 db 91 58 ec cc-aa 16 19 9a 91 74 e2 46   .....X.......t.F
0090 - 22 a7 a7 f7 9e 3c 97 82-2c e4 21 b3 fa ef ba 3f   "....<..,.!....?
00a0 - 57 48 e4 b2 84 b7 c2 81-92 a9 f1 03 68 f4 e6 0c   WH..........h...
00b0 - fd 54 87 f5 e9 a0 5d e6-5f 0e bd 80 86 27 ab 0e   .T....]._....'..
00c0 - cf 92 4f bd fc 24 b9 54-72 5f 58 df 6b 2b 1d 97   ..O..$.Tr_X.k+..
00d0 - 00 60 fe 95 b0 aa d6 c7-c1 3a f9 2e 7c 92 a9 6d   .`.......:..|..m
00e0 - 28 a3 ef 3e c1 e6 2d 2d-e8 db 81 ea 51 02 3f 64   (..>..--....Q.?d
00f0 - a8 66 14 c1 4b 17 1f 55-c6 5b 3b 38 c3 6a 61 a8   .f..K..U.[;8.ja.
0100 - f7 ad 65 7d cb 14 6d b3-0f 76 19 25 8e ed bd 53   ..e}..m..v.%...S
0110 - 35 a9 a1 34 00 9d 07 81-84 51 35 e0 83 83 e3 a6   5..4.....Q5.....
0120 - c7 77 4c 61 e4 78 9c cb-f5 92 4e d6 dd c4 c2 2b   .wLa.x....N....+
0130 - 75 9e 72 a6 7f 81 6a 1c-fc 4a 51 91 81 b4 cc 33   u.r...j..JQ....3
0140 - 1c 8b 0a b6 94 8b 16 1b-86 2f 31 5e 31 e1 57 14   ........./1^1.W.
0150 - 2e b5 09 5d cf 6f ea b2-94 e9 5c cc b9 fc 24 a0   ...].o....\...$.
0160 - b7 f1 f4 9d 95 46 4f 08-5c 45 c6 2f 9f 7d 76 09   .....FO.\E./.}v.
0170 - 6a af 50 2c 89 76 82 5f-e8 34 d8 4b 84 b6 34 18   j.P,.v._.4.K..4.
0180 - 85 95 4a 3f 0f 28 88 3a-71 32 90                  ..J?.(.:q2.

write to 0x1d750b0 [0x1e74620] (6 bytes => 6 (0x6))
0000 - 14 03 03 00 01 01                                 ......           >> CHANGE_CIPHER_SPEC

write to 0x1d750b0 [0x1e74620] (53 bytes => 53 (0x35))
0000 - 16 03 03 0030  14 00000c  [ed b9 e1 33 36 0b 76   ....0.......36.v >> FINISHED (0x14) [PRF 12 bytes|SHA256_OUTSIZE 32 bytes]
0010 - c0 d1 d4 0b a3|73 ec a8-fa b5 cb 12 b6 4c 2a b1   .....s.......L*.
0020 - fb 42 7f 73 0d 06 1c 87-56 f0 db df e6 6a 25 aa   .B.s....V....j%.
0030 - fc 42 38 cb 0b]                                    .B8..

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 16 03 03 00 aa                                    .....
read from 0x1d750b0 [0x1e6ac08] (170 bytes => 170 (0xAA))
0000 - 04 00 00 a6 00 00 1c 20-00 a0 dd f4 52 01 54 8d   ....... ....R.T. << NEW_SESSION_TICKET
0010 - f8 a6 f9 2d 7d 19 20 5b-14 44 d3 2d 7b f2 ca e8   ...-}. [.D.-{...
0020 - 01 4e 94 7b fe 12 59 3a-00 2e 7e cf 74 43 7a f7   .N.{..Y:..~.tCz.
0030 - 9e cc 70 80 70 7c e3 a5-c6 9d 85 2c 36 19 4c 5c   ..p.p|.....,6.L\
0040 - ba 3b c3 e5 69 dc f3 a4-47 38 11 c9 7d 1a b0 6e   .;..i...G8..}..n
0050 - d8 49 a0 a8 e4 de 70 a8-d0 6b e4 7a b7 65 25 df   .I....p..k.z.e%.
0060 - 1b 5f 64 0f 89 69 02 72-fe eb d3 7a af 51 78 0e   ._d..i.r...z.Qx.
0070 - de 17 06 a5 f0 47 9d e0-04 d4 b1 1e be 7e ed bd   .....G.......~..
0080 - 27 8f 5d e8 ac f6 45 aa-e0 12 93 41 5f a8 4b b9   '.]...E....A_.K.
0090 - bd 43 8f a1 23 51 af 92-77 8f 38 23 3e 2e c2 f0   .C..#Q..w.8#>...
00a0 - a3 74 fa 83 94 ce 19 8a-5b 5b                     .t......[[

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 14 03 03 00 01                                    .....            << CHANGE_CIPHER_SPEC
read from 0x1d750b0 [0x1e6ac08] (1 bytes => 1 (0x1))
0000 - 01                                                .

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 16 03 03 00 30                                    ....0
read from 0x1d750b0 [0x1e6ac08] (48 bytes => 48 (0x30))
0000 - 14 00000c  [06 86 0d 5c-92 0b 63 04 cc b4 f0 00   .......\..c..... << FINISHED (0x14) [PRF 12 bytes|SHA256_OUTSIZE 32 bytes]
0010 -|49 d6 dd 56 73 e3 d2 e8-22 d6 bd 61 b2 b3 af f0   I..Vs..."..a....
0020 - f5 00 8a 80 82 04 33 a7-50 8e ae 3b 4c 8c cf 4a]  ......3.P..;L..J
---
Certificate chain
 0 s:/CN=localhost
   i:/CN=localhost
---
Server certificate
-----BEGIN CERTIFICATE-----
...".......".......".......".......".......".......".......".......".....
-----END CERTIFICATE-----
subject=/CN=localhost
issuer=/CN=localhost
---
No client certificate CA names sent
---
SSL handshake has read 1346 bytes and written 553 bytes
---
New, TLSv1/SSLv3, Cipher is NULL-SHA256
Server public key is 3072 bit
Secure Renegotiation IS supported
Compression: NONE
Expansion: NONE
No ALPN negotiated
SSL-Session:
    Protocol  : TLSv1.2
    Cipher    : NULL-SHA256
    Session-ID: 5D62B36950F3DEB571707CD1B815E9E275041B9DB70D7F3E25C4A6535B13B616
    Session-ID-ctx:
    Master-Key: 4D08108C59417E0A41656636C51BA5B83F4EFFF9F4C860987B47B31250E5D1816D00940DBCCC196C2D99C8462C889DF1
    Key-Arg   : None
    Krb5 Principal: None
    PSK identity: None
    PSK identity hint: None
    TLS session ticket lifetime hint: 7200 (seconds)
    TLS session ticket:
    0000 - dd f4 52 01 54 8d f8 a6-f9 2d 7d 19 20 5b 14 44   ..R.T....-}. [.D
    0010 - d3 2d 7b f2 ca e8 01 4e-94 7b fe 12 59 3a 00 2e   .-{....N.{..Y:..
    0020 - 7e cf 74 43 7a f7 9e cc-70 80 70 7c e3 a5 c6 9d   ~.tCz...p.p|....
    0030 - 85 2c 36 19 4c 5c ba 3b-c3 e5 69 dc f3 a4 47 38   .,6.L\.;..i...G8
    0040 - 11 c9 7d 1a b0 6e d8 49-a0 a8 e4 de 70 a8 d0 6b   ..}..n.I....p..k
    0050 - e4 7a b7 65 25 df 1b 5f-64 0f 89 69 02 72 fe eb   .z.e%.._d..i.r..
    0060 - d3 7a af 51 78 0e de 17-06 a5 f0 47 9d e0 04 d4   .z.Qx......G....
    0070 - b1 1e be 7e ed bd 27 8f-5d e8 ac f6 45 aa e0 12   ...~..'.]...E...
    0080 - 93 41 5f a8 4b b9 bd 43-8f a1 23 51 af 92 77 8f   .A_.K..C..#Q..w.
    0090 - 38 23 3e 2e c2 f0 a3 74-fa 83 94 ce 19 8a 5b 5b   8#>....t......[[

    Start Time: 1484574330
    Timeout   : 7200 (sec)
    Verify return code: 18 (self signed certificate)
---
read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 17 03 03 00 21                                    ....!
read from 0x1d750b0 [0x1e6ac08] (33 bytes => 33 (0x21))
0000 - 0a 74 5b 50 02 13 75 a4-27 0a 40 b1 53 74 52 14   .t[P..u.'.@.StR.
0010 - e7 1e 6a 6c c1 60 2e 93-7e a5 d9 43 1d 8e f6 08   ..jl.`..~..C....
0020 - 69                                                i

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 17 03 03 00 21                                    ....!
read from 0x1d750b0 [0x1e6ac08] (33 bytes => 33 (0x21))
0000 - 0a 1b ce 44 98 4f 81 c5-28 7a cc 79 62 db d2 86   ...D.O..(z.yb...
0010 - 6a 55 a4 c7 73 49 ef 3e-bd 03 99 76 df 65 2a a1   jU..sI.>...v.e*.
0020 - b6                                                .

read from 0x1d750b0 [0x1e6ac03] (5 bytes => 5 (0x5))
0000 - 17 03 03 00 21                                    ....!
read from 0x1d750b0 [0x1e6ac08] (33 bytes => 33 (0x21))
0000 - 0a 67 66 34 ba 68 36 3c-ad 0a c1 f5 c0 5a 50 fe   .gf4.h6<.....ZP.
0010 - 68 cd 04 65 e9 de 6e 98-f9 e2 41 1e 0b 9b 84 06   h..e..n...A.....
0020 - 64                                                d
*/
