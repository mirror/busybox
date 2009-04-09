/* vi: set sw=4 ts=4: */
/* options.h */
#ifndef UDHCP_OPTIONS_H
#define UDHCP_OPTIONS_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

#define TYPE_MASK       0x0F

enum {
	OPTION_IP = 1,
	OPTION_IP_PAIR,
	OPTION_STRING,
#if ENABLE_FEATURE_UDHCP_RFC3397
	OPTION_STR1035,	/* RFC1035 compressed domain name list */
#endif
	OPTION_BOOLEAN,
	OPTION_U8,
	OPTION_U16,
	OPTION_S16,
	OPTION_U32,
	OPTION_S32
};

#define OPTION_REQ      0x10 /* have the client request this option */
#define OPTION_LIST     0x20 /* There can be a list of 1 or more of these */

/*****************************************************************/
/* Do not modify below here unless you know what you are doing!! */
/*****************************************************************/

/* DHCP protocol -- see RFC 2131 */
#define DHCP_MAGIC		0x63825363

/* DHCP option codes (partial list) */
#define DHCP_PADDING            0x00
#define DHCP_SUBNET             0x01
#define DHCP_TIME_OFFSET        0x02
#define DHCP_ROUTER             0x03
#define DHCP_TIME_SERVER        0x04
#define DHCP_NAME_SERVER        0x05
#define DHCP_DNS_SERVER         0x06
#define DHCP_LOG_SERVER         0x07
#define DHCP_COOKIE_SERVER      0x08
#define DHCP_LPR_SERVER         0x09
#define DHCP_HOST_NAME          0x0c
#define DHCP_BOOT_SIZE          0x0d
#define DHCP_DOMAIN_NAME        0x0f
#define DHCP_SWAP_SERVER        0x10
#define DHCP_ROOT_PATH          0x11
#define DHCP_IP_TTL             0x17
#define DHCP_MTU                0x1a
#define DHCP_BROADCAST          0x1c
#define DHCP_NTP_SERVER         0x2a
#define DHCP_WINS_SERVER        0x2c
#define DHCP_REQUESTED_IP       0x32
#define DHCP_LEASE_TIME         0x33
#define DHCP_OPTION_OVERLOAD    0x34
#define DHCP_MESSAGE_TYPE       0x35
#define DHCP_SERVER_ID          0x36
#define DHCP_PARAM_REQ          0x37
#define DHCP_MESSAGE            0x38
#define DHCP_MAX_SIZE           0x39
#define DHCP_T1                 0x3a
#define DHCP_T2                 0x3b
#define DHCP_VENDOR             0x3c
#define DHCP_CLIENT_ID          0x3d
#define DHCP_FQDN               0x51
#define DHCP_END                0xFF
/* Offsets in option byte sequence */
#define OPT_CODE                0
#define OPT_LEN                 1
#define OPT_DATA                2
/* Bits in "overload" option */
#define OPTION_FIELD            0
#define FILE_FIELD              1
#define SNAME_FIELD             2

#define BOOTREQUEST             1
#define BOOTREPLY               2

#define ETH_10MB                1
#define ETH_10MB_LEN            6

#define DHCPDISCOVER            1 /* client -> server */
#define DHCPOFFER               2 /* client <- server */
#define DHCPREQUEST             3 /* client -> server */
#define DHCPDECLINE             4 /* client -> server */
#define DHCPACK                 5 /* client <- server */
#define DHCPNAK                 6 /* client <- server */
#define DHCPRELEASE             7 /* client -> server */
#define DHCPINFORM              8 /* client -> server */

struct dhcp_option {
	uint8_t flags;
	uint8_t code;
};

extern const struct dhcp_option dhcp_options[];
extern const char dhcp_option_strings[];
extern const uint8_t dhcp_option_lengths[];

uint8_t *get_option(struct dhcpMessage *packet, int code) FAST_FUNC;
int end_option(uint8_t *optionptr) FAST_FUNC;
int add_option_string(uint8_t *optionptr, uint8_t *string) FAST_FUNC;
int add_simple_option(uint8_t *optionptr, uint8_t code, uint32_t data) FAST_FUNC;
#if ENABLE_FEATURE_UDHCP_RFC3397
char *dname_dec(const uint8_t *cstr, int clen, const char *pre) FAST_FUNC;
uint8_t *dname_enc(const uint8_t *cstr, int clen, const char *src, int *retlen) FAST_FUNC;
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
