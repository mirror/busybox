/* vi: set sw=4 ts=4: */
/*
 * bare bones version of lpr & lpq: BSD printing utilities
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Original idea and code:
 *      Walter Harms <WHarms@bfs.de>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 *
 * See RFC 1179 for propocol description.
 */
#include "libbb.h"

/*
 * LPD returns binary 0 on success.
 * Otherwise it returns error message.
 */
static void get_response_or_say_and_die(const char *errmsg)
{
	char buf = ' ';

	fflush(stdout);

	safe_read(STDOUT_FILENO, &buf, 1);
	if ('\0' != buf) {
		// request has failed
		bb_error_msg("error while %s. Server said:", errmsg);
		safe_write(STDERR_FILENO, &buf, 1);
		logmode = 0; /* no errors from bb_copyfd_eof() */
		bb_copyfd_eof(STDOUT_FILENO, STDERR_FILENO);
		xfunc_die();
	}
}

int lpqr_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int lpqr_main(int argc, char *argv[])
{
	enum {
		OPT_P           = 1 << 0, // -P queue[@host[:port]]. If no -P is given use $PRINTER, then "lp@localhost:515"
		OPT_U           = 1 << 1, // -U username

		LPR_V           = 1 << 2, // -V: be verbose
		LPR_h           = 1 << 3, // -h: want banner printed    
		LPR_C           = 1 << 4, // -C class: job "class" (? supposedly printed on banner)
		LPR_J           = 1 << 5, // -J title: the job title for the banner page
		LPR_m           = 1 << 6, // -m: send mail back to user

		LPQ_SHORT_FMT   = 1 << 2, // -s: short listing format
		LPQ_DELETE      = 1 << 3, // -d: delete job(s)
		LPQ_FORCE       = 1 << 4, // -f: force waiting job(s) to be printed
	};
	char tempfile[sizeof("/tmp/lprXXXXXX")];
	const char *job_title;
	const char *printer_class = "";   // printer class, max 32 char
	const char *queue;                // name of printer queue
	const char *server = "localhost"; // server[:port] of printer queue
	char *hostname;
	// N.B. IMHO getenv("USER") can be way easily spoofed!
	const char *user = bb_getpwuid(NULL, -1, getuid());
	unsigned job;
	unsigned opts;
	int old_stdout, fd;

	// parse options
	// TODO: set opt_complementary: s,d,f are mutually exclusive
	opts = getopt32(argv,
		(/*lp*/'r' == applet_name[2]) ? "P:U:VhC:J:m" : "P:U:sdf"
		, &queue, &user
		, &printer_class, &job_title
	);
	argv += optind;

	// if queue is not specified -> use $PRINTER
	if (!(opts & OPT_P))
		queue = getenv("PRINTER");
	// if queue is still not specified ->
	if (!queue) {
		// ... queue defaults to "lp"
		// server defaults to "localhost"
		queue = "lp";
	// if queue is specified ->
	} else {
		// queue name is to the left of '@'
		char *s = strchr(queue, '@');
		if (s) {
			// server name is to the right of '@'
			*s = '\0';
			server = s + 1;
		}
	}

	// do connect
	fd = create_and_connect_stream_or_die(server, 515);
	// play with descriptors to save space: fdprintf > printf
	old_stdout = dup(STDOUT_FILENO);
	xmove_fd(fd, STDOUT_FILENO);

	//
	// LPQ ------------------------
	//
	if (/*lp*/'q' == applet_name[2]) {
		char cmd;
		// force printing of every job still in queue
		if (opts & LPQ_FORCE) {
			cmd = 1;
			goto command;
		// delete job(s)
		} else if (opts & LPQ_DELETE) {
			printf("\x5" "%s %s", queue, user);
			while (*argv) {
				printf(" %s", *argv++);
			}
			bb_putchar('\n');
		// dump current jobs status
		// N.B. periodical polling should be achieved
		// via "watch -n delay lpq"
		// They say it's the UNIX-way :)
		} else {
			cmd = (opts & LPQ_SHORT_FMT) ? 3 : 4;
 command:
			printf("%c" "%s\n", cmd, queue);
			bb_copyfd_eof(STDOUT_FILENO, old_stdout);
		}

		return EXIT_SUCCESS;
	}

	//
	// LPR ------------------------
	//
	if (opts & LPR_V)
		bb_error_msg("connected to server");

	job = getpid() % 1000;
	// TODO: when do finally we invent char *xgethostname()?!!
	hostname = xzalloc(MAXHOSTNAMELEN+1);
	gethostname(hostname, MAXHOSTNAMELEN);

	// no files given on command line? -> use stdin
	if (!*argv)
		*--argv = (char *)"-";

	printf("\x2" "%s\n", queue);
	get_response_or_say_and_die("setting queue");

	// process files
	do {
		struct stat st;
		char *c;
		char *remote_filename;
		char *controlfile;

		// if data file is stdin, we need to dump it first
		if (LONE_DASH(*argv)) {
			strcpy(tempfile, "/tmp/lprXXXXXX");
			fd = mkstemp(tempfile);
			if (fd < 0)
				bb_perror_msg_and_die("mkstemp");
			bb_copyfd_eof(STDIN_FILENO, fd);
			xlseek(fd, 0, SEEK_SET);
			*argv = (char*)bb_msg_standard_input;
		} else {
			fd = xopen(*argv, O_RDONLY);
		}

		/* "The name ... should start with ASCII "cfA",
		 * followed by a three digit job number, followed
		 * by the host name which has constructed the file."
		 * We supply 'c' or 'd' as needed for control/data file. */
		remote_filename = xasprintf("fA%03u%s", job, hostname);

		// create control file
		// TODO: all lines but 2 last are constants! How we can use this fact?
		controlfile = xasprintf(
			"H" "%.32s\n" "P" "%.32s\n" /* H HOST, P USER */
			"C" "%.32s\n" /* C CLASS - printed on banner page (if L cmd is also given) */
			"J" "%.99s\n" /* J JOBNAME */
			/* "class name for banner page and job name
			 * for banner page commands must precede L command" */
			"L" "%.32s\n" /* L USER - print banner page, with given user's name */
			"M" "%.32s\n" /* M WHOM_TO_MAIL */
			"l" "d%.31s\n" /* l DATA_FILE_NAME ("dfAxxx") */
			, hostname, user
			, printer_class /* can be "" */
			, ((opts & LPR_J) ? job_title : *argv)
			, (opts & LPR_h) ? user : ""
			, (opts & LPR_m) ? user : ""
			, remote_filename
		);
		// delete possible "\nX\n" patterns
		while ((c = strchr(controlfile, '\n')) != NULL && c[1] && c[2] == '\n')
			memmove(c, c+2, strlen(c+1)); /* strlen(c+1) == strlen(c+2) + 1 */

		// send control file
		if (opts & LPR_V)
			bb_error_msg("sending control file");
		/* "Once all of the contents have
		 * been delivered, an octet of zero bits is sent as
		 * an indication that the file being sent is complete.
		 * A second level of acknowledgement processing
		 * must occur at this point." */
		printf("\x2" "%u %s\n" "c%s" "%c",
				(unsigned)strlen(controlfile),
				remote_filename, controlfile, '\0');
		get_response_or_say_and_die("sending control file");

		// send data file, with name "dfaXXX"
		if (opts & LPR_V)
			bb_error_msg("sending data file");
		st.st_size = 0; /* paranoia: fstat may theoretically fail */
		fstat(fd, &st);
		printf("\x3" "%"OFF_FMT"u d%s\n", st.st_size, remote_filename);
		if (bb_copyfd_size(fd, STDOUT_FILENO, st.st_size) != st.st_size) {
			// We're screwed. We sent less bytes than we advertised.
			bb_error_msg_and_die("local file changed size?!");
		}
		bb_putchar('\0');
		get_response_or_say_and_die("sending data file");

		// delete temporary file if we dumped stdin
		if (*argv == (char*)bb_msg_standard_input)
			unlink(tempfile);

		// cleanup
		close(fd);
		free(remote_filename);
		free(controlfile);

		// next, please!
		job = (job + 1) % 1000;
	} while (*++argv);

	return EXIT_SUCCESS;
}
