/* vi: set sw=4 ts=4: */
/*
 * Mini netstat implementation(s) for busybox
 * based in part on the netstat implementation from net-tools.
 *
 * Copyright (C) 2002 by Bart Visscher <magick@linux-fan.com>
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
 * 2002-04-20
 * IPV6 support added by Bart Visscher <magick@linux-fan.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "inet_common.h"
#include "busybox.h"
#include "pwd_.h"

#ifdef CONFIG_ROUTE
extern void displayroutes(int noresolve, int netstatfmt);
#endif

#define NETSTAT_CONNECTED	0x01
#define NETSTAT_LISTENING	0x02
#define NETSTAT_NUMERIC		0x04
#define NETSTAT_TCP			0x10
#define NETSTAT_UDP			0x20
#define NETSTAT_RAW			0x40
#define NETSTAT_UNIX		0x80

static int flags = NETSTAT_CONNECTED |
			NETSTAT_TCP | NETSTAT_UDP | NETSTAT_RAW | NETSTAT_UNIX;

#define PROGNAME_WIDTHs PROGNAME_WIDTH1(PROGNAME_WIDTH)
#define PROGNAME_WIDTH1(s) PROGNAME_WIDTH2(s)
#define PROGNAME_WIDTH2(s) #s

#define PRG_HASH_SIZE 211

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
    TCP_CLOSING			/* now a valid state */
};

static const char * const tcp_state[] =
{
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
    SS_FREE = 0,		/* not allocated                */
    SS_UNCONNECTED,		/* unconnected to any socket    */
    SS_CONNECTING,		/* in process of connecting     */
    SS_CONNECTED,		/* connected to socket          */
    SS_DISCONNECTING		/* in process of disconnecting  */
} socket_state;

#define SO_ACCEPTCON    (1<<16)	/* performed a listen           */
#define SO_WAITDATA     (1<<17)	/* wait data to read            */
#define SO_NOSPACE      (1<<18)	/* no space to write            */

static char *itoa(unsigned int i)
{
	/* 21 digits plus null terminator, good for 64-bit or smaller ints */
	static char local[22];
	char *p = &local[21];
	*p-- = '\0';
	do {
		*p-- = '0' + i % 10;
		i /= 10;
	} while (i > 0);
	return p + 1;
}

static char *get_sname(int port, const char *proto, int num)
{
	char *str=itoa(ntohs(port));
	if (num) {
	} else {
		struct servent *se=getservbyport(port,proto);
		if (se)
			str=se->s_name;
	}
	if (!port) {
		str="*";
	}
	return str;
}

static void snprint_ip_port(char *ip_port, int size, struct sockaddr *addr, int port, char *proto, int numeric)
{
	char *port_name;

#ifdef CONFIG_FEATURE_IPV6
	if (addr->sa_family == AF_INET6) {
		INET6_rresolve(ip_port, size, (struct sockaddr_in6 *)addr,
					   (numeric&NETSTAT_NUMERIC) ? 0x0fff : 0);
	} else
#endif
	{
	INET_rresolve(ip_port, size, (struct sockaddr_in *)addr,
		0x4000 | ((numeric&NETSTAT_NUMERIC) ? 0x0fff : 0),
		0xffffffff);
	}
	port_name=get_sname(htons(port), proto, numeric);
	if ((strlen(ip_port) + strlen(port_name)) > 22)
		ip_port[22 - strlen(port_name)] = '\0';
	ip_port+=strlen(ip_port);
	strcat(ip_port, ":");
	strcat(ip_port, port_name);
}

