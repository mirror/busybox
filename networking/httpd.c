/*
 * httpd implementation for busybox
 *
 * Copyright (C) 2002,2003 Glenn Engel <glenne@engel.org>
 * Copyright (C) 2003 Vladimir Oleynik <dzo@simtreas.ru>
 *
 * simplify patch stolen from libbb without using strdup
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
 * after setting QUERY_STRING and other environment variables.  If url args
 * are included in the url or as a post, the args are placed into decoded
 * environment variables.  e.g. /cgi-bin/setup?foo=Hello%20World  will set
 * the $CGI_foo environment variable to "Hello World" while
 * CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV enabled.
 *
 * The server can also be invoked as a url arg decoder and html text encoder
 * as follows:
 *  foo=`httpd -d $foo`             # decode "Hello%20World" as "Hello World"
 *  bar=`httpd -e "<Hello World>"`  # encode as "&#60Hello&#32World&#62"
 *
 * httpd.conf has the following format:

A:172.20.         # Allow any address that begins with 172.20
A:10.10.          # Allow any address that begins with 10.10.
A:10.10           # Allow any address that previous set and 10.100-109.X.X
A:127.0.0.1       # Allow local loopback connections
D:*               # Deny from other IP connections
/cgi-bin:foo:bar  # Require user foo, pwd bar on urls starting with /cgi-bin/
/adm:admin:setup  # Require user admin, pwd setup on urls starting with /adm/
/adm:toor:PaSsWd  # or user toor, pwd PaSsWd on urls starting with /adm/
.au:audio/basic   # additional mime type for audio.au files

A/D may be as a/d or allow/deny - first char case unsensitive parsed only.

Each subdir can have config file.
You can set less IP allow from subdir config.
Password protection from subdir config can rewriten previous sets for
current or/and next subpathes.
For protect as user:pass current subdir and subpathes set from subdir config:
/:user:pass
/subpath:user2:pass2

 If -c don`t setted, used httpd root config, else httpd root config skiped.
 */

#include <stdio.h>
#include <ctype.h>         /* for isspace           */
#include <string.h>
#include <stdlib.h>        /* for malloc            */
#include <time.h>
#include <unistd.h>        /* for close             */
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>    /* for connect and socket*/
#include <netinet/in.h>    /* for sockaddr_in       */
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>         /* for open modes        */
#include "busybox.h"


static const char httpdVersion[] = "busybox httpd/1.25 10-May-2003";
static const char default_path_httpd_conf[] = "/etc";
static const char httpd_conf[] = "httpd.conf";
static const char home[] = "/www";

// Note: bussybox xfuncs are not used because we want the server to keep running
//       if something bad happens due to a malformed user request.
//       As a result, all memory allocation after daemonize
//       is checked rigorously

//#define DEBUG 1

/* Configure options, disabled by default as custom httpd feature */

/* disabled as optional features */
//#define CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV
//#define CONFIG_FEATURE_HTTPD_ENCODE_URL_STR
//#define CONFIG_FEATURE_HTTPD_SET_REMOTE_PORT_TO_ENV
//#define CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
//#define CONFIG_FEATURE_HTTPD_SETUID
//#define CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP

/* If set, use this server from internet superserver only */
//#define CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY

/* You can use this server as standalone, require libbb.a for linking */
//#define HTTPD_STANDALONE

/* Config options, disable this for do very small module */
//#define CONFIG_FEATURE_HTTPD_CGI
//#define CONFIG_FEATURE_HTTPD_BASIC_AUTH

#ifdef HTTPD_STANDALONE
/* standalone, enable all features */
#undef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
/* unset config option for remove warning as redefined */
#undef CONFIG_FEATURE_HTTPD_BASIC_AUTH
#undef CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV
#undef CONFIG_FEATURE_HTTPD_ENCODE_URL_STR
#undef CONFIG_FEATURE_HTTPD_SET_REMOTE_PORT_TO_ENV
#undef CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
#undef CONFIG_FEATURE_HTTPD_CGI
#undef CONFIG_FEATURE_HTTPD_SETUID
#undef CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
/* enable all features now */
#define CONFIG_FEATURE_HTTPD_BASIC_AUTH
#define CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV
#define CONFIG_FEATURE_HTTPD_ENCODE_URL_STR
#define CONFIG_FEATURE_HTTPD_SET_REMOTE_PORT_TO_ENV
#define CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
#define CONFIG_FEATURE_HTTPD_CGI
#define CONFIG_FEATURE_HTTPD_SETUID
#define CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP

/* require from libbb.a for linking */
const char *bb_applet_name = "httpd";

void bb_show_usage(void)
{
  fprintf(stderr, "Usage: %s [-p <port>] [-c configFile] [-d/-e <string>] "
		  "[-r realm] [-u user] [-h homedir]\n", bb_applet_name);
  exit(1);
}
#endif

#ifdef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
#undef CONFIG_FEATURE_HTTPD_SETUID  /* use inetd user.group config settings */
#undef CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP    /* so is not daemon */
/* inetd set stderr to accepted socket and we can`t true see debug messages */
#undef DEBUG
#endif

/* CGI environ size */
#ifdef CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV
#define ENVSIZE 50    /* set max 35 CGI_variable */
#else
#define ENVSIZE 15    /* minimal requires */
#endif

#define MAX_POST_SIZE (64*1024) /* 64k. Its Small? May be ;) */

#define MAX_MEMORY_BUFF 8192    /* IO buffer */

typedef struct HT_ACCESS {
	char *after_colon;
	struct HT_ACCESS *next;
	char before_colon[1];         /* really bigger, must last */
} Htaccess;

