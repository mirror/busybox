/* vi: set sw=4 ts=4: */
/*
 * ftpget 
 *  
 * Mini implementation of FTP to retrieve a remote file.
 *
 * Copyright (C) 2002 Jeff Angielski, The PTR Group <jeff@theptrgroup.com>
 * Copyright (C) 2002 Glenn McGrath <bug1@optushome.com.au>
 *
 * Based on wget.c by Chip Rosenthal Covad Communications
 * <chip@laserlink.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netdb.h>

#include "busybox.h"

typedef struct ftp_host_info_s {
	char *host;
	char *port;
	char *user;
	char *password;
} ftp_host_info_t;

static char verbose_flag;
static char do_continue = 0;

static int copyfd_chunk(int fd1, int fd2, const off_t chunksize)
{
	size_t nread;
	size_t nwritten;
	size_t size;
	size_t remaining;
	char buffer[BUFSIZ];

	if (chunksize) {
		remaining = chunksize;
	} else {
		remaining = -1;
	}

	do {
		if ((chunksize > BUFSIZ) || (chunksize == 0)) {
			size = BUFSIZ;
		} else {
			size = chunksize;
		}

		nread = safe_read(fd1, buffer, size);

		if (nread <= 0) {
			if (chunksize) {
				perror_msg_and_die("read error");
			} else {
				return(0);
			}
		}

		nwritten = full_write(fd2, buffer, nread);

		if (nwritten != nread) {
			error_msg_and_die("Unable to write all data");
		}

		if (chunksize) {
			remaining -= nwritten;
		}
	} while (remaining != 0);

	return 0;
}

static ftp_host_info_t *ftp_init(void)
{
	ftp_host_info_t *host;

	host = xcalloc(1, sizeof(ftp_host_info_t));

	/* Set the default port */
	if (getservbyname("ftp", "tcp")) {
		host->port = "ftp";
	} else {
		host->port = "21";
	}
	host->user = "anonymous";
	host->password = "busybox@";

	return(host);
}

static int ftpcmd(const char *s1, const char *s2, FILE *stream, char *buf)
{
	if (verbose_flag) {
		error_msg("cmd %s%s", s1, s2);
	}

	if (s1) {
		if (s2) {
			fprintf(stream, "%s%s\n", s1, s2);
		} else {
			fprintf(stream, "%s\n", s1);
		}
	}

	do {
		if (fgets(buf, 510, stream) == NULL) {
			perror_msg_and_die("fgets()");
		}
	} while (! isdigit(buf[0]) || buf[3] != ' ');

	return atoi(buf);
}

static int xconnect_ftpdata(const char *target_host, const char *buf)
{
	char *buf_ptr;
	char data_port[6];
	unsigned short port_num;

	buf_ptr = strrchr(buf, ',');
	*buf_ptr = '\0';
	port_num = atoi(buf_ptr + 1);

	buf_ptr = strrchr(buf, ',');
	*buf_ptr = '\0';
	port_num += atoi(buf_ptr + 1) * 256;

	sprintf(data_port, "%d", port_num);
	return(xconnect(target_host, data_port));
}

static FILE *ftp_login(ftp_host_info_t *server)
{
	FILE *control_stream;
	char buf[512];
	int control_fd;

	/* Connect to the command socket */
	control_fd = xconnect(server->host, server->port);
	control_stream = fdopen(control_fd, "r+");
	if (control_stream == NULL) {
		perror_msg_and_die("Couldnt open control stream");
	}

	if (ftpcmd(NULL, NULL, control_stream, buf) != 220) {
		error_msg_and_die("%s", buf + 4);
	}

	/*  Login to the server */
	switch (ftpcmd("USER ", server->user, control_stream, buf)) {
	case 230:
		break;
	case 331:
		if (ftpcmd("PASS ", server->password, control_stream, buf) != 230) {
			error_msg_and_die("PASS error: %s", buf + 4);
		}
		break;
	default:
		error_msg_and_die("USER error: %s", buf + 4);
	}

	ftpcmd("TYPE I", NULL, control_stream, buf);

	return(control_stream);
}

