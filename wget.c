/* vi: set sw=4 ts=4: */
/*
 * wget - retrieve a file using HTTP
 *
 * Chip Rosenthal Covad Communications <chip@laserlink.net>
 *
 * Note: According to RFC2616 section 3.6.1, "All HTTP/1.1 applications MUST be
 * able to receive and decode the "chunked" transfer-coding, and MUST ignore
 * chunk-extension extensions they do not understand."  
 *
 * This prevents this particular wget app from completely RFC compliant, and as
 * such, prevents it from being used as a general purpose web browser...  This
 * is a design decision, since it makes the code smaller.
 *
 */

#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


void parse_url(char *url, char **uri_host, int *uri_port, char **uri_path);
FILE *open_socket(char *host, int port);
char *gethdr(char *buf, size_t bufsiz, FILE *fp, int *istrunc);
void progressmeter(int flag);

/* Globals (can be accessed from signal handlers */
static off_t filesize = 0;		/* content-length of the file */
#ifdef BB_FEATURE_STATUSBAR
static char *curfile;			/* Name of current file being transferred. */
static struct timeval start;	/* Time a transfer started. */
volatile unsigned long statbytes; /* Number of bytes transferred so far. */
/* For progressmeter() -- number of seconds before xfer considered "stalled" */
#define STALLTIME	5
#endif

int wget_main(int argc, char **argv)
{
	FILE *sfp;					/* socket to web server				*/
	char *uri_host, *uri_path;	/* parsed from command line url		*/
	char *proxy;
	int uri_port;
	char *s, buf[512];
	int n;

	char *fname_out = NULL;		/* where to direct output (-O)		*/
	int do_continue = 0;		/* continue a prev transfer (-c)	*/
	long beg_range = 0L;		/*   range at which continue begins	*/
	int got_clen = 0;			/* got content-length: from server	*/
	FILE *output;					/* socket to web server				*/

	/*
	 * Crack command line.
	 */
	while ((n = getopt(argc, argv, "cO:")) != EOF) {
		switch (n) {
		case 'c':
			++do_continue;
			break;
		case 'O':
			/* can't set fname_out to NULL if outputting to stdout, because
			 * this gets interpreted as the auto-gen output filename
			 * case below  - tausq@debian.org
			 */
			fname_out = (strcmp(optarg, "-") == 0 ? (char *)1 : optarg);
			break;
		default:
			usage(wget_usage);
		}
	}

	if (argc - optind != 1)
			usage(wget_usage);

	/* Guess an output filename */
	if (!fname_out) {
		fname_out = 
#ifdef BB_FEATURE_STATUSBAR
			curfile = 
#endif
			get_last_path_component(argv[optind]);
#ifdef BB_FEATURE_STATUSBAR
	} else {
		curfile=argv[optind];
#endif
	}


	if (do_continue && !fname_out)
		error_msg_and_die("cannot specify continue (-c) without a filename (-O)\n");

	/*
	 * Use the proxy if necessary.
	 */
	if ((proxy = getenv("http_proxy")) != NULL) {
		proxy = xstrdup(proxy);
		parse_url(proxy, &uri_host, &uri_port, &uri_path);
		uri_path = argv[optind];
	} else {
		/*
		 * Parse url into components.
		 */
		parse_url(argv[optind], &uri_host, &uri_port, &uri_path);
	}

	/*
	 * Open socket to server.
	 */
	sfp = open_socket(uri_host, uri_port);

	/*
	 * Open the output stream.
	 */
	if (fname_out != (char *)1) {
		if ( (output=fopen(fname_out, (do_continue ? "a" : "w"))) 
				== NULL)
			perror_msg_and_die("fopen(%s)", fname_out);
	} else {
		output = stdout;
	}

	/*
	 * Determine where to start transfer.
	 */
	if (do_continue) {
		struct stat sbuf;
		if (fstat(fileno(output), &sbuf) < 0)
			error_msg_and_die("fstat()");
		if (sbuf.st_size > 0)
			beg_range = sbuf.st_size;
		else
			do_continue = 0;
	}

	/*
	 * Send HTTP request.
	 */
	fprintf(sfp, "GET %s HTTP/1.1\r\nHost: %s\r\n", uri_path, uri_host);
	if (do_continue)
		fprintf(sfp, "Range: bytes=%ld-\r\n", beg_range);
	fputs("Connection: close\r\n\r\n", sfp);

	/*
	 * Retrieve HTTP response line and check for "200" status code.
	 */
	if (fgets(buf, sizeof(buf), sfp) == NULL)
		error_msg_and_die("no response from server\n");
	for (s = buf ; *s != '\0' && !isspace(*s) ; ++s)
		;
	for ( ; isspace(*s) ; ++s)
		;
	switch (atoi(s)) {
		case 200:
			if (!do_continue)
				break;
			error_msg_and_die("server does not support ranges\n");
		case 206:
			if (do_continue)
				break;
			/*FALLTHRU*/
		default:
			error_msg_and_die("server returned error: %s", buf);
	}

	/*
	 * Retrieve HTTP headers.
	 */
	while ((s = gethdr(buf, sizeof(buf), sfp, &n)) != NULL) {
		if (strcmp(buf, "content-length") == 0) {
			filesize = atol(s);
			got_clen = 1;
			continue;
		}
		if (strcmp(buf, "transfer-encoding") == 0) {
			error_msg_and_die("server wants to do %s transfer encoding\n", s);
			continue;
		}
	}

	/*
	 * Retrieve HTTP body.
	 */
#ifdef BB_FEATURE_STATUSBAR
	statbytes=0;
	progressmeter(-1);
#endif
	while (filesize > 0 && (n = fread(buf, 1, sizeof(buf), sfp)) > 0) {
		fwrite(buf, 1, n, output);
#ifdef BB_FEATURE_STATUSBAR
		statbytes+=n;
		progressmeter(1);
#endif
		if (got_clen)
			filesize -= n;
	}
	if (n == 0 && ferror(sfp))
		perror_msg_and_die("network read error");

	exit(0);
}


