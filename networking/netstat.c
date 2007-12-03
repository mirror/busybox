/* vi: set sw=4 ts=4: */
/*
 * Mini netstat implementation(s) for busybox
 * based in part on the netstat implementation from net-tools.
 *
 * Copyright (C) 2002 by Bart Visscher <magick@linux-fan.com>
 *
 * 2002-04-20
 * IPV6 support added by Bart Visscher <magick@linux-fan.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "inet_common.h"

enum {
	OPT_extended = 0x4,
	OPT_showroute = 0x100,
	OPT_widedisplay = 0x200 * ENABLE_FEATURE_NETSTAT_WIDE,
};
# define NETSTAT_OPTS "laentuwxr"USE_FEATURE_NETSTAT_WIDE("W")

#define NETSTAT_CONNECTED 0x01
#define NETSTAT_LISTENING 0x02
#define NETSTAT_NUMERIC   0x04
/* Must match getopt32 option string */
#define NETSTAT_TCP       0x10
#define NETSTAT_UDP       0x20
#define NETSTAT_RAW       0x40
#define NETSTAT_UNIX      0x80
#define NETSTAT_ALLPROTO  (NETSTAT_TCP|NETSTAT_UDP|NETSTAT_RAW|NETSTAT_UNIX)

static smallint flags = NETSTAT_CONNECTED | NETSTAT_ALLPROTO;

enum {
	TCP_ESTABLISHED = 1,
	TCP_SYN_SENT,
	TCP_SYN_RECV,
	TCP_FIN_WAIT1,
	TCP_FIN_WAIT2,
	TCP_TIME_WAIT,
	TCP_CLOSE,
	TCP_CLOSE_WAIT,
	TCP_LAST_ACK,
	TCP_LISTEN,
	TCP_CLOSING /* now a valid state */
};

static const char *const tcp_state[] = {
	"",
	"ESTABLISHED",
	"SYN_SENT",
	"SYN_RECV",
	"FIN_WAIT1",
	"FIN_WAIT2",
	"TIME_WAIT",
	"CLOSE",
	"CLOSE_WAIT",
	"LAST_ACK",
	"LISTEN",
	"CLOSING"
};

typedef enum {
	SS_FREE = 0,     /* not allocated                */
	SS_UNCONNECTED,  /* unconnected to any socket    */
	SS_CONNECTING,   /* in process of connecting     */
	SS_CONNECTED,    /* connected to socket          */
	SS_DISCONNECTING /* in process of disconnecting  */
} socket_state;

#define SO_ACCEPTCON (1<<16)	/* performed a listen           */
#define SO_WAITDATA  (1<<17)	/* wait data to read            */
#define SO_NOSPACE   (1<<18)	/* no space to write            */

/* Standard printout size */
#define PRINT_IP_MAX_SIZE           23
#define PRINT_NET_CONN              "%s   %6ld %6ld %-23s %-23s %-12s\n"
#define PRINT_NET_CONN_HEADER       "\nProto Recv-Q Send-Q %-23s %-23s State\n"

/* When there are IPv6 connections the IPv6 addresses will be
 * truncated to none-recognition. The '-W' option makes the
 * address columns wide enough to accomodate for longest possible
 * IPv6 addresses, i.e. addresses of the form
 * xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:ddd.ddd.ddd.ddd
 */
#define PRINT_IP_MAX_SIZE_WIDE      51  /* INET6_ADDRSTRLEN + 5 for the port number */
#define PRINT_NET_CONN_WIDE         "%s   %6ld %6ld %-51s %-51s %-12s\n"
#define PRINT_NET_CONN_HEADER_WIDE  "\nProto Recv-Q Send-Q %-51s %-51s State\n"

static const char *net_conn_line = PRINT_NET_CONN;


#if ENABLE_FEATURE_IPV6
static void build_ipv6_addr(char* local_addr, struct sockaddr_in6* localaddr)
{
	char addr6[INET6_ADDRSTRLEN];
	struct in6_addr in6;

	sscanf(local_addr, "%08X%08X%08X%08X",
		   &in6.s6_addr32[0], &in6.s6_addr32[1],
		   &in6.s6_addr32[2], &in6.s6_addr32[3]);
	inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
	inet_pton(AF_INET6, addr6, (struct sockaddr *) &localaddr->sin6_addr);

	localaddr->sin6_family = AF_INET6;
}
#endif

