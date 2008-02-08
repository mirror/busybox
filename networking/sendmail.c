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
	} else {
		len = strlen(text);
	}

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
}

static pid_t helper_pid;

static void kill_helper(void)
{
	// TODO!!!: is there more elegant way to terminate child on program failure?
	if (helper_pid > 0)
		kill(helper_pid, SIGTERM);
}

// generic signal handler
static void signal_handler(int signo)
{
	int err;

	if (SIGALRM == signo) {
		kill_helper();
		bb_error_msg_and_die("timed out");
	}

	// SIGCHLD. reap zombies
	if (wait_any_nohang(&err) > 0)
		if (WIFEXITED(err) && WEXITSTATUS(err))
#if ENABLE_FEATURE_SENDMAIL_BLOATY
			bb_error_msg_and_die("child exited (%d)", WEXITSTATUS(err));
#else
			bb_error_msg_and_die("child failed");
#endif
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
	idx = (!helper_pid)*2;
	xdup2(pipes[idx], STDIN_FILENO);
	xdup2(pipes[3-idx], STDOUT_FILENO);
	if (ENABLE_FEATURE_CLEAN_UP)
		for (int i = 4; --i >= 0; )
			if (pipes[i] > STDOUT_FILENO)
				close(pipes[i]);
	if (!helper_pid) {
		// child - try to execute connection helper
		BB_EXECVP(argv[0], (char **)argv);
		_exit(127);
	}
	// parent - check whether child is alive
	sig_catch(SIGCHLD, signal_handler);
	sig_catch(SIGALRM, signal_handler);
	signal_handler(SIGCHLD);
	// child seems OK -> parent goes on
}

static unsigned timeout;

static char *command(const char *fmt, const char *param)
{
	char *msg = (char *)fmt;
	alarm(timeout);
	if (msg) {
//		if (param)
			msg = xasprintf(fmt, param);
		printf("%s\r\n", msg);
	}
	fflush(stdout);
	return msg;
}

static int smtp_checkp(const char *fmt, const char *param, int code)
{
	char *answer;
	char *msg = command(fmt, param);
	// read stdin
	// if the string has a form \d\d\d- -- read next string. E.g. EHLO response
	// parse first bytes to a number
	// if code = -1 then just return this number
	// if code != -1 then checks whether the number equals the code
	// if not equal -> die saying msg
#if ENABLE_FEATURE_SENDMAIL_EHLO
	while ((answer = xmalloc_getline(stdin)) && '-' == answer[3]) ;
#else
	answer = xmalloc_getline(stdin);
#endif
	if (answer) {
		int n = atoi(answer);
		alarm(0);
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(msg);
			free(answer);
		}
		if (-1 == code || n == code) {
			return n;
		}
	}
	kill_helper();
	bb_error_msg_and_die("%s failed", msg);
}

static int smtp_check(const char *fmt, int code)
{
	return smtp_checkp(fmt, NULL, code);
}

static void pop3_checkr(const char *fmt, const char *param, char **ret)
{
	char *msg = command(fmt, param);
	char *answer = xmalloc_getline(stdin);
	if (answer && '+' == answer[0]) {
		alarm(0);
		if (ret)
			*ret = answer;
		else
			free(answer);
		return;
	}
	kill_helper();
	bb_error_msg_and_die("%s failed", msg);
}