typedef struct
{
#ifdef CONFIG_FEATURE_HTTPD_CGI
  char *envp[ENVSIZE+1];
  int envCount;
#endif
  char buf[MAX_MEMORY_BUFF];

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
  const char *realm;
#endif
  const char *configFile;

  char rmt_ip[16];         /* for set env REMOTE_ADDR */
  unsigned port;           /* server initial port and for
			      set env REMOTE_PORT */

  const char *found_mime_type;
  off_t ContentLength;          /* -1 - unknown */
  time_t last_mod;

  Htaccess *ip_a_d;             /* config allow/deny lines */
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
  Htaccess *auth;               /* config user:password lines */
#endif
#ifdef CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
  Htaccess *mime_a;             /* config mime types */
#endif

#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
  int accepted_socket;
#define a_c_r config->accepted_socket
#define a_c_w config->accepted_socket
  int debugHttpd;          /* if seted, don`t stay daemon */
#else
#define a_c_r 0
#define a_c_w 1
#endif
} HttpdConfig;

static HttpdConfig *config;

static const char request_GET[] = "GET";    /* size algorithic optimize */

static const char* const suffixTable [] = {
/* Warning: shorted equalent suffix in one line must be first */
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

typedef enum
{
  HTTP_OK = 200,
  HTTP_UNAUTHORIZED = 401, /* authentication needed, respond with auth hdr */
  HTTP_NOT_FOUND = 404,
  HTTP_NOT_IMPLEMENTED = 501,   /* used for unrecognized requests */
  HTTP_BAD_REQUEST = 400,       /* malformed syntax */
  HTTP_FORBIDDEN = 403,
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
  HTTP_MOVED_TEMPORARILY = 302,
  HTTP_NOT_MODIFIED = 304,
  HTTP_PAYMENT_REQUIRED = 402,
  HTTP_BAD_GATEWAY = 502,
  HTTP_SERVICE_UNAVAILABLE = 503, /* overload, maintenance */
  HTTP_RESPONSE_SETSIZE=0xffffffff
#endif
} HttpResponseNum;

typedef struct
{
  HttpResponseNum type;
  const char *name;
  const char *info;
} HttpEnumString;

static const HttpEnumString httpResponseNames[] = {
  { HTTP_OK, "OK" },
  { HTTP_NOT_IMPLEMENTED, "Not Implemented",
    "The requested method is not recognized by this server." },
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
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
  { HTTP_MOVED_TEMPORARILY, "Moved Temporarily" },
  { HTTP_NOT_MODIFIED, "Not Modified" },
  { HTTP_BAD_GATEWAY, "Bad Gateway", "" },
  { HTTP_SERVICE_UNAVAILABLE, "Service Unavailable", "" },
#endif
};


static const char RFC1123FMT[] = "%a, %d %b %Y %H:%M:%S GMT";
static const char Content_length[] = "Content-length:";



static void free_config_lines(Htaccess **pprev)
{
    Htaccess *prev = *pprev;

    while( prev ) {
	Htaccess *cur = prev;

	prev = cur->next;
	free(cur);
    }
    *pprev = NULL;
}

static void add_config_line(Htaccess **pprev, Htaccess *cur)
{
    if(*pprev == NULL) {
	*pprev = cur;
    } else {
	Htaccess *prev;

	for(prev = *pprev; prev->next; prev = prev->next)
		;
	prev->next = cur;
    }
}

/* flag */
#define FIRST_PARSE          0
#define SUBDIR_PARSE         1
#define SIGNALED_PARSE       2
#define FIND_FROM_HTTPD_ROOT 3

static void parse_conf(const char *path, int flag)
{
    FILE *f;
    Htaccess *cur;
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
    Htaccess *prev;
#endif

    const char *cf = config->configFile;
    char buf[80];
    char *p0 = NULL;
    char *c, *p;

    /* free previous setuped */
    free_config_lines(&config->ip_a_d);
    if(flag != SUBDIR_PARSE) {
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
	free_config_lines(&config->auth)
#endif
	;   /* syntax confuse */
#ifdef CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
	free_config_lines(&config->mime_a);
#endif
    }

    if(flag == SUBDIR_PARSE || cf == NULL) {
	cf = alloca(strlen(path) + sizeof(httpd_conf) + 2);
	if(cf == NULL) {
	    if(flag == FIRST_PARSE)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	    return;
	}
	sprintf((char *)cf, "%s/%s", path, httpd_conf);
    }

    while((f = fopen(cf, "r")) == NULL) {
	if(flag != FIRST_PARSE) {
	    /* config file not found */
	    return;
	}
	if(config->configFile)      /* if -c option given */
	    bb_perror_msg_and_die("%s", cf);
	flag = FIND_FROM_HTTPD_ROOT;
	cf = httpd_conf;
    }

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
    prev = config->auth;
#endif
    /* This could stand some work */
    while ( (p0 = fgets(buf, 80, f)) != NULL) {
	c = NULL;
	for(p = p0; *p0 != 0 && *p0 != '#'; p0++) {
		if(!isspace(*p0)) {
		    *p++ = *p0;
		    if(*p0 == ':' && c == NULL)
			c = p;
		}
	}
	*p = 0;

	/* test for empty or strange line */
	if (c == NULL || *c == 0)
	    continue;
	if(*c == '*')
	    *c = 0;   /* Allow all */
	p0 = buf;
	if(*p0 == 'a')
	    *p0 = 'A';
	if(*p0 == 'd')
	    *p0 = 'D';
	if(*p0 != 'A' && *p0 != 'D'
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
				    && *p0 != '/'
#endif
#ifdef CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
					&& *p0 != '.'
#endif
						       )
	       continue;

	if(*p0 == 'A' && *c == 0) {
	    /* skip default A:* */
	    continue;
	}
	p0 = buf;
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
	if(*p0 == '/') {
	    if(*c == 0) {
		/* skip /path:* */
		continue;
	    }
	    /* make full path from httpd root / curent_path / config_line_path */
	    cf = flag == SUBDIR_PARSE ? path : "";
	    p0 = malloc(strlen(cf) + (c - buf) + 2 + strlen(c));
	    if(p0 == NULL)
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
				    while (*--p != '/');    /* omit previous dir */
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
	/* storing current config line */

	cur = calloc(1, sizeof(Htaccess) + strlen(p0));
	if(cur) {
	    cf = strcpy(cur->before_colon, p0);
	    c = strchr(cf, ':');
	    *c++ = 0;
	    cur->after_colon = c;
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
	    if(*cf == '/')
		free(p0);
#endif
	    if(*cf == 'A' || *cf == 'D')
		add_config_line(&config->ip_a_d, cur);
#ifdef CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
	    else if(*cf == '.')
		add_config_line(&config->mime_a, cur);
#endif

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
	    else if(prev == NULL) {
		/* first line */
		config->auth = prev = cur;
	    } else {
		/* sort path, if current lenght eq or bigger then move up */
		Htaccess *prev_hti = config->auth;
		int l = strlen(cf);
		Htaccess *hti;

		for(hti = prev_hti; hti; hti = hti->next) {
		    if(l >= strlen(hti->before_colon)) {
			/* insert before hti */
			cur->next = hti;
			if(prev_hti != hti) {
			    prev_hti->next = cur;
			    break;
			} else {
			    /* insert as top */
			    config->auth = cur;
			    break;
			}
		    }
		    if(prev_hti != hti)
			    prev_hti = prev_hti->next;
		}
		if(!hti)  {       /* not inserted, add to bottom */
		    prev->next = cur;
		    prev = cur;
		}
	    }
#endif
	}
   }
   fclose(f);
}

#ifdef CONFIG_FEATURE_HTTPD_ENCODE_URL_STR
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
  char *out = malloc(len*5 +1);
  char *p=out;
  char ch;

  if (!out) return "";
  while ((ch = *string++)) {
    // very simple check for what to encode
    if (isalnum(ch)) *p++ = ch;
    else p += sprintf(p, "&#%d", (unsigned char) ch);
  }
  *p=0;
  return out;
}
#endif          /* CONFIG_FEATURE_HTTPD_ENCODE_URL_STR */

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
 *      (int)    flag   . . . 1 if require decode '+' as ' ' for CGI
 *
 * $Return: (char *)  . . . . A pointer to the decoded string (same as input).
 *
 * $Errors: None
 *
 ****************************************************************************/
static char *decodeString(char *string, int flag_plus_to_space)
{
  /* note that decoded string is always shorter than original */
  char *orig = string;
  char *ptr = string;
  while (*ptr)
  {
    if (*ptr == '+' && flag_plus_to_space)    { *string++ = ' '; ptr++; }
    else if (*ptr != '%') *string++ = *ptr++;
    else  {
      unsigned int value;
      sscanf(ptr+1, "%2X", &value);
      *string++ = value;
      ptr += 3;
    }
  }
  *string = '\0';
  return orig;
}


#ifdef CONFIG_FEATURE_HTTPD_CGI
/****************************************************************************
 *
 > $Function: addEnv()
 *
 * $Description: Add an enviornment variable setting to the global list.
 *    A NAME=VALUE string is allocated, filled, and added to the list of
 *    environment settings passed to the cgi execution script.
 *
 * $Parameters:
 *  (char *) name_before_underline - The first part environment variable name.
 *  (char *) name_after_underline  - The second part environment variable name.
 *  (char *) value  . . The value to which the env variable is set.
 *
 * $Return: (void)
 *
 * $Errors: Silently returns if the env runs out of space to hold the new item
 *
 ****************************************************************************/
static void addEnv(const char *name_before_underline,
			const char *name_after_underline, const char *value)
{
  char *s;

  if (config->envCount >= ENVSIZE)
	return;
  if (!value)
	value = "";
  s = malloc(strlen(name_before_underline) + strlen(name_after_underline) +
			strlen(value) + 3);
  if (s) {
    const char *underline = *name_after_underline ? "_" : "";

    sprintf(s,"%s%s%s=%s", name_before_underline, underline,
					name_after_underline, value);
    config->envp[config->envCount++] = s;
    config->envp[config->envCount] = 0;
  }
}

/* set environs SERVER_PORT and REMOTE_PORT */
static void addEnvPort(const char *port_name)
{
      char buf[16];

      sprintf(buf, "%u", config->port);
      addEnv(port_name, "PORT", buf);
}
#endif          /* CONFIG_FEATURE_HTTPD_CGI */

#ifdef CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV
/****************************************************************************
 *
 > $Function: addEnvCgi
 *
 * $Description: Create environment variables given a URL encoded arg list.
 *   For each variable setting the URL encoded arg list, create a corresponding
 *   environment variable.  URL encoded arguments have the form
 *      name1=value1&name2=value2&name3=&ignores
 *       from this example, name3 set empty value, tail without '=' skiping
 *
 * $Parameters:
 *      (char *) pargs . . . . A pointer to the URL encoded arguments.
 *
 * $Return: None
 *
 * $Errors: None
 *
 ****************************************************************************/
static void addEnvCgi(const char *pargs)
{
  char *args;
  char *memargs;
  if (pargs==0) return;

  /* args are a list of name=value&name2=value2 sequences */
  memargs = args = strdup(pargs);
  while (args && *args) {
    const char *name = args;
    char *value = strchr(args, '=');

    if (!value)         /* &XXX without '=' */
	break;
    *value++ = 0;
    args = strchr(value, '&');
    if (args)
	*args++ = 0;
    addEnv("CGI", name, decodeString(value, 1));
  }
  free(memargs);
}
#endif /* CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV */


#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
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

  const unsigned char *in = Data;
  // The decoded size will be at most 3/4 the size of the encoded
  unsigned long ch = 0;
  int i = 0;

  while (*in) {
    int t = *in++;

    switch(t) {
	case '+':
		t = 62;
		break;
	case '/':
		t = 63;
		break;
	case '=':
		t = 0;
		break;
	case 'A' ... 'Z':
		t = t - 'A';
		break;
	case 'a' ... 'z':
		t = t - 'a' + 26;
		break;
	case '0' ... '9':
		t = t - '0' + 52;
		break;
	default:
		continue;
    }
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


#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
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
  struct sockaddr_in lsocket;
  int fd;

  /* create the socket right now */
  /* inet_addr() returns a value that is already in network order */
  memset(&lsocket, 0, sizeof(lsocket));
  lsocket.sin_family = AF_INET;
  lsocket.sin_addr.s_addr = INADDR_ANY;
  lsocket.sin_port = htons(config->port) ;
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd >= 0) {
    /* tell the OS it's OK to reuse a previous address even though */
    /* it may still be in a close down state.  Allows bind to succeed. */
    int on = 1;
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (void *)&on, sizeof(on)) ;
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on)) ;
#endif
    if (bind(fd, (struct sockaddr *)&lsocket, sizeof(lsocket)) == 0) {
      listen(fd, 9);
      signal(SIGCHLD, SIG_IGN);   /* prevent zombie (defunct) processes */
    } else {
	bb_perror_msg_and_die("bind");
    }
  } else {
	bb_perror_msg_and_die("create socket");
  }
  return fd;
}
#endif  /* CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY */

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
  unsigned int i;
  time_t timer = time(0);
  char timeStr[80];
  int len;

  for (i = 0;
	i < (sizeof(httpResponseNames)/sizeof(httpResponseNames[0])); i++) {
		if (httpResponseNames[i].type == responseNum) {
			responseString = httpResponseNames[i].name;
			infoString = httpResponseNames[i].info;
			break;
		}
  }
  if (responseNum != HTTP_OK) {
	config->found_mime_type = "text/html";  // error message is HTML
  }

  /* emit the current date */
  strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&timer));
  len = sprintf(buf,
	"HTTP/1.0 %d %s\nContent-type: %s\r\n"
	"Date: %s\r\nConnection: close\r\n",
	  responseNum, responseString, config->found_mime_type, timeStr);

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
  if (responseNum == HTTP_UNAUTHORIZED) {
    len += sprintf(buf+len, "WWW-Authenticate: Basic realm=\"%s\"\r\n",
							    config->realm);
  }
