/* vi: set sw=4 ts=4: */
/*
 * wget - retrieve a file using HTTP or FTP
 *
 * Chip Rosenthal Covad Communications <chip@laserlink.net>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 *
 * Copyright (C) 2010 Bradley M. Kuhn <bkuhn@ebb.org>
 * Kuhn's copyrights are licensed GPLv2-or-later.  File as a whole remains GPLv2.
 */
#include "libbb.h"

struct host_info {
	// May be used if we ever will want to free() all xstrdup()s...
	/* char *allocated; */
	const char *path;
	const char *user;
	char       *host;
	int         port;
	smallint    is_ftp;
};


/* Globals */
struct globals {
	off_t content_len;        /* Content-length of the file */
	off_t beg_range;          /* Range at which continue begins */
#if ENABLE_FEATURE_WGET_STATUSBAR
	off_t transferred;        /* Number of bytes transferred so far */
	const char *curfile;      /* Name of current file being transferred */
	bb_progress_t pmt;
#endif
#if ENABLE_FEATURE_WGET_TIMEOUT
	unsigned timeout_seconds;
#endif
	smallint chunked;         /* chunked transfer encoding */
	smallint got_clen;        /* got content-length: from server  */
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
struct BUG_G_too_big {
	char BUG_G_too_big[sizeof(G) <= COMMON_BUFSIZE ? 1 : -1];
};
#define INIT_G() do { \
	IF_FEATURE_WGET_TIMEOUT(G.timeout_seconds = 900;) \
} while (0)


/* Must match option string! */
enum {
	WGET_OPT_CONTINUE   = (1 << 0),
	WGET_OPT_SPIDER     = (1 << 1),
	WGET_OPT_QUIET      = (1 << 2),
	WGET_OPT_OUTNAME    = (1 << 3),
	WGET_OPT_PREFIX     = (1 << 4),
	WGET_OPT_PROXY      = (1 << 5),
	WGET_OPT_USER_AGENT = (1 << 6),
	WGET_OPT_NETWORK_READ_TIMEOUT = (1 << 7),
	WGET_OPT_RETRIES    = (1 << 8),
	WGET_OPT_PASSIVE    = (1 << 9),
	WGET_OPT_HEADER     = (1 << 10) * ENABLE_FEATURE_WGET_LONG_OPTIONS,
	WGET_OPT_POST_DATA  = (1 << 11) * ENABLE_FEATURE_WGET_LONG_OPTIONS,
};

enum {
	PROGRESS_START = -1,
	PROGRESS_END   = 0,
	PROGRESS_BUMP  = 1,
};
#if ENABLE_FEATURE_WGET_STATUSBAR
static void progress_meter(int flag)
{
	if (option_mask32 & WGET_OPT_QUIET)
		return;

	if (flag == PROGRESS_START)
		bb_progress_init(&G.pmt);

	bb_progress_update(&G.pmt, G.curfile, G.beg_range, G.transferred,
			   G.chunked ? 0 : G.beg_range + G.transferred + G.content_len);

	if (flag == PROGRESS_END) {
		bb_putchar_stderr('\n');
		G.transferred = 0;
	}
}
#else
static ALWAYS_INLINE void progress_meter(int flag UNUSED_PARAM) { }
#endif


/* IPv6 knows scoped address types i.e. link and site local addresses. Link
 * local addresses can have a scope identifier to specify the
 * interface/link an address is valid on (e.g. fe80::1%eth0). This scope
 * identifier is only valid on a single node.
 *
 * RFC 4007 says that the scope identifier MUST NOT be sent across the wire,
 * unless all nodes agree on the semantic. Apache e.g. regards zone identifiers
 * in the Host header as invalid requests, see
 * https://issues.apache.org/bugzilla/show_bug.cgi?id=35122
 */
static void strip_ipv6_scope_id(char *host)
{
	char *scope, *cp;

	/* bbox wget actually handles IPv6 addresses without [], like
	 * wget "http://::1/xxx", but this is not standard.
	 * To save code, _here_ we do not support it. */

	if (host[0] != '[')
		return; /* not IPv6 */

	scope = strchr(host, '%');
	if (!scope)
		return;

	/* Remove the IPv6 zone identifier from the host address */
	cp = strchr(host, ']');
	if (!cp || (cp[1] != ':' && cp[1] != '\0')) {
		/* malformed address (not "[xx]:nn" or "[xx]") */
		return;
	}

	/* cp points to "]...", scope points to "%eth0]..." */
	overlapping_strcpy(scope, cp);
}