#if ENABLE_FEATURE_IPV6
static void build_ipv4_addr(char* local_addr, struct sockaddr_in6* localaddr)
#else
static void build_ipv4_addr(char* local_addr, struct sockaddr_in* localaddr)
#endif
{
	sscanf(local_addr, "%X",
		   &((struct sockaddr_in *) localaddr)->sin_addr.s_addr);
	((struct sockaddr *) localaddr)->sa_family = AF_INET;
}

static const char *get_sname(int port, const char *proto, int numeric)
{
	if (!port)
		return "*";
	if (!numeric) {
		struct servent *se = getservbyport(port, proto);
		if (se)
			return se->s_name;
	}
	/* hummm, we may return static buffer here!! */
	return itoa(ntohs(port));
}

static char *ip_port_str(struct sockaddr *addr, int port, const char *proto, int numeric)
{
	enum { salen = USE_FEATURE_IPV6(sizeof(struct sockaddr_in6)) SKIP_FEATURE_IPV6(sizeof(struct sockaddr_in)) };
	char *host, *host_port;

	/* Code which used "*" for INADDR_ANY is removed: it's ambiguous in IPv6,
	 * while "0.0.0.0" is not. */

	host = numeric ? xmalloc_sockaddr2dotted_noport(addr)
	               : xmalloc_sockaddr2host_noport(addr);

	host_port = xasprintf("%s:%s", host, get_sname(htons(port), proto, numeric));
	free(host);
	return host_port;
}

static void tcp_do_one(int lnr, char *line)
{
	char local_addr[64], rem_addr[64];
	char more[512];
	int num, local_port, rem_port, d, state, timer_run, uid, timeout;
#if ENABLE_FEATURE_IPV6
	struct sockaddr_in6 localaddr, remaddr;
#else
	struct sockaddr_in localaddr, remaddr;
#endif
	unsigned long rxq, txq, time_len, retr, inode;

	if (lnr == 0)
		return;

	more[0] = '\0';
	num = sscanf(line,
			"%d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %lX:%lX %X:%lX %lX %d %d %ld %512s\n",
			&d, local_addr, &local_port,
			rem_addr, &rem_port, &state,
			&txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &inode, more);

	if (strlen(local_addr) > 8) {
#if ENABLE_FEATURE_IPV6
		build_ipv6_addr(local_addr, &localaddr);
		build_ipv6_addr(rem_addr, &remaddr);
#endif
	} else {
		build_ipv4_addr(local_addr, &localaddr);
		build_ipv4_addr(rem_addr, &remaddr);
	}

	if (num < 10) {
		bb_error_msg("warning, got bogus tcp line");
		return;
	}

	if ((rem_port && (flags & NETSTAT_CONNECTED))
	 || (!rem_port && (flags & NETSTAT_LISTENING))
	) {
		char *l = ip_port_str(
				(struct sockaddr *) &localaddr, local_port,
				"tcp", flags & NETSTAT_NUMERIC);
		char *r = ip_port_str(
				(struct sockaddr *) &remaddr, rem_port,
				"tcp", flags & NETSTAT_NUMERIC);
		printf(net_conn_line,
			"tcp", rxq, txq, l, r, tcp_state[state]);
		free(l);
		free(r);
	}
}