#endif
  if (config->ContentLength != -1) {    /* file */
    strftime(timeStr, sizeof(timeStr), RFC1123FMT, gmtime(&config->last_mod));
    len += sprintf(buf+len, "Last-Modified: %s\r\n%s %ld\r\n",
			      timeStr, Content_length, config->ContentLength);
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
#ifdef DEBUG
  if (config->debugHttpd) fprintf(stderr, "Headers: '%s'", buf);
#endif
  return bb_full_write(a_c_w, buf, len);
}

/****************************************************************************
 *
 > $Function: getLine()
 *
 * $Description: Read from the socket until an end of line char found.
 *
 *   Characters are read one at a time until an eol sequence is found.
 *
 * $Parameters:
 *      (char *) buf  . . Where to place the read result.
 *
 * $Return: (int) . . . . number of characters read.  -1 if error.
 *
 ****************************************************************************/
static int getLine(char *buf)
{
  int  count = 0;

  while (read(a_c_r, buf + count, 1) == 1) {
    if (buf[count] == '\r') continue;
    if (buf[count] == '\n') {
      buf[count] = 0;
      return count;
    }
    if(count < (MAX_MEMORY_BUFF-1))      /* check owerflow */
	count++;
  }
  if (count) return count;
  else return -1;
}

#ifdef CONFIG_FEATURE_HTTPD_CGI
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
 *      (const char *) url . . . The requested URL (with leading /).
 *      (const char *urlArgs). . Any URL arguments.
 *      (const char *body) . . . POST body contents.
 *      (int bodyLen)  . . . . . Length of the post body.
 *      (const char *cookie) . . For set HTTP_COOKIE.

 *
 * $Return: (char *)  . . . . A pointer to the decoded string (same as input).
 *
 * $Errors: None
 *
 ****************************************************************************/
