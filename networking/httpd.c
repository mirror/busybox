/*
 * httpd implementation for busybox
 *
 * Copyright (C) 2002 Glenn Engel <glenne@engel.org>
 *
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
 *   cd /var/www
 *   httpd 
 * This is equivalent to
 *    cd /var/www
 *    httpd -p 80 -c /etc/httpd.conf -r "Web Server Authentication"
 *
 * When a url contains "cgi-bin" it is assumed to be a cgi script.  The
 * server changes directory to the location of the script and executes it
 * after setting QUERY_STRING and other environment variables.  If url args
 * are included in the url or as a post, the args are placed into decoded
 * environment variables.  e.g. /cgi-bin/setup?foo=Hello%20World  will set
 * the $CGI_foo environment variable to "Hello World".
 *
 * The server can also be invoked as a url arg decoder and html text encoder
 * as follows:
 *  foo=`httpd -d $foo`           # decode "Hello%20World" as "Hello World"
 *  bar=`httpd -e "<Hello World>"`  # encode as "&#60Hello&#32World&#62"
 *
 * httpd.conf has the following format:
 
ip:10.10.         # Allow any address that begins with 10.10.
ip:172.20.        # Allow 172.20.x.x
ip:127.0.0.1      # Allow local loopback connections
/cgi-bin:foo:bar  # Require user foo, pwd bar on urls starting with /cgi-bin
/:admin:setup     # Require user admin, pwd setup on urls starting with /

 *
 * To open up the server: 
 * ip:*              # Allow any IP address
 * /:*               # no password required for urls starting with / (all)
 *
 * Processing of the file stops on the first sucessful match.  If the file
 * is not found, the server is assumed to be wide open.
 *
 *****************************************************************************
 *
 * Desired enhancements:
 *   cache httpd.conf
 *   support tinylogin
 *
 */
#include <stdio.h>
#include <ctype.h>         /* for isspace           */
#include <stdarg.h>        /* for varargs           */
#include <string.h>        /* for strerror          */
#include <stdlib.h>        /* for malloc            */
#include <time.h>
#include <errno.h>
#include <unistd.h>        /* for close             */
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>    /* for connect and socket*/
#include <netinet/in.h>    /* for sockaddr_in       */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

static const char httpdVersion[] = "busybox httpd/1.13 3-Jan-2003";

// #define DEBUG 1 
#ifndef HTTPD_STANDALONE
#include <config.h>
#include <busybox.h>
// Note: xfuncs are not used because we want the server to keep running
//       if something bad happens due to a malformed user request.
//       As a result, all memory allocation is checked rigorously
#else
/* standalone */
#define CONFIG_FEATURE_HTTPD_BASIC_AUTH
void show_usage()
{
  fprintf(stderr,"Usage: httpd [-p <port>] [-c configFile] [-d/-e <string>] [-r realm]\n");
}
#endif

/* minimal global vars for busybox */
#ifndef ENVSIZE
#define ENVSIZE 50
#endif
int debugHttpd;
static char **envp;
static int envCount;
static char *realm = "Web Server Authentication";
static char *configFile;

static const char* const suffixTable [] = {
  ".htm.html", "text/html",
  ".jpg.jpeg", "image/jpeg",
  ".gif", "image/gif",
  ".png", "image/png",
  ".txt.h.c.cc.cpp", "text/plain",
  0,0
  };

typedef enum
{
  HTTP_OK = 200,
  HTTP_UNAUTHORIZED = 401, /* authentication needed, respond with auth hdr */
  HTTP_NOT_FOUND = 404,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED = 501,  /* used for unrecognized requests */
  HTTP_BAD_REQUEST = 400,  /* malformed syntax */
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
  HTTP_FORBIDDEN = 403,
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
  { HTTP_UNAUTHORIZED, "Unauthorized", "" },
  { HTTP_NOT_FOUND, "Not Found",
    "The requested URL was not found on this server." },
  { HTTP_INTERNAL_SERVER_ERROR, "Internal Server Error"
    "Internal Server Error" },
  { HTTP_BAD_REQUEST, "Bad Request" ,
    "Unsupported method.\n" },
#if 0
  { HTTP_CREATED, "Created" },
  { HTTP_ACCEPTED, "Accepted" },
  { HTTP_NO_CONTENT, "No Content" },
  { HTTP_MULTIPLE_CHOICES, "Multiple Choices" },
  { HTTP_MOVED_PERMANENTLY, "Moved Permanently" },
  { HTTP_MOVED_TEMPORARILY, "Moved Temporarily" },
  { HTTP_NOT_MODIFIED, "Not Modified" },
  { HTTP_FORBIDDEN, "Forbidden", "" },
  { HTTP_BAD_GATEWAY, "Bad Gateway", "" },
  { HTTP_SERVICE_UNAVAILABLE, "Service Unavailable", "" },
#endif
};

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
  char *out = (char*)malloc(len*5 +1); 
  char *p=out;
  char ch;
  if (!out) return "";
  while ((ch = *string++))
  {
    // very simple check for what to encode
    if (isalnum(ch)) *p++ = ch;
    else p += sprintf(p,"&#%d", (unsigned char) ch);
  }
  *p=0;
  return out;
}

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
 *
 * $Return: (char *)  . . . . A pointer to the decoded string (same as input).
 *
 * $Errors: None
 *
 ****************************************************************************/
