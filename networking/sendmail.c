/* vi: set sw=4 ts=4: */
/*
 * bare bones sendmail/fetchmail
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

#define INITIAL_STDIN_FILENO 3

static void uuencode(char *fname, const char *text)
{
	enum {
		SRC_BUF_SIZE = 45,  /* This *MUST* be a multiple of 3 */
		DST_BUF_SIZE = 4 * ((SRC_BUF_SIZE + 2) / 3),
	};

#define src_buf text
	int fd;
#define len fd
	char dst_buf[DST_BUF_SIZE + 1];

	if (fname) {
		fd = INITIAL_STDIN_FILENO;
		if (NOT_LONE_DASH(fname))
			fd = xopen(fname, O_RDONLY);
		src_buf = bb_common_bufsiz1;
	// N.B. strlen(NULL) segfaults!
	} else if (text) {
		// though we do not call uuencode(NULL, NULL) explicitly
		// still we do not want to break things suddenly
		len = strlen(text);
	} else
		return;

	fflush(stdout); // sync stdio and unistd output
	while (1) {
		size_t size;
		if (fname) {
			size = full_read(fd, (char *)src_buf, SRC_BUF_SIZE);
			if ((ssize_t)size < 0)
				bb_perror_msg_and_die(bb_msg_read_error);
		} else {
			size = len;
			if (len > SRC_BUF_SIZE)
				size = SRC_BUF_SIZE;
		}
		if (!size)
			break;
		// encode the buffer we just read in
		bb_uuencode(dst_buf, src_buf, size, bb_uuenc_tbl_base64);
		if (fname) {
			xwrite(STDOUT_FILENO, "\r\n", 2);
		} else {
			src_buf += size;
			len -= size;
		}
		xwrite(STDOUT_FILENO, dst_buf, 4 * ((size + 2) / 3));
	}
	if (fname)
		close(fd);
#undef src_buf
#undef len
}

struct globals {
	pid_t helper_pid;
	unsigned timeout;
	// arguments for SSL connection helper
	const char *xargs[9];
	// arguments for postprocess helper
	const char *fargs[3];
};
#define G (*ptr_to_globals)
#define helper_pid      (G.helper_pid)
#define timeout         (G.timeout   )
#define xargs           (G.xargs     )
#define fargs           (G.fargs     )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	xargs[0] = "openssl"; \
	xargs[1] = "s_client"; \
	xargs[2] = "-quiet"; \
	xargs[3] = "-connect"; \
	/*xargs[4] = "localhost";*/ \
	xargs[5] = "-tls1"; \
	xargs[6] = "-starttls"; \
	xargs[7] = "smtp"; \
	fargs[0] = "utf-8"; \
} while (0)

#define opt_connect	  (xargs[4])
#define opt_after_connect (xargs[5])
#define opt_charset	  (fargs[0])
#define opt_subject	  (fargs[1])

static void kill_helper(void)
{
	// TODO!!!: is there more elegant way to terminate child on program failure?
	if (helper_pid > 0)
		kill(helper_pid, SIGTERM);
}

// generic signal handler
static void signal_handler(int signo)
{
#define err signo

	if (SIGALRM == signo) {
		kill_helper();
		bb_error_msg_and_die("timed out");
	}

	// SIGCHLD. reap zombies
	if (wait_any_nohang(&err) > 0)
		if (WIFEXITED(err) && WEXITSTATUS(err))
			bb_error_msg_and_die("child exited (%d)", WEXITSTATUS(err));
#undef err
}

static void launch_helper(const char **argv)
{
	// setup vanilla unidirectional pipes interchange
	int idx;
	int pipes[4];

	xpipe(pipes);
	xpipe(pipes+2);
	helper_pid = vfork();
	if (helper_pid < 0)
		bb_perror_msg_and_die("vfork");
	idx = (!helper_pid) * 2;
	xdup2(pipes[idx], STDIN_FILENO);
	xdup2(pipes[3-idx], STDOUT_FILENO);
	if (ENABLE_FEATURE_CLEAN_UP)
		for (int i = 4; --i >= 0; )
			if (pipes[i] > STDOUT_FILENO)
				close(pipes[i]);
	if (!helper_pid) {
		// child: try to execute connection helper
		BB_EXECVP(*argv, (char **)argv);
		_exit(127);
	}
	// parent: check whether child is alive
	bb_signals(0
		+ (1 << SIGCHLD)
		+ (1 << SIGALRM)
		, signal_handler);
	signal_handler(SIGCHLD);
	// child seems OK -> parent goes on
}