/* Read NMEMB bytes into PTR from STREAM.  Returns the number of bytes read,
 * and a short count if an eof or non-interrupt error is encountered.  */
static size_t safe_fread(void *ptr, size_t nmemb, FILE *stream)
{
	size_t ret;
	char *p = (char*)ptr;

	do {
		clearerr(stream);
		errno = 0;
		ret = fread(p, 1, nmemb, stream);
		p += ret;
		nmemb -= ret;
	} while (nmemb && ferror(stream) && errno == EINTR);

	return p - (char*)ptr;
}

/* Read a line or SIZE-1 bytes into S, whichever is less, from STREAM.
 * Returns S, or NULL if an eof or non-interrupt error is encountered.  */
static char *safe_fgets(char *s, int size, FILE *stream)
{
	char *ret;

	do {
		clearerr(stream);
		errno = 0;
		ret = fgets(s, size, stream);
	} while (ret == NULL && ferror(stream) && errno == EINTR);

	return ret;
}

#if ENABLE_FEATURE_WGET_AUTHENTICATION
/* Base64-encode character string. buf is assumed to be char buf[512]. */
static char *base64enc_512(char buf[512], const char *str)
{
	unsigned len = strlen(str);
	if (len > 512/4*3 - 10) /* paranoia */
		len = 512/4*3 - 10;
	bb_uuencode(buf, str, len, bb_uuenc_tbl_base64);
	return buf;
}
#endif

static char* sanitize_string(char *s)
{
	unsigned char *p = (void *) s;
	while (*p >= ' ')
		p++;
	*p = '\0';
	return s;
}

static FILE *open_socket(len_and_sockaddr *lsa)
{
	FILE *fp;

	/* glibc 2.4 seems to try seeking on it - ??! */
	/* hopefully it understands what ESPIPE means... */
	fp = fdopen(xconnect_stream(lsa), "r+");
	if (fp == NULL)
		bb_perror_msg_and_die("fdopen");

	return fp;
}

static int ftpcmd(const char *s1, const char *s2, FILE *fp, char *buf)
{
	int result;
	if (s1) {
		if (!s2) s2 = "";
		fprintf(fp, "%s%s\r\n", s1, s2);
		fflush(fp);
	}

	do {
		char *buf_ptr;

		if (fgets(buf, 510, fp) == NULL) {
			bb_perror_msg_and_die("error getting response");
		}
		buf_ptr = strstr(buf, "\r\n");
		if (buf_ptr) {
			*buf_ptr = '\0';
		}
	} while (!isdigit(buf[0]) || buf[3] != ' ');

	buf[3] = '\0';
	result = xatoi_positive(buf);
	buf[3] = ' ';
	return result;
}

static void parse_url(char *src_url, struct host_info *h)
{
	char *url, *p, *sp;

	/* h->allocated = */ url = xstrdup(src_url);

	if (strncmp(url, "http://", 7) == 0) {
		h->port = bb_lookup_port("http", "tcp", 80);
		h->host = url + 7;
		h->is_ftp = 0;
	} else if (strncmp(url, "ftp://", 6) == 0) {
		h->port = bb_lookup_port("ftp", "tcp", 21);
		h->host = url + 6;
		h->is_ftp = 1;
	} else
		bb_error_msg_and_die("not an http or ftp url: %s", sanitize_string(url));

	// FYI:
	// "Real" wget 'http://busybox.net?var=a/b' sends this request:
	//   'GET /?var=a/b HTTP 1.0'
	//   and saves 'index.html?var=a%2Fb' (we save 'b')
	// wget 'http://busybox.net?login=john@doe':
	//   request: 'GET /?login=john@doe HTTP/1.0'
	//   saves: 'index.html?login=john@doe' (we save '?login=john@doe')
	// wget 'http://busybox.net#test/test':
	//   request: 'GET / HTTP/1.0'
	//   saves: 'index.html' (we save 'test')
	//
	// We also don't add unique .N suffix if file exists...
	sp = strchr(h->host, '/');
	p = strchr(h->host, '?'); if (!sp || (p && sp > p)) sp = p;
	p = strchr(h->host, '#'); if (!sp || (p && sp > p)) sp = p;
	if (!sp) {
		h->path = "";
	} else if (*sp == '/') {
		*sp = '\0';
		h->path = sp + 1;
	} else { // '#' or '?'
		// http://busybox.net?login=john@doe is a valid URL
		// memmove converts to:
		// http:/busybox.nett?login=john@doe...
		memmove(h->host - 1, h->host, sp - h->host);
		h->host--;
		sp[-1] = '\0';
		h->path = sp;
	}

	// We used to set h->user to NULL here, but this interferes
	// with handling of code 302 ("object was moved")

	sp = strrchr(h->host, '@');
	if (sp != NULL) {
		h->user = h->host;
		*sp = '\0';
		h->host = sp + 1;
	}

	sp = h->host;
}

