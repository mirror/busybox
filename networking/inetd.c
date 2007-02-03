/* vi: set sw=4 ts=4: */
/*      $Slackware: inetd.c 1.79s 2001/02/06 13:18:00 volkerdi Exp $    */
/*      $OpenBSD: inetd.c,v 1.79 2001/01/30 08:30:57 deraadt Exp $      */
/*      $NetBSD: inetd.c,v 1.11 1996/02/22 11:14:41 mycroft Exp $       */
/* Busybox port by Vladimir Oleynik (C) 2001-2005 <dzo@simtreas.ru>     */
/*
 * Copyright (c) 1983,1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND
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
 */

/* Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.
 * connection-oriented services are invoked each time a
 * connection is made, by creating a process.  This process
 * is passed the connection as file descriptor 0 and is
 * expected to do a getpeername to find out the source host
 * and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or "take over the socket",
 * processing all arriving datagrams and, eventually, timing
 * out.  The first type of server is said to be "multi-threaded";
 * the second type of server "single-threaded".
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is "free format" with fields given in the
 * order shown below.  Continuation lines for an entry must begin with
 * a space or tab.  All fields must be present in each entry.
 *
 *      service name                    must be in /etc/services
 *      socket type                     stream/dgram/raw/rdm/seqpacket
 *      protocol                        must be in /etc/protocols
 *      wait/nowait[.max]               single-threaded/multi-threaded, max #
 *      user[.group] or user[:group]    user/group to run daemon as
 *      server program                  full path name
 *      server program arguments        maximum of MAXARGS (20)
 *
 * For RPC services
 *      service name/version            must be in /etc/rpc
 *      socket type                     stream/dgram/raw/rdm/seqpacket
 *      protocol                        must be in /etc/protocols
 *      wait/nowait[.max]               single-threaded/multi-threaded
 *      user[.group] or user[:group]    user to run daemon as
 *      server program                  full path name
 *      server program arguments        maximum of MAXARGS (20)
 *
 * For non-RPC services, the "service name" can be of the form
 * hostaddress:servicename, in which case the hostaddress is used
 * as the host portion of the address to listen on.  If hostaddress
 * consists of a single `*' character, INADDR_ANY is used.
 *
 * A line can also consist of just
 *      hostaddress:
 * where hostaddress is as in the preceding paragraph.  Such a line must
 * have no further fields; the specified hostaddress is remembered and
 * used for all further lines that have no hostaddress specified,
 * until the next such line (or EOF).  (This is why * is provided to
 * allow explicit specification of INADDR_ANY.)  A line
 *      *:
 * is implicitly in effect at the beginning of the file.
 *
 * The hostaddress specifier may (and often will) contain dots;
 * the service name must not.
 *
 * For RPC services, host-address specifiers are accepted and will
 * work to some extent; however, because of limitations in the
 * portmapper interface, it will not work to try to give more than
 * one line for any given RPC service, even if the host-address
 * specifiers are different.
 *
 * Comment lines are indicated by a `#' in column 1.
 */

/* inetd rules for passing file descriptors to children
 * (http://www.freebsd.org/cgi/man.cgi?query=inetd):
 *
 * The wait/nowait entry specifies whether the server that is invoked by
 * inetd will take over the socket associated with the service access point,
 * and thus whether inetd should wait for the server to exit before listen-
 * ing for new service requests.  Datagram servers must use "wait", as
 * they are always invoked with the original datagram socket bound to the
 * specified service address.  These servers must read at least one datagram
 * from the socket before exiting.  If a datagram server connects to its
 * peer, freeing the socket so inetd can receive further messages on the
 * socket, it is said to be a "multi-threaded" server; it should read one
 * datagram from the socket and create a new socket connected to the peer.
 * It should fork, and the parent should then exit to allow inetd to check
 * for new service requests to spawn new servers.  Datagram servers which
 * process all incoming datagrams on a socket and eventually time out are
 * said to be "single-threaded".  The comsat(8), (biff(1)) and talkd(8)
 * utilities are both examples of the latter type of datagram server.  The
 * tftpd(8) utility is an example of a multi-threaded datagram server.
 *
 * Servers using stream sockets generally are multi-threaded and use the
 * "nowait" entry. Connection requests for these services are accepted by
 * inetd, and the server is given only the newly-accepted socket connected
 * to a client of the service.  Most stream-based services operate in this
 * manner.  Stream-based servers that use "wait" are started with the lis-
 * tening service socket, and must accept at least one connection request
 * before exiting.  Such a server would normally accept and process incoming
 * connection requests until a timeout.
 */

/* Here's the scoop concerning the user[.:]group feature:
 *
 * 1) set-group-option off.
 *
 *      a) user = root: NO setuid() or setgid() is done
 *
 *      b) other:       setgid(primary group as found in passwd)
 *                      initgroups(name, primary group)
 *                      setuid()
 *
 * 2) set-group-option on.
 *
 *      a) user = root: setgid(specified group)
 *                      NO initgroups()
 *                      NO setuid()
 *
 *      b) other:       setgid(specified group)
 *                      initgroups(name, specified group)
 *                      setuid()
 */

#include "busybox.h"
#include <syslog.h>
#include <sys/un.h>

//#define ENABLE_FEATURE_INETD_RPC 1
//#define ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_ECHO 1
//#define ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DISCARD 1
//#define ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_TIME 1
//#define ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME 1
//#define ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN 1
//#define ENABLE_FEATURE_IPV6 1

#if ENABLE_FEATURE_INETD_RPC
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#endif

#define _PATH_INETDCONF "/etc/inetd.conf"
#define _PATH_INETDPID  "/var/run/inetd.pid"


#define CNT_INTVL       60              /* servers in CNT_INTVL sec. */
#define RETRYTIME       (60*10)         /* retry after bind or server fail */

#ifndef RLIMIT_NOFILE
#define RLIMIT_NOFILE   RLIMIT_OFILE
#endif

#ifndef OPEN_MAX
#define OPEN_MAX        64
#endif

/* Reserve some descriptors, 3 stdio + at least: 1 log, 1 conf. file */
#define FD_MARGIN       8
static rlim_t rlim_ofile_cur = OPEN_MAX;
static struct rlimit rlim_ofile;


/* Check unsupporting builtin */
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_ECHO || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DISCARD || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_TIME || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN
# define INETD_FEATURE_ENABLED
#endif

#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_ECHO || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DISCARD || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN
# define INETD_SETPROCTITLE
#endif

