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

#define DEBUG 0

#include "libbb.h"
#include <syslog.h>

#if DEBUG
#define TELCMDS
#define TELOPTS
#endif
#include <arpa/telnet.h>

/* Structure that describes a session */
struct tsession {
	struct tsession *next;
	int sockfd_read, sockfd_write, ptyfd;
	int shell_pid;

	/* two circular buffers */
	/*char *buf1, *buf2;*/
/*#define TS_BUF1 ts->buf1*/
/*#define TS_BUF2 TS_BUF2*/
#define TS_BUF1 ((unsigned char*)(ts + 1))
#define TS_BUF2 (((unsigned char*)(ts + 1)) + BUFSIZE)
	int rdidx1, wridx1, size1;
	int rdidx2, wridx2, size2;
};

/* Two buffers are directly after tsession in malloced memory.
 * Make whole thing fit in 4k */
enum { BUFSIZE = (4 * 1024 - sizeof(struct tsession)) / 2 };


/* Globals */
static int maxfd;
static struct tsession *sessions;
static const char *loginpath = "/bin/login";
static const char *issuefile = "/etc/issue.net";


/*
   Remove all IAC's from buf1 (received IACs are ignored and must be removed
   so as to not be interpreted by the terminal).  Make an uninterrupted
   string of characters fit for the terminal.  Do this by packing
   all characters meant for the terminal sequentially towards the end of buf.

   Return a pointer to the beginning of the characters meant for the terminal.
   and make *num_totty the number of characters that should be sent to
   the terminal.

   Note - If an IAC (3 byte quantity) starts before (bf + len) but extends
   past (bf + len) then that IAC will be left unprocessed and *processed
   will be less than len.

   FIXME - if we mean to send 0xFF to the terminal then it will be escaped,
   what is the escape character?  We aren't handling that situation here.

   CR-LF ->'s CR mapping is also done here, for convenience.

   NB: may fail to remove iacs which wrap around buffer!
 */
