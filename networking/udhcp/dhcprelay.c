/* vi: set sw=4 ts=4: */
/* Port to Busybox Copyright (C) 2006 Jesse Dutton <jessedutton@gmail.com>
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 *
 * DHCP Relay for 'DHCPv4 Configuration of IPSec Tunnel Mode' support
 * Copyright (C) 2002 Mario Strasser <mast@gmx.net>,
 *                   Zuercher Hochschule Winterthur,
 *                   Netbeat AG
 * Upstream has GPL v2 or later
 */

#include "common.h"
#include "options.h"

/* constants */
#define SERVER_PORT      67
#define SELECT_TIMEOUT    5 /* select timeout in sec. */
#define MAX_LIFETIME   2*60 /* lifetime of an xid entry in sec. */

/* This list holds information about clients. The xid_* functions manipulate this list. */
struct xid_item {
	unsigned timestamp;
	int client;
	uint32_t xid;
	struct sockaddr_in ip;
	struct xid_item *next;
};

#define dhcprelay_xid_list (*(struct xid_item*)&bb_common_bufsiz1)

static struct xid_item *xid_add(uint32_t xid, struct sockaddr_in *ip, int client)
{
	struct xid_item *item;

	/* create new xid entry */
	item = xmalloc(sizeof(struct xid_item));

	/* add xid entry */
	item->ip = *ip;
	item->xid = xid;
	item->client = client;
	item->timestamp = monotonic_sec();
	item->next = dhcprelay_xid_list.next;
	dhcprelay_xid_list.next = item;

	return item;
}

static void xid_expire(void)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	struct xid_item *last = &dhcprelay_xid_list;
	unsigned current_time = monotonic_sec();

	while (item != NULL) {
		if ((current_time - item->timestamp) > MAX_LIFETIME) {
			last->next = item->next;
			free(item);
			item = last->next;
		} else {
			last = item;
			item = item->next;
		}
	}
}

static struct xid_item *xid_find(uint32_t xid)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	while (item != NULL) {
		if (item->xid == xid) {
			return item;
		}
		item = item->next;
	}
	return NULL;
}

static void xid_del(uint32_t xid)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	struct xid_item *last = &dhcprelay_xid_list;
	while (item != NULL) {
		if (item->xid == xid) {
			last->next = item->next;
			free(item);
			item = last->next;
		} else {
			last = item;
			item = item->next;
		}
	}
}

/**
 * get_dhcp_packet_type - gets the message type of a dhcp packet
 * p - pointer to the dhcp packet
 * returns the message type on success, -1 otherwise
 */
static int get_dhcp_packet_type(struct dhcpMessage *p)
{
	uint8_t *op;

	/* it must be either a BOOTREQUEST or a BOOTREPLY */
	if (p->op != BOOTREQUEST && p->op != BOOTREPLY)
		return -1;
	/* get message type option */
	op = get_option(p, DHCP_MESSAGE_TYPE);
	if (op != NULL)
		return op[0];
	return -1;
}

/**
 * get_client_devices - parses the devices list
 * dev_list - comma separated list of devices
 * returns array
 */
static char **get_client_devices(char *dev_list, int *client_number)
{
	char *s, **client_dev;
	int i, cn;

	/* copy list */
	dev_list = xstrdup(dev_list);

	/* get number of items, replace ',' with NULs */
	s = dev_list;
	cn = 1;
	while (*s) {
		if (*s == ',') {
			*s = '\0';
			cn++;
		}
		s++;
	}
	*client_number = cn;

	/* create vector of pointers */
	client_dev = xzalloc(cn * sizeof(*client_dev));
	client_dev[0] = dev_list;
	i = 1;
	while (i != cn) {
		client_dev[i] = client_dev[i - 1] + strlen(client_dev[i - 1]) + 1;
		i++;
	}
	return client_dev;
}


/* Creates listen sockets (in fds) and returns numerically max fd. */
static int init_sockets(char **client, int num_clients,
			char *server, int *fds)
{
	int i, n;

	/* talk to real server on bootps */
	fds[0] = listen_socket(/*INADDR_ANY,*/ SERVER_PORT, server);
	n = fds[0];

	for (i = 1; i < num_clients; i++) {
		/* listen for clients on bootps */
		fds[i] = listen_socket(/*INADDR_ANY,*/ SERVER_PORT, client[i-1]);
		if (fds[i] > n)
			n = fds[i];
	}
	return n;
}


/**
 * pass_on() - forwards dhcp packets from client to server
 * p - packet to send
 * client - number of the client
 */
