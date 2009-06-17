/* vi: set sw=4 ts=4: */
/* dhcpc.h */
#ifndef UDHCP_DHCPC_H
#define UDHCP_DHCPC_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

struct client_config_t {
	uint8_t client_mac[6];          /* Our mac address */
	char no_default_options;        /* Do not include default options in request */
	IF_FEATURE_UDHCP_PORT(uint16_t port;)
	int ifindex;                    /* Index number of the interface to use */
	int verbose;
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

uint32_t random_xid(void) FAST_FUNC;
int send_discover(uint32_t xid, uint32_t requested) FAST_FUNC;
int send_select(uint32_t xid, uint32_t server, uint32_t requested) FAST_FUNC;
#if ENABLE_FEATURE_UDHCPC_ARPING
int send_decline(uint32_t xid, uint32_t server, uint32_t requested) FAST_FUNC;
#endif
int send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr) FAST_FUNC;
int send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr) FAST_FUNC;
int send_release(uint32_t server, uint32_t ciaddr) FAST_FUNC;

int udhcp_recv_raw_packet(struct dhcp_packet *dhcp_pkt, int fd) FAST_FUNC;

POP_SAVED_FUNCTION_VISIBILITY

#endif
