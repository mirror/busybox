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

#ifdef CONFIG_LFS
# define FILEOFF_TYPE off64_t
# define FILEOFF_FMT "%lld"
# define LSEEK lseek64
# define STRTOOFF strtoll
# define SAFE_STRTOOFF safe_strtoll
/* stat64 etc as needed...  */
#else
# define FILEOFF_TYPE off_t
# define FILEOFF_FMT "%ld"
# define LSEEK lseek
# define STRTOOFF strtol
# define SAFE_STRTOOFF safe_strtol
/* Do we need to undefine O_LARGEFILE? */
#endif

struct host_info {
	char *host;
	int port;
	char *path;
	int is_ftp;
	char *user;
};

static void parse_url(char *url, struct host_info *h);
static FILE *open_socket(struct sockaddr_in *s_in);
static char *gethdr(char *buf, size_t bufsiz, FILE *fp, int *istrunc);
static int ftpcmd(char *s1, char *s2, FILE *fp, char *buf);

/* Globals (can be accessed from signal handlers */
static FILEOFF_TYPE content_len;        /* Content-length of the file */
static FILEOFF_TYPE beg_range;          /* Range at which continue begins */
static FILEOFF_TYPE transferred;        /* Number of bytes transferred so far */
static int chunked;                     /* chunked transfer encoding */
#ifdef CONFIG_FEATURE_WGET_STATUSBAR
static void progressmeter(int flag);
static char *curfile;                   /* Name of current file being transferred */
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

#ifdef CONFIG_FEATURE_WGET_AUTHENTICATION
/*
 *  Base64-encode character string and return the string.
 */
static char *base64enc(unsigned char *p, char *buf, int len)
{
	bb_uuencode(p, buf, len, bb_uuenc_tbl_base64);
	return buf;
}
#endif

#define WGET_OPT_CONTINUE     1
#define WGET_OPT_QUIET        2
#define WGET_OPT_PASSIVE      4
#define WGET_OPT_OUTNAME      8
#define WGET_OPT_HEADER      16
#define WGET_OPT_PREFIX      32
#define WGET_OPT_PROXY       64
#define WGET_OPT_USER_AGENT 128

#if ENABLE_FEATURE_WGET_LONG_OPTIONS
static const struct option wget_long_options[] = {
	{ "continue",        0, NULL, 'c' },
	{ "quiet",           0, NULL, 'q' },
	{ "passive-ftp",     0, NULL, 139 }, /* FIXME: what is this - 139?? */
	{ "output-document", 1, NULL, 'O' },
	{ "header",          1, NULL, 131 },
	{ "directory-prefix",1, NULL, 'P' },
	{ "proxy",           1, NULL, 'Y' },
	{ "user-agent",      1, NULL, 'U' },
	{ 0,                 0, 0, 0 }
};
#endif

