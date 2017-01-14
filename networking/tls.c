/*
 * Licensed under GPLv2, see file LICENSE in this source tree.
 *
 * Copyright (C) 2017 Denys Vlasenko
 */
//config:config TLS
//config:	bool "tls (debugging)"
//config:	default n

//applet:IF_TLS(APPLET(tls, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TLS) += tls.o
////kbuild:lib-$(CONFIG_TLS) += tls_ciphers.o
////kbuild:lib-$(CONFIG_TLS) += tls_aes.o
////kbuild:lib-$(CONFIG_TLS) += tls_aes_gcm.o

//usage:#define tls_trivial_usage
//usage:       "HOST[:PORT]"
//usage:#define tls_full_usage "\n\n"

#include "libbb.h"
//#include "tls_cryptoapi.h"
//#include "tls_ciphers.h"

#if 1
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

#define RECORD_TYPE_CHANGE_CIPHER_SPEC 20
#define RECORD_TYPE_ALERT              21
#define RECORD_TYPE_HANDSHAKE          22
#define RECORD_TYPE_APPLICATION_DATA   23

#define HANDSHAKE_HELLO_REQUEST        0
#define HANDSHAKE_CLIENT_HELLO         1
#define HANDSHAKE_SERVER_HELLO         2
#define HANDSHAKE_HELLO_VERIFY_REQUEST 3
#define HANDSHAKE_NEW_SESSION_TICKET   4
#define HANDSHAKE_CERTIFICATE          11
#define HANDSHAKE_SERVER_KEY_EXCHANGE  12
#define HANDSHAKE_CERTIFICATE_REQUEST  13
#define HANDSHAKE_SERVER_HELLO_DONE    14
#define HANDSHAKE_CERTIFICATE_VERIFY   15
#define HANDSHAKE_CLIENT_KEY_EXCHANGE  16
#define HANDSHAKE_FINISHED             20

#define SSL_NULL_WITH_NULL_NULL                 0x0000
#define SSL_RSA_WITH_NULL_MD5                   0x0001
#define SSL_RSA_WITH_NULL_SHA                   0x0002
#define SSL_RSA_WITH_RC4_128_MD5                0x0004
#define SSL_RSA_WITH_RC4_128_SHA                0x0005
#define SSL_RSA_WITH_3DES_EDE_CBC_SHA           0x000A  /* 10 */
#define TLS_RSA_WITH_AES_128_CBC_SHA            0x002F  /* 47 */
#define TLS_RSA_WITH_AES_256_CBC_SHA            0x0035  /* 53 */
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
#define CIPHER_ID TLS_RSA_WITH_AES_128_GCM_SHA256 // ok, no SERVER_KEY_EXCHANGE
//#define CIPHER_ID TLS_DH_anon_WITH_AES_256_CBC_SHA // SSL_ALERT_HANDSHAKE_FAILURE
// (tested b/c this one doesn't req server certs... no luck)
//test TLS_RSA_WITH_AES_128_CBC_SHA, in tls 1.2 it's mandated to be always supported

struct record_hdr {
	uint8_t type;
	uint8_t proto_maj, proto_min;
	uint8_t len16_hi, len16_lo;
};

typedef struct tls_state {
	int fd;

	uint8_t *pubkey;
	int pubkey_len;

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
	uint8_t inbuf[18*1024];
} tls_state_t;

static
tls_state_t *new_tls_state(void)
{
	tls_state_t *tls = xzalloc(sizeof(*tls));
	tls->fd = -1;
	return tls;
}

static unsigned get24be(const uint8_t *p)
{
	return 0x100*(0x100*p[0] + p[1]) + p[2];
}

static void dump(const void *vp, int len)
{
	char hexbuf[32 * 1024 + 4];
	const uint8_t *p = vp;

	while (len > 0) {
		unsigned xhdr_len;
		if (len < 5) {
			bin2hex(hexbuf, (void*)p, len)[0] = '\0';
			dbg("< |%s|\n", hexbuf);
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
		bin2hex(hexbuf, (void*)p, xhdr_len)[0] = '\0';
		dbg(" |%s|\n", hexbuf);
		p += xhdr_len;
		len -= xhdr_len;
	}
}

static void tls_error_die(tls_state_t *tls)
{
	dump(tls->inbuf, tls->insize + tls->tail);
	xfunc_die();
}

static int xread_tls_block(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	int len;
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
		len = safe_read(tls->fd, tls->inbuf + total, sizeof(tls->inbuf) - total);
		if (len <= 0)
			bb_perror_msg_and_die("short read");
		total += len;
	}
	tls->tail = total - target;
	tls->insize = target;
	target -= sizeof(*xhdr);
	dbg("got block len:%u\n", target);
	return target;
}