static int sendCgi(const char *url,
		   const char *request, const char *urlArgs,
		   const char *body, int bodyLen, const char *cookie)
{
  int fromCgi[2];  /* pipe for reading data from CGI */
  int toCgi[2];    /* pipe for sending data to CGI */

  static char * argp[] = { 0, 0 };
  int pid = 0;
  int inFd;
  int outFd;
  int firstLine = 1;

  do {
    if (pipe(fromCgi) != 0) {
      break;
    }
    if (pipe(toCgi) != 0) {
      break;
    }

    pid = fork();
    if (pid < 0) {
	pid = 0;
	break;
    }

    if (!pid) {
      /* child process */
      char *script;
      char *purl = strdup( url );
      char realpath_buff[MAXPATHLEN];

      if(purl == NULL)
	_exit(242);

      inFd  = toCgi[0];
      outFd = fromCgi[1];

      dup2(inFd, 0);  // replace stdin with the pipe
      dup2(outFd, 1);  // replace stdout with the pipe

#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
      if (!config->debugHttpd)
#endif
	dup2(outFd, 2);  // replace stderr with the pipe

      close(toCgi[0]);
      close(toCgi[1]);
      close(fromCgi[0]);
      close(fromCgi[1]);

      /*
       * Find PATH_INFO.
       */
      script = purl;
      while((script = strchr( script + 1, '/' )) != NULL) {
	/* have script.cgi/PATH_INFO or dirs/script.cgi[/PATH_INFO] */
	struct stat sb;

	*script = '\0';
	if(is_directory(purl + 1, 1, &sb) == 0) {
		/* not directory, found script.cgi/PATH_INFO */
		*script = '/';
		break;
	}
	*script = '/';          /* is directory, find next '/' */
      }
      addEnv("PATH", "INFO", script);   /* set /PATH_INFO or NULL */
      addEnv("PATH",           "",         getenv("PATH"));
      addEnv("REQUEST",        "METHOD",   request);
      if(urlArgs) {
	char *uri = alloca(strlen(purl) + 2 + strlen(urlArgs));
	if(uri)
	    sprintf(uri, "%s?%s", purl, urlArgs);
	addEnv("REQUEST",        "URI",   uri);
      } else {
	addEnv("REQUEST",        "URI",   purl);
      }
      if(script != NULL)
	*script = '\0';         /* reduce /PATH_INFO */
      /* set SCRIPT_NAME as full path: /cgi-bin/dirs/script.cgi */
      addEnv("SCRIPT_NAME",    "",         purl);
      addEnv("QUERY_STRING",   "",         urlArgs);
      addEnv("SERVER",         "SOFTWARE", httpdVersion);
      addEnv("SERVER",         "PROTOCOL", "HTTP/1.0");
      addEnv("GATEWAY_INTERFACE", "",      "CGI/1.1");
#ifdef CONFIG_FEATURE_HTTPD_SET_REMOTE_PORT_TO_ENV
      addEnv("REMOTE",         "ADDR",     config->rmt_ip);
      addEnvPort("REMOTE");
#else
      addEnv("REMOTE_ADDR",     "",        config->rmt_ip);
#endif
      if(bodyLen) {
	char sbl[32];

	sprintf(sbl, "%d", bodyLen);
	addEnv("CONTENT_LENGTH", "", sbl);
      }
      if(cookie)
	addEnv("HTTP_COOKIE", "", cookie);

#ifdef CONFIG_FEATURE_HTTPD_SET_CGI_VARS_TO_ENV
      if (request != request_GET) {
	addEnvCgi(body);
      } else {
	addEnvCgi(urlArgs);
      }
#endif

	/* set execve argp[0] without path */
      argp[0] = strrchr( purl, '/' ) + 1;
	/* but script argp[0] must have absolute path and chdiring to this */
      if(realpath(purl + 1, realpath_buff) != NULL) {
	    script = strrchr(realpath_buff, '/');
	    if(script) {
		*script = '\0';
		if(chdir(realpath_buff) == 0) {
		    *script = '/';
      // now run the program.  If it fails, use _exit() so no destructors
      // get called and make a mess.
		    execve(realpath_buff, argp, config->envp);
		}
	    }
      }
#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
      config->accepted_socket = 1;      /* send to stdout */
#endif
      sendHeaders(HTTP_NOT_FOUND);
      _exit(242);
    } /* end child */

  } while (0);

  if (pid) {
    /* parent process */
    int status;

    inFd  = fromCgi[0];
    outFd = toCgi[1];
    close(fromCgi[1]);
    close(toCgi[0]);
    if (body) bb_full_write(outFd, body, bodyLen);
    close(outFd);

    while (1) {
      struct timeval timeout;
      fd_set readSet;
      char buf[160];
      int nfound;
      int count;

      FD_ZERO(&readSet);
      FD_SET(inFd, &readSet);

      /* Now wait on the set of sockets! */
      timeout.tv_sec = 0;
      timeout.tv_usec = 10000;
      nfound = select(inFd + 1, &readSet, 0, 0, &timeout);

      if (nfound <= 0) {
	if (waitpid(pid, &status, WNOHANG) > 0) {
	  close(inFd);
#ifdef DEBUG
	  if (config->debugHttpd) {
	    if (WIFEXITED(status))
	      bb_error_msg("piped has exited with status=%d", WEXITSTATUS(status));
	    if (WIFSIGNALED(status))
	      bb_error_msg("piped has exited with signal=%d", WTERMSIG(status));
	  }
#endif
	  pid = -1;
	  break;
	}
      } else {
	int s = a_c_w;

	// There is something to read
	count = bb_full_read(inFd, buf, sizeof(buf)-1);
	// If a read returns 0 at this point then some type of error has
	// occurred.  Bail now.
	if (count == 0) break;
	if (count > 0) {
	  if (firstLine) {
	    /* check to see if the user script added headers */
	    if (strncmp(buf, "HTTP/1.0 200 OK\n", 4) != 0) {
	      bb_full_write(s, "HTTP/1.0 200 OK\n", 16);
	    }
	    if (strstr(buf, "ontent-") == 0) {
	      bb_full_write(s, "Content-type: text/plain\n\n", 26);
	    }
	    firstLine=0;
	  }
	  bb_full_write(s, buf, count);
#ifdef DEBUG
	  if (config->debugHttpd)
		fprintf(stderr, "cgi read %d bytes\n", count);
#endif
	}
      }
    }
  }
  return 0;
}
#endif          /* CONFIG_FEATURE_HTTPD_CGI */

