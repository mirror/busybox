/* NB: this file is to be removed soon. See isrv_identd.c */

/* vi: set sw=4 ts=4: */
/*
 * A fake identd server
 *
 * Adapted to busybox by Thomas Lundquist <thomasez@zelow.no>
 * Original Author: Tomi Ollila <too@iki.fi>
 *                  http://www.guru-group.fi/~too/sw/
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Ident crash course
 *
 * Incoming requests are of form "6191, 23\r\n" - peer asks us
 * "which user connected from your port 6191 to my port 23?"
 * We should answer:
 * "6193, 23 : USERID : UNIX : username\r\n"
 * and close the connection.
 * We can also reply:
 * "6195, 23 : USERID : OTHER[,US-ASCII] : username\r\n"
 * "6195, 23 : ERROR : INVALID-PORT/NO-USER/HIDDEN-USER/UNKNOWN-ERROR\r\n"
 * but we probably will never want that.
 */

#include "busybox.h"

#define SANE_INETD_ONLY_VERSION

#ifdef SANE_INETD_ONLY_VERSION

int fakeidentd_main(int argc, char **argv)
{
	char buf[64];
	const char *bogouser = "nobody";
	char *cur = buf;
	int rem = sizeof(buf)-1;

	if (argv[1])
		bogouser = argv[1];

	alarm(30);
	while (1) {
		char *p;
		int sz = safe_read(0, cur, rem);
		if (sz < 0) return 1;
		cur[sz] = '\0';
		p = strpbrk(cur, "\r\n");
		if (p) {
			*p = '\0';
			break;
		}
		cur += sz;
		rem -= sz;
		if (!rem || !sz)
			break;
	}
	printf("%s : USERID : UNIX : %s\r\n", buf, bogouser);
	return 0;
}

#else

/* Welcome to the bloaty horrors */

#include <sys/syslog.h>
#include <sys/uio.h>

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
	time_t lasttime;
	int len;
	char buf[20];
} conns[MAXCONNS];

/* When using global variables, bind those at least to a structure. */
static struct {
	const char *identuser;
	fd_set readfds;
	int conncnt;
} G;

static char *bind_ip_address;

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

static void replyError(int s, char *buf)
{
	struct iovec iv[3];
	iv[0].iov_base = "0, 0 : ERROR : ";   iv[0].iov_len = 15;
	iv[1].iov_base = buf;                 iv[1].iov_len = strlen(buf);
	iv[2].iov_base = "\r\n";              iv[2].iov_len = 2;
	writev(s, iv, 3);
}

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

static void movefd(int from, int to)
{
	if (from != to) {
		dup2(from, to);
		close(from);
	}
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

/* May succeed. If not, won't care. */
static const char *to_unlink;
static void writepid(void)
{
	int fd = open(PIDFILE, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd < 0)
		return;
	to_unlink = PIDFILE;
	fdprintf(fd, "%d\n", getpid());
	close(fd);
}

static void handlexitsigs(int signum)
{
	if (to_unlink)
		if (unlink(to_unlink) < 0)
			close(open(to_unlink, O_WRONLY|O_CREAT|O_TRUNC, 0644));
	exit(0);
}

int fakeidentd_main(int argc, char **argv)
{
	int fd;
	pid_t pid;

	/* FD_ZERO(&G.readfds); - in bss, already zeroed */
	FD_SET(0, &G.readfds);

	/* handle -b <ip> parameter */
	getopt32(argc, argv, "b:", &bind_ip_address);
	/* handle optional REPLY STRING */
	if (optind < argc)
		G.identuser = argv[optind];
	else
		G.identuser = "nobody";

	writepid();
	signal(SIGTERM, handlexitsigs);
	signal(SIGINT,  handlexitsigs);
	signal(SIGQUIT, handlexitsigs);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN); /* ignore closed connections when writing */

	fd = create_and_bind_stream_or_die(bind_ip_address, bb_lookup_port("identd", "tcp", 113));
	xlisten(fd, 5);

	pid = fork();
	if (pid < 0)
		bb_perror_msg_and_die("fork");
	if (pid != 0) /* parent */
		exit(0);
	/* child */
	setsid();
	movefd(fd, 0);
	while (fd)
		close(fd--);
	openlog(applet_name, 0, LOG_DAEMON);
	logmode = LOGMODE_SYSLOG;

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
				unsigned len = conns[i].len;
				unsigned l;

				l = read(s, buf + len, sizeof(conns[0].buf) - len);
				if (l > 0) {
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
				if (errno != EINTR)
					bb_perror_msg("accept");
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
	} /* end of while (1) */

	return 0;
}

#endif /* !SANE_INETD_ONLY_VERSION */