static char *decodeString(char *string)
{
  /* note that decoded string is always shorter than original */
  char *orig = string;
  char *ptr = string;
  while (*ptr)
  {
    if (*ptr == '+') { *string++ = ' '; ptr++; }
    else if (*ptr != '%') *string++ = *ptr++;
    else
    {
      unsigned int value;
      sscanf(ptr+1,"%2X",&value);
      *string++ = value;
      ptr += 3;
    }
  }
  *string = '\0';
  return orig;
}


/****************************************************************************
 *
 > $Function: addEnv()
 *
 * $Description: Add an enviornment variable setting to the global list.
 *    A NAME=VALUE string is allocated, filled, and added to the list of
 *    environment settings passed to the cgi execution script.
 *
 * $Parameters:
 *      (char *) name . . . The environment variable name.
 *      (char *) value  . . The value to which the env variable is set.
 *
 * $Return: (void)  
 *
 * $Errors: Silently returns if the env runs out of space to hold the new item
 *
 ****************************************************************************/
static void addEnv(const char *name, const char *value)
{
  char *s;
  if (envCount >= ENVSIZE) return;
  if (!value) value = "";
  s=(char*)malloc(strlen(name)+strlen(value)+2);
  if (s)
  {
    sprintf(s,"%s=%s",name, value);
    envp[envCount++]=s;
    envp[envCount]=0;
  }
}

/****************************************************************************
 *
 > $Function: addEnvCgi
 *
 * $Description: Create environment variables given a URL encoded arg list.
 *   For each variable setting the URL encoded arg list, create a corresponding
 *   environment variable.  URL encoded arguments have the form
 *      name1=value1&name2=value2&name3=value3
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
  if (pargs==0) return;
  
  /* args are a list of name=value&name2=value2 sequences */
  args = strdup(pargs);
  while (args && *args)
  {
    char *sep;
    char *name=args;
    char *value=strchr(args,'=');
    char *cginame;
    if (!value) break;
    *value++=0;
    sep=strchr(value,'&');
    if (sep)
    {
      *sep=0;
      args=sep+1;
    }
    else
    {
      sep = value + strlen(value);
      args = 0; /* no more */
    }
    cginame=(char*)malloc(strlen(decodeString(name))+5);
    if (!cginame) break;
    sprintf(cginame,"CGI_%s",name);
    addEnv(cginame,decodeString(value));
    free(cginame);
  }
}

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
static const unsigned char base64ToBin[] = {
    255 /* ' ' */,  255 /* '!' */,   255 /* '"' */,   255 /* '#' */,
     1 /* '$' */,  255 /* '%' */,    255 /* '&' */,   255 /* ''' */,
    255 /* '(' */,  255 /* ')' */,   255 /* '*' */,    62 /* '+' */,
    255 /* ',' */,  255 /* '-' */,   255 /* '.' */,    63 /* '/' */,
    52 /* '0' */,    53 /* '1' */,    54 /* '2' */,    55 /* '3' */,
    56 /* '4' */,    57 /* '5' */,    58 /* '6' */,    59 /* '7' */,
    60 /* '8' */,    61 /* '9' */,   255 /* ':' */,   255 /* ';' */,
    255 /* '<' */,   00 /* '=' */,   255 /* '>' */,   255 /* '?' */,
    255 /* '@' */,   00 /* 'A' */,    01 /* 'B' */,    02 /* 'C' */,
    03 /* 'D' */,    04 /* 'E' */,    05 /* 'F' */,    06 /* 'G' */,
     7 /* 'H' */,     8 /* 'I' */,     9 /* 'J' */,    10 /* 'K' */,
    11 /* 'L' */,    12 /* 'M' */,    13 /* 'N' */,    14 /* 'O' */,
    15 /* 'P' */,    16 /* 'Q' */,    17 /* 'R' */,    18 /* 'S' */,
    19 /* 'T' */,    20 /* 'U' */,    21 /* 'V' */,    22 /* 'W' */,
    23 /* 'X' */,    24 /* 'Y' */,    25 /* 'Z' */,   255 /* '[' */,
    255 /* '\' */,  255 /* ']' */,   255 /* '^' */,   255 /* '_' */,
    255 /* '`' */,   26 /* 'a' */,    27 /* 'b' */,    28 /* 'c' */,
    29 /* 'd' */,    30 /* 'e' */,    31 /* 'f' */,    32 /* 'g' */,
    33 /* 'h' */,    34 /* 'i' */,    35 /* 'j' */,    36 /* 'k' */,
    37 /* 'l' */,    38 /* 'm' */,    39 /* 'n' */,    40 /* 'o' */,
    41 /* 'p' */,    42 /* 'q' */,    43 /* 'r' */,    44 /* 's' */,
    45 /* 't' */,    46 /* 'u' */,    47 /* 'v' */,    48 /* 'w' */,
    49 /* 'x' */,    50 /* 'y' */,    51 /* 'z' */,   255 /* '{' */,
    255 /* '|' */,  255 /* '}' */,   255 /* '~' */,   255 /* '' */
};

