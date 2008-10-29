/* vi: set sw=4 ts=4: */
/*
 * bare bones sendmail
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

struct globals {
	pid_t helper_pid;
	unsigned timeout;
	FILE *fp0; // initial stdin
	// arguments for SSL connection helper
	const char *xargs[9];
};
#define G (*ptr_to_globals)
#define helper_pid      (G.helper_pid)
#define timeout         (G.timeout   )
#define fp0             (G.fp0       )
#define xargs           (G.xargs     )
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
} while (0)

#define opt_connect (xargs[4])

static void uuencode(char *fname, const char *text)
{
	enum {
		SRC_BUF_SIZE = 45,  /* This *MUST* be a multiple of 3 */
		DST_BUF_SIZE = 4 * ((SRC_BUF_SIZE + 2) / 3),
	};

#define src_buf text
	FILE *fp = fp;
	ssize_t len = len;
	char dst_buf[DST_BUF_SIZE + 1];

	if (fname) {
		fp = (NOT_LONE_DASH(fname)) ? xfopen_for_read(fname) : fp0;
		src_buf = bb_common_bufsiz1;
	// N.B. strlen(NULL) segfaults!
	} else if (text) {
		// though we do not call uuencode(NULL, NULL) explicitly
		// still we do not want to break things suddenly
		len = strlen(text);
	} else
		return;

	while (1) {
		size_t size;
		if (fname) {
			size = fread((char *)src_buf, 1, SRC_BUF_SIZE, fp);
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
			printf("\r\n");
		} else {
			src_buf += size;
			len -= size;
		}
		fwrite(dst_buf, 1, 4 * ((size + 2) / 3), stdout);
	}
	if (fname)
		fclose(fp);
#undef src_buf
}


#if ENABLE_FEATURE_SENDMAIL_SSL
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
		if (WIFEXITED(err)) {
//			if (WEXITSTATUS(err))
				bb_error_msg_and_die("child exited (%d)", WEXITSTATUS(err));
//			else
//				kill(0, SIGCONT);
		}
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
#else
#define kill_helper() ((void)0)
#define launch_helper(x) bb_error_msg_and_die("no SSL support")
#endif

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
		free(answer);
		if (-1 == code || n == code)
			return n;
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

