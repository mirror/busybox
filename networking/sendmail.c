/* vi: set sw=4 ts=4: */
/*
 * bare bones sendmail
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

/*
   Extracted from BB uuencode.c
 */
enum {
	SRC_BUF_SIZE = 45,  /* This *MUST* be a multiple of 3 */
	DST_BUF_SIZE = 4 * ((SRC_BUF_SIZE + 2) / 3),
};

static void uuencode(const char *fname)
{
	int fd;
	char src_buf[SRC_BUF_SIZE];
	char dst_buf[1 + DST_BUF_SIZE + 1];

	fd = xopen(fname, O_RDONLY);
	fflush(stdout);
	dst_buf[0] = '\n';
	while (1) {
		size_t size = full_read(fd, src_buf, SRC_BUF_SIZE);
		if (!size)
			break;
		if ((ssize_t)size < 0)
			bb_perror_msg_and_die(bb_msg_read_error);
		/* Encode the buffer we just read in */
		bb_uuencode(dst_buf + 1, src_buf, size, bb_uuenc_tbl_base64);
		xwrite(STDOUT_FILENO, dst_buf, 1 + 4 * ((size + 2) / 3));
	}
	close(fd);
}

// "inline" version
// encodes content of given buffer instead of fd
// used to encode subject and authentication terms
static void uuencode_inline(const char *src_buf)
{
	size_t len;
	char dst_buf[DST_BUF_SIZE + 1];

	len = strlen(src_buf);
	fflush(stdout);
	while (len > 0) {
		size_t chunk = (len <= SRC_BUF_SIZE) ? len : SRC_BUF_SIZE;
		bb_uuencode(dst_buf, src_buf, chunk, bb_uuenc_tbl_base64);
		xwrite(STDOUT_FILENO, dst_buf, 4 * ((chunk + 2) / 3));
		src_buf += chunk;
		len -= chunk;
	}
}

#if ENABLE_FEATURE_SENDMAIL_NETWORK
// generic signal handler
static void signal_handler(int signo)
{
	int err;

	if (SIGALRM == signo)
		bb_error_msg_and_die("timed out");

	// SIGCHLD. reap zombies
	if (wait_any_nohang(&err) > 0)
		if (WIFEXITED(err) && WEXITSTATUS(err))
			bb_error_msg_and_die("child exited (%d)", WEXITSTATUS(err));
}

static pid_t helper_pid = -1;

// read stdin, parses first bytes to a number, i.e. server response
// if code = -1 then just return this number
// if code != -1 then checks whether the number equals the code
// if not equal -> die saying errmsg
static int check(int code, const char *errmsg)
{
	char *answer;

	// read a string and match it against the set of available answers
	fflush(stdout);
	answer = xmalloc_getline(stdin);
	if (answer) {
		int n = atoi(answer);
		if (-1 == code || n == code) {
			free(answer);
			return n;
		}
	}
	// TODO!!!: is there more elegant way to terminate child on program failure?
	if (helper_pid > 0)
		kill(helper_pid, SIGTERM);
	if (!answer)
		answer = (char*)"EOF";
	else
		*strchrnul(answer, '\r') = '\0';
	bb_error_msg_and_die("error at %s: got '%s' instead", errmsg, answer);
}

static int puts_and_check(const char *msg, int code, const char *errmsg)
{
	puts(msg);
	return check(code, errmsg);
}
#endif

