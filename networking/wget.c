/* vi: set sw=4 ts=4: */
/*
 * wget - retrieve a file using HTTP or FTP
 *
 * Chip Rosenthal Covad Communications <chip@laserlink.net>
 *
 */

/* We want libc to give us xxx64 functions also */
/* http://www.unix.org/version2/whatsnew/lfs20mar.html */
#define _LARGEFILE64_SOURCE 1

#include "busybox.h"
#include <getopt.h>	/* for struct option */

struct host_info {
	// May be used if we ever will want to free() all xstrdup()s...
	/* char *allocated; */
	char *host;
	int port;
	char *path;
	int is_ftp;
	char *user;
};

static void parse_url(char *url, struct host_info *h);
static FILE *open_socket(len_and_sockaddr *lsa);
static char *gethdr(char *buf, size_t bufsiz, FILE *fp, int *istrunc);
static int ftpcmd(const char *s1, const char *s2, FILE *fp, char *buf);

/* Globals (can be accessed from signal handlers */
static off_t content_len;        /* Content-length of the file */
static off_t beg_range;          /* Range at which continue begins */
#if ENABLE_FEATURE_WGET_STATUSBAR
static off_t transferred;        /* Number of bytes transferred so far */
#endif
static int chunked;                     /* chunked transfer encoding */
#if ENABLE_FEATURE_WGET_STATUSBAR
static void progressmeter(int flag);
static const char *curfile;             /* Name of current file being transferred */
static struct timeval start;            /* Time a transfer started */
enum {
	STALLTIME = 5                   /* Seconds when xfer considered "stalled" */
};
#else
static void progressmeter(int flag) {}
#endif

/* Read NMEMB elements of SIZE bytes into PTR from STREAM.  Returns the
 * number of elements read, and a short count if an eof or non-interrupt
 * error is encountered.  */
static size_t safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret = 0;

	do {
		clearerr(stream);
		ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
	} while (ret < nmemb && ferror(stream) && errno == EINTR);

	return ret;
}

/* Read a line or SIZE - 1 bytes into S, whichever is less, from STREAM.
 * Returns S, or NULL if an eof or non-interrupt error is encountered.  */
static char *safe_fgets(char *s, int size, FILE *stream)
{
	char *ret;

	do {
		clearerr(stream);
		ret = fgets(s, size, stream);
	} while (ret == NULL && ferror(stream) && errno == EINTR);

	return ret;
}

#if ENABLE_FEATURE_WGET_AUTHENTICATION
/*
 *  Base64-encode character string and return the string.
 */
static char *base64enc(unsigned char *p, char *buf, int len)
{
	bb_uuencode(p, buf, len, bb_uuenc_tbl_base64);
	return buf;
}
#endif

