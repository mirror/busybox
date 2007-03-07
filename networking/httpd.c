/* vi: set sw=4 ts=4: */
/*
 * httpd implementation for busybox
 *
 * Copyright (C) 2002,2003 Glenn Engel <glenne@engel.org>
 * Copyright (C) 2003-2006 Vladimir Oleynik <dzo@simtreas.ru>
 *
 * simplify patch stolen from libbb without using strdup
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 *****************************************************************************
 *
 * Typical usage:
 *   for non root user
 * httpd -p 8080 -h $HOME/public_html
 *   or for daemon start from rc script with uid=0:
 * httpd -u www
 * This is equivalent if www user have uid=80 to
 * httpd -p 80 -u 80 -h /www -c /etc/httpd.conf -r "Web Server Authentication"
 *
 *
 * When a url contains "cgi-bin" it is assumed to be a cgi script.  The
 * server changes directory to the location of the script and executes it
 * after setting QUERY_STRING and other environment variables.
 *
 * Doc:
 * "CGI Environment Variables": http://hoohoo.ncsa.uiuc.edu/cgi/env.html
 *
 * The server can also be invoked as a url arg decoder and html text encoder
 * as follows:
 *  foo=`httpd -d $foo`           # decode "Hello%20World" as "Hello World"
 *  bar=`httpd -e "<Hello World>"`  # encode as "&#60Hello&#32World&#62"
 * Note that url encoding for arguments is not the same as html encoding for
 * presentation.  -d decodes a url-encoded argument while -e encodes in html
 * for page display.
 *
 * httpd.conf has the following format:
 *
 * A:172.20.         # Allow address from 172.20.0.0/16
 * A:10.0.0.0/25     # Allow any address from 10.0.0.0-10.0.0.127
 * A:10.0.0.0/255.255.255.128  # Allow any address that previous set
 * A:127.0.0.1       # Allow local loopback connections
 * D:*               # Deny from other IP connections
 * /cgi-bin:foo:bar  # Require user foo, pwd bar on urls starting with /cgi-bin/
 * /adm:admin:setup  # Require user admin, pwd setup on urls starting with /adm/
 * /adm:toor:PaSsWd  # or user toor, pwd PaSsWd on urls starting with /adm/
 * .au:audio/basic   # additional mime type for audio.au files
 * *.php:/path/php   # running cgi.php scripts through an interpreter
 *
 * A/D may be as a/d or allow/deny - first char case insensitive
 * Deny IP rules take precedence over allow rules.
 *
 *
 * The Deny/Allow IP logic:
 *
 *  - Default is to allow all.  No addresses are denied unless
 *         denied with a D: rule.
 *  - Order of Deny/Allow rules is significant
 *  - Deny rules take precedence over allow rules.
 *  - If a deny all rule (D:*) is used it acts as a catch-all for unmatched
 *       addresses.
 *  - Specification of Allow all (A:*) is a no-op
 *
 * Example:
 *   1. Allow only specified addresses
 *     A:172.20          # Allow any address that begins with 172.20.
 *     A:10.10.          # Allow any address that begins with 10.10.
 *     A:127.0.0.1       # Allow local loopback connections
 *     D:*               # Deny from other IP connections
 *
 *   2. Only deny specified addresses
 *     D:1.2.3.        # deny from 1.2.3.0 - 1.2.3.255
 *     D:2.3.4.        # deny from 2.3.4.0 - 2.3.4.255
 *     A:*             # (optional line added for clarity)
 *
 * If a sub directory contains a config file it is parsed and merged with
 * any existing settings as if it was appended to the original configuration.
 *
 * subdir paths are relative to the containing subdir and thus cannot
 * affect the parent rules.
 *
 * Note that since the sub dir is parsed in the forked thread servicing the
 * subdir http request, any merge is discarded when the process exits.  As a
 * result, the subdir settings only have a lifetime of a single request.
 *
 *
 * If -c is not set, an attempt will be made to open the default
 * root configuration file.  If -c is set and the file is not found, the
 * server exits with an error.
 *
*/

#include "busybox.h"

/* amount of buffering in a pipe */
#ifndef PIPE_BUF
# define PIPE_BUF 4096
#endif

static const char httpdVersion[] = "busybox httpd/1.35 6-Oct-2004";
static const char default_path_httpd_conf[] = "/etc";
static const char httpd_conf[] = "httpd.conf";
static const char home[] = "./";

#define TIMEOUT 60

// Note: busybox xfuncs are not used because we want the server to keep running
//       if something bad happens due to a malformed user request.
//       As a result, all memory allocation after daemonize
//       is checked rigorously

//#define DEBUG 1
#define DEBUG 0

#define MAX_MEMORY_BUFF 8192    /* IO buffer */

typedef struct HT_ACCESS {
	char *after_colon;
	struct HT_ACCESS *next;
	char before_colon[1];         /* really bigger, must last */
} Htaccess;

typedef struct HT_ACCESS_IP {
	unsigned int ip;
	unsigned int mask;
	int allow_deny;
	struct HT_ACCESS_IP *next;
} Htaccess_IP;

typedef struct {
	char buf[MAX_MEMORY_BUFF];

	USE_FEATURE_HTTPD_BASIC_AUTH(const char *realm;)
	USE_FEATURE_HTTPD_BASIC_AUTH(char *remoteuser;)

	const char *query;

	USE_FEATURE_HTTPD_CGI(char *referer;)

	const char *configFile;

	unsigned int rmt_ip;
#if ENABLE_FEATURE_HTTPD_CGI || DEBUG
	char *rmt_ip_str;        /* for set env REMOTE_ADDR */
#endif
	unsigned port;           /* server initial port and for
						      set env REMOTE_PORT */
	const char *found_mime_type;
	const char *found_moved_temporarily;

	off_t ContentLength;          /* -1 - unknown */
	time_t last_mod;

	Htaccess_IP *ip_a_d;          /* config allow/deny lines */
	int flg_deny_all;
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	Htaccess *auth;               /* config user:password lines */
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
	Htaccess *mime_a;             /* config mime types */
#endif

	int server_socket;
	int accepted_socket;
	volatile int alarm_signaled;

#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
	Htaccess *script_i;           /* config script interpreters */
#endif
} HttpdConfig;

static HttpdConfig *config;

static const char request_GET[] = "GET";    /* size algorithmic optimize */

static const char* const suffixTable [] = {
/* Warning: shorted equivalent suffix in one line must be first */
	".htm.html", "text/html",
	".jpg.jpeg", "image/jpeg",
	".gif", "image/gif",
	".png", "image/png",
	".txt.h.c.cc.cpp", "text/plain",
	".css", "text/css",
	".wav", "audio/wav",
	".avi", "video/x-msvideo",
	".qt.mov", "video/quicktime",
	".mpe.mpeg", "video/mpeg",
	".mid.midi", "audio/midi",
	".mp3", "audio/mpeg",
#if 0                        /* unpopular */
	".au", "audio/basic",
	".pac", "application/x-ns-proxy-autoconfig",
	".vrml.wrl", "model/vrml",
#endif
	0, "application/octet-stream" /* default */
};

typedef enum {
	HTTP_OK = 200,
	HTTP_MOVED_TEMPORARILY = 302,
	HTTP_BAD_REQUEST = 400,       /* malformed syntax */
	HTTP_UNAUTHORIZED = 401, /* authentication needed, respond with auth hdr */
	HTTP_NOT_FOUND = 404,
	HTTP_FORBIDDEN = 403,
	HTTP_REQUEST_TIMEOUT = 408,
	HTTP_NOT_IMPLEMENTED = 501,   /* used for unrecognized requests */
	HTTP_INTERNAL_SERVER_ERROR = 500,
#if 0 /* future use */
	HTTP_CONTINUE = 100,
	HTTP_SWITCHING_PROTOCOLS = 101,
	HTTP_CREATED = 201,
	HTTP_ACCEPTED = 202,
	HTTP_NON_AUTHORITATIVE_INFO = 203,
	HTTP_NO_CONTENT = 204,
	HTTP_MULTIPLE_CHOICES = 300,
	HTTP_MOVED_PERMANENTLY = 301,
	HTTP_NOT_MODIFIED = 304,
	HTTP_PAYMENT_REQUIRED = 402,
	HTTP_BAD_GATEWAY = 502,
	HTTP_SERVICE_UNAVAILABLE = 503, /* overload, maintenance */
	HTTP_RESPONSE_SETSIZE = 0xffffffff
#endif
} HttpResponseNum;