static void udp_do_one(int lnr, char *line)
{
	char local_addr[64], rem_addr[64];
	const char *state_str;
	char more[512];
	int num, local_port, rem_port, d, state, timer_run, uid, timeout;
#if ENABLE_FEATURE_IPV6
	struct sockaddr_in6 localaddr, remaddr;
#else
	struct sockaddr_in localaddr, remaddr;
#endif
	unsigned long rxq, txq, time_len, retr, inode;

	if (lnr == 0)
		return;

	more[0] = '\0';
	num = sscanf(line,
			"%d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %lX:%lX %X:%lX %lX %d %d %ld %512s\n",
			&d, local_addr, &local_port,
			rem_addr, &rem_port, &state,
			&txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &inode, more);

	if (strlen(local_addr) > 8) {
#if ENABLE_FEATURE_IPV6
		/* Demangle what the kernel gives us */
		build_ipv6_addr(local_addr, &localaddr);
		build_ipv6_addr(rem_addr, &remaddr);
#endif
	} else {
		build_ipv4_addr(local_addr, &localaddr);
		build_ipv4_addr(rem_addr, &remaddr);
	}

	if (num < 10) {
		bb_error_msg("warning, got bogus udp line");
		return;
	}
	switch (state) {
		case TCP_ESTABLISHED:
			state_str = "ESTABLISHED";
			break;
		case TCP_CLOSE:
			state_str = "";
			break;
		default:
			state_str = "UNKNOWN";
			break;
	}

#if ENABLE_FEATURE_IPV6
# define notnull(A) ( \
	( (A.sin6_family == AF_INET6)                               \
	  && (A.sin6_addr.s6_addr32[0] | A.sin6_addr.s6_addr32[1] | \
	      A.sin6_addr.s6_addr32[2] | A.sin6_addr.s6_addr32[3])  \
	) || (                                                      \
	  (A.sin6_family == AF_INET)                                \
	  && ((struct sockaddr_in*)&A)->sin_addr.s_addr             \
	)                                                           \
)
#else
# define notnull(A) (A.sin_addr.s_addr)
#endif
	{
		int have_remaddr = notnull(remaddr);
		if ((have_remaddr && (flags & NETSTAT_CONNECTED))
		 || (!have_remaddr && (flags & NETSTAT_LISTENING))
		) {
			char *l = ip_port_str(
				(struct sockaddr *) &localaddr, local_port,
				"udp", flags & NETSTAT_NUMERIC);
			char *r = ip_port_str(
				(struct sockaddr *) &remaddr, rem_port,
				"udp", flags & NETSTAT_NUMERIC);
			printf(net_conn_line,
				"udp", rxq, txq, l, r, state_str);
			free(l);
			free(r);
		}
	}
}

static void raw_do_one(int lnr, char *line)
{
	char local_addr[64], rem_addr[64];
	char more[512];
	int num, local_port, rem_port, d, state, timer_run, uid, timeout;
#if ENABLE_FEATURE_IPV6
	struct sockaddr_in6 localaddr, remaddr;
#else
	struct sockaddr_in localaddr, remaddr;
#endif
	unsigned long rxq, txq, time_len, retr, inode;

	if (lnr == 0)
		return;

	more[0] = '\0';
	num = sscanf(line,
			"%d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %lX:%lX %X:%lX %lX %d %d %ld %512s\n",
			&d, local_addr, &local_port,
			rem_addr, &rem_port, &state,
			&txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &inode, more);

	if (strlen(local_addr) > 8) {
#if ENABLE_FEATURE_IPV6
		build_ipv6_addr(local_addr, &localaddr);
		build_ipv6_addr(rem_addr, &remaddr);
#endif
	} else {
		build_ipv4_addr(local_addr, &localaddr);
		build_ipv4_addr(rem_addr, &remaddr);
	}

	if (num < 10) {
		bb_error_msg("warning, got bogus raw line");
		return;
	}

	{
		int have_remaddr = notnull(remaddr);
		if ((have_remaddr && (flags & NETSTAT_CONNECTED))
		 || (!have_remaddr && (flags & NETSTAT_LISTENING))
		) {
			char *l = ip_port_str(
				(struct sockaddr *) &localaddr, local_port,
				"raw", flags & NETSTAT_NUMERIC);
			char *r = ip_port_str(
				(struct sockaddr *) &remaddr, rem_port,
				"raw", flags & NETSTAT_NUMERIC);
			printf(net_conn_line,
				"raw", rxq, txq, l, r, itoa(state));
			free(l);
			free(r);
		}
	}
}

