/* Based on ipsvd utilities written by Gerrit Pape <pape@smarden.org>
 * which are released into public domain by the author.
 * Homepage: http://smarden.sunsite.dk/ipsvd/
 *
 * Copyright (C) 2007 Denis Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/* TCP and UDP server are using a lot of same string constants
 * We reuse them by keeping both in one source file */

#include "busybox.h"

static unsigned verbose;

static void sig_term_handler(int sig)
{
	if (verbose)
		printf("%s: info: sigterm received, exit\n", applet_name);
	exit(0);
}

/* Little bloated, but tries to give accurate info how child exited.
 * Makes easier to spot segfaulting children etc... */
static void print_waitstat(unsigned pid, int wstat)
{
	unsigned e = 0;
	const char *cause = "?exit";

	if (WIFEXITED(wstat)) {
		cause++;
		e = WEXITSTATUS(wstat);
	} else if (WIFSIGNALED(wstat)) {
		cause = "signal";
		e = WTERMSIG(wstat);
	}
	printf("%s: info: end %d %s %d\n", applet_name, pid, cause, e);
}


#if ENABLE_UDPSVD
/* Based on ipsvd ipsvd-0.12.1. This udpsvd accepts all options
 * which are supported by one from ipsvd-0.12.1, but not all are
 * functional. See help text at the end of this file for details.
 *
 * Output of verbose mode matches original (modulo bugs and
 * unimplemented stuff). Unnatural splitting of IP and PORT
 * is retained (personally I prefer one-value "IP:PORT" notation -
 * it is a natural string representation of struct sockaddr_XX).
 */

#include "udp_io.c"