/****************************************************************************
 *
 > $Function: decodeBase64()
 *
 > $Description: Decode a base 64 data stream as per rfc1521.
 *    Note that the rfc states that none base64 chars are to be ignored.
 *    Since the decode always results in a shorter size than the input, it is
 *    OK to pass the input arg as an output arg.
 *
 * $Parameters:
 *      (void *) outData. . . Where to place the decoded data.
 *      (size_t) outDataLen . The length of the output data string.
 *      (void *) inData . . . A pointer to a base64 encoded string.
 *      (size_t) inDataLen  . The length of the input data string.
 *
 * $Return: (char *)  . . . . A pointer to the decoded string (same as input).
 *
 * $Errors: None
 *
 ****************************************************************************/
static size_t decodeBase64(void *outData, size_t outDataLen,
			   void *inData, size_t inDataLen)
{
  int i = 0;
  unsigned char *in = inData;
  unsigned char *out = outData;
  unsigned long ch = 0;
  while (inDataLen && outDataLen)
  {
    unsigned char conv = 0;
    unsigned char newch;
    
    while (inDataLen)
    {
      inDataLen--;
      newch = *in++;
      if ((newch < '0') || (newch > 'z')) continue;
      conv = base64ToBin[newch - 32];
      if (conv == 255) continue;
      break;
    }
    ch = (ch << 6) | conv;
    i++;
    if (i== 4)
    {
      if (outDataLen >= 3)
      {
	*(out++) = (unsigned char) (ch >> 16);
	*(out++) = (unsigned char) (ch >> 8);
	*(out++) = (unsigned char) ch;
	outDataLen-=3;
      }
      
      i = 0;
    }
    
    if ((inDataLen == 0) && (i != 0))
    {
      /* error - non multiple of 4 chars on input */
      break;
    }

  }

  /* return the actual number of chars in output array */
  return out-(unsigned char*) outData;
}
#endif

/****************************************************************************
 *
 > $Function: perror_and_exit()
 *
 > $Description: A helper function to print an error and exit.
 *
 * $Parameters:
 *      (const char *) msg . . . A 'context' message to include.
 *
 * $Return: None
 *
 * $Errors: None
 *
 ****************************************************************************/
static void perror_exit(const char *msg)
{
  perror(msg);
  exit(1);
}


/****************************************************************************
 *
 > $Function: strncmpi()
 *
 * $Description: compare two strings without regard to case.
 *
 * $Parameters:
 *      (char *) a . . . . . The first string.
 *      (char *) b . . . . . The second string.
 *      (int) n  . . . . . . The number of chars to compare.
 *
 * $Return: (int) . . . . . . 0 if strings equal. 1 if a>b, -1 if b < a.
 *
 * $Errors: None
 *
 ****************************************************************************/
#define __toupper(c)   ((('a' <= (c))&&((c) <= 'z')) ? ((c) - 'a' + 'A') : (c))
#define __tolower(c)   ((('A' <= (c))&&((c) <= 'Z')) ? ((c) - 'A' + 'a') : (c))
static int strncmpi(const char *a, const char *b,int n)
{
  char a1,b1;
  a1 = b1 = 0;

  while(n-- && ((a1 = *a++) != '\0') && ((b1 = *b++) != '\0'))
  {
    if(a1 == b1) continue;       /* No need to convert */
    a1 = __tolower(a1);
    b1 = __tolower(b1);
    if(a1 != b1) break;          /* No match, abort */
  }
  if (n>=0) 
  {
    if(a1 > b1) return 1;
    if(a1 < b1) return -1;
  }
  return 0;
}

