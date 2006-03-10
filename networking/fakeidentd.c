/* vi: set sw=4 ts=4: */
/*
 * A fake identd server
 *
 * Adapted to busybox by Thomas Lundquist <thomasez@zelow.no>
 * Original Author: Tomi Ollila <too@iki.fi>
 *                  http://www.guru-group.fi/~too/sw/
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
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/syslog.h>

#include <pwd.h>
#include <netdb.h>

#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/uio.h>

#include "busybox.h"

#define IDENT_PORT  113
#define MAXCONNS    20
#define MAXIDLETIME 45

static const char ident_substr[] = " : USERID : UNIX : ";
enum { ident_substr_len = sizeof(ident_substr) - 1 };
#define PIDFILE "/var/run/identd.pid"

/*
 * We have to track the 'first connection socket' so that we
 * don't go around closing file descriptors for non-clients.
 *
 * descriptor setup normally
 *  0 = server socket
 *  1 = syslog fd (hopefully -- otherwise this won't work)
 *  2 = connection socket after detached from tty. standard error before that
 *  3 - 2 + MAXCONNS = rest connection sockets
 *
 * To try to make sure that syslog fd is what is "requested", the that fd
 * is closed before openlog() call.  It can only severely fail if fd 0
 * is initially closed.
 */
#define FCS 2

/*
 * FD of the connection is always the index of the connection structure
 * in `conns' array + FCS
 */
static struct {
	char buf[20];
	int len;
	time_t lasttime;
} conns[MAXCONNS];

/* When using global variables, bind those at least to a structure. */
static struct {
	const char *identuser;
	fd_set readfds;
	int conncnt;
} G;

/*
 * Prototypes
 */
static void reply(int s, char *buf);
static void replyError(int s, char *buf);

static const char *nobodystr = "nobody"; /* this needs to be declared like this */
static char *bind_ip_address = "0.0.0.0";

static inline void movefd(int from, int to)
{
	if (from != to) {
		dup2(from, to);
		close(from);
	}
}

static void inetbind(void)
{
	int s, port;
	struct sockaddr_in addr;
	int len = sizeof(addr);
	int one = 1;
	struct servent *se;

	if ((se = getservbyname("identd", "tcp")) == NULL)
		port = IDENT_PORT;
	else
		port = se->s_port;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		bb_perror_msg_and_die("Cannot create server socket");

	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = inet_addr(bind_ip_address);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (bind(s, (struct sockaddr *)&addr, len) < 0)
		bb_perror_msg_and_die("Cannot bind() port %i", IDENT_PORT);

	if (listen(s, 5) < 0)
		bb_perror_msg_and_die("Cannot listen() on port %i", IDENT_PORT);

	movefd(s, 0);
}

static void handlexitsigs(int signum)
{
	if (unlink(PIDFILE) < 0)
		close(open(PIDFILE, O_WRONLY|O_CREAT|O_TRUNC, 0644));
	exit(0);
}

/* May succeed. If not, won't care. */
static inline void writepid(uid_t nobody, uid_t nogrp)
{
	char buf[24];
	int fd = open(PIDFILE, O_WRONLY|O_CREAT|O_TRUNC, 0664);

	if (fd < 0)
		return;

	snprintf(buf, 23, "%d\n", getpid());
	write(fd, buf, strlen(buf));
	fchown(fd, nobody, nogrp);
	close(fd);

	/* should this handle ILL, ... (see signal(7)) */
	signal(SIGTERM, handlexitsigs);
	signal(SIGINT,  handlexitsigs);
	signal(SIGQUIT, handlexitsigs);
}

/* return 0 as parent, 1 as child */
static int godaemon(void)
{
	uid_t nobody, nogrp;
	struct passwd *pw;

	switch (fork()) {
	case -1:
		bb_perror_msg_and_die("Could not fork");

	case 0:
		pw = getpwnam(nobodystr);
		if (pw == NULL)
			bb_error_msg_and_die("Cannot find uid/gid of user '%s'", nobodystr);
		nobody = pw->pw_uid;
		nogrp = pw->pw_gid;
		writepid(nobody, nogrp);

		close(0);
		inetbind();
		if (setgid(nogrp))   bb_error_msg_and_die("Could not setgid()");
		if (setuid(nobody))  bb_error_msg_and_die("Could not setuid()");
		close(1);
		close(2);

		signal(SIGHUP, SIG_IGN);
		signal(SIGPIPE, SIG_IGN); /* connection closed when writing (raises ???) */

		setsid();

		openlog(bb_applet_name, 0, LOG_DAEMON);
		return 1;
	}

	return 0;
}

static void deleteConn(int s)
{
	int i = s - FCS;

	close(s);

	G.conncnt--;

	/*
	 * Most of the time there is 0 connections. Most often that there
	 * is connections, there is just one connection. When this one connection
	 * closes, i == G.conncnt = 0 -> no copying.
	 * When there is more than one connection, the oldest connections closes
	 * earlier on average. When this happens, the code below starts copying
	 * the connection structure w/ highest index to the place which which is
	 * just deleted. This means that the connection structures are no longer
	 * in chronological order. I'd quess this means that when there is more
	 * than 1 connection, on average every other connection structure needs
	 * to be copied over the time all these connections are deleted.
	 */
	if (i != G.conncnt) {
		memcpy(&conns[i], &conns[G.conncnt], sizeof(conns[0]));
		movefd(G.conncnt + FCS, s);
	}

	FD_CLR(G.conncnt + FCS, &G.readfds);
}