int udpsvd_main(int argc, char **argv);
int udpsvd_main(int argc, char **argv)
{
	const char *instructs;
	char *str_t, *user;
	unsigned opt;

	char *remote_hostname = (char*)""; /* used if no -h */
	char *local_hostname = NULL;
	char *remote_ip;
	char *local_ip;// = local_ip; /* gcc */
	uint16_t local_port, remote_port;
	len_and_sockaddr remote;
	len_and_sockaddr *localp;
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

	opt_complementary = "-3:ph:vv";
	opt = getopt32(argc, argv, "vu:l:hpi:x:t:",
			&user, &local_hostname, &instructs, &instructs, &str_t, &verbose);
	if (opt & OPT_u) {
		if (!get_uidgid(&ugid, user, 1))
			bb_error_msg_and_die("unknown user/group: %s", user);
	}
	argv += optind;
	if (!argv[0][0] || LONE_CHAR(argv[0], '0'))
		argv[0] = (char*)"0.0.0.0";

	/* stdout is used for logging, don't buffer */
	setlinebuf(stdout);
	bb_sanitize_stdio(); /* fd# 1,2 must be opened */

	signal(SIGTERM, sig_term_handler);
	signal(SIGPIPE, SIG_IGN);

	local_port = bb_lookup_port(argv[1], "udp", 0);
	localp = xhost2sockaddr(argv[0], local_port);
	/* fd #0 is the open UDP socket */
	xmove_fd(xsocket(localp->sa.sa_family, SOCK_DGRAM, 0), 0);
	setsockopt_reuseaddr(0); /* crucial */
	xbind(0, &localp->sa, localp->len);
	socket_want_pktinfo(0); /* needed for recv_from_to to work */

	if (opt & OPT_u) { /* drop permissions */
		xsetgid(ugid.gid);
		xsetuid(ugid.uid);
	}

	if (verbose) {
		/* we do it only for ":port" cosmetics... oh well */
		char *addr = xmalloc_sockaddr2dotted(&localp->sa, localp->len);

		printf("%s: info: listening on %s", applet_name, addr);
		free(addr);
		if (option_mask32 & OPT_u)
			printf(", uid %u, gid %u",
				(unsigned)ugid.uid, (unsigned)ugid.gid);
		puts(", starting");
	}

 again:
	/* if (recvfrom(0, NULL, 0, MSG_PEEK, &remote.sa, &localp->len) < 0) { */
	if (recv_from_to(0, NULL, 0, MSG_PEEK, &remote.sa, &localp->sa, localp->len) < 0) {
		bb_perror_msg("recvfrom");
		goto again;
	}

	while ((pid = fork()) < 0) {
		bb_perror_msg("fork failed, sleeping");
		sleep(5);
	}
	if (pid > 0) { /* parent */
		while (wait_pid(&wstat, pid) < 0)
			bb_perror_msg("error waiting for child");
		if (verbose)
			print_waitstat(pid, wstat);
		goto again;
	}

	/* Child */

	if (verbose) {
		remote_ip = xmalloc_sockaddr2dotted_noport(&remote.sa, localp->len);
		local_ip = xmalloc_sockaddr2dotted_noport(&localp->sa, localp->len);

		pid = getpid();
		printf("%s: info: pid %u from %s\n", applet_name, pid, remote_ip);

		if (!local_hostname) {
			local_hostname = xmalloc_sockaddr2host_noport(&localp->sa, localp->len);
			if (!local_hostname)
				bb_error_msg_and_die("cannot look up local hostname for %s", local_ip);
		}
		if (opt & OPT_h) {
			remote_hostname = xmalloc_sockaddr2host(&remote.sa, localp->len);
			if (!remote_hostname) {
				bb_error_msg("warning: cannot look up hostname for %s", remote_ip);
				remote_hostname = (char*)"";
			}
		}

		remote_port = get_nport(&remote.sa);
		remote_port = ntohs(remote_port);
		printf("%s: info: %u %s:%s :%s:%s:%u\n",
				applet_name, pid, local_hostname, local_ip,
				remote_hostname, remote_ip, remote_port);
	}

	/* Doesn't work:
	 * we cannot replace fd #0 - we will lose pending packet
	 * which is already buffered for us! And we cannot use fd #1
	 * instead - it will "intercept" all following packets, but child
	 * do not expect data coming *from fd #1*! */
#if 0
	/* Make it so that local addr is fixed to localp->sa
	 * and we don't accidentally accept packets to other local IPs. */
	/* NB: we possibly bind to the _very_ same_ address & port as the one
	 * already bound in parent! This seems to work in Linux.
	 * (otherwise we can move socket to fd #0 only if bind succeeds) */
	close(0);
	set_nport(localp, htons(local_port));
	xmove_fd(xsocket(localp->sa.sa_family, SOCK_DGRAM, 0), 0);
	setsockopt_reuseaddr(0); /* crucial */
	xbind(0, &localp->sa, localp->len);
#endif

	/* Make plain write to fd #1 work for the child by supplying default
	 * destination address. This also restricts incoming packets
	 * to ones coming from this remote IP. */
	xconnect(0, &remote.sa, localp->len);
	dup2(0 ,1);

	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	argv += 2;
	BB_EXECVP(argv[0], argv);
	bb_perror_msg_and_die("exec '%s'", argv[0]);
}