/****************************************************************************
 *
 > $Function: openServer()
 *
 * $Description: create a listen server socket on the designated port.
 *
 * $Parameters:
 *      (int) port . . . The port to listen on for connections.
 *
 * $Return: (int)  . . . A connection socket. -1 for errors.
 *
 * $Errors: None
 *
 ****************************************************************************/
static int openServer(int port)
{
  struct sockaddr_in lsocket;
  int fd;
  
  /* create the socket right now */
  /* inet_addr() returns a value that is already in network order */
  memset(&lsocket, 0, sizeof(lsocket));
  lsocket.sin_family = AF_INET;
  lsocket.sin_addr.s_addr = INADDR_ANY;
  lsocket.sin_port = htons(port) ;
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd >= 0)
  {
    /* tell the OS it's OK to reuse a previous address even though */
    /* it may still be in a close down state.  Allows bind to succeed. */
    int one = 1;
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one)) ;
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) ;
#endif
    if (bind(fd, (struct sockaddr *)&lsocket, sizeof(lsocket)) == 0)
    {
      listen(fd, 9);
      signal(SIGCHLD, SIG_IGN);   /* prevent zombie (defunct) processes */
    }
    else
    {
      perror("failure to bind to server port");
      shutdown(fd,0);
      close(fd);
      fd = -1;
      }
    }
    else
    {
      fprintf(stderr,"httpd: unable to create socket \n");
    }
  return fd;
}

static int sendBuf(int s, char *buf, int len)
{
  if (len == -1) len = strlen(buf);
  return send(s, buf, len, 0);
}

/****************************************************************************
 *
 > $Function: sendHeaders()
 *
 * $Description: Create and send HTTP response headers.
 *   The arguments are combined and sent as one write operation.  Note that
 *   IE will puke big-time if the headers are not sent in one packet and the
 *   second packet is delayed for any reason.  If contentType is null the
 *   content type is assumed to be text/html
 *
 * $Parameters:
 *      (int) s . . . The http socket.
 *      (HttpResponseNum) responseNum . . . The result code to send.
 *      (const char *) contentType  . . . . A string indicating the type.
 *      (int) contentLength . . . . . . . . Content length.  -1 if unknown.
 *      (time_t) expire . . . . . . . . . . Expiration time (secs since 1970)
 *
 * $Return: (int)  . . . . Always 0
 *
 * $Errors: None
 *
 ****************************************************************************/
