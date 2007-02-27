/* vi: set sw=4 ts=4: */
/* options.h */
#ifndef _OPTIONS_H
#define _OPTIONS_H

#define TYPE_MASK	0x0F

enum {
	OPTION_IP=1,
	OPTION_IP_PAIR,
	OPTION_STRING,
#if ENABLE_FEATURE_RFC3397
	OPTION_STR1035,	/* RFC1035 compressed domain name list */
#endif
	OPTION_BOOLEAN,
	OPTION_U8,
	OPTION_U16,
	OPTION_S16,
	OPTION_U32,
	OPTION_S32
};

#define OPTION_REQ	0x10 /* have the client request this option */
#define OPTION_LIST	0x20 /* There can be a list of 1 or more of these */

struct dhcp_option {
	char name[12];
	char flags;
	uint8_t code;
};

extern const struct dhcp_option dhcp_options[];
extern const unsigned char option_lengths[];

uint8_t *get_option(struct dhcpMessage *packet, int code);
int end_option(uint8_t *optionptr);
int add_option_string(uint8_t *optionptr, uint8_t *string);
int add_simple_option(uint8_t *optionptr, uint8_t code, uint32_t data);
#if ENABLE_FEATURE_RFC3397
char *dname_dec(const uint8_t *cstr, int clen, const char *pre);
uint8_t *dname_enc(const uint8_t *cstr, int clen, const char *src, int *retlen);
#endif

#endif