static const char *command(const char *fmt, const char *param)
{
	const char *msg = fmt;
	alarm(timeout);
	if (msg) {
		msg = xasprintf(fmt, param);
		printf("%s\r\n", msg);
	}
	fflush(stdout);
	return msg;
}

static int smtp_checkp(const char *fmt, const char *param, int code)
{
	char *answer;
	const char *msg = command(fmt, param);
	// read stdin
	// if the string has a form \d\d\d- -- read next string. E.g. EHLO response
	// parse first bytes to a number
	// if code = -1 then just return this number
	// if code != -1 then checks whether the number equals the code
	// if not equal -> die saying msg
	while ((answer = xmalloc_fgetline(stdin)) != NULL)
		if (strlen(answer) <= 3 || '-' != answer[3])
			break;
	if (answer) {
		int n = atoi(answer);
		alarm(0);
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(answer);
		}
		if (-1 == code || n == code) {
			return n;
		}
	}
	kill_helper();
	bb_error_msg_and_die("%s failed", msg);
}

static inline int smtp_check(const char *fmt, int code)
{
	return smtp_checkp(fmt, NULL, code);
}

// strip argument of bad chars
static char *sane(char *str)
{
	char *s = str;
	char *p = s;
	while (*s) {
		if (isalnum(*s) || '_' == *s || '-' == *s || '.' == *s || '@' == *s) {
			*p++ = *s;
		}
		s++;
	}
	*p = '\0';
	return str;
}

#if ENABLE_FETCHMAIL
static void pop3_checkr(const char *fmt, const char *param, char **ret)
{
	const char *msg = command(fmt, param);
	char *answer = xmalloc_fgetline(stdin);
	if (answer && '+' == *answer) {
		alarm(0);
		if (ret)
			*ret = answer+4; // skip "+OK "
		else if (ENABLE_FEATURE_CLEAN_UP)
			free(answer);
		return;
	}
	kill_helper();
	bb_error_msg_and_die("%s failed", msg);
}

static inline void pop3_check(const char *fmt, const char *param)
{
	pop3_checkr(fmt, param, NULL);
}

static void pop3_message(const char *filename)
{
	int fd;
	char *answer;
	// create and open file filename
	// read stdin, copy to created file
	fd = xopen(filename, O_CREAT | O_WRONLY | O_TRUNC | O_EXCL);
	while ((answer = xmalloc_fgets_str(stdin, "\r\n")) != NULL) {
		char *s = answer;
		if ('.' == *answer) {
			if ('.' == answer[1])
				s++;
			else if ('\r' == answer[1] && '\n' == answer[2] && '\0' == answer[3])
				break;
		}
		xwrite(fd, s, strlen(s));
		free(answer);
	}
	close(fd);
}
#endif

// NB: parse_url can modify url[] (despite const), but only if '@' is there
static const char *parse_url(const char *url, const char **user, const char **pass)
{
	// parse [user[:pass]@]host
	// return host
	char *s = strchr(url, '@');
	*user = *pass = NULL;
	if (s) {
		*s++ = '\0';
		*user = url;
		url = s;
		s = strchr(*user, ':');
		if (s) {
			*s++ = '\0';
			*pass = s;
		}
	}
	return url;
}

static void rcptto(const char *s)
{
	smtp_checkp("RCPT TO:<%s>", s, 250);
}