typedef struct servtab {
	char *se_hostaddr;                    /* host address to listen on */
	char *se_service;                     /* name of service */
	int se_socktype;                      /* type of socket to use */
	int se_family;                        /* address family */
	char *se_proto;                       /* protocol used */
#if ENABLE_FEATURE_INETD_RPC
	int se_rpcprog;                       /* rpc program number */
	int se_rpcversl;                      /* rpc program lowest version */
	int se_rpcversh;                      /* rpc program highest version */
#define isrpcservice(sep)       ((sep)->se_rpcversl != 0)
#else
#define isrpcservice(sep)       0
#endif
	pid_t se_wait;                        /* single threaded server */
	short se_checked;                     /* looked at during merge */
	char *se_user;                        /* user name to run as */
	char *se_group;                       /* group name to run as */
#ifdef INETD_FEATURE_ENABLED
	const struct builtin *se_bi;          /* if built-in, description */
#endif
	char *se_server;                      /* server program */
#define MAXARGV 20
	char *se_argv[MAXARGV + 1];           /* program arguments */
	int se_fd;                            /* open descriptor */
	union {
		struct sockaddr se_un_ctrladdr;
		struct sockaddr_in se_un_ctrladdr_in;
#if ENABLE_FEATURE_IPV6
		struct sockaddr_in6 se_un_ctrladdr_in6;
#endif
		struct sockaddr_un se_un_ctrladdr_un;
	} se_un;                              /* bound address */
#define se_ctrladdr     se_un.se_un_ctrladdr
#define se_ctrladdr_in  se_un.se_un_ctrladdr_in
#define se_ctrladdr_in6 se_un.se_un_ctrladdr_in6
#define se_ctrladdr_un  se_un.se_un_ctrladdr_un
	int se_ctrladdr_size;
	int se_max;                           /* max # of instances of this service */
	int se_count;                         /* number started since se_time */
	struct timeval se_time;               /* start of se_count */
	struct servtab *se_next;
} servtab_t;

static servtab_t *servtab;

#ifdef INETD_FEATURE_ENABLED
struct builtin {
	const char *bi_service;               /* internally provided service name */
	int bi_socktype;                      /* type of socket supported */
	short bi_fork;                        /* 1 if should fork before call */
	short bi_wait;                        /* 1 if should wait for child */
	void (*bi_fn) (int, servtab_t *);
};

		/* Echo received data */
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_ECHO
static void echo_stream(int, servtab_t *);
static void echo_dg(int, servtab_t *);
#endif
		/* Internet /dev/null */
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DISCARD
static void discard_stream(int, servtab_t *);
static void discard_dg(int, servtab_t *);
#endif
		/* Return 32 bit time since 1900 */
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_TIME
static void machtime_stream(int, servtab_t *);
static void machtime_dg(int, servtab_t *);
#endif
		/* Return human-readable time */
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME
static void daytime_stream(int, servtab_t *);
static void daytime_dg(int, servtab_t *);
#endif
		/* Familiar character generator */
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN
static void chargen_stream(int, servtab_t *);
static void chargen_dg(int, servtab_t *);
#endif

static const struct builtin builtins[] = {
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_ECHO
	/* Echo received data */
	{"echo", SOCK_STREAM, 1, 0, echo_stream,},
	{"echo", SOCK_DGRAM, 0, 0, echo_dg,},
#endif
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DISCARD
	/* Internet /dev/null */
	{"discard", SOCK_STREAM, 1, 0, discard_stream,},
	{"discard", SOCK_DGRAM, 0, 0, discard_dg,},
#endif
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_TIME
	/* Return 32 bit time since 1900 */
	{"time", SOCK_STREAM, 0, 0, machtime_stream,},
	{"time", SOCK_DGRAM, 0, 0, machtime_dg,},
#endif
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME
	/* Return human-readable time */
	{"daytime", SOCK_STREAM, 0, 0, daytime_stream,},
	{"daytime", SOCK_DGRAM, 0, 0, daytime_dg,},
#endif
#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN
	/* Familiar character generator */
	{"chargen", SOCK_STREAM, 1, 0, chargen_stream,},
	{"chargen", SOCK_DGRAM, 0, 0, chargen_dg,},
#endif
	{NULL, 0, 0, 0, NULL}
};
#endif /* INETD_FEATURE_ENABLED */

static int global_queuelen = 128;
static int nsock, maxsock;
static fd_set allsock;
static int toomany;
static int timingout;
static struct servent *sp;
static uid_t uid;

static const char *CONFIG = _PATH_INETDCONF;

static FILE *fconfig;
static char line[1024];
static char *defhost;

/* xstrdup(NULL) returns NULL, but this one
 * will return newly-allocated "" if called with NULL arg
 * TODO: audit whether this makes any real difference
 */
static char *xxstrdup(char *cp)
{
	return xstrdup(cp ? cp : "");
}

static int setconfig(void)
{
	free(defhost);
	defhost = xstrdup("*");
	if (fconfig != NULL) {
		fseek(fconfig, 0L, SEEK_SET);
		return 1;
	}
	fconfig = fopen(CONFIG, "r");
	return (fconfig != NULL);
}

static void endconfig(void)
{
	if (fconfig) {
		(void) fclose(fconfig);
		fconfig = NULL;
	}
	free(defhost);
	defhost = 0;
}

#if ENABLE_FEATURE_INETD_RPC
static void register_rpc(servtab_t *sep)
{
	int n;
	struct sockaddr_in ir_sin;
	struct protoent *pp;
	socklen_t size;

	if ((pp = getprotobyname(sep->se_proto + 4)) == NULL) {
		bb_perror_msg("%s: getproto", sep->se_proto);
		return;
	}
	size = sizeof ir_sin;
	if (getsockname(sep->se_fd, (struct sockaddr *) &ir_sin, &size) < 0) {
		bb_perror_msg("%s/%s: getsockname",
				sep->se_service, sep->se_proto);
		return;
	}

	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		(void) pmap_unset(sep->se_rpcprog, n);
		if (!pmap_set(sep->se_rpcprog, n, pp->p_proto, ntohs(ir_sin.sin_port)))
			bb_perror_msg("%s %s: pmap_set: %u %u %u %u",
					sep->se_service, sep->se_proto,
					sep->se_rpcprog, n, pp->p_proto, ntohs(ir_sin.sin_port));
	}
}

static void unregister_rpc(servtab_t *sep)
{
	int n;

	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		if (!pmap_unset(sep->se_rpcprog, n))
			bb_error_msg("pmap_unset(%u, %u)", sep->se_rpcprog, n);
	}
}
#endif /* FEATURE_INETD_RPC */

static void freeconfig(servtab_t *cp)
{
	int i;

	free(cp->se_hostaddr);
	free(cp->se_service);
	free(cp->se_proto);
	free(cp->se_user);
	free(cp->se_group);
	free(cp->se_server);
	for (i = 0; i < MAXARGV; i++)
		free(cp->se_argv[i]);
}