static char *gethdr(char *buf, size_t bufsiz, FILE *fp /*, int *istrunc*/)
{
	char *s, *hdrval;
	int c;

	/* *istrunc = 0; */

	/* retrieve header line */
	if (fgets(buf, bufsiz, fp) == NULL)
		return NULL;

	/* see if we are at the end of the headers */
	for (s = buf; *s == '\r'; ++s)
		continue;
	if (*s == '\n')
		return NULL;

	/* convert the header name to lower case */
	for (s = buf; isalnum(*s) || *s == '-' || *s == '.'; ++s) {
		/* tolower for "A-Z", no-op for "0-9a-z-." */
		*s = (*s | 0x20);
	}

	/* verify we are at the end of the header name */
	if (*s != ':')
		bb_error_msg_and_die("bad header line: %s", sanitize_string(buf));

	/* locate the start of the header value */
	*s++ = '\0';
	hdrval = skip_whitespace(s);

	/* locate the end of header */
	while (*s && *s != '\r' && *s != '\n')
		++s;

	/* end of header found */
	if (*s) {
		*s = '\0';
		return hdrval;
	}

	/* Rats! The buffer isn't big enough to hold the entire header value */
	while (c = getc(fp), c != EOF && c != '\n')
		continue;
	/* *istrunc = 1; */
	return hdrval;
}

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
static char *URL_escape(const char *str)
{
	/* URL encode, see RFC 2396 */
	char *dst;
	char *res = dst = xmalloc(strlen(str) * 3 + 1);
	unsigned char c;

	while (1) {
		c = *str++;
		if (c == '\0'
		/* || strchr("!&'()*-.=_~", c) - more code */
		 || c == '!'
		 || c == '&'
		 || c == '\''
		 || c == '('
		 || c == ')'
		 || c == '*'
		 || c == '-'
		 || c == '.'
		 || c == '='
		 || c == '_'
		 || c == '~'
		 || (c >= '0' && c <= '9')
		 || ((c|0x20) >= 'a' && (c|0x20) <= 'z')
		) {
			*dst++ = c;
			if (c == '\0')
				return res;
		} else {
			*dst++ = '%';
			*dst++ = bb_hexdigits_upcase[c >> 4];
			*dst++ = bb_hexdigits_upcase[c & 0xf];
		}
	}
}
#endif