static void send_client_hello(tls_state_t *tls)
{
	struct client_hello {
		struct record_hdr xhdr;
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
	struct client_hello hello;

	memset(&hello, 0, sizeof(hello));
	hello.xhdr.type = RECORD_TYPE_HANDSHAKE;
	hello.xhdr.proto_maj = TLS_MAJ;
	hello.xhdr.proto_min = TLS_MIN;
	//zero: hello.xhdr.len16_hi = (sizeof(hello) - sizeof(hello.xhdr)) >> 8;
	hello.xhdr.len16_lo = (sizeof(hello) - sizeof(hello.xhdr));
	hello.type = HANDSHAKE_CLIENT_HELLO;
	//hello.len24_hi  = 0;
	//zero: hello.len24_mid = (sizeof(hello) - sizeof(hello.xhdr) - 4) >> 8;
	hello.len24_lo  = (sizeof(hello) - sizeof(hello.xhdr) - 4);
	hello.proto_maj = TLS_MAJ;
	hello.proto_min = TLS_MIN;
	open_read_close("/dev/urandom", hello.rand32, sizeof(hello.rand32));
	//hello.session_id_len = 0;
	//hello.cipherid_len16_hi = 0;
	hello.cipherid_len16_lo = 2 * 1;
	hello.cipherid[0] = CIPHER_ID >> 8;
	hello.cipherid[1] = CIPHER_ID & 0xff;
	hello.comprtypes_len = 1;
	//hello.comprtypes[0] = 0;

	xwrite(tls->fd, &hello, sizeof(hello));
}

static void get_server_hello_or_die(tls_state_t *tls)
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
	int len;

	len = xread_tls_block(tls);

	hp = (void*)tls->inbuf;
	if (len != 74 /* TODO: if we accept extensions, should be < instead of != */
	 || hp->xhdr.type != RECORD_TYPE_HANDSHAKE
	 || hp->xhdr.proto_maj != TLS_MAJ
	 || hp->xhdr.proto_min != TLS_MIN
	) {
		/* example: RECORD_TYPE_ALERT if server can't support our ciphers */
		tls_error_die(tls);
	}
	dbg("got HANDSHAKE\n");
	// 74 bytes:
	// 02  000046 03|03   58|78|cf|c1 50|a5|49|ee|7e|29|48|71|fe|97|fa|e8|2d|19|87|72|90|84|9d|37|a3|f0|cb|6f|5f|e3|3c|2f |20  |d8|1a|78|96|52|d6|91|01|24|b3|d6|5b|b7|d0|6c|b3|e1|78|4e|3c|95|de|74|a0|ba|eb|a7|3a|ff|bd|a2|bf |00|9c |00|
	//SvHl len=70 maj.min unixtime^^^ 28randbytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^_^^^ slen sid32bytes^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ cipSel comprSel
	if (hp->type != HANDSHAKE_SERVER_HELLO
	 || hp->len24_hi  != 0
	 || hp->len24_mid != 0
	 || hp->len24_lo  != 70
	 || hp->proto_maj != TLS_MAJ
	 || hp->proto_min != TLS_MIN
	 || hp->session_id_len != 32
	 || hp->cipherid_hi != (CIPHER_ID >> 8)
	 || hp->cipherid_lo != (CIPHER_ID & 0xff)
	 || hp->comprtype != 0
	) {
		tls_error_die(tls);
	}
	dbg("got SERVER_HELLO\n");
}

static unsigned get_der_len(uint8_t **bodyp, uint8_t *der, uint8_t *end)
{
	unsigned len;

	if (end - der < 2)
		xfunc_die();
//	if ((der[0] & 0x1f) == 0x1f) /* not single-byte item code? */
//		xfunc_die();

	len = der[1]; /* maybe it's short len */
	if (len >= 0x80) {
		/* no */
		if (len != 0x82) {
			/* 0x80 is "0 bytes of len", invalid DER: must use short len if can */
			/* 0x81 is "1 byte of len", invalid DER */
			/* >0x82 is "3+ bytes of len", should not happen realistically */
			xfunc_die();
		}
		if (end - der < 4)
			xfunc_die();
		/* it's "ii 82 xx yy" */
		len = 0x100*der[2] + der[3];
//		if (len < 0x80)
//			xfunc_die(); /* invalid DER: must use short len if can */

		der += 2; /* skip [code]+[82]+[2byte_len] */
	}
	der += 2; /* skip [code]+[1byte_len] */

	if (end - der < len)
		xfunc_die();
	*bodyp = der;

	return len;
}

