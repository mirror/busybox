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
 * When a url starts by "/cgi-bin/" it is assumed to be a cgi script.  The
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
 * E404:/path/e404.html # /path/e404.html is the 404 (not found) error page
 *
 * P:/url:[http://]hostname[:port]/new/path
 *                   # When /urlXXXXXX is requested, reverse proxy
 *                   # it to http://hostname[:port]/new/pathXXXXXX
 *
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
 * Custom error pages can contain an absolute path or be relative to
 * 'home_httpd'. Error pages are to be static files (no CGI or script). Error
 * page can only be defined in the root configuration file and are not taken
 * into account in local (directories) config files.
 *
 * If -c is not set, an attempt will be made to open the default
 * root configuration file.  If -c is set and the file is not found, the
 * server exits with an error.
 *
 */

#include "libbb.h"
#if ENABLE_FEATURE_HTTPD_USE_SENDFILE
#include <sys/sendfile.h>
#endif

//#define DEBUG 1
#define DEBUG 0

#define IOBUF_SIZE 8192    /* IO buffer */

/* amount of buffering in a pipe */
#ifndef PIPE_BUF
# define PIPE_BUF 4096
#endif
#if PIPE_BUF >= IOBUF_SIZE
# error "PIPE_BUF >= IOBUF_SIZE"
#endif

#define HEADER_READ_TIMEOUT 60

static const char default_path_httpd_conf[] ALIGN1 = "/etc";
static const char httpd_conf[] ALIGN1 = "httpd.conf";
static const char HTTP_200[] ALIGN1 = "HTTP/1.0 200 OK\r\n";

typedef struct has_next_ptr {
	struct has_next_ptr *next;
} has_next_ptr;

/* Must have "next" as a first member */
typedef struct Htaccess {
	struct Htaccess *next;
	char *after_colon;
	char before_colon[1];  /* really bigger, must be last */
} Htaccess;

/* Must have "next" as a first member */
typedef struct Htaccess_IP {
	struct Htaccess_IP *next;
	unsigned ip;
	unsigned mask;
	int allow_deny;
} Htaccess_IP;

/* Must have "next" as a first member */
typedef struct Htaccess_Proxy {
	struct Htaccess_Proxy *next;
	char *url_from;
	char *host_port;
	char *url_to;
} Htaccess_Proxy;

enum {
	HTTP_OK = 200,
	HTTP_PARTIAL_CONTENT = 206,
	HTTP_MOVED_TEMPORARILY = 302,
	HTTP_BAD_REQUEST = 400,       /* malformed syntax */
	HTTP_UNAUTHORIZED = 401, /* authentication needed, respond with auth hdr */
	HTTP_NOT_FOUND = 404,
	HTTP_FORBIDDEN = 403,
	HTTP_REQUEST_TIMEOUT = 408,
	HTTP_NOT_IMPLEMENTED = 501,   /* used for unrecognized requests */
	HTTP_INTERNAL_SERVER_ERROR = 500,
	HTTP_CONTINUE = 100,
#if 0   /* future use */
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
};

static const uint16_t http_response_type[] ALIGN2 = {
	HTTP_OK,
#if ENABLE_FEATURE_HTTPD_RANGES
	HTTP_PARTIAL_CONTENT,
#endif
	HTTP_MOVED_TEMPORARILY,
	HTTP_REQUEST_TIMEOUT,
	HTTP_NOT_IMPLEMENTED,
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	HTTP_UNAUTHORIZED,
#endif
	HTTP_NOT_FOUND,
	HTTP_BAD_REQUEST,
	HTTP_FORBIDDEN,
	HTTP_INTERNAL_SERVER_ERROR,
#if 0   /* not implemented */
	HTTP_CREATED,
	HTTP_ACCEPTED,
	HTTP_NO_CONTENT,
	HTTP_MULTIPLE_CHOICES,
	HTTP_MOVED_PERMANENTLY,
	HTTP_NOT_MODIFIED,
	HTTP_BAD_GATEWAY,
	HTTP_SERVICE_UNAVAILABLE,
#endif
};

static const struct {
	const char *name;
	const char *info;
} http_response[ARRAY_SIZE(http_response_type)] = {
	{ "OK", NULL },
#if ENABLE_FEATURE_HTTPD_RANGES
	{ "Partial Content", NULL },
#endif
	{ "Found", NULL },
	{ "Request Timeout", "No request appeared within 60 seconds" },
	{ "Not Implemented", "The requested method is not recognized" },
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	{ "Unauthorized", "" },
#endif
	{ "Not Found", "The requested URL was not found" },
	{ "Bad Request", "Unsupported method" },
	{ "Forbidden", ""  },
	{ "Internal Server Error", "Internal Server Error" },
#if 0   /* not implemented */
	{ "Created" },
	{ "Accepted" },
	{ "No Content" },
	{ "Multiple Choices" },
	{ "Moved Permanently" },
	{ "Not Modified" },
	{ "Bad Gateway", "" },
	{ "Service Unavailable", "" },
#endif
};


struct globals {
	int verbose;            /* must be int (used by getopt32) */
	smallint flg_deny_all;

	unsigned rmt_ip;	/* used for IP-based allow/deny rules */
	time_t last_mod;
	char *rmt_ip_str;       /* for $REMOTE_ADDR and $REMOTE_PORT */
	const char *bind_addr_or_port;

	const char *g_query;
	const char *configFile;
	const char *home_httpd;

	const char *found_mime_type;
	const char *found_moved_temporarily;
	Htaccess_IP *ip_a_d;    /* config allow/deny lines */

	USE_FEATURE_HTTPD_BASIC_AUTH(const char *g_realm;)
	USE_FEATURE_HTTPD_BASIC_AUTH(char *remoteuser;)
	USE_FEATURE_HTTPD_CGI(char *referer;)
	USE_FEATURE_HTTPD_CGI(char *user_agent;)

	off_t file_size;        /* -1 - unknown */
#if ENABLE_FEATURE_HTTPD_RANGES
	off_t range_start;
	off_t range_end;
	off_t range_len;
#endif

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	Htaccess *g_auth;       /* config user:password lines */
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
	Htaccess *mime_a;       /* config mime types */
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
	Htaccess *script_i;     /* config script interpreters */
#endif
	char *iobuf;	        /* [IOBUF_SIZE] */
#define hdr_buf bb_common_bufsiz1
	char *hdr_ptr;
	int hdr_cnt;
#if ENABLE_FEATURE_HTTPD_ERROR_PAGES
	const char *http_error_page[ARRAY_SIZE(http_response_type)];
#endif
#if ENABLE_FEATURE_HTTPD_PROXY
	Htaccess_Proxy *proxy;
#endif
};
#define G (*ptr_to_globals)
#define verbose           (G.verbose          )
#define flg_deny_all      (G.flg_deny_all     )
#define rmt_ip            (G.rmt_ip           )
#define bind_addr_or_port (G.bind_addr_or_port)
#define g_query           (G.g_query          )
#define configFile        (G.configFile       )
#define home_httpd        (G.home_httpd       )
#define found_mime_type   (G.found_mime_type  )
#define found_moved_temporarily (G.found_moved_temporarily)
#define last_mod          (G.last_mod         )
#define ip_a_d            (G.ip_a_d           )
#define g_realm           (G.g_realm          )
#define remoteuser        (G.remoteuser       )
#define referer           (G.referer          )
#define user_agent        (G.user_agent       )
#define file_size         (G.file_size        )
#if ENABLE_FEATURE_HTTPD_RANGES
#define range_start       (G.range_start      )
#define range_end         (G.range_end        )
#define range_len         (G.range_len        )
#endif
#define rmt_ip_str        (G.rmt_ip_str       )
#define g_auth            (G.g_auth           )
#define mime_a            (G.mime_a           )
#define script_i          (G.script_i         )
#define iobuf             (G.iobuf            )
#define hdr_ptr           (G.hdr_ptr          )
#define hdr_cnt           (G.hdr_cnt          )
#define http_error_page   (G.http_error_page  )
#define proxy             (G.proxy            )
#define INIT_G() do { \
	PTR_TO_GLOBALS = xzalloc(sizeof(G)); \
	USE_FEATURE_HTTPD_BASIC_AUTH(g_realm = "Web Server Authentication";) \
	bind_addr_or_port = "80"; \
	file_size = -1; \
} while (0)

#if !ENABLE_FEATURE_HTTPD_RANGES
enum {
	range_start = 0,
	range_end = MAXINT(off_t) - 1,
	range_len = MAXINT(off_t),
};
#endif


#define STRNCASECMP(a, str) strncasecmp((a), (str), sizeof(str)-1)

/* Prototypes */
static void send_file_and_exit(const char *url, int headers) ATTRIBUTE_NORETURN;

static void free_llist(has_next_ptr **pptr)
{
	has_next_ptr *cur = *pptr;
	while (cur) {
		has_next_ptr *t = cur;
		cur = cur->next;
		free(t);
	}
	*pptr = NULL;
}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
static ALWAYS_INLINE void free_Htaccess_list(Htaccess **pptr)
{
	free_llist((has_next_ptr**)pptr);
}
#endif

