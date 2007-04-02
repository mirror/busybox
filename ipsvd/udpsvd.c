/* Based on ipsvd utilities written by Gerrit Pape <pape@smarden.org>
 * which are released into public domain by the author.
 * Homepage: http://smarden.sunsite.dk/ipsvd/
 *
 * Copyright (C) 2007 Denis Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/* Based on ipsvd ipsvd-0.12.1. This tcpsvd accepts all options
 * which are supported by one from ipsvd-0.12.1, but not all are
 * functional. See help text at the end of this file for details.
 *
 * Code inside #if 0" is parts of original tcpsvd which are not implemented
 * for busyboxed version.
 *
 * Output of verbose mode matches original (modulo bugs and
 * unimplemented stuff). Unnatural splitting of IP and PORT
 * is retained (personally I prefer one-value "IP:PORT" notation -
 * it is a natural string representation of struct sockaddr_XX).
 */

#include "busybox.h"

unsigned verbose;

static void sig_term_handler(int sig)
{
	if (verbose)
		printf("udpsvd: info: sigterm received, exit\n");
	exit(0);
}

int udpsvd_main(int argc, char **argv);
int udpsvd_main(int argc, char **argv)
{
	const char *instructs;
	char *str_t, *user;
	unsigned opt;
//	unsigned lookuphost = 0;
//	unsigned paranoid = 0;
//	unsigned long timeout = 0;

	char *remote_hostname;
	char *local_hostname = local_hostname; /* gcc */
	char *remote_ip;
	char *local_ip = local_ip; /* gcc */
	uint16_t local_port, remote_port;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		USE_FEATURE_IPV6(struct sockaddr_in6 sin6;)
	} sock_adr;
	socklen_t sockadr_size;
	int sock;
	int wstat;
	unsigned pid;
	struct bb_uidgid_t ugid;

	enum {
		OPT_v = (1 << 0),
		OPT_u = (1 << 1),
		OPT_l = (1 << 2),
		OPT_h = (1 << 3),
		OPT_p = (1 << 4),
		OPT_i = (1 << 5),
		OPT_x = (1 << 6),
		OPT_t = (1 << 7),
	};

	opt_complementary = "ph:vv";
	opt = getopt32(argc, argv, "vu:l:hpi:x:t:",
			&user, &local_hostname, &instructs, &instructs, &str_t, &verbose);
	//if (opt & OPT_x) iscdb =1;
	//if (opt & OPT_t) timeout = xatou(str_t);
	if (!(opt & OPT_h))
		remote_hostname = (char *)"";
	if (opt & OPT_u) {
		if (!get_uidgid(&ugid, user, 1))
			bb_error_msg_and_die("unknown user/group: %s", user);
	}
	argv += optind;
	if (!argv[0][0] || LONE_CHAR(argv[0], '0'))
		argv[0] = (char*)"0.0.0.0";

	setlinebuf(stdout);

	signal(SIGTERM, sig_term_handler);
	signal(SIGPIPE, SIG_IGN);

	local_port = bb_lookup_port(argv[1], "udp", 0);
	sock = create_and_bind_dgram_or_die(argv[0], local_port);

	if (opt & OPT_u) { /* drop permissions */
		xsetgid(ugid.gid);
		xsetuid(ugid.uid);
	}
	bb_sanitize_stdio(); /* fd# 1,2 must be opened */
	close(0);

	if (verbose) {
		/* we do it only for ":port" cosmetics... oh well */
		len_and_sockaddr *lsa = xhost2sockaddr(argv[0], local_port);
		char *addr = xmalloc_sockaddr2dotted(&lsa->sa, lsa->len);
		printf("udpsvd: info: listening on %s", addr);
		free(addr);
		if (option_mask32 & OPT_u)
			printf(", uid %u, gid %u",
				(unsigned)ugid.uid, (unsigned)ugid.gid);
		puts(", starting");
	}

 again:
/*	io[0].fd = s;
	io[0].events = IOPAUSE_READ;
	io[0].revents = 0;
	taia_now(&now);
	taia_uint(&deadline, 3600);
	taia_add(&deadline, &now, &deadline);
	iopause(io, 1, &deadline, &now);
	if (!(io[0].revents | IOPAUSE_READ))
		goto again;
	io[0].revents = 0;
*/
	sockadr_size = sizeof(sock_adr);
	if (recvfrom(sock, NULL, 0, MSG_PEEK, &sock_adr.sa, &sockadr_size) == -1) {
		bb_perror_msg("recvfrom");
		goto again;
	}

	while ((pid = fork()) < 0) {
		bb_perror_msg("fork failed, sleeping");
		sleep(5);
	}
	if (pid > 0) { /* parent */
		while (wait_pid(&wstat, pid) == -1)
			bb_perror_msg("error waiting for child");
		if (verbose)
			printf("udpsvd: info: end %u\n", pid);
		goto again;
	}

	/* Child */

/*	if (recvfrom(sock, 0, 0, MSG_PEEK, (struct sockaddr *)&sock_adr, &sockadr_size) == -1)
		drop("unable to read from socket");
*/
	if (verbose) {
		local_ip = argv[0]; // TODO: recv_from_to!
		local_hostname = (char*)"localhost";
	}

	remote_ip = xmalloc_sockaddr2dotted_noport(&sock_adr.sa, sockadr_size);
	remote_port = get_nport(&sock_adr.sa);
	remote_port = ntohs(remote_port);
	if (verbose) {
		printf("udpsvd: info: pid %u from %s\n", pid, remote_ip);
	}
	if (opt & OPT_h) {
		remote_hostname = xmalloc_sockaddr2host(&sock_adr.sa, sizeof(sock_adr));
		if (!remote_hostname) {
			bb_error_msg("warning: cannot look up hostname for %s", remote_ip);
			remote_hostname = (char*)"";
		}
	}

#if 0
	if (instructs) {
		ac = ipsvd_check(iscdb, &inst, &match, (char*)instructs,
				remote_ip, remote_hostname.s, timeout);
		if (ac == -1) discard("unable to check inst", remote_ip);
		if (ac == IPSVD_ERR) discard("unable to read", (char*)instructs);
	} else
		ac = IPSVD_DEFAULT;
#endif

	if (verbose) {
#if 0
		out("udpsvd: info: ");
		switch(ac) {
		case IPSVD_DENY: out("deny "); break;
		case IPSVD_DEFAULT: case IPSVD_INSTRUCT: out("start "); break;
		case IPSVD_EXEC: out("exec "); break;
		}
#endif
		printf("udpsvd: info: %u %s:%s :%s:%s:%u\n",
				pid, local_hostname, local_ip,
				remote_hostname, remote_ip, remote_port);
#if 0
		if (instructs) {
			out(" ");
			if (iscdb) {
				out((char*)instructs); out("/");
			}
			outfix(match.s);
			if(inst.s && inst.len && (verbose > 1)) {
				out(": "); outinst(&inst);
			}
		}
#endif
	}

#if 0
	if (ac == IPSVD_DENY) {
		recv(s, 0, 0, 0);
		_exit(100);
	}
	if (ac == IPSVD_EXEC) {
		args[0] = "/bin/sh";
		args[1] = "-c";
		args[2] = inst.s;
		args[3] = NULL;
		run = args;
	} else run = prog;
#endif

	xmove_fd(sock, 0);
	dup2(0, 1);

	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	argv += 2;

	BB_EXECVP(argv[0], argv);
	bb_perror_msg_and_die("exec '%s'", argv[0]);
}