#ifdef CONFIG_FTPGET
static int ftp_recieve(FILE *control_stream, const char *host, const char *local_path, char *server_path)
{
	char *filename;
	char *local_file;
	char buf[512];
	off_t filesize = 0;
	int fd_data;
	int fd_local;
	off_t beg_range = 0;

	filename = get_last_path_component(server_path);
	local_file = concat_path_file(local_path, filename);

	/* Connect to the data socket */
	if (ftpcmd("PASV", NULL, control_stream, buf) != 227) {
		error_msg_and_die("PASV error: %s", buf + 4);
	}
	fd_data = xconnect_ftpdata(host, buf);

	if (ftpcmd("SIZE ", server_path, control_stream, buf) == 213) {
		filesize = atol(buf + 4);
	}

	/* only make a local file if we know that one exists on the remote server */
	if (do_continue) {
		fd_local = xopen(local_file, O_APPEND | O_WRONLY);
	} else {
		fd_local = xopen(local_file, O_CREAT | O_TRUNC | O_WRONLY);
	}

	if (do_continue) {
		struct stat sbuf;
		if (fstat(fd_local, &sbuf) < 0) {
			perror_msg_and_die("fstat()");
		}
		if (sbuf.st_size > 0) {
			beg_range = sbuf.st_size;
		} else {
			do_continue = 0;
		}
	}

	if (do_continue) {
		sprintf(buf, "REST %ld", beg_range);
		if (ftpcmd(buf, NULL, control_stream, buf) != 350) {
			do_continue = 0;
		} else {
			filesize -= beg_range;
		}
	}

	if (ftpcmd("RETR ", server_path, control_stream, buf) > 150) {
		error_msg_and_die("RETR error: %s", buf + 4);
	}

	/* Copy the file */
	copyfd_chunk(fd_data, fd_local, filesize);

	/* close it all down */
	close(fd_data);
	if (ftpcmd(NULL, NULL, control_stream, buf) != 226) {
		error_msg_and_die("ftp error: %s", buf + 4);
	}
	ftpcmd("QUIT", NULL, control_stream, buf);
	
	return(EXIT_SUCCESS);
}
#endif

#ifdef CONFIG_FTPPUT
static int ftp_send(FILE *control_stream, const char *host, const char *local_path, char *server_path)
{
	struct stat sbuf;
	char buf[512];
	int fd_data;
	int fd_local;
	int response;

	/*  Connect to the data socket */
	if (ftpcmd("PASV", NULL, control_stream, buf) != 227) {
		error_msg_and_die("PASV error: %s", buf + 4);
	}
	fd_data = xconnect_ftpdata(host, buf);

	if (ftpcmd("CWD ", server_path, control_stream, buf) != 250) {
		error_msg_and_die("CWD error: %s", buf + 4);
	}

	/* get the local file */
	fd_local = xopen(local_path, O_RDONLY);
	fstat(fd_local, &sbuf);

	sprintf(buf, "ALLO %lu", sbuf.st_size);
	response = ftpcmd(buf, NULL, control_stream, buf);
	switch (response) {
	case 200:
	case 202:
		break;
	default:
		close(fd_local);
		error_msg_and_die("ALLO error: %s", buf + 4);
		break;
	}

	response = ftpcmd("STOR ", local_path, control_stream, buf);
	switch (response) {
	case 125:
	case 150:
		break;
	default:
		close(fd_local);
		error_msg_and_die("STOR error: %s", buf + 4);
	}

	/* transfer the file  */
	copyfd_chunk(fd_local, fd_data, 0);

	/* close it all down */
	close(fd_data);
	if (ftpcmd(NULL, NULL, control_stream, buf) != 226) {
		error_msg_and_die("error: %s", buf + 4);
	}
	ftpcmd("QUIT", NULL, control_stream, buf);

	return(EXIT_SUCCESS);
}
#endif

int ftpgetput_main(int argc, char **argv)
{
	/* content-length of the file */
	int option_index = -1;
	int opt;

	/* socket to ftp server */
	FILE *control_stream;

	/* continue a prev transfer (-c) */
	ftp_host_info_t *server;

	int (*ftp_action)(FILE *, const char *, const char *, char *) = NULL;

	struct option long_options[] = {
		{"username", 1, NULL, 'u'},
		{"password", 1, NULL, 'p'},
		{"port", 1, NULL, 'P'},
		{"continue", 1, NULL, 'c'},
		{"verbose", 0, NULL, 'v'},
		{0, 0, 0, 0}
	};

#ifdef CONFIG_FTPPUT
	if (applet_name[3] == 'p') {
		ftp_action = ftp_send;
	} 
#endif
#ifdef CONFIG_FTPGET
	if (applet_name[3] == 'g') {
		ftp_action = ftp_recieve;
	}
#endif

	/* Set default values */
	server = ftp_init();
	verbose_flag = 0;

	/* 
	 * Decipher the command line 
	 */
	while ((opt = getopt_long(argc, argv, "u:p:P:cv", long_options, &option_index)) != EOF) {
		switch(opt) {
		case 'c':
			do_continue = 1;
			break;
		case 'u':
			server->user = optarg;
			break;
		case 'p':
			server->password = optarg;
			break;
		case 'P':
			server->port = optarg;
			break;
		case 'v':
			verbose_flag = 1;
			break;
		default:
			show_usage();
		}
	}

	/*
	 * Process the non-option command line arguments
	 */
	if (argc - optind != 3) {
		show_usage();
	}

	/*  Connect/Setup/Configure the FTP session */
	server->host = argv[optind];
	control_stream = ftp_login(server);

	return(ftp_action(control_stream, argv[optind], argv[optind + 1], argv[optind + 2]));
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
