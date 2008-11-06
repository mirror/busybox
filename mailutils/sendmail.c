/* vi: set sw=4 ts=4: */
/*
 * bare bones sendmail
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include "mail.h"

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
		if (timeout)
			alarm(0);
		free(answer);
		if (-1 == code || n == code)
			return n;
	}
	bb_error_msg_and_die("%s failed", msg);
}

static int smtp_check(const char *fmt, int code)
{
	return smtp_checkp(fmt, NULL, code);
}

// strip argument of bad chars
static char *sane_address(char *str)
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
#if ENABLE_FEATURE_SENDMAIL_MAILXX
	llist_t *opt_carboncopies = NULL;
	char *opt_errors_to;
#endif
#endif
	char *opt_connect = opt_connect;
	char *opt_from, *opt_fullname;
	char *boundary;
	llist_t *l;
	llist_t *headers = NULL;
	char *domain = sane_address(safe_getdomainname());
	int code;

	enum {
		OPT_w = 1 << 0,         // network timeout
		OPT_t = 1 << 1,         // read message for recipients
		OPT_N = 1 << 2,         // request notification
		OPT_f = 1 << 3,         // sender address
		OPT_F = 1 << 4,         // sender name, overrides $NAME
		OPT_s = 1 << 5,         // subject
		OPT_j = 1 << 6,         // assumed charset
		OPT_a = 1 << 7,         // attachment(s)
		OPT_H = 1 << 8,         // use external connection helper
		OPT_S = 1 << 9,         // specify connection string
		OPT_c = 1 << 10,        // carbon copy
		OPT_e = 1 << 11,        // errors-to address
	};

	// init global variables
	INIT_G();

	// save initial stdin since body is piped!
	xdup2(STDIN_FILENO, 3);
	G.fp0 = fdopen(3, "r");

	// parse options
	opt_complementary = "w+" USE_FEATURE_SENDMAIL_MAILX(":a::H--S:S--H") USE_FEATURE_SENDMAIL_MAILXX(":c::");
	opts = getopt32(argv,
		"w:t" "N:f:F:" USE_FEATURE_SENDMAIL_MAILX("s:j:a:H:S:") USE_FEATURE_SENDMAIL_MAILXX("c:e:")
		"X:V:vq:R:O:o:nmL:Iih:GC:B:b:A:" // postfix compat only, ignored
		// r:Q:p:M:Dd are candidates from another man page. TODO?
		"46E", // ssmtp introduces another quirks. TODO?: -a[upm] (user, pass, method) to be supported
		&timeout /* -w */, NULL, &opt_from, &opt_fullname,
		USE_FEATURE_SENDMAIL_MAILX(&opt_subject, &G.opt_charset, &opt_attachments, &opt_connect, &opt_connect,)
		USE_FEATURE_SENDMAIL_MAILXX(&opt_carboncopies, &opt_errors_to,)
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	);
	//argc -= optind;
	argv += optind;

	// connect to server

#if ENABLE_FEATURE_SENDMAIL_MAILX
	// N.B. -H and -S are mutually exclusive so they do not spoil opt_connect
	// connection helper ordered? ->
	if (opts & OPT_H) {
		const char *args[] = { "sh", "-c", opt_connect, NULL };
		// plug it in
		launch_helper(args);
	// vanilla connection
	} else