static int sendHeaders(int s, HttpResponseNum responseNum ,
		       const char *contentType,
		       int contentLength, time_t expire)
{
  char buf[1200];
  const char *responseString = "";
  const char *infoString = 0;
  unsigned int i;
  time_t timer = time(0);
  char timeStr[80];
  for (i=0;
       i < (sizeof(httpResponseNames)/sizeof(httpResponseNames[0])); i++)
  {
    if (httpResponseNames[i].type != responseNum) continue;
    responseString = httpResponseNames[i].name;
    infoString = httpResponseNames[i].info;
    break;
  }
  if (infoString || !contentType)
  {
    contentType = "text/html";
  }
  
  sprintf(buf, "HTTP/1.0 %d %s\nContent-type: %s\r\n",
	  responseNum, responseString, contentType);

  /* emit the current date */
  strftime(timeStr, sizeof(timeStr),
           "%a, %d %b %Y %H:%M:%S GMT",gmtime(&timer));
  sprintf(buf+strlen(buf), "Date: %s\r\n", timeStr);
  sprintf(buf+strlen(buf), "Connection: close\r\n");
  if (expire)
  {
    strftime(timeStr, sizeof(timeStr),
             "%a, %d %b %Y %H:%M:%S GMT",gmtime(&expire));
    sprintf(buf+strlen(buf), "Expire: %s\r\n", timeStr);
  }

  if (responseNum == HTTP_UNAUTHORIZED)
  {
    sprintf(buf+strlen(buf),
	    "WWW-Authenticate: Basic realm=\"%s\"\r\n", realm);
  }
  if (contentLength != -1)
  {
    int len = strlen(buf);
    sprintf(buf+len,"Content-length: %d\r\n", contentLength);
  }
  strcat(buf,"\r\n");
  if (infoString)
  {
    sprintf(buf+strlen(buf),
	    "<HEAD><TITLE>%d %s</TITLE></HEAD>\n"
	    "<BODY><H1>%d %s</H1>\n%s\n</BODY>\n",
	    responseNum, responseString,
	    responseNum, responseString,
	    infoString);
  }
#ifdef DEBUG
  if (debugHttpd) fprintf(stderr,"Headers:'%s'", buf);
#endif
  sendBuf(s, buf,-1);
  return 0;
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
 *      (int) s . . . . . The socket fildes.
 *      (char *) buf  . . Where to place the read result.
 *      (int) maxBuf  . . Maximum number of chars to fit in buf.
 *
 * $Return: (int) . . . . number of characters read.  -1 if error.
 *
 ****************************************************************************/
static int getLine(int s, char *buf, int maxBuf)
{
  int  count = 0;
  while (recv(s, buf+count, 1, 0) == 1)
  {
    if (buf[count] == '\r') continue;
    if (buf[count] == '\n')
    {
      buf[count] = 0;
      return count;
    }
    count++;
  }
  if (count) return count;
  else return -1;
}

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
 *      (int ) s . . . . . . . . The session socket.
 *      (const char *) url . . . The requested URL (with leading /).
 *      (const char *urlArgs). . Any URL arguments.
 *      (const char *body) . . . POST body contents.
 *      (int bodyLen)  . . . . . Length of the post body.
 
 *
 * $Return: (char *)  . . . . A pointer to the decoded string (same as input).
 *
 * $Errors: None
 *
 ****************************************************************************/
static int sendCgi(int s, const char *url, 
                   const char *request, const char *urlArgs,
		   const char *body, int bodyLen)
{
  int fromCgi[2];  /* pipe for reading data from CGI */
  int toCgi[2];    /* pipe for sending data to CGI */
  
  char *argp[] = { 0, 0 };
  int pid=0;
  int inFd=inFd;
  int outFd;
  int firstLine=1;

  do
  {
    if (pipe(fromCgi) != 0)
    {
      break;
    }
    if (pipe(toCgi) != 0)
    {
      break;
    }

    pid = fork();
    if (pid < 0)
    {
	pid = 0;
	break;;
    }
    
    if (!pid)   
    {
      /* child process */
      char *script;
      char *directory;
      inFd=toCgi[0];
      outFd=fromCgi[1];

      dup2(inFd, 0);  // replace stdin with the pipe
      dup2(outFd, 1);  // replace stdout with the pipe
      if (!debugHttpd) dup2(outFd, 2);  // replace stderr with the pipe
      close(toCgi[0]);
      close(toCgi[1]);
      close(fromCgi[0]);
      close(fromCgi[1]);

#if 0
      fcntl(0,F_SETFD, 1);
      fcntl(1,F_SETFD, 1);
      fcntl(2,F_SETFD, 1);
#endif
      
      script = (char*) malloc(strlen(url)+2);
      if (!script) _exit(242);
      sprintf(script,".%s",url);
	
      envCount=0;
      addEnv("SCRIPT_NAME",script);
      addEnv("REQUEST_METHOD",request);
      addEnv("QUERY_STRING",urlArgs);
      addEnv("SERVER_SOFTWARE",httpdVersion);
      if (strncmpi(request,"POST",4)==0) addEnvCgi(body);
      else addEnvCgi(urlArgs);
      
      /*
       * Most HTTP servers chdir to the cgi directory.
       */
      while (*url == '/') url++;  // skip leading slash(s)
      directory = strdup( url );  
      if ( directory == (char*) 0 )
	script = (char*) (url);	/* ignore errors */
      else
      {
	script = strrchr( directory, '/' );
	if ( script == (char*) 0 )
	  script = directory;
	else
	{
	  *script++ = '\0';
	  (void) chdir( directory );	/* ignore errors */
	}
      }
      // now run the program.  If it fails, use _exit() so no destructors
      // get called and make a mess.
      execve(script, argp, envp);
      
#ifdef DEBUG
      fprintf(stderr, "exec failed\n");
#endif
      close(2);
      close(1);
      close(0);
      _exit(242);
    } /* end child */

    /* parent process */
    inFd=fromCgi[0];
    outFd=toCgi[1];
    close(fromCgi[1]);
    close(toCgi[0]);
    if (body) write(outFd, body, bodyLen);
    close(outFd);

  } while (0);

  if (pid)
  {
    int status;
    pid_t dead_pid;
    
    while (1)
    {
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
      nfound = select(inFd+1, &readSet, 0, 0, &timeout);

      if (nfound <= 0)
      {
	dead_pid = waitpid(pid, &status, WNOHANG);
	if (dead_pid != 0)
	{
	  close(fromCgi[0]);
	  close(fromCgi[1]);
	  close(toCgi[0]);
	  close(toCgi[1]);
#ifdef DEBUG
	  if (debugHttpd)
	  {
	    if (WIFEXITED(status))
	      fprintf(stderr,"piped has exited with status=%d\n", WEXITSTATUS(status));
	    if (WIFSIGNALED(status))
	      fprintf(stderr,"piped has exited with signal=%d\n", WTERMSIG(status));
	  }
#endif  
	  pid = -1;
	  break;
	}
      }
      else
      {
	// There is something to read
	count = read(inFd,buf,sizeof(buf)-1);
	// If a read returns 0 at this point then some type of error has
	// occurred.  Bail now.
	if (count == 0) break;
	if (count > 0)
	{
	  if (firstLine)
	  {
	    /* check to see if the user script added headers */
	    if (strcmp(buf,"HTTP")!= 0)
	    {
	      write(s,"HTTP/1.0 200 OK\n", 16);
	    }
	    if (strstr(buf,"ontent-") == 0)
	    {
	      write(s,"Content-type: text/plain\n\n", 26);
	    }
	    
	    firstLine=0;
	  }
	  write(s,buf,count);
#ifdef DEBUG
	  if (debugHttpd) fprintf(stderr,"cgi read %d bytes\n", count);
#endif
	}
      }
    }
  }
  return 0;
}

/****************************************************************************
 *
 > $Function: sendFile()
 *
 * $Description: Send a file response to an HTTP request
 *
 * $Parameters:
 *      (int) s  . . . . . . . The http session socket.
 *      (const char *) url . . The URL requested.
 *
 * $Return: (int)  . . . . . . Always 0.
 *
 ****************************************************************************/
static int sendFile(int s, const char *url)
{
  char *suffix = strrchr(url,'.');
  const char *content = "application/octet-stream";
  int  f;

  if (suffix)
  {
    const char ** table;
    for (table = (const char **) &suffixTable[0]; 
         *table && (strstr(*table, suffix) == 0); table+=2);
    if (table) content = *(table+1);
  }
  
  if (*url == '/') url++;
  suffix = strchr(url,'?');
  if (suffix) *suffix = 0;

#ifdef DEBUG
    fprintf(stderr,"Sending file '%s'\n", url);
#endif
  
  f = open(url,O_RDONLY, 0444);
  if (f >= 0)
  {
    char buf[1450];
    int count;
    sendHeaders(s, HTTP_OK, content, -1, 0 );
    while ((count = read(f, buf, sizeof(buf))))
    {
      sendBuf(s, buf, count);
    }
    close(f);
  }
  else
  {
#ifdef DEBUG
    fprintf(stderr,"Unable to open '%s'\n", url);
#endif
    sendHeaders(s, HTTP_NOT_FOUND, "text/html", -1, 0);
  }
  
  return 0;
}

/****************************************************************************
 *
 > $Function: checkPerm()
 *
 * $Description: Check the permission file for access.
 *
 *   Both IP addresses as well as url pathnames can be specified.  If an IP
 *   address check is desired, the 'path' should be specified as "ip" and the
 *   dotted decimal IP address placed in request.
 *
 *   For url pathnames, place the url (with leading /) in 'path' and any
 *   authentication information in request.  e.g. "user:pass"
 *
 *******
 *
 *   Keep the algorithm simple.
 *   If config file isn't present, everything is allowed.
 *   Run down /etc/httpd.hosts a line at a time.
 *   Stop if match is found.
 *   Entries are of the form:
 *   ip:10.10    # any address that begins with 10.10
 *   dir:user:pass  # dir security for dirs that start with 'dir'
 *
 * httpd.conf has the following format:
 *   ip:10.10.         # Allow any address that begins with 10.10.
 *   ip:172.20.        # Allow 172.20.x.x
 *   /cgi-bin:foo:bar  # Require user foo, pwd bar on urls starting with /cgi-bin
 *   /:foo:bar         # Require user foo, pwd bar on urls starting with /
 *
 * To open up the server: 
 *   ip:*              # Allow any IP address
 *   /:*               # no password required for urls starting with / (all)
 *
 * $Parameters:
 *      (const char *) path  . . . . The file path or "ip" for ip addresses.
 *      (const char *) request . . . User information to validate.
 *
 * $Return: (int)  . . . . . . . . . 1 if request OK, 0 otherwise.
 *
 ****************************************************************************/
static int checkPerm(const char *path, const char *request)
{
    FILE *f=NULL;
    int rval;
    char buf[80];
    char *p;
    int ipaddr=0;

    /* If httpd.conf not there assume anyone can get in */
    if (configFile) f = fopen(configFile,"r");
    if(f == NULL) f = fopen("/etc/httpd.conf","r");
    if(f == NULL) f = fopen("httpd.conf","r");
    if(f == NULL) {
        return(1);
    }
    if (strcmp("ip",path) == 0) ipaddr=1;

    rval=0;

    /* This could stand some work */
    while ( fgets(buf, 80, f) != NULL)
    {
        if(buf[0] == '#') continue;
        if(buf[0] == '\0') continue;
        for(p = buf + (strlen(buf) - 1); p >= buf; p--)
	{
            if(isspace(*p)) *p = 0;
        }
	
        p = strchr(buf,':');
	if (!p) continue;
	*p++=0;
#ifdef DEBUG
        fprintf(stderr,"checkPerm: '%s' ? '%s'\n",buf,path);
#endif
	if((ipaddr ? strcmp(buf,path) : strncmp(buf, path, strlen(buf))) == 0)
	{
	  /* match found.  Check request */
	  if ((strcmp("*",p) == 0) ||
	      (strcmp(p, request) == 0) ||
	      (ipaddr && (strncmp(p, request, strlen(p)) == 0)))
	  {
	    rval = 1;
	    break;
	  }

	  /* reject on first failure for non ipaddresses */
	  if (!ipaddr) break;
	}
    };
    fclose(f);
    return(rval);
};


/****************************************************************************
 *
 > $Function: handleIncoming()
 *
 * $Description: Handle an incoming http request.
 *
 * $Parameters:
 *      (s) s . . . . . The http request socket.
 *
 * $Return: (int) . . . Always 0.
 *
 ****************************************************************************/
static int handleIncoming(int s)
{
  char buf[8192];
  char url[8192];  /* hold args too initially */
  char credentials[80];
  char request[20];
  long length=0;
  int  major;
  int  minor;
  char *urlArgs;
  char *body=0;

  credentials[0] = 0;
  do
  {
    int  count = getLine(s, buf, sizeof(buf));
    int blank;
    if (count <= 0)  break;
    count = sscanf(buf, "%9s %1000s HTTP/%d.%d", request,
		   url, &major, &minor);
    
    if (count < 2) 
    {
      /* Garbled request/URL */
#if 0
      genHttpHeader(&requestInfo,
		    HTTP_BAD_REQUEST, requestInfo.dataType,
		    HTTP_LENGTH_UNKNOWN);
#endif
      break;
    }
    
    /* If no version info, assume 0.9 */
    if (count != 4)
    {
      major = 0;
      minor = 9;
    }

    /* extract url args if present */
    urlArgs = strchr(url,'?');
    if (urlArgs)
    {
      *urlArgs=0;
      urlArgs++;
    }
    
#ifdef DEBUG
    if (debugHttpd) fprintf(stderr,"url='%s', args=%s\n", url, urlArgs);
#endif  

    // read until blank line(s)
    blank = 0;
    while ((count = getLine(s, buf, sizeof(buf))) >= 0)
    {
      if (count == 0)
      {
	if (major > 0) break;
	blank++;
	if (blank == 2) break;
      }
#ifdef DEBUG
      if (debugHttpd) fprintf(stderr,"Header: '%s'\n", buf);
#endif  

      /* try and do our best to parse more lines */
      if ((strncmpi(buf, "Content-length:", 15) == 0))
      {
	sscanf(buf, "%*s %ld", &length);
      }    
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
      else if (strncmpi(buf, "Authorization:", 14) == 0)
      {
	/* We only allow Basic credentials.
	 * It shows up as "Authorization: Basic <userid:password>" where
	 * the userid:password is base64 encoded.
	 */
	char *ptr = buf+14;
	while (*ptr == ' ') ptr++;
	if (strncmpi(ptr, "Basic", 5) != 0) break;
	ptr += 5;
	while (*ptr == ' ') ptr++;
	memset(credentials, 0, sizeof(credentials));
	decodeBase64(credentials,
		     sizeof(credentials)-1,
		     ptr,
		     strlen(ptr) );
    
      }
    }
    if (!checkPerm(url, credentials))
    {
      sendHeaders(s, HTTP_UNAUTHORIZED, 0, -1, 0);
      length=-1;
      break; /* no more processing */
    }
#else
    }
#endif /* CONFIG_FEATURE_HTTPD_BASIC_AUTH */

    /* we are done if an error occurred */
    if (length == -1) break;

    if (strcmp(url,"/") == 0) strcpy(url,"/index.html");

    if (length>0)
    {
      body=(char*) malloc(length+1);
      if (body)
      {
	length = read(s,body,length);
	body[length]=0; // always null terminate for safety
	urlArgs=body;
      }
    }
    
    if (strstr(url,"..") || strstr(url, "httpd.conf")) 
    {
      /* protect from .. path creep */
      sendHeaders(s, HTTP_NOT_FOUND, "text/html", -1, 0);
    }
    else if (strstr(url,"cgi-bin")) 
    { 
      sendCgi(s, url, request, urlArgs, body, length);
    }
    else if (strncmpi(request,"GET",3) == 0)
    {
      sendFile(s, url);
    }
    else
    {
      sendHeaders(s, HTTP_NOT_IMPLEMENTED, 0, -1, 0);
    }
  } while (0);

#ifdef DEBUG
  if (debugHttpd) fprintf(stderr,"closing socket\n");
#endif  
  if (body) free(body);
  shutdown(s,SHUT_WR);
  shutdown(s,SHUT_RD);
  close(s);
  
  return 0;
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
  int nfound;
  
  FD_ZERO(&portfd);
  FD_SET(server, &portfd);
  
  /* copy the ports we are watching to the readfd set */
  while (1) 
  {
    readfd = portfd ;
    
    /* Now wait INDEFINATELY on the set of sockets! */
    nfound = select(server+1, &readfd, 0, 0, 0);
    
    switch (nfound)
    {
    case 0:
      /* select timeout error! */
      break ;
    case -1:
      /* select error */
      break;
    default:
      if (FD_ISSET(server, &readfd))
      {
	char on;
	struct sockaddr_in fromAddr;
	char rmt_ip[20];
	int addr;
	socklen_t fromAddrLen = sizeof(fromAddr);
	int s = accept(server,
		       (struct sockaddr *)&fromAddr, &fromAddrLen) ;
	if (s < 0) 
	{
	  continue;
	}
	addr = ntohl(fromAddr.sin_addr.s_addr);
	sprintf(rmt_ip,"%u.%u.%u.%u",
                (unsigned char)(addr >> 24),
                (unsigned char)(addr >> 16),
                (unsigned char)(addr >> 8),
                (unsigned char)(addr >> 0));
#ifdef DEBUG
	if (debugHttpd)
	{
	  fprintf(stderr,"httpMini.cpp: connection from IP=%s, port %d\n",
		 rmt_ip, ntohs(fromAddr.sin_port));
	}
#endif  
	if(checkPerm("ip", rmt_ip) == 0)
	{
	  close(s);
	  continue;
	}
	
	/*  set the KEEPALIVE option to cull dead connections */
	on = 1;
	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *) &on,
		   sizeof (on));

	if (fork() == 0) 
	{
	  /* This is the spawned thread */
	  handleIncoming(s);
	  exit(0);
	}
	close(s);
      }
    }
  } // while (1)
  return 0;
}