static void tcp_do_one(int lnr, const char *line)
{
	char local_addr[64], rem_addr[64];
	const char *state_str;
	char more[512];
	int num, local_port, rem_port, d, state, timer_run, uid, timeout;
#ifdef CONFIG_FEATURE_IPV6
	struct sockaddr_in6 localaddr, remaddr;
	char addr6[INET6_ADDRSTRLEN];
	struct in6_addr in6;
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
#ifdef CONFIG_FEATURE_IPV6
		sscanf(local_addr, "%08X%08X%08X%08X",
			   &in6.s6_addr32[0], &in6.s6_addr32[1],
			   &in6.s6_addr32[2], &in6.s6_addr32[3]);
		inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
		inet_pton(AF_INET6, addr6, (struct sockaddr *) &localaddr.sin6_addr);
		sscanf(rem_addr, "%08X%08X%08X%08X",
			   &in6.s6_addr32[0], &in6.s6_addr32[1],
			   &in6.s6_addr32[2], &in6.s6_addr32[3]);
		inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
		inet_pton(AF_INET6, addr6, (struct sockaddr *) &remaddr.sin6_addr);
		localaddr.sin6_family = AF_INET6;
		remaddr.sin6_family = AF_INET6;
#endif
	} else {
		sscanf(local_addr, "%X",
			   &((struct sockaddr_in *) &localaddr)->sin_addr.s_addr);
		sscanf(rem_addr, "%X",
			   &((struct sockaddr_in *) &remaddr)->sin_addr.s_addr);
		((struct sockaddr *) &localaddr)->sa_family = AF_INET;
		((struct sockaddr *) &remaddr)->sa_family = AF_INET;
	}

	if (num < 10) {
		bb_error_msg("warning, got bogus tcp line.");
		return;
	}
	state_str = tcp_state[state];
	if ((rem_port && (flags&NETSTAT_CONNECTED)) ||
		(!rem_port && (flags&NETSTAT_LISTENING)))
	{
		snprint_ip_port(local_addr, sizeof(local_addr),
						(struct sockaddr *) &localaddr, local_port,
						"tcp", flags&NETSTAT_NUMERIC);

		snprint_ip_port(rem_addr, sizeof(rem_addr),
						(struct sockaddr *) &remaddr, rem_port,
						"tcp", flags&NETSTAT_NUMERIC);

		printf("tcp   %6ld %6ld %-23s %-23s %-12s\n",
			   rxq, txq, local_addr, rem_addr, state_str);

	}
}

static void udp_do_one(int lnr, const char *line)
{
	char local_addr[64], rem_addr[64];
	char *state_str, more[512];
	int num, local_port, rem_port, d, state, timer_run, uid, timeout;
#ifdef CONFIG_FEATURE_IPV6
	struct sockaddr_in6 localaddr, remaddr;
	char addr6[INET6_ADDRSTRLEN];
	struct in6_addr in6;
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
#ifdef CONFIG_FEATURE_IPV6
        /* Demangle what the kernel gives us */
		sscanf(local_addr, "%08X%08X%08X%08X",
			   &in6.s6_addr32[0], &in6.s6_addr32[1],
			   &in6.s6_addr32[2], &in6.s6_addr32[3]);
		inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
		inet_pton(AF_INET6, addr6, (struct sockaddr *) &localaddr.sin6_addr);
		sscanf(rem_addr, "%08X%08X%08X%08X",
			   &in6.s6_addr32[0], &in6.s6_addr32[1],
			   &in6.s6_addr32[2], &in6.s6_addr32[3]);
		inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
		inet_pton(AF_INET6, addr6, (struct sockaddr *) &remaddr.sin6_addr);
		localaddr.sin6_family = AF_INET6;
		remaddr.sin6_family = AF_INET6;
#endif
	} else {
		sscanf(local_addr, "%X",
			   &((struct sockaddr_in *) &localaddr)->sin_addr.s_addr);
		sscanf(rem_addr, "%X",
			   &((struct sockaddr_in *) &remaddr)->sin_addr.s_addr);
		((struct sockaddr *) &localaddr)->sa_family = AF_INET;
		((struct sockaddr *) &remaddr)->sa_family = AF_INET;
	}

	if (num < 10) {
		bb_error_msg("warning, got bogus udp line.");
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

#ifdef CONFIG_FEATURE_IPV6
# define notnull(A) (((A.sin6_family == AF_INET6) &&            \
					 ((A.sin6_addr.s6_addr32[0]) ||            \
					  (A.sin6_addr.s6_addr32[1]) ||            \
					  (A.sin6_addr.s6_addr32[2]) ||            \
					  (A.sin6_addr.s6_addr32[3]))) ||          \
					((A.sin6_family == AF_INET) &&             \
					 ((struct sockaddr_in *) &A)->sin_addr.s_addr))
#else
# define notnull(A) (A.sin_addr.s_addr)
#endif
	if ((notnull(remaddr) && (flags&NETSTAT_CONNECTED)) ||
		(!notnull(remaddr) && (flags&NETSTAT_LISTENING)))
	{
		snprint_ip_port(local_addr, sizeof(local_addr),
						(struct sockaddr *) &localaddr, local_port,
						"udp", flags&NETSTAT_NUMERIC);

		snprint_ip_port(rem_addr, sizeof(rem_addr),
						(struct sockaddr *) &remaddr, rem_port,
						"udp", flags&NETSTAT_NUMERIC);

		printf("udp   %6ld %6ld %-23s %-23s %-12s\n",
			   rxq, txq, local_addr, rem_addr, state_str);

	}
}