int wget_main(int argc, char **argv);
int wget_main(int argc, char **argv)
{
	char buf[512];
	struct host_info server, target;
	len_and_sockaddr *lsa;
	int n, status;
	int port;
	int try = 5;
	unsigned opt;
	char *s;
	char *proxy = 0;
	char *dir_prefix = NULL;
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	char *extra_headers = NULL;
	llist_t *headers_llist = NULL;
#endif

	/* server.allocated = target.allocated = NULL; */

	FILE *sfp = NULL;               /* socket to web/ftp server         */
	FILE *dfp = NULL;               /* socket to ftp server (data)      */
	char *fname_out = NULL;         /* where to direct output (-O)      */
	int got_clen = 0;               /* got content-length: from server  */
	int output_fd = -1;
	int use_proxy = 1;              /* Use proxies if env vars are set  */
	const char *proxy_flag = "on";  /* Use proxies if env vars are set  */
	const char *user_agent = "Wget";/* Content of the "User-Agent" header field */

	/*
	 * Crack command line.
	 */
	enum {
		WGET_OPT_CONTINUE   = 0x1,
		WGET_OPT_QUIET      = 0x2,
		WGET_OPT_OUTNAME    = 0x4,
		WGET_OPT_PREFIX     = 0x8,
		WGET_OPT_PROXY      = 0x10,
		WGET_OPT_USER_AGENT = 0x20,
		WGET_OPT_PASSIVE    = 0x40,
		WGET_OPT_HEADER     = 0x80,
	};
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	static const struct option wget_long_options[] = {
		// name, has_arg, flag, val
		{ "continue",         no_argument, NULL, 'c' },
		{ "quiet",            no_argument, NULL, 'q' },
		{ "output-document",  required_argument, NULL, 'O' },
		{ "directory-prefix", required_argument, NULL, 'P' },
		{ "proxy",            required_argument, NULL, 'Y' },
		{ "user-agent",       required_argument, NULL, 'U' },
		{ "passive-ftp",      no_argument, NULL, 0xff },
		{ "header",           required_argument, NULL, 0xfe },
		{ 0, 0, 0, 0 }
	};
	applet_long_options = wget_long_options;
#endif
	opt_complementary = "-1" USE_FEATURE_WGET_LONG_OPTIONS(":\xfe::");
	opt = getopt32(argc, argv, "cqO:P:Y:U:",
				&fname_out, &dir_prefix,
				&proxy_flag, &user_agent
				USE_FEATURE_WGET_LONG_OPTIONS(, &headers_llist)
				);
	if (strcmp(proxy_flag, "off") == 0) {
		/* Use the proxy if necessary. */
		use_proxy = 0;
	}
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	if (headers_llist) {
		int size = 1;
		char *cp;
		llist_t *ll = headers_llist = llist_rev(headers_llist);
		while (ll) {
			size += strlen(ll->data) + 2;
			ll = ll->link;
		}
		extra_headers = cp = xmalloc(size);
		while (headers_llist) {
			cp += sprintf(cp, "%s\r\n", headers_llist->data);
			headers_llist = headers_llist->link;
		}
	}
#endif

	parse_url(argv[optind], &target);
	server.host = target.host;
	server.port = target.port;

	/*
	 * Use the proxy if necessary.
	 */
	if (use_proxy) {
		proxy = getenv(target.is_ftp ? "ftp_proxy" : "http_proxy");
		if (proxy && *proxy) {
			parse_url(proxy, &server);
		} else {
			use_proxy = 0;
		}
	}

	/* Guess an output filename */
	if (!fname_out) {
		// Dirty hack. Needed because bb_get_last_path_component
		// will destroy trailing / by storing '\0' in last byte!
		if (!last_char_is(target.path, '/')) {
			fname_out = bb_get_last_path_component(target.path);
#if ENABLE_FEATURE_WGET_STATUSBAR
			curfile = fname_out;
#endif
		}
		if (!fname_out || !fname_out[0]) {
			/* bb_get_last_path_component writes
			 * to last '/' only. We don't have one here... */
			fname_out = (char*)"index.html";
#if ENABLE_FEATURE_WGET_STATUSBAR
			curfile = fname_out;
#endif
		}
		if (dir_prefix != NULL)
			fname_out = concat_path_file(dir_prefix, fname_out);
#if ENABLE_FEATURE_WGET_STATUSBAR
	} else {
		curfile = bb_get_last_path_component(fname_out);
#endif
	}
	/* Impossible?
	if ((opt & WGET_OPT_CONTINUE) && !fname_out)
		bb_error_msg_and_die("cannot specify continue (-c) without a filename (-O)"); */

	/*
	 * Determine where to start transfer.
	 */
	if (LONE_DASH(fname_out)) {
		output_fd = 1;
		opt &= ~WGET_OPT_CONTINUE;
	}
	if (opt & WGET_OPT_CONTINUE) {
		output_fd = open(fname_out, O_WRONLY);
		if (output_fd >= 0) {
			beg_range = xlseek(output_fd, 0, SEEK_END);
		}
		/* File doesn't exist. We do not create file here yet.
		   We are not sure it exists on remove side */
	}

	/* We want to do exactly _one_ DNS lookup, since some
	 * sites (i.e. ftp.us.debian.org) use round-robin DNS
	 * and we want to connect to only one IP... */
	lsa = xhost2sockaddr(server.host, server.port);
	if (!(opt & WGET_OPT_QUIET)) {
		fprintf(stderr, "Connecting to %s (%s)\n", server.host,
				xmalloc_sockaddr2dotted(&lsa->sa, lsa->len));
		/* We leak result of xmalloc_sockaddr2dotted */
	}

	if (use_proxy || !target.is_ftp) {
		/*
		 *  HTTP session
		 */
		do {
			got_clen = chunked = 0;

			if (!--try)
				bb_error_msg_and_die("too many redirections");

			/*
			 * Open socket to http server
			 */
			if (sfp) fclose(sfp);
			sfp = open_socket(lsa);

			/*
			 * Send HTTP request.
			 */
			if (use_proxy) {
				fprintf(sfp, "GET %stp://%s/%s HTTP/1.1\r\n",
					target.is_ftp ? "f" : "ht", target.host,
					target.path);
			} else {
				fprintf(sfp, "GET /%s HTTP/1.1\r\n", target.path);
			}

			fprintf(sfp, "Host: %s\r\nUser-Agent: %s\r\n",
				target.host, user_agent);

#if ENABLE_FEATURE_WGET_AUTHENTICATION
			if (target.user) {
				fprintf(sfp, "Authorization: Basic %s\r\n",
					base64enc((unsigned char*)target.user, buf, sizeof(buf)));
			}
			if (use_proxy && server.user) {
				fprintf(sfp, "Proxy-Authorization: Basic %s\r\n",
					base64enc((unsigned char*)server.user, buf, sizeof(buf)));
			}
#endif

			if (beg_range)
				fprintf(sfp, "Range: bytes=%"OFF_FMT"d-\r\n", beg_range);
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
			if (extra_headers)
				fputs(extra_headers, sfp);
#endif
			fprintf(sfp, "Connection: close\r\n\r\n");

			/*
			* Retrieve HTTP response line and check for "200" status code.
			*/
 read_response:
			if (fgets(buf, sizeof(buf), sfp) == NULL)
				bb_error_msg_and_die("no response from server");

			s = buf;
			while (*s != '\0' && !isspace(*s)) ++s;
			s = skip_whitespace(s);
			// FIXME: no error check
			// xatou wouldn't work: "200 OK"
			status = atoi(s);
			switch (status) {
			case 0:
			case 100:
				while (gethdr(buf, sizeof(buf), sfp, &n) != NULL)
					/* eat all remaining headers */;
				goto read_response;
			case 200:
				break;
			case 300:	/* redirection */
			case 301:
			case 302:
			case 303:
				break;
			case 206:
				if (beg_range)
					break;
				/*FALLTHRU*/
			default:
				/* Show first line only and kill any ESC tricks */
				buf[strcspn(buf, "\n\r\x1b")] = '\0';
				bb_error_msg_and_die("server returned error: %s", buf);
			}

			/*
			 * Retrieve HTTP headers.
			 */
			while ((s = gethdr(buf, sizeof(buf), sfp, &n)) != NULL) {
				if (strcasecmp(buf, "content-length") == 0) {
					content_len = BB_STRTOOFF(s, NULL, 10);
					if (errno || content_len < 0) {
						bb_error_msg_and_die("content-length %s is garbage", s);
					}
					got_clen = 1;
					continue;
				}
				if (strcasecmp(buf, "transfer-encoding") == 0) {
					if (strcasecmp(s, "chunked") != 0)
						bb_error_msg_and_die("server wants to do %s transfer encoding", s);
					chunked = got_clen = 1;
				}
				if (strcasecmp(buf, "location") == 0) {
					if (s[0] == '/')
						/* free(target.allocated); */
						target.path = /* target.allocated = */ xstrdup(s+1);
					else {
						parse_url(s, &target);
						if (use_proxy == 0) {
							server.host = target.host;
							server.port = target.port;
						}
						free(lsa);
						lsa = xhost2sockaddr(server.host, server.port);
						break;
					}
				}
			}
		} while (status >= 300);

		dfp = sfp;

	} else {

		/*
		 *  FTP session
		 */
		if (!target.user)
			target.user = xstrdup("anonymous:busybox@");

		sfp = open_socket(lsa);
		if (ftpcmd(NULL, NULL, sfp, buf) != 220)
			bb_error_msg_and_die("%s", buf+4);

		/*
		 * Splitting username:password pair,
		 * trying to log in
		 */
		s = strchr(target.user, ':');
		if (s)
			*(s++) = '\0';
		switch (ftpcmd("USER ", target.user, sfp, buf)) {
		case 230:
			break;
		case 331:
			if (ftpcmd("PASS ", s, sfp, buf) == 230)
				break;
			/* FALLTHRU (failed login) */
		default:
			bb_error_msg_and_die("ftp login: %s", buf+4);
		}

		ftpcmd("TYPE I", NULL, sfp, buf);

		/*
		 * Querying file size
		 */
		if (ftpcmd("SIZE ", target.path, sfp, buf) == 213) {
			content_len = BB_STRTOOFF(buf+4, NULL, 10);
			if (errno || content_len < 0) {
				bb_error_msg_and_die("SIZE value is garbage");
			}
			got_clen = 1;
		}

		/*
		 * Entering passive mode
		 */
		if (ftpcmd("PASV", NULL, sfp, buf) != 227) {
 pasv_error:
			bb_error_msg_and_die("bad response to %s: %s", "PASV", buf);
		}
		// Response is "227 garbageN1,N2,N3,N4,P1,P2[)garbage]
		// Server's IP is N1.N2.N3.N4 (we ignore it)
		// Server's port for data connection is P1*256+P2
		s = strrchr(buf, ')');
		if (s) s[0] = '\0';
		s = strrchr(buf, ',');
		if (!s) goto pasv_error;
		port = xatou_range(s+1, 0, 255);
		*s = '\0';
		s = strrchr(buf, ',');
		if (!s) goto pasv_error;
		port += xatou_range(s+1, 0, 255) * 256;
		set_nport(lsa, htons(port));
		dfp = open_socket(lsa);

		if (beg_range) {
			sprintf(buf, "REST %"OFF_FMT"d", beg_range);
			if (ftpcmd(buf, NULL, sfp, buf) == 350)
				content_len -= beg_range;
		}

		if (ftpcmd("RETR ", target.path, sfp, buf) > 150)
			bb_error_msg_and_die("bad response to RETR: %s", buf);
	}


	/*
	 * Retrieve file
	 */
	if (chunked) {
		fgets(buf, sizeof(buf), dfp);
		content_len = STRTOOFF(buf, NULL, 16);
		/* FIXME: error check?? */
	}

	/* Do it before progressmeter (want to have nice error message) */
	if (output_fd < 0)
		output_fd = xopen(fname_out,
			O_WRONLY|O_CREAT|O_EXCL|O_TRUNC);

	if (!(opt & WGET_OPT_QUIET))
		progressmeter(-1);

	do {
		while (content_len > 0 || !got_clen) {
			unsigned rdsz = sizeof(buf);
			if (content_len < sizeof(buf) && (chunked || got_clen))
				rdsz = (unsigned)content_len;
			n = safe_fread(buf, 1, rdsz, dfp);
			if (n <= 0)
				break;
			if (full_write(output_fd, buf, n) != n) {
				bb_perror_msg_and_die(bb_msg_write_error);
			}
#if ENABLE_FEATURE_WGET_STATUSBAR
			transferred += n;
#endif
			if (got_clen) {
				content_len -= n;
			}
		}

		if (chunked) {
			safe_fgets(buf, sizeof(buf), dfp); /* This is a newline */
			safe_fgets(buf, sizeof(buf), dfp);
			content_len = STRTOOFF(buf, NULL, 16);
			/* FIXME: error check? */
			if (content_len == 0) {
				chunked = 0; /* all done! */
			}
		}

		if (n == 0 && ferror(dfp)) {
			bb_perror_msg_and_die(bb_msg_read_error);
		}
	} while (chunked);

	if (!(opt & WGET_OPT_QUIET))
		progressmeter(1);

	if ((use_proxy == 0) && target.is_ftp) {
		fclose(dfp);
		if (ftpcmd(NULL, NULL, sfp, buf) != 226)
			bb_error_msg_and_die("ftp error: %s", buf+4);
		ftpcmd("QUIT", NULL, sfp, buf);
	}
	exit(EXIT_SUCCESS);
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
		bb_error_msg_and_die("not an http or ftp url: %s", url);

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
		/* must be writable because of bb_get_last_path_component() */
		static char nullstr[] = "";
		h->path = nullstr;
	} else if (*sp == '/') {
		*sp = '\0';
		h->path = sp + 1;
	} else { // '#' or '?'
		// http://busybox.net?login=john@doe is a valid URL
		// memmove converts to:
		// http:/busybox.nett?login=john@doe...
		memmove(h->host-1, h->host, sp - h->host);
		h->host--;
		sp[-1] = '\0';
		h->path = sp;
	}

	sp = strrchr(h->host, '@');
	h->user = NULL;
	if (sp != NULL) {
		h->user = h->host;
		*sp = '\0';
		h->host = sp + 1;
	}

	sp = h->host;
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