/*
udpsvd [-hpvv] [-u user] [-l name] [-i dir|-x cdb] [-t sec] host port prog

udpsvd creates an UDP/IP socket, binds it to the address host:port,
and listens on the socket for incoming datagrams.

If a datagram is available on the socket, udpsvd conditionally starts
a program, with standard input reading from the socket, and standard
output redirected to standard error, to handle this, and possibly
more datagrams. udpsvd does not start the program if another program
that it has started before still is running. If the program exits,
udpsvd again listens to the socket until a new datagram is available.
If there are still datagrams available on the socket, the program
is restarted immediately.

udpsvd optionally checks for special intructions depending on
the IP address or hostname of the client sending the datagram which
not yet was handled by a running program, see ipsvd-instruct(5)
for details.

Attention:
UDP is a connectionless protocol. Most programs that handle user datagrams,
such as talkd(8), keep running after receiving a datagram, and process
subsequent datagrams sent to the socket until a timeout is reached.
udpsvd only checks special instructions for a datagram that causes a startup
of the program; not if a program handling datagrams already is running.
It doesn't make much sense to restrict access through special instructions
when using such a program.

On the other hand, it makes perfectly sense with programs like tftpd(8),
that fork to establish a separate connection to the client when receiving
the datagram. In general it's adequate to set up special instructions for
programs that support being run by tcpwrapper.
Options

host
    host either is a hostname, or a dotted-decimal IP address, or 0.
    If host is 0, udpsvd accepts datagrams to any local IP address.
port
    udpsvd accepts datagrams to host:port. port may be a name from
    /etc/services or a number.
prog
    prog consists of one or more arguments. udpsvd normally runs prog
    to handle a datagram, and possibly more, that is sent to the socket,
    if there is no program that was started before by udpsvd still running
    and handling datagrams.
-i dir
    read instructions for handling new connections from the instructions
    directory dir. See ipsvd-instruct(5) for details.
-x cdb
    read instructions for handling new connections from the constant
    database cdb. The constant database normally is created from
    an instructions directory by running ipsvd-cdb(8).
-t sec
    timeout. This option only takes effect if the -i option is given.
    While checking the instructions directory, check the time of last
    access of the file that matches the clients address or hostname if any,
    discard and remove the file if it wasn't accessed within the last
    sec seconds; udpsvd does not discard or remove a file if the user's
    write permission is not set, for those files the timeout is disabled.
    Default is 0, which means that the timeout is disabled.
-l name
    local hostname. Do not look up the local hostname in DNS, but use name
    as hostname. By default udpsvd looks up the local hostname once at startup.
-u user[:group]
    drop permissions. Switch user ID to user's UID, and group ID to user's
    primary GID after creating and binding to the socket. If user
    is followed by a colon and a group name, the group ID is switched
    to the GID of group instead. All supplementary groups are removed.
-h
    Look up the client's hostname in DNS.
-p
    paranoid. After looking up the client's hostname in DNS, look up
    the IP addresses in DNS for that hostname, and forget the hostname
    if none of the addresses match the client's IP address. You should
    set this option if you use hostname based instructions. The -p option
    implies the -h option.
-v
    verbose. Print verbose messages to standard output.
-vv
    more verbose. Print more verbose messages to standard output.
*/
#endif


#if ENABLE_TCPSVD
/* Based on ipsvd ipsvd-0.12.1. This tcpsvd accepts all options
 * which are supported by one from ipsvd-0.12.1, but not all are
 * functional. See help text at the end of this file for details.
 *
 * Code inside "#ifdef SSLSVD" is for sslsvd and is currently unused.
 *
 * Output of verbose mode matches original (modulo bugs and
 * unimplemented stuff). Unnatural splitting of IP and PORT
 * is retained (personally I prefer one-value "IP:PORT" notation -
 * it is a natural string representation of struct sockaddr_XX).
 *
 * TCPORIGDST{IP,PORT} is busybox-specific addition
 */

#include <limits.h>
#include <linux/netfilter_ipv4.h> /* wants <limits.h> */
#include "ipsvd_perhost.h"

#ifdef SSLSVD
#include "matrixSsl.h"
#include "ssl_io.h"
#endif

static unsigned max_per_host; /* originally in ipsvd_check.c */
static unsigned cur_per_host;
static unsigned cnum;
static unsigned cmax = 30;

