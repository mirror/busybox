/* vi: set sw=4 ts=4: */
/*
 * wget - retrieve a file using HTTP
 *
 * Chip Rosenthal
 * Covad Communications
 * <chip@laserlink.net>
 */

#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


void parse_url(char *url, char **uri_host, int *uri_port, char **uri_path);
FILE *open_socket(char *host, int port);
char *gethdr(char *buf, size_t bufsiz, FILE *fp, int *istrunc);


int wget_main(int argc, char **argv)
{
	FILE *sfp;					/* socket to web server				*/
	char *uri_host, *uri_path;	/* parsed from command line url		*/
	int uri_port;
	char *s, buf[512];
	int n;

	char *fname_out = NULL;		/* where to direct output (-O)		*/
	int do_continue = 0;		/* continue a prev transfer (-c)	*/
	long beg_range = 0L;		/*   range at which continue begins	*/
	int got_clen = 0;			/* got content-length: from server	*/
	long clen = 1L;				/*   the content length				*/

	/*
	 * Crack command line.
	 */
	while ((n = getopt(argc, argv, "cO:")) != EOF) {
		switch (n) {
		case 'c':
			++do_continue;
			break;
		case 'O':
			fname_out = (strcmp(optarg, "-") == 0 ? NULL : optarg);
			break;
		default:
			usage(wget_usage);
		}
	}
	if (do_continue && !fname_out)
		fatalError("wget: cannot specify continue (-c) without a filename (-O)\n");
	if (argc - optind != 1)
			usage(wget_usage);
	/*
	 * Parse url into components.
	 */
	parse_url(argv[optind], &uri_host, &uri_port, &uri_path);

	/*
	 * Open socket to server.
	 */
	sfp = open_socket(uri_host, uri_port);

	/*
	 * Open the output stream.
	 */
	if (fname_out != NULL) {
		if (freopen(fname_out, (do_continue ? "a" : "w"), stdout) == NULL)
			fatalError("wget: freopen(%s): %s\n", fname_out, strerror(errno));
	}

	/*
	 * Determine where to start transfer.
	 */
	if (do_continue) {
		struct stat sbuf;
		if (fstat(fileno(stdout), &sbuf) < 0)
			fatalError("wget: fstat(): %s\n", strerror(errno));
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
		fatalError("wget: no response from server\n");
	for (s = buf ; *s != '\0' && !isspace(*s) ; ++s)
		;
	for ( ; isspace(*s) ; ++s)
		;
	switch (atoi(s)) {
	case 200:
		if (!do_continue)
			break;
		fatalError("wget: cannot continue - server does not properly support ranges\n");
	case 206:
		if (do_continue)
			break;
		/*FALLTHRU*/
	default:
		fatalError("wget: server returned error: %s", buf);
	}

	/*
	 * Retrieve HTTP headers.
	 */
	while ((s = gethdr(buf, sizeof(buf), sfp, &n)) != NULL) {
		if (strcmp(buf, "content-length") == 0) {
			clen = atol(s);
			got_clen = 1;
			continue;
		}
		if (strcmp(buf, "transfer-encoding") == 0) {
			fatalError("wget: i do not do transfer encodings, server wants to do: %s\n", s);
			continue;
		}
	}

	/*
	 * Retrieve HTTP body.
	 */
	while (clen > 0 && (n = fread(buf, 1, sizeof(buf), sfp)) > 0) {
		fwrite(buf, 1, n, stdout);
		if (got_clen)
			clen -= n;
	}
	if (n == 0 && ferror(sfp))
		fatalError("wget: network read error: %s", strerror(errno));

	exit(0);
}


void parse_url(char *url, char **uri_host, int *uri_port, char **uri_path)
{
	char *s, *h;

	*uri_port = 80;

	if (strncmp(url, "http://", 7) != 0)
		fatalError("wget: not an http url: %s\n", url);

	/* pull the host portion to the front of the buffer */
	for (s = url, h = url+7 ; *h != '/' ; ++h) {
		if (*h == '\0')
			fatalError("wget: cannot parse url: %s\n", url);
		if (*h == ':') {
			*uri_port = atoi(h+1);
			*h = '\0';
		}
		*s++ = *h;
	}
	*s = '\0';
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
		fatalError("wget: cannot resolve %s\n", host);
	memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
	sin.sin_port = htons(port);

	/*
	 * Get the server onto a stdio stream.
	 */
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatalError("wget: socket(): %s\n", strerror(errno));
	if (connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
		fatalError("wget: connect(%s): %s\n", host, strerror(errno));
	if ((fp = fdopen(fd, "r+")) == NULL)
		fatalError("wget: fdopen(): %s\n", strerror(errno));

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
		fatalError("wget: bad header line: %s\n", buf);

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

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