static char *gethdr(char *buf, size_t bufsiz, FILE *fp, int *istrunc)
{
	char *s, *hdrval;
	int c;

	*istrunc = 0;

	/* retrieve header line */
	if (fgets(buf, bufsiz, fp) == NULL)
		return NULL;

	/* see if we are at the end of the headers */
	for (s = buf; *s == '\r'; ++s)
		;
	if (s[0] == '\n')
		return NULL;

	/* convert the header name to lower case */
	for (s = buf; isalnum(*s) || *s == '-'; ++s)
		*s = tolower(*s);

	/* verify we are at the end of the header name */
	if (*s != ':')
		bb_error_msg_and_die("bad header line: %s", buf);

	/* locate the start of the header value */
	for (*s++ = '\0'; *s == ' ' || *s == '\t'; ++s)
		;
	hdrval = s;

	/* locate the end of header */
	while (*s != '\0' && *s != '\r' && *s != '\n')
		++s;

	/* end of header found */
	if (*s != '\0') {
		*s = '\0';
		return hdrval;
	}

	/* Rats!  The buffer isn't big enough to hold the entire header value. */
	while (c = getc(fp), c != EOF && c != '\n')
		;
	*istrunc = 1;
	return hdrval;
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
	result = xatoi_u(buf);
	buf[3] = ' ';
	return result;
}