static int bump_nofile(void)
{
#define FD_CHUNK        32

	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		bb_perror_msg("getrlimit");
		return -1;
	}
	rl.rlim_cur = MIN(rl.rlim_max, rl.rlim_cur + FD_CHUNK);
	rl.rlim_cur = MIN(FD_SETSIZE, rl.rlim_cur + FD_CHUNK);
	if (rl.rlim_cur <= rlim_ofile_cur) {
		bb_error_msg("bump_nofile: cannot extend file limit, max = %d",
						(int) rl.rlim_cur);
		return -1;
	}

	if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
		bb_perror_msg("setrlimit");
		return -1;
	}

	rlim_ofile_cur = rl.rlim_cur;
	return 0;
}

static void setup(servtab_t *sep)
{
	int r;

	sep->se_fd = socket(sep->se_family, sep->se_socktype, 0);
	if (sep->se_fd < 0) {
		bb_perror_msg("%s/%s: socket", sep->se_service, sep->se_proto);
		return;
	}
	if (setsockopt_reuseaddr(sep->se_fd) < 0)
		bb_perror_msg("setsockopt(SO_REUSEADDR)");

#if ENABLE_FEATURE_INETD_RPC
	if (isrpcservice(sep)) {
		struct passwd *pwd;

		/*
		 * for RPC services, attempt to use a reserved port
		 * if they are going to be running as root.
		 *
		 * Also, zero out the port for all RPC services; let bind()
		 * find one.
		 */
		sep->se_ctrladdr_in.sin_port = 0;
		if (sep->se_user && (pwd = getpwnam(sep->se_user)) &&
				pwd->pw_uid == 0 && uid == 0)
			r = bindresvport(sep->se_fd, &sep->se_ctrladdr_in);
		else {
			r = bind(sep->se_fd, &sep->se_ctrladdr, sep->se_ctrladdr_size);
			if (r == 0) {
				socklen_t len = sep->se_ctrladdr_size;
				int saveerrno = errno;

				/* update se_ctrladdr_in.sin_port */
				r = getsockname(sep->se_fd, &sep->se_ctrladdr, &len);
				if (r <= 0)
					errno = saveerrno;
			}
		}
	} else
#endif
		r = bind(sep->se_fd, &sep->se_ctrladdr, sep->se_ctrladdr_size);
	if (r < 0) {
		bb_perror_msg("%s/%s (%d): bind",
				sep->se_service, sep->se_proto, sep->se_ctrladdr.sa_family);
		close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, global_queuelen);

	FD_SET(sep->se_fd, &allsock);
	nsock++;
	if (sep->se_fd > maxsock) {
		maxsock = sep->se_fd;
		if ((rlim_t)maxsock > rlim_ofile_cur - FD_MARGIN)
			bump_nofile();
	}
}

static char *nextline(void)
{
	char *cp;
	FILE *fd = fconfig;

	if (fgets(line, sizeof(line), fd) == NULL)
		return NULL;
	cp = strchr(line, '\n');
	if (cp)
		*cp = '\0';
	return line;
}

static char *skip(char **cpp) /* int report; */
{
	char *cp = *cpp;
	char *start;

/* erp: */
	if (*cpp == NULL) {
		/* if (report) */
		/* bb_error_msg("syntax error in inetd config file"); */
		return NULL;
	}

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		int c;

		c = getc(fconfig);
		(void) ungetc(c, fconfig);
		if (c == ' ' || c == '\t')
			if ((cp = nextline()))
				goto again;
		*cpp = NULL;
		/* goto erp; */
		return NULL;
	}
	start = cp;
	while (*cp && *cp != ' ' && *cp != '\t')
		cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	/* if ((*cpp = cp) == NULL) */
	/* goto erp; */

	*cpp = cp;
	return start;
}

static servtab_t *new_servtab(void)
{
	return xmalloc(sizeof(servtab_t));
}

static servtab_t *dupconfig(servtab_t *sep)
{
	servtab_t *newtab;
	int argc;

	newtab = new_servtab();
	memset(newtab, 0, sizeof(servtab_t));
	newtab->se_service = xstrdup(sep->se_service);
	newtab->se_socktype = sep->se_socktype;
	newtab->se_family = sep->se_family;
	newtab->se_proto = xstrdup(sep->se_proto);
#if ENABLE_FEATURE_INETD_RPC
	newtab->se_rpcprog = sep->se_rpcprog;
	newtab->se_rpcversl = sep->se_rpcversl;
	newtab->se_rpcversh = sep->se_rpcversh;
#endif
	newtab->se_wait = sep->se_wait;
	newtab->se_user = xstrdup(sep->se_user);
	newtab->se_group = xstrdup(sep->se_group);
#ifdef INETD_FEATURE_ENABLED
	newtab->se_bi = sep->se_bi;
#endif
	newtab->se_server = xstrdup(sep->se_server);

	for (argc = 0; argc <= MAXARGV; argc++)
		newtab->se_argv[argc] = xstrdup(sep->se_argv[argc]);
	newtab->se_max = sep->se_max;

	return newtab;
}