int httpd_main(int argc, char *argv[])
{
  int server;
  int port = 80;
  int c;
  
  /* check if user supplied a port number */
  for (;;) {
    c = getopt( argc, argv, "p:ve:d:"
#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
    "r:c:"
#endif
    );
    if (c == EOF) break;
    switch (c) {
    case 'v':
      debugHttpd=1;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'd':
      printf("%s",decodeString(optarg));
      return 0;
    case 'e':
      printf("%s",encodeString(optarg));
      return 0;
    case 'r':
      realm = optarg;
      break;
    case 'c':
      configFile = optarg;
      break;
    default:
      fprintf(stderr,"%s\n", httpdVersion);
      show_usage();
      exit(1);
    }
  }

  envp = (char**) malloc((ENVSIZE+1)*sizeof(char*));
  if (envp == 0) perror_exit("envp alloc");

  server = openServer(port);
  if (server < 0) exit(1);

  if (!debugHttpd)
  {
    /* remember our current pwd, daemonize, chdir back */
    char *dir = (char *) malloc(256);
    if (dir == 0) perror_exit("out of memory for getpwd");
    if (getcwd(dir, 256) == 0) perror_exit("getcwd failed");
    if (daemon(0, 1) < 0) perror_exit("daemon");
    chdir(dir);
    free(dir);
  }

  miniHttpd(server);
  
  return 0;
}

#ifdef HTTPD_STANDALONE
int main(int argc, char *argv[])
{ 
  return httpd_main(argc, argv);
}

#endif