typedef struct {
	HttpResponseNum type;
	const char *name;
	const char *info;
} HttpEnumString;

static const HttpEnumString httpResponseNames[] = {
	{ HTTP_OK, "OK", NULL },
	{ HTTP_MOVED_TEMPORARILY, "Found", "Directories must end with a slash." },
	{ HTTP_REQUEST_TIMEOUT, "Request Timeout",
		"No request appeared within a reasonable time period." },
	{ HTTP_NOT_IMPLEMENTED, "Not Implemented",
		"The requested method is not recognized by this server." },
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	{ HTTP_UNAUTHORIZED, "Unauthorized", "" },
#endif
	{ HTTP_NOT_FOUND, "Not Found",
		"The requested URL was not found on this server." },
	{ HTTP_BAD_REQUEST, "Bad Request", "Unsupported method." },
	{ HTTP_FORBIDDEN, "Forbidden", "" },
	{ HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error",
		"Internal Server Error" },
#if 0                               /* not implemented */
	{ HTTP_CREATED, "Created" },
	{ HTTP_ACCEPTED, "Accepted" },
	{ HTTP_NO_CONTENT, "No Content" },
	{ HTTP_MULTIPLE_CHOICES, "Multiple Choices" },
	{ HTTP_MOVED_PERMANENTLY, "Moved Permanently" },
	{ HTTP_NOT_MODIFIED, "Not Modified" },
	{ HTTP_BAD_GATEWAY, "Bad Gateway", "" },
	{ HTTP_SERVICE_UNAVAILABLE, "Service Unavailable", "" },
#endif
};


static const char RFC1123FMT[] = "%a, %d %b %Y %H:%M:%S GMT";


#define STRNCASECMP(a, str) strncasecmp((a), (str), sizeof(str)-1)


static int scan_ip(const char **ep, unsigned int *ip, unsigned char endc)
{
	const char *p = *ep;
	int auto_mask = 8;
	int j;

	*ip = 0;
	for (j = 0; j < 4; j++) {
		unsigned int octet;

		if ((*p < '0' || *p > '9') && (*p != '/' || j == 0) && *p != 0)
			return -auto_mask;
		octet = 0;
		while (*p >= '0' && *p <= '9') {
			octet *= 10;
			octet += *p - '0';
			if (octet > 255)
				return -auto_mask;
			p++;
		}
		if (*p == '.')
			p++;
		if (*p != '/' && *p != 0)
			auto_mask += 8;
		*ip = ((*ip) << 8) | octet;
	}
	if (*p != 0) {
		if (*p != endc)
			return -auto_mask;
		p++;
		if (*p == 0)
			return -auto_mask;
	}
	*ep = p;
	return auto_mask;
}

static int scan_ip_mask(const char *ipm, unsigned int *ip, unsigned int *mask)
{
	int i;
	unsigned int msk;

	i = scan_ip(&ipm, ip, '/');
	if (i < 0)
		return i;
	if (*ipm) {
		const char *p = ipm;

		i = 0;
		while (*p) {
			if (*p < '0' || *p > '9') {
				if (*p == '.') {
					i = scan_ip(&ipm, mask, 0);
					return i != 32;
				}
				return -1;
			}
			i *= 10;
			i += *p - '0';
			p++;
		}
	}
	if (i > 32 || i < 0)
		return -1;
	msk = 0x80000000;
	*mask = 0;
	while (i > 0) {
		*mask |= msk;
		msk >>= 1;
		i--;
	}
	return 0;
}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
static void free_config_lines(Htaccess **pprev)
{
	Htaccess *prev = *pprev;

	while (prev) {
		Htaccess *cur = prev;

		prev = cur->next;
		free(cur);
	}
	*pprev = NULL;
}
#endif

/* flag */
#define FIRST_PARSE          0
#define SUBDIR_PARSE         1
#define SIGNALED_PARSE       2
#define FIND_FROM_HTTPD_ROOT 3
/****************************************************************************
 *
 > $Function: parse_conf()
 *
 * $Description: parse configuration file into in-memory linked list.
 *
 * The first non-white character is examined to determine if the config line
 * is one of the following:
 *    .ext:mime/type   # new mime type not compiled into httpd
 *    [adAD]:from      # ip address allow/deny, * for wildcard
 *    /path:user:pass  # username/password
 *
 * Any previous IP rules are discarded.
 * If the flag argument is not SUBDIR_PARSE then all /path and mime rules
 * are also discarded.  That is, previous settings are retained if flag is
 * SUBDIR_PARSE.
 *
 * $Parameters:
 *      (const char *) path . . null for ip address checks, path for password
 *                              checks.
 *      (int) flag  . . . . . . the source of the parse request.
 *
 * $Return: (None)
 *
 ****************************************************************************/
