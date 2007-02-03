/* vi: set sw=4 ts=4: */
/*
 * Simple telnet server
 * Bjorn Wesen, Axis Communications AB (bjornw@axis.com)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * ---------------------------------------------------------------------------
 * (C) Copyright 2000, Axis Communications AB, LUND, SWEDEN
 ****************************************************************************
 *
 * The telnetd manpage says it all:
 *
 *   Telnetd operates by allocating a pseudo-terminal device (see pty(4))  for
 *   a client, then creating a login process which has the slave side of the
 *   pseudo-terminal as stdin, stdout, and stderr. Telnetd manipulates the
 *   master side of the pseudo-terminal, implementing the telnet protocol and
 *   passing characters between the remote client and the login process.
 *
 * Vladimir Oleynik <dzo@simtreas.ru> 2001
 *     Set process group corrections, initial busybox port
 */

/*#define DEBUG 1 */
#define DEBUG 0

#include "busybox.h"

#if DEBUG
#define TELCMDS
#define TELOPTS
#endif
#include <arpa/telnet.h>
#include <sys/syslog.h>


#define BUFSIZE 4000

#if ENABLE_LOGIN
static const char *loginpath = "/bin/login";
#else
static const char *loginpath = DEFAULT_SHELL;
#endif

static const char *issuefile = "/etc/issue.net";

/* shell name and arguments */

static const char *argv_init[2];

/* structure that describes a session */

struct tsession {
	struct tsession *next;
	int sockfd_read, sockfd_write, ptyfd;
	int shell_pid;
	/* two circular buffers */
	char *buf1, *buf2;
	int rdidx1, wridx1, size1;
	int rdidx2, wridx2, size2;
};

/*
   This is how the buffers are used. The arrows indicate the movement
   of data.

   +-------+     wridx1++     +------+     rdidx1++     +----------+
   |       | <--------------  | buf1 | <--------------  |          |
   |       |     size1--      +------+     size1++      |          |
   |  pty  |                                            |  socket  |
   |       |     rdidx2++     +------+     wridx2++     |          |
   |       |  --------------> | buf2 |  --------------> |          |
   +-------+     size2++      +------+     size2--      +----------+

   Each session has got two buffers.
*/

static int maxfd;

static struct tsession *sessions;


/*
   Remove all IAC's from the buffer pointed to by bf (received IACs are ignored
   and must be removed so as to not be interpreted by the terminal).  Make an
   uninterrupted string of characters fit for the terminal.  Do this by packing
   all characters meant for the terminal sequentially towards the end of bf.

   Return a pointer to the beginning of the characters meant for the terminal.
   and make *num_totty the number of characters that should be sent to
   the terminal.

   Note - If an IAC (3 byte quantity) starts before (bf + len) but extends
   past (bf + len) then that IAC will be left unprocessed and *processed will be
   less than len.

   FIXME - if we mean to send 0xFF to the terminal then it will be escaped,
   what is the escape character?  We aren't handling that situation here.

   CR-LF ->'s CR mapping is also done here, for convenience
 */
static char *
remove_iacs(struct tsession *ts, int *pnum_totty)
{
	unsigned char *ptr0 = (unsigned char *)ts->buf1 + ts->wridx1;
	unsigned char *ptr = ptr0;
	unsigned char *totty = ptr;
	unsigned char *end = ptr + MIN(BUFSIZE - ts->wridx1, ts->size1);
	int processed;
	int num_totty;

	while (ptr < end) {
		if (*ptr != IAC) {
			int c = *ptr;
			*totty++ = *ptr++;
			/* We now map \r\n ==> \r for pragmatic reasons.
			 * Many client implementations send \r\n when
			 * the user hits the CarriageReturn key.
			 */
			if (c == '\r' && (*ptr == '\n' || *ptr == 0) && ptr < end)
				ptr++;
		} else {
			/*
			 * TELOPT_NAWS support!
			 */
			if ((ptr+2) >= end) {
				/* only the beginning of the IAC is in the
				buffer we were asked to process, we can't
				process this char. */
				break;
			}

			/*
			 * IAC -> SB -> TELOPT_NAWS -> 4-byte -> IAC -> SE
			 */
			else if (ptr[1] == SB && ptr[2] == TELOPT_NAWS) {
				struct winsize ws;
				if ((ptr+8) >= end)
					break;	/* incomplete, can't process */
				ws.ws_col = (ptr[3] << 8) | ptr[4];
				ws.ws_row = (ptr[5] << 8) | ptr[6];
				ioctl(ts->ptyfd, TIOCSWINSZ, (char *)&ws);
				ptr += 9;
			} else {
				/* skip 3-byte IAC non-SB cmd */
#if DEBUG
				fprintf(stderr, "Ignoring IAC %s,%s\n",
					TELCMD(ptr[1]), TELOPT(ptr[2]));
#endif
				ptr += 3;
			}
		}
	}

	processed = ptr - ptr0;
	num_totty = totty - ptr0;
	/* the difference between processed and num_to tty
	   is all the iacs we removed from the stream.
	   Adjust buf1 accordingly. */
	ts->wridx1 += processed - num_totty;
	ts->size1 -= processed - num_totty;
	*pnum_totty = num_totty;
	/* move the chars meant for the terminal towards the end of the
	buffer. */
	return memmove(ptr - num_totty, ptr0, num_totty);
}