int sendmail_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sendmail_main(int argc UNUSED_PARAM, char **argv)
{
#if ENABLE_FEATURE_SENDMAIL_MAILX
	llist_t *opt_attachments = NULL;
	const char *opt_subject;
	const char *opt_charset = CONFIG_FEATURE_SENDMAIL_CHARSET;
#if ENABLE_FEATURE_SENDMAIL_MAILXX
	llist_t *opt_carboncopies = NULL;
	char *opt_errors_to;
#endif
#endif
	char *opt_from, *opt_fullname;
	const char *opt_user;
	const char *opt_pass;
	int code;
	char *boundary;
	llist_t *l;
	llist_t *headers = NULL;
	char *domain = sane(safe_getdomainname());
	unsigned opts;

	enum {
		OPT_w = 1 << 0,         // network timeout
		OPT_H = 1 << 1,         // [user:password@]server[:port]
		OPT_S = 1 << 2,         // connect using openssl s_client helper
		OPT_t = 1 << 3,         // read message for recipients
		OPT_N = 1 << 4,         // request notification
		OPT_f = 1 << 5,         // sender address
		OPT_F = 1 << 6,         // sender name, overrides $NAME
#if ENABLE_FEATURE_SENDMAIL_MAILX
		OPT_s = 1 << 7,         // subject
		OPT_j = 1 << 8,         // assumed charset
		OPT_a = 1 << 9,         // attachment(s)
#if ENABLE_FEATURE_SENDMAIL_MAILXX
		OPT_c = 1 << 10,        // carbon copy
		OPT_e = 1 << 11,        // errors-to address
#endif
#endif
	};

	// init global variables
	INIT_G();

	// save initial stdin since body is piped!
	xdup2(STDIN_FILENO, 3);
	fp0 = fdopen(3, "r");

	// parse options
	opt_complementary = "w+:a::" USE_FEATURE_SENDMAIL_MAILXX("c::");
	opts = getopt32(argv,
		"w:H:St" "N:f:F:" USE_FEATURE_SENDMAIL_MAILX("s:j:a:") USE_FEATURE_SENDMAIL_MAILXX("c:e:")
		"X:V:vq:R:O:o:nmL:Iih:GC:B:b:A:" // postfix compat only, ignored
		// r:Q:p:M:Dd are candidates from another man page. TODO?
		"46E", // ssmtp introduces another quirks. TODO?: -a[upm] (user, pass, method) to be supported
		&timeout /* -w */, &opt_connect /* -H */,
		NULL, &opt_from, &opt_fullname,
		USE_FEATURE_SENDMAIL_MAILX(&opt_subject, &opt_charset, &opt_attachments,)
		USE_FEATURE_SENDMAIL_MAILXX(&opt_carboncopies, &opt_errors_to,)
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	);
	//argc -= optind;
	argv += optind;

	// connect to server
	// host[:port] not specified ? -> use $SMTPHOST. no $SMTPHOST ? -> use localhost
	if (!(opts & OPT_H)) {
		opt_connect = getenv("SMTPHOST");
		if (!opt_connect)
			opt_connect = "127.0.0.1";
	}
	// fetch username and password, if any
	// NB: parse_url modifies opt_connect[] ONLY if '@' is there.
	// Thus "127.0.0.1" won't be modified, an is ok that it is RO.
	opt_connect = parse_url(opt_connect, &opt_user, &opt_pass);

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

	// got no sender address? -> use username as a resort
	if (!(opts & OPT_f)) {
		opt_from = xasprintf("%s@%s", opt_user, domain);
	}

	// introduce to server

	// we didn't use SSL helper? ->
	if (!(opts & OPT_S)) {
		// ... wait for initial server OK
		smtp_check(NULL, 220);
	}

	// we should start with modern EHLO
	if (250 != smtp_checkp("EHLO %s", domain, -1)) {
		smtp_checkp("HELO %s", domain, 250);
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		free(domain);

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
		char *s = sane(*argv);
		// loose test on email address validity
		if (strchr(s, '@')) {
			rcptto(s);
			llist_add_to_end(&headers, xasprintf("To: %s", s));
		}
		argv++;
	}

#if ENABLE_FEATURE_SENDMAIL_MAILXX
	// carbon copies recipients specified as -c options
	for (l = opt_carboncopies; l; l = l->link) {
		char *s = sane(l->data);
		// loose test on email address validity
		if (strchr(s, '@')) {
			rcptto(s);
			// TODO: do we ever need to mangle the message?
			//llist_add_to_end(&headers, xasprintf("Cc: %s", s));
		}
	}
#endif

	// if -t specified or no recipients specified -> read recipients from message
	// i.e. scan stdin for To:, Cc:, Bcc: lines ...
	// ... and then use the rest of stdin as message body
	// N.B. subject read from body can be further overrided with one specified on command line.
	// recipients are merged. Bcc: lines are deleted
	// N.B. other headers are collected and will be dumped verbatim
	if (opts & OPT_t || !headers) {
		// fetch recipients and (optionally) subject
		char *s;
		while ((s = xmalloc_fgetline(fp0)) != NULL) {
			if (0 == strncasecmp("To: ", s, 4) || 0 == strncasecmp("Cc: ", s, 4)) {
				rcptto(sane(s+4));
				llist_add_to_end(&headers, s);
			} else if (0 == strncasecmp("Bcc: ", s, 5)) {
				rcptto(sane(s+5));
				free(s);
				// N.B. Bcc vanishes from headers!
			} else if (0 == strncmp("Subject: ", s, 9)) {
				// we read subject -> use it verbatim unless it is specified
				// on command line
#if ENABLE_FEATURE_SENDMAIL_MAILX
				if (opts & OPT_s)
					free(s);
				else
#endif
					llist_add_to_end(&headers, s);
			} else if (s[0]) {
				// misc header
				llist_add_to_end(&headers, s);
			} else {
				free(s);
				break; // stop on the first empty line
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

	// put (possibly encoded) subject
#if ENABLE_FEATURE_SENDMAIL_MAILX
	if (opts & OPT_j)
		sane((char *)opt_charset);
	if (opts & OPT_s) {
		printf("Subject: ");
		if (opts & OPT_j) {
			printf("=?%s?B?", opt_charset);
			uuencode(NULL, opt_subject);
			printf("?=");
		} else {
			printf("%s", opt_subject);
		}
		printf("\r\n");
	}
#endif

	// put sender name, $NAME is the default
	if (!(opts & OPT_F))
		opt_fullname = getenv("NAME");
	if (opt_fullname)
		printf("From: \"%s\" <%s>\r\n", opt_fullname, opt_from);

	// put notification
	if (opts & OPT_N)
		printf("Disposition-Notification-To: %s\r\n", opt_from);

#if ENABLE_FEATURE_SENDMAIL_MAILXX
	// put errors recipient
	if (opts & OPT_e)
		printf("Errors-To: %s\r\n", opt_errors_to);
#endif

	// make a random string -- it will delimit message parts
	srand(monotonic_us());
	boundary = xasprintf("%d-%d-%d", rand(), rand(), rand());

	// put common headers
	// TODO: do we really need this?
//	printf("Message-ID: <%s>\r\n", boundary);

#if ENABLE_FEATURE_SENDMAIL_MAILX
	// have attachments? -> compose multipart MIME
	if (opt_attachments) {
		const char *fmt;
		const char *p;
		char *q;

		printf(
			"Mime-Version: 1.0\r\n"
			"%smultipart/mixed; boundary=\"%s\"\r\n"
			, "Content-Type: "
			, boundary
		);

		// body is pseudo attachment read from stdin in first turn
		llist_add_to(&opt_attachments, (char *)"-");

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

	// no attachments? -> just dump message
	} else
#endif
	{
		char *s;
		// terminate headers
		printf("\r\n");
		// put plain text respecting leading dots
		while ((s = xmalloc_fgetline(fp0)) != NULL) {
			// escape leading dots
			// N.B. this feature is implied even if no -i (-oi) switch given
			// N.B. we need to escape the leading dot regardless of
			// whether it is single or not character on the line
			if ('.' == s[0] /*&& '\0' == s[1] */)
				printf(".");
			// dump read line
			printf("%s\r\n", s);
		}
	}

	// leave "put message" mode
	smtp_check(".", 250);
	// ... and say goodbye
	smtp_check("QUIT", 221);
	// cleanup
	if (ENABLE_FEATURE_CLEAN_UP)
		fclose(fp0);

	return EXIT_SUCCESS;
}
