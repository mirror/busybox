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

#include "udp_io.c"

unsigned verbose;

static void sig_term_handler(int sig)
{
	if (verbose)
		printf("%s: info: sigterm received, exit\n", applet_name);
	exit(0);
}

int udpsvd_main(int argc, char **argv);
int udpsvd_main(int argc, char **argv)
{
	const char *instructs;
	char *str_t, *user;
	unsigned opt;

	char *remote_hostname;
	char *local_hostname = NULL;
	char *remote_ip;
	char *local_ip = local_ip; /* gcc */
	uint16_t local_port, remote_port;
	len_and_sockaddr remote;
	len_and_sockaddr *localp;
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

	opt_complementary = "-3:ph:vv";
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

	/* stdout is used for logging, don't buffer */
	setlinebuf(stdout);
	bb_sanitize_stdio(); /* fd# 1,2 must be opened */

	signal(SIGTERM, sig_term_handler);
	signal(SIGPIPE, SIG_IGN);

	local_port = bb_lookup_port(argv[1], "udp", 0);
	localp = xhost2sockaddr(argv[0], local_port);
	sock = xsocket(localp->sa.sa_family, SOCK_DGRAM, 0);
	xmove_fd(sock, 0); /* fd# 0 is the open UDP socket */
	xbind(0, &localp->sa, localp->len);
	socket_want_pktinfo(0);

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
		while (wait_pid(&wstat, pid) == -1)
			bb_perror_msg("error waiting for child");
		if (verbose)
			printf("%s: info: end %u\n", applet_name, pid);
		goto again;
	}

	/* Child */

#if 0
	/* I'd like to make it so that local addr is fixed to localp->sa,
	 * but how? The below trick doesn't work... */
	close(0);
	set_nport(localp, htons(local_port));
	xmove_fd(xsocket(localp->sa.sa_family, SOCK_DGRAM, 0), 0);
	xbind(0, &localp->sa, localp->len);
#endif

	if (verbose) {
		local_ip = xmalloc_sockaddr2dotted_noport(&localp->sa, localp->len);
		if (!local_hostname) {
			local_hostname = xmalloc_sockaddr2host_noport(&localp->sa, localp->len);
			if (!local_hostname)
				bb_error_msg_and_die("cannot look up local hostname for %s", local_ip);
		}
	}

	remote_ip = xmalloc_sockaddr2dotted_noport(&remote.sa, localp->len);
	remote_port = get_nport(&remote.sa);
	remote_port = ntohs(remote_port);
	if (verbose)
		printf("%s: info: pid %u from %s\n", applet_name, pid, remote_ip);

	if (opt & OPT_h) {
		remote_hostname = xmalloc_sockaddr2host(&remote.sa, localp->len);
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
		out("%s: info: ", applet_name);
		switch(ac) {
		case IPSVD_DENY: out("deny "); break;
		case IPSVD_DEFAULT: case IPSVD_INSTRUCT: out("start "); break;
		case IPSVD_EXEC: out("exec "); break;
		}
#endif
		printf("%s: info: %u %s:%s :%s:%s:%u\n",
				applet_name, pid, local_hostname, local_ip,
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
		recv(0, 0, 0, 0);
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
	/* Make plain write(1) work for the child by supplying default
	 * destination address */
	xconnect(0, &remote.sa, localp->len);
	dup2(0, 1);

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
