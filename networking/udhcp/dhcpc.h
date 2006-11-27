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
	char release_on_quit;           /* perform release on quit */
	char abort_if_no_lease;         /* Abort if no lease */
	char background_if_no_lease;    /* Fork to background if no lease */
	char *interface;                /* The name of the interface to use */
	char *pidfile;                  /* Optionally store the process ID */
	char *script;                   /* User script to run at dhcp events */
	uint8_t *clientid;              /* Optional client id to use */
	uint8_t *vendorclass;           /* Optional vendor class-id to use */
	uint8_t *hostname;              /* Optional hostname to use */
	uint8_t *fqdn;                  /* Optional fully qualified domain name to use */
	int ifindex;                    /* Index number of the interface to use */
	int retries;                    /* Max number of request packets */
	int timeout;                    /* Number of seconds to try to get a lease */
	uint8_t arp[6];                 /* Our arp address */
};

extern struct client_config_t client_config;


/*** clientpacket.h ***/

unsigned long random_xid(void);
int send_discover(unsigned long xid, unsigned long requested);
int send_selecting(unsigned long xid, unsigned long server, unsigned long requested);
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_renew(unsigned long xid, unsigned long server, unsigned long ciaddr);
int send_release(unsigned long server, unsigned long ciaddr);
int get_raw_packet(struct dhcpMessage *payload, int fd);


#endif