static void unix_do_one(int nr, char *line)
{
	unsigned long refcnt, proto, unix_flags;
	unsigned long inode;
	int type, state;
	int num, path_ofs;
	void *d;
	const char *ss_proto, *ss_state, *ss_type;
	char ss_flags[32];

	if (nr == 0)
		return; /* skip header */

	{
		char *last = last_char_is(line, '\n');
		if (last)
			*last = '\0';
	}

	/* 2.6.15 may report lines like "... @/tmp/fam-user-^@^@^@^@^@^@^@..."
	 * (those ^@ are NUL bytes). fgets sees them as tons of empty lines. */
	if (!line[0])
		return;

	path_ofs = 0; /* paranoia */
	num = sscanf(line, "%p: %lX %lX %lX %X %X %lu %n",
			&d, &refcnt, &proto, &unix_flags, &type, &state, &inode, &path_ofs);
	if (num < 7) {
		bb_error_msg("got bogus unix line '%s'", line);
		return;
	}
	if ((flags & (NETSTAT_LISTENING|NETSTAT_CONNECTED)) != (NETSTAT_LISTENING|NETSTAT_CONNECTED)) {
		if ((state == SS_UNCONNECTED) && (unix_flags & SO_ACCEPTCON)) {
			if (!(flags & NETSTAT_LISTENING))
				return;
		} else {
			if (!(flags & NETSTAT_CONNECTED))
				return;
		}
	}

	switch (proto) {
		case 0:
			ss_proto = "unix";
			break;
		default:
			ss_proto = "??";
	}

	switch (type) {
		case SOCK_STREAM:
			ss_type = "STREAM";
			break;
		case SOCK_DGRAM:
			ss_type = "DGRAM";
			break;
		case SOCK_RAW:
			ss_type = "RAW";
			break;
		case SOCK_RDM:
			ss_type = "RDM";
			break;
		case SOCK_SEQPACKET:
			ss_type = "SEQPACKET";
			break;
		default:
			ss_type = "UNKNOWN";
	}

	switch (state) {
		case SS_FREE:
			ss_state = "FREE";
			break;
		case SS_UNCONNECTED:
			/*
			 * Unconnected sockets may be listening
			 * for something.
			 */
			if (unix_flags & SO_ACCEPTCON) {
				ss_state = "LISTENING";
			} else {
				ss_state = "";
			}
			break;
		case SS_CONNECTING:
			ss_state = "CONNECTING";
			break;
		case SS_CONNECTED:
			ss_state = "CONNECTED";
			break;
		case SS_DISCONNECTING:
			ss_state = "DISCONNECTING";
			break;
		default:
			ss_state = "UNKNOWN";
	}

	strcpy(ss_flags, "[ ");
	if (unix_flags & SO_ACCEPTCON)
		strcat(ss_flags, "ACC ");
	if (unix_flags & SO_WAITDATA)
		strcat(ss_flags, "W ");
	if (unix_flags & SO_NOSPACE)
		strcat(ss_flags, "N ");
	strcat(ss_flags, "]");

	printf("%-5s %-6ld %-11s %-10s %-13s %6lu %s\n",
		ss_proto, refcnt, ss_flags, ss_type, ss_state, inode,
		line + path_ofs);
}

#define _PATH_PROCNET_UDP "/proc/net/udp"
#define _PATH_PROCNET_UDP6 "/proc/net/udp6"
#define _PATH_PROCNET_TCP "/proc/net/tcp"
#define _PATH_PROCNET_TCP6 "/proc/net/tcp6"
#define _PATH_PROCNET_RAW "/proc/net/raw"
#define _PATH_PROCNET_RAW6 "/proc/net/raw6"
#define _PATH_PROCNET_UNIX "/proc/net/unix"

static void do_info(const char *file, const char *name, void (*proc)(int, char *))
{
	int lnr;
	FILE *procinfo;
	char *buffer;

	procinfo = fopen(file, "r");
	if (procinfo == NULL) {
		if (errno != ENOENT) {
			bb_simple_perror_msg(file);
		} else {
			bb_error_msg("no support for '%s' on this system", name);
		}
		return;
	}
	lnr = 0;
	do {
		buffer = xmalloc_fgets(procinfo);
		if (buffer) {
			(proc)(lnr++, buffer);
			free(buffer);
		}
	} while (buffer);
	fclose(procinfo);
}