static void parse_conf(const char *path, int flag)
{
	FILE *f;
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	Htaccess *prev;
#endif
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
	Htaccess *cur;
#endif

	const char *cf = config->configFile;
	char buf[160];
	char *p0 = NULL;
	char *c, *p;

	/* free previous ip setup if present */
	Htaccess_IP *pip = config->ip_a_d;

	while (pip) {
		Htaccess_IP *cur_ipl = pip;

		pip = cur_ipl->next;
		free(cur_ipl);
	}
	config->ip_a_d = NULL;

	config->flg_deny_all = 0;

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
	/* retain previous auth and mime config only for subdir parse */
	if (flag != SUBDIR_PARSE) {
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		free_config_lines(&config->auth);
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
		free_config_lines(&config->mime_a);
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
		free_config_lines(&config->script_i);
#endif
	}
#endif

	if (flag == SUBDIR_PARSE || cf == NULL) {
		cf = alloca(strlen(path) + sizeof(httpd_conf) + 2);
		if (cf == NULL) {
			if (flag == FIRST_PARSE)
			bb_error_msg_and_die(bb_msg_memory_exhausted);
			return;
		}
		sprintf((char *)cf, "%s/%s", path, httpd_conf);
	}

	while ((f = fopen(cf, "r")) == NULL) {
		if (flag == SUBDIR_PARSE || flag == FIND_FROM_HTTPD_ROOT) {
			/* config file not found, no changes to config */
			return;
		}
		if (config->configFile && flag == FIRST_PARSE) /* if -c option given */
			bb_perror_msg_and_die("%s", cf);
		flag = FIND_FROM_HTTPD_ROOT;
		cf = httpd_conf;
	}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		prev = config->auth;
#endif
		/* This could stand some work */
	while ((p0 = fgets(buf, sizeof(buf), f)) != NULL) {
		c = NULL;
		for (p = p0; *p0 != 0 && *p0 != '#'; p0++) {
			if (!isspace(*p0)) {
				*p++ = *p0;
				if (*p0 == ':' && c == NULL)
				c = p;
			}
		}
		*p = 0;

		/* test for empty or strange line */
		if (c == NULL || *c == 0)
			continue;
		p0 = buf;
		if (*p0 == 'd')
				*p0 = 'D';
		if (*c == '*') {
			if (*p0 == 'D') {
				/* memorize deny all */
				config->flg_deny_all++;
			}
			/* skip default other "word:*" config lines */
			continue;
		}

		if (*p0 == 'a')
			*p0 = 'A';
		else if (*p0 != 'D' && *p0 != 'A'
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
			 && *p0 != '/'
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
			 && *p0 != '.'
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
			 && *p0 != '*'
#endif
			)
			 continue;
		if (*p0 == 'A' || *p0 == 'D') {
			/* storing current config IP line */
			pip = xzalloc(sizeof(Htaccess_IP));
			if (pip) {
				if (scan_ip_mask(c, &(pip->ip), &(pip->mask))) {
					/* syntax IP{/mask} error detected, protect all */
					*p0 = 'D';
					pip->mask = 0;
				}
				pip->allow_deny = *p0;
				if (*p0 == 'D') {
					/* Deny:form_IP move top */
					pip->next = config->ip_a_d;
					config->ip_a_d = pip;
				} else {
					/* add to bottom A:form_IP config line */
					Htaccess_IP *prev_IP = config->ip_a_d;

					if (prev_IP == NULL) {
						config->ip_a_d = pip;
					} else {
						while (prev_IP->next)
							prev_IP = prev_IP->next;
						prev_IP->next = pip;
					}
				}
			}
			continue;
		}
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		if (*p0 == '/') {
			/* make full path from httpd root / curent_path / config_line_path */
			cf = flag == SUBDIR_PARSE ? path : "";
			p0 = malloc(strlen(cf) + (c - buf) + 2 + strlen(c));
			if (p0 == NULL)
				continue;
			c[-1] = 0;
			sprintf(p0, "/%s%s", cf, buf);

			/* another call bb_simplify_path */
			cf = p = p0;

			do {
				if (*p == '/') {
					if (*cf == '/') {    /* skip duplicate (or initial) slash */
						continue;
					} else if (*cf == '.') {
						if (cf[1] == '/' || cf[1] == 0) { /* remove extra '.' */
							continue;
						} else if ((cf[1] == '.') && (cf[2] == '/' || cf[2] == 0)) {
							++cf;
							if (p > p0) {
								while (*--p != '/') /* omit previous dir */;
							}
							continue;
						}
					}
				}
				*++p = *cf;
			} while (*++cf);

			if ((p == p0) || (*p != '/')) {      /* not a trailing slash */
				++p;                             /* so keep last character */
			}
			*p = 0;
			sprintf(p0, "%s:%s", p0, c);
		}
#endif

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
		/* storing current config line */
		cur = xzalloc(sizeof(Htaccess) + strlen(p0));
		if (cur) {
			cf = strcpy(cur->before_colon, p0);
			c = strchr(cf, ':');
			*c++ = 0;
			cur->after_colon = c;
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
			if (*cf == '.') {
				/* config .mime line move top for overwrite previous */
				cur->next = config->mime_a;
				config->mime_a = cur;
				continue;
			}
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
			if (*cf == '*' && cf[1] == '.') {
				/* config script interpreter line move top for overwrite previous */
				cur->next = config->script_i;
				config->script_i = cur;
				continue;
			}
#endif
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
			free(p0);
			if (prev == NULL) {
				/* first line */
				config->auth = prev = cur;
			} else {
				/* sort path, if current lenght eq or bigger then move up */
				Htaccess *prev_hti = config->auth;
				size_t l = strlen(cf);
				Htaccess *hti;

				for (hti = prev_hti; hti; hti = hti->next) {
					if (l >= strlen(hti->before_colon)) {
						/* insert before hti */
						cur->next = hti;
						if (prev_hti != hti) {
							prev_hti->next = cur;
						} else {
							/* insert as top */
							config->auth = cur;
						}
						break;
					}
					if (prev_hti != hti)
						prev_hti = prev_hti->next;
				}
				if (!hti) {       /* not inserted, add to bottom */
					prev->next = cur;
					prev = cur;
				}
			}
#endif
		}
#endif
	 }
	 fclose(f);
}

#if ENABLE_FEATURE_HTTPD_ENCODE_URL_STR
/****************************************************************************
 *
 > $Function: encodeString()
 *
 * $Description: Given a string, html encode special characters.
 *   This is used for the -e command line option to provide an easy way
 *   for scripts to encode result data without confusing browsers.  The
 *   returned string pointer is memory allocated by malloc().
 *
 * $Parameters:
 *      (const char *) string . . The first string to encode.
 *
 * $Return: (char *) . . . .. . . A pointer to the encoded string.
 *
 * $Errors: Returns a null string ("") if memory is not available.
 *
 ****************************************************************************/
static char *encodeString(const char *string)
{
	/* take the simple route and encode everything */
	/* could possibly scan once to get length.     */
	int len = strlen(string);
	char *out = xmalloc(len * 6 + 1);
	char *p = out;
	char ch;

	while ((ch = *string++)) {
		// very simple check for what to encode
		if (isalnum(ch)) *p++ = ch;
		else p += sprintf(p, "&#%d;", (unsigned char) ch);
	}
	*p = '\0';
	return out;
}
#endif          /* FEATURE_HTTPD_ENCODE_URL_STR */

/****************************************************************************
 *
 > $Function: decodeString()
 *
 * $Description: Given a URL encoded string, convert it to plain ascii.
 *   Since decoding always makes strings smaller, the decode is done in-place.
 *   Thus, callers should strdup() the argument if they do not want the
 *   argument modified.  The return is the original pointer, allowing this
 *   function to be easily used as arguments to other functions.
 *
 * $Parameters:
 *      (char *) string . . . The first string to decode.
 *      (int)    option_d . . 1 if called for httpd -d
 *
 * $Return: (char *)  . . . . A pointer to the decoded string (same as input).
 *
 * $Errors: None
 *
 ****************************************************************************/
static char *decodeString(char *orig, int option_d)
{
	/* note that decoded string is always shorter than original */
	char *string = orig;
	char *ptr = string;
	char c;

	while ((c = *ptr++) != '\0') {
		unsigned value1, value2;

		if (option_d && c == '+') {
			*string++ = ' ';
			continue;
		}
		if (c != '%') {
			*string++ = c;
			continue;
		}
		if (sscanf(ptr, "%1X", &value1) != 1
		 || sscanf(ptr+1, "%1X", &value2) != 1
		) {
			if (!option_d)
				return NULL;
			*string++ = '%';
			continue;
		}
		value1 = value1 * 16 + value2;
		if (!option_d && (value1 == '/' || value1 == '\0')) {
			/* caller takes it as indication of invalid
			 * (dangerous wrt exploits) chars */
			return orig + 1;
		}
		*string++ = value1;
		ptr += 2;
	}
	*string = '\0';
	return orig;
}


#if ENABLE_FEATURE_HTTPD_CGI
/****************************************************************************
 * setenv helpers
 ****************************************************************************/
static void setenv1(const char *name, const char *value)
{
	if (!value)
		value = "";
	setenv(name, value, 1);
}
static void setenv_long(const char *name, long value)
{
	char buf[sizeof(value)*3 + 1];
	sprintf(buf, "%ld", value);
	setenv(name, buf, 1);
}
#endif

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
/****************************************************************************
 *
 > $Function: decodeBase64()
 *
 > $Description: Decode a base 64 data stream as per rfc1521.
 *    Note that the rfc states that none base64 chars are to be ignored.
 *    Since the decode always results in a shorter size than the input, it is
 *    OK to pass the input arg as an output arg.
 *
 * $Parameter:
 *      (char *) Data . . . . A pointer to a base64 encoded string.
 *                            Where to place the decoded data.
 *
 * $Return: void
 *
 * $Errors: None
 *
 ****************************************************************************/
static void decodeBase64(char *Data)
{

	const unsigned char *in = (const unsigned char *)Data;
	// The decoded size will be at most 3/4 the size of the encoded
	unsigned long ch = 0;
	int i = 0;

	while (*in) {
		int t = *in++;

		if (t >= '0' && t <= '9')
			t = t - '0' + 52;
		else if (t >= 'A' && t <= 'Z')
			t = t - 'A';
		else if (t >= 'a' && t <= 'z')
			t = t - 'a' + 26;
		else if (t == '+')
			t = 62;
		else if (t == '/')
			t = 63;
		else if (t == '=')
			t = 0;
		else
			continue;

		ch = (ch << 6) | t;
		i++;
		if (i == 4) {
			*Data++ = (char) (ch >> 16);
			*Data++ = (char) (ch >> 8);
			*Data++ = (char) ch;
			i = 0;
		}
	}
	*Data = 0;
}
#endif


/****************************************************************************
 *
 > $Function: openServer()
 *
 * $Description: create a listen server socket on the designated port.
 *
 * $Return: (int)  . . . A connection socket. -1 for errors.
 *
 * $Errors: None
 *
 ****************************************************************************/
