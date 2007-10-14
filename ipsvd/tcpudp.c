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
 * Code inside "#ifdef SSLSVD" is for sslsvd and is currently unused.
 *
 * Output of verbose mode matches original (modulo bugs and
 * unimplemented stuff). Unnatural splitting of IP and PORT
 * is retained (personally I prefer one-value "IP:PORT" notation -
 * it is a natural string representation of struct sockaddr_XX).
 *
 * TCPORIGDST{IP,PORT} is busybox-specific addition
 *
 * udp server is hacked up by reusing TCP code. It has the following
 * limitation inherent in Unix DGRAM sockets implementation:
 * - local IP address is retrieved (using recvmsg voodoo) but
 *   child's socket is not bound to it (bind cannot be called on
 *   already bound socket). Thus it still can emit outgoing packets
 *   with wrong source IP...
 * - don't know how to retrieve ORIGDST for udp.
 */

#include <limits.h>
#include <linux/netfilter_ipv4.h> /* wants <limits.h> */

#include "libbb.h"
#include "ipsvd_perhost.h"

#ifdef SSLSVD
#include "matrixSsl.h"
#include "ssl_io.h"
#endif

struct globals {
	unsigned verbose;
	unsigned max_per_host;
	unsigned cur_per_host;
	unsigned cnum;
	unsigned cmax;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define verbose      (G.verbose     )
#define max_per_host (G.max_per_host)
#define cur_per_host (G.cur_per_host)
#define cnum         (G.cnum        )
#define cmax         (G.cmax        )
#define INIT_G() \
	do { \
		cmax = 30; \
	} while (0)


static void xsetenv_proto(const char *proto, const char *n, const char *v)
{
	putenv(xasprintf("%s%s=%s", proto, n, v));
}

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
	/* "only 1 client max" desn't need this */
	if (cmax > 1)
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

int tcpudpsvd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tcpudpsvd_main(int argc, char **argv)
{
	char *str_c, *str_C, *str_b, *str_t;
	char *user;
	struct hcc *hccp;
	const char *instructs;
	char *msg_per_host = NULL;
	unsigned len_per_host = len_per_host; /* gcc */
#ifndef SSLSVD
	struct bb_uidgid_t ugid;
#endif
	bool need_hostnames, need_remote_ip, tcp;
	uint16_t local_port;
	char *local_hostname = NULL;
	char *remote_hostname = (char*)""; /* "" used if no -h */
	char *local_addr = local_addr; /* gcc */
	char *remote_addr = remote_addr; /* gcc */
	char *remote_ip = remote_addr; /* gcc */
	len_and_sockaddr *lsa;
	len_and_sockaddr local, remote;
	socklen_t sa_len;
	int pid;
	int sock;
	int conn;
	unsigned backlog = 20;

	INIT_G();

	tcp = (applet_name[0] == 't');

	/* 3+ args, -i at most once, -p implies -h, -v is counter */
	opt_complementary = "-3:i--i:ph:vv";
#ifdef SSLSVD
	getopt32(argv, "+c:C:i:x:u:l:Eb:hpt:vU:/:Z:K:",
		&str_c, &str_C, &instructs, &instructs, &user, &local_hostname,
		&str_b, &str_t, &ssluser, &root, &cert, &key, &verbose
	);
#else
	getopt32(argv, "+c:C:i:x:u:l:Eb:hpt:v",
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
	if (option_mask32 & OPT_U) ssluser = optarg;
	if (option_mask32 & OPT_slash) root = optarg;
	if (option_mask32 & OPT_Z) cert = optarg;
	if (option_mask32 & OPT_K) key = optarg;
#endif
	argv += optind;
	if (!argv[0][0] || LONE_CHAR(argv[0], '0'))
		argv[0] = (char*)"0.0.0.0";

	/* Per-IP flood protection is not thought-out for UDP */
	if (!tcp)
		max_per_host = 0;

	/* stdout is used for logging, don't buffer */
	setlinebuf(stdout);
	bb_sanitize_stdio(); /* fd# 0,1,2 must be opened */

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
				bb_perror_msg_and_die("fatal: cannot get user/group: %s", ssluser);
			}
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

	local_port = bb_lookup_port(argv[1], tcp ? "tcp" : "udp", 0);
	lsa = xhost2sockaddr(argv[0], local_port);
	sock = xsocket(lsa->sa.sa_family, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	setsockopt_reuseaddr(sock);
	sa_len = lsa->len; /* I presume sockaddr len stays the same */
	xbind(sock, &lsa->sa, sa_len);
	if (tcp)
		xlisten(sock, backlog);
	else /* udp: needed for recv_from_to to work: */
		socket_want_pktinfo(sock);
	/* ndelay_off(sock); - it is the default I think? */

#ifndef SSLSVD
	if (option_mask32 & OPT_u) {
		/* drop permissions */
		xsetgid(ugid.gid);
		xsetuid(ugid.uid);
	}
#endif

	if (verbose) {
		char *addr = xmalloc_sockaddr2dotted(&lsa->sa);
		printf("%s: info: listening on %s", applet_name, addr);
		free(addr);
#ifndef SSLSVD
		if (option_mask32 & OPT_u)
			printf(", uid %u, gid %u",
				(unsigned)ugid.uid, (unsigned)ugid.gid);
#endif
		puts(", starting");
	}

	/* Main accept() loop */

 again:
	hccp = NULL;

	while (cnum >= cmax)
		sig_pause(); /* wait for any signal (expecting SIGCHLD) */

	/* Accept a connection to fd #0 */
 again1:
	close(0);
 again2:
	sig_unblock(SIGCHLD);
	if (tcp) {
		remote.len = sa_len;
		conn = accept(sock, &remote.sa, &remote.len);
	} else {
		/* In case recv_from_to won't be able to recover local addr.
		 * Also sets port - recv_from_to is unable to do it. */
		local = *lsa;
		conn = recv_from_to(sock, NULL, 0, MSG_PEEK, &remote.sa, &local.sa, sa_len);
	}
	sig_block(SIGCHLD);
	if (conn < 0) {
		if (errno != EINTR)
			bb_perror_msg(tcp ? "accept" : "recv");
		goto again2;
	}
	xmove_fd(tcp ? conn : sock, 0);

	if (max_per_host) {
		/* Drop connection immediately if cur_per_host > max_per_host
		 * (minimizing load under SYN flood) */
		remote_ip = xmalloc_sockaddr2dotted_noport(&remote.sa);
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

	if (!tcp) {
		/* Voodoo magic: making udp sockets each receive its own
		 * packets is not trivial, and I still not sure
		 * I do it 100% right.
		 * 1) we have to do it before fork()
		 * 2) order is important - is it right now? */

		/* Make plain write/send work for this socket by supplying default
		 * destination address. This also restricts incoming packets
		 * to ones coming from this remote IP. */
		xconnect(0, &remote.sa, sa_len);
	/* hole? at this point we have no wildcard udp socket...
	 * can this cause clients to get "port unreachable" icmp?
	 * Yup, time window is very small, but it exists (is it?) */
		/* Open new non-connected UDP socket for further clients */
		sock = xsocket(lsa->sa.sa_family, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
		setsockopt_reuseaddr(sock);
		xbind(sock, &lsa->sa, sa_len);
		socket_want_pktinfo(sock);

		/* Doesn't work:
		 * we cannot replace fd #0 - we will lose pending packet
		 * which is already buffered for us! And we cannot use fd #1
		 * instead - it will "intercept" all following packets, but child
		 * does not expect data coming *from fd #1*! */
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
	}

	pid = fork();
	if (pid == -1) {
		bb_perror_msg("fork");
		goto again;
	}


	if (pid != 0) {
		/* parent */
		cnum++;
		if (verbose)
			connection_status();
		if (hccp)
			hccp->pid = pid;
		goto again;
	}

	/* Child: prepare env, log, and exec prog */

	/* Closing tcp listening socket */
	if (tcp)
		close(sock);

	if (need_remote_ip)
		remote_addr = xmalloc_sockaddr2dotted(&remote.sa);

	if (need_hostnames) {
		if (option_mask32 & OPT_h) {
			remote_hostname = xmalloc_sockaddr2host_noport(&remote.sa);
			if (!remote_hostname) {
				bb_error_msg("warning: cannot look up hostname for %s", remote_addr);
				remote_hostname = (char*)"";
			}
		}
		/* Find out local IP peer connected to.
		 * Errors ignored (I'm not paranoid enough to imagine kernel
		 * which doesn't know local IP). */
		if (tcp) {
			local.len = sa_len;
			getsockname(0, &local.sa, &local.len);
		}
		local_addr = xmalloc_sockaddr2dotted(&local.sa);
		if (!local_hostname) {
			local_hostname = xmalloc_sockaddr2host_noport(&local.sa);
			if (!local_hostname)
				bb_error_msg_and_die("warning: cannot look up hostname for %s"+9, local_addr);
		}
	}

	if (verbose) {
		pid = getpid();
		printf("%s: info: pid %u from %s\n", applet_name, pid, remote_addr);
		if (max_per_host)
			printf("%s: info: concurrency %u %s %u/%u\n",
				applet_name, pid, remote_ip, cur_per_host, max_per_host);
		printf("%s: info: start %u %s:%s :%s:%s\n",
			applet_name, pid,
			local_hostname, local_addr,
			remote_hostname, remote_addr);
	}

	if (!(option_mask32 & OPT_E)) {
		/* setup ucspi env */
		const char *proto = tcp ? "TCP" : "UDP";

		/* Extract "original" destination addr:port
		 * from Linux firewall. Useful when you redirect
		 * an outbond connection to local handler, and it needs
		 * to know where it originally tried to connect */
		if (tcp && getsockopt(0, SOL_IP, SO_ORIGINAL_DST, &lsa->sa, &lsa->len) == 0) {
			char *addr = xmalloc_sockaddr2dotted(&lsa->sa);
			xsetenv("TCPORIGDSTADDR", addr);
			free(addr);
		}
		xsetenv("PROTO", proto);
		xsetenv_proto(proto, "LOCALADDR", local_addr);
		xsetenv_proto(proto, "LOCALHOST", local_hostname);
		xsetenv_proto(proto, "REMOTEADDR", remote_addr);
		if (option_mask32 & OPT_h) {
			xsetenv_proto(proto, "REMOTEHOST", remote_hostname);
		}
		xsetenv_proto(proto, "REMOTEINFO", "");
		/* additional */
		if (cur_per_host > 0) /* can not be true for udp */
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