static FILE* prepare_ftp_session(FILE **dfpp, struct host_info *target, len_and_sockaddr *lsa)
{
	char buf[512];
	FILE *sfp;
	char *str;
	int port;

	if (!target->user)
		target->user = xstrdup("anonymous:busybox@");

	sfp = open_socket(lsa);
	if (ftpcmd(NULL, NULL, sfp, buf) != 220)
		bb_error_msg_and_die("%s", sanitize_string(buf+4));

	/*
	 * Splitting username:password pair,
	 * trying to log in
	 */
	str = strchr(target->user, ':');
	if (str)
		*str++ = '\0';
	switch (ftpcmd("USER ", target->user, sfp, buf)) {
	case 230:
		break;
	case 331:
		if (ftpcmd("PASS ", str, sfp, buf) == 230)
			break;
		/* fall through (failed login) */
	default:
		bb_error_msg_and_die("ftp login: %s", sanitize_string(buf+4));
	}

	ftpcmd("TYPE I", NULL, sfp, buf);

	/*
	 * Querying file size
	 */
	if (ftpcmd("SIZE ", target->path, sfp, buf) == 213) {
		G.content_len = BB_STRTOOFF(buf+4, NULL, 10);
		if (G.content_len < 0 || errno) {
			bb_error_msg_and_die("SIZE value is garbage");
		}
		G.got_clen = 1;
	}

	/*
	 * Entering passive mode
	 */
	if (ftpcmd("PASV", NULL, sfp, buf) != 227) {
 pasv_error:
		bb_error_msg_and_die("bad response to %s: %s", "PASV", sanitize_string(buf));
	}
	// Response is "227 garbageN1,N2,N3,N4,P1,P2[)garbage]
	// Server's IP is N1.N2.N3.N4 (we ignore it)
	// Server's port for data connection is P1*256+P2
	str = strrchr(buf, ')');
	if (str) str[0] = '\0';
	str = strrchr(buf, ',');
	if (!str) goto pasv_error;
	port = xatou_range(str+1, 0, 255);
	*str = '\0';
	str = strrchr(buf, ',');
	if (!str) goto pasv_error;
	port += xatou_range(str+1, 0, 255) * 256;
	set_nport(lsa, htons(port));

	*dfpp = open_socket(lsa);

	if (G.beg_range) {
		sprintf(buf, "REST %"OFF_FMT"u", G.beg_range);
		if (ftpcmd(buf, NULL, sfp, buf) == 350)
			G.content_len -= G.beg_range;
	}

	if (ftpcmd("RETR ", target->path, sfp, buf) > 150)
		bb_error_msg_and_die("bad response to %s: %s", "RETR", sanitize_string(buf));

	return sfp;
}

static void NOINLINE retrieve_file_data(FILE *dfp, int output_fd)
{
	char buf[4*1024]; /* made bigger to speed up local xfers */
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
# if ENABLE_FEATURE_WGET_TIMEOUT
	unsigned second_cnt;
# endif
	struct pollfd polldata;

	polldata.fd = fileno(dfp);
	polldata.events = POLLIN | POLLPRI;
#endif
	progress_meter(PROGRESS_START);

	if (G.chunked)
		goto get_clen;

	/* Loops only if chunked */
	while (1) {

#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
		ndelay_on(polldata.fd);
#endif
		while (1) {
			int n;
			unsigned rdsz;

			rdsz = sizeof(buf);
			if (G.got_clen) {
				if (G.content_len < (off_t)sizeof(buf)) {
					if ((int)G.content_len <= 0)
						break;
					rdsz = (unsigned)G.content_len;
				}
			}
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
# if ENABLE_FEATURE_WGET_TIMEOUT
			second_cnt = G.timeout_seconds;
# endif
			while (1) {
				if (safe_poll(&polldata, 1, 1000) != 0)
					break; /* error, EOF, or data is available */
# if ENABLE_FEATURE_WGET_TIMEOUT
				if (second_cnt != 0 && --second_cnt == 0) {
					progress_meter(PROGRESS_END);
					bb_perror_msg_and_die("download timed out");
				}
# endif
				/* Needed for "stalled" indicator */
				progress_meter(PROGRESS_BUMP);
			}
#endif
			/* fread internally uses read loop, which in our case
			 * is usually exited when we get EAGAIN.
			 * In this case, libc sets error marker on the stream.
			 * Need to clear it before next fread to avoid possible
			 * rare false positive ferror below. Rare because usually
			 * fread gets more than zero bytes, and we don't fall
			 * into if (n <= 0) ...
			 */
			clearerr(dfp);
			errno = 0;
			n = safe_fread(buf, rdsz, dfp);
			/* man fread:
			 * If error occurs, or EOF is reached, the return value
			 * is a short item count (or zero).
			 * fread does not distinguish between EOF and error.
			 */
			if (n <= 0) {
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
				if (errno == EAGAIN) /* poll lied, there is no data? */
					continue; /* yes */
#endif
				if (ferror(dfp))
					bb_perror_msg_and_die(bb_msg_read_error);
				break; /* EOF, not error */
			}

			xwrite(output_fd, buf, n);
#if ENABLE_FEATURE_WGET_STATUSBAR
			G.transferred += n;
			progress_meter(PROGRESS_BUMP);
#endif
			if (G.got_clen) {
				G.content_len -= n;
				if (G.content_len == 0)
					break;
			}
		}
#if ENABLE_FEATURE_WGET_STATUSBAR || ENABLE_FEATURE_WGET_TIMEOUT
		ndelay_off(polldata.fd);
#endif

		if (!G.chunked)
			break;

		safe_fgets(buf, sizeof(buf), dfp); /* This is a newline */
 get_clen:
		safe_fgets(buf, sizeof(buf), dfp);
		G.content_len = STRTOOFF(buf, NULL, 16);
		/* FIXME: error check? */
		if (G.content_len == 0)
			break; /* all done! */
		G.got_clen = 1;
	}

	progress_meter(PROGRESS_END);
}