static servtab_t *getconfigent(void)
{
	servtab_t *sep;
	int argc;
	char *cp, *arg;
	char *hostdelim;
	servtab_t *nsep;
	servtab_t *psep;

	sep = new_servtab();

	/* memset(sep, 0, sizeof *sep); */
 more:
	/* freeconfig(sep); */

	while ((cp = nextline()) && *cp == '#') /* skip comment line */;
	if (cp == NULL) {
		/* free(sep); */
		return NULL;
	}

	memset((char *) sep, 0, sizeof *sep);
	arg = skip(&cp);
	if (arg == NULL) {
		/* A blank line. */
		goto more;
	}

	/* Check for a host name. */
	hostdelim = strrchr(arg, ':');
	if (hostdelim) {
		*hostdelim = '\0';
		sep->se_hostaddr = xstrdup(arg);
		arg = hostdelim + 1;
		/*
		 * If the line is of the form `host:', then just change the
		 * default host for the following lines.
		 */
		if (*arg == '\0') {
			arg = skip(&cp);
			if (cp == NULL) {
				free(defhost);
				defhost = sep->se_hostaddr;
				goto more;
			}
		}
	} else
		sep->se_hostaddr = xxstrdup(defhost);

	sep->se_service = xxstrdup(arg);
	arg = skip(&cp);

	if (strcmp(arg, "stream") == 0)
		sep->se_socktype = SOCK_STREAM;
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;

	sep->se_proto = xxstrdup(skip(&cp));

	if (strcmp(sep->se_proto, "unix") == 0) {
		sep->se_family = AF_UNIX;
	} else {
		sep->se_family = AF_INET;
		if (sep->se_proto[strlen(sep->se_proto) - 1] == '6')
#if ENABLE_FEATURE_IPV6
			sep->se_family = AF_INET6;
#else
			bb_error_msg("%s: IPV6 not supported", sep->se_proto);
#endif
		if (strncmp(sep->se_proto, "rpc/", 4) == 0) {
#if ENABLE_FEATURE_INETD_RPC
			char *p, *ccp;
			long l;

			p = strchr(sep->se_service, '/');
			if (p == 0) {
				bb_error_msg("%s: no rpc version", sep->se_service);
				goto more;
			}
			*p++ = '\0';
			l = strtol(p, &ccp, 0);
			if (ccp == p || l < 0 || l > INT_MAX) {
			badafterall:
				bb_error_msg("%s/%s: bad rpc version", sep->se_service, p);
				goto more;
			}
			sep->se_rpcversl = sep->se_rpcversh = l;
			if (*ccp == '-') {
				p = ccp + 1;
				l = strtol(p, &ccp, 0);
				if (ccp == p || l < 0 || l > INT_MAX || l < sep->se_rpcversl || *ccp)
					goto badafterall;
				sep->se_rpcversh = l;
			} else if (*ccp != '\0')
				goto badafterall;
#else
			bb_error_msg("%s: rpc services not supported", sep->se_service);
#endif
		}
	}
	arg = skip(&cp);
	if (arg == NULL)
		goto more;

	{
		char *s = strchr(arg, '.');
		if (s) {
			*s++ = '\0';
			sep->se_max = xatoi(s);
		} else
			sep->se_max = toomany;
	}
	sep->se_wait = strcmp(arg, "wait") == 0;
	/* if ((arg = skip(&cp, 1)) == NULL) */
	/* goto more; */
	sep->se_user = xxstrdup(skip(&cp));
	arg = strchr(sep->se_user, '.');
	if (arg == NULL)
		arg = strchr(sep->se_user, ':');
	if (arg) {
		*arg++ = '\0';
		sep->se_group = xstrdup(arg);
	}
	/* if ((arg = skip(&cp, 1)) == NULL) */
	/* goto more; */

	sep->se_server = xxstrdup(skip(&cp));
	if (strcmp(sep->se_server, "internal") == 0) {
#ifdef INETD_FEATURE_ENABLED
		const struct builtin *bi;

		for (bi = builtins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
					strcmp(bi->bi_service, sep->se_service) == 0)
				break;
		if (bi->bi_service == 0) {
			bb_error_msg("internal service %s unknown", sep->se_service);
			goto more;
		}
		sep->se_bi = bi;
		sep->se_wait = bi->bi_wait;
#else
		bb_perror_msg("internal service %s unknown", sep->se_service);
		goto more;
#endif
	}
#ifdef INETD_FEATURE_ENABLED
		else
		sep->se_bi = NULL;
#endif
	argc = 0;
	for (arg = skip(&cp); cp; arg = skip(&cp)) {
		if (argc < MAXARGV)
			sep->se_argv[argc++] = xxstrdup(arg);
	}
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;

	/*
	 * Now that we've processed the entire line, check if the hostname
	 * specifier was a comma separated list of hostnames. If so
	 * we'll make new entries for each address.
	 */
	while ((hostdelim = strrchr(sep->se_hostaddr, ',')) != NULL) {
		nsep = dupconfig(sep);

		/*
		 * NULL terminate the hostname field of the existing entry,
		 * and make a dup for the new entry.
		 */
		*hostdelim++ = '\0';
		nsep->se_hostaddr = xstrdup(hostdelim);

		nsep->se_next = sep->se_next;
		sep->se_next = nsep;
	}

	nsep = sep;
	while (nsep != NULL) {
		nsep->se_checked = 1;
		if (nsep->se_family == AF_INET) {
			if (LONE_CHAR(nsep->se_hostaddr, '*'))
				nsep->se_ctrladdr_in.sin_addr.s_addr = INADDR_ANY;
			else if (!inet_aton(nsep->se_hostaddr, &nsep->se_ctrladdr_in.sin_addr)) {
				struct hostent *hp;

				hp = gethostbyname(nsep->se_hostaddr);
				if (hp == 0) {
					bb_error_msg("%s: unknown host", nsep->se_hostaddr);
					nsep->se_checked = 0;
					goto skip;
				} else if (hp->h_addrtype != AF_INET) {
					bb_error_msg("%s: address isn't an Internet "
								  "address", nsep->se_hostaddr);
					nsep->se_checked = 0;
					goto skip;
				} else {
					int i = 1;

					memmove(&nsep->se_ctrladdr_in.sin_addr,
								   hp->h_addr_list[0], sizeof(struct in_addr));
					while (hp->h_addr_list[i] != NULL) {
						psep = dupconfig(nsep);
						psep->se_hostaddr = xxstrdup(nsep->se_hostaddr);
						psep->se_checked = 1;
						memmove(&psep->se_ctrladdr_in.sin_addr,
								     hp->h_addr_list[i], sizeof(struct in_addr));
						psep->se_ctrladdr_size = sizeof(psep->se_ctrladdr_in);
						i++;
						/* Prepend to list, don't want to look up */
						/* its hostname again. */
						psep->se_next = sep;
						sep = psep;
					}
				}
			}
		}
/* XXX BUG?: is this skip: label supposed to remain? */
	skip:
		nsep = nsep->se_next;
	}

	/*
	 * Finally, free any entries which failed the gethostbyname
	 * check.
	 */
	psep = NULL;
	nsep = sep;
	while (nsep != NULL) {
		servtab_t *tsep;

		if (nsep->se_checked == 0) {
			tsep = nsep;
			if (psep == NULL) {
				sep = nsep->se_next;
				nsep = sep;
			} else {
				nsep = nsep->se_next;
				psep->se_next = nsep;
			}
			freeconfig(tsep);
		} else {
			nsep->se_checked = 0;
			psep = nsep;
			nsep = nsep->se_next;
		}
	}

	return sep;
}

#define Block_Using_Signals(m) do { \
	sigemptyset(&m); \
	sigaddset(&m, SIGCHLD); \
	sigaddset(&m, SIGHUP); \
	sigaddset(&m, SIGALRM); \
	sigprocmask(SIG_BLOCK, &m, NULL); \
} while (0)

static servtab_t *enter(servtab_t *cp)
{
	servtab_t *sep;
	sigset_t omask;

	sep = new_servtab();
	*sep = *cp;
	sep->se_fd = -1;
#if ENABLE_FEATURE_INETD_RPC
	sep->se_rpcprog = -1;
#endif
	Block_Using_Signals(omask);
	sep->se_next = servtab;
	servtab = sep;
	sigprocmask(SIG_UNBLOCK, &omask, NULL);
	return sep;
}

