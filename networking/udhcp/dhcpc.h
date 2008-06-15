/* vi: set sw=4 ts=4: */
/* dhcpc.h */

#ifndef _DHCPC_H
#define _DHCPC_H

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

struct client_config_t {
	uint8_t arp[6];                 /* Our arp address */
	/* TODO: combine flag fields into single "unsigned opt" */
	/* (can be set directly to the result of getopt32) */
	char no_default_options;        /* Do not include default optins in request */
	USE_FEATURE_UDHCP_PORT(uint16_t port;)
	int ifindex;                    /* Index number of the interface to use */
	uint8_t opt_mask[256 / 8];      /* Bitmask of options to send (-O option) */
	const char *interface;          /* The name of the interface to use */
	char *pidfile;                  /* Optionally store the process ID */
	const char *script;             /* User script to run at dhcp events */
	uint8_t *clientid;              /* Optional client id to use */
	uint8_t *vendorclass;           /* Optional vendor class-id to use */
	uint8_t *hostname;              /* Optional hostname to use */
	uint8_t *fqdn;                  /* Optional fully qualified domain name to use */
};

/* server_config sits in 1st half of bb_common_bufsiz1 */
#define client_config (*(struct client_config_t*)(&bb_common_bufsiz1[COMMON_BUFSIZE / 2]))

#if ENABLE_FEATURE_UDHCP_PORT
#define CLIENT_PORT (client_config.port)
#else
#define CLIENT_PORT 68
#endif


/*** clientpacket.h ***/

uint32_t random_xid(void);
int send_discover(uint32_t xid, uint32_t requested);
int send_selecting(uint32_t xid, uint32_t server, uint32_t requested);
#if ENABLE_FEATURE_UDHCPC_ARPING
int send_decline(uint32_t xid, uint32_t server, uint32_t requested);
#endif
int send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr);
int send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr);
int send_release(uint32_t server, uint32_t ciaddr);

int udhcp_recv_raw_packet(struct dhcpMessage *payload, int fd);

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif

#endif
