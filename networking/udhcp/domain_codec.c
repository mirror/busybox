/* vi: set sw=4 ts=4: */

/* RFC1035 domain compression routines (C) 2007 Gabriel Somlo <somlo at cmu.edu>
 *
 * Loosely based on the isc-dhcpd implementation by dhankins@isc.org
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#if ENABLE_FEATURE_RFC3397

#include "common.h"
#include "options.h"

#define NS_MAXDNAME  1025	/* max domain name length */
#define NS_MAXCDNAME  255	/* max compressed domain name length */
#define NS_MAXLABEL    63	/* max label length */
#define NS_MAXDNSRCH    6	/* max domains in search path */
#define NS_CMPRSFLGS 0xc0	/* name compression pointer flag */


/* expand a RFC1035-compressed list of domain names "cstr", of length "clen";
 * returns a newly allocated string containing the space-separated domains,
 * prefixed with the contents of string pre, or NULL if an error occurs.
 */
char *dname_dec(const uint8_t *cstr, int clen, const char *pre)
{
	const uint8_t *c;
	int crtpos, retpos, depth, plen = 0, len = 0;
	char *dst = NULL;

	if (!cstr)
		return NULL;

	if (pre)
		plen = strlen(pre);

	/* We make two passes over the cstr string. First, we compute
	 * how long the resulting string would be. Then we allocate a
	 * new buffer of the required length, and fill it in with the
	 * expanded content. The advantage of this approach is not
	 * having to deal with requiring callers to supply their own
	 * buffer, then having to check if it's sufficiently large, etc.
	 */

	while (!dst) {

		if (len > 0) {	/* second pass? allocate dst buffer and copy pre */
			dst = xmalloc(len + plen);
			memcpy(dst, pre, plen);
		}

		crtpos = retpos = depth = len = 0;

		while (crtpos < clen) {
			c = cstr + crtpos;

			if ((*c & NS_CMPRSFLGS) != 0) {	/* pointer */
				if (crtpos + 2 > clen)		/* no offset to jump to? abort */
					return NULL;
				if (retpos == 0)			/* toplevel? save return spot */
					retpos = crtpos + 2;
				depth++;
				crtpos = ((*c & 0x3f) << 8) | (*(c + 1) & 0xff); /* jump */
			} else if (*c) {			/* label */
				if (crtpos + *c + 1 > clen)		/* label too long? abort */
					return NULL;
				if (dst)
					memcpy(dst + plen + len, c + 1, *c);
				len += *c + 1;
				crtpos += *c + 1;
				if (dst)
					*(dst + plen + len - 1) = '.';
			} else {					/* null: end of current domain name */
				if (retpos == 0) {			/* toplevel? keep going */
					crtpos++;
				} else {					/* return to toplevel saved spot */
					crtpos = retpos;
					retpos = depth = 0;
				}
				if (dst)
					*(dst + plen + len - 1) = ' ';
			}

			if (depth > NS_MAXDNSRCH || /* too many jumps? abort, it's a loop */
				len > NS_MAXDNAME * NS_MAXDNSRCH) /* result too long? abort */
				return NULL;
		}

		if (!len)			/* expanded string has 0 length? abort */
			return NULL;

		if (dst)
			*(dst + plen + len - 1) = '\0';
	}

	return dst;
}

/* Convert a domain name (src) from human-readable "foo.blah.com" format into
 * RFC1035 encoding "\003foo\004blah\003com\000". Return allocated string, or
 * NULL if an error occurs.
 */
static uint8_t *convert_dname(const char *src)
{
	uint8_t c, *res, *lp, *rp;
	int len;

	res = xmalloc(strlen(src) + 2);
	rp = lp = res;
	rp++;

	for (;;) {
		c = (uint8_t)*src++;
		if (c == '.' || c == '\0') {	/* end of label */
			len = rp - lp - 1;
			/* label too long, too short, or two '.'s in a row? abort */
			if (len > NS_MAXLABEL || len == 0 || (c == '.' && *src == '.')) {
				free(res);
				return NULL;
			}
			*lp = len;
			lp = rp++;
			if (c == '\0' || *src == '\0')	/* end of dname */
				break;
		} else {
			if (c >= 0x41 && c <= 0x5A)		/* uppercase? convert to lower */
				c += 0x20;
			*rp++ = c;
		}
	}

	*lp = 0;
	if (rp - res > NS_MAXCDNAME) {	/* dname too long? abort */
		free(res);
		return NULL;
	}
	return res;
}

/* returns the offset within cstr at which dname can be found, or -1
 */
static int find_offset(const uint8_t *cstr, int clen, const uint8_t *dname)
{
	const uint8_t *c, *d;
	int off, inc;

	/* find all labels in cstr */
	off = 0;
	while (off < clen) {
		c = cstr + off;

		if ((*c & NS_CMPRSFLGS) != 0) {	/* pointer, skip */
			off += 2;
		} else if (*c) {	/* label, try matching dname */
			inc = *c + 1;
			d = dname;
			while (*c == *d && memcmp(c + 1, d + 1, *c) == 0) {
				if (*c == 0)	/* match, return offset */
					return off;
				d += *c + 1;
				c += *c + 1;
				if ((*c & NS_CMPRSFLGS) != 0)	/* pointer, jump */
					c = cstr + (((*c & 0x3f) << 8) | (*(c + 1) & 0xff));
			}
			off += inc;
		} else {	/* null, skip */
			off++;
		}
	}

	return -1;
}

/* computes string to be appended to cstr so that src would be added to
 * the compression (best case, it's a 2-byte pointer to some offset within
 * cstr; worst case, it's all of src, converted to rfc3011 format).
 * The computed string is returned directly; its length is returned via retlen;
 * NULL and 0, respectively, are returned if an error occurs.
 */
uint8_t *dname_enc(const uint8_t *cstr, int clen, const char *src, int *retlen)
{
	uint8_t *d, *dname;
	int off;

	dname = convert_dname(src);
	if (dname == NULL) {
		*retlen = 0;
		return NULL;
	}

	for (d = dname; *d != 0; d += *d + 1) {
		off = find_offset(cstr, clen, d);
		if (off >= 0) {	/* found a match, add pointer and terminate string */
			*d++ = NS_CMPRSFLGS;
			*d = off;
			break;
		}
	}

	*retlen = d - dname + 1;
	return dname;
}

#endif /* ENABLE_FEATURE_RFC3397 */