static int matchconf(servtab_t *old, servtab_t *new)
{
	if (strcmp(old->se_service, new->se_service) != 0)
		return 0;

	if (strcmp(old->se_hostaddr, new->se_hostaddr) != 0)
		return 0;

	if (strcmp(old->se_proto, new->se_proto) != 0)
		return 0;

	/*
	 * If the new servtab is bound to a specific address, check that the
	 * old servtab is bound to the same entry. If the new service is not
	 * bound to a specific address then the check of se_hostaddr above
	 * is sufficient.
	 */

	if (old->se_family == AF_INET && new->se_family == AF_INET &&
			memcmp(&old->se_ctrladdr_in.sin_addr,
					&new->se_ctrladdr_in.sin_addr,
					sizeof(new->se_ctrladdr_in.sin_addr)) != 0)
		return 0;

#if ENABLE_FEATURE_IPV6
	if (old->se_family == AF_INET6 && new->se_family == AF_INET6 &&
			memcmp(&old->se_ctrladdr_in6.sin6_addr,
					&new->se_ctrladdr_in6.sin6_addr,
					sizeof(new->se_ctrladdr_in6.sin6_addr)) != 0)
		return 0;
#endif
	return 1;
}

static void config(int sig ATTRIBUTE_UNUSED)
{
	servtab_t *sep, *cp, **sepp;
	sigset_t omask;
	size_t n;
	char protoname[10];

	if (!setconfig()) {
		bb_perror_msg("%s", CONFIG);
		return;
	}
	for (sep = servtab; sep; sep = sep->se_next)
		sep->se_checked = 0;
	cp = getconfigent();
	while (cp != NULL) {
		for (sep = servtab; sep; sep = sep->se_next)
			if (matchconf(sep, cp))
				break;

		if (sep != 0) {
			int i;

#define SWAP(type, a, b) do {type c=(type)a; a=(type)b; b=(type)c;} while (0)

			Block_Using_Signals(omask);
			/*
			 * sep->se_wait may be holding the pid of a daemon
			 * that we're waiting for.  If so, don't overwrite
			 * it unless the config file explicitly says don't
			 * wait.
			 */
			if (
#ifdef INETD_FEATURE_ENABLED
				cp->se_bi == 0 &&
#endif
				(sep->se_wait == 1 || cp->se_wait == 0))
				sep->se_wait = cp->se_wait;
			SWAP(int, cp->se_max, sep->se_max);
			SWAP(char *, sep->se_user, cp->se_user);
			SWAP(char *, sep->se_group, cp->se_group);
			SWAP(char *, sep->se_server, cp->se_server);
			for (i = 0; i < MAXARGV; i++)
				SWAP(char *, sep->se_argv[i], cp->se_argv[i]);
#undef SWAP

#if ENABLE_FEATURE_INETD_RPC
			if (isrpcservice(sep))
				unregister_rpc(sep);
			sep->se_rpcversl = cp->se_rpcversl;
			sep->se_rpcversh = cp->se_rpcversh;
#endif
			sigprocmask(SIG_UNBLOCK, &omask, NULL);
			freeconfig(cp);
		} else {
			sep = enter(cp);
		}
		sep->se_checked = 1;

		switch (sep->se_family) {
		case AF_UNIX:
			if (sep->se_fd != -1)
				break;
			(void) unlink(sep->se_service);
			n = strlen(sep->se_service);
			if (n > sizeof sep->se_ctrladdr_un.sun_path - 1)
				n = sizeof sep->se_ctrladdr_un.sun_path - 1;
			safe_strncpy(sep->se_ctrladdr_un.sun_path, sep->se_service, n + 1);
			sep->se_ctrladdr_un.sun_family = AF_UNIX;
			sep->se_ctrladdr_size = n + sizeof sep->se_ctrladdr_un.sun_family;
			setup(sep);
			break;
		case AF_INET:
			sep->se_ctrladdr_in.sin_family = AF_INET;
			/* se_ctrladdr_in was set in getconfigent */
			sep->se_ctrladdr_size = sizeof sep->se_ctrladdr_in;

#if ENABLE_FEATURE_INETD_RPC
			if (isrpcservice(sep)) {
				struct rpcent *rp;
				// FIXME: atoi_or_else(str, 0) would be handy here
				sep->se_rpcprog = atoi(sep->se_service);
				if (sep->se_rpcprog == 0) {
					rp = getrpcbyname(sep->se_service);
					if (rp == 0) {
						bb_error_msg("%s: unknown rpc service", sep->se_service);
						goto serv_unknown;
					}
					sep->se_rpcprog = rp->r_number;
				}
				if (sep->se_fd == -1)
					setup(sep);
				if (sep->se_fd != -1)
					register_rpc(sep);
			} else
#endif
			{
				uint16_t port = htons(atoi(sep->se_service));
				// FIXME: atoi_or_else(str, 0) would be handy here
				if (!port) {
					 /*XXX*/ strncpy(protoname, sep->se_proto, sizeof(protoname));
					if (isdigit(protoname[strlen(protoname) - 1]))
						protoname[strlen(protoname) - 1] = '\0';
					sp = getservbyname(sep->se_service, protoname);
					if (sp == 0) {
						bb_error_msg("%s/%s: unknown service",
								sep->se_service, sep->se_proto);
						goto serv_unknown;
					}
					port = sp->s_port;
				}
				if (port != sep->se_ctrladdr_in.sin_port) {
					sep->se_ctrladdr_in.sin_port = port;
					if (sep->se_fd != -1) {
						FD_CLR(sep->se_fd, &allsock);
						nsock--;
						(void) close(sep->se_fd);
					}
					sep->se_fd = -1;
				}
				if (sep->se_fd == -1)
					setup(sep);
			}
			break;
#if ENABLE_FEATURE_IPV6
		case AF_INET6:
			sep->se_ctrladdr_in6.sin6_family = AF_INET6;
			/* se_ctrladdr_in was set in getconfigent */
			sep->se_ctrladdr_size = sizeof sep->se_ctrladdr_in6;

#if ENABLE_FEATURE_INETD_RPC
			if (isrpcservice(sep)) {
				struct rpcent *rp;

				sep->se_rpcprog = atoi(sep->se_service);
				if (sep->se_rpcprog == 0) {
					rp = getrpcbyname(sep->se_service);
					if (rp == 0) {
						bb_error_msg("%s: unknown rpc service", sep->se_service);
						goto serv_unknown;
					}
					sep->se_rpcprog = rp->r_number;
				}
				if (sep->se_fd == -1)
					setup(sep);
				if (sep->se_fd != -1)
					register_rpc(sep);
			} else
#endif
			{
				uint16_t port = htons(atoi(sep->se_service));

				if (!port) {
					 /*XXX*/ strncpy(protoname, sep->se_proto, sizeof(protoname));
					if (isdigit(protoname[strlen(protoname) - 1]))
						protoname[strlen(protoname) - 1] = '\0';
					sp = getservbyname(sep->se_service, protoname);
					if (sp == 0) {
						bb_error_msg("%s/%s: unknown service",
								sep->se_service, sep->se_proto);
						goto serv_unknown;
					}
					port = sp->s_port;
				}
				if (port != sep->se_ctrladdr_in6.sin6_port) {
					sep->se_ctrladdr_in6.sin6_port = port;
					if (sep->se_fd != -1) {
						FD_CLR(sep->se_fd, &allsock);
						nsock--;
						(void) close(sep->se_fd);
					}
					sep->se_fd = -1;
				}
				if (sep->se_fd == -1)
					setup(sep);
			}
			break;
#endif /* FEATURE_IPV6 */
		}
	serv_unknown:
		if (cp->se_next != NULL) {
			servtab_t *tmp = cp;

			cp = cp->se_next;
			free(tmp);
		} else {
			free(cp);
			cp = getconfigent();
		}
	}
	endconfig();
	/*
	 * Purge anything not looked at above.
	 */
	Block_Using_Signals(omask);
	sepp = &servtab;
	while ((sep = *sepp)) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd != -1) {
			FD_CLR(sep->se_fd, &allsock);
			nsock--;
			(void) close(sep->se_fd);
		}