static int
getpty(char *line, int size)
{
	int p;
#if ENABLE_FEATURE_DEVPTS
	p = open("/dev/ptmx", O_RDWR);
	if (p > 0) {
		const char *name;
		grantpt(p);
		unlockpt(p);
		name = ptsname(p);
		if (!name) {
			bb_perror_msg("ptsname error (is /dev/pts mounted?)");
			return -1;
		}
		safe_strncpy(line, name, size);
		return p;
	}
#else
	struct stat stb;
	int i;
	int j;

	strcpy(line, "/dev/ptyXX");

	for (i = 0; i < 16; i++) {
		line[8] = "pqrstuvwxyzabcde"[i];
		line[9] = '0';
		if (stat(line, &stb) < 0) {
			continue;
		}
		for (j = 0; j < 16; j++) {
			line[9] = j < 10 ? j + '0' : j - 10 + 'a';
			if (DEBUG)
				fprintf(stderr, "Trying to open device: %s\n", line);
			p = open(line, O_RDWR | O_NOCTTY);
			if (p >= 0) {
				line[5] = 't';
				return p;
			}
		}
	}
#endif /* FEATURE_DEVPTS */
	return -1;
}


static void
send_iac(struct tsession *ts, unsigned char command, int option)
{
	/* We rely on that there is space in the buffer for now. */
	char *b = ts->buf2 + ts->rdidx2;
	*b++ = IAC;
	*b++ = command;
	*b++ = option;
	ts->rdidx2 += 3;
	ts->size2 += 3;
}


static struct tsession *
make_new_session(
		USE_FEATURE_TELNETD_STANDALONE(int sock_r, int sock_w)
		SKIP_FEATURE_TELNETD_STANDALONE(void)
) {
	struct termios termbuf;
	int fd, pid;
	char tty_name[32];
	struct tsession *ts = xzalloc(sizeof(struct tsession) + BUFSIZE * 2);

	ts->buf1 = (char *)(&ts[1]);
	ts->buf2 = ts->buf1 + BUFSIZE;

	/* Got a new connection, set up a tty. */
	fd = getpty(tty_name, 32);
	if (fd < 0) {
		bb_error_msg("all terminals in use");
		return NULL;
	}
	if (fd > maxfd) maxfd = fd;
	ndelay_on(ts->ptyfd = fd);
#if ENABLE_FEATURE_TELNETD_STANDALONE
	if (sock_w > maxfd) maxfd = sock_w;
	if (sock_r > maxfd) maxfd = sock_r;
	ndelay_on(ts->sockfd_write = sock_w);
	ndelay_on(ts->sockfd_read = sock_r);
#else
	ts->sockfd_write = 1;
	/* xzalloc: ts->sockfd_read = 0; */
	ndelay_on(0);
	ndelay_on(1);
#endif
	/* Make the telnet client understand we will echo characters so it
	 * should not do it locally. We don't tell the client to run linemode,
	 * because we want to handle line editing and tab completion and other
	 * stuff that requires char-by-char support. */
	send_iac(ts, DO, TELOPT_ECHO);
	send_iac(ts, DO, TELOPT_NAWS);
	send_iac(ts, DO, TELOPT_LFLOW);
	send_iac(ts, WILL, TELOPT_ECHO);
	send_iac(ts, WILL, TELOPT_SGA);

	pid = fork();
	if (pid < 0) {
		free(ts);
		close(fd);
		bb_perror_msg("fork");
		return NULL;
	}
	if (pid > 0) {
		/* parent */
		ts->shell_pid = pid;
		return ts;
	}

	/* child */

	/* make new process group */
	setsid();
	tcsetpgrp(0, getpid());
	/* ^^^ strace says: "ioctl(0, TIOCSPGRP, [pid]) = -1 ENOTTY" -- ??! */

	/* open the child's side of the tty. */
	/* NB: setsid() disconnects from any previous ctty's. Therefore
	 * we must open child's side of the tty AFTER setsid! */
	fd = xopen(tty_name, O_RDWR); /* becomes our ctty */
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	while (fd > 2) close(fd--);

	/* The pseudo-terminal allocated to the client is configured to operate in
	 * cooked mode, and with XTABS CRMOD enabled (see tty(4)). */
	tcgetattr(0, &termbuf);
	termbuf.c_lflag |= ECHO; /* if we use readline we dont want this */
	termbuf.c_oflag |= ONLCR|XTABS;
	termbuf.c_iflag |= ICRNL;
	termbuf.c_iflag &= ~IXOFF;
	/*termbuf.c_lflag &= ~ICANON;*/
	tcsetattr(0, TCSANOW, &termbuf);

	print_login_issue(issuefile, NULL);

	/* exec shell, with correct argv and env */
	execv(loginpath, (char *const *)argv_init);
	bb_perror_msg_and_die("execv");
}

