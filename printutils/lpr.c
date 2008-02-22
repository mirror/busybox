/* vi: set sw=4 ts=4: */
/*
 * Copyright 2008 Walter Harms (WHarms@bfs.de)
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */
#include "libbb.h"
#include "lpr.h"

static char *mygethostname31(void)
{
	char *s = xzalloc(32);
	if (gethostname(s, 31) < 0)
		bb_perror_msg_and_die("gethostname");
	/* gethostname() does not guarantee NUL-termination. xzalloc does. */
	return s;
}

static int xmkstemp(char *temp_name)
{
	int fd;

	fd = mkstemp(temp_name);
	if (fd < 0)
		bb_perror_msg_and_die("mkstemp");
	return fd;
}

/* lpd server sends NUL byte on success.
 * Else read the errormessage and exit.
 */
static void get_response(int server_sock, const char *emsg)
{
	char buf = '\0';

	safe_read(server_sock, &buf, 1);
	if (buf != '\0') {
		bb_error_msg("%s. Server said:", emsg);
		fputc(buf, stderr);
		logmode = 0; /* no errors from bb_copyfd_eof() */
		bb_copyfd_eof(server_sock, STDERR_FILENO);
		xfunc_die();
	}
}

int lpr_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int lpr_main(int argc, char *argv[])
{
	struct netprint netprint;
	char temp_name[] = "/tmp/lprXXXXXX"; /* for mkstemp */
	char *strings[5];
	const char *netopt;
	const char *jobtitle;
	const char *hostname;
	const char *jobclass;
	char *logname;
	int pid1000;
	int server_sock, tmp_fd;
	unsigned opt;
	enum {
		VERBOSE = 1 << 0,
		USE_HEADER = 1 << 1, /* -h banner or header for this job */
		USE_MAIL = 1 << 2, /* send mail to user@hostname */
		OPT_U = 1 << 3, /* -U <username> */
		OPT_J = 1 << 4, /* -J <title> is the jobtitle for the banner page */
		OPT_C = 1 << 5, /* -C <class> job classification */
		OPT_P = 1 << 6, /* -P <queue[@host[:port]]> */
		/* if no -P is given use $PRINTER, then "lp@localhost:515" */
	};

	/* Set defaults, parse options */
	hostname = mygethostname31();
	netopt = NULL;
	logname = getenv("LOGNAME");
	if (logname == NULL)
		logname = (char*)"user"; /* TODO: getpwuid(getuid())->pw_name? */
	opt = getopt32(argv, "VhmU:J:C:P:",
			&logname, &jobtitle, &jobclass, &netopt);
	argv += optind;
	parse_prt(netopt, &netprint);
	logname = xstrndup(logname, 31);

	/* For stdin we need to save it to a tempfile,
	 * otherwise we can't know the size. */
	tmp_fd = -1;
	if (!*argv) {
		if (jobtitle == NULL)
			jobtitle = (char *) bb_msg_standard_input;

		tmp_fd = xmkstemp(temp_name);
		if (bb_copyfd_eof(STDIN_FILENO, tmp_fd) < 0)
			goto del_temp_file;
		/* TODO: we actually can have deferred write errors... */
		close(tmp_fd);
		argv--; /* back off from NULL */
		*argv = temp_name;
	}

	if (opt & VERBOSE)
		puts("connect to server");
	server_sock = xconnect_stream(netprint.lsa);

	/* "Receive a printer job" command */
	fdprintf(server_sock, "\x2" "%s\n", netprint.queue);
	get_response(server_sock, "set queue failed");

	pid1000 = getpid() % 1000;
	while (*argv) {
		char dfa_name[sizeof("dfAnnn") + 32];
		struct stat st;
		char **sptr;
		unsigned size;
		int fd;

		fd = xopen(*argv, O_RDONLY);

		/* "The name ... should start with ASCII "dfA",
		 * followed by a three digit job number, followed
		 * by the host name which has constructed the file." */
		snprintf(dfa_name, sizeof(dfa_name),
			"dfA%03u%s", pid1000, hostname);
		pid1000 = (pid1000 + 1) % 1000;

		/*
		 * Generate control file contents
		 */
		/* H HOST, P USER, l DATA_FILE_NAME, J JOBNAME */
		strings[0] = xasprintf("H%.32s\n" "P%.32s\n" "l%.32s\n"
			"J%.99s\n",
			hostname, logname, dfa_name,
			!(opt & OPT_J) ? *argv : jobtitle);
		sptr = &strings[1];
		/* C CLASS - printed on banner page (if L cmd is also given) */
		if (opt & OPT_J) /* [1] */
			*sptr++ = xasprintf("C%.32s\n", jobclass);
		/* M WHOM_TO_MAIL */
		if (opt & USE_MAIL) /* [2] */
			*sptr++ = xasprintf("M%.32s\n", logname);
		/* H USER - print banner page, with given user's name */
		if (opt & USE_HEADER) /* [3] */
			*sptr++ = xasprintf("L%.32s\n", logname);
		*sptr = NULL; /* [4] max */

		/* RFC 1179: "LPR servers MUST be able
		 * to receive the control file subcommand first
		 * and SHOULD be able to receive the data file
		 * subcommand first".
		 * Ok, we'll send control file first. */
		size = 0;
		sptr = strings;
		while (*sptr)
			size += strlen(*sptr++);
		if (opt & VERBOSE)
			puts("send control file");
		/* 2: "Receive control file" subcommand */
		fdprintf(server_sock, "\x2" "%u c%s\n", size, dfa_name + 1);
		sptr = strings;
		while (*sptr) {
			xwrite(server_sock, *sptr, strlen(*sptr));
			free(*sptr);
			sptr++;
		}
		free(strings);
		/* "Once all of the contents have
		 * been delivered, an octet of zero bits is sent as
		 * an indication that the file being sent is complete.
		 * A second level of acknowledgement processing
		 * must occur at this point." */
		xwrite(server_sock, "", 1);
		get_response(server_sock, "send control file failed");

		/* Sending data */
		st.st_size = 0; /* paranoia */
		fstat(fd, &st);
		if (opt & VERBOSE)
			puts("send data file");
		/* 3: "Receive data file" subcommand */
		fdprintf(server_sock, "\x3" "%"OFF_FMT"u %s\n", st.st_size, dfa_name);
		/* TODO: if file shrank and we wrote less than st.st_size,
		 * pad output with NUL bytes? Otherwise server won't know
		 * that we are done. */
		if (bb_copyfd_size(fd, server_sock, st.st_size) < 0)
			xfunc_die();
		close(fd);
		xwrite(server_sock, "", 1);
		get_response(server_sock, "send file failed");

		argv++;
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(server_sock);

	if (tmp_fd >= 0) {
 del_temp_file:
		unlink(temp_name);
	}

	return 0;
}