static uint8_t *enter_der_item(uint8_t *der, uint8_t **endp)
{
	uint8_t *new_der;
	unsigned len = get_der_len(&new_der, der, *endp);
	dbg("entered der @%p:0x%02x len:%u inner_byte @%p:0x%02x\n", der, der[0], len, new_der, new_der[0]);
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
	dbg("skipped der 0x%02x, next byte 0x%02x\n", der[0], new_der[0]);
	return new_der;
}

static void *find_key_in_der_cert(int *key_len, uint8_t *der, int len)
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

	/* enter "subjectPublicKeyInfo" */
	der = enter_der_item(der, &end);

	/* skip "subjectPublicKeyInfo.algorithm" */
	der = skip_der_item(der, end);
	/* enter "subjectPublicKeyInfo.publicKey" */
//	die_if_not_this_der_type(der, end, 0x03); /* must be BITSTRING */
	der = enter_der_item(der, &end);

	/* return a copy */
	*key_len = end - der;
	dbg("copying key bytes:%u, first:0x%02x\n", *key_len, der[0]);
	return xmemdup(der, *key_len);
}

static void get_server_cert_or_die(tls_state_t *tls)
{
	struct record_hdr *xhdr;
	uint8_t *certbuf;
	int len, len1;

	len = xread_tls_block(tls);
	xhdr = (void*)tls->inbuf;
	if (len < sizeof(*xhdr) + 10
	 || xhdr->type != RECORD_TYPE_HANDSHAKE
	 || xhdr->proto_maj != TLS_MAJ
	 || xhdr->proto_min != TLS_MIN
	) {
		tls_error_die(tls);
	}
	dbg("got HANDSHAKE\n");
	certbuf = (void*)(xhdr + 1);
	if (certbuf[0] != HANDSHAKE_CERTIFICATE)
		tls_error_die(tls);
	dbg("got CERTIFICATE\n");
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
		tls->pubkey = find_key_in_der_cert(&tls->pubkey_len, certbuf + 10, len);
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
#if 0 /* dump */
	for (;;) {
		uint8_t buf[16*1024];
		sleep(2);
		len = recv(tls->fd, buf, sizeof(buf), 0); //MSG_DONTWAIT);
		if (len < 0) {
			if (errno == EAGAIN)
				continue;
			bb_perror_msg_and_die("recv");
		}
		if (len == 0)
			break;
		dump(buf, len);
	}
#endif

	get_server_hello_or_die(tls);

	//RFC 5246
	// The server MUST send a Certificate message whenever the agreed-
	// upon key exchange method uses certificates for authentication
	// (this includes all key exchange methods defined in this document
	// except DH_anon).  This message will always immediately follow the
	// ServerHello message.
	//
	// IOW: in practice, Certificate *always* follows.
	// (for example, kernel.org does not even accept DH_anon cipher id)
	get_server_cert_or_die(tls);

	len = xread_tls_block(tls);
	/* Next handshake type is not predetermined */
	switch (tls->inbuf[5]) {
	case HANDSHAKE_SERVER_KEY_EXCHANGE:
		// 459 bytes:
		// 0c   00|01|c7 03|00|17|41|04|87|94|2e|2f|68|d0|c9|f4|97|a8|2d|ef|ed|67|ea|c6|f3|b3|56|47|5d|27|b6|bd|ee|70|25|30|5e|b0|8e|f6|21|5a...
		//SvKey len=455^
		dbg("got SERVER_KEY_EXCHANGE\n");
		len = xread_tls_block(tls);
		break;
	case HANDSHAKE_CERTIFICATE_REQUEST:
		dbg("got CERTIFICATE_REQUEST\n");
		len = xread_tls_block(tls);
		break;
	case HANDSHAKE_SERVER_HELLO_DONE:
		// 0e 000000 (len:0)
		dbg("got SERVER_HELLO_DONE\n");
		break;
	default:
		tls_error_die(tls);
	}
}

int tls_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tls_main(int argc UNUSED_PARAM, char **argv)
{
	tls_state_t *tls;
	len_and_sockaddr *lsa;
	int fd;

	// INIT_G();
	// getopt32(argv, "myopts")

	if (!argv[1])
		bb_show_usage();

	lsa = xhost2sockaddr(argv[1], 443);
	fd = xconnect_stream(lsa);

	tls = new_tls_state();
	tls->fd = fd;
	tls_handshake(tls);

	return EXIT_SUCCESS;
}