/****************************************************************************
 *
 > $Function: sendFile()
 *
 * $Description: Send a file response to an HTTP request
 *
 * $Parameter:
 *      (const char *) url . . The URL requested.
 *      (char *) buf . . . . . The stack buffer.
 *
 * $Return: (int)  . . . . . . Always 0.
 *
 ****************************************************************************/
static int sendFile(const char *url, char *buf)
{
  char * suffix;
  int  f;
  const char * const * table;
  const char * try_suffix;

  suffix = strrchr(url, '.');

  for (table = suffixTable; *table; table += 2)
	if(suffix != NULL && (try_suffix = strstr(*table, suffix)) != 0) {
		try_suffix += strlen(suffix);
		if(*try_suffix == 0 || *try_suffix == '.')
			break;
	}
  /* also, if not found, set default as "application/octet-stream";  */
  config->found_mime_type = *(table+1);
#ifdef CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES
  if (suffix) {
    Htaccess * cur;

    for (cur = config->mime_a; cur; cur = cur->next) {
	if(strcmp(cur->before_colon, suffix) == 0) {
		config->found_mime_type = cur->after_colon;
		break;
	}
    }
  }
#endif  /* CONFIG_FEATURE_HTTPD_CONFIG_WITH_MIME_TYPES */

#ifdef DEBUG
    if (config->debugHttpd)
	fprintf(stderr, "Sending file '%s' Content-type: %s\n",
					url, config->found_mime_type);
#endif

  f = open(url, O_RDONLY);
  if (f >= 0) {
	int count;

	sendHeaders(HTTP_OK);
	while ((count = bb_full_read(f, buf, MAX_MEMORY_BUFF)) > 0) {
		bb_full_write(a_c_w, buf, count);
	}
	close(f);
  } else {
#ifdef DEBUG
	if (config->debugHttpd)
		bb_perror_msg("Unable to open '%s'", url);
#endif
	sendHeaders(HTTP_NOT_FOUND);
  }

  return 0;
}