static int openServer(void)
{
	int fd;

	/* create the socket right now */
	fd = create_and_bind_stream_or_die(NULL, config->port);
	xlisten(fd, 9);
	return fd;
}

/****************************************************************************
 *
 > $Function: sendHeaders()
 *
 * $Description: Create and send HTTP response headers.
 *   The arguments are combined and sent as one write operation.  Note that
 *   IE will puke big-time if the headers are not sent in one packet and the
 *   second packet is delayed for any reason.
 *
 * $Parameter:
 *      (HttpResponseNum) responseNum . . . The result code to send.
 *
 * $Return: (int)  . . . . writing errors
 *
 ****************************************************************************/
static int sendHeaders(HttpResponseNum responseNum)
{
	char *buf = config->buf;
	const char *responseString = "";
	const char *infoString = 0;
	const char *mime_type;
	unsigned i;
	time_t timer = time(0);
	char timeStr[80];
	int len;
	enum {
		numNames = sizeof(httpResponseNames) / sizeof(httpResponseNames[0])
	};

	for (i = 0; i < numNames; i++) {
		if (httpResponseNames[i].type == responseNum) {
			responseString = httpResponseNames[i].name;
			infoString = httpResponseNames[i].info;
			break;
		}
	}
	/* error message is HTML */
	mime_type = responseNum == HTTP_OK ?
				config->found_mime_type : "text/html";

	/* emit the current date */
	strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&timer));
	len = sprintf(buf,
			"HTTP/1.0 %d %s\r\nContent-type: %s\r\n"
			"Date: %s\r\nConnection: close\r\n",
			responseNum, responseString, mime_type, timeStr);

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	if (responseNum == HTTP_UNAUTHORIZED) {
		len += sprintf(buf+len,
				"WWW-Authenticate: Basic realm=\"%s\"\r\n",
				config->realm);
	}
#endif
	if (responseNum == HTTP_MOVED_TEMPORARILY) {
		len += sprintf(buf+len, "Location: %s/%s%s\r\n",
				config->found_moved_temporarily,
				(config->query ? "?" : ""),
				(config->query ? config->query : ""));
	}

	if (config->ContentLength != -1) {    /* file */
		strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&config->last_mod));
		len += sprintf(buf+len, "Last-Modified: %s\r\n%s %"OFF_FMT"d\r\n",
			timeStr, "Content-length:", config->ContentLength);
	}
	strcat(buf, "\r\n");
	len += 2;
	if (infoString) {
		len += sprintf(buf+len,
				"<HEAD><TITLE>%d %s</TITLE></HEAD>\n"
				"<BODY><H1>%d %s</H1>\n%s\n</BODY>\n",
				responseNum, responseString,
				responseNum, responseString, infoString);
	}
	if (DEBUG)
		fprintf(stderr, "headers: '%s'\n", buf);
	i = config->accepted_socket;
	if (i == 0) i++; /* write to fd# 1 in inetd mode */
	return full_write(i, buf, len);
}

/****************************************************************************
 *
 > $Function: getLine()
 *
 * $Description: Read from the socket until an end of line char found.
 *
 *   Characters are read one at a time until an eol sequence is found.
 *
 * $Return: (int) . . . . number of characters read.  -1 if error.
 *
 ****************************************************************************/
static int getLine(void)
{
	int count = 0;
	char *buf = config->buf;

	while (read(config->accepted_socket, buf + count, 1) == 1) {
		if (buf[count] == '\r') continue;
		if (buf[count] == '\n') {
			buf[count] = 0;
			return count;
		}
		if (count < (MAX_MEMORY_BUFF-1))      /* check overflow */
			count++;
	}
	if (count) return count;
	else return -1;
}

#if ENABLE_FEATURE_HTTPD_CGI
/****************************************************************************
 *
 > $Function: sendCgi()
 *
 * $Description: Execute a CGI script and send it's stdout back
 *
 *   Environment variables are set up and the script is invoked with pipes
 *   for stdin/stdout.  If a post is being done the script is fed the POST
 *   data in addition to setting the QUERY_STRING variable (for GETs or POSTs).
 *
 * $Parameters:
 *      (const char *) url . . . . . . The requested URL (with leading /).
 *      (int bodyLen)  . . . . . . . . Length of the post body.
 *      (const char *cookie) . . . . . For set HTTP_COOKIE.
 *      (const char *content_type) . . For set CONTENT_TYPE.
 *
 * $Return: (char *)  . . . . A pointer to the decoded string (same as input).
 *
 * $Errors: None
 *
 ****************************************************************************/