static void raw_do_one(int lnr, const char *line)
{
	char local_addr[64], rem_addr[64];
	char *state_str, more[512];
	int num, local_port, rem_port, d, state, timer_run, uid, timeout;
#ifdef CONFIG_FEATURE_IPV6
	struct sockaddr_in6 localaddr, remaddr;
	char addr6[INET6_ADDRSTRLEN];
	struct in6_addr in6;
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
#ifdef CONFIG_FEATURE_IPV6
		sscanf(local_addr, "%08X%08X%08X%08X",
			   &in6.s6_addr32[0], &in6.s6_addr32[1],
			   &in6.s6_addr32[2], &in6.s6_addr32[3]);
		inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
		inet_pton(AF_INET6, addr6, (struct sockaddr *) &localaddr.sin6_addr);
		sscanf(rem_addr, "%08X%08X%08X%08X",
			   &in6.s6_addr32[0], &in6.s6_addr32[1],
			   &in6.s6_addr32[2], &in6.s6_addr32[3]);
		inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
		inet_pton(AF_INET6, addr6, (struct sockaddr *) &remaddr.sin6_addr);
		localaddr.sin6_family = AF_INET6;
		remaddr.sin6_family = AF_INET6;
#endif
	} else {
		sscanf(local_addr, "%X",
			   &((struct sockaddr_in *) &localaddr)->sin_addr.s_addr);
		sscanf(rem_addr, "%X",
			   &((struct sockaddr_in *) &remaddr)->sin_addr.s_addr);
		((struct sockaddr *) &localaddr)->sa_family = AF_INET;
		((struct sockaddr *) &remaddr)->sa_family = AF_INET;
	}

	if (num < 10) {
		bb_error_msg("warning, got bogus raw line.");
		return;
	}
	state_str=itoa(state);

#ifdef CONFIG_FEATURE_IPV6
# define notnull(A) (((A.sin6_family == AF_INET6) &&            \
					 ((A.sin6_addr.s6_addr32[0]) ||            \
					  (A.sin6_addr.s6_addr32[1]) ||            \
					  (A.sin6_addr.s6_addr32[2]) ||            \
					  (A.sin6_addr.s6_addr32[3]))) ||          \
					((A.sin6_family == AF_INET) &&             \
					 ((struct sockaddr_in *) &A)->sin_addr.s_addr))
#else
# define notnull(A) (A.sin_addr.s_addr)
#endif
	if ((notnull(remaddr) && (flags&NETSTAT_CONNECTED)) ||
		(!notnull(remaddr) && (flags&NETSTAT_LISTENING)))
	{
		snprint_ip_port(local_addr, sizeof(local_addr),
						(struct sockaddr *) &localaddr, local_port,
						"raw", flags&NETSTAT_NUMERIC);

		snprint_ip_port(rem_addr, sizeof(rem_addr),
						(struct sockaddr *) &remaddr, rem_port,
						"raw", flags&NETSTAT_NUMERIC);

		printf("raw   %6ld %6ld %-23s %-23s %-12s\n",
			   rxq, txq, local_addr, rem_addr, state_str);

	}
}

#define HAS_INODE 1