/****************************************************************************
 *
 > $Function: checkPerm()
 *
 * $Description: Check the permission file for access.
 *
 *   If config file isn't present, everything is allowed.
 *   Entries are of the form you can see example from header source
 *
 * $Parameters:
 *      (const char *) path  . . . . The file path or NULL for ip addresses.
 *      (const char *) request . . . User information to validate.
 *
 * $Return: (int)  . . . . . . . . . 1 if request OK, 0 otherwise.
 *
 ****************************************************************************/

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
static int checkPerm(const char *path, const char *request)
{
    Htaccess * cur;
    const char *p;
    const char *p0;

    int ipaddr = path == NULL;
    const char *prev = NULL;

    /* This could stand some work */
    for (cur = ipaddr ? config->ip_a_d : config->auth; cur; cur = cur->next) {
	p0 = cur->before_colon;
	if(prev != NULL && strcmp(prev, p0) != 0)
	    continue;       /* find next identical */
	p = cur->after_colon;
#ifdef DEBUG
	if (config->debugHttpd)
	    fprintf(stderr,"checkPerm: '%s' ? '%s'\n",
				(ipaddr ? (*p ? p : "*") : p0), request);
#endif
	if(ipaddr) {
	    if(strncmp(p, request, strlen(p)) != 0)
		continue;
	    return *p0 == 'A';   /* Allow/Deny */
	} else {
	    int l = strlen(p0);

	    if(strncmp(p0, path, l) == 0 &&
			    (l == 1 || path[l] == '/' || path[l] == 0)) {
		/* path match found.  Check request */
		if (strcmp(p, request) == 0)
		    return 1;   /* Ok */
		/* unauthorized, but check next /path:user:password */
		prev = p0;
	    }
	}
    }   /* for */

    return prev == NULL;
}

#else /* ifndef CONFIG_FEATURE_HTTPD_BASIC_AUTH */
static int checkPermIP(const char *request)
{
    Htaccess * cur;
    const char *p;

    /* This could stand some work */
    for (cur = config->ip_a_d; cur; cur = cur->next) {
	p = cur->after_colon;
#ifdef DEBUG
	if (config->debugHttpd)
	    fprintf(stderr, "checkPerm: '%s' ? '%s'\n",
					(*p ? p : "*"), request);
#endif
	if(strncmp(p, request, strlen(p)) == 0)
	    return *cur->before_colon == 'A';   /* Allow/Deny */
    }

    /* if uncofigured, return 1 - access from all */
    return 1;
}
#define checkPerm(null, request) checkPermIP(request)
#endif  /* CONFIG_FEATURE_HTTPD_BASIC_AUTH */


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
  char *urlArgs;
#ifdef CONFIG_FEATURE_HTTPD_CGI
  const char *prequest = request_GET;
  char *body = 0;
  long length=0;
  char *cookie = 0;
#endif
  char *test;
  struct stat sb;
  int ip_allowed;

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
  int credentials = -1;  /* if not requred this is Ok */