#endif
	{
		int fd;
		// host[:port] not explicitly specified ? -> use $SMTPHOST
		// no $SMTPHOST ? -> use localhost
		if (!(opts & OPT_S)) {
			opt_connect = getenv("SMTPHOST");
			if (!opt_connect)
				opt_connect = (char *)"127.0.0.1";
		}
		// do connect
		fd = create_and_connect_stream_or_die(opt_connect, 25);
		// and make ourselves a simple IO filter
		xmove_fd(fd, STDIN_FILENO);
		xdup2(STDIN_FILENO, STDOUT_FILENO);
	}
	// N.B. from now we know nothing about network :)

	// wait for initial server OK
	// N.B. if we used openssl the initial 220 answer is already swallowed during openssl TLS init procedure
	// so we need to push the server to see whether we are ok
	code = smtp_check("NOOP", -1);
	// 220 on plain connection, 250 on openssl-helped TLS session
	if (220 == code)
		smtp_check(NULL, 250); // reread the code to stay in sync
	else if (250 != code)
		bb_error_msg_and_die("INIT failed");

	// we should start with modern EHLO
	if (250 != smtp_checkp("EHLO %s", domain, -1)) {
		smtp_checkp("HELO %s", domain, 250);
	}

	// set sender
	// N.B. we have here a very loosely defined algotythm
	// since sendmail historically offers no means to specify secrets on cmdline.
	// 1) server can require no authentication ->
	//	we must just provide a (possibly fake) reply address.
	// 2) server can require AUTH ->
	//	we must provide valid username and password along with a (possibly fake) reply address.
	//	For the sake of security username and password are to be read either from console or from a secured file.
	//	Since reading from console may defeat usability, the solution is either to read from a predefined
	//	file descriptor (e.g. 4), or again from a secured file.

	// got no sender address? -> use system username as a resort
	if (!(opts & OPT_f)) {
		// N.B. IMHO getenv("USER") can be way easily spoofed!
		G.user = bb_getpwuid(NULL, -1, getuid());
		opt_from = xasprintf("%s@%s", G.user, domain);
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		free(domain);

	code = -1; // first try softly without authentication
	while (250 != smtp_checkp("MAIL FROM:<%s>", opt_from, code)) {
		// MAIL FROM failed -> authentication needed
		if (334 == smtp_check("AUTH LOGIN", -1)) {
			// we must read credentials
			get_cred_or_die(4);
			encode_base64(NULL, G.user, NULL);
			smtp_check("", 334);
			encode_base64(NULL, G.pass, NULL);
			smtp_check("", 235);
		}
		// authenticated OK? -> retry to set sender
		// but this time die on failure!
		code = 250;
	}

	// recipients specified as arguments
	while (*argv) {
		char *s = sane_address(*argv);
		// loose test on email address validity
//		if (strchr(s, '@')) {
			rcptto(s);
			llist_add_to_end(&headers, xasprintf("To: %s", s));
//		}
		argv++;
	}

#if ENABLE_FEATURE_SENDMAIL_MAILXX
	// carbon copies recipients specified as -c options
	for (l = opt_carboncopies; l; l = l->link) {
		char *s = sane_address(l->data);
		// loose test on email address validity
//		if (strchr(s, '@')) {
			rcptto(s);
			// TODO: do we ever need to mangle the message?
			//llist_add_to_end(&headers, xasprintf("Cc: %s", s));
//		}
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
		while ((s = xmalloc_fgetline(G.fp0)) != NULL) {
			if (0 == strncasecmp("To: ", s, 4) || 0 == strncasecmp("Cc: ", s, 4)) {
				rcptto(sane_address(s+4));
				llist_add_to_end(&headers, s);
			} else if (0 == strncasecmp("Bcc: ", s, 5)) {
				rcptto(sane_address(s+5));
				free(s);
				// N.B. Bcc vanishes from headers!
			} else if (0 == strncmp("Subject: ", s, 9)) {
				// we read subject -> use it verbatim unless it is specified
				// on command line
				if (!(opts & OPT_s))
					llist_add_to_end(&headers, s);
				else
					free(s);
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
	if (opts & OPT_s) {
		printf("Subject: ");
		if (opts & OPT_j) {
			printf("=?%s?B?", G.opt_charset);
			encode_base64(NULL, opt_subject, NULL);
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
	boundary = xasprintf("%d=_%d-%d", rand(), rand(), rand());

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
		p = G.opt_charset;
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
			encode_base64(l->data, (const char *)G.fp0, "\r");
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
		while ((s = xmalloc_fgetline(G.fp0)) != NULL) {
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
		fclose(G.fp0);

	return EXIT_SUCCESS;
}