int wget_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int wget_main(int argc UNUSED_PARAM, char **argv)
{
	char buf[512];
	struct host_info server, target;
	len_and_sockaddr *lsa;
	unsigned opt;
	int redir_limit;
	char *proxy = NULL;
	char *dir_prefix = NULL;
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	char *post_data;
	char *extra_headers = NULL;
	llist_t *headers_llist = NULL;
#endif
	FILE *sfp;                      /* socket to web/ftp server         */
	FILE *dfp;                      /* socket to ftp server (data)      */
	char *fname_out;                /* where to direct output (-O)      */
	int output_fd = -1;
	bool use_proxy;                 /* Use proxies if env vars are set  */
	const char *proxy_flag = "on";  /* Use proxies if env vars are set  */
	const char *user_agent = "Wget";/* "User-Agent" header field        */

	static const char keywords[] ALIGN1 =
		"content-length\0""transfer-encoding\0""chunked\0""location\0";
	enum {
		KEY_content_length = 1, KEY_transfer_encoding, KEY_chunked, KEY_location
	};
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	static const char wget_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"continue\0"         No_argument       "c"
		"spider\0"           No_argument       "s"
		"quiet\0"            No_argument       "q"
		"output-document\0"  Required_argument "O"
		"directory-prefix\0" Required_argument "P"
		"proxy\0"            Required_argument "Y"
		"user-agent\0"       Required_argument "U"
#if ENABLE_FEATURE_WGET_TIMEOUT
		"timeout\0"          Required_argument "T"
#endif
		/* Ignored: */
		// "tries\0"            Required_argument "t"
		/* Ignored (we always use PASV): */
		"passive-ftp\0"      No_argument       "\xff"
		"header\0"           Required_argument "\xfe"
		"post-data\0"        Required_argument "\xfd"
		/* Ignored (we don't do ssl) */
		"no-check-certificate\0" No_argument   "\xfc"
		;
#endif

	INIT_G();

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	applet_long_options = wget_longopts;
#endif
	/* server.allocated = target.allocated = NULL; */
	opt_complementary = "-1" IF_FEATURE_WGET_TIMEOUT(":T+") IF_FEATURE_WGET_LONG_OPTIONS(":\xfe::");
	opt = getopt32(argv, "csqO:P:Y:U:T:" /*ignored:*/ "t:",
				&fname_out, &dir_prefix,
				&proxy_flag, &user_agent,
				IF_FEATURE_WGET_TIMEOUT(&G.timeout_seconds) IF_NOT_FEATURE_WGET_TIMEOUT(NULL),
				NULL /* -t RETRIES */
				IF_FEATURE_WGET_LONG_OPTIONS(, &headers_llist)
				IF_FEATURE_WGET_LONG_OPTIONS(, &post_data)
				);
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	if (headers_llist) {
		int size = 1;
		char *cp;
		llist_t *ll = headers_llist;
		while (ll) {
			size += strlen(ll->data) + 2;
			ll = ll->link;
		}
		extra_headers = cp = xmalloc(size);
		while (headers_llist) {
			cp += sprintf(cp, "%s\r\n", (char*)llist_pop(&headers_llist));
		}
	}
#endif

	/* TODO: compat issue: should handle "wget URL1 URL2..." */

	target.user = NULL;
	parse_url(argv[optind], &target);

	/* Use the proxy if necessary */
	use_proxy = (strcmp(proxy_flag, "off") != 0);
	if (use_proxy) {
		proxy = getenv(target.is_ftp ? "ftp_proxy" : "http_proxy");
		if (proxy && proxy[0]) {
			server.user = NULL;
			parse_url(proxy, &server);
		} else {
			use_proxy = 0;
		}
	}
	if (!use_proxy) {
		server.port = target.port;
		if (ENABLE_FEATURE_IPV6) {
			server.host = xstrdup(target.host);
		} else {
			server.host = target.host;
		}
	}

	if (ENABLE_FEATURE_IPV6)
		strip_ipv6_scope_id(target.host);

	/* Guess an output filename, if there was no -O FILE */
	if (!(opt & WGET_OPT_OUTNAME)) {
		fname_out = bb_get_last_path_component_nostrip(target.path);
		/* handle "wget http://kernel.org//" */
		if (fname_out[0] == '/' || !fname_out[0])
			fname_out = (char*)"index.html";
		/* -P DIR is considered only if there was no -O FILE */
		if (dir_prefix)
			fname_out = concat_path_file(dir_prefix, fname_out);
	} else {
		if (LONE_DASH(fname_out)) {
			/* -O - */
			output_fd = 1;
			opt &= ~WGET_OPT_CONTINUE;
		}
	}
#if ENABLE_FEATURE_WGET_STATUSBAR
	G.curfile = bb_get_last_path_component_nostrip(fname_out);
#endif

	/* Impossible?
	if ((opt & WGET_OPT_CONTINUE) && !fname_out)
		bb_error_msg_and_die("can't specify continue (-c) without a filename (-O)");
	*/

	/* Determine where to start transfer */
	if (opt & WGET_OPT_CONTINUE) {
		output_fd = open(fname_out, O_WRONLY);
		if (output_fd >= 0) {
			G.beg_range = xlseek(output_fd, 0, SEEK_END);
		}
		/* File doesn't exist. We do not create file here yet.
		 * We are not sure it exists on remove side */
	}

	redir_limit = 5;
 resolve_lsa:
	lsa = xhost2sockaddr(server.host, server.port);
	if (!(opt & WGET_OPT_QUIET)) {
		char *s = xmalloc_sockaddr2dotted(&lsa->u.sa);
		fprintf(stderr, "Connecting to %s (%s)\n", server.host, s);
		free(s);
	}
 establish_session:
	if (use_proxy || !target.is_ftp) {
		/*
		 *  HTTP session
		 */
		char *str;
		int status;

		/* Open socket to http server */
		sfp = open_socket(lsa);

		/* Send HTTP request */
		if (use_proxy) {
			fprintf(sfp, "GET %stp://%s/%s HTTP/1.1\r\n",
				target.is_ftp ? "f" : "ht", target.host,
				target.path);
		} else {
			if (opt & WGET_OPT_POST_DATA)
				fprintf(sfp, "POST /%s HTTP/1.1\r\n", target.path);
			else
				fprintf(sfp, "GET /%s HTTP/1.1\r\n", target.path);
		}

		fprintf(sfp, "Host: %s\r\nUser-Agent: %s\r\n",
			target.host, user_agent);

		/* Ask server to close the connection as soon as we are done
		 * (IOW: we do not intend to send more requests)
		 */
		fprintf(sfp, "Connection: close\r\n");

#if ENABLE_FEATURE_WGET_AUTHENTICATION
		if (target.user) {
			fprintf(sfp, "Proxy-Authorization: Basic %s\r\n"+6,
				base64enc_512(buf, target.user));
		}
		if (use_proxy && server.user) {
			fprintf(sfp, "Proxy-Authorization: Basic %s\r\n",
				base64enc_512(buf, server.user));
		}
#endif

		if (G.beg_range)
			fprintf(sfp, "Range: bytes=%"OFF_FMT"u-\r\n", G.beg_range);

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
		if (extra_headers)
			fputs(extra_headers, sfp);

		if (opt & WGET_OPT_POST_DATA) {
			char *estr = URL_escape(post_data);
			fprintf(sfp,
				"Content-Type: application/x-www-form-urlencoded\r\n"
				"Content-Length: %u\r\n"
				"\r\n"
				"%s",
				(int) strlen(estr), estr
			);
			free(estr);
		} else
#endif
		{
			fprintf(sfp, "\r\n");
		}

		fflush(sfp);

		/*
		 * Retrieve HTTP response line and check for "200" status code.
		 */
 read_response:
		if (fgets(buf, sizeof(buf), sfp) == NULL)
			bb_error_msg_and_die("no response from server");

		str = buf;
		str = skip_non_whitespace(str);
		str = skip_whitespace(str);
		// FIXME: no error check
		// xatou wouldn't work: "200 OK"
		status = atoi(str);
		switch (status) {
		case 0:
		case 100:
			while (gethdr(buf, sizeof(buf), sfp /*, &n*/) != NULL)
				/* eat all remaining headers */;
			goto read_response;
		case 200:
/*
Response 204 doesn't say "null file", it says "metadata
has changed but data didn't":

"10.2.5 204 No Content
The server has fulfilled the request but does not need to return
an entity-body, and might want to return updated metainformation.
The response MAY include new or updated metainformation in the form
of entity-headers, which if present SHOULD be associated with
the requested variant.

If the client is a user agent, it SHOULD NOT change its document
view from that which caused the request to be sent. This response
is primarily intended to allow input for actions to take place
without causing a change to the user agent's active document view,
although any new or updated metainformation SHOULD be applied
to the document currently in the user agent's active view.

The 204 response MUST NOT include a message-body, and thus
is always terminated by the first empty line after the header fields."

However, in real world it was observed that some web servers
(e.g. Boa/0.94.14rc21) simply use code 204 when file size is zero.
*/
		case 204:
			break;
		case 300:  /* redirection */
		case 301:
		case 302:
		case 303:
			break;
		case 206:
			if (G.beg_range)
				break;
			/* fall through */
		default:
			bb_error_msg_and_die("server returned error: %s", sanitize_string(buf));
		}

		/*
		 * Retrieve HTTP headers.
		 */
		while ((str = gethdr(buf, sizeof(buf), sfp /*, &n*/)) != NULL) {
			/* gethdr converted "FOO:" string to lowercase */
			smalluint key;
			/* strip trailing whitespace */
			char *s = strchrnul(str, '\0') - 1;
			while (s >= str && (*s == ' ' || *s == '\t')) {
				*s = '\0';
				s--;
			}
			key = index_in_strings(keywords, buf) + 1;
			if (key == KEY_content_length) {
				G.content_len = BB_STRTOOFF(str, NULL, 10);
				if (G.content_len < 0 || errno) {
					bb_error_msg_and_die("content-length %s is garbage", sanitize_string(str));
				}
				G.got_clen = 1;
				continue;
			}
			if (key == KEY_transfer_encoding) {
				if (index_in_strings(keywords, str_tolower(str)) + 1 != KEY_chunked)
					bb_error_msg_and_die("transfer encoding '%s' is not supported", sanitize_string(str));
				G.chunked = G.got_clen = 1;
			}
			if (key == KEY_location && status >= 300) {
				if (--redir_limit == 0)
					bb_error_msg_and_die("too many redirections");
				fclose(sfp);
				G.got_clen = 0;
				G.chunked = 0;
				if (str[0] == '/')
					/* free(target.allocated); */
					target.path = /* target.allocated = */ xstrdup(str+1);
					/* lsa stays the same: it's on the same server */
				else {
					parse_url(str, &target);
					if (!use_proxy) {
						server.host = target.host;
						/* strip_ipv6_scope_id(target.host); - no! */
						/* we assume remote never gives us IPv6 addr with scope id */
						server.port = target.port;
						free(lsa);
						goto resolve_lsa;
					} /* else: lsa stays the same: we use proxy */
				}
				goto establish_session;
			}
		}
//		if (status >= 300)
//			bb_error_msg_and_die("bad redirection (no Location: header from server)");

		/* For HTTP, data is pumped over the same connection */
		dfp = sfp;

	} else {
		/*
		 *  FTP session
		 */
		sfp = prepare_ftp_session(&dfp, &target, lsa);
	}

	if (opt & WGET_OPT_SPIDER) {
		if (ENABLE_FEATURE_CLEAN_UP)
			fclose(sfp);
		return EXIT_SUCCESS;
	}

	if (output_fd < 0) {
		int o_flags = O_WRONLY | O_CREAT | O_TRUNC | O_EXCL;
		/* compat with wget: -O FILE can overwrite */
		if (opt & WGET_OPT_OUTNAME)
			o_flags = O_WRONLY | O_CREAT | O_TRUNC;
		output_fd = xopen(fname_out, o_flags);
	}

	retrieve_file_data(dfp, output_fd);
	xclose(output_fd);

	if (dfp != sfp) {
		/* It's ftp. Close it properly */
		fclose(dfp);
		if (ftpcmd(NULL, NULL, sfp, buf) != 226)
			bb_error_msg_and_die("ftp error: %s", sanitize_string(buf+4));
		/* ftpcmd("QUIT", NULL, sfp, buf); - why bother? */
	}

	return EXIT_SUCCESS;
}