static int sendCgi(const char *url,
		const char *request, int bodyLen, const char *cookie,
		const char *content_type)
{
	int fromCgi[2];  /* pipe for reading data from CGI */
	int toCgi[2];    /* pipe for sending data to CGI */

	static char * argp[] = { 0, 0 };
	int pid = 0;
	int inFd;
	int outFd;
	int buf_count;
	int status;
	size_t post_read_size, post_read_idx;

	if (pipe(fromCgi) != 0)
		return 0;
	if (pipe(toCgi) != 0)
		return 0;

/*
 * Note: We can use vfork() here in the no-mmu case, although
 * the child modifies the parent's variables, due to:
 * 1) The parent does not use the child-modified variables.
 * 2) The allocated memory (in the child) is freed when the process
 *    exits. This happens instantly after the child finishes,
 *    since httpd is run from inetd (and it can't run standalone
 *    in uClinux).
 */
#ifdef BB_NOMMU
	pid = vfork();
#else
	pid = fork();
#endif
	if (pid < 0)
		return 0;

	if (!pid) {
		/* child process */
		char *script;
		char *purl;
		char realpath_buff[MAXPATHLEN];

		if (config->accepted_socket > 1)
			close(config->accepted_socket);
		if (config->server_socket > 1)
			close(config->server_socket);

		dup2(toCgi[0], 0);  // replace stdin with the pipe
		dup2(fromCgi[1], 1);  // replace stdout with the pipe
		/* Huh? User seeing stderr can be a security problem...
		 * and if CGI really wants that, it can always dup2(1,2)...
		if (!DEBUG)
			dup2(fromCgi[1], 2);  // replace stderr with the pipe
		*/
		/* I think we cannot inadvertently close 0, 1 here... */
		close(toCgi[0]);
		close(toCgi[1]);
		close(fromCgi[0]);
		close(fromCgi[1]);

		/*
		 * Find PATH_INFO.
		 */
		xfunc_error_retval = 242;
		purl = xstrdup(url);
		script = purl;
		while ((script = strchr(script + 1, '/')) != NULL) {
			/* have script.cgi/PATH_INFO or dirs/script.cgi[/PATH_INFO] */
			struct stat sb;

			*script = '\0';
			if (is_directory(purl + 1, 1, &sb) == 0) {
				/* not directory, found script.cgi/PATH_INFO */
				*script = '/';
				break;
			}
			*script = '/';          /* is directory, find next '/' */
		}
		setenv1("PATH_INFO", script);   /* set /PATH_INFO or "" */
		/* setenv1("PATH", getenv("PATH")); redundant */
		setenv1("REQUEST_METHOD", request);
		if (config->query) {
			char *uri = alloca(strlen(purl) + 2 + strlen(config->query));
			if (uri)
				sprintf(uri, "%s?%s", purl, config->query);
			setenv1("REQUEST_URI", uri);
		} else {
			setenv1("REQUEST_URI", purl);
		}
		if (script != NULL)
			*script = '\0';         /* cut off /PATH_INFO */
		 /* SCRIPT_FILENAME required by PHP in CGI mode */
		if (!realpath(purl + 1, realpath_buff))
			goto error_execing_cgi;
		setenv1("SCRIPT_FILENAME", realpath_buff);
		/* set SCRIPT_NAME as full path: /cgi-bin/dirs/script.cgi */
		setenv1("SCRIPT_NAME", purl);
		/* http://hoohoo.ncsa.uiuc.edu/cgi/env.html:
		 * QUERY_STRING: The information which follows the ? in the URL
		 * which referenced this script. This is the query information.
		 * It should not be decoded in any fashion. This variable
		 * should always be set when there is query information,
		 * regardless of command line decoding. */
		/* (Older versions of bbox seem to do some decoding) */
		setenv1("QUERY_STRING", config->query);
		setenv1("SERVER_SOFTWARE", httpdVersion);
		putenv((char*)"SERVER_PROTOCOL=HTTP/1.0");
		putenv((char*)"GATEWAY_INTERFACE=CGI/1.1");
		/* Having _separate_ variables for IP and port defeats
		 * the purpose of having socket abstraction. Which "port"
		 * are you using on Unix domain socket?
		 * IOW - REMOTE_PEER="1.2.3.4:56" makes much more sense.
		 * Oh well... */
		{
			char *p = config->rmt_ip_str ? : (char*)"";
			char *cp = strrchr(p, ':');
			if (ENABLE_FEATURE_IPV6 && cp && strchr(cp, ']'))
				cp = NULL;
			if (cp) *cp = '\0'; /* delete :PORT */
			setenv1("REMOTE_ADDR", p);
		}
#if ENABLE_FEATURE_HTTPD_SET_REMOTE_PORT_TO_ENV
		setenv_long("REMOTE_PORT", config->port);
#endif
		if (bodyLen)
			setenv_long("CONTENT_LENGTH", bodyLen);
		if (cookie)
			setenv1("HTTP_COOKIE", cookie);
		if (content_type)
			setenv1("CONTENT_TYPE", content_type);
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		if (config->remoteuser) {
			setenv1("REMOTE_USER", config->remoteuser);
			putenv((char*)"AUTH_TYPE=Basic");
		}
#endif
		if (config->referer)
			setenv1("HTTP_REFERER", config->referer);

		/* set execve argp[0] without path */
		argp[0] = strrchr(purl, '/') + 1;
		/* but script argp[0] must have absolute path and chdiring to this */
		script = strrchr(realpath_buff, '/');
		if (!script)
			goto error_execing_cgi;
		*script = '\0';
		if (chdir(realpath_buff) == 0) {
			// Now run the program.  If it fails,
			// use _exit() so no destructors
			// get called and make a mess.
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
			char *interpr = NULL;
			char *suffix = strrchr(purl, '.');

			if (suffix) {
				Htaccess *cur;
				for (cur = config->script_i; cur; cur = cur->next) {
					if (strcmp(cur->before_colon + 1, suffix) == 0) {
						interpr = cur->after_colon;
						break;
					}
				}
			}
#endif
			*script = '/';
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
			if (interpr)
				execv(interpr, argp);
			else
#endif
				execv(realpath_buff, argp);
		}
 error_execing_cgi:
		/* send to stdout (even if we are not from inetd) */
		config->accepted_socket = 1;
		sendHeaders(HTTP_NOT_FOUND);
		_exit(242);
	} /* end child */

	/* parent process */

	buf_count = 0;
	post_read_size = 0;
	post_read_idx = 0; /* for gcc */
	inFd = fromCgi[0];
	outFd = toCgi[1];
	close(fromCgi[1]);
	close(toCgi[0]);
	signal(SIGPIPE, SIG_IGN);

	while (1) {
		fd_set readSet;
		fd_set writeSet;
		char wbuf[128];
		int nfound;
		int count;

		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);
		FD_SET(inFd, &readSet);
		if (bodyLen > 0 || post_read_size > 0) {
			FD_SET(outFd, &writeSet);
			nfound = outFd > inFd ? outFd : inFd;
			if (post_read_size == 0) {
				FD_SET(config->accepted_socket, &readSet);
				if (nfound < config->accepted_socket)
					nfound = config->accepted_socket;
			}
			/* Now wait on the set of sockets! */
			nfound = select(nfound + 1, &readSet, &writeSet, NULL, NULL);
		} else {
			if (!bodyLen) {
				close(outFd); /* no more POST data to CGI */
				bodyLen = -1;
			}
			nfound = select(inFd + 1, &readSet, NULL, NULL, NULL);
		}

		if (nfound <= 0) {
			if (waitpid(pid, &status, WNOHANG) <= 0) {
				/* Weird. CGI didn't exit and no fd's
				 * are ready, yet select returned?! */
				continue;
			}
			close(inFd);
			if (DEBUG && WIFEXITED(status))
				bb_error_msg("piped has exited with status=%d", WEXITSTATUS(status));
			if (DEBUG && WIFSIGNALED(status))
				bb_error_msg("piped has exited with signal=%d", WTERMSIG(status));
			break;
		}

		if (post_read_size > 0 && FD_ISSET(outFd, &writeSet)) {
			/* Have data from peer and can write to CGI */
		// huh? why full_write? what if we will block?
		// (imagine that CGI does not read its stdin...)
			count = full_write(outFd, wbuf + post_read_idx, post_read_size);
			if (count > 0) {
				post_read_idx += count;
				post_read_size -= count;
			} else {
				post_read_size = bodyLen = 0; /* broken pipe to CGI */
			}
		} else if (bodyLen > 0 && post_read_size == 0
		 && FD_ISSET(config->accepted_socket, &readSet)
		) {
			/* We expect data, prev data portion is eaten by CGI
			 * and there *is* data to read from the peer
			 * (POSTDATA?) */
			count = bodyLen > (int)sizeof(wbuf) ? (int)sizeof(wbuf) : bodyLen;
			count = safe_read(config->accepted_socket, wbuf, count);
			if (count > 0) {
				post_read_size = count;
				post_read_idx = 0;
				bodyLen -= count;
			} else {
				bodyLen = 0;    /* closed */
			}
		}

#define PIPESIZE PIPE_BUF
#if PIPESIZE >= MAX_MEMORY_BUFF
# error "PIPESIZE >= MAX_MEMORY_BUFF"
#endif
		if (FD_ISSET(inFd, &readSet)) {
			/* There is something to read from CGI */
			int s = config->accepted_socket;
			char *rbuf = config->buf;

			/* Are we still buffering CGI output? */
			if (buf_count >= 0) {
				static const char HTTP_200[] = "HTTP/1.0 200 OK\r\n";
				/* Must use safe_read, not full_read, because
				 * CGI may output a few first bytes and then wait
				 * for POSTDATA without closing stdout.
				 * With full_read we may wait here forever. */
				count = safe_read(inFd, rbuf + buf_count, PIPESIZE - 4);
				if (count <= 0) {
					/* eof (or error) and there was no "HTTP",
					 * so add one and write out the received data */
					if (buf_count) {
						full_write(s, HTTP_200, sizeof(HTTP_200)-1);
						full_write(s, rbuf, buf_count);
					}
					break;  /* closed */
				}
				buf_count += count;
				count = 0;
				if (buf_count >= 4) {
					/* check to see if CGI added "HTTP" */
					if (memcmp(rbuf, HTTP_200, 4) != 0) {
						/* there is no "HTTP", do it ourself */
						if (full_write(s, HTTP_200, sizeof(HTTP_200)-1) != sizeof(HTTP_200)-1)
							break;
					}
					/* example of valid CGI without "Content-type:"
					 * echo -en "HTTP/1.0 302 Found\r\n"
					 * echo -en "Location: http://www.busybox.net\r\n"
					 * echo -en "\r\n"
					if (!strstr(rbuf, "ontent-")) {
						full_write(s, "Content-type: text/plain\r\n\r\n", 28);
					}
					 */
					count = buf_count;
					buf_count = -1; /* buffering off */
				}
			} else {
				count = safe_read(inFd, rbuf, PIPESIZE);
				if (count <= 0)
					break;  /* eof (or error) */
			}
			if (full_write(s, rbuf, count) != count)
				break;
			if (DEBUG)
				fprintf(stderr, "cgi read %d bytes: '%.*s'\n", count, count, rbuf);
		} /* if (FD_ISSET(inFd)) */
	} /* while (1) */
	return 0;
}
#endif          /* FEATURE_HTTPD_CGI */

/****************************************************************************
 *
 > $Function: sendFile()
 *
 * $Description: Send a file response to a HTTP request
 *
 * $Parameter:
 *      (const char *) url . . The URL requested.
 *
 * $Return: (int)  . . . . . . Always 0.
 *
 ****************************************************************************/