#if ENABLE_FEATURE_INETD_RPC
		if (isrpcservice(sep))
			unregister_rpc(sep);
#endif
		if (sep->se_family == AF_UNIX)
			(void) unlink(sep->se_service);
		freeconfig(sep);
		free(sep);
	}
	sigprocmask(SIG_UNBLOCK, &omask, NULL);
}


static void reapchild(int sig ATTRIBUTE_UNUSED)
{
	pid_t pid;
	int save_errno = errno, status;
	servtab_t *sep;

	for (;;) {
		pid = wait3(&status, WNOHANG, NULL);
		if (pid <= 0)
			break;
		for (sep = servtab; sep; sep = sep->se_next)
			if (sep->se_wait == pid) {
				if (WIFEXITED(status) && WEXITSTATUS(status))
					bb_error_msg("%s: exit status 0x%x",
							sep->se_server, WEXITSTATUS(status));
				else if (WIFSIGNALED(status))
					bb_error_msg("%s: exit signal 0x%x",
							sep->se_server, WTERMSIG(status));
				sep->se_wait = 1;
				FD_SET(sep->se_fd, &allsock);
				nsock++;
			}
	}
	errno = save_errno;
}

static void retry(int sig ATTRIBUTE_UNUSED)
{
	servtab_t *sep;

	timingout = 0;
	for (sep = servtab; sep; sep = sep->se_next) {
		if (sep->se_fd == -1) {
			switch (sep->se_family) {
			case AF_UNIX:
			case AF_INET:
#if ENABLE_FEATURE_IPV6
			case AF_INET6:
#endif
				setup(sep);
#if ENABLE_FEATURE_INETD_RPC
				if (sep->se_fd != -1 && isrpcservice(sep))
					register_rpc(sep);
#endif
				break;
			}
		}
	}
}

static void goaway(int sig ATTRIBUTE_UNUSED)
{
	servtab_t *sep;

	/* XXX signal race walking sep list */
	for (sep = servtab; sep; sep = sep->se_next) {
		if (sep->se_fd == -1)
			continue;

		switch (sep->se_family) {
		case AF_UNIX:
			(void) unlink(sep->se_service);
			break;
		case AF_INET:
#if ENABLE_FEATURE_IPV6
		case AF_INET6:
#endif
#if ENABLE_FEATURE_INETD_RPC
			if (sep->se_wait == 1 && isrpcservice(sep))
				unregister_rpc(sep);   /* XXX signal race */
#endif
			break;
		}
		(void) close(sep->se_fd);
	}
	(void) unlink(_PATH_INETDPID);
	exit(0);
}


#ifdef INETD_SETPROCTITLE
static char **Argv;
static char *LastArg;

static void
inetd_setproctitle(char *a, int s)
{
	socklen_t size;
	char *cp;
	struct sockaddr_in prt_sin;
	char buf[80];

	cp = Argv[0];
	size = sizeof(prt_sin);
	(void) snprintf(buf, sizeof buf, "-%s", a);
	if (getpeername(s, (struct sockaddr *) &prt_sin, &size) == 0) {
		char *sa = inet_ntoa(prt_sin.sin_addr);

		buf[sizeof(buf) - 1 - strlen(sa) - 3] = '\0';
		strcat(buf, " [");
		strcat(buf, sa);
		strcat(buf, "]");
	}
	strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = ' ';
}
#endif


int inetd_main(int argc, char *argv[]);
int inetd_main(int argc, char *argv[])
{
	servtab_t *sep;
	struct passwd *pwd;
	struct group *grp = NULL;
	int tmpint;
	struct sigaction sa, sapipe;
	int opt;
	pid_t pid;
	char buf[50];
	char *stoomany;
	sigset_t omask, wait_mask;

#ifdef INETD_SETPROCTITLE
	extern char **environ;
	char **envp = environ;

	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);
#endif

	opt = getopt32(argc, argv, "R:f", &stoomany);
	if(opt & 1) {
		toomany = xatoi_u(stoomany);
	}
	argc -= optind;
	argv += optind;

	uid = getuid();
	if (uid != 0)
		CONFIG = NULL;
	if (argc > 0)
		CONFIG = argv[0];
	if (CONFIG == NULL)
		bb_error_msg_and_die("non-root must specify a config file");

#ifdef BB_NOMMU
	if (!(opt & 2)) {
		/* reexec for vfork() do continue parent */
		vfork_daemon_rexec(0, 0, argc, argv, "-f");
	}
	bb_sanitize_stdio();
#else
	bb_sanitize_stdio_maybe_daemonize(!(opt & 2));