/*
 * Our main function.
 */

int netstat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int netstat_main(int argc, char **argv)
{
	const char *net_conn_line_header = PRINT_NET_CONN_HEADER;
	unsigned opt;
#if ENABLE_FEATURE_IPV6
	smallint inet = 1;
	smallint inet6 = 1;
#else
	enum { inet = 1, inet6 = 0 };
#endif

	/* Option string must match NETSTAT_xxx constants */
	opt = getopt32(argv, NETSTAT_OPTS);
	if (opt & 0x1) { // -l
		flags &= ~NETSTAT_CONNECTED;
		flags |= NETSTAT_LISTENING;
	}
	if (opt & 0x2) flags |= NETSTAT_LISTENING | NETSTAT_CONNECTED; // -a
	//if (opt & 0x4) // -e
	if (opt & 0x8) flags |= NETSTAT_NUMERIC; // -n
	//if (opt & 0x10) // -t: NETSTAT_TCP
	//if (opt & 0x20) // -u: NETSTAT_UDP
	//if (opt & 0x40) // -w: NETSTAT_RAW
	//if (opt & 0x80) // -x: NETSTAT_UNIX
	if (opt & OPT_showroute) { // -r
#if ENABLE_ROUTE
		bb_displayroutes(flags & NETSTAT_NUMERIC, !(opt & OPT_extended));
		return 0;
#else
		bb_show_usage();
#endif
	}

	if (opt & OPT_widedisplay) { // -W
		net_conn_line = PRINT_NET_CONN_WIDE;
		net_conn_line_header = PRINT_NET_CONN_HEADER_WIDE;
	}

	opt &= NETSTAT_ALLPROTO;
	if (opt) {
		flags &= ~NETSTAT_ALLPROTO;
		flags |= opt;
	}
	if (flags & (NETSTAT_TCP|NETSTAT_UDP|NETSTAT_RAW)) {
		printf("Active Internet connections ");	/* xxx */

		if ((flags & (NETSTAT_LISTENING|NETSTAT_CONNECTED)) == (NETSTAT_LISTENING|NETSTAT_CONNECTED))
			printf("(servers and established)");
		else if (flags & NETSTAT_LISTENING)
			printf("(only servers)");
		else
			printf("(w/o servers)");
		printf(net_conn_line_header, "Local Address", "Foreign Address");
	}
	if (inet && flags & NETSTAT_TCP)
		do_info(_PATH_PROCNET_TCP, "AF INET (tcp)", tcp_do_one);
#if ENABLE_FEATURE_IPV6
	if (inet6 && flags & NETSTAT_TCP)
		do_info(_PATH_PROCNET_TCP6, "AF INET6 (tcp)", tcp_do_one);
#endif
	if (inet && flags & NETSTAT_UDP)
		do_info(_PATH_PROCNET_UDP, "AF INET (udp)", udp_do_one);
#if ENABLE_FEATURE_IPV6
	if (inet6 && flags & NETSTAT_UDP)
		do_info(_PATH_PROCNET_UDP6, "AF INET6 (udp)", udp_do_one);
#endif
	if (inet && flags & NETSTAT_RAW)
		do_info(_PATH_PROCNET_RAW, "AF INET (raw)", raw_do_one);
#if ENABLE_FEATURE_IPV6
	if (inet6 && flags & NETSTAT_RAW)
		do_info(_PATH_PROCNET_RAW6, "AF INET6 (raw)", raw_do_one);
#endif
	if (flags & NETSTAT_UNIX) {
		printf("Active UNIX domain sockets ");
		if ((flags & (NETSTAT_LISTENING|NETSTAT_CONNECTED)) == (NETSTAT_LISTENING|NETSTAT_CONNECTED))
			printf("(servers and established)");
		else if (flags & NETSTAT_LISTENING)
			printf("(only servers)");
		else
			printf("(w/o servers)");
		printf("\nProto RefCnt Flags       Type       State         I-Node Path\n");
		do_info(_PATH_PROCNET_UNIX, "AF UNIX", unix_do_one);
	}
	return 0;
}