int wget_main(int argc, char **argv)
{
	int n, try=5, status;
	unsigned long opt;
	int port;
	char *proxy = 0;
	char *dir_prefix=NULL;
	char *s, buf[512];
	char extra_headers[1024];
	char *extra_headers_ptr = extra_headers;
	int extra_headers_left = sizeof(extra_headers);
	struct host_info server, target;
	struct sockaddr_in s_in;
	llist_t *headers_llist = NULL;

	FILE *sfp = NULL;               /* socket to web/ftp server         */
	FILE *dfp = NULL;               /* socket to ftp server (data)      */
	char *fname_out = NULL;         /* where to direct output (-O)      */
	int got_clen = 0;               /* got content-length: from server  */
	int output_fd = -1;
	int use_proxy = 1;              /* Use proxies if env vars are set  */
	char *proxy_flag = "on";        /* Use proxies if env vars are set  */
	char *user_agent = "Wget";      /* Content of the "User-Agent" header field */

	/*
	 * Crack command line.
	 */
	bb_opt_complementally = "-1:\203::";
#if ENABLE_FEATURE_WGET_LONG_OPTIONS
	bb_applet_long_options = wget_long_options;
#endif
	opt = bb_getopt_ulflags(argc, argv, "cq\213O:\203:P:Y:U:",
					&fname_out, &headers_llist,
					&dir_prefix, &proxy_flag, &user_agent);
	if (strcmp(proxy_flag, "off") == 0) {
		/* Use the proxy if necessary. */
		use_proxy = 0;
	}
	if (opt & WGET_OPT_HEADER) {
		while (headers_llist) {
			int arglen = strlen(headers_llist->data);
			if (extra_headers_left - arglen - 2 <= 0)
				bb_error_msg_and_die("extra_headers buffer too small "
					"(need %i)", extra_headers_left - arglen);
			strcpy(extra_headers_ptr, headers_llist->data);
			extra_headers_ptr += arglen;
			extra_headers_left -= ( arglen + 2 );
			*extra_headers_ptr++ = '\r';
			*extra_headers_ptr++ = '\n';
			*(extra_headers_ptr + 1) = 0;
			headers_llist = headers_llist->link;
		}
	}

	parse_url(argv[optind], &target);
	server.host = target.host;
	server.port = target.port;

	/*
	 * Use the proxy if necessary.
	 */
	if (use_proxy) {
		proxy = getenv(target.is_ftp ? "ftp_proxy" : "http_proxy");
		if (proxy && *proxy) {
			parse_url(xstrdup(proxy), &server);
		} else {
			use_proxy = 0;
		}
	}

	/* Guess an output filename */
	if (!fname_out) {
		// Dirty hack. Needed because bb_get_last_path_component
		// will destroy trailing / by storing '\0' in last byte!
		if (*target.path && target.path[strlen(target.path)-1] != '/') {
			fname_out =
#ifdef CONFIG_FEATURE_WGET_STATUSBAR
				curfile =
#endif
				bb_get_last_path_component(target.path);
		}
		if (!fname_out || !fname_out[0]) {
			fname_out =
#ifdef CONFIG_FEATURE_WGET_STATUSBAR
				curfile =
#endif
				"index.html";
		}
		if (dir_prefix != NULL)
			fname_out = concat_path_file(dir_prefix, fname_out);
#ifdef CONFIG_FEATURE_WGET_STATUSBAR
	} else {
		curfile = bb_get_last_path_component(fname_out);
#endif
	}
	if ((opt & WGET_OPT_CONTINUE) && !fname_out)
		bb_error_msg_and_die("cannot specify continue (-c) without a filename (-O)");

	/*
	 * Determine where to start transfer.
	 */
	if (!strcmp(fname_out, "-")) {
		output_fd = 1;
		opt |= WGET_OPT_QUIET;
		opt &= ~WGET_OPT_CONTINUE;
	} else if (opt & WGET_OPT_CONTINUE) {
		output_fd = open(fname_out, O_WRONLY|O_LARGEFILE);
		if (output_fd >= 0) {
			beg_range = LSEEK(output_fd, 0, SEEK_END);
			if (beg_range == (FILEOFF_TYPE)-1)
				bb_perror_msg_and_die("lseek");
		}
		/* File doesn't exist. We do not create file here yet.
		   We are not sure it exists on remove side */
	}

	/* We want to do exactly _one_ DNS lookup, since some
	 * sites (i.e. ftp.us.debian.org) use round-robin DNS
	 * and we want to connect to only one IP... */
	bb_lookup_host(&s_in, server.host);
	s_in.sin_port = server.port;
	if (!(opt & WGET_OPT_QUIET)) {
		printf("Connecting to %s[%s]:%d\n",
				server.host, inet_ntoa(s_in.sin_addr), ntohs(server.port));
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
			sfp = open_socket(&s_in);

			/*
			 * Send HTTP request.
			 */
			if (use_proxy) {
				const char *format = "GET %stp://%s:%d/%s HTTP/1.1\r\n";
#ifdef CONFIG_FEATURE_WGET_IP6_LITERAL
				if (strchr(target.host, ':'))
					format = "GET %stp://[%s]:%d/%s HTTP/1.1\r\n";
#endif
				fprintf(sfp, format,
					target.is_ftp ? "f" : "ht", target.host,
					ntohs(target.port), target.path);
			} else {
				fprintf(sfp, "GET /%s HTTP/1.1\r\n", target.path);
			}

			fprintf(sfp, "Host: %s\r\nUser-Agent: %s\r\n", target.host,
			        user_agent);

#ifdef CONFIG_FEATURE_WGET_AUTHENTICATION
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
				fprintf(sfp, "Range: bytes="FILEOFF_FMT"-\r\n", beg_range);
			if(extra_headers_left < sizeof(extra_headers))
				fputs(extra_headers,sfp);
			fprintf(sfp,"Connection: close\r\n\r\n");

			/*
			* Retrieve HTTP response line and check for "200" status code.
			*/
read_response:
			if (fgets(buf, sizeof(buf), sfp) == NULL)
				bb_error_msg_and_die("no response from server");

			for (s = buf ; *s != '\0' && !isspace(*s) ; ++s)
				;
			for ( ; isspace(*s) ; ++s)
				;
			switch (status = atoi(s)) {
				case 0:
				case 100:
					while (gethdr(buf, sizeof(buf), sfp, &n) != NULL);
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
					chomp(buf);
					bb_error_msg_and_die("server returned error %d: %s", atoi(s), buf);
			}

			/*
			 * Retrieve HTTP headers.
			 */
			while ((s = gethdr(buf, sizeof(buf), sfp, &n)) != NULL) {
				if (strcasecmp(buf, "content-length") == 0) {
					if (SAFE_STRTOOFF(s, &content_len) || content_len < 0) {
						bb_error_msg_and_die("content-length %s is garbage", s);
					}
					got_clen = 1;
					continue;
				}
				if (strcasecmp(buf, "transfer-encoding") == 0) {
					if (strcasecmp(s, "chunked") == 0) {
						chunked = got_clen = 1;
					} else {
						bb_error_msg_and_die("server wants to do %s transfer encoding", s);
					}
				}
				if (strcasecmp(buf, "location") == 0) {
					if (s[0] == '/')
						target.path = xstrdup(s+1);
					else {
						parse_url(xstrdup(s), &target);
						if (use_proxy == 0) {
							server.host = target.host;
							server.port = target.port;
						}
						bb_lookup_host(&s_in, server.host);
						s_in.sin_port = server.port;
						break;
					}
				}
			}
		} while(status >= 300);

		dfp = sfp;
	}
	else
	{
		/*
		 *  FTP session
		 */
		if (!target.user)
			target.user = xstrdup("anonymous:busybox@");

		sfp = open_socket(&s_in);
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
			if (SAFE_STRTOOFF(buf+4, &content_len) || content_len < 0) {
				bb_error_msg_and_die("SIZE value is garbage");
			}
			got_clen = 1;
		}

		/*
		 * Entering passive mode
		 */
		if (ftpcmd("PASV", NULL, sfp, buf) !=  227)
			bb_error_msg_and_die("PASV: %s", buf+4);
		s = strrchr(buf, ',');
		*s = 0;
		port = atoi(s+1);
		s = strrchr(buf, ',');
		port += atoi(s+1) * 256;
		s_in.sin_port = htons(port);
		dfp = open_socket(&s_in);

		if (beg_range) {
			sprintf(buf, "REST "FILEOFF_FMT, beg_range);
			if (ftpcmd(buf, NULL, sfp, buf) == 350)
				content_len -= beg_range;
		}

		if (ftpcmd("RETR ", target.path, sfp, buf) > 150)
			bb_error_msg_and_die("RETR: %s", buf+4);
	}


	/*
	 * Retrieve file
	 */
	if (chunked) {
		fgets(buf, sizeof(buf), dfp);
		content_len = STRTOOFF(buf, (char **) NULL, 16);
		/* FIXME: error check?? */
	}

	/* Do it before progressmeter (want to have nice error message) */
	if (output_fd < 0)
		output_fd = xopen3(fname_out,
			O_WRONLY|O_CREAT|O_EXCL|O_TRUNC|O_LARGEFILE, 0666);

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
#ifdef CONFIG_FEATURE_WGET_STATUSBAR
			transferred += n;