#if ENABLE_FEATURE_TELNETD_STANDALONE

static void
free_session(struct tsession *ts)
{
	struct tsession *t = sessions;

	/* unlink this telnet session from the session list */
	if (t == ts)
		sessions = ts->next;
	else {
		while (t->next != ts)
			t = t->next;
		t->next = ts->next;
	}

	kill(ts->shell_pid, SIGKILL);
	wait4(ts->shell_pid, NULL, 0, NULL);
	close(ts->ptyfd);
	close(ts->sockfd_read);
	/* error if ts->sockfd_read == ts->sockfd_write. So what? ;) */
	close(ts->sockfd_write);
	free(ts);

	/* scan all sessions and find new maxfd */
	ts = sessions;
	maxfd = 0;
	while (ts) {
		if (maxfd < ts->ptyfd)
			maxfd = ts->ptyfd;
		if (maxfd < ts->sockfd_read)
			maxfd = ts->sockfd_read;
		if (maxfd < ts->sockfd_write)
			maxfd = ts->sockfd_write;
		ts = ts->next;
	}
}

#else /* !FEATURE_TELNETD_STANDALONE */

/* Never actually called */
void free_session(struct tsession *ts);

#endif


int telnetd_main(int argc, char **argv);
int telnetd_main(int argc, char **argv)
{
	fd_set rdfdset, wrfdset;
	unsigned opt;
	int selret, maxlen, w, r;
	struct tsession *ts;
#if ENABLE_FEATURE_TELNETD_STANDALONE
#define IS_INETD (opt & OPT_INETD)
	int master_fd = -1; /* be happy, gcc */
	unsigned portnbr = 23;
	char *opt_bindaddr = NULL;
	char *opt_portnbr;
#else
	enum {
		IS_INETD = 1,
		master_fd = -1,
		portnbr = 23,
	};
#endif
	enum {
		OPT_PORT = 4 * ENABLE_FEATURE_TELNETD_STANDALONE,
		OPT_FOREGROUND = 0x10 * ENABLE_FEATURE_TELNETD_STANDALONE,
		OPT_INETD = 0x20 * ENABLE_FEATURE_TELNETD_STANDALONE,
	};

	opt = getopt32(argc, argv, "f:l:" USE_FEATURE_TELNETD_STANDALONE("p:b:Fi"),
			&issuefile, &loginpath
			USE_FEATURE_TELNETD_STANDALONE(, &opt_portnbr, &opt_bindaddr));
	/* Redirect log to syslog early, if needed */
	if (IS_INETD || !(opt & OPT_FOREGROUND)) {
		openlog(applet_name, 0, LOG_USER);
		logmode = LOGMODE_SYSLOG;
	}
	//if (opt & 1) // -f
	//if (opt & 2) // -l
	USE_FEATURE_TELNETD_STANDALONE(
		if (opt & OPT_PORT) // -p
			portnbr = xatou16(opt_portnbr);
		//if (opt & 8) // -b
		//if (opt & 0x10) // -F
		//if (opt & 0x20) // -i
	);

	/* Used to check access(loginpath, X_OK) here. Pointless.
	 * exec will do this for us for free later. */
	argv_init[0] = loginpath;

#if ENABLE_FEATURE_TELNETD_STANDALONE
	if (IS_INETD) {
		sessions = make_new_session(0, 1);
	} else {
		master_fd = create_and_bind_stream_or_die(opt_bindaddr, portnbr);
		xlisten(master_fd, 1);
		if (!(opt & OPT_FOREGROUND))
			xdaemon(0, 0);
	}
#else
	sessions = make_new_session();
#endif

	/* We don't want to die if just one session is broken */
	signal(SIGPIPE, SIG_IGN);

 again:
	FD_ZERO(&rdfdset);
	FD_ZERO(&wrfdset);
	if (!IS_INETD) {
		FD_SET(master_fd, &rdfdset);
		/* This is needed because free_session() does not
		 * take into account master_fd when it finds new
		 * maxfd among remaining fd's: */
		if (master_fd > maxfd)
			maxfd = master_fd;
	}

	/* select on the master socket, all telnet sockets and their
	 * ptys if there is room in their session buffers. */
	ts = sessions;
	while (ts) {
		/* buf1 is used from socket to pty
		 * buf2 is used from pty to socket */
		if (ts->size1 > 0)       /* can write to pty */
			FD_SET(ts->ptyfd, &wrfdset);
		if (ts->size1 < BUFSIZE) /* can read from socket */
			FD_SET(ts->sockfd_read, &rdfdset);
		if (ts->size2 > 0)       /* can write to socket */
			FD_SET(ts->sockfd_write, &wrfdset);
		if (ts->size2 < BUFSIZE) /* can read from pty */
			FD_SET(ts->ptyfd, &rdfdset);
		ts = ts->next;
	}

	selret = select(maxfd + 1, &rdfdset, &wrfdset, 0, 0);
	if (!selret)
		return 0;

#if ENABLE_FEATURE_TELNETD_STANDALONE
	/* First check for and accept new sessions. */
	if (!IS_INETD && FD_ISSET(master_fd, &rdfdset)) {
		int fd;
		struct tsession *new_ts;

		fd = accept(master_fd, NULL, 0);
		if (fd < 0)
			goto again;
		/* Create a new session and link it into our active list */
		new_ts = make_new_session(fd, fd);
		if (new_ts) {
			new_ts->next = sessions;
			sessions = new_ts;
		} else {
			close(fd);
		}
	}
#endif

	/* Then check for data tunneling. */
	ts = sessions;
	while (ts) { /* For all sessions... */
		struct tsession *next = ts->next; /* in case we free ts. */

		if (ts->size1 && FD_ISSET(ts->ptyfd, &wrfdset)) {
			int num_totty;
			char *ptr;
			/* Write to pty from buffer 1. */
			ptr = remove_iacs(ts, &num_totty);
			w = safe_write(ts->ptyfd, ptr, num_totty);
			/* needed? if (w < 0 && errno == EAGAIN) continue; */
			if (w < 0) {
				if (IS_INETD)
					return 0;
				free_session(ts);
				ts = next;
				continue;
			}
			ts->wridx1 += w;
			ts->size1 -= w;
			if (ts->wridx1 == BUFSIZE)
				ts->wridx1 = 0;
		}

		if (ts->size2 && FD_ISSET(ts->sockfd_write, &wrfdset)) {
			/* Write to socket from buffer 2. */
			maxlen = MIN(BUFSIZE - ts->wridx2, ts->size2);
			w = safe_write(ts->sockfd_write, ts->buf2 + ts->wridx2, maxlen);
			/* needed? if (w < 0 && errno == EAGAIN) continue; */
			if (w < 0) {
				if (IS_INETD)
					return 0;
				free_session(ts);
				ts = next;
				continue;
			}
			ts->wridx2 += w;
			ts->size2 -= w;
			if (ts->wridx2 == BUFSIZE)
				ts->wridx2 = 0;
		}

		if (ts->size1 < BUFSIZE && FD_ISSET(ts->sockfd_read, &rdfdset)) {
			/* Read from socket to buffer 1. */
			maxlen = MIN(BUFSIZE - ts->rdidx1, BUFSIZE - ts->size1);
			r = safe_read(ts->sockfd_read, ts->buf1 + ts->rdidx1, maxlen);
			if (r < 0 && errno == EAGAIN) continue;
			if (r <= 0) {
				if (IS_INETD)
					return 0;
				free_session(ts);
				ts = next;
				continue;
			}
			if (!ts->buf1[ts->rdidx1 + r - 1])
				if (!--r)
					continue;
			ts->rdidx1 += r;
			ts->size1 += r;
			if (ts->rdidx1 == BUFSIZE)
				ts->rdidx1 = 0;
		}

		if (ts->size2 < BUFSIZE && FD_ISSET(ts->ptyfd, &rdfdset)) {
			/* Read from pty to buffer 2. */
			maxlen = MIN(BUFSIZE - ts->rdidx2, BUFSIZE - ts->size2);
			r = safe_read(ts->ptyfd, ts->buf2 + ts->rdidx2, maxlen);
			if (r < 0 && errno == EAGAIN) continue;
			if (r <= 0) {
				if (IS_INETD)
					return 0;
				free_session(ts);
				ts = next;
				continue;
			}
			ts->rdidx2 += r;
			ts->size2 += r;
			if (ts->rdidx2 == BUFSIZE)
				ts->rdidx2 = 0;
		}

		if (ts->size1 == 0) {
			ts->rdidx1 = 0;
			ts->wridx1 = 0;
		}
		if (ts->size2 == 0) {
			ts->rdidx2 = 0;
			ts->wridx2 = 0;
		}
		ts = next;
	}
	goto again;
}