static unsigned char *
remove_iacs(struct tsession *ts, int *pnum_totty)
{
	unsigned char *ptr0 = TS_BUF1 + ts->wridx1;
	unsigned char *ptr = ptr0;
	unsigned char *totty = ptr;
	unsigned char *end = ptr + MIN(BUFSIZE - ts->wridx1, ts->size1);
	int num_totty;

	while (ptr < end) {
		if (*ptr != IAC) {
			char c = *ptr;

			*totty++ = c;
			ptr++;
			/* We now map \r\n ==> \r for pragmatic reasons.
			 * Many client implementations send \r\n when
			 * the user hits the CarriageReturn key.
			 */
			if (c == '\r' && ptr < end && (*ptr == '\n' || *ptr == '\0'))
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

	num_totty = totty - ptr0;
	*pnum_totty = num_totty;
	/* the difference between ptr and totty is number of iacs
	   we removed from the stream. Adjust buf1 accordingly. */
	if ((ptr - totty) == 0) /* 99.999% of cases */
		return ptr0;
	ts->wridx1 += ptr - totty;
	ts->size1 -= ptr - totty;
	/* move chars meant for the terminal towards the end of the buffer */
	return memmove(ptr - num_totty, ptr0, num_totty);
}


static struct tsession *
make_new_session(
		USE_FEATURE_TELNETD_STANDALONE(int sock)
		SKIP_FEATURE_TELNETD_STANDALONE(void)
) {
	const char *login_argv[2];
	struct termios termbuf;
	int fd, pid;
	char tty_name[GETPTY_BUFSIZE];
	struct tsession *ts = xzalloc(sizeof(struct tsession) + BUFSIZE * 2);

	/*ts->buf1 = (char *)(ts + 1);*/
	/*ts->buf2 = ts->buf1 + BUFSIZE;*/

	/* Got a new connection, set up a tty. */
	fd = xgetpty(tty_name);
	if (fd > maxfd)
		maxfd = fd;
	ts->ptyfd = fd;
	ndelay_on(fd);
#if ENABLE_FEATURE_TELNETD_STANDALONE
	ts->sockfd_read = sock;
	ndelay_on(sock);
	if (!sock) { /* We are called with fd 0 - we are in inetd mode */
		sock++; /* so use fd 1 for output */
		ndelay_on(sock);
	}
	ts->sockfd_write = sock;
	if (sock > maxfd)
		maxfd = sock;
#else
	/* ts->sockfd_read = 0; - done by xzalloc */
	ts->sockfd_write = 1;
	ndelay_on(0);
	ndelay_on(1);
#endif
	/* Make the telnet client understand we will echo characters so it
	 * should not do it locally. We don't tell the client to run linemode,
	 * because we want to handle line editing and tab completion and other
	 * stuff that requires char-by-char support. */
	{
		static const char iacs_to_send[] ALIGN1 = {
			IAC, DO, TELOPT_ECHO,
			IAC, DO, TELOPT_NAWS,
			IAC, DO, TELOPT_LFLOW,
			IAC, WILL, TELOPT_ECHO,
			IAC, WILL, TELOPT_SGA
		};
		memcpy(TS_BUF2, iacs_to_send, sizeof(iacs_to_send));
		ts->rdidx2 = sizeof(iacs_to_send);
		ts->size2 = sizeof(iacs_to_send);
	}

	fflush(NULL); /* flush all streams */
	pid = vfork(); /* NOMMU-friendly */
	if (pid < 0) {
		free(ts);
		close(fd);
		/* sock will be closed by caller */
		bb_perror_msg("vfork");
		return NULL;
	}
	if (pid > 0) {
		/* Parent */
		ts->shell_pid = pid;
		return ts;
	}

	/* Child */
	/* Careful - we are after vfork! */

	/* make new session and process group */
	setsid();

	/* Restore default signal handling */
	bb_signals((1 << SIGCHLD) + (1 << SIGPIPE), SIG_DFL);

	/* open the child's side of the tty. */
	/* NB: setsid() disconnects from any previous ctty's. Therefore
	 * we must open child's side of the tty AFTER setsid! */
	fd = xopen(tty_name, O_RDWR); /* becomes our ctty */
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	while (fd > 2) close(fd--);
	tcsetpgrp(0, getpid()); /* switch this tty's process group to us */

	/* The pseudo-terminal allocated to the client is configured to operate in
	 * cooked mode, and with XTABS CRMOD enabled (see tty(4)). */
	tcgetattr(0, &termbuf);
	termbuf.c_lflag |= ECHO; /* if we use readline we dont want this */
	termbuf.c_oflag |= ONLCR | XTABS;
	termbuf.c_iflag |= ICRNL;
	termbuf.c_iflag &= ~IXOFF;
	/*termbuf.c_lflag &= ~ICANON;*/
	tcsetattr(0, TCSANOW, &termbuf);

	/* Uses FILE-based I/O to stdout, but does fflush(stdout),
	 * so should be safe with vfork.
	 * I fear, though, that some users will have ridiculously big
	 * issue files, and they may block writing to fd 1,
	 * (parent is supposed to read it, but parent waits
	 * for vforked child to exec!) */
	print_login_issue(issuefile, NULL);

	/* Exec shell / login / whatever */
	login_argv[0] = loginpath;
	login_argv[1] = NULL;
	/* exec busybox applet (if PREFER_APPLETS=y), if that fails,
	 * exec external program */
	BB_EXECVP(loginpath, (char **)login_argv);
	/* _exit is safer with vfork, and we shouldn't send message
	 * to remote clients anyway */
	_exit(EXIT_FAILURE); /*bb_perror_msg_and_die("execv %s", loginpath);*/
}

/* Must match getopt32 string */
enum {
	OPT_WATCHCHILD = (1 << 2), /* -K */
	OPT_INETD      = (1 << 3) * ENABLE_FEATURE_TELNETD_STANDALONE, /* -i */
	OPT_PORT       = (1 << 4) * ENABLE_FEATURE_TELNETD_STANDALONE, /* -p */
	OPT_FOREGROUND = (1 << 6) * ENABLE_FEATURE_TELNETD_STANDALONE, /* -F */
};

#if ENABLE_FEATURE_TELNETD_STANDALONE

static void
free_session(struct tsession *ts)
{
	struct tsession *t = sessions;

	if (option_mask32 & OPT_INETD)
		exit(EXIT_SUCCESS);

	/* Unlink this telnet session from the session list */
	if (t == ts)
		sessions = ts->next;
	else {
		while (t->next != ts)
			t = t->next;
		t->next = ts->next;
	}

#if 0
	/* It was said that "normal" telnetd just closes ptyfd,
	 * doesn't send SIGKILL. When we close ptyfd,
	 * kernel sends SIGHUP to processes having slave side opened. */
	kill(ts->shell_pid, SIGKILL);
	wait4(ts->shell_pid, NULL, 0, NULL);
#endif
	close(ts->ptyfd);
	close(ts->sockfd_read);
	/* We do not need to close(ts->sockfd_write), it's the same
	 * as sockfd_read unless we are in inetd mode. But in inetd mode
	 * we do not reach this */
	free(ts);

	/* Scan all sessions and find new maxfd */
	maxfd = 0;
	ts = sessions;
	while (ts) {
		if (maxfd < ts->ptyfd)
			maxfd = ts->ptyfd;
		if (maxfd < ts->sockfd_read)
			maxfd = ts->sockfd_read;
#if 0
		/* Again, sockfd_write == sockfd_read here */
		if (maxfd < ts->sockfd_write)
			maxfd = ts->sockfd_write;
#endif
		ts = ts->next;
	}
}

#else /* !FEATURE_TELNETD_STANDALONE */

/* Used in main() only, thus "return 0" actually is exit(EXIT_SUCCESS). */
#define free_session(ts) return 0

#endif

static void handle_sigchld(int sig ATTRIBUTE_UNUSED)
{
	pid_t pid;
	struct tsession *ts;

	/* Looping: more than one child may have exited */
	while (1) {
		pid = wait_any_nohang(NULL);
		if (pid <= 0)
			break;
		ts = sessions;
		while (ts) {
			if (ts->shell_pid == pid) {
				ts->shell_pid = -1;
				break;
			}
			ts = ts->next;
		}
	}
}

int telnetd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int telnetd_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	fd_set rdfdset, wrfdset;
	unsigned opt;
	int count;
	struct tsession *ts;
#if ENABLE_FEATURE_TELNETD_STANDALONE
#define IS_INETD (opt & OPT_INETD)
	int master_fd = master_fd; /* be happy, gcc */
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
	/* Even if !STANDALONE, we accept (and ignore) -i, thus people
	 * don't need to guess whether it's ok to pass -i to us */
	opt = getopt32(argv, "f:l:Ki" USE_FEATURE_TELNETD_STANDALONE("p:b:F"),
			&issuefile, &loginpath
			USE_FEATURE_TELNETD_STANDALONE(, &opt_portnbr, &opt_bindaddr));
	if (!IS_INETD /*&& !re_execed*/) {
		/* inform that we start in standalone mode?
		 * May be useful when people forget to give -i */
		/*bb_error_msg("listening for connections");*/
		if (!(opt & OPT_FOREGROUND)) {
			/* DAEMON_CHDIR_ROOT was giving inconsistent
			 * behavior with/without -F, -i */
			bb_daemonize_or_rexec(0 /*was DAEMON_CHDIR_ROOT*/, argv);
		}
	}
	/* Redirect log to syslog early, if needed */
	if (IS_INETD || !(opt & OPT_FOREGROUND)) {
		openlog(applet_name, 0, LOG_USER);
		logmode = LOGMODE_SYSLOG;
	}
	USE_FEATURE_TELNETD_STANDALONE(
		if (opt & OPT_PORT)
			portnbr = xatou16(opt_portnbr);
	);

	/* Used to check access(loginpath, X_OK) here. Pointless.
	 * exec will do this for us for free later. */

#if ENABLE_FEATURE_TELNETD_STANDALONE
	if (IS_INETD) {
		sessions = make_new_session(0);
		if (!sessions) /* pty opening or vfork problem, exit */
			return 1; /* make_new_session prints error message */
	} else {
		master_fd = create_and_bind_stream_or_die(opt_bindaddr, portnbr);
		xlisten(master_fd, 1);
	}
#else
	sessions = make_new_session();
	if (!sessions) /* pty opening or vfork problem, exit */
		return 1; /* make_new_session prints error message */
#endif

	/* We don't want to die if just one session is broken */
	signal(SIGPIPE, SIG_IGN);

	if (opt & OPT_WATCHCHILD)
		signal(SIGCHLD, handle_sigchld);
	else /* prevent dead children from becoming zombies */
		signal(SIGCHLD, SIG_IGN);

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

   size1: "how many bytes are buffered for pty between rdidx1 and wridx1?"
   size2: "how many bytes are buffered for socket between rdidx2 and wridx2?"

   Each session has got two buffers. Buffers are circular. If sizeN == 0,
   buffer is empty. If sizeN == BUFSIZE, buffer is full. In both these cases
   rdidxN == wridxN.
*/
 again:
	FD_ZERO(&rdfdset);
	FD_ZERO(&wrfdset);

	/* Select on the master socket, all telnet sockets and their
	 * ptys if there is room in their session buffers.
	 * NB: scalability problem: we recalculate entire bitmap
	 * before each select. Can be a problem with 500+ connections. */
	ts = sessions;
	while (ts) {
		struct tsession *next = ts->next; /* in case we free ts. */
		if (ts->shell_pid == -1) {
			/* Child died and we detected that */
			free_session(ts);
		} else {
			if (ts->size1 > 0)       /* can write to pty */
				FD_SET(ts->ptyfd, &wrfdset);
			if (ts->size1 < BUFSIZE) /* can read from socket */
				FD_SET(ts->sockfd_read, &rdfdset);
			if (ts->size2 > 0)       /* can write to socket */
				FD_SET(ts->sockfd_write, &wrfdset);
			if (ts->size2 < BUFSIZE) /* can read from pty */
				FD_SET(ts->ptyfd, &rdfdset);
		}
		ts = next;
	}
	if (!IS_INETD) {
		FD_SET(master_fd, &rdfdset);
		/* This is needed because free_session() does not
		 * take master_fd into account when it finds new
		 * maxfd among remaining fd's */
		if (master_fd > maxfd)
			maxfd = master_fd;
	}

	count = select(maxfd + 1, &rdfdset, &wrfdset, NULL, NULL);
	if (count < 0)
		goto again; /* EINTR or ENOMEM */

#if ENABLE_FEATURE_TELNETD_STANDALONE
	/* First check for and accept new sessions. */
	if (!IS_INETD && FD_ISSET(master_fd, &rdfdset)) {
		int fd;
		struct tsession *new_ts;

		fd = accept(master_fd, NULL, NULL);
		if (fd < 0)
			goto again;
		/* Create a new session and link it into our active list */
		new_ts = make_new_session(fd);
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

		if (/*ts->size1 &&*/ FD_ISSET(ts->ptyfd, &wrfdset)) {
			int num_totty;
			unsigned char *ptr;
			/* Write to pty from buffer 1. */
			ptr = remove_iacs(ts, &num_totty);
			count = safe_write(ts->ptyfd, ptr, num_totty);
			if (count < 0) {
				if (errno == EAGAIN)
					goto skip1;
				goto kill_session;
			}
			ts->size1 -= count;
			ts->wridx1 += count;
			if (ts->wridx1 >= BUFSIZE) /* actually == BUFSIZE */
				ts->wridx1 = 0;
		}
 skip1:
		if (/*ts->size2 &&*/ FD_ISSET(ts->sockfd_write, &wrfdset)) {
			/* Write to socket from buffer 2. */
			count = MIN(BUFSIZE - ts->wridx2, ts->size2);
			count = safe_write(ts->sockfd_write, TS_BUF2 + ts->wridx2, count);
			if (count < 0) {
				if (errno == EAGAIN)
					goto skip2;
				goto kill_session;
			}
			ts->size2 -= count;
			ts->wridx2 += count;
			if (ts->wridx2 >= BUFSIZE) /* actually == BUFSIZE */
				ts->wridx2 = 0;
		}
 skip2:
		/* Should not be needed, but... remove_iacs is actually buggy
		 * (it cannot process iacs which wrap around buffer's end)!
		 * Since properly fixing it requires writing bigger code,
		 * we rely instead on this code making it virtually impossible
		 * to have wrapped iac (people don't type at 2k/second).
		 * It also allows for bigger reads in common case. */
		if (ts->size1 == 0) {
			ts->rdidx1 = 0;
			ts->wridx1 = 0;
		}
		if (ts->size2 == 0) {
			ts->rdidx2 = 0;
			ts->wridx2 = 0;
		}

		if (/*ts->size1 < BUFSIZE &&*/ FD_ISSET(ts->sockfd_read, &rdfdset)) {
			/* Read from socket to buffer 1. */
			count = MIN(BUFSIZE - ts->rdidx1, BUFSIZE - ts->size1);
			count = safe_read(ts->sockfd_read, TS_BUF1 + ts->rdidx1, count);
			if (count <= 0) {
				if (count < 0 && errno == EAGAIN)
					goto skip3;
				goto kill_session;
			}
			/* Ignore trailing NUL if it is there */
			if (!TS_BUF1[ts->rdidx1 + count - 1]) {
				--count;
			}
			ts->size1 += count;
			ts->rdidx1 += count;
			if (ts->rdidx1 >= BUFSIZE) /* actually == BUFSIZE */
				ts->rdidx1 = 0;
		}
 skip3:
		if (/*ts->size2 < BUFSIZE &&*/ FD_ISSET(ts->ptyfd, &rdfdset)) {
			/* Read from pty to buffer 2. */
			count = MIN(BUFSIZE - ts->rdidx2, BUFSIZE - ts->size2);
			count = safe_read(ts->ptyfd, TS_BUF2 + ts->rdidx2, count);
			if (count <= 0) {
				if (count < 0 && errno == EAGAIN)
					goto skip4;
				goto kill_session;
			}
			ts->size2 += count;
			ts->rdidx2 += count;
			if (ts->rdidx2 >= BUFSIZE) /* actually == BUFSIZE */
				ts->rdidx2 = 0;
		}
 skip4:
		ts = next;
		continue;
 kill_session:
		free_session(ts);
		ts = next;
	}

	goto again;
}