#endif

  do {
    int  count;

    if (getLine(buf) <= 0)
	break;  /* closed */

    purl = strpbrk(buf, " \t");
    if(purl == NULL) {
BAD_REQUEST:
      sendHeaders(HTTP_BAD_REQUEST);
      break;
    }
    *purl = 0;
#ifdef CONFIG_FEATURE_HTTPD_CGI
    if(strcasecmp(buf, prequest) != 0) {
	prequest = "POST";
	if(strcasecmp(buf, prequest) != 0) {
	    sendHeaders(HTTP_NOT_IMPLEMENTED);
	    break;
	}
    }
#else
    if(strcasecmp(buf, request_GET) != 0) {
	sendHeaders(HTTP_NOT_IMPLEMENTED);
	break;
    }
#endif
    *purl = ' ';
    count = sscanf(purl, " %[^ ] HTTP/%d.%*d", buf, &blank);

    decodeString(buf, 0);
    if (count < 1 || buf[0] != '/') {
      /* Garbled request/URL */
      goto BAD_REQUEST;
    }
    url = alloca(strlen(buf) + 12);      /* + sizeof("/index.html\0") */
    if(url == NULL) {
	sendHeaders(HTTP_INTERNAL_SERVER_ERROR);
	break;
    }
    strcpy(url, buf);
    /* extract url args if present */
    urlArgs = strchr(url, '?');
    if (urlArgs)
      *urlArgs++ = 0;

    /* algorithm stolen from libbb bb_simplify_path(),
       but don`t strdup and reducing trailing slash and protect out root */
    purl = test = url;

    do {
	if (*purl == '/') {
	    if (*test == '/') {        /* skip duplicate (or initial) slash */
		continue;
	    } else if (*test == '.') {
		if (test[1] == '/' || test[1] == 0) { /* skip extra '.' */
		    continue;
		} else if ((test[1] == '.') && (test[2] == '/' || test[2] == 0)) {
		    ++test;
		    if (purl == url) {
			/* protect out root */
			goto BAD_REQUEST;
		    }
		    while (*--purl != '/');    /* omit previous dir */
		    continue;
		}
	    }
	}
	*++purl = *test;
    } while (*++test);

    *++purl = 0;        /* so keep last character */
    test = purl;        /* end ptr */

    /* If URL is directory, adding '/' */
    if(test[-1] != '/') {
	    if ( is_directory(url + 1, 1, &sb) ) {
		    *test++ = '/';
		    *test = 0;
		    purl = test;    /* end ptr */
	    }
    }
#ifdef DEBUG
    if (config->debugHttpd)
	fprintf(stderr, "url='%s', args=%s\n", url, urlArgs);
#endif

    test = url;
    ip_allowed = checkPerm(NULL, config->rmt_ip);
    while(ip_allowed && (test = strchr( test + 1, '/' )) != NULL) {
	/* have path1/path2 */
	*test = '\0';
	if( is_directory(url + 1, 1, &sb) ) {
		/* may be having subdir config */
		parse_conf(url + 1, SUBDIR_PARSE);
		ip_allowed = checkPerm(NULL, config->rmt_ip);
	}
	*test = '/';
    }

    // read until blank line for HTTP version specified, else parse immediate
    while (blank >= 0 && (count = getLine(buf)) > 0) {

#ifdef DEBUG
      if (config->debugHttpd) fprintf(stderr, "Header: '%s'\n", buf);
#endif

#ifdef CONFIG_FEATURE_HTTPD_CGI
      /* try and do our best to parse more lines */
      if ((strncasecmp(buf, Content_length, 15) == 0)) {
	if(prequest != request_GET)
		length = strtol(buf + 15, 0, 0); // extra read only for POST
      } else if ((strncasecmp(buf, "Cookie:", 7) == 0)) {
		for(test = buf + 7; isspace(*test); test++)
			;
		cookie = strdup(test);
      }
#endif

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
      if (strncasecmp(buf, "Authorization:", 14) == 0) {
	/* We only allow Basic credentials.
	 * It shows up as "Authorization: Basic <userid:password>" where
	 * the userid:password is base64 encoded.
	 */
	for(test = buf + 14; isspace(*test); test++)
		;
	if (strncasecmp(test, "Basic", 5) != 0)
		continue;

	test += 5;  /* decodeBase64() skiping space self */
	decodeBase64(test);
	credentials = checkPerm(url, test);
      }
#endif          /* CONFIG_FEATURE_HTTPD_BASIC_AUTH */

    }   /* while extra header reading */


    if (strcmp(strrchr(url, '/') + 1, httpd_conf) == 0 || ip_allowed == 0) {
		/* protect listing [/path]/httpd_conf or IP deny */
#ifdef CONFIG_FEATURE_HTTPD_CGI
FORBIDDEN:      /* protect listing /cgi-bin */
#endif
		sendHeaders(HTTP_FORBIDDEN);
		break;
    }

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
    if (credentials <= 0 && checkPerm(url, ":") == 0) {
      sendHeaders(HTTP_UNAUTHORIZED);
      break;
    }
#endif

    test = url + 1;      /* skip first '/' */

#ifdef CONFIG_FEATURE_HTTPD_CGI
    /* if strange Content-Length */
    if (length < 0 || length > MAX_POST_SIZE)
	break;

    if (length > 0) {
      body = malloc(length + 1);
      if (body) {
	length = bb_full_read(a_c_r, body, length);
	if(length < 0)          // closed
		length = 0;
	body[length] = 0;       // always null terminate for safety
      }
    }

    if (strncmp(test, "cgi-bin", 7) == 0) {
		if(test[7] == '/' && test[8] == 0)
			goto FORBIDDEN;     // protect listing cgi-bin/
		sendCgi(url, prequest, urlArgs, body, length, cookie);
    } else {
	if (prequest != request_GET)
		sendHeaders(HTTP_NOT_IMPLEMENTED);
	else {
#endif  /* CONFIG_FEATURE_HTTPD_CGI */
		if(purl[-1] == '/')
			strcpy(purl, "index.html");
		if ( stat(test, &sb ) == 0 ) {
			config->ContentLength = sb.st_size;
			config->last_mod = sb.st_mtime;
		}
		sendFile(test, buf);
#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
		/* unset if non inetd looped */
		config->ContentLength = -1;
#endif

#ifdef CONFIG_FEATURE_HTTPD_CGI
	}
    }
#endif

  } while (0);


#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
/* from inetd don`t looping: freeing, closing automatic from exit always */
# ifdef DEBUG
  if (config->debugHttpd) fprintf(stderr, "closing socket\n");
# endif
# ifdef CONFIG_FEATURE_HTTPD_CGI
  free(body);
  free(cookie);
# endif
  shutdown(a_c_w, SHUT_WR);
  shutdown(a_c_r, SHUT_RD);
  close(config->accepted_socket);