static int closeOldest(void)
{
	time_t min = conns[0].lasttime;
	int idx = 0;
	int i;

	for (i = 1; i < MAXCONNS; i++)
		if (conns[i].lasttime < min)
			idx = i;

	replyError(idx + FCS, "X-SERVER-TOO-BUSY");
	close(idx + FCS);

	return idx;
}

static int checkInput(char *buf, int len, int l)
{
	int i;
	for (i = len; i < len + l; ++i)
		if (buf[i] == '\n')
			return 1;
	return 0;
}

int fakeidentd_main(int argc, char **argv)
{
	memset(conns, 0, sizeof(conns));
	memset(&G, 0, sizeof(G));
	FD_ZERO(&G.readfds);
	FD_SET(0, &G.readfds);

	/* handle -b <ip> parameter */
	bb_getopt_ulflags(argc, argv, "b:", &bind_ip_address);
	/* handle optional REPLY STRING */
	if (optind < argc)
		G.identuser = argv[optind];
	else
		G.identuser = nobodystr;

	/* daemonize and have the parent return */
	if (godaemon() == 0)
		return 0;

	/* main loop where we process all events and never exit */
	while (1) {
	fd_set rfds = G.readfds;
	struct timeval tv = { 15, 0 };
	int i;
	int tim = time(NULL);

	select(G.conncnt + FCS, &rfds, NULL, NULL, G.conncnt? &tv: NULL);

	for (i = G.conncnt - 1; i >= 0; i--) {
		int s = i + FCS;

		if (FD_ISSET(s, &rfds)) {
			char *buf = conns[i].buf;
			unsigned int len = conns[i].len;
			unsigned int l;

			if ((l = read(s, buf + len, sizeof(conns[0].buf) - len)) > 0) {
				if (checkInput(buf, len, l)) {
					reply(s, buf);
					goto deleteconn;
				} else if (len + l >= sizeof(conns[0].buf)) {
					replyError(s, "X-INVALID-REQUEST");
					goto deleteconn;
				} else {
					conns[i].len += l;
				}
			} else {
				goto deleteconn;
			}

			conns[i].lasttime = tim;
			continue;

deleteconn:
			deleteConn(s);
		} else {
			/* implement as time_after() in linux kernel sources ... */
			if (conns[i].lasttime + MAXIDLETIME <= tim) {
				replyError(s, "X-TIMEOUT");
				deleteConn(s);
			}
		}
	}

	if (FD_ISSET(0, &rfds)) {
		int s = accept(0, NULL, 0);

		if (s < 0) {
			if (errno != EINTR) /* EINTR */
				syslog(LOG_ERR, "accept: %s", strerror(errno));
		} else {
			if (G.conncnt == MAXCONNS)
				i = closeOldest();
			else
				i = G.conncnt++;

			movefd(s, i + FCS); /* move if not already there */
			FD_SET(i + FCS, &G.readfds);

			conns[i].len = 0;
			conns[i].lasttime = time(NULL);
		}
	}
	} /* end of while(1) */

	return 0;
}

static int parseAddrs(char *ptr, char **myaddr, char **heraddr);
static void reply(int s, char *buf)
{
	char *myaddr, *heraddr;

	myaddr = heraddr = NULL;

	if (parseAddrs(buf, &myaddr, &heraddr))
		replyError(s, "X-INVALID-REQUEST");
	else {
		struct iovec iv[6];
		iv[0].iov_base = myaddr;               iv[0].iov_len = strlen(myaddr);
		iv[1].iov_base = ", ";                 iv[1].iov_len = 2;
		iv[2].iov_base = heraddr;              iv[2].iov_len = strlen(heraddr);
		iv[3].iov_base = (void *)ident_substr; iv[3].iov_len = ident_substr_len;
		iv[4].iov_base = (void *)G.identuser;  iv[4].iov_len = strlen(G.identuser);
		iv[5].iov_base = "\r\n";               iv[5].iov_len = 2;
		writev(s, iv, 6);
	}
}

static void replyError(int s, char *buf)
{
	struct iovec iv[3];
	iv[0].iov_base = "0, 0 : ERROR : ";   iv[0].iov_len = 15;
	iv[1].iov_base = buf;                 iv[1].iov_len = strlen(buf);
	iv[2].iov_base = "\r\n";              iv[2].iov_len = 2;
	writev(s, iv, 3);
}

static int chmatch(char c, char *chars)
{
	for (; *chars; chars++)
		if (c == *chars)
			return 1;
	return 0;
}

static int skipchars(char **p, char *chars)
{
	while (chmatch(**p, chars))
		(*p)++;
	if (**p == '\r' || **p == '\n')
		return 0;
	return 1;
}

static int parseAddrs(char *ptr, char **myaddr, char **heraddr)
{
	/* parse <port-on-server> , <port-on-client> */

	if (!skipchars(&ptr, " \t"))
		return -1;

	*myaddr = ptr;

	if (!skipchars(&ptr, "1234567890"))
		return -1;

	if (!chmatch(*ptr, " \t,"))
		return -1;

	*ptr++ = '\0';

	if (!skipchars(&ptr, " \t,") )
		return -1;

	*heraddr = ptr;

	skipchars(&ptr, "1234567890");

	if (!chmatch(*ptr, " \n\r"))
		return -1;

	*ptr = '\0';

	return 0;
}