static int sendFile(const char *url)
{
	char * suffix;
	int  f;
	const char * const * table;
	const char * try_suffix;

	suffix = strrchr(url, '.');

	for (table = suffixTable; *table; table += 2)
		if (suffix != NULL && (try_suffix = strstr(*table, suffix)) != 0) {
			try_suffix += strlen(suffix);
			if (*try_suffix == 0 || *try_suffix == '.')
				break;
		}
	/* also, if not found, set default as "application/octet-stream";  */
	config->found_mime_type = table[1];
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
	if (suffix) {
		Htaccess * cur;

		for (cur = config->mime_a; cur; cur = cur->next) {
			if (strcmp(cur->before_colon, suffix) == 0) {
				config->found_mime_type = cur->after_colon;
				break;
			}
		}
	}
#endif  /* FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES */

	if (DEBUG)
		fprintf(stderr, "sending file '%s' content-type: %s\n",
			url, config->found_mime_type);

	f = open(url, O_RDONLY);
	if (f >= 0) {
		int count;
		char *buf = config->buf;

		sendHeaders(HTTP_OK);
		/* TODO: sendfile() */
		while ((count = full_read(f, buf, MAX_MEMORY_BUFF)) > 0) {
			int fd = config->accepted_socket;
			if (fd == 0) fd++; /* write to fd# 1 in inetd mode */
			if (full_write(fd, buf, count) != count)
				break;
		}
		close(f);
	} else {
		if (DEBUG)
			bb_perror_msg("cannot open '%s'", url);
		sendHeaders(HTTP_NOT_FOUND);
	}

	return 0;
}

static int checkPermIP(void)
{
	Htaccess_IP * cur;

	/* This could stand some work */
	for (cur = config->ip_a_d; cur; cur = cur->next) {
#if ENABLE_FEATURE_HTTPD_CGI && DEBUG
		fprintf(stderr, "checkPermIP: '%s' ? ", config->rmt_ip_str);
#endif
#if DEBUG
		fprintf(stderr, "'%u.%u.%u.%u/%u.%u.%u.%u'\n",
			(unsigned char)(cur->ip >> 24),
			(unsigned char)(cur->ip >> 16),
			(unsigned char)(cur->ip >> 8),
			(unsigned char)(cur->ip),
			(unsigned char)(cur->mask >> 24),
			(unsigned char)(cur->mask >> 16),
			(unsigned char)(cur->mask >> 8),
			(unsigned char)(cur->mask)
		);
#endif
		if ((config->rmt_ip & cur->mask) == cur->ip)
			return cur->allow_deny == 'A';   /* Allow/Deny */
	}

	/* if unconfigured, return 1 - access from all */
	return !config->flg_deny_all;
}

/****************************************************************************
 *
 > $Function: checkPerm()
 *
 * $Description: Check the permission file for access password protected.
 *
 *   If config file isn't present, everything is allowed.
 *   Entries are of the form you can see example from header source
 *
 * $Parameters:
 *      (const char *) path  . . . . The file path.
 *      (const char *) request . . . User information to validate.
 *
 * $Return: (int)  . . . . . . . . . 1 if request OK, 0 otherwise.
 *
 ****************************************************************************/

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
static int checkPerm(const char *path, const char *request)
{
	Htaccess * cur;
	const char *p;
	const char *p0;

	const char *prev = NULL;

	/* This could stand some work */
	for (cur = config->auth; cur; cur = cur->next) {
		size_t l;

		p0 = cur->before_colon;
		if (prev != NULL && strcmp(prev, p0) != 0)
			continue;       /* find next identical */
		p = cur->after_colon;
		if (DEBUG)
			fprintf(stderr, "checkPerm: '%s' ? '%s'\n", p0, request);

		l = strlen(p0);
		if (strncmp(p0, path, l) == 0
		 && (l == 1 || path[l] == '/' || path[l] == '\0')
		) {
			char *u;
			/* path match found.  Check request */
			/* for check next /path:user:password */
			prev = p0;
			u = strchr(request, ':');
			if (u == NULL) {
				/* bad request, ':' required */
				break;
			}

			if (ENABLE_FEATURE_HTTPD_AUTH_MD5) {
				char *cipher;
				char *pp;

				if (strncmp(p, request, u-request) != 0) {
					/* user uncompared */
					continue;
				}
				pp = strchr(p, ':');
				if (pp && pp[1] == '$' && pp[2] == '1' &&
						pp[3] == '$' && pp[4]) {
					pp++;
					cipher = pw_encrypt(u+1, pp);
					if (strcmp(cipher, pp) == 0)
						goto set_remoteuser_var;   /* Ok */
					/* unauthorized */
					continue;
				}
			}

			if (strcmp(p, request) == 0) {
set_remoteuser_var:
				config->remoteuser = strdup(request);
				if (config->remoteuser)
					config->remoteuser[(u - request)] = 0;
				return 1;   /* Ok */
			}
			/* unauthorized */
		}
	}   /* for */

	return prev == NULL;
}

#endif  /* FEATURE_HTTPD_BASIC_AUTH */

/****************************************************************************
 *
 > $Function: handle_sigalrm()
 *
 * $Description: Handle timeouts
 *
 ****************************************************************************/

static void handle_sigalrm(int sig)
{
	sendHeaders(HTTP_REQUEST_TIMEOUT);
	config->alarm_signaled = sig;
}

/****************************************************************************
 *
 > $Function: handleIncoming()
 *
 * $Description: Handle an incoming http request.
 *
 ****************************************************************************/