#if ENABLE_FEATURE_WGET_STATUSBAR
/* Stuff below is from BSD rcp util.c, as added to openshh.
 * Original copyright notice is retained at the end of this file.
 */
static int
getttywidth(void)
{
	int width;
	get_terminal_width_height(0, &width, NULL);
	return width;
}

static void
updateprogressmeter(int ignore)
{
	int save_errno = errno;

	progressmeter(0);
	errno = save_errno;
}

static void alarmtimer(int iwait)
{
	struct itimerval itv;

	itv.it_value.tv_sec = iwait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}


static void
progressmeter(int flag)
{
	static struct timeval lastupdate;
	static off_t lastsize, totalsize;

	struct timeval now, td, tvwait;
	off_t abbrevsize;
	int elapsed, ratio, barlength, i;
	char buf[256];

	if (flag == -1) { /* first call to progressmeter */
		gettimeofday(&start, (struct timezone *) 0);
		lastupdate = start;
		lastsize = 0;
		totalsize = content_len + beg_range; /* as content_len changes.. */
	}

	gettimeofday(&now, (struct timezone *) 0);
	ratio = 100;
	if (totalsize != 0 && !chunked) {
		/* long long helps to have working ETA even if !LFS */
		ratio = (int) (100 * (unsigned long long)(transferred+beg_range) / totalsize);
		ratio = MIN(ratio, 100);
	}

	fprintf(stderr, "\r%-20.20s%4d%% ", curfile, ratio);

	barlength = getttywidth() - 51;
	if (barlength > 0 && barlength < sizeof(buf)) {
		i = barlength * ratio / 100;
		memset(buf, '*', i);
		memset(buf + i, ' ', barlength - i);
		buf[barlength] = '\0';
		fprintf(stderr, "|%s|", buf);
	}
	i = 0;
	abbrevsize = transferred + beg_range;
	while (abbrevsize >= 100000) {
		i++;
		abbrevsize >>= 10;
	}
	/* see http://en.wikipedia.org/wiki/Tera */
	fprintf(stderr, "%6d %c%c ", (int)abbrevsize, " KMGTPEZY"[i], i?'B':' ');

	timersub(&now, &lastupdate, &tvwait);
	if (transferred > lastsize) {
		lastupdate = now;
		lastsize = transferred;
		if (tvwait.tv_sec >= STALLTIME)
			timeradd(&start, &tvwait, &start);
		tvwait.tv_sec = 0;
	}
	timersub(&now, &start, &td);
	elapsed = td.tv_sec;

	if (tvwait.tv_sec >= STALLTIME) {
		fprintf(stderr, " - stalled -");
	} else {
		off_t to_download = totalsize - beg_range;
		if (transferred <= 0 || elapsed <= 0 || transferred > to_download || chunked) {
			fprintf(stderr, "--:--:-- ETA");
		} else {
			/* to_download / (transferred/elapsed) - elapsed: */
			int eta = (int) ((unsigned long long)to_download*elapsed/transferred - elapsed);
			/* (long long helps to have working ETA even if !LFS) */
			i = eta % 3600;
			fprintf(stderr, "%02d:%02d:%02d ETA", eta / 3600, i / 60, i % 60);
		}
	}

	if (flag == -1) { /* first call to progressmeter */
		struct sigaction sa;
		sa.sa_handler = updateprogressmeter;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		sigaction(SIGALRM, &sa, NULL);
		alarmtimer(1);
	} else if (flag == 1) { /* last call to progressmeter */
		alarmtimer(0);
		transferred = 0;
		putc('\n', stderr);
	}
}
#endif

/* Original copyright notice which applies to the CONFIG_FEATURE_WGET_STATUSBAR stuff,
 * much of which was blatantly stolen from openssh.  */

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. <BSD Advertising Clause omitted per the July 22, 1999 licensing change
 *		ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change>
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