#endif
	openlog(applet_name, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	logmode = LOGMODE_SYSLOG;

	if (uid == 0) {
		/* If run by hand, ensure groups vector gets trashed */
		gid_t gid = getgid();
		setgroups(1, &gid);
	}

	{
		FILE *fp = fopen(_PATH_INETDPID, "w");
		if (fp != NULL) {
			fprintf(fp, "%u\n", getpid());
			fclose(fp);
		}
	}

	if (getrlimit(RLIMIT_NOFILE, &rlim_ofile) < 0) {
		bb_perror_msg("getrlimit");
	} else {
		rlim_ofile_cur = rlim_ofile.rlim_cur;
		if (rlim_ofile_cur == RLIM_INFINITY)    /* ! */
			rlim_ofile_cur = OPEN_MAX;
	}

	memset((char *) &sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGALRM);
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigaddset(&sa.sa_mask, SIGHUP);
	sa.sa_handler = retry;
	sigaction(SIGALRM, &sa, NULL);
	config(SIGHUP);
	sa.sa_handler = config;
	sigaction(SIGHUP, &sa, NULL);
	sa.sa_handler = reapchild;
	sigaction(SIGCHLD, &sa, NULL);
	sa.sa_handler = goaway;
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = goaway;
	sigaction(SIGINT, &sa, NULL);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, &sapipe);
	memset(&wait_mask, 0, sizeof(wait_mask));
	{
		/* space for daemons to overwrite environment for ps */
#define DUMMYSIZE       100
		char dummy[DUMMYSIZE];

		(void) memset(dummy, 'x', DUMMYSIZE - 1);
		dummy[DUMMYSIZE - 1] = '\0';

		(void) setenv("inetd_dummy", dummy, 1);
	}

	for (;;) {
		int n, ctrl = -1;
		fd_set readable;

		if (nsock == 0) {
			Block_Using_Signals(omask);
			while (nsock == 0)
				sigsuspend(&wait_mask);
			sigprocmask(SIG_UNBLOCK, &omask, NULL);
		}

		readable = allsock;
		n = select(maxsock + 1, &readable, NULL, NULL, NULL);
		if (n <= 0) {
			if (n < 0 && errno != EINTR) {
				bb_perror_msg("select");
				sleep(1);
			}
			continue;
		}

		for (sep = servtab; n && sep; sep = sep->se_next) {
			if (sep->se_fd == -1 || !FD_ISSET(sep->se_fd, &readable))
				continue;

			n--;
			if (!sep->se_wait && sep->se_socktype == SOCK_STREAM) {
				ctrl = accept(sep->se_fd, NULL, NULL);
				if (ctrl < 0) {
					if (errno == EINTR)
						continue;
					bb_perror_msg("accept (for %s)", sep->se_service);
					continue;
				}
				if (sep->se_family == AF_INET && sep->se_socktype == SOCK_STREAM) {
					struct sockaddr_in peer;
					socklen_t plen = sizeof(peer);

					if (getpeername(ctrl, (struct sockaddr *) &peer, &plen) < 0) {
						bb_error_msg("cannot getpeername");
						close(ctrl);
						continue;
					}
					if (ntohs(peer.sin_port) == 20) {
						/* XXX ftp bounce */
						close(ctrl);
						continue;
					}
				}
			} else
				ctrl = sep->se_fd;

			Block_Using_Signals(omask);
			pid = 0;
#ifdef INETD_FEATURE_ENABLED
			if (sep->se_bi == 0 || sep->se_bi->bi_fork)
#endif
			{
				if (sep->se_count++ == 0)
					(void) gettimeofday(&sep->se_time, NULL);
				else if (toomany > 0 && sep->se_count >= sep->se_max) {
					struct timeval now;

					(void) gettimeofday(&now, NULL);
					if (now.tv_sec - sep->se_time.tv_sec > CNT_INTVL) {
						sep->se_time = now;
						sep->se_count = 1;
					} else {
						if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
							close(ctrl);
						if (sep->se_family == AF_INET &&
							  ntohs(sep->se_ctrladdr_in.sin_port) >= IPPORT_RESERVED) {
							/*
							 * Cannot close it -- there are
							 * thieves on the system.
							 * Simply ignore the connection.
							 */
							--sep->se_count;
							continue;
						}
						bb_error_msg("%s/%s server failing (looping), service terminated",
							      sep->se_service, sep->se_proto);
						if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
							close(ctrl);
						FD_CLR(sep->se_fd, &allsock);
						(void) close(sep->se_fd);
						sep->se_fd = -1;
						sep->se_count = 0;
						nsock--;
						sigprocmask(SIG_UNBLOCK, &omask, NULL);
						if (!timingout) {
							timingout = 1;
							alarm(RETRYTIME);
						}
						continue;
					}
				}
				pid = fork();
			}
			if (pid < 0) {
				bb_perror_msg("fork");
				if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
					close(ctrl);
				sigprocmask(SIG_UNBLOCK, &omask, NULL);
				sleep(1);
				continue;
			}
			if (pid && sep->se_wait) {
				sep->se_wait = pid;
				FD_CLR(sep->se_fd, &allsock);
				nsock--;
			}
			sigprocmask(SIG_UNBLOCK, &omask, NULL);
			if (pid == 0) {
#ifdef INETD_FEATURE_ENABLED
				if (sep->se_bi) {
					(*sep->se_bi->bi_fn)(ctrl, sep);
				} else
#endif
				{
					pwd = getpwnam(sep->se_user);
					if (pwd == NULL) {
						bb_error_msg("getpwnam: %s: no such user", sep->se_user);
						goto do_exit1;
					}
					if (setsid() < 0)
						bb_perror_msg("%s: setsid", sep->se_service);
					if (sep->se_group && (grp = getgrnam(sep->se_group)) == NULL) {
						bb_error_msg("getgrnam: %s: no such group", sep->se_group);
						goto do_exit1;
					}
					if (uid != 0) {
						/* a user running private inetd */
						if (uid != pwd->pw_uid)
							_exit(1);
					} else if (pwd->pw_uid) {
						if (sep->se_group)
							pwd->pw_gid = grp->gr_gid;
						xsetgid((gid_t) pwd->pw_gid);
						initgroups(pwd->pw_name, pwd->pw_gid);
						xsetuid((uid_t) pwd->pw_uid);
					} else if (sep->se_group) {
						xsetgid(grp->gr_gid);
						setgroups(1, &grp->gr_gid);
					}
					dup2(ctrl, 0);
					if (ctrl) close(ctrl);
					dup2(0, 1);
					dup2(0, 2);
					if (rlim_ofile.rlim_cur != rlim_ofile_cur)
						if (setrlimit(RLIMIT_NOFILE, &rlim_ofile) < 0)
							bb_perror_msg("setrlimit");
					closelog();
					for (tmpint = rlim_ofile_cur - 1; --tmpint > 2;)
						(void) close(tmpint);
					sigaction(SIGPIPE, &sapipe, NULL);
					execv(sep->se_server, sep->se_argv);
					bb_perror_msg("execv %s", sep->se_server);
do_exit1:
					if (sep->se_socktype != SOCK_STREAM)
						recv(0, buf, sizeof(buf), 0);
					_exit(1);
				}
			}
			if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
				close(ctrl);
		} /* for (sep = servtab...) */
	} /* for (;;) */
}