static void handleIncoming(void)
{
	char *buf = config->buf;
	char *url;
	char *purl;
	int  blank = -1;
	char *test;
	struct stat sb;
	int ip_allowed;
#if ENABLE_FEATURE_HTTPD_CGI
	const char *prequest = request_GET;
	unsigned long length = 0;
	char *cookie = 0;
	char *content_type = 0;
#endif
	fd_set s_fd;
	struct timeval tv;
	int retval;
	struct sigaction sa;

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	int credentials = -1;  /* if not required this is Ok */
#endif

	sa.sa_handler = handle_sigalrm;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0; /* no SA_RESTART */
	sigaction(SIGALRM, &sa, NULL);

	do {
		int count;

		(void) alarm(TIMEOUT);
		if (getLine() <= 0)
			break;  /* closed */

		purl = strpbrk(buf, " \t");
		if (purl == NULL) {
 BAD_REQUEST:
			sendHeaders(HTTP_BAD_REQUEST);
			break;
		}
		*purl = '\0';
#if ENABLE_FEATURE_HTTPD_CGI
		if (strcasecmp(buf, prequest) != 0) {
			prequest = "POST";
			if (strcasecmp(buf, prequest) != 0) {
				sendHeaders(HTTP_NOT_IMPLEMENTED);
				break;
			}
		}
#else
		if (strcasecmp(buf, request_GET) != 0) {
			sendHeaders(HTTP_NOT_IMPLEMENTED);
			break;
		}
#endif
		*purl = ' ';
		count = sscanf(purl, " %[^ ] HTTP/%d.%*d", buf, &blank);

		if (count < 1 || buf[0] != '/') {
			/* Garbled request/URL */
			goto BAD_REQUEST;
		}
		url = alloca(strlen(buf) + sizeof("/index.html"));
		if (url == NULL) {
			sendHeaders(HTTP_INTERNAL_SERVER_ERROR);
			break;
		}
		strcpy(url, buf);
		/* extract url args if present */
		test = strchr(url, '?');
		config->query = NULL;
		if (test) {
			*test++ = '\0';
			config->query = test;
		}

		test = decodeString(url, 0);
		if (test == NULL)
			goto BAD_REQUEST;
		if (test == url+1) {
			/* '/' or NUL is encoded */
			sendHeaders(HTTP_NOT_FOUND);
			break;
		}

		/* algorithm stolen from libbb bb_simplify_path(),
			 but don't strdup and reducing trailing slash and protect out root */
		purl = test = url;
		do {
			if (*purl == '/') {
				/* skip duplicate (or initial) slash */
				if (*test == '/') {
					continue;
				}
				if (*test == '.') {
					/* skip extra '.' */
					if (test[1] == '/' || test[1] == 0) {
						continue;
					} else
					/* '..': be careful */
					if (test[1] == '.' && (test[2] == '/' || test[2] == 0)) {
						++test;
						if (purl == url) {
							/* protect out root */
							goto BAD_REQUEST;
						}
						while (*--purl != '/') /* omit previous dir */;
						continue;
					}
				}
			}
			*++purl = *test;
		} while (*++test);
		*++purl = '\0';       /* so keep last character */
		test = purl;          /* end ptr */

		/* If URL is directory, adding '/' */
		if (test[-1] != '/') {
			if (is_directory(url + 1, 1, &sb)) {
				config->found_moved_temporarily = url;
			}
		}
		if (DEBUG)
			fprintf(stderr, "url='%s', args=%s\n", url, config->query);

		test = url;
		ip_allowed = checkPermIP();
		while (ip_allowed && (test = strchr(test + 1, '/')) != NULL) {
			/* have path1/path2 */
			*test = '\0';
			if (is_directory(url + 1, 1, &sb)) {
				/* may be having subdir config */
				parse_conf(url + 1, SUBDIR_PARSE);
				ip_allowed = checkPermIP();
			}
			*test = '/';
		}
		if (blank >= 0) {
			/* read until blank line for HTTP version specified, else parse immediate */
			while (1) {
				alarm(TIMEOUT);
				count = getLine();
				if (count <= 0)
					break;

				if (DEBUG)
					fprintf(stderr, "header: '%s'\n", buf);

#if ENABLE_FEATURE_HTTPD_CGI
				/* try and do our best to parse more lines */
				if ((STRNCASECMP(buf, "Content-length:") == 0)) {
					/* extra read only for POST */
					if (prequest != request_GET) {
						test = buf + sizeof("Content-length:")-1;
						if (!test[0])
							goto bail_out;
						errno = 0;
						/* not using strtoul: it ignores leading munis! */
						length = strtol(test, &test, 10);
						/* length is "ulong", but we need to pass it to int later */
						/* so we check for negative or too large values in one go: */
						/* (long -> ulong conv caused negatives to be seen as > INT_MAX) */
						if (test[0] || errno || length > INT_MAX)
							goto bail_out;
					}
				} else if ((STRNCASECMP(buf, "Cookie:") == 0)) {
					cookie = strdup(skip_whitespace(buf + sizeof("Cookie:")-1));
				} else if ((STRNCASECMP(buf, "Content-Type:") == 0)) {
					content_type = strdup(skip_whitespace(buf + sizeof("Content-Type:")-1));
				} else if ((STRNCASECMP(buf, "Referer:") == 0)) {
					config->referer = strdup(skip_whitespace(buf + sizeof("Referer:")-1));
				}
#endif

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
				if (STRNCASECMP(buf, "Authorization:") == 0) {
					/* We only allow Basic credentials.
					 * It shows up as "Authorization: Basic <userid:password>" where
					 * the userid:password is base64 encoded.
					 */
					test = skip_whitespace(buf + sizeof("Authorization:")-1);
					if (STRNCASECMP(test, "Basic") != 0)
						continue;
					test += sizeof("Basic")-1;
					/* decodeBase64() skips whitespace itself */
					decodeBase64(test);
					credentials = checkPerm(url, test);
				}
#endif          /* FEATURE_HTTPD_BASIC_AUTH */

			} /* while extra header reading */
		}
		alarm(0);
		if (config->alarm_signaled)
			break;

		if (strcmp(strrchr(url, '/') + 1, httpd_conf) == 0 || ip_allowed == 0) {
			/* protect listing [/path]/httpd_conf or IP deny */
#if ENABLE_FEATURE_HTTPD_CGI
 FORBIDDEN:		/* protect listing /cgi-bin */
#endif
			sendHeaders(HTTP_FORBIDDEN);
			break;
		}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		if (credentials <= 0 && checkPerm(url, ":") == 0) {
			sendHeaders(HTTP_UNAUTHORIZED);
			break;
		}
#endif

		if (config->found_moved_temporarily) {
			sendHeaders(HTTP_MOVED_TEMPORARILY);
			/* clear unforked memory flag */
			config->found_moved_temporarily = NULL;
			break;
		}

		test = url + 1;      /* skip first '/' */

#if ENABLE_FEATURE_HTTPD_CGI
		if (strncmp(test, "cgi-bin", 7) == 0) {
			if (test[7] == '/' && test[8] == 0)
				goto FORBIDDEN;     /* protect listing cgi-bin/ */
			sendCgi(url, prequest, length, cookie, content_type);
			break;
		}
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
		{
			char *suffix = strrchr(test, '.');
			if (suffix) {
				Htaccess *cur;
				for (cur = config->script_i; cur; cur = cur->next) {
					if (strcmp(cur->before_colon + 1, suffix) == 0) {
						sendCgi(url, prequest, length, cookie, content_type);
						goto bail_out;
					}
				}
			}
		}
#endif
		if (prequest != request_GET) {
			sendHeaders(HTTP_NOT_IMPLEMENTED);
			break;
		}
#endif  /* FEATURE_HTTPD_CGI */
		if (purl[-1] == '/')
			strcpy(purl, "index.html");
		if (stat(test, &sb) == 0) {
			/* It's a dir URL and there is index.html */
			config->ContentLength = sb.st_size;
			config->last_mod = sb.st_mtime;
		}
#if ENABLE_FEATURE_HTTPD_CGI
		else if (purl[-1] == '/') {
			/* It's a dir URL and there is no index.html
			 * Try cgi-bin/index.cgi */
			if (access("/cgi-bin/index.cgi"+1, X_OK) == 0) {
				purl[0] = '\0';
				config->query = url;
				sendCgi("/cgi-bin/index.cgi", prequest, length, cookie, content_type);
				break;
			}
		}
#endif  /* FEATURE_HTTPD_CGI */
		sendFile(test);
		config->ContentLength = -1;
	} while (0);

#if ENABLE_FEATURE_HTTPD_CGI
 bail_out:
#endif

	if (DEBUG)
		fprintf(stderr, "closing socket\n\n");
#if ENABLE_FEATURE_HTTPD_CGI
	free(cookie);
	free(content_type);
	free(config->referer);
	config->referer = NULL;
# if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	free(config->remoteuser);
	config->remoteuser = NULL;
# endif
#endif
	shutdown(config->accepted_socket, SHUT_WR);

	/* Properly wait for remote to closed */
	FD_ZERO(&s_fd);
	FD_SET(config->accepted_socket, &s_fd);

	do {
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		retval = select(config->accepted_socket + 1, &s_fd, NULL, NULL, &tv);
	} while (retval > 0 && read(config->accepted_socket, buf, sizeof(config->buf) > 0));

	shutdown(config->accepted_socket, SHUT_RD);
	/* In inetd case, we close fd 1 (stdout) here. We will exit soon anyway */
	close(config->accepted_socket);
}

/****************************************************************************
 *
 > $Function: miniHttpd()
 *
 * $Description: The main http server function.
 *
 *   Given an open socket fildes, listen for new connections and farm out
 *   the processing as a forked process.
 *
 * $Parameters:
 *      (int) server. . . The server socket fildes.
 *
 * $Return: (int) . . . . Always 0.
 *
 ****************************************************************************/