void parse_url(char *url, char **uri_host, int *uri_port, char **uri_path)
{
	char *s, *h;
	static char *defaultpath = "/";

	*uri_port = 80;

	if (strncmp(url, "http://", 7) != 0)
		error_msg_and_die("not an http url: %s\n", url);

	/* pull the host portion to the front of the buffer */
	for (s = url, h = url+7 ; *h != '/' && *h != 0; ++h) {
		if (*h == ':') {
			*uri_port = atoi(h+1);
			*h = '\0';
		}
		*s++ = *h;
	}
	*s = '\0';

	if (*h == 0) h = defaultpath;

	*uri_host = url;
	*uri_path = h;
}


FILE *open_socket(char *host, int port)
{
	struct sockaddr_in sin;
	struct hostent *hp;
	int fd;
	FILE *fp;

	memzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	if ((hp = (struct hostent *) gethostbyname(host)) == NULL)
		error_msg_and_die("cannot resolve %s\n", host);
	memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
	sin.sin_port = htons(port);

	/*
	 * Get the server onto a stdio stream.
	 */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		perror_msg_and_die("socket()");
	if (connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
		perror_msg_and_die("connect(%s)", host);
	if ((fp = fdopen(fd, "r+")) == NULL)
		perror_msg_and_die("fdopen()");

	return fp;
}


char *gethdr(char *buf, size_t bufsiz, FILE *fp, int *istrunc)
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
		error_msg_and_die("bad header line: %s\n", buf);

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

#ifdef BB_FEATURE_STATUSBAR
/* Stuff below is from BSD rcp util.c, as added to openshh. 
 * Original copyright notice is retained at the end of this file.
 * 
 */ 


int
getttywidth(void)
{
	struct winsize winsize;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) != -1)
		return (winsize.ws_col ? winsize.ws_col : 80);
	else
		return (80);
}

void
updateprogressmeter(int ignore)
{
	int save_errno = errno;

	progressmeter(0);
	errno = save_errno;
}

void
alarmtimer(int wait)
{
	struct itimerval itv;

	itv.it_value.tv_sec = wait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}


void
progressmeter(int flag)
{
	static const char prefixes[] = " KMGTP";
	static struct timeval lastupdate;
	static off_t lastsize;
	struct timeval now, td, wait;
	off_t cursize, abbrevsize;
	double elapsed;
	int ratio, barlength, i, remaining;
	char buf[256];

	if (flag == -1) {
		(void) gettimeofday(&start, (struct timezone *) 0);
		lastupdate = start;
		lastsize = 0;
	}

	(void) gettimeofday(&now, (struct timezone *) 0);
	cursize = statbytes;
	if (filesize != 0) {
		ratio = 100.0 * cursize / filesize;
		ratio = MAX(ratio, 0);
		ratio = MIN(ratio, 100);
	} else
		ratio = 100;

	snprintf(buf, sizeof(buf), "\r%-20.20s %3d%% ", curfile, ratio);

	barlength = getttywidth() - 51;
	if (barlength > 0) {
		i = barlength * ratio / 100;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "|%.*s%*s|", i,
			 "*****************************************************************************"
			 "*****************************************************************************",
			 barlength - i, "");
	}
	i = 0;
	abbrevsize = cursize;
	while (abbrevsize >= 100000 && i < sizeof(prefixes)) {
		i++;
		abbrevsize >>= 10;
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %5d %c%c ",
	     (int) abbrevsize, prefixes[i], prefixes[i] == ' ' ? ' ' :
		 'B');

	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		if (wait.tv_sec >= STALLTIME) {
			start.tv_sec += wait.tv_sec;
			start.tv_usec += wait.tv_usec;
		}
		wait.tv_sec = 0;
	}
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	if (statbytes <= 0 || elapsed <= 0.0 || cursize > filesize) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 " - stalled -");
	} else {
		remaining = (int) (filesize / (statbytes / elapsed) - elapsed);
		i = remaining / 3600;
		if (i)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				 "%2d:", i);
		else
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				 "   ");
		i = remaining % 3600;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "%02d:%02d ETA", i / 60, i % 60);
	}
	write(fileno(stderr), buf, strlen(buf));

	if (flag == -1) {
		struct sigaction sa;
		sa.sa_handler = updateprogressmeter;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART;
		sigaction(SIGALRM, &sa, NULL);
		alarmtimer(1);
	} else if (flag == 1) {
		alarmtimer(0);
		statbytes = 0;
	}
}
#endif

/* Original copyright notice which applies to the BB_FEATURE_STATUSBAR stuff,
 * much of which was blatently stolen from openssh.  */
 
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
 *	$Id: wget.c,v 1.11 2000/12/07 22:42:11 andersen Exp $
 */



/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/