int sendmail_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sendmail_main(int argc, char **argv)
{
	llist_t *recipients = NULL;
	llist_t *bodies = NULL;
	llist_t *attachments = NULL;
	const char *from;
	const char *notify;
	const char *subject;
	const char *charset = "utf-8";
#if ENABLE_FEATURE_SENDMAIL_NETWORK
	const char *wsecs = "10";
	const char *server = "127.0.0.1";
	const char *port = NULL;
	const char *opt_user;
	const char *opt_pass;
	unsigned timeout;
#endif
	char *boundary;
	unsigned opts;
	enum {
		OPT_f = 1 << 0,         // sender
		OPT_n = 1 << 2,         // notification
		OPT_s = 1 << 3,         // subject given
		OPT_d = 1 << 7,         // dry run - no networking
		OPT_w = 1 << 8,         // network timeout
		OPT_h = 1 << 9,         // server
		OPT_p = 1 << 10,        // port
		OPT_U = 1 << 11,        // user specified
		OPT_P = 1 << 12,        // password specified
	};

	// -f must be specified
	// -t, -b, -a may be multiple
	opt_complementary = "f:t::b::a::";
	opts = getopt32(argv,
		"f:t:n::s:b:a:c:" USE_FEATURE_SENDMAIL_NETWORK("dw:h:p:U:P:"),
		&from, &recipients, &notify, &subject, &bodies, &attachments, &charset
		USE_FEATURE_SENDMAIL_NETWORK(, &wsecs, &server, &port, &opt_user, &opt_pass)
	);
	//argc -= optind;
	argv += optind;

//printf("OPTS[%4x]\n", opts);

	// TODO!!!: strip recipients and sender from <>

	// establish connection
#if ENABLE_FEATURE_SENDMAIL_NETWORK
	timeout = xatou(wsecs);
	if (!(opts & OPT_d)) {
		// ask password if we need to and while we're still have terminal
		// TODO: get password directly from /dev/tty? or from a secret file?
		if ((opts & (OPT_U+OPT_P)) == OPT_U) {
			if (!isatty(STDIN_FILENO) || !(opt_pass = bb_askpass(0, "Password: "))) {
				bb_error_msg_and_die("no password");
			}
		}
//printf("OPTS[%4x][%s][%s]\n", opts, opt_user, opt_pass);
//exit(0);
		// set chat timeout
		alarm(timeout);
		// connect to server
		if (argv[0]) {
			// if connection helper given
			// setup vanilla unidirectional pipes interchange
			int pipes[4];
			xpipe(pipes);
			xpipe(pipes+2);
			helper_pid = vfork();
			if (helper_pid < 0)
				bb_perror_msg_and_die("vfork");
			xdup2(pipes[(helper_pid)?0:2], STDIN_FILENO);
			xdup2(pipes[(helper_pid)?3:1], STDOUT_FILENO);
			if (ENABLE_FEATURE_CLEAN_UP)
				for (int i = 4; --i >= 0; )
					if (pipes[i] > STDOUT_FILENO)
						close(pipes[i]);
			// replace child with connection helper
			if (!helper_pid) {
				// child - try to execute connection helper
				BB_EXECVP(argv[0], argv);
				_exit(127);
			}
			// parent - check whether child is alive
			sig_catch(SIGCHLD, signal_handler);
			sig_catch(SIGALRM, signal_handler);
			signal_handler(SIGCHLD);
			// child seems OK -> parent goes on SMTP chat
		} else {
			// no connection helper provided -> make plain connect
			int fd = create_and_connect_stream_or_die(
				server,
				bb_lookup_port(port, "tcp", 25)
			);
			xmove_fd(fd, STDIN_FILENO);
			xdup2(STDIN_FILENO, STDOUT_FILENO);
			// wait for OK
			check(220, "INIT");
		}
		// mail user specified? try modern AUTHentication
		if (opt_user && (334 == puts_and_check("auth login", -1, "auth login"))) {
			uuencode_inline(opt_user);
			puts_and_check("", 334, "AUTH");
			uuencode_inline(opt_pass);
			puts_and_check("", 235, "AUTH");
		// no mail user specified or modern AUTHentication is not supported?
		} else {
			// fallback to simple HELO authentication
			// fetch domain name (defaults to local)
			const char *domain = strchr(from, '@');
			if (domain)
				domain++;
			else
				domain = "local";
			printf("helo %s\n", domain);
			check(250, "HELO");
		}

		// set addresses
		printf("mail from:<%s>\n", from);
		check(250, "MAIL FROM");
		for (llist_t *to = recipients; to; to = to->link) {
			printf("rcpt to:<%s>\n", to->data);
			check(250, "RCPT TO");
		}
		puts_and_check("data", 354, "DATA");
		// no timeout while sending message
		alarm(0);
	}
#endif

	// now put message
	// put address headers
	printf("From: %s\n", from);
	for (llist_t *to = recipients; to; to = to->link) {
		printf("To: %s\n", to->data);
	}
	// put encoded subject
	if (opts & OPT_s) {
		printf("Subject: =?%s?B?", charset);
		uuencode_inline(subject);
		puts("?=");
	}
	// put notification
	if (opts & OPT_n) {
		const char *s = notify;
		if (!s[0])
			s = from; // notify sender by default
		printf("Disposition-Notification-To: %s\n", s);
	}
	// put common headers and body start
	//srand(?);
	boundary = xasprintf("%d-%d-%d", rand(), rand(), rand());
	printf(
		"X-Mailer: busybox " BB_VER " sendmail\n"
		"X-Priority: 3\n"
		"Message-ID: <%s>\n"
		"Mime-Version: 1.0\n"
		"Content-Type: multipart/mixed; boundary=\"%s\"\n"
		"\n"
		"--%s\n"
		"Content-Type: text/plain; charset=%s\n"
		"%s\n%s"
		, boundary, boundary, boundary, charset
		, "Content-Disposition: inline"
		, "Content-Transfer-Encoding: base64\n"
	);
	// put body(ies)
	for (llist_t *f = bodies; f; f = f->link) {
		uuencode(f->data);
	}
	// put attachment(s)
	for (llist_t *f = attachments; f; f = f->link) {
		printf(
			"\n--%s\n"
			"Content-Type: application/octet-stream\n"
			"%s; filename=\"%s\"\n"
			"%s"
			, boundary
			, "Content-Disposition: inline"
			, bb_get_last_path_component_strip(f->data)
			, "Content-Transfer-Encoding: base64\n"
		);
		uuencode(f->data);
	}
	// put terminator
	printf("\n--%s--\n\n", boundary);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(boundary);

#if ENABLE_FEATURE_SENDMAIL_NETWORK
	// end message and say goodbye
	if (!(opts & OPT_d)) {
		alarm(timeout);
		puts_and_check(".", 250, "BODY");
		puts_and_check("quit", 221, "QUIT");
	}
#endif

	return 0;
}