int sendgetmail_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sendgetmail_main(int argc UNUSED_PARAM, char **argv)
{
	llist_t *opt_attachments = NULL;
	char *opt_from;
	const char *opt_user;
	const char *opt_pass;
	enum {
		OPT_w = 1 << 0,         // network timeout
		OPT_H = 1 << 1,         // [user:password@]server[:port]
		OPT_S = 1 << 2,         // connect using openssl s_client helper

		OPTS_t = 1 << 3,        // sendmail: read addresses from body
		OPTF_t = 1 << 3,        // fetchmail: use "TOP" not "RETR"

		OPTS_s = 1 << 4,        // sendmail: subject
		OPTF_z = 1 << 4,        // fetchmail: delete from server

		OPTS_c = 1 << 5,        // sendmail: assumed charset
		OPTS_a = 1 << 6,        // sendmail: attachment(s)
		OPTS_i = 1 << 7,        // sendmail: ignore lone dots in message body (implied)

		OPTS_N = 1 << 8,        // sendmail: request notification
		OPTS_f = 1 << 9,        // sendmail: sender address
	};
	const char *options;
	int opts;

	// init global variables
	INIT_G();

	// parse options, different option sets for sendmail and fetchmail
	// N.B. opt_after_connect hereafter is NULL if we are called as fetchmail
	// and is NOT NULL if we are called as sendmail
	if (!ENABLE_FETCHMAIL || 's' == applet_name[0]) {
		// SENDMAIL
		// save initial stdin since body is piped!
		xdup2(STDIN_FILENO, INITIAL_STDIN_FILENO);
		opt_complementary = "w+:a::";
		options = "w:H:St" "s:c:a:iN:f:";
		// body is pseudo attachment read from stdin
		llist_add_to_end(&opt_attachments, (char *)"-");
	} else {
		// FETCHMAIL
		opt_after_connect = NULL;
		opt_complementary = "-1:w+";
		options = "w:H:St" "z";
	}
	opts = getopt32(argv, options,
		&timeout /* -w */, &opt_connect /* -H */,
		&opt_subject, &opt_charset, &opt_attachments, NULL, &opt_from
	);
	//argc -= optind;
	argv += optind;

	// connect to server
	// host[:port] not specified ? -> use $HOSTNAME. no $HOSTNAME ? -> use localhost
	if (!(opts & OPT_H)) {
		opt_connect = getenv("HOSTNAME");
		if (!opt_connect)
			opt_connect = "127.0.0.1";
	}
	// fetch username and password, if any
	// NB: parse_url modifies opt_connect[] ONLY if '@' is there.
	// Thus "127.0.0.1" won't be modified, an is ok that it is RO.
	opt_connect = parse_url(opt_connect, &opt_user, &opt_pass);
//	bb_error_msg("H[%s] U[%s] P[%s]", opt_connect, opt_user, opt_pass);

	// username must be defined!
	if (!opt_user) {
		// N.B. IMHO getenv("USER") can be way easily spoofed!
		opt_user = bb_getpwuid(NULL, -1, getuid());
	}

	// SSL ordered? ->
	if (opts & OPT_S) {
		// ... use openssl helper
		launch_helper(xargs);
	// no SSL ordered? ->
	} else {
		// ... make plain connect
		int fd = create_and_connect_stream_or_die(opt_connect, 25);
		// make ourselves a simple IO filter
		// from now we know nothing about network :)
		xmove_fd(fd, STDIN_FILENO);
		xdup2(STDIN_FILENO, STDOUT_FILENO);
	}

	// are we sendmail?
	if (!ENABLE_FETCHMAIL || opt_after_connect)
/***************************************************
 * SENDMAIL
 ***************************************************/
	{
		int code;
		char *boundary;
		const char *fmt;
		const char *p;
		char *q;
		llist_t *l;
		llist_t *headers = NULL;

		// got no sender address? -> use username as a resort
		if (!(opts & OPTS_f)) {
			char *domain = safe_getdomainname();
			opt_from = xasprintf("%s@%s", opt_user, domain);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(domain);
		}

		// introduce to server

		// we didn't use SSL helper? ->
		if (!(opts & OPT_S)) {
			// ... wait for initial server OK
			smtp_check(NULL, 220);
		}

		// we should start with modern EHLO
		if (250 != smtp_checkp("EHLO %s", sane(opt_from), -1)) {
			smtp_checkp("HELO %s", opt_from, 250);
		}

		// set sender
		// NOTE: if password has not been specified
		// then no authentication is possible
		code = (opt_pass ? -1 : 250);
		// first try softly without authentication
		while (250 != smtp_checkp("MAIL FROM:<%s>", opt_from, code)) {
			// MAIL FROM failed -> authentication needed
			if (334 == smtp_check("AUTH LOGIN", -1)) {
				uuencode(NULL, opt_user); // opt_user != NULL
				smtp_check("", 334);
				uuencode(NULL, opt_pass);
				smtp_check("", 235);
			}
			// authenticated OK? -> retry to set sender
			// but this time die on failure!
			code = 250;
		}

		// recipients specified as arguments
		while (*argv) {
			// loose test on email address validity
			if (strchr(sane(*argv), '@')) {
				rcptto(sane(*argv));
				llist_add_to_end(&headers, xasprintf("To: %s", *argv));
			}
			argv++;
		}

		// if -t specified or no recipients specified -> enter all-included mode
		// i.e. scan stdin for To:, Cc:, Bcc:, and Subject: lines ...
		// ... and then use the rest of stdin as message body
		// N.B. subject read from body has priority
		// over that specified on command line.
		// recipients are merged
		// N.B. other headers are collected and will be dumped verbatim
		if (opts & OPTS_t || !headers) {
			// fetch recipients and (optionally) subject
			char *s;
			while ((s = xmalloc_reads(INITIAL_STDIN_FILENO, NULL, NULL)) != NULL) {
				if (0 == strncasecmp("To: ", s, 4) || 0 == strncasecmp("Cc: ", s, 4)) {
					rcptto(sane(s+4));
					llist_add_to_end(&headers, s);
				} else if (0 == strncasecmp("Bcc: ", s, 5)) {
					rcptto(sane(s+5));
					if (ENABLE_FEATURE_CLEAN_UP)
						free(s);
					// N.B. Bcc vanishes from headers!
/*				} else if (0 == strncmp("From: ", s, 6)) {
					opt_from = s+6;
					opts |= OPTS_f;
*/				} else if (0 == strncmp("Subject: ", s, 9)) {
					opt_subject = s+9;
					opts |= OPTS_s;
				} else if (s[0]) {
					// misc header
					llist_add_to_end(&headers, s);
				} else {
					free(s);
					break; // empty line
				}
			}
		}

		// enter "put message" mode
		smtp_check("DATA", 354);

		// put headers we could have preread with -t
		for (l = headers; l; l = l->link) {
			printf("%s\r\n", l->data);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(l->data);
		}

		// put address header
		printf("From: %s\r\n", opt_from);

		// put encoded subject
		if (opts & OPTS_c)
			sane((char *)opt_charset);
		if (opts & OPTS_s) {
			printf("Subject: =?%s?B?", opt_charset);
			uuencode(NULL, opt_subject);
			printf("?=\r\n");
		}

		// put notification
		if (opts & OPTS_N)
			printf("Disposition-Notification-To: %s\r\n", opt_from);

		// make a random string -- it will delimit message parts
		srand(monotonic_us());
		boundary = xasprintf("%d-%d-%d", rand(), rand(), rand());

		// put common headers and body start
		printf(
			"Message-ID: <%s>\r\n"
			"Mime-Version: 1.0\r\n"
			"%smultipart/mixed; boundary=\"%s\"\r\n"
			, boundary
			, "Content-Type: "
			, boundary
		);

		// put body + attachment(s)
		// N.B. all these weird things just to be tiny
		// by reusing string patterns!
		fmt =
			"\r\n--%s\r\n"
			"%stext/plain; charset=%s\r\n"
			"%s%s\r\n"
			"%s"
		;
		p = opt_charset;
		q = (char *)"";
		l = opt_attachments;
		while (l) {
			printf(
				fmt
				, boundary
				, "Content-Type: "
				, p
				, "Content-Disposition: inline"
				, q
				, "Content-Transfer-Encoding: base64\r\n"
			);
			p = "";
			fmt =
				"\r\n--%s\r\n"
				"%sapplication/octet-stream%s\r\n"
				"%s; filename=\"%s\"\r\n"
				"%s"
			;
			uuencode(l->data, NULL);
			l = l->link;
			if (l)
				q = bb_get_last_path_component_strip(l->data);
		}

		// put message terminator
		printf("\r\n--%s--\r\n" "\r\n", boundary);

		// leave "put message" mode
		smtp_check(".", 250);
		// ... and say goodbye
		smtp_check("QUIT", 221);
	}
#if ENABLE_FETCHMAIL
/***************************************************
 * FETCHMAIL
 ***************************************************/
	else {
		char *buf;
		unsigned nmsg;
		char *hostname;
		pid_t pid;

		// cache fetch command:
		// TOP will return only the headers
		// RETR will dump the whole message
		const char *retr = (opts & OPTF_t) ? "TOP %u 0" : "RETR %u";

		// goto maildir
		xchdir(*argv++);

		// cache postprocess program
		*fargs = *argv;

		// authenticate

		// password is mandatory
		if (!opt_pass) {
			bb_error_msg_and_die("no password");
		}

		// get server greeting
		pop3_checkr(NULL, NULL, &buf);

		// server supports APOP?
		if ('<' == *buf) {
			md5_ctx_t md5;
			// yes! compose <stamp><password>
			char *s = strchr(buf, '>');
			if (s)
				strcpy(s+1, opt_pass);
			s = buf;
			// get md5 sum of <stamp><password>
			md5_begin(&md5);
			md5_hash(s, strlen(s), &md5);
			md5_end(s, &md5);
			// NOTE: md5 struct contains enough space
			// so we reuse md5 space instead of xzalloc(16*2+1)
#define md5_hex ((uint8_t *)&md5)
//			uint8_t *md5_hex = (uint8_t *)&md5;
			*bin2hex((char *)md5_hex, s, 16) = '\0';
			// APOP
			s = xasprintf("%s %s", opt_user, md5_hex);
#undef md5_hex
			pop3_check("APOP %s", s);
			if (ENABLE_FEATURE_CLEAN_UP) {
				free(s);
				free(buf-4); // buf is "+OK " away from malloc'ed string
			}
		// server ignores APOP -> use simple text authentication
		} else {
			// USER
			pop3_check("USER %s", opt_user);
			// PASS
			pop3_check("PASS %s", opt_pass);
		}

		// get mailbox statistics
		pop3_checkr("STAT", NULL, &buf);

		// prepare message filename suffix
		hostname = safe_gethostname();
		pid = getpid();

		// get messages counter
		// NOTE: we don't use xatou(buf) since buf is "nmsg nbytes"
		// we only need nmsg and atoi is just exactly what we need
		// if atoi fails to convert buf into number it returns 0
		// in this case the following loop simply will not be executed
		nmsg = atoi(buf);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(buf-4); // buf is "+OK " away from malloc'ed string

		// loop through messages
		for (; nmsg; nmsg--) {

			// generate unique filename
			char *filename = xasprintf("tmp/%llu.%u.%s",
					monotonic_us(), (unsigned)pid, hostname);
			char *target;
			int rc;

			// retrieve message in ./tmp/
			pop3_check(retr, (const char *)(ptrdiff_t)nmsg);
			pop3_message(filename);
			// delete message from server
			if (opts & OPTF_z)
				pop3_check("DELE %u", (const char*)(ptrdiff_t)nmsg);

			// run postprocessing program
			if (*fargs) {
				fargs[1] = filename;
				rc = wait4pid(spawn((char **)fargs));
				if (99 == rc)
					break;
				if (1 == rc)
					goto skip;
			}

			// atomically move message to ./new/
			target = xstrdup(filename);
			strncpy(target, "new", 3);
			// ... or just stop receiving on error
			if (rename_or_warn(filename, target))
				break;
			free(target);
 skip:
			free(filename);
		}

		// Bye
		pop3_check("QUIT", NULL);
	}
#endif // ENABLE_FETCHMAIL

	return 0;
}