#endif
			if (got_clen) {
				content_len -= n;
			}
		}

		if (chunked) {
			safe_fgets(buf, sizeof(buf), dfp); /* This is a newline */
			safe_fgets(buf, sizeof(buf), dfp);
			content_len = STRTOOFF(buf, (char **) NULL, 16);
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


static void parse_url(char *url, struct host_info *h)
{
	char *cp, *sp, *up, *pp;

	if (strncmp(url, "http://", 7) == 0) {
		h->port = bb_lookup_port("http", "tcp", 80);
		h->host = url + 7;
		h->is_ftp = 0;
	} else if (strncmp(url, "ftp://", 6) == 0) {
		h->port = bb_lookup_port("ftp", "tfp", 21);
		h->host = url + 6;
		h->is_ftp = 1;
	} else
		bb_error_msg_and_die("not an http or ftp url: %s", url);

	sp = strchr(h->host, '/');
	if (sp) {
		*sp++ = '\0';
		h->path = sp;
	} else
		h->path = xstrdup("");

	up = strrchr(h->host, '@');
	if (up != NULL) {
		h->user = h->host;
		*up++ = '\0';
		h->host = up;
	} else
		h->user = NULL;

	pp = h->host;

#ifdef CONFIG_FEATURE_WGET_IP6_LITERAL
	if (h->host[0] == '[') {
		char *ep;

		ep = h->host + 1;
		while (*ep == ':' || isxdigit (*ep))
			ep++;
		if (*ep == ']') {
			h->host++;
			*ep = '\0';
			pp = ep + 1;
		}
	}
#endif

	cp = strchr(pp, ':');
	if (cp != NULL) {
		*cp++ = '\0';
		h->port = htons(atoi(cp));
	}
}


static FILE *open_socket(struct sockaddr_in *s_in)
{
	FILE *fp;

	fp = fdopen(xconnect(s_in), "r+");
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
	for (s = buf ; *s == '\r' ; ++s)
		;
	if (s[0] == '\n')
		return NULL;

	/* convert the header name to lower case */
	for (s = buf ; isalnum(*s) || *s == '-' ; ++s)
		*s = tolower(*s);

	/* verify we are at the end of the header name */
	if (*s != ':')
		bb_error_msg_and_die("bad header line: %s", buf);

	/* locate the start of the header value */
	for (*s++ = '\0' ; *s == ' ' || *s == '\t' ; ++s)
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

static int ftpcmd(char *s1, char *s2, FILE *fp, char *buf)
{
	if (s1) {
		if (!s2) s2="";
		fprintf(fp, "%s%s\r\n", s1, s2);
		fflush(fp);
	}

	do {
		char *buf_ptr;

		if (fgets(buf, 510, fp) == NULL) {
			bb_perror_msg_and_die("fgets");
		}
		buf_ptr = strstr(buf, "\r\n");
		if (buf_ptr) {
			*buf_ptr = '\0';
		}
	} while (!isdigit(buf[0]) || buf[3] != ' ');

	return atoi(buf);
}

#ifdef CONFIG_FEATURE_WGET_STATUSBAR
/* Stuff below is from BSD rcp util.c, as added to openshh.
 * Original copyright notice is retained at the end of this file.
 */
static int
getttywidth(void)
{
	int width=0;
	get_terminal_width_height(0, &width, NULL);
	return (width);
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
	static FILEOFF_TYPE lastsize, totalsize;

	struct timeval now, td, tvwait;
	FILEOFF_TYPE abbrevsize;
	int elapsed, ratio, barlength, i;
	char buf[256];

	if (flag == -1) { /* first call to progressmeter */
		(void) gettimeofday(&start, (struct timezone *) 0);
		lastupdate = start;
		lastsize = 0;
		totalsize = content_len + beg_range; /* as content_len changes.. */
	}

	(void) gettimeofday(&now, (struct timezone *) 0);
	ratio = 100;
	if (totalsize != 0 && !chunked) {
		ratio = (int) (100 * (transferred+beg_range) / totalsize);
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
	/* See http://en.wikipedia.org/wiki/Tera */
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
		FILEOFF_TYPE to_download = totalsize - beg_range;
		if (transferred <= 0 || elapsed <= 0 || transferred > to_download || chunked) {
			fprintf(stderr, "--:--:-- ETA");
		} else {
			/* to_download / (transferred/elapsed) - elapsed: */
			int eta = (int) (to_download*elapsed/transferred - elapsed);
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
 *	$Id: wget.c,v 1.75 2004/10/08 08:27:40 andersen Exp $
 */
