/* vi: set sw=4 ts=4: */
/* dhcpc.h */

#ifndef _DHCPC_H
#define _DHCPC_H

#define INIT_SELECTING	0
#define REQUESTING	1
#define BOUND		2
#define RENEWING	3
#define REBINDING	4
#define INIT_REBOOT	5
#define RENEW_REQUESTED 6
#define RELEASED	7

struct client_config_t {
	/* TODO: combine flag fields into single "unsigned opt" */
	/* (can be set directly to the result of getopt32) */
	char foreground;                /* Do not fork */
	char quit_after_lease;          /* Quit after obtaining lease */
	char release_on_quit;           /* Perform release on quit */
	char abort_if_no_lease;         /* Abort if no lease */
	char background_if_no_lease;    /* Fork to background if no lease */
	const char *interface;          /* The name of the interface to use */
	char *pidfile;                  /* Optionally store the process ID */
	const char *script;             /* User script to run at dhcp events */
	uint8_t *clientid;              /* Optional client id to use */
	uint8_t *vendorclass;           /* Optional vendor class-id to use */
	uint8_t *hostname;              /* Optional hostname to use */
	uint8_t *fqdn;                  /* Optional fully qualified domain name to use */
	int ifindex;                    /* Index number of the interface to use */
	uint8_t arp[6];                 /* Our arp address */
	uint8_t opt_mask[256 / 8];      /* Bitmask of options to send (-O option) */
};

#define client_config (*(struct client_config_t*)&bb_common_bufsiz1)


/*** clientpacket.h ***/

uint32_t random_xid(void);
int send_discover(uint32_t xid, uint32_t requested);
int send_selecting(uint32_t xid, uint32_t server, uint32_t requested);
#if ENABLE_FEATURE_UDHCPC_ARPING
int send_decline(uint32_t xid, uint32_t server);
#endif
int send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr);
int send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr);
int send_release(uint32_t server, uint32_t ciaddr);
int get_raw_packet(struct dhcpMessage *payload, int fd);


#endif