static void unix_do_one(int nr, const char *line)
{
	static int has = 0;
	char path[PATH_MAX], ss_flags[32];
	char *ss_proto, *ss_state, *ss_type;
	int num, state, type, inode;
	void *d;
	unsigned long refcnt, proto, unix_flags;

	if (nr == 0) {
		if (strstr(line, "Inode"))
			has |= HAS_INODE;
		return;
	}
	path[0] = '\0';
	num = sscanf(line, "%p: %lX %lX %lX %X %X %d %s",
				 &d, &refcnt, &proto, &unix_flags, &type, &state, &inode, path);
	if (num < 6) {
		bb_error_msg("warning, got bogus unix line.");
		return;
	}
	if (!(has & HAS_INODE))
		snprintf(path,sizeof(path),"%d",inode);

	if ((flags&(NETSTAT_LISTENING|NETSTAT_CONNECTED))!=(NETSTAT_LISTENING|NETSTAT_CONNECTED)) {
		if ((state == SS_UNCONNECTED) && (unix_flags & SO_ACCEPTCON)) {
			if (!(flags&NETSTAT_LISTENING))
				return;
		} else {
			if (!(flags&NETSTAT_CONNECTED))
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

	printf("%-5s %-6ld %-11s %-10s %-13s ",
		   ss_proto, refcnt, ss_flags, ss_type, ss_state);
	if (has & HAS_INODE)
		printf("%-6d ",inode);
	else
		printf("-      ");
	puts(path);
}

#define _PATH_PROCNET_UDP "/proc/net/udp"
#define _PATH_PROCNET_UDP6 "/proc/net/udp6"
#define _PATH_PROCNET_TCP "/proc/net/tcp"
#define _PATH_PROCNET_TCP6 "/proc/net/tcp6"
#define _PATH_PROCNET_RAW "/proc/net/raw"
#define _PATH_PROCNET_RAW6 "/proc/net/raw6"
#define _PATH_PROCNET_UNIX "/proc/net/unix"

static void do_info(const char *file, const char *name, void (*proc)(int, const char *))
{
	char buffer[8192];
	int lnr = 0;
	FILE *procinfo;

	procinfo = fopen(file, "r");
	if (procinfo == NULL) {
		if (errno != ENOENT) {
			perror(file);
		} else {
		bb_error_msg("no support for `%s' on this system.", name);
		}
	} else {
		do {
			if (fgets(buffer, sizeof(buffer), procinfo))
				(proc)(lnr++, buffer);
		} while (!feof(procinfo));
		fclose(procinfo);
	}
}

/*
 * Our main function.
 */

int netstat_main(int argc, char **argv)
{
	int opt;
	int new_flags=0;
	int showroute = 0, extended = 0;
#ifdef CONFIG_FEATURE_IPV6
	int inet=1;
	int inet6=1;
#else
# define inet 1
# define inet6 0
#endif
	while ((opt = getopt(argc, argv, "laenrtuwx")) != -1)
		switch (opt) {
		case 'l':
			flags &= ~NETSTAT_CONNECTED;
			flags |= NETSTAT_LISTENING;
			break;
		case 'a':
			flags |= NETSTAT_LISTENING | NETSTAT_CONNECTED;
			break;
		case 'n':
			flags |= NETSTAT_NUMERIC;
			break;
		case 'r':
			showroute = 1;
			break;
		case 'e':
			extended = 1;
			break;
		case 't':
			new_flags |= NETSTAT_TCP;
			break;
		case 'u':
			new_flags |= NETSTAT_UDP;
			break;
		case 'w':
			new_flags |= NETSTAT_RAW;
			break;
		case 'x':
			new_flags |= NETSTAT_UNIX;
			break;
		default:
			bb_show_usage();
		}
	if ( showroute ) {
#ifdef CONFIG_ROUTE
		displayroutes ( flags & NETSTAT_NUMERIC, !extended );
		return 0;
#else
		bb_error_msg_and_die( "-r (display routing table) is not compiled in." );
#endif
	}

	if (new_flags) {
		flags &= ~(NETSTAT_TCP|NETSTAT_UDP|NETSTAT_RAW|NETSTAT_UNIX);
		flags |= new_flags;
	}
	if (flags&(NETSTAT_TCP|NETSTAT_UDP|NETSTAT_RAW)) {
		printf("Active Internet connections ");	/* xxx */

		if ((flags&(NETSTAT_LISTENING|NETSTAT_CONNECTED))==(NETSTAT_LISTENING|NETSTAT_CONNECTED))
			printf("(servers and established)");
		else {
			if (flags&NETSTAT_LISTENING)
				printf("(only servers)");
			else
				printf("(w/o servers)");
		}
		printf("\nProto Recv-Q Send-Q Local Address           Foreign Address         State      \n");
	}
	if (inet && flags&NETSTAT_TCP)
		do_info(_PATH_PROCNET_TCP,"AF INET (tcp)",tcp_do_one);
#ifdef CONFIG_FEATURE_IPV6
	if (inet6 && flags&NETSTAT_TCP)
		do_info(_PATH_PROCNET_TCP6,"AF INET6 (tcp)",tcp_do_one);
#endif
	if (inet && flags&NETSTAT_UDP)
		do_info(_PATH_PROCNET_UDP,"AF INET (udp)",udp_do_one);
#ifdef CONFIG_FEATURE_IPV6
	if (inet6 && flags&NETSTAT_UDP)
		do_info(_PATH_PROCNET_UDP6,"AF INET6 (udp)",udp_do_one);
#endif
	if (inet && flags&NETSTAT_RAW)
		do_info(_PATH_PROCNET_RAW,"AF INET (raw)",raw_do_one);
#ifdef CONFIG_FEATURE_IPV6
	if (inet6 && flags&NETSTAT_RAW)
		do_info(_PATH_PROCNET_RAW6,"AF INET6 (raw)",raw_do_one);
#endif
	if (flags&NETSTAT_UNIX) {
		printf("Active UNIX domain sockets ");
		if ((flags&(NETSTAT_LISTENING|NETSTAT_CONNECTED))==(NETSTAT_LISTENING|NETSTAT_CONNECTED))
			printf("(servers and established)");
		else {
			if (flags&NETSTAT_LISTENING)
				printf("(only servers)");
			else
				printf("(w/o servers)");
		}

		printf("\nProto RefCnt Flags       Type       State         I-Node Path\n");
		do_info(_PATH_PROCNET_UNIX,"AF UNIX",unix_do_one);
	}
	return 0;
}