#endif  /* CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY */
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
#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
static int miniHttpd(int server)
{
  fd_set readfd, portfd;

  FD_ZERO(&portfd);
  FD_SET(server, &portfd);

  /* copy the ports we are watching to the readfd set */
  while (1) {
    readfd = portfd;

    /* Now wait INDEFINATELY on the set of sockets! */
    if (select(server + 1, &readfd, 0, 0, 0) > 0) {
      if (FD_ISSET(server, &readfd)) {
	int on;
	struct sockaddr_in fromAddr;

	unsigned int addr;
	socklen_t fromAddrLen = sizeof(fromAddr);
	int s = accept(server,
		       (struct sockaddr *)&fromAddr, &fromAddrLen);

	if (s < 0) {
	    continue;
	}
	config->accepted_socket = s;
	addr = ntohl(fromAddr.sin_addr.s_addr);
	sprintf(config->rmt_ip, "%u.%u.%u.%u",
		(unsigned char)(addr >> 24),
		(unsigned char)(addr >> 16),
		(unsigned char)(addr >> 8),
				addr & 0xff);
	config->port = ntohs(fromAddr.sin_port);
#ifdef DEBUG
	if (config->debugHttpd) {
	    bb_error_msg("connection from IP=%s, port %u\n",
					config->rmt_ip, config->port);
	}
#endif
	/*  set the KEEPALIVE option to cull dead connections */
	on = 1;
	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (void *)&on, sizeof (on));

	if (config->debugHttpd || fork() == 0) {
	    /* This is the spawned thread */
#ifdef CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
	    /* protect reload config, may be confuse checking */
	    signal(SIGHUP, SIG_IGN);
#endif
	    handleIncoming();
	    if(!config->debugHttpd)
		exit(0);
	}
	close(s);
      }
    }
  } // while (1)
  return 0;
}

#else
    /* from inetd */

static int miniHttpd(void)
{
  struct sockaddr_in fromAddrLen;
  socklen_t sinlen = sizeof (struct sockaddr_in);
  unsigned int addr;

  getpeername (0, (struct sockaddr *)&fromAddrLen, &sinlen);
  addr = ntohl(fromAddrLen.sin_addr.s_addr);
  sprintf(config->rmt_ip, "%u.%u.%u.%u",
		(unsigned char)(addr >> 24),
		(unsigned char)(addr >> 16),
		(unsigned char)(addr >> 8),
				addr & 0xff);
  config->port = ntohs(fromAddrLen.sin_port);
  handleIncoming();
  return 0;
}
#endif  /* CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY */

#ifdef CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
static void sighup_handler(int sig)
{
	/* set and reset */
	struct sigaction sa;

	sa.sa_handler = sighup_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGHUP, &sa, NULL);
	parse_conf(default_path_httpd_conf,
		    sig == SIGHUP ? SIGNALED_PARSE : FIRST_PARSE);
}
#endif

#ifdef HTTPD_STANDALONE
int main(int argc, char *argv[])
#else
int httpd_main(int argc, char *argv[])
#endif
{
  const char *home_httpd = home;

#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
  int server;
#endif

#ifdef CONFIG_FEATURE_HTTPD_SETUID
  long uid = -1;
#endif

  config = xcalloc(1, sizeof(*config));
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
  config->realm = "Web Server Authentication";
#endif

#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
  config->port = 80;
#endif

  config->ContentLength = -1;

  /* check if user supplied a port number */
  for (;;) {
    int c = getopt( argc, argv, "c:d:h:"
#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
				"p:v"
#endif
#ifdef CONFIG_FEATURE_HTTPD_ENCODE_URL_STR
		"e:"
#endif
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
		"r:"
#endif
#ifdef CONFIG_FEATURE_HTTPD_SETUID
		"u:"
#endif
    );
    if (c == EOF) break;
    switch (c) {
    case 'c':
      config->configFile = optarg;
      break;
    case 'h':
      home_httpd = optarg;
      break;
#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
    case 'v':
      config->debugHttpd = 1;
      break;
    case 'p':
      config->port = atoi(optarg);
      if(config->port <= 0 || config->port > 0xffff)
	bb_error_msg_and_die("invalid %s for -p", optarg);
      break;
#endif
    case 'd':
      printf("%s", decodeString(optarg, 1));
      return 0;
#ifdef CONFIG_FEATURE_HTTPD_ENCODE_URL_STR
    case 'e':
      printf("%s", encodeString(optarg));
      return 0;
#endif
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
    case 'r':
      config->realm = optarg;
      break;
#endif
#ifdef CONFIG_FEATURE_HTTPD_SETUID
    case 'u':
      {
	char *e;

	uid = strtol(optarg, &e, 0);
	if(*e != '\0') {
		/* not integer */
		uid = my_getpwnam(optarg);
	}
      }
      break;
#endif
    default:
      bb_error_msg("%s", httpdVersion);
      bb_show_usage();
    }
  }

  if(chdir(home_httpd)) {
    bb_perror_msg_and_die("can`t chdir to %s", home_httpd);
  }
#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
  server = openServer();
# ifdef CONFIG_FEATURE_HTTPD_SETUID
  /* drop privilegies */
  if(uid > 0)
	setuid(uid);
# endif
# ifdef CONFIG_FEATURE_HTTPD_CGI
  addEnvPort("SERVER");
# endif
#endif

#ifdef CONFIG_FEATURE_HTTPD_RELOAD_CONFIG_SIGHUP
  sighup_handler(0);
#else
  parse_conf(default_path_httpd_conf, FIRST_PARSE);
#endif

#ifndef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
  if (!config->debugHttpd) {
    if (daemon(1, 0) < 0)     /* don`t change curent directory */
	bb_perror_msg_and_die("daemon");
  }
  return miniHttpd(server);
#else
  return miniHttpd();
#endif
}
