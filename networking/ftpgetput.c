/* vi: set sw=4 ts=4: */
/*
 * ftpget
 *
 * Mini implementation of FTP to retrieve a remote file.
 *
 * Copyright (C) 2002 Jeff Angielski, The PTR Group <jeff@theptrgroup.com>
 * Copyright (C) 2002 Glenn McGrath <bug1@iinet.net.au>
 *
 * Based on wget.c by Chip Rosenthal Covad Communications
 * <chip@laserlink.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <getopt.h>

typedef struct ftp_host_info_s {
	char *user;
	char *password;
	struct len_and_sockaddr *lsa;
} ftp_host_info_t;

static smallint verbose_flag;
static smallint do_continue;

static void ftp_die(const char *msg, const char *remote) ATTRIBUTE_NORETURN;
static void ftp_die(const char *msg, const char *remote)
{
	/* Guard against garbage from remote server */
	const char *cp = remote;
	while (*cp >= ' ' && *cp < '\x7f') cp++;
	bb_error_msg_and_die("unexpected server response%s%s: %.*s",
			msg ? " to " : "", msg ? msg : "",
			(int)(cp - remote), remote);
}


static int ftpcmd(const char *s1, const char *s2, FILE *stream, char *buf)
{
	unsigned n;
	if (verbose_flag) {
		bb_error_msg("cmd %s %s", s1, s2);
	}

	if (s1) {
		if (s2) {
			fprintf(stream, "%s %s\r\n", s1, s2);
		} else {
			fprintf(stream, "%s\r\n", s1);
		}
	}
	do {
		char *buf_ptr;

		if (fgets(buf, 510, stream) == NULL) {
			bb_perror_msg_and_die("fgets");
		}
		buf_ptr = strstr(buf, "\r\n");
		if (buf_ptr) {
			*buf_ptr = '\0';
		}
	} while (!isdigit(buf[0]) || buf[3] != ' ');

	buf[3] = '\0';
	n = xatou(buf);
	buf[3] = ' ';
	return n;
}

static int xconnect_ftpdata(ftp_host_info_t *server, char *buf)
{
	char *buf_ptr;
	unsigned short port_num;

	/* Response is "NNN garbageN1,N2,N3,N4,P1,P2[)garbage]
	 * Server's IP is N1.N2.N3.N4 (we ignore it)
	 * Server's port for data connection is P1*256+P2 */
	buf_ptr = strrchr(buf, ')');
	if (buf_ptr) *buf_ptr = '\0';

	buf_ptr = strrchr(buf, ',');
	*buf_ptr = '\0';
	port_num = xatoul_range(buf_ptr + 1, 0, 255);

	buf_ptr = strrchr(buf, ',');
	*buf_ptr = '\0';
	port_num += xatoul_range(buf_ptr + 1, 0, 255) * 256;

	set_nport(server->lsa, htons(port_num));
	return xconnect_stream(server->lsa);
}

static FILE *ftp_login(ftp_host_info_t *server)
{
	FILE *control_stream;
	char buf[512];

	/* Connect to the command socket */
	control_stream = fdopen(xconnect_stream(server->lsa), "r+");
	if (control_stream == NULL) {
		/* fdopen failed - extremely unlikely */
		bb_perror_nomsg_and_die();
	}

	if (ftpcmd(NULL, NULL, control_stream, buf) != 220) {
		ftp_die(NULL, buf);
	}

	/*  Login to the server */
	switch (ftpcmd("USER", server->user, control_stream, buf)) {
	case 230:
		break;
	case 331:
		if (ftpcmd("PASS", server->password, control_stream, buf) != 230) {
			ftp_die("PASS", buf);
		}
		break;
	default:
		ftp_die("USER", buf);
	}

	ftpcmd("TYPE I", NULL, control_stream, buf);

	return control_stream;
}

#if !ENABLE_FTPGET
int ftp_receive(ftp_host_info_t *server, FILE *control_stream,
		const char *local_path, char *server_path);
#else
static
int ftp_receive(ftp_host_info_t *server, FILE *control_stream,
		const char *local_path, char *server_path)
{
	char buf[512];
/* I think 'filesize' usage here is bogus. Let's see... */
	//off_t filesize = -1;
#define filesize ((off_t)-1)
	int fd_data;
	int fd_local = -1;
	off_t beg_range = 0;

	/* Connect to the data socket */
	if (ftpcmd("PASV", NULL, control_stream, buf) != 227) {
		ftp_die("PASV", buf);
	}
	fd_data = xconnect_ftpdata(server, buf);

	if (ftpcmd("SIZE", server_path, control_stream, buf) == 213) {
		//filesize = BB_STRTOOFF(buf + 4, NULL, 10);
		//if (errno || filesize < 0)
		//	ftp_die("SIZE", buf);
	} else {
		do_continue = 0;
	}

	if (LONE_DASH(local_path)) {
		fd_local = STDOUT_FILENO;
		do_continue = 0;
	}

	if (do_continue) {
		struct stat sbuf;
		if (lstat(local_path, &sbuf) < 0) {
			bb_perror_msg_and_die("lstat");
		}
		if (sbuf.st_size > 0) {
			beg_range = sbuf.st_size;
		} else {
			do_continue = 0;
		}
	}

	if (do_continue) {
		sprintf(buf, "REST %"OFF_FMT"d", beg_range);
		if (ftpcmd(buf, NULL, control_stream, buf) != 350) {
			do_continue = 0;
		} else {
			//if (filesize != -1)
			//	filesize -= beg_range;
		}
	}

	if (ftpcmd("RETR", server_path, control_stream, buf) > 150) {
		ftp_die("RETR", buf);
	}

	/* only make a local file if we know that one exists on the remote server */
	if (fd_local == -1) {
		if (do_continue) {
			fd_local = xopen(local_path, O_APPEND | O_WRONLY);
		} else {
			fd_local = xopen(local_path, O_CREAT | O_TRUNC | O_WRONLY);
		}
	}

	/* Copy the file */
	if (filesize != -1) {
		if (bb_copyfd_size(fd_data, fd_local, filesize) == -1)
			return EXIT_FAILURE;
	} else {
		if (bb_copyfd_eof(fd_data, fd_local) == -1)
			return EXIT_FAILURE;
	}

	/* close it all down */
	close(fd_data);
	if (ftpcmd(NULL, NULL, control_stream, buf) != 226) {
		ftp_die(NULL, buf);
	}
	ftpcmd("QUIT", NULL, control_stream, buf);

	return EXIT_SUCCESS;
}
#endif