static int miniHttpd(int server)
{
	fd_set readfd, portfd;

	FD_ZERO(&portfd);
	FD_SET(server, &portfd);

	/* copy the ports we are watching to the readfd set */
	while (1) {
		int s;
		union {
			struct sockaddr sa;
			struct sockaddr_in sin;
			USE_FEATURE_IPV6(struct sockaddr_in6 sin6;)
		} fromAddr;
		socklen_t fromAddrLen = sizeof(fromAddr);

		/* Now wait INDEFINITELY on the set of sockets! */
		readfd = portfd;
		if (select(server + 1, &readfd, 0, 0, 0) <= 0)
			continue;
		if (!FD_ISSET(server, &readfd))
			continue;
		s = accept(server, &fromAddr.sa, &fromAddrLen);
		if (s < 0)
			continue;
		config->accepted_socket = s;
		config->rmt_ip = 0;
		config->port = 0;
#if ENABLE_FEATURE_HTTPD_CGI || DEBUG
		free(config->rmt_ip_str);
		config->rmt_ip_str = xmalloc_sockaddr2dotted(&fromAddr.sa, fromAddrLen);
#if DEBUG
		bb_error_msg("connection from '%s'", config->rmt_ip_str);
#endif
#endif /* FEATURE_HTTPD_CGI */
		if (fromAddr.sa.sa_family == AF_INET) {
			config->rmt_ip = ntohl(fromAddr.sin.sin_addr.s_addr);
			config->port = ntohs(fromAddr.sin.sin_port);
		}
#if ENABLE_FEATURE_IPV6
		if (fromAddr.sa.sa_family == AF_INET6) {
			//config->rmt_ip = ntohl(fromAddr.sin.sin_addr.s_addr);
			config->port = ntohs(fromAddr.sin6.sin6_port);
		}
#endif

		/* set the KEEPALIVE option to cull dead connections */
		setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &const_int_1, sizeof(const_int_1));

		if (DEBUG || fork() == 0) {
			/* child */
#if ENABLE_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
			/* protect reload config, may be confuse checking */
			signal(SIGHUP, SIG_IGN);
#endif
			handleIncoming();
			if (!DEBUG)
				exit(0);
		}
		close(s);
	} /* while (1) */
	return 0;
}

/* from inetd */
static int miniHttpd_inetd(void)
{
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		USE_FEATURE_IPV6(struct sockaddr_in6 sin6;)
	} fromAddr;
	socklen_t fromAddrLen = sizeof(fromAddr);

	getpeername(0, &fromAddr.sa, &fromAddrLen);
	config->rmt_ip = 0;
	config->port = 0;
#if ENABLE_FEATURE_HTTPD_CGI || DEBUG
	free(config->rmt_ip_str);
	config->rmt_ip_str = xmalloc_sockaddr2dotted(&fromAddr.sa, fromAddrLen);
#endif
	if (fromAddr.sa.sa_family == AF_INET) {
		config->rmt_ip = ntohl(fromAddr.sin.sin_addr.s_addr);
		config->port = ntohs(fromAddr.sin.sin_port);
	}
#if ENABLE_FEATURE_IPV6
	if (fromAddr.sa.sa_family == AF_INET6) {
		//config->rmt_ip = ntohl(fromAddr.sin.sin_addr.s_addr);
		config->port = ntohs(fromAddr.sin6.sin6_port);
	}
#endif
	handleIncoming();
	return 0;
}

#if ENABLE_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
static void sighup_handler(int sig)
{
	/* set and reset */
	struct sigaction sa;

	parse_conf(default_path_httpd_conf, sig == SIGHUP ? SIGNALED_PARSE : FIRST_PARSE);
	sa.sa_handler = sighup_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGHUP, &sa, NULL);
}
#endif

enum {
	c_opt_config_file = 0,
	d_opt_decode_url,
	h_opt_home_httpd,
	USE_FEATURE_HTTPD_ENCODE_URL_STR(e_opt_encode_url,)
	USE_FEATURE_HTTPD_BASIC_AUTH(    r_opt_realm     ,)
	USE_FEATURE_HTTPD_AUTH_MD5(      m_opt_md5       ,)
	USE_FEATURE_HTTPD_SETUID(        u_opt_setuid    ,)
	p_opt_port      ,
	p_opt_inetd     ,
	p_opt_foreground,
	OPT_CONFIG_FILE = 1 << c_opt_config_file,
	OPT_DECODE_URL  = 1 << d_opt_decode_url,
	OPT_HOME_HTTPD  = 1 << h_opt_home_httpd,
	OPT_ENCODE_URL  = USE_FEATURE_HTTPD_ENCODE_URL_STR((1 << e_opt_encode_url)) + 0,
	OPT_REALM       = USE_FEATURE_HTTPD_BASIC_AUTH(    (1 << r_opt_realm     )) + 0,
	OPT_MD5         = USE_FEATURE_HTTPD_AUTH_MD5(      (1 << m_opt_md5       )) + 0,
	OPT_SETUID      = USE_FEATURE_HTTPD_SETUID(        (1 << u_opt_setuid    )) + 0,
	OPT_PORT        = 1 << p_opt_port,
	OPT_INETD       = 1 << p_opt_inetd,
	OPT_FOREGROUND  = 1 << p_opt_foreground,
};

static const char httpd_opts[] = "c:d:h:"
	USE_FEATURE_HTTPD_ENCODE_URL_STR("e:")
	USE_FEATURE_HTTPD_BASIC_AUTH("r:")
	USE_FEATURE_HTTPD_AUTH_MD5("m:")
	USE_FEATURE_HTTPD_SETUID("u:")
	"p:if";


int httpd_main(int argc, char *argv[]);
int httpd_main(int argc, char *argv[])
{
	unsigned opt;
	const char *home_httpd = home;
	char *url_for_decode;
	USE_FEATURE_HTTPD_ENCODE_URL_STR(const char *url_for_encode;)
	const char *s_port;
	USE_FEATURE_HTTPD_SETUID(const char *s_ugid = NULL;)
	USE_FEATURE_HTTPD_SETUID(struct bb_uidgid_t ugid;)
	USE_FEATURE_HTTPD_AUTH_MD5(const char *pass;)

#if ENABLE_LOCALE_SUPPORT
	/* Undo busybox.c: we want to speak English in http (dates etc) */
	setlocale(LC_TIME, "C");
#endif

	config = xzalloc(sizeof(*config));
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	config->realm = "Web Server Authentication";
#endif
	config->port = 80;
	config->ContentLength = -1;

	opt = getopt32(argc, argv, httpd_opts,
			&(config->configFile), &url_for_decode, &home_httpd
			USE_FEATURE_HTTPD_ENCODE_URL_STR(, &url_for_encode)
			USE_FEATURE_HTTPD_BASIC_AUTH(, &(config->realm))
			USE_FEATURE_HTTPD_AUTH_MD5(, &pass)
			USE_FEATURE_HTTPD_SETUID(, &s_ugid)
			, &s_port
		);
	if (opt & OPT_DECODE_URL) {
		printf("%s", decodeString(url_for_decode, 1));
		return 0;
	}
#if ENABLE_FEATURE_HTTPD_ENCODE_URL_STR
	if (opt & OPT_ENCODE_URL) {
		printf("%s", encodeString(url_for_encode));
		return 0;
	}
#endif
#if ENABLE_FEATURE_HTTPD_AUTH_MD5
	if (opt & OPT_MD5) {
		puts(pw_encrypt(pass, "$1$"));
		return 0;
	}
#endif
	if (opt & OPT_PORT)
		config->port = xatou16(s_port);

#if ENABLE_FEATURE_HTTPD_SETUID
	if (opt & OPT_SETUID) {
		if (!get_uidgid(&ugid, s_ugid, 1))
			bb_error_msg_and_die("unrecognized user[:group] "
						"name '%s'", s_ugid);
	}
#endif

	xchdir(home_httpd);
	if (!(opt & OPT_INETD)) {
		signal(SIGCHLD, SIG_IGN);
		config->server_socket = openServer();
#if ENABLE_FEATURE_HTTPD_SETUID
		/* drop privileges */
		if (opt & OPT_SETUID) {
			if (ugid.gid != (gid_t)-1) {
				if (setgroups(1, &ugid.gid) == -1)
					bb_perror_msg_and_die("setgroups");
				xsetgid(ugid.gid);
			}
			xsetuid(ugid.uid);
		}
#endif
	}

#if ENABLE_FEATURE_HTTPD_CGI
	{
		char *p = getenv("PATH");
		p = xstrdup(p); /* if gets NULL, returns NULL */
		clearenv();
		if (p)
			setenv1("PATH", p);
		if (!(opt & OPT_INETD))
			setenv_long("SERVER_PORT", config->port);
	}
#endif

#if ENABLE_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
	sighup_handler(0);
#else
	parse_conf(default_path_httpd_conf, FIRST_PARSE);
#endif

	if (opt & OPT_INETD)
		return miniHttpd_inetd();

	if (!(opt & OPT_FOREGROUND))
		xdaemon(1, 0);     /* don't change current directory */
	return miniHttpd(config->server_socket);
}
