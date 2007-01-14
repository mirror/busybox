/* vi: set sw=4 ts=4: */
/*
 * Fake identd server.
 *
 * Copyright (C) 2007 Denis Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include <syslog.h>
#include "busybox.h"
#include "isrv.h"

enum { TIMEOUT = 20 };

/* Why use alarm(TIMEOUT-1)?
 * isrv's internal select() will run with timeout=TIMEOUT.
 * If nothing happens during TIMEOUT-1 seconds (no accept/read),
 * then ALL sessions timed out by now. Instead of closing them one-by-one
 * (isrv calls do_timeout for each 'stale' session),
 * SIGALRM triggered by alarm(TIMEOUT-1) will kill us, terminating them all.
 */

typedef struct identd_buf_t {
	int pos;
	int fd_flag;
	char buf[64 - 2*sizeof(int)];
} identd_buf_t;

static const char *bogouser = "nobody";

static int new_peer(isrv_state_t *state, int fd)
{
	int peer;
	identd_buf_t *buf = xzalloc(sizeof(*buf));

	alarm(TIMEOUT - 1);

	peer = isrv_register_peer(state, buf);
	if (peer < 0)
		return 0; /* failure */
	if (isrv_register_fd(state, peer, fd) < 0)
		return peer; /* failure, unregister peer */

	buf->fd_flag = fcntl(fd, F_GETFL, 0) | O_NONBLOCK;
	isrv_want_rd(state, fd);
	return 0;
}

static int do_rd(int fd, void **paramp)
{
	identd_buf_t *buf = *paramp;
	char *cur, *p;
	int sz;

	alarm(TIMEOUT - 1);

	cur = buf->buf + buf->pos;

	fcntl(fd, F_SETFL, buf->fd_flag | O_NONBLOCK);
	sz = safe_read(fd, cur, sizeof(buf->buf) - buf->pos);

	if (sz < 0) {
		if (errno != EAGAIN)
			goto term; /* terminate this session if !EAGAIN */
		goto ok;
	}

	buf->pos += sz;
	buf->buf[buf->pos] = '\0';
	p = strpbrk(cur, "\r\n");
	if (p)
		*p = '\0';
	if (p || !sz || buf->pos == sizeof(buf->buf)) {
		/* fd is still in nonblocking mode - we never block here */
		fdprintf(fd, "%s : USERID : UNIX : %s\r\n", buf->buf, bogouser);
		goto term;
	}
 ok:
	fcntl(fd, F_SETFL, buf->fd_flag & ~O_NONBLOCK);
	return 0;
 term:
	fcntl(fd, F_SETFL, buf->fd_flag & ~O_NONBLOCK);
	free(buf);
	return 1;
}

static int do_timeout(void **paramp)
{
	return 1; /* terminate session */
}

static void inetd_mode(void)
{
	identd_buf_t *buf = xzalloc(sizeof(*buf));
	/* We do NOT want nonblocking I/O here! */
	buf->fd_flag = fcntl(0, F_GETFL, 0);
	while (do_rd(0, (void*)&buf) == 0) /* repeat */;
}

int fakeidentd_main(int argc, char **argv)
{
	enum {
		OPT_foreground = 0x1,
		OPT_inetd      = 0x2,
		OPT_inetdwait  = 0x4,
		OPT_nodeamon   = 0x7,
		OPT_bindaddr   = 0x8,
	};

	const char *bind_address = NULL;
	unsigned opt;
	int fd;

	opt = getopt32(argc, argv, "fiwb:", &bind_address);
	if (optind < argc)
		bogouser = argv[optind];

	/* Daemonize if no -f or -i or -w */
	bb_sanitize_stdio(!(opt & OPT_nodeamon));
	if (!(opt & OPT_nodeamon)) {
		openlog(applet_name, 0, LOG_DAEMON);
		logmode = LOGMODE_SYSLOG;
	}

	if (opt & OPT_inetd) {
		inetd_mode();
		return 0;
	}

	/* Ignore closed connections when writing */
	signal(SIGPIPE, SIG_IGN);

	if (opt & OPT_inetdwait) {
		fd = 0;
	} else {
		fd = create_and_bind_stream_or_die(bind_address,
				bb_lookup_port("identd", "tcp", 113));
		xlisten(fd, 5);
	}

	isrv_run(fd, new_peer, do_rd, NULL, do_timeout, TIMEOUT, 1);
	return 0;
}