static void pop3_check(const char *fmt, const char *param)
{
	pop3_checkr(fmt, param, NULL);
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

static void pop3_message(int fd)
{
	char *answer;
	// read stdin, copy to file fd
	while ((answer = xmalloc_fgets_str(stdin, "\r\n"))) {
		char *s = answer;
		if ('.' == answer[0]) {
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

static const char *args[] = {
	"openssl", "s_client", "-quiet", "-connect", NULL, "-tls1", "-starttls", "smtp", NULL
};
#define opt_connect	args[4]
#define opt_after_connect args[5]

int sendgetmail_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sendgetmail_main(int argc, char **argv)
{
	llist_t *recipients = NULL;
	char *from;
	const char *subject;
	char *charset = (char *)"utf-8";

	const char *opt_user;
	const char *opt_pass;
	const char *opt_timeout;
	const char *opt_chdir;

	enum {
		OPT_C = 1 << 0,         // chdir
		OPT_w = 1 << 1,         // network timeout
		OPT_U = 1 << 2,         // user
		OPT_P = 1 << 3,         // password
		OPT_X = 1 << 4,         // use openssl connection helper

		OPTS_t = 1 << 5,        // sendmail "to"
		OPTF_t = 1 << 5,        // fetchmail "TOP"

		OPTS_f = 1 << 6,        // sendmail "from"
		OPTF_z = 1 << 6,        // fetchmail "delete"

		OPTS_n = 1 << 7,        // notification
		OPTS_s = 1 << 8,        // subject given
		OPTS_c = 1 << 9,        // charset for subject and body
	};

	const char *options;
	unsigned opts;

	// SENDMAIL
	if ('s' == applet_name[0]) {
		// save initial stdin
		xdup2(STDIN_FILENO, INITIAL_STDIN_FILENO);
		// -f must be specified
		// -t may be multiple
		opt_complementary = "-1:f:t::";
		options = "C:w:U:P:X" "t:f:ns:c:";
	// FETCHMAIL
	} else {
		opt_after_connect = NULL;
		opt_complementary = "=1:P";
		options = "C:w:U:P:X" "tz";
	}
	opts = getopt32(argv, options,
		&opt_chdir, &opt_timeout, &opt_user, &opt_pass,
		&recipients, &from, &subject, &charset
	);

	//argc -= optind;
	argv += optind;

	// first argument is remote server[:port]
	opt_connect = *argv++;

	if (opts & OPT_w)
		timeout = xatou(opt_timeout);

	// chdir
	if (opts & OPT_C)
		xchdir(opt_chdir);

	// connect to server
	if (opts & OPT_X) {
		launch_helper(args);
	} else {
		// no connection helper provided -> make plain connect
		int fd = create_and_connect_stream_or_die(opt_connect, 0);
		xmove_fd(fd, STDIN_FILENO);
		xdup2(STDIN_FILENO, STDOUT_FILENO);
	}

	// randomize
	srand(time(NULL));

	// SENDMAIL
	if (recipients) {
		int code;
		char *boundary;

		// wait for initial OK on plain connect
		if (!(opts & OPT_X))
			smtp_check(NULL, 220);

		sane(from);
		// introduce to server
		// should we respect modern (but useless here) EHLO?
		// or should they respect we wanna be tiny?!
		if (!ENABLE_FEATURE_SENDMAIL_EHLO || 250 != smtp_checkp("EHLO %s", from, -1)) {
			smtp_checkp("HELO %s", from, 250);
		}

		// set sender
		// NOTE: if password has not been specified ->
		// no authentication is possible
		code = (opts & OPT_P) ? -1 : 250;
		// first try softly without authentication
		while (250 != smtp_checkp("MAIL FROM:<%s>", from, code)) {
			// MAIL FROM failed -> authentication needed
			// do we have username?
			if (!(opts & OPT_U)) {
				// no! fetch it from "from" option
				//opts |= OPT_U;
				opt_user = xstrdup(from);
				*strchrnul(opt_user, '@') = '\0';
			}
			// now it seems we have username
			// try to authenticate
			if (334 == smtp_check("AUTH LOGIN", -1)) {
				uuencode(NULL, opt_user);
				smtp_check("", 334);
				uuencode(NULL, opt_pass);
				smtp_check("", 235);
			}
			// authenticated -> retry set sender
			// but now die on failure
			code = 250;
		}
		// set recipients
		for (llist_t *to = recipients; to; to = to->link) {
			smtp_checkp("RCPT TO:<%s>", sane(to->data), 250);
		}

		// now put message
		smtp_check("DATA", 354);
		// put address headers
		printf("From: %s\r\n", from);
		for (llist_t *to = recipients; to; to = to->link) {
			printf("To: %s\r\n", to->data);
		}
		// put encoded subject
		if (opts & OPTS_c)
			sane(charset);
		if (opts & OPTS_s) {
			printf("Subject: =?%s?B?", charset);
			uuencode(NULL, subject);
			printf("?=\r\n");
		}
		// put notification
		if (opts & OPTS_n)
			printf("Disposition-Notification-To: %s\r\n", from);
		// put common headers and body start
		boundary = xasprintf("%d-%d-%d", rand(), rand(), rand());
		printf(
			USE_FEATURE_SENDMAIL_BLOATY("X-Mailer: busybox " BB_VER " sendmail\r\n")
			"Message-ID: <%s>\r\n"
			"Mime-Version: 1.0\r\n"
			"%smultipart/mixed; boundary=\"%s\"\r\n"
			, boundary
			, "Content-Type: "
			, boundary
		);
		// put body + attachment(s)
	{
		const char *fmt =
			"\r\n--%s\r\n"
			"%stext/plain; charset=%s\r\n"
			"%s%s\r\n"
			"%s"
		;
		const char *p = charset;
		char *q = (char *)"";
		while (argv[0]) {
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
			uuencode(*argv, NULL);
			if (*(++argv))
				q = bb_get_last_path_component_strip(argv[0]);
		}
	}
		// put terminator
		printf("\r\n--%s--\r\n" "\r\n", boundary);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(boundary);

		// end message and say goodbye
		smtp_check(".", 250);
		smtp_check("QUIT", 221);

	// FETCHMAIL
	} else {
		// authenticate
		char *buf;
		unsigned nmsg;
		if (!(opts & OPT_U)) {
			//opts |= OPT_U;
			opt_user = getenv("USER");
		}
#if ENABLE_FEATURE_FETCHMAIL_APOP
		pop3_checkr(NULL, NULL, &buf);
		// server supports APOP?
		if ('<' == buf[4]) {
			md5_ctx_t md5;
			uint8_t hex[16*2 + 1];
			// yes. compose <stamp><password>
			char *s = strchr(buf, '>');
			if (s)
				strcpy(s+1, opt_pass);
			s = buf+4;
			// get md5 sum of <stamp><password>
			md5_begin(&md5);
			md5_hash(s, strlen(s), &md5);
			md5_end(s, &md5);
			bin2hex(hex, s, 16);
			// APOP
			s = xasprintf("%s %s", opt_user, hex);
			pop3_check("APOP %s", s);
			if (ENABLE_FEATURE_CLEAN_UP) {
				free(s);
				free(buf);
			}
		} else {
#else
		{
			pop3_check(NULL, NULL);
#endif
			// USER
			pop3_check("USER %s", opt_user);
			// PASS
			pop3_check("PASS %s", opt_pass);
		}

		// get statistics
		pop3_checkr("STAT", NULL, &buf);

		// get number of messages
		nmsg = atoi(buf+4);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(buf);

		// lock maildir
		////USE_FEATURE_CLEAN_UP(close)(xopen(".lock", O_CREAT | O_WRONLY | O_TRUNC | O_EXCL));
		
		// make tempnam(dir, salt) respect dir argument
		unsetenv("TMPDIR");

		// TODO: piping thru external filter argv... if *argv

		// cache fetch command
	{
		const char *retr = (opts & OPTF_t) ? "TOP %u 0" : "RETR %u";
		// loop thru messages
		for (; nmsg; nmsg--) {
			int fd;
			char tmp_name[sizeof("tmp/XXXXXX")];
			char new_name[sizeof("new/XXXXXX")];

			// retrieve message in ./tmp
			strcpy(tmp_name, "tmp/XXXXXX");
			fd = mkstemp(tmp_name);
			if (fd < 0)
				bb_perror_msg_and_die("cannot create unique file");
			pop3_check(retr, (const char *)nmsg);
			pop3_message(fd); // NB: closes fd

			// move file to ./new atomically
			strncpy(new_name, "new", 3);
			strcpy(new_name + 3, tmp_name + 3);
			if (rename(tmp_name, new_name) < 0) {
				// rats! such file exists! try to make unique name
				strcpy(new_name + 3, "tmp/XXXXXX" + 3);
				fd = mkstemp(new_name);
				if (fd < 0)
					bb_perror_msg_and_die("cannot create unique file");
				close(fd);
				if (rename(tmp_name, new_name) < 0) {
					// something is very wrong
					bb_perror_msg_and_die("cannot move %s to %s", tmp_name, new_name);
				}
			}

			// delete message from server
			if (opts & OPTF_z)
				pop3_check("DELE %u", (const char*)nmsg);
		}
	}

		// Bye
		pop3_check("QUIT", NULL);

		// unlock maildir
		////unlink(".lock");
	}

	return 0;
}