static void pass_on(struct dhcpMessage *p, int packet_len, int client, int *fds,
			struct sockaddr_in *client_addr, struct sockaddr_in *server_addr)
{
	int res, type;
	struct xid_item *item;

	/* check packet_type */
	type = get_dhcp_packet_type(p);
	if (type != DHCPDISCOVER && type != DHCPREQUEST
	 && type != DHCPDECLINE && type != DHCPRELEASE
	 && type != DHCPINFORM
	) {
		return;
	}

	/* create new xid entry */
	item = xid_add(p->xid, client_addr, client);

	/* forward request to LAN (server) */
	res = sendto(fds[0], p, packet_len, 0, (struct sockaddr*)server_addr,
			sizeof(struct sockaddr_in));
	if (res != packet_len) {
		bb_perror_msg("pass_on");
		return;
	}
}

/**
 * pass_back() - forwards dhcp packets from server to client
 * p - packet to send
 */
static void pass_back(struct dhcpMessage *p, int packet_len, int *fds)
{
	int res, type;
	struct xid_item *item;

	/* check xid */
	item = xid_find(p->xid);
	if (!item) {
		return;
	}

	/* check packet type */
	type = get_dhcp_packet_type(p);
	if (type != DHCPOFFER && type != DHCPACK && type != DHCPNAK) {
		return;
	}

	if (item->ip.sin_addr.s_addr == htonl(INADDR_ANY))
		item->ip.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	res = sendto(fds[item->client], p, packet_len, 0, (struct sockaddr*)(&item->ip),
				sizeof(item->ip));
	if (res != packet_len) {
		bb_perror_msg("pass_back");
		return;
	}

	/* remove xid entry */
	xid_del(p->xid);
}

static void dhcprelay_loop(int *fds, int num_sockets, int max_socket, char **clients,
		struct sockaddr_in *server_addr, uint32_t gw_ip) ATTRIBUTE_NORETURN;
static void dhcprelay_loop(int *fds, int num_sockets, int max_socket, char **clients,
		struct sockaddr_in *server_addr, uint32_t gw_ip)
{
	struct dhcpMessage dhcp_msg;
	fd_set rfds;
	size_t packlen;
	socklen_t addr_size;
	struct sockaddr_in client_addr;
	struct timeval tv;
	int i;

	while (1) {
		FD_ZERO(&rfds);
		for (i = 0; i < num_sockets; i++)
			FD_SET(fds[i], &rfds);
		tv.tv_sec = SELECT_TIMEOUT;
		tv.tv_usec = 0;
		if (select(max_socket + 1, &rfds, NULL, NULL, &tv) > 0) {
			/* server */
			if (FD_ISSET(fds[0], &rfds)) {
				packlen = udhcp_recv_kernel_packet(&dhcp_msg, fds[0]);
				if (packlen > 0) {
					pass_back(&dhcp_msg, packlen, fds);
				}
			}
			for (i = 1; i < num_sockets; i++) {
				/* clients */
				if (!FD_ISSET(fds[i], &rfds))
					continue;
				addr_size = sizeof(struct sockaddr_in);
				packlen = recvfrom(fds[i], &dhcp_msg, sizeof(dhcp_msg), 0,
							(struct sockaddr *)(&client_addr), &addr_size);
				if (packlen <= 0)
					continue;
				if (read_interface(clients[i-1], NULL, &dhcp_msg.giaddr, NULL))
					dhcp_msg.giaddr = gw_ip;
				pass_on(&dhcp_msg, packlen, i, fds, &client_addr, server_addr);
			}
		}
		xid_expire();
	}
}

int dhcprelay_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dhcprelay_main(int argc, char **argv)
{
	int num_sockets, max_socket;
	int *fds;
	uint32_t gw_ip;
	char **clients;
	struct sockaddr_in server_addr;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	if (argc == 4) {
		if (!inet_aton(argv[3], &server_addr.sin_addr))
			bb_perror_msg_and_die("didn't grok server");
	} else if (argc == 3) {
		server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	} else {
		bb_show_usage();
	}

	clients = get_client_devices(argv[1], &num_sockets);
	num_sockets++; /* for server socket at fds[0] */
	fds = xmalloc(num_sockets * sizeof(fds[0]));
	max_socket = init_sockets(clients, num_sockets, argv[2], fds);

	if (read_interface(argv[2], NULL, &gw_ip, NULL))
		return 1;

	/* doesn't return */
	dhcprelay_loop(fds, num_sockets, max_socket, clients, &server_addr, gw_ip);
	/* return 0; - not reached */
}