static ALWAYS_INLINE void free_Htaccess_IP_list(Htaccess_IP **pptr)
{
	free_llist((has_next_ptr**)pptr);
}

/* Returns presumed mask width in bits or < 0 on error.
 * Updates strp, stores IP at provided pointer */
static int scan_ip(const char **strp, unsigned *ipp, unsigned char endc)
{
	const char *p = *strp;
	int auto_mask = 8;
	unsigned ip = 0;
	int j;

	if (*p == '/')
		return -auto_mask;

	for (j = 0; j < 4; j++) {
		unsigned octet;

		if ((*p < '0' || *p > '9') && *p != '/' && *p)
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
		if (*p != '/' && *p)
			auto_mask += 8;
		ip = (ip << 8) | octet;
	}
	if (*p) {
		if (*p != endc)
			return -auto_mask;
		p++;
		if (*p == '\0')
			return -auto_mask;
	}
	*ipp = ip;
	*strp = p;
	return auto_mask;
}

/* Returns 0 on success. Stores IP and mask at provided pointers */
static int scan_ip_mask(const char *str, unsigned *ipp, unsigned *maskp)
{
	int i;
	unsigned mask;
	char *p;

	i = scan_ip(&str, ipp, '/');
	if (i < 0)
		return i;

	if (*str) {
		/* there is /xxx after dotted-IP address */
		i = bb_strtou(str, &p, 10);
		if (*p == '.') {
			/* 'xxx' itself is dotted-IP mask, parse it */
			/* (return 0 (success) only if it has N.N.N.N form) */
			return scan_ip(&str, maskp, '\0') - 32;
		}
		if (*p)
			return -1;
	}

	if (i > 32)
		return -1;

	if (sizeof(unsigned) == 4 && i == 32) {
		/* mask >>= 32 below may not work */
		mask = 0;
	} else {
		mask = 0xffffffff;
		mask >>= i;
	}
	/* i == 0 -> *maskp = 0x00000000
	 * i == 1 -> *maskp = 0x80000000
	 * i == 4 -> *maskp = 0xf0000000
	 * i == 31 -> *maskp = 0xfffffffe
	 * i == 32 -> *maskp = 0xffffffff */
	*maskp = (uint32_t)(~mask);
	return 0;
}

/*
 * Parse configuration file into in-memory linked list.
 *
 * The first non-white character is examined to determine if the config line
 * is one of the following:
 *    .ext:mime/type   # new mime type not compiled into httpd
 *    [adAD]:from      # ip address allow/deny, * for wildcard
 *    /path:user:pass  # username/password
 *    Ennn:error.html  # error page for status nnn
 *    P:/url:[http://]hostname[:port]/new/path # reverse proxy
 *
 * Any previous IP rules are discarded.
 * If the flag argument is not SUBDIR_PARSE then all /path and mime rules
 * are also discarded.  That is, previous settings are retained if flag is
 * SUBDIR_PARSE.
 * Error pages are only parsed on the main config file.
 *
 * path   Path where to look for httpd.conf (without filename).
 * flag   Type of the parse request.
 */
/* flag */
#define FIRST_PARSE          0
#define SUBDIR_PARSE         1
#define SIGNALED_PARSE       2
#define FIND_FROM_HTTPD_ROOT 3
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
	const char *cf = configFile;
	char buf[160];
	char *p0;
	char *c, *p;
	Htaccess_IP *pip;

	/* discard old rules */
	free_Htaccess_IP_list(&ip_a_d);
	flg_deny_all = 0;
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
	/* retain previous auth and mime config only for subdir parse */
	if (flag != SUBDIR_PARSE) {
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		free_Htaccess_list(&g_auth);
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
		free_Htaccess_list(&mime_a);
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
		free_Htaccess_list(&script_i);
#endif
	}
#endif

	if (flag == SUBDIR_PARSE || cf == NULL) {
		cf = alloca(strlen(path) + sizeof(httpd_conf) + 2);
		sprintf((char *)cf, "%s/%s", path, httpd_conf);
	}

	while ((f = fopen(cf, "r")) == NULL) {
		if (flag == SUBDIR_PARSE || flag == FIND_FROM_HTTPD_ROOT) {
			/* config file not found, no changes to config */
			return;
		}
		if (configFile && flag == FIRST_PARSE) /* if -c option given */
			bb_simple_perror_msg_and_die(cf);
		flag = FIND_FROM_HTTPD_ROOT;
		cf = httpd_conf;
	}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	prev = g_auth;
#endif
	/* This could stand some work */
	while ((p0 = fgets(buf, sizeof(buf), f)) != NULL) {
		c = NULL;
		for (p = p0; *p0 != '\0' && *p0 != '#'; p0++) {
			if (!isspace(*p0)) {
				*p++ = *p0;
				if (*p0 == ':' && c == NULL)
					c = p;
			}
		}
		*p = '\0';

		/* test for empty or strange line */
		if (c == NULL || *c == '\0')
			continue;
		p0 = buf;
		if (*p0 == 'd')
			*p0 = 'D';
		if (*c == '*') {
			if (*p0 == 'D') {
				/* memorize deny all */
				flg_deny_all = 1;
			}
			/* skip default other "word:*" config lines */
			continue;
		}

		if (*p0 == 'a')
			*p0 = 'A';
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
					/* Deny:from_IP move top */
					pip->next = ip_a_d;
					ip_a_d = pip;
				} else {
					/* add to bottom A:form_IP config line */
					Htaccess_IP *prev_IP = ip_a_d;

					if (prev_IP == NULL) {
						ip_a_d = pip;
					} else {
						while (prev_IP->next)
							prev_IP = prev_IP->next;
						prev_IP->next = pip;
					}
				}
			}
			continue;
		}

#if ENABLE_FEATURE_HTTPD_ERROR_PAGES
		if (flag == FIRST_PARSE && *p0 == 'E') {
			int i;
			/* error status code */
			int status = atoi(++p0);
			/* c already points at the character following ':' in parse loop */
			/* c = strchr(p0, ':'); c++; */
			if (status < HTTP_CONTINUE) {
				bb_error_msg("config error '%s' in '%s'", buf, cf);
				continue;
			}

			/* then error page; find matching status */
			for (i = 0; i < ARRAY_SIZE(http_response_type); i++) {
				if (http_response_type[i] == status) {
					http_error_page[i] = concat_path_file((*c == '/') ? NULL : home_httpd, c);
					break;
				}
			}
			continue;
		}
#endif

#if ENABLE_FEATURE_HTTPD_PROXY
		if (flag == FIRST_PARSE && *p0 == 'P') {
			/* P:/url:[http://]hostname[:port]/new/path */
			char *url_from, *host_port, *url_to;
			Htaccess_Proxy *proxy_entry;

			url_from = c;
			host_port = strchr(c, ':');
			if (host_port == NULL) {
				bb_error_msg("config error '%s' in '%s'", buf, cf);
				continue;
			}
			*host_port++ = '\0';
			if (strncmp(host_port, "http://", 7) == 0)
				host_port += 7;
			if (*host_port == '\0') {
				bb_error_msg("config error '%s' in '%s'", buf, cf);
				continue;
			}
			url_to = strchr(host_port, '/');
			if (url_to == NULL) {
				bb_error_msg("config error '%s' in '%s'", buf, cf);
				continue;
			}
			*url_to = '\0';
			proxy_entry = xzalloc(sizeof(Htaccess_Proxy));
			proxy_entry->url_from = xstrdup(url_from);
			proxy_entry->host_port = xstrdup(host_port);
			*url_to = '/';
			proxy_entry->url_to = xstrdup(url_to);
			proxy_entry->next = proxy;
			proxy = proxy_entry;
			continue;
		}