#if !ENABLE_FTPPUT
int ftp_send(ftp_host_info_t *server, FILE *control_stream,
		const char *server_path, char *local_path);
#else
static
int ftp_send(ftp_host_info_t *server, FILE *control_stream,
		const char *server_path, char *local_path)
{
	struct stat sbuf;
	char buf[512];
	int fd_data;
	int fd_local;
	int response;

	/*  Connect to the data socket */
	if (ftpcmd("PASV", NULL, control_stream, buf) != 227) {
		ftp_die("PASV", buf);
	}
	fd_data = xconnect_ftpdata(server, buf);

	/* get the local file */
	fd_local = STDIN_FILENO;
	if (NOT_LONE_DASH(local_path)) {
		fd_local = xopen(local_path, O_RDONLY);
		fstat(fd_local, &sbuf);

		sprintf(buf, "ALLO %"OFF_FMT"u", sbuf.st_size);
		response = ftpcmd(buf, NULL, control_stream, buf);
		switch (response) {
		case 200:
		case 202:
			break;
		default:
			close(fd_local);
			ftp_die("ALLO", buf);
			break;
		}
	}
	response = ftpcmd("STOR", server_path, control_stream, buf);
	switch (response) {
	case 125:
	case 150:
		break;
	default:
		close(fd_local);
		ftp_die("STOR", buf);
	}

	/* transfer the file  */
	if (bb_copyfd_eof(fd_local, fd_data) == -1) {
		exit(EXIT_FAILURE);
	}

	/* close it all down */
	close(fd_data);
	if (ftpcmd(NULL, NULL, control_stream, buf) != 226) {
		ftp_die("close", buf);
	}
	ftpcmd("QUIT", NULL, control_stream, buf);

	return EXIT_SUCCESS;
}
#endif

#define FTPGETPUT_OPT_CONTINUE	1
#define FTPGETPUT_OPT_VERBOSE	2
#define FTPGETPUT_OPT_USER	4
#define FTPGETPUT_OPT_PASSWORD	8
#define FTPGETPUT_OPT_PORT	16

#if ENABLE_FEATURE_FTPGETPUT_LONG_OPTIONS
static const struct option ftpgetput_long_options[] = {
	{ "continue", 1, NULL, 'c' },
	{ "verbose", 0, NULL, 'v' },
	{ "username", 1, NULL, 'u' },
	{ "password", 1, NULL, 'p' },
	{ "port", 1, NULL, 'P' },
	{ 0, 0, 0, 0 }
};
#endif

int ftpgetput_main(int argc, char **argv)
{
	/* content-length of the file */
	unsigned opt;
	const char *port = "ftp";
	/* socket to ftp server */
	FILE *control_stream;
	/* continue previous transfer (-c) */
	ftp_host_info_t *server;

#if ENABLE_FTPPUT && !ENABLE_FTPGET
# define ftp_action ftp_send
#elif ENABLE_FTPGET && !ENABLE_FTPPUT
# define ftp_action ftp_receive
#else
	int (*ftp_action)(ftp_host_info_t *, FILE *, const char *, char *) = ftp_send;
	/* Check to see if the command is ftpget or ftput */
	if (applet_name[3] == 'g') {
		ftp_action = ftp_receive;
	}
#endif

	/* Set default values */
	server = xmalloc(sizeof(*server));
	server->user = "anonymous";
	server->password = "busybox@";

	/*
	 * Decipher the command line
	 */
#if ENABLE_FEATURE_FTPGETPUT_LONG_OPTIONS
	applet_long_options = ftpgetput_long_options;
#endif
	opt_complementary = "=3"; /* must have 3 params */
	opt = getopt32(argc, argv, "cvu:p:P:", &server->user, &server->password, &port);
	argv += optind;

	/* Process the non-option command line arguments */
	if (opt & FTPGETPUT_OPT_CONTINUE) {
		do_continue = 1;
	}
	if (opt & FTPGETPUT_OPT_VERBOSE) {
		verbose_flag = 1;
	}

	/* We want to do exactly _one_ DNS lookup, since some
	 * sites (i.e. ftp.us.debian.org) use round-robin DNS
	 * and we want to connect to only one IP... */
	server->lsa = host2sockaddr(argv[0], bb_lookup_port(port, "tcp", 21));
	if (verbose_flag) {
		printf("Connecting to %s [%s]\n", argv[0],
			xmalloc_sockaddr2dotted(&server->lsa->sa, server->lsa->len));
	}

	/*  Connect/Setup/Configure the FTP session */
	control_stream = ftp_login(server);

	return ftp_action(server, control_stream, argv[1], argv[2]);
}