/*
 * Internet services provided internally by inetd:
 */
#define BUFSIZE 4096

#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_ECHO || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN || \
	ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME
static int dg_badinput(struct sockaddr_in *dg_sin)
{
	if (ntohs(dg_sin->sin_port) < IPPORT_RESERVED)
		return 1;
	if (dg_sin->sin_addr.s_addr == htonl(INADDR_BROADCAST))
		return 1;
	/* XXX compare against broadcast addresses in SIOCGIFCONF list? */
	return 0;
}
#endif

#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_ECHO
/* Echo service -- echo data back */
/* ARGSUSED */
static void
echo_stream(int s, servtab_t *sep)
{
	char buffer[BUFSIZE];
	int i;

	inetd_setproctitle(sep->se_service, s);
	while (1) {
		i = read(s, buffer, sizeof(buffer));
		if (i <= 0) break;
		/* FIXME: this isnt correct - safe_write()? */
		if (write(s, buffer, i) <= 0) break;
	}
	exit(0);
}

/* Echo service -- echo data back */
/* ARGSUSED */
static void
echo_dg(int s, servtab_t *sep ATTRIBUTE_UNUSED)
{
	char buffer[BUFSIZE];
	int i;
	socklen_t size;
	/* struct sockaddr_storage ss; */
	struct sockaddr sa;

	size = sizeof(sa);
	i = recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size);
	if (i < 0)
		return;
	if (dg_badinput((struct sockaddr_in *) &sa))
		return;
	(void) sendto(s, buffer, i, 0, &sa, sizeof(sa));
}
#endif  /* FEATURE_INETD_SUPPORT_BUILTIN_ECHO */

#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DISCARD
/* Discard service -- ignore data */
/* ARGSUSED */
static void
discard_stream(int s, servtab_t *sep)
{
	char buffer[BUFSIZE];

	inetd_setproctitle(sep->se_service, s);
	while (1) {
		errno = 0;
		if (read(s, buffer, sizeof(buffer)) <= 0 && errno != EINTR)
			exit(0);
	}
}

/* Discard service -- ignore data */
/* ARGSUSED */
static void
discard_dg(int s, servtab_t *sep ATTRIBUTE_UNUSED)
{
	char buffer[BUFSIZE];

	(void) read(s, buffer, sizeof(buffer));
}
#endif /* FEATURE_INETD_SUPPORT_BUILTIN_DISCARD */


#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN
#define LINESIZ 72
static char ring[128];
static char *endring;

static void
initring(void)
{
	int i;

	endring = ring;

	for (i = 0; i <= 128; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* Character generator */
/* ARGSUSED */
static void
chargen_stream(int s, servtab_t *sep)
{
	char *rs;
	int len;
	char text[LINESIZ + 2];

	inetd_setproctitle(sep->se_service, s);

	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	rs = ring;
	for (;;) {
		len = endring - rs;
		if (len >= LINESIZ)
			memmove(text, rs, LINESIZ);
		else {
			memmove(text, rs, len);
			memmove(text + len, ring, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
	exit(0);
}

/* Character generator */
/* ARGSUSED */
static void
chargen_dg(int s, servtab_t *sep ATTRIBUTE_UNUSED)
{
	/* struct sockaddr_storage ss; */
	struct sockaddr sa;
	static char *rs;
	int len;
	char text[LINESIZ + 2];
	socklen_t size;

	if (endring == 0) {
		initring();
		rs = ring;
	}

	size = sizeof(sa);
	if (recvfrom(s, text, sizeof(text), 0, &sa, &size) < 0)
		return;
	if (dg_badinput((struct sockaddr_in *) &sa))
		return;

	if ((len = endring - rs) >= LINESIZ)
		memmove(text, rs, LINESIZ);
	else {
		memmove(text, rs, len);
		memmove(text + len, ring, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0, &sa, sizeof(sa));
}
#endif /* FEATURE_INETD_SUPPORT_BUILTIN_CHARGEN */


#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_TIME
/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

static unsigned machtime(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		fprintf(stderr, "Unable to get time of day\n");
		return 0L;
	}
	return htonl((unsigned) tv.tv_sec + 2208988800UL);
}

/* ARGSUSED */
static void
machtime_stream(int s, servtab_t *sep ATTRIBUTE_UNUSED)
{
	unsigned result;

	result = machtime();
	(void) write(s, (char *) &result, sizeof(result));
}

/* ARGSUSED */
static void
machtime_dg(int s, servtab_t *sep ATTRIBUTE_UNUSED)
{
	unsigned result;
	/* struct sockaddr_storage ss; */
	struct sockaddr sa;
	struct sockaddr_in *dg_sin;
	socklen_t size;

	size = sizeof(sa);
	if (recvfrom(s, (char *) &result, sizeof(result), 0, &sa, &size) < 0)
		return;
	/* if (dg_badinput((struct sockaddr *)&ss)) */
	dg_sin = (struct sockaddr_in *) &sa;
	if (dg_sin->sin_addr.s_addr == htonl(INADDR_BROADCAST) ||
			ntohs(dg_sin->sin_port) < IPPORT_RESERVED / 2)
		return;
	result = machtime();
	(void) sendto(s, (char *) &result, sizeof(result), 0, &sa, sizeof(sa));
}
#endif /* FEATURE_INETD_SUPPORT_BUILTIN_TIME */


#if ENABLE_FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME
/* Return human-readable time of day */
/* ARGSUSED */
static void daytime_stream(int s, servtab_t *sep ATTRIBUTE_UNUSED)
{
	char buffer[32];
	time_t t;

	t = time(NULL);

// fdprintf instead?
	(void) sprintf(buffer, "%.24s\r\n", ctime(&t));
	(void) write(s, buffer, strlen(buffer));
}

/* Return human-readable time of day */
/* ARGSUSED */
void
daytime_dg(int s, servtab_t *sep ATTRIBUTE_UNUSED)
{
	char buffer[256];
	time_t t;
	/* struct sockaddr_storage ss; */
	struct sockaddr sa;
	socklen_t size;

	t = time(NULL);

	size = sizeof(sa);
	if (recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size) < 0)
		return;
	if (dg_badinput((struct sockaddr_in *) &sa))
		return;
	(void) sprintf(buffer, "%.24s\r\n", ctime(&t));
	(void) sendto(s, buffer, strlen(buffer), 0, &sa, sizeof(sa));
}
#endif /* FEATURE_INETD_SUPPORT_BUILTIN_DAYTIME */