#endif

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		if (*p0 == '/') {
			/* make full path from httpd root / current_path / config_line_path */
			cf = (flag == SUBDIR_PARSE ? path : "");
			p0 = xmalloc(strlen(cf) + (c - buf) + 2 + strlen(c));
			c[-1] = '\0';
			sprintf(p0, "/%s%s", cf, buf);

			/* another call bb_simplify_path */
			cf = p = p0;

			do {
				if (*p == '/') {
					if (*cf == '/') {    /* skip duplicate (or initial) slash */
						continue;
					}
					if (*cf == '.') {
						if (cf[1] == '/' || cf[1] == '\0') { /* remove extra '.' */
							continue;
						}
						if ((cf[1] == '.') && (cf[2] == '/' || cf[2] == '\0')) {
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
			*p = ':';
			strcpy(p + 1, c);
		}
#endif

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES \
 || ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
		/* storing current config line */
		cur = xzalloc(sizeof(Htaccess) + strlen(p0));
		cf = strcpy(cur->before_colon, p0);
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		if (*p0 == '/')
			free(p0);
#endif
		c = strchr(cf, ':');
		*c++ = '\0';
		cur->after_colon = c;
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
		if (*cf == '.') {
			/* config .mime line move top for overwrite previous */
			cur->next = mime_a;
			mime_a = cur;
			continue;
		}
#endif
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
		if (*cf == '*' && cf[1] == '.') {
			/* config script interpreter line move top for overwrite previous */
			cur->next = script_i;
			script_i = cur;
			continue;
		}
#endif
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
		if (prev == NULL) {
			/* first line */
			g_auth = prev = cur;
		} else {
			/* sort path, if current length eq or bigger then move up */
			Htaccess *prev_hti = g_auth;
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
						g_auth = cur;
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
#endif /* BASIC_AUTH */
#endif /* BASIC_AUTH || MIME_TYPES || SCRIPT_INTERPR */
	 } /* while (fgets) */
	 fclose(f);
}

#if ENABLE_FEATURE_HTTPD_ENCODE_URL_STR
/*
 * Given a string, html-encode special characters.
 * This is used for the -e command line option to provide an easy way
 * for scripts to encode result data without confusing browsers.  The
 * returned string pointer is memory allocated by malloc().
 *
 * Returns a pointer to the encoded string (malloced).
 */
static char *encodeString(const char *string)
{
	/* take the simple route and encode everything */
	/* could possibly scan once to get length.     */
	int len = strlen(string);
	char *out = xmalloc(len * 6 + 1);
	char *p = out;
	char ch;

	while ((ch = *string++)) {
		/* very simple check for what to encode */
		if (isalnum(ch))
			*p++ = ch;
		else
			p += sprintf(p, "&#%d;", (unsigned char) ch);
	}
	*p = '\0';
	return out;
}
#endif          /* FEATURE_HTTPD_ENCODE_URL_STR */

/*
 * Given a URL encoded string, convert it to plain ascii.
 * Since decoding always makes strings smaller, the decode is done in-place.
 * Thus, callers should strdup() the argument if they do not want the
 * argument modified.  The return is the original pointer, allowing this
 * function to be easily used as arguments to other functions.
 *
 * string    The first string to decode.
 * option_d  1 if called for httpd -d
 *
 * Returns a pointer to the decoded string (same as input).
 */
static unsigned hex_to_bin(unsigned char c)
{
	unsigned v;

	v = c - '0';
	if (v <= 9)
		return v;
	/* c | 0x20: letters to lower case, non-letters
	 * to (potentially different) non-letters */
	v = (unsigned)(c | 0x20) - 'a';
	if (v <= 5)
		return v + 10;
	return ~0;
}
/* For testing:
void t(char c) { printf("'%c'(%u) %u\n", c, c, hex_to_bin(c)); }
int main() { t(0x10); t(0x20); t('0'); t('9'); t('A'); t('F'); t('a'); t('f');
t('0'-1); t('9'+1); t('A'-1); t('F'+1); t('a'-1); t('f'+1); return 0; }
*/
static char *decodeString(char *orig, int option_d)
{
	/* note that decoded string is always shorter than original */
	char *string = orig;
	char *ptr = string;
	char c;

	while ((c = *ptr++) != '\0') {
		unsigned v;

		if (option_d && c == '+') {
			*string++ = ' ';
			continue;
		}
		if (c != '%') {
			*string++ = c;
			continue;
		}
		v = hex_to_bin(ptr[0]);
		if (v > 15) {
 bad_hex:
			if (!option_d)
				return NULL;
			*string++ = '%';
			continue;
		}
		v = (v * 16) | hex_to_bin(ptr[1]);
		if (v > 255)
			goto bad_hex;
		if (!option_d && (v == '/' || v == '\0')) {
			/* caller takes it as indication of invalid
			 * (dangerous wrt exploits) chars */
			return orig + 1;
		}
		*string++ = v;
		ptr += 2;
	}
	*string = '\0';
	return orig;
}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
/*
 * Decode a base64 data stream as per rfc1521.
 * Note that the rfc states that non base64 chars are to be ignored.
 * Since the decode always results in a shorter size than the input,
 * it is OK to pass the input arg as an output arg.
 * Parameter: a pointer to a base64 encoded string.
 * Decoded data is stored in-place.
 */
static void decodeBase64(char *Data)
{
	const unsigned char *in = (const unsigned char *)Data;
	/* The decoded size will be at most 3/4 the size of the encoded */
	unsigned ch = 0;
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
	*Data = '\0';
}
#endif

/*
 * Create a listen server socket on the designated port.
 */
static int openServer(void)
{
	unsigned n = bb_strtou(bind_addr_or_port, NULL, 10);
	if (!errno && n && n <= 0xffff)
		n = create_and_bind_stream_or_die(NULL, n);
	else
		n = create_and_bind_stream_or_die(bind_addr_or_port, 80);
	xlisten(n, 9);
	return n;
}

/*
 * Log the connection closure and exit.
 */
static void log_and_exit(void) ATTRIBUTE_NORETURN;
static void log_and_exit(void)
{
	/* Paranoia. IE said to be buggy. It may send some extra data
	 * or be confused by us just exiting without SHUT_WR. Oh well. */
	shutdown(1, SHUT_WR);
	ndelay_on(0);
	while (read(0, iobuf, IOBUF_SIZE) > 0)
		continue;

	if (verbose > 2)
		bb_error_msg("closed");
	_exit(xfunc_error_retval);
}

/*
 * Create and send HTTP response headers.
 * The arguments are combined and sent as one write operation.  Note that
 * IE will puke big-time if the headers are not sent in one packet and the
 * second packet is delayed for any reason.
 * responseNum - the result code to send.
 */
static void send_headers(int responseNum)
{
	static const char RFC1123FMT[] ALIGN1 = "%a, %d %b %Y %H:%M:%S GMT";

	const char *responseString = "";
	const char *infoString = NULL;
	const char *mime_type;
#if ENABLE_FEATURE_HTTPD_ERROR_PAGES
	const char *error_page = 0;
#endif
	unsigned i;
	time_t timer = time(0);
	char tmp_str[80];
	int len;

	for (i = 0; i < ARRAY_SIZE(http_response_type); i++) {
		if (http_response_type[i] == responseNum) {
			responseString = http_response[i].name;
			infoString = http_response[i].info;
#if ENABLE_FEATURE_HTTPD_ERROR_PAGES
			error_page = http_error_page[i];
#endif
			break;
		}
	}
	/* error message is HTML */
	mime_type = responseNum == HTTP_OK ?
				found_mime_type : "text/html";

	if (verbose)
		bb_error_msg("response:%u", responseNum);

	/* emit the current date */
	strftime(tmp_str, sizeof(tmp_str), RFC1123FMT, gmtime(&timer));
	len = sprintf(iobuf,
			"HTTP/1.0 %d %s\r\nContent-type: %s\r\n"
			"Date: %s\r\nConnection: close\r\n",
			responseNum, responseString, mime_type, tmp_str);

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	if (responseNum == HTTP_UNAUTHORIZED) {
		len += sprintf(iobuf + len,
				"WWW-Authenticate: Basic realm=\"%s\"\r\n",
				g_realm);
	}
#endif
	if (responseNum == HTTP_MOVED_TEMPORARILY) {
		len += sprintf(iobuf + len, "Location: %s/%s%s\r\n",
				found_moved_temporarily,
				(g_query ? "?" : ""),
				(g_query ? g_query : ""));
	}

#if ENABLE_FEATURE_HTTPD_ERROR_PAGES
	if (error_page && !access(error_page, R_OK)) {
		strcat(iobuf, "\r\n");
		len += 2;

		if (DEBUG)
			fprintf(stderr, "headers: '%s'\n", iobuf);
		full_write(1, iobuf, len);
		if (DEBUG)
			fprintf(stderr, "writing error page: '%s'\n", error_page);
		return send_file_and_exit(error_page, FALSE);
	}
#endif

	if (file_size != -1) {    /* file */
		strftime(tmp_str, sizeof(tmp_str), RFC1123FMT, gmtime(&last_mod));
#if ENABLE_FEATURE_HTTPD_RANGES
		if (responseNum == HTTP_PARTIAL_CONTENT) {
			len += sprintf(iobuf + len, "Content-Range: bytes %"OFF_FMT"d-%"OFF_FMT"d/%"OFF_FMT"d\r\n",
					range_start,
					range_end,
					file_size);
			file_size = range_end - range_start + 1;
		}
#endif
		len += sprintf(iobuf + len,
#if ENABLE_FEATURE_HTTPD_RANGES
			"Accept-Ranges: bytes\r\n"
#endif
			"Last-Modified: %s\r\n%s %"OFF_FMT"d\r\n",
				tmp_str,
				"Content-length:",
				file_size
		);
	}
	iobuf[len++] = '\r';
	iobuf[len++] = '\n';
	if (infoString) {
		len += sprintf(iobuf + len,
				"<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\n"
				"<BODY><H1>%d %s</H1>\n%s\n</BODY></HTML>\n",
				responseNum, responseString,
				responseNum, responseString, infoString);
	}
	if (DEBUG)
		fprintf(stderr, "headers: '%s'\n", iobuf);
	if (full_write(1, iobuf, len) != len) {
		if (verbose > 1)
			bb_perror_msg("error");
		log_and_exit();
	}
}

static void send_headers_and_exit(int responseNum) ATTRIBUTE_NORETURN;
static void send_headers_and_exit(int responseNum)
{
	send_headers(responseNum);
	log_and_exit();
}

/*
 * Read from the socket until '\n' or EOF. '\r' chars are removed.
 * '\n' is replaced with NUL.
 * Return number of characters read or 0 if nothing is read
 * ('\r' and '\n' are not counted).
 * Data is returned in iobuf.
 */
static int get_line(void)
{
	int count = 0;
	char c;

	while (1) {
		if (hdr_cnt <= 0) {
			hdr_cnt = safe_read(0, hdr_buf, sizeof(hdr_buf));
			if (hdr_cnt <= 0)
				break;
			hdr_ptr = hdr_buf;
		}
		iobuf[count] = c = *hdr_ptr++;
		hdr_cnt--;

		if (c == '\r')
			continue;
		if (c == '\n') {
			iobuf[count] = '\0';
			return count;
		}
		if (count < (IOBUF_SIZE - 1))      /* check overflow */
			count++;
	}
	return count;
}

#if ENABLE_FEATURE_HTTPD_CGI || ENABLE_FEATURE_HTTPD_PROXY

/* gcc 4.2.1 fares better with NOINLINE */
static NOINLINE void cgi_io_loop_and_exit(int fromCgi_rd, int toCgi_wr, int post_len) ATTRIBUTE_NORETURN;
static NOINLINE void cgi_io_loop_and_exit(int fromCgi_rd, int toCgi_wr, int post_len)
{
	enum { FROM_CGI = 1, TO_CGI = 2 }; /* indexes in pfd[] */
	struct pollfd pfd[3];
	int out_cnt; /* we buffer a bit of initial CGI output */
	int count;

	/* iobuf is used for CGI -> network data,
	 * hdr_buf is for network -> CGI data (POSTDATA) */

	/* If CGI dies, we still want to correctly finish reading its output
	 * and send it to the peer. So please no SIGPIPEs! */
	signal(SIGPIPE, SIG_IGN);

	// We inconsistently handle a case when more POSTDATA from network
	// is coming than we expected. We may give *some part* of that
	// extra data to CGI.

	//if (hdr_cnt > post_len) {
	//	/* We got more POSTDATA from network than we expected */
	//	hdr_cnt = post_len;
	//}
	post_len -= hdr_cnt;
	/* post_len - number of POST bytes not yet read from network */

	/* NB: breaking out of this loop jumps to log_and_exit() */
	out_cnt = 0;
	while (1) {
		memset(pfd, 0, sizeof(pfd));

		pfd[FROM_CGI].fd = fromCgi_rd;
		pfd[FROM_CGI].events = POLLIN;

		if (toCgi_wr) {
			pfd[TO_CGI].fd = toCgi_wr;
			if (hdr_cnt > 0) {
				pfd[TO_CGI].events = POLLOUT;
			} else if (post_len > 0) {
				pfd[0].events = POLLIN;
			} else {
				/* post_len <= 0 && hdr_cnt <= 0:
				 * no more POST data to CGI,
				 * let CGI see EOF on CGI's stdin */
				close(toCgi_wr);
				toCgi_wr = 0;
			}
		}

		/* Now wait on the set of sockets */
		count = safe_poll(pfd, 3, -1);
		if (count <= 0) {
#if 0
			if (waitpid(pid, &status, WNOHANG) <= 0) {
				/* Weird. CGI didn't exit and no fd's
				 * are ready, yet poll returned?! */
				continue;
			}
			if (DEBUG && WIFEXITED(status))
				bb_error_msg("CGI exited, status=%d", WEXITSTATUS(status));
			if (DEBUG && WIFSIGNALED(status))
				bb_error_msg("CGI killed, signal=%d", WTERMSIG(status));
#endif
			break;
		}

		if (pfd[TO_CGI].revents) {
			/* hdr_cnt > 0 here due to the way pfd[TO_CGI].events set */
			/* Have data from peer and can write to CGI */
			count = safe_write(toCgi_wr, hdr_ptr, hdr_cnt);
			/* Doesn't happen, we dont use nonblocking IO here
			 *if (count < 0 && errno == EAGAIN) {
			 *	...
			 *} else */
			if (count > 0) {
				hdr_ptr += count;
				hdr_cnt -= count;
			} else {
				/* EOF/broken pipe to CGI, stop piping POST data */
				hdr_cnt = post_len = 0;
			}
		}

		if (pfd[0].revents) {
			/* post_len > 0 && hdr_cnt == 0 here */
			/* We expect data, prev data portion is eaten by CGI
			 * and there *is* data to read from the peer
			 * (POSTDATA) */
			//count = post_len > (int)sizeof(hdr_buf) ? (int)sizeof(hdr_buf) : post_len;
			//count = safe_read(0, hdr_buf, count);
			count = safe_read(0, hdr_buf, sizeof(hdr_buf));
			if (count > 0) {
				hdr_cnt = count;
				hdr_ptr = hdr_buf;
				post_len -= count;
			} else {
				/* no more POST data can be read */
				post_len = 0;
			}
		}

		if (pfd[FROM_CGI].revents) {
			/* There is something to read from CGI */
			char *rbuf = iobuf;

			/* Are we still buffering CGI output? */
			if (out_cnt >= 0) {
				/* HTTP_200[] has single "\r\n" at the end.
				 * According to http://hoohoo.ncsa.uiuc.edu/cgi/out.html,
				 * CGI scripts MUST send their own header terminated by
				 * empty line, then data. That's why we have only one
				 * <cr><lf> pair here. We will output "200 OK" line
				 * if needed, but CGI still has to provide blank line
				 * between header and body */

				/* Must use safe_read, not full_read, because
				 * CGI may output a few first bytes and then wait
				 * for POSTDATA without closing stdout.
				 * With full_read we may wait here forever. */
				count = safe_read(fromCgi_rd, rbuf + out_cnt, PIPE_BUF - 8);
				if (count <= 0) {
					/* eof (or error) and there was no "HTTP",
					 * so write it, then write received data */
					if (out_cnt) {
						full_write(1, HTTP_200, sizeof(HTTP_200)-1);
						full_write(1, rbuf, out_cnt);
					}
					break; /* CGI stdout is closed, exiting */
				}
				out_cnt += count;
				count = 0;
				/* "Status" header format is: "Status: 302 Redirected\r\n" */
				if (out_cnt >= 8 && memcmp(rbuf, "Status: ", 8) == 0) {
					/* send "HTTP/1.0 " */
					if (full_write(1, HTTP_200, 9) != 9)
						break;
					rbuf += 8; /* skip "Status: " */
					count = out_cnt - 8;
					out_cnt = -1; /* buffering off */
				} else if (out_cnt >= 4) {
					/* Did CGI add "HTTP"? */
					if (memcmp(rbuf, HTTP_200, 4) != 0) {
						/* there is no "HTTP", do it ourself */
						if (full_write(1, HTTP_200, sizeof(HTTP_200)-1) != sizeof(HTTP_200)-1)
							break;
					}
					/* Commented out:
					if (!strstr(rbuf, "ontent-")) {
						full_write(s, "Content-type: text/plain\r\n\r\n", 28);
					}
					 * Counter-example of valid CGI without Content-type:
					 * echo -en "HTTP/1.0 302 Found\r\n"
					 * echo -en "Location: http://www.busybox.net\r\n"
					 * echo -en "\r\n"
					 */
					count = out_cnt;
					out_cnt = -1; /* buffering off */
				}
			} else {
				count = safe_read(fromCgi_rd, rbuf, PIPE_BUF);
				if (count <= 0)
					break;  /* eof (or error) */
			}
			if (full_write(1, rbuf, count) != count)
				break;
			if (DEBUG)
				fprintf(stderr, "cgi read %d bytes: '%.*s'\n", count, count, rbuf);
		} /* if (pfd[FROM_CGI].revents) */
	} /* while (1) */
	log_and_exit();
}
#endif

#if ENABLE_FEATURE_HTTPD_CGI

static void setenv1(const char *name, const char *value)
{
	setenv(name, value ? value : "", 1);
}

/*
 * Spawn CGI script, forward CGI's stdin/out <=> network
 *
 * Environment variables are set up and the script is invoked with pipes
 * for stdin/stdout.  If a POST is being done the script is fed the POST
 * data in addition to setting the QUERY_STRING variable (for GETs or POSTs).
 *
 * Parameters:
 * const char *url              The requested URL (with leading /).
 * int post_len                 Length of the POST body.
 * const char *cookie           For set HTTP_COOKIE.
 * const char *content_type     For set CONTENT_TYPE.
 */
static void send_cgi_and_exit(
		const char *url,
		const char *request,
		int post_len,
		const char *cookie,
		const char *content_type) ATTRIBUTE_NORETURN;
static void send_cgi_and_exit(
		const char *url,
		const char *request,
		int post_len,
		const char *cookie,
		const char *content_type)
{
	struct { int rd; int wr; } fromCgi;  /* CGI -> httpd pipe */
	struct { int rd; int wr; } toCgi;    /* httpd -> CGI pipe */
	char *fullpath;
	char *script;
	char *purl;
	int pid;

	/*
	 * We are mucking with environment _first_ and then vfork/exec,
	 * this allows us to use vfork safely. Parent don't care about
	 * these environment changes anyway.
	 */

	/*
	 * Find PATH_INFO.
	 */
	purl = xstrdup(url);
	script = purl;
	while ((script = strchr(script + 1, '/')) != NULL) {
		/* have script.cgi/PATH_INFO or dirs/script.cgi[/PATH_INFO] */
		struct stat sb;

		*script = '\0';
		if (!is_directory(purl + 1, 1, &sb)) {
			/* not directory, found script.cgi/PATH_INFO */
			*script = '/';
			break;
		}
		*script = '/';          /* is directory, find next '/' */
	}
	setenv1("PATH_INFO", script);   /* set /PATH_INFO or "" */
	setenv1("REQUEST_METHOD", request);
	if (g_query) {
		putenv(xasprintf("%s=%s?%s", "REQUEST_URI", purl, g_query));
	} else {
		setenv1("REQUEST_URI", purl);
	}
	if (script != NULL)
		*script = '\0';         /* cut off /PATH_INFO */

	/* SCRIPT_FILENAME required by PHP in CGI mode */
	fullpath = concat_path_file(home_httpd, purl);
	setenv1("SCRIPT_FILENAME", fullpath);
	/* set SCRIPT_NAME as full path: /cgi-bin/dirs/script.cgi */
	setenv1("SCRIPT_NAME", purl);
	/* http://hoohoo.ncsa.uiuc.edu/cgi/env.html:
	 * QUERY_STRING: The information which follows the ? in the URL
	 * which referenced this script. This is the query information.
	 * It should not be decoded in any fashion. This variable
	 * should always be set when there is query information,
	 * regardless of command line decoding. */
	/* (Older versions of bbox seem to do some decoding) */
	setenv1("QUERY_STRING", g_query);
	putenv((char*)"SERVER_SOFTWARE=busybox httpd/"BB_VER);
	putenv((char*)"SERVER_PROTOCOL=HTTP/1.0");
	putenv((char*)"GATEWAY_INTERFACE=CGI/1.1");
	/* Having _separate_ variables for IP and port defeats
	 * the purpose of having socket abstraction. Which "port"
	 * are you using on Unix domain socket?
	 * IOW - REMOTE_PEER="1.2.3.4:56" makes much more sense.
	 * Oh well... */
	{
		char *p = rmt_ip_str ? rmt_ip_str : (char*)"";
		char *cp = strrchr(p, ':');
		if (ENABLE_FEATURE_IPV6 && cp && strchr(cp, ']'))
			cp = NULL;
		if (cp) *cp = '\0'; /* delete :PORT */
		setenv1("REMOTE_ADDR", p);
		if (cp) {
			*cp = ':';
#if ENABLE_FEATURE_HTTPD_SET_REMOTE_PORT_TO_ENV
			setenv1("REMOTE_PORT", cp + 1);
#endif
		}
	}
	setenv1("HTTP_USER_AGENT", user_agent);
	if (post_len)
		putenv(xasprintf("CONTENT_LENGTH=%d", post_len));
	if (cookie)
		setenv1("HTTP_COOKIE", cookie);
	if (content_type)
		setenv1("CONTENT_TYPE", content_type);
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	if (remoteuser) {
		setenv1("REMOTE_USER", remoteuser);
		putenv((char*)"AUTH_TYPE=Basic");
	}
#endif
	if (referer)
		setenv1("HTTP_REFERER", referer);

	xpipe(&fromCgi.rd);
	xpipe(&toCgi.rd);

	pid = vfork();
	if (pid < 0) {
		/* TODO: log perror? */
		log_and_exit();
	}

	if (!pid) {
		/* Child process */
		xfunc_error_retval = 242;

		xmove_fd(toCgi.rd, 0);  /* replace stdin with the pipe */
		xmove_fd(fromCgi.wr, 1);  /* replace stdout with the pipe */
		close(fromCgi.rd);
		close(toCgi.wr);
		/* User seeing stderr output can be a security problem.
		 * If CGI really wants that, it can always do dup itself. */
		/* dup2(1, 2); */

		/* script must have absolute path */
		script = strrchr(fullpath, '/');
		if (!script)
			goto error_execing_cgi;
		*script = '\0';
		/* chdiring to script's dir */
		if (chdir(fullpath) == 0) {
			char *argv[2];
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
			char *interpr = NULL;
			char *suffix = strrchr(purl, '.');

			if (suffix) {
				Htaccess *cur;
				for (cur = script_i; cur; cur = cur->next) {
					if (strcmp(cur->before_colon + 1, suffix) == 0) {
						interpr = cur->after_colon;
						break;
					}
				}
			}
#endif
			*script = '/';
			/* set argv[0] to name without path */
			argv[0] = (char*)bb_basename(purl);
			argv[1] = NULL;
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
			if (interpr)
				execv(interpr, argv);
			else
#endif
				execv(fullpath, argv);
		}
 error_execing_cgi:
		/* send to stdout
		 * (we are CGI here, our stdout is pumped to the net) */
		send_headers_and_exit(HTTP_NOT_FOUND);
	} /* end child */

	/* Parent process */

	/* Restore variables possibly changed by child */
	xfunc_error_retval = 0;

	/* Pump data */
	close(fromCgi.wr);
	close(toCgi.rd);
	cgi_io_loop_and_exit(fromCgi.rd, toCgi.wr, post_len);
}

#endif          /* FEATURE_HTTPD_CGI */

/*
 * Send a file response to a HTTP request, and exit
 *
 * Parameters:
 * const char *url    The requested URL (with leading /).
 * headers            Don't send headers before if FALSE.
 */
static void send_file_and_exit(const char *url, int headers)
{
	static const char *const suffixTable[] = {
	/* Warning: shorter equivalent suffix in one line must be first */
		".htm.html", "text/html",
		".jpg.jpeg", "image/jpeg",
		".gif",      "image/gif",
		".png",      "image/png",
		".txt.h.c.cc.cpp", "text/plain",
		".css",      "text/css",
		".wav",      "audio/wav",
		".avi",      "video/x-msvideo",
		".qt.mov",   "video/quicktime",
		".mpe.mpeg", "video/mpeg",
		".mid.midi", "audio/midi",
		".mp3",      "audio/mpeg",
#if 0                        /* unpopular */
		".au",       "audio/basic",
		".pac",      "application/x-ns-proxy-autoconfig",
		".vrml.wrl", "model/vrml",
#endif
		NULL
	};

	char *suffix;
	int f;
	const char *const *table;
	const char *try_suffix;
	ssize_t count;
#if ENABLE_FEATURE_HTTPD_USE_SENDFILE
	off_t offset;
#endif

	suffix = strrchr(url, '.');

	/* If not found, set default as "application/octet-stream";  */
	found_mime_type = "application/octet-stream";
	if (suffix) {
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
		Htaccess *cur;
#endif
		for (table = suffixTable; *table; table += 2) {
			try_suffix = strstr(table[0], suffix);
			if (try_suffix) {
				try_suffix += strlen(suffix);
				if (*try_suffix == '\0' || *try_suffix == '.') {
					found_mime_type = table[1];
					break;
				}
			}
		}
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
		for (cur = mime_a; cur; cur = cur->next) {
			if (strcmp(cur->before_colon, suffix) == 0) {
				found_mime_type = cur->after_colon;
				break;
			}
		}
#endif
	}

	if (DEBUG)
		bb_error_msg("sending file '%s' content-type: %s",
			url, found_mime_type);

	f = open(url, O_RDONLY);
	if (f < 0) {
		if (DEBUG)
			bb_perror_msg("cannot open '%s'", url);
		if (headers)
			send_headers_and_exit(HTTP_NOT_FOUND);
	}
#if ENABLE_FEATURE_HTTPD_RANGES
	if (!headers)
		range_start = 0; /* err pages and ranges don't mix */
	range_len = MAXINT(off_t);
	if (range_start) {
		if (!range_end) {
			range_end = file_size - 1;
		}
		if (range_end < range_start
		 || lseek(f, range_start, SEEK_SET) != range_start
		) {
			lseek(f, 0, SEEK_SET);
			range_start = 0;
		} else {
			range_len = range_end - range_start + 1;
			send_headers(HTTP_PARTIAL_CONTENT);
			headers = 0;
		}
	}
#endif

	if (headers)
		send_headers(HTTP_OK);

	/* If you want to know about EPIPE below
	 * (happens if you abort downloads from local httpd): */
	signal(SIGPIPE, SIG_IGN);

#if ENABLE_FEATURE_HTTPD_USE_SENDFILE
	offset = range_start;
	do {
		/* sz is rounded down to 64k */
		ssize_t sz = MAXINT(ssize_t) - 0xffff;
		USE_FEATURE_HTTPD_RANGES(if (sz > range_len) sz = range_len;)
		count = sendfile(1, f, &offset, sz);
		if (count < 0) {
			if (offset == range_start)
				goto fallback;
			goto fin;
		}
		USE_FEATURE_HTTPD_RANGES(range_len -= sz;)
	} while (count > 0 && range_len);
	log_and_exit();

 fallback:
#endif
	while ((count = safe_read(f, iobuf, IOBUF_SIZE)) > 0) {
		ssize_t n;
		USE_FEATURE_HTTPD_RANGES(if (count > range_len) count = range_len;)
		n = full_write(1, iobuf, count);
		if (count != n)
			break;
		USE_FEATURE_HTTPD_RANGES(range_len -= count;)
		if (!range_len)
			break;
	}
#if ENABLE_FEATURE_HTTPD_USE_SENDFILE
 fin:
#endif
	if (count < 0 && verbose > 1)
		bb_perror_msg("error");
	log_and_exit();
}

static int checkPermIP(void)
{
	Htaccess_IP *cur;

	/* This could stand some work */
	for (cur = ip_a_d; cur; cur = cur->next) {
#if DEBUG
		fprintf(stderr,
			"checkPermIP: '%s' ? '%u.%u.%u.%u/%u.%u.%u.%u'\n",
			rmt_ip_str,
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
		if ((rmt_ip & cur->mask) == cur->ip)
			return cur->allow_deny == 'A';   /* Allow/Deny */
	}

	/* if unconfigured, return 1 - access from all */
	return !flg_deny_all;
}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
/*
 * Check the permission file for access password protected.
 *
 * If config file isn't present, everything is allowed.
 * Entries are of the form you can see example from header source
 *
 * path      The file path.
 * request   User information to validate.
 *
 * Returns 1 if request is OK.
 */
static int checkPerm(const char *path, const char *request)
{
	Htaccess *cur;
	const char *p;
	const char *p0;

	const char *prev = NULL;

	/* This could stand some work */
	for (cur = g_auth; cur; cur = cur->next) {
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

				if (strncmp(p, request, u - request) != 0) {
					/* user doesn't match */
					continue;
				}
				pp = strchr(p, ':');
				if (pp && pp[1] == '$' && pp[2] == '1'
				 && pp[3] == '$' && pp[4]
				) {
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
				remoteuser = strdup(request);
				if (remoteuser)
					remoteuser[u - request] = '\0';
				return 1;   /* Ok */
			}
			/* unauthorized */
		}
	} /* for */

	return prev == NULL;
}
#endif  /* FEATURE_HTTPD_BASIC_AUTH */

#if ENABLE_FEATURE_HTTPD_PROXY
static Htaccess_Proxy *find_proxy_entry(const char *url)
{
	Htaccess_Proxy *p;
	for (p = proxy; p; p = p->next) {
		if (strncmp(url, p->url_from, strlen(p->url_from)) == 0)
			return p;
	}
	return NULL;
}
#endif

/*
 * Handle timeouts
 */
static void exit_on_signal(int sig) ATTRIBUTE_NORETURN;
static void exit_on_signal(int sig)
{
	send_headers_and_exit(HTTP_REQUEST_TIMEOUT);
}

/*
 * Handle an incoming http request and exit.
 */
static void handle_incoming_and_exit(const len_and_sockaddr *fromAddr) ATTRIBUTE_NORETURN;
static void handle_incoming_and_exit(const len_and_sockaddr *fromAddr)
{
	static const char request_GET[] ALIGN1 = "GET";

	struct stat sb;
	char *urlcopy;
	char *urlp;
	char *tptr;
	int ip_allowed;
#if ENABLE_FEATURE_HTTPD_CGI
	const char *prequest;
	char *cookie = NULL;
	char *content_type = NULL;
	unsigned long length = 0;
#elif ENABLE_FEATURE_HTTPD_PROXY
#define prequest request_GET
	unsigned long length = 0;
#endif
	char http_major_version;
#if ENABLE_FEATURE_HTTPD_PROXY
	char http_minor_version;
	char *header_buf = header_buf; /* for gcc */
	char *header_ptr = header_ptr;
	Htaccess_Proxy *proxy_entry;
#endif
	struct sigaction sa;
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	int credentials = -1;  /* if not required this is Ok */
#endif

	/* Allocation of iobuf is postponed until now
	 * (IOW, server process doesn't need to waste 8k) */
	iobuf = xmalloc(IOBUF_SIZE);

	rmt_ip = 0;
	if (fromAddr->sa.sa_family == AF_INET) {
		rmt_ip = ntohl(fromAddr->sin.sin_addr.s_addr);
	}
#if ENABLE_FEATURE_IPV6
	if (fromAddr->sa.sa_family == AF_INET6
	 && fromAddr->sin6.sin6_addr.s6_addr32[0] == 0
	 && fromAddr->sin6.sin6_addr.s6_addr32[1] == 0
	 && ntohl(fromAddr->sin6.sin6_addr.s6_addr32[2]) == 0xffff)
		rmt_ip = ntohl(fromAddr->sin6.sin6_addr.s6_addr32[3]);
#endif
	if (ENABLE_FEATURE_HTTPD_CGI || DEBUG || verbose) {
		rmt_ip_str = xmalloc_sockaddr2dotted(&fromAddr->sa);
	}
	if (verbose) {
		/* this trick makes -v logging much simpler */
		applet_name = rmt_ip_str;
		if (verbose > 2)
			bb_error_msg("connected");
	}

	/* Install timeout handler */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = exit_on_signal;
	/* sigemptyset(&sa.sa_mask); - memset should be enough */
	/*sa.sa_flags = 0; - no SA_RESTART */
	sigaction(SIGALRM, &sa, NULL);
	alarm(HEADER_READ_TIMEOUT);

	if (!get_line()) /* EOF or error or empty line */
		send_headers_and_exit(HTTP_BAD_REQUEST);

	/* Determine type of request (GET/POST) */
	urlp = strpbrk(iobuf, " \t");
	if (urlp == NULL)
		send_headers_and_exit(HTTP_BAD_REQUEST);
	*urlp++ = '\0';
#if ENABLE_FEATURE_HTTPD_CGI
	prequest = request_GET;
	if (strcasecmp(iobuf, prequest) != 0) {
		prequest = "POST";
		if (strcasecmp(iobuf, prequest) != 0)
			send_headers_and_exit(HTTP_NOT_IMPLEMENTED);
	}
#else
	if (strcasecmp(iobuf, request_GET) != 0)
		send_headers_and_exit(HTTP_NOT_IMPLEMENTED);
#endif
	urlp = skip_whitespace(urlp);
	if (urlp[0] != '/')
		send_headers_and_exit(HTTP_BAD_REQUEST);

	/* Find end of URL and parse HTTP version, if any */
	http_major_version = '0';
	USE_FEATURE_HTTPD_PROXY(http_minor_version = '0';)
	tptr = strchrnul(urlp, ' ');
	/* Is it " HTTP/"? */
	if (tptr[0] && strncmp(tptr + 1, HTTP_200, 5) == 0) {
		http_major_version = tptr[6];
		USE_FEATURE_HTTPD_PROXY(http_minor_version = tptr[8];)
	}
	*tptr = '\0';

	/* Copy URL from after "GET "/"POST " to stack-allocated char[] */
	urlcopy = alloca((tptr - urlp) + sizeof("/index.html"));
	/*if (urlcopy == NULL)
	 *	send_headers_and_exit(HTTP_INTERNAL_SERVER_ERROR);*/
	strcpy(urlcopy, urlp);
	/* NB: urlcopy ptr is never changed after this */

	/* Extract url args if present */
	g_query = NULL;
	tptr = strchr(urlcopy, '?');
	if (tptr) {
		*tptr++ = '\0';
		g_query = tptr;
	}

	/* Decode URL escape sequences */
	tptr = decodeString(urlcopy, 0);
	if (tptr == NULL)
		send_headers_and_exit(HTTP_BAD_REQUEST);
	if (tptr == urlcopy + 1) {
		/* '/' or NUL is encoded */
		send_headers_and_exit(HTTP_NOT_FOUND);
	}

	/* Canonicalize path */
	/* Algorithm stolen from libbb bb_simplify_path(),
	 * but don't strdup and reducing trailing slash and protect out root */
	urlp = tptr = urlcopy;
	do {
		if (*urlp == '/') {
			/* skip duplicate (or initial) slash */
			if (*tptr == '/') {
				continue;
			}
			if (*tptr == '.') {
				/* skip extra '.' */
				if (tptr[1] == '/' || !tptr[1]) {
					continue;
				}
				/* '..': be careful */
				if (tptr[1] == '.' && (tptr[2] == '/' || !tptr[2])) {
					++tptr;
					if (urlp == urlcopy) /* protect root */
						send_headers_and_exit(HTTP_BAD_REQUEST);
					while (*--urlp != '/') /* omit previous dir */;
						continue;
				}
			}
		}
		*++urlp = *tptr;
	} while (*++tptr);
	*++urlp = '\0';       /* so keep last character */
	tptr = urlp;          /* end ptr */

	/* If URL is a directory, add '/' */
	if (tptr[-1] != '/') {
		if (is_directory(urlcopy + 1, 1, &sb)) {
			found_moved_temporarily = urlcopy;
		}
	}

	/* Log it */
	if (verbose > 1)
		bb_error_msg("url:%s", urlcopy);

	tptr = urlcopy;
	ip_allowed = checkPermIP();
	while (ip_allowed && (tptr = strchr(tptr + 1, '/')) != NULL) {
		/* have path1/path2 */
		*tptr = '\0';
		if (is_directory(urlcopy + 1, 1, &sb)) {
			/* may be having subdir config */
			parse_conf(urlcopy + 1, SUBDIR_PARSE);
			ip_allowed = checkPermIP();
		}
		*tptr = '/';
	}

#if ENABLE_FEATURE_HTTPD_PROXY
	proxy_entry = find_proxy_entry(urlcopy);
	if (proxy_entry)
		header_buf = header_ptr = xmalloc(IOBUF_SIZE);
#endif

	if (http_major_version >= '0') {
		/* Request was with "... HTTP/nXXX", and n >= 0 */

		/* Read until blank line for HTTP version specified, else parse immediate */
		while (1) {
			alarm(HEADER_READ_TIMEOUT);
			if (!get_line())
				break; /* EOF or error or empty line */
			if (DEBUG)
				bb_error_msg("header: '%s'", iobuf);

#if ENABLE_FEATURE_HTTPD_PROXY
			/* We need 2 more bytes for yet another "\r\n" -
			 * see near fdprintf(proxy_fd...) further below */
			if (proxy_entry && (header_ptr - header_buf) < IOBUF_SIZE - 2) {
				int len = strlen(iobuf);
				if (len > IOBUF_SIZE - (header_ptr - header_buf) - 4)
					len = IOBUF_SIZE - (header_ptr - header_buf) - 4;
				memcpy(header_ptr, iobuf, len);
				header_ptr += len;
				header_ptr[0] = '\r';
				header_ptr[1] = '\n';
				header_ptr += 2;
			}
#endif

#if ENABLE_FEATURE_HTTPD_CGI || ENABLE_FEATURE_HTTPD_PROXY
			/* Try and do our best to parse more lines */
			if ((STRNCASECMP(iobuf, "Content-length:") == 0)) {
				/* extra read only for POST */
				if (prequest != request_GET) {
					tptr = iobuf + sizeof("Content-length:") - 1;
					if (!tptr[0])
						send_headers_and_exit(HTTP_BAD_REQUEST);
					errno = 0;
					/* not using strtoul: it ignores leading minus! */
					length = strtol(tptr, &tptr, 10);
					/* length is "ulong", but we need to pass it to int later */
					/* so we check for negative or too large values in one go: */
					/* (long -> ulong conv caused negatives to be seen as > INT_MAX) */
					if (tptr[0] || errno || length > INT_MAX)
						send_headers_and_exit(HTTP_BAD_REQUEST);
				}
			}
#endif
#if ENABLE_FEATURE_HTTPD_CGI
			else if (STRNCASECMP(iobuf, "Cookie:") == 0) {
				cookie = strdup(skip_whitespace(iobuf + sizeof("Cookie:")-1));
			} else if (STRNCASECMP(iobuf, "Content-Type:") == 0) {
				content_type = strdup(skip_whitespace(iobuf + sizeof("Content-Type:")-1));
			} else if (STRNCASECMP(iobuf, "Referer:") == 0) {
				referer = strdup(skip_whitespace(iobuf + sizeof("Referer:")-1));
			} else if (STRNCASECMP(iobuf, "User-Agent:") == 0) {
				user_agent = strdup(skip_whitespace(iobuf + sizeof("User-Agent:")-1));
			}
#endif
#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
			if (STRNCASECMP(iobuf, "Authorization:") == 0) {
				/* We only allow Basic credentials.
				 * It shows up as "Authorization: Basic <userid:password>" where
				 * the userid:password is base64 encoded.
				 */
				tptr = skip_whitespace(iobuf + sizeof("Authorization:")-1);
				if (STRNCASECMP(tptr, "Basic") != 0)
					continue;
				tptr += sizeof("Basic")-1;
				/* decodeBase64() skips whitespace itself */
				decodeBase64(tptr);
				credentials = checkPerm(urlcopy, tptr);
			}
#endif          /* FEATURE_HTTPD_BASIC_AUTH */
#if ENABLE_FEATURE_HTTPD_RANGES
			if (STRNCASECMP(iobuf, "Range:") == 0) {
				/* We know only bytes=NNN-[MMM] */
				char *s = skip_whitespace(iobuf + sizeof("Range:")-1);
				if (strncmp(s, "bytes=", 6) == 0) {
					s += sizeof("bytes=")-1;
					range_start = BB_STRTOOFF(s, &s, 10);
					if (s[0] != '-' || range_start < 0) {
						range_start = 0;
					} else if (s[1]) {
						range_end = BB_STRTOOFF(s+1, NULL, 10);
						if (errno || range_end < range_start)
							range_start = 0;
					}
				}
			}
#endif
		} /* while extra header reading */
	}

	/* We are done reading headers, disable peer timeout */
	alarm(0);

	if (strcmp(bb_basename(urlcopy), httpd_conf) == 0 || ip_allowed == 0) {
		/* protect listing [/path]/httpd_conf or IP deny */
		send_headers_and_exit(HTTP_FORBIDDEN);
	}

#if ENABLE_FEATURE_HTTPD_BASIC_AUTH
	if (credentials <= 0 && checkPerm(urlcopy, ":") == 0) {
		send_headers_and_exit(HTTP_UNAUTHORIZED);
	}
#endif

	if (found_moved_temporarily) {
		send_headers_and_exit(HTTP_MOVED_TEMPORARILY);
	}

#if ENABLE_FEATURE_HTTPD_PROXY
	if (proxy_entry != NULL) {
		int proxy_fd;
		len_and_sockaddr *lsa;

		proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (proxy_fd < 0)
			send_headers_and_exit(HTTP_INTERNAL_SERVER_ERROR);
		lsa = host2sockaddr(proxy_entry->host_port, 80);
		if (lsa == NULL)
			send_headers_and_exit(HTTP_INTERNAL_SERVER_ERROR);
		if (connect(proxy_fd, &lsa->sa, lsa->len) < 0)
			send_headers_and_exit(HTTP_INTERNAL_SERVER_ERROR);
		fdprintf(proxy_fd, "%s %s%s%s%s HTTP/%c.%c\r\n",
				prequest, /* GET or POST */
				proxy_entry->url_to, /* url part 1 */
				urlcopy + strlen(proxy_entry->url_from), /* url part 2 */
				(g_query ? "?" : ""), /* "?" (maybe) */
				(g_query ? g_query : ""), /* query string (maybe) */
				http_major_version, http_minor_version);
		header_ptr[0] = '\r';
		header_ptr[1] = '\n';
		header_ptr += 2;
		write(proxy_fd, header_buf, header_ptr - header_buf);
		free(header_buf); /* on the order of 8k, free it */
		/* cgi_io_loop_and_exit needs to have two disctinct fds */
		cgi_io_loop_and_exit(proxy_fd, dup(proxy_fd), length);
	}
#endif

	tptr = urlcopy + 1;      /* skip first '/' */

#if ENABLE_FEATURE_HTTPD_CGI
	if (strncmp(tptr, "cgi-bin/", 8) == 0) {
		if (tptr[8] == '\0') {
			/* protect listing "cgi-bin/" */
			send_headers_and_exit(HTTP_FORBIDDEN);
		}
		send_cgi_and_exit(urlcopy, prequest, length, cookie, content_type);
	}
#if ENABLE_FEATURE_HTTPD_CONFIG_WITH_SCRIPT_INTERPR
	{
		char *suffix = strrchr(tptr, '.');
		if (suffix) {
			Htaccess *cur;
			for (cur = script_i; cur; cur = cur->next) {
				if (strcmp(cur->before_colon + 1, suffix) == 0) {
					send_cgi_and_exit(urlcopy, prequest, length, cookie, content_type);
				}
			}
		}
	}
#endif
	if (prequest != request_GET) {
		send_headers_and_exit(HTTP_NOT_IMPLEMENTED);
	}
#endif  /* FEATURE_HTTPD_CGI */

	if (urlp[-1] == '/')
		strcpy(urlp, "index.html");
	if (stat(tptr, &sb) == 0) {
		file_size = sb.st_size;
		last_mod = sb.st_mtime;
	}
#if ENABLE_FEATURE_HTTPD_CGI
	else if (urlp[-1] == '/') {
		/* It's a dir URL and there is no index.html
		 * Try cgi-bin/index.cgi */
		if (access("/cgi-bin/index.cgi"+1, X_OK) == 0) {
			urlp[0] = '\0';
			g_query = urlcopy;
			send_cgi_and_exit("/cgi-bin/index.cgi", prequest, length, cookie, content_type);
		}
	}
#endif
	/* else {
	 *	fall through to send_file, it errors out if open fails
	 * }
	 */

	send_file_and_exit(tptr, TRUE);
}

/*
 * The main http server function.
 * Given a socket, listen for new connections and farm out
 * the processing as a [v]forked process.
 * Never returns.
 */
#if BB_MMU
static void mini_httpd(int server_socket) ATTRIBUTE_NORETURN;
static void mini_httpd(int server_socket)
{
	/* NB: it's best to not use xfuncs in this loop before fork().
	 * Otherwise server may die on transient errors (temporary
	 * out-of-memory condition, etc), which is Bad(tm).
	 * Try to do any dangerous calls after fork.
	 */
	while (1) {
		int n;
		len_and_sockaddr fromAddr;

		/* Wait for connections... */
		fromAddr.len = LSA_SIZEOF_SA;
		n = accept(server_socket, &fromAddr.sa, &fromAddr.len);

		if (n < 0)
			continue;
		/* set the KEEPALIVE option to cull dead connections */
		setsockopt(n, SOL_SOCKET, SO_KEEPALIVE, &const_int_1, sizeof(const_int_1));

		if (fork() == 0) {
			/* child */
#if ENABLE_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
			/* Do not reload config on HUP */
			signal(SIGHUP, SIG_IGN);
#endif
			close(server_socket);
			xmove_fd(n, 0);
			xdup2(0, 1);

			handle_incoming_and_exit(&fromAddr);
		}
		/* parent, or fork failed */
		close(n);
	} /* while (1) */
	/* never reached */
}
#else
static void mini_httpd_nommu(int server_socket, int argc, char **argv) ATTRIBUTE_NORETURN;
static void mini_httpd_nommu(int server_socket, int argc, char **argv)
{
	char *argv_copy[argc + 2];

	argv_copy[0] = argv[0];
	argv_copy[1] = (char*)"-i";
	memcpy(&argv_copy[2], &argv[1], argc * sizeof(argv[0]));

	/* NB: it's best to not use xfuncs in this loop before vfork().
	 * Otherwise server may die on transient errors (temporary
	 * out-of-memory condition, etc), which is Bad(tm).
	 * Try to do any dangerous calls after fork.
	 */
	while (1) {
		int n;
		len_and_sockaddr fromAddr;

		/* Wait for connections... */
		fromAddr.len = LSA_SIZEOF_SA;
		n = accept(server_socket, &fromAddr.sa, &fromAddr.len);

		if (n < 0)
			continue;
		/* set the KEEPALIVE option to cull dead connections */
		setsockopt(n, SOL_SOCKET, SO_KEEPALIVE, &const_int_1, sizeof(const_int_1));

		if (vfork() == 0) {
			/* child */
#if ENABLE_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
			/* Do not reload config on HUP */
			signal(SIGHUP, SIG_IGN);
#endif
			close(server_socket);
			xmove_fd(n, 0);
			xdup2(0, 1);

			/* Run a copy of ourself in inetd mode */
			re_exec(argv_copy);
		}
		/* parent, or vfork failed */
		close(n);
	} /* while (1) */
	/* never reached */
}
#endif

/*
 * Process a HTTP connection on stdin/out.
 * Never returns.
 */
static void mini_httpd_inetd(void) ATTRIBUTE_NORETURN;
static void mini_httpd_inetd(void)
{
	len_and_sockaddr fromAddr;

	fromAddr.len = LSA_SIZEOF_SA;
	getpeername(0, &fromAddr.sa, &fromAddr.len);
	handle_incoming_and_exit(&fromAddr);
}

#if ENABLE_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
static void sighup_handler(int sig)
{
	struct sigaction sa;

	parse_conf(default_path_httpd_conf, sig == SIGHUP ? SIGNALED_PARSE : FIRST_PARSE);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sighup_handler;
	/*sigemptyset(&sa.sa_mask); - memset should be enough */
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
	p_opt_verbose   ,
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
	OPT_VERBOSE     = 1 << p_opt_verbose,
};


int httpd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int httpd_main(int argc, char **argv)
{
	int server_socket = server_socket; /* for gcc */
	unsigned opt;
	char *url_for_decode;
	USE_FEATURE_HTTPD_ENCODE_URL_STR(const char *url_for_encode;)
	USE_FEATURE_HTTPD_SETUID(const char *s_ugid = NULL;)
	USE_FEATURE_HTTPD_SETUID(struct bb_uidgid_t ugid;)
	USE_FEATURE_HTTPD_AUTH_MD5(const char *pass;)

	INIT_G();

#if ENABLE_LOCALE_SUPPORT
	/* Undo busybox.c: we want to speak English in http (dates etc) */
	setlocale(LC_TIME, "C");
#endif

	home_httpd = xrealloc_getcwd_or_warn(NULL);
	/* -v counts, -i implies -f */
	opt_complementary = "vv:if";
	/* We do not "absolutize" path given by -h (home) opt.
	 * If user gives relative path in -h, $SCRIPT_FILENAME can end up
	 * relative too. */
	opt = getopt32(argv, "c:d:h:"
			USE_FEATURE_HTTPD_ENCODE_URL_STR("e:")
			USE_FEATURE_HTTPD_BASIC_AUTH("r:")
			USE_FEATURE_HTTPD_AUTH_MD5("m:")
			USE_FEATURE_HTTPD_SETUID("u:")
			"p:ifv",
			&configFile, &url_for_decode, &home_httpd
			USE_FEATURE_HTTPD_ENCODE_URL_STR(, &url_for_encode)
			USE_FEATURE_HTTPD_BASIC_AUTH(, &g_realm)
			USE_FEATURE_HTTPD_AUTH_MD5(, &pass)
			USE_FEATURE_HTTPD_SETUID(, &s_ugid)
			, &bind_addr_or_port
			, &verbose
		);
	if (opt & OPT_DECODE_URL) {
		fputs(decodeString(url_for_decode, 1), stdout);
		return 0;
	}
#if ENABLE_FEATURE_HTTPD_ENCODE_URL_STR
	if (opt & OPT_ENCODE_URL) {
		fputs(encodeString(url_for_encode), stdout);
		return 0;
	}
#endif
#if ENABLE_FEATURE_HTTPD_AUTH_MD5
	if (opt & OPT_MD5) {
		puts(pw_encrypt(pass, "$1$"));
		return 0;
	}
#endif
#if ENABLE_FEATURE_HTTPD_SETUID
	if (opt & OPT_SETUID) {
		if (!get_uidgid(&ugid, s_ugid, 1))
			bb_error_msg_and_die("unrecognized user[:group] "
						"name '%s'", s_ugid);
	}
#endif

#if !BB_MMU
	if (!(opt & OPT_FOREGROUND)) {
		bb_daemonize_or_rexec(0, argv); /* don't change current directory */
	}
#endif

	xchdir(home_httpd);
	if (!(opt & OPT_INETD)) {
		signal(SIGCHLD, SIG_IGN);
		server_socket = openServer();
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

#if 0 /*was #if ENABLE_FEATURE_HTTPD_CGI*/
	/* User can do it himself: 'env - PATH="$PATH" httpd'
	 * We don't do it because we don't want to screw users
	 * which want to do
	 * 'env - VAR1=val1 VAR2=val2 httpd'
	 * and have VAR1 and VAR2 values visible in their CGIs.
	 * Besides, it is also smaller. */
	{
		char *p = getenv("PATH");
		/* env strings themself are not freed, no need to strdup(p): */
		clearenv();
		if (p)
			putenv(p - 5);
//		if (!(opt & OPT_INETD))
//			setenv_long("SERVER_PORT", ???);
	}
#endif

#if ENABLE_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
	if (!(opt & OPT_INETD))
		sighup_handler(0);
	else /* do not install HUP handler in inetd mode */
#endif
		parse_conf(default_path_httpd_conf, FIRST_PARSE);

	xfunc_error_retval = 0;
	if (opt & OPT_INETD)
		mini_httpd_inetd();
#if BB_MMU
	if (!(opt & OPT_FOREGROUND))
		bb_daemonize(0); /* don't change current directory */
	mini_httpd(server_socket); /* never returns */
#else
	mini_httpd_nommu(server_socket, argc, argv); /* never returns */
#endif
	/* return 0; */
}