/* Must match getopt32 in main! */
enum {
	OPT_c = (1 << 0),
	OPT_C = (1 << 1),
	OPT_i = (1 << 2),
	OPT_x = (1 << 3),
	OPT_u = (1 << 4),
	OPT_l = (1 << 5),
	OPT_E = (1 << 6),
	OPT_b = (1 << 7),
	OPT_h = (1 << 8),
	OPT_p = (1 << 9),
	OPT_t = (1 << 10),
	OPT_v = (1 << 11),
	OPT_V = (1 << 12),
	OPT_U = (1 << 13), /* from here: sslsvd only */
	OPT_slash = (1 << 14),
	OPT_Z = (1 << 15),
	OPT_K = (1 << 16),
};

static void connection_status(void)
{
	printf("%s: info: status %u/%u\n", applet_name, cnum, cmax);
}

static void sig_child_handler(int sig)
{
	int wstat;
	int pid;

	while ((pid = wait_nohang(&wstat)) > 0) {
		if (max_per_host)
			ipsvd_perhost_remove(pid);
		if (cnum)
			cnum--;
		if (verbose)
			print_waitstat(pid, wstat);
	}
	if (verbose)
		connection_status();
}

int tcpsvd_main(int argc, char **argv);
int tcpsvd_main(int argc, char **argv)
{
	char *str_c, *str_C, *str_b, *str_t;
	char *user;
	struct hcc *hccp;
	const char *instructs;
	char *msg_per_host = NULL;
	unsigned len_per_host = len_per_host; /* gcc */
	int need_hostnames, need_remote_ip;
	int pid;
	int sock;
	int conn;
	unsigned backlog = 20;
	len_and_sockaddr *lsa;
	uint16_t local_port;
	uint16_t remote_port = remote_port; /* gcc */
	char *local_hostname = NULL;
	char *remote_hostname = (char*)""; /* "" used if no -h */
	char *local_ip = local_ip; /* gcc */
	char *remote_ip = remote_ip; /* gcc */
#ifndef SSLSVD
	struct bb_uidgid_t ugid;
#endif

	/* 3+ args, -i at most once, -p implies -h, -v is counter */
	opt_complementary = "-3:?:i--i:ph:vv";
#ifdef SSLSVD
	getopt32(argc, argv, "c:C:i:x:u:l:Eb:hpt:vU:/:Z:K:",
		&str_c, &str_C, &instructs, &instructs, &user, &local_hostname,
		&str_b, &str_t, &ssluser, &root, &cert, &key, &verbose
	);
#else
	getopt32(argc, argv, "c:C:i:x:u:l:Eb:hpt:v",
		&str_c, &str_C, &instructs, &instructs, &user, &local_hostname,
		&str_b, &str_t, &verbose
	);
#endif
	if (option_mask32 & OPT_c)
		cmax = xatou_range(str_c, 1, INT_MAX);
	if (option_mask32 & OPT_C) { /* -C n[:message] */
		max_per_host = bb_strtou(str_C, &str_C, 10);
		if (str_C[0]) {
			if (str_C[0] != ':')
				bb_show_usage();
			msg_per_host = str_C + 1;
			len_per_host = strlen(msg_per_host);
		}
	}
	if (max_per_host > cmax)
		max_per_host = cmax;
	if (option_mask32 & OPT_u) {
		if (!get_uidgid(&ugid, user, 1))
			bb_error_msg_and_die("unknown user/group: %s", user);
	}
	if (option_mask32 & OPT_b)
		backlog = xatou(str_b);
#ifdef SSLSVD
	if (option_mask32 & OPT_U) ssluser = (char*)optarg; break;
	if (option_mask32 & OPT_slash) root = (char*)optarg; break;
	if (option_mask32 & OPT_Z) cert = (char*)optarg; break;
	if (option_mask32 & OPT_K) key = (char*)optarg; break;
#endif
	argv += optind;
	if (!argv[0][0] || LONE_CHAR(argv[0], '0'))
		argv[0] = (char*)"0.0.0.0";

	/* stdout is used for logging, don't buffer */
	setlinebuf(stdout);
	bb_sanitize_stdio(); /* fd# 1,2 must be opened */

	need_hostnames = verbose || !(option_mask32 & OPT_E);
	need_remote_ip = max_per_host || need_hostnames;

#ifdef SSLSVD
	sslser = user;
	client = 0;
	if ((getuid() == 0) && !(option_mask32 & OPT_u)) {
		xfunc_exitcode = 100;
		bb_error_msg_and_die("fatal: -U ssluser must be set when running as root");
	}
	if (option_mask32 & OPT_u)
		if (!uidgid_get(&sslugid, ssluser, 1)) {
			if (errno) {
				xfunc_exitcode = 100;
				bb_perror_msg_and_die("fatal: cannot get user/group: %s", ssluser);
			}
			xfunc_exitcode = 111;
			bb_error_msg_and_die("fatal: unknown user/group '%s'", ssluser);
		}
	if (!cert) cert = "./cert.pem";
	if (!key) key = cert;
	if (matrixSslOpen() < 0)
		fatal("cannot initialize ssl");
	if (matrixSslReadKeys(&keys, cert, key, 0, ca) < 0) {
		if (client)
			fatal("cannot read cert, key, or ca file");
		fatal("cannot read cert or key file");
	}
	if (matrixSslNewSession(&ssl, keys, 0, SSL_FLAGS_SERVER) < 0)
		fatal("cannot create ssl session");
#endif

	sig_block(SIGCHLD);
	signal(SIGCHLD, sig_child_handler);
	signal(SIGTERM, sig_term_handler);
	signal(SIGPIPE, SIG_IGN);

	if (max_per_host)
		ipsvd_perhost_init(cmax);

	local_port = bb_lookup_port(argv[1], "tcp", 0);
	lsa = xhost2sockaddr(argv[0], local_port);
	sock = xsocket(lsa->sa.sa_family, SOCK_STREAM, 0);
	setsockopt_reuseaddr(sock); /* desirable */
	xbind(sock, &lsa->sa, lsa->len);
	xlisten(sock, backlog);
	/* ndelay_off(sock); - it is the default I think? */

#ifndef SSLSVD
	if (option_mask32 & OPT_u) {
		/* drop permissions */
		xsetgid(ugid.gid);
		xsetuid(ugid.uid);
	}
#endif

	if (verbose) {
		/* we do it only for ":port" cosmetics... oh well */
		char *addr = xmalloc_sockaddr2dotted(&lsa->sa, lsa->len);

		printf("%s: info: listening on %s", applet_name, addr);
		free(addr);
#ifndef SSLSVD
		if (option_mask32 & OPT_u)
			printf(", uid %u, gid %u",
				(unsigned)ugid.uid, (unsigned)ugid.gid);
#endif
		puts(", starting");
	}

	/* The rest is a main accept() loop */

 again:
	hccp = NULL;

	while (cnum >= cmax)
		sig_pause(); /* wait for any signal (expecting SIGCHLD) */

	/* Accept a connection to fd #0 */
 again1:
	close(0);
 again2:
	sig_unblock(SIGCHLD);
	conn = accept(sock, &lsa->sa, &lsa->len);
	sig_block(SIGCHLD);
	if (conn < 0) {
		if (errno != EINTR)
			bb_perror_msg("accept");
		goto again2;
	}
	xmove_fd(conn, 0);

	if (max_per_host) {
		/* Drop connection immediately if cur_per_host > max_per_host
		 * (minimizing load under SYN flood) */
		remote_ip = xmalloc_sockaddr2dotted_noport(&lsa->sa, lsa->len);
		cur_per_host = ipsvd_perhost_add(remote_ip, max_per_host, &hccp);
		if (cur_per_host > max_per_host) {
			/* ipsvd_perhost_add detected that max is exceeded
			 * (and did not store ip in connection table) */
			free(remote_ip);
			if (msg_per_host) {
				/* don't block or test for errors */
				ndelay_on(0);
				write(0, msg_per_host, len_per_host);
			}
			goto again1;
		}
	}

	cnum++;
	if (verbose)
		connection_status();

	pid = fork();
	if (pid == -1) {
		bb_perror_msg("fork");
		goto again;
	}
	if (pid != 0) {
		/* parent */
		if (hccp)
			hccp->pid = pid;
		goto again;
	}

	/* Child: prepare env, log, and exec prog */

	close(sock); /* listening socket */
	/* Find out local IP peer connected to.
	 * Errors ignored (I'm not paranoid enough to imagine kernel
	 * which doesn't know local IP). */
	getsockname(0, &lsa->sa, &lsa->len);

	if (need_remote_ip) {
		if (!max_per_host)
			remote_ip = xmalloc_sockaddr2dotted_noport(&lsa->sa, lsa->len);
		/* else it is already done */
		remote_port = get_nport(&lsa->sa);
		remote_port = ntohs(remote_port);
	}

	if (need_hostnames) {
		if (option_mask32 & OPT_h) {
			remote_hostname = xmalloc_sockaddr2host(&lsa->sa, lsa->len);
			if (!remote_hostname) {
				bb_error_msg("warning: cannot look up hostname for %s", remote_ip);
				remote_hostname = (char*)"";
			}
		}
		local_ip = xmalloc_sockaddr2dotted_noport(&lsa->sa, lsa->len);
		local_port = get_nport(&lsa->sa);
		local_port = ntohs(local_port);
		if (!local_hostname) {
			local_hostname = xmalloc_sockaddr2host_noport(&lsa->sa, lsa->len);
			if (!local_hostname)
				bb_error_msg_and_die("cannot look up local hostname for %s", local_ip);
		}
	}

	if (verbose) {
		pid = getpid();
		printf("%s: info: pid %u from %s\n", applet_name, pid, remote_ip);
		if (max_per_host)
			printf("%s: info: concurrency %u %s %u/%u\n",
				applet_name, pid, remote_ip, cur_per_host, max_per_host);
		printf("%s: info: start %u %s:%s :%s:%s:%u\n",
			applet_name, pid,
			local_hostname, local_ip,
			remote_hostname, remote_ip, (unsigned)remote_port);
	}

	if (!(option_mask32 & OPT_E)) {
		/* setup ucspi env */

		/* Extract "original" destination addr:port
		 * from Linux firewall. Useful when you redirect
		 * an outbond connection to local handler, and it needs
		 * to know where it originally tried to connect */
		if (getsockopt(0, SOL_IP, SO_ORIGINAL_DST, &lsa->sa, &lsa->len) == 0) {
			char *ip = xmalloc_sockaddr2dotted_noport(&lsa->sa, lsa->len);
			unsigned port = get_nport(&lsa->sa);
			port = ntohs(port);
			xsetenv("TCPORIGDSTIP", ip);
			xsetenv("TCPORIGDSTPORT", utoa(port));
			free(ip);
		}
		xsetenv("PROTO", "TCP");
		xsetenv("TCPLOCALIP", local_ip);
		xsetenv("TCPLOCALPORT", utoa(local_port));
		xsetenv("TCPLOCALHOST", local_hostname);
		xsetenv("TCPREMOTEIP", remote_ip);
		xsetenv("TCPREMOTEPORT", utoa(remote_port));
		if (option_mask32 & OPT_h) {
			xsetenv("TCPREMOTEHOST", remote_hostname);
		}
		xsetenv("TCPREMOTEINFO", "");
		/* additional */
		if (cur_per_host > 0)
			xsetenv("TCPCONCURRENCY", utoa(cur_per_host));
	}

	dup2(0, 1);

	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	sig_unblock(SIGCHLD);

	argv += 2;
#ifdef SSLSVD
	strcpy(id, utoa(pid);
	ssl_io(0, argv);
#else
	BB_EXECVP(argv[0], argv);
#endif
	bb_perror_msg_and_die("exec '%s'", argv[0]);
}

/*
tcpsvd [-hpEvv] [-c n] [-C n:msg] [-b n] [-u user] [-l name]
	[-i dir|-x cdb] [ -t sec] host port prog

tcpsvd creates a TCP/IP socket, binds it to the address host:port,
and listens on the socket for incoming connections.

On each incoming connection, tcpsvd conditionally runs a program,
with standard input reading from the socket, and standard output
writing to the socket, to handle this connection. tcpsvd keeps
listening on the socket for new connections, and can handle
multiple connections simultaneously.

tcpsvd optionally checks for special instructions depending
on the IP address or hostname of the client that initiated
the connection, see ipsvd-instruct(5).

host
    host either is a hostname, or a dotted-decimal IP address,
    or 0. If host is 0, tcpsvd accepts connections to any local
    IP address.
    * busybox accepts IPv6 addresses and host:port pairs too
      In this case second parameter is ignored
port
    tcpsvd accepts connections to host:port. port may be a name
    from /etc/services or a number.
prog
    prog consists of one or more arguments. For each connection,
    tcpsvd normally runs prog, with file descriptor 0 reading from
    the network, and file descriptor 1 writing to the network.
    By default it also sets up TCP-related environment variables,
    see tcp-environ(5)
-i dir
    read instructions for handling new connections from the instructions
    directory dir. See ipsvd-instruct(5) for details.
    * ignored by busyboxed version
-x cdb
    read instructions for handling new connections from the constant database
    cdb. The constant database normally is created from an instructions
    directory by running ipsvd-cdb(8).
    * ignored by busyboxed version
-t sec
    timeout. This option only takes effect if the -i option is given.
    While checking the instructions directory, check the time of last access
    of the file that matches the clients address or hostname if any, discard
    and remove the file if it wasn't accessed within the last sec seconds;
    tcpsvd does not discard or remove a file if the user's write permission
    is not set, for those files the timeout is disabled. Default is 0,
    which means that the timeout is disabled.
    * ignored by busyboxed version
-l name
    local hostname. Do not look up the local hostname in DNS, but use name
    as hostname. This option must be set if tcpsvd listens on port 53
    to avoid loops.
-u user[:group]
    drop permissions. Switch user ID to user's UID, and group ID to user's
    primary GID after creating and binding to the socket. If user is followed
    by a colon and a group name, the group ID is switched to the GID of group
    instead. All supplementary groups are removed.
-c n
    concurrency. Handle up to n connections simultaneously. Default is 30.
    If there are n connections active, tcpsvd defers acceptance of a new
    connection until an active connection is closed.
-C n[:msg]
    per host concurrency. Allow only up to n connections from the same IP
    address simultaneously. If there are n active connections from one IP
    address, new incoming connections from this IP address are closed
    immediately. If n is followed by :msg, the message msg is written
    to the client if possible, before closing the connection. By default
    msg is empty. See ipsvd-instruct(5) for supported escape sequences in msg.

    For each accepted connection, the current per host concurrency is
    available through the environment variable TCPCONCURRENCY. n and msg
    can be overwritten by ipsvd(7) instructions, see ipsvd-instruct(5).
    By default tcpsvd doesn't keep track of connections.
-h
    Look up the client's hostname in DNS.
-p
    paranoid. After looking up the client's hostname in DNS, look up the IP
    addresses in DNS for that hostname, and forget about the hostname
    if none of the addresses match the client's IP address. You should
    set this option if you use hostname based instructions. The -p option
    implies the -h option.
    * ignored by busyboxed version
-b n
    backlog. Allow a backlog of approximately n TCP SYNs. On some systems n
    is silently limited. Default is 20.
-E
    no special environment. Do not set up TCP-related environment variables.
-v
    verbose. Print verbose messsages to standard output.
-vv
    more verbose. Print more verbose messages to standard output.
    * no difference between -v and -vv in busyboxed version
*/
#endif
