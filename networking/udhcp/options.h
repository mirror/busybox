/* vi: set sw=4 ts=4: */
/* options.h */
#ifndef UDHCP_OPTIONS_H
#define UDHCP_OPTIONS_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN


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
	OPTION_S32,
	OPTION_STATIC_ROUTES,

	OPTION_TYPE_MASK = 0x0f,
	/* Client requests this option by default */
	OPTION_REQ  = 0x10,
	/* There can be a list of 1 or more of these */
	OPTION_LIST = 0x20,
};

/* DHCP option codes (partial list). See RFC 2132 and
 * http://www.iana.org/assignments/bootp-dhcp-parameters/
 * Commented out options are handled by common option machinery,
 * uncommented ones have spacial cases (grep for them to see).
 */
#define DHCP_PADDING            0x00
#define DHCP_SUBNET             0x01
//#define DHCP_TIME_OFFSET      0x02 /* (localtime - UTC_time) in seconds. signed */
//#define DHCP_ROUTER           0x03
//#define DHCP_TIME_SERVER      0x04 /* RFC 868 time server (32-bit, 0 = 1.1.1900) */
//#define DHCP_NAME_SERVER      0x05 /* IEN 116 _really_ ancient kind of NS */
//#define DHCP_DNS_SERVER       0x06
//#define DHCP_LOG_SERVER       0x07 /* port 704 UDP log (not syslog)
//#define DHCP_COOKIE_SERVER    0x08 /* "quote of the day" server */
//#define DHCP_LPR_SERVER       0x09
#define DHCP_HOST_NAME          0x0c /* either client informs server or server gives name to client */
//#define DHCP_BOOT_SIZE        0x0d
//#define DHCP_DOMAIN_NAME      0x0f /* server gives domain suffix */
//#define DHCP_SWAP_SERVER      0x10
//#define DHCP_ROOT_PATH        0x11
//#define DHCP_IP_TTL           0x17
//#define DHCP_MTU              0x1a
//#define DHCP_BROADCAST        0x1c
//#define DHCP_NIS_DOMAIN       0x28
//#define DHCP_NIS_SERVER       0x29
//#define DHCP_NTP_SERVER       0x2a
//#define DHCP_WINS_SERVER      0x2c
#define DHCP_REQUESTED_IP       0x32 /* sent by client if specific IP is wanted */
#define DHCP_LEASE_TIME         0x33
#define DHCP_OPTION_OVERLOAD    0x34
#define DHCP_MESSAGE_TYPE       0x35
#define DHCP_SERVER_ID          0x36 /* by default server's IP */
#define DHCP_PARAM_REQ          0x37 /* list of options client wants */
//#define DHCP_ERR_MESSAGE      0x38 /* error message when sending NAK etc */
#define DHCP_MAX_SIZE           0x39
#define DHCP_VENDOR             0x3c /* client's vendor (a string) */
#define DHCP_CLIENT_ID          0x3d /* by default client's MAC addr, but may be arbitrarily long */
//#define DHCP_TFTP_SERVER_NAME 0x42 /* same as 'sname' field */
//#define DHCP_BOOT_FILE        0x43 /* same as 'file' field */
//#define DHCP_USER_CLASS       0x4d /* RFC 3004. set of LASCII strings. "I am a printer" etc */
#define DHCP_FQDN               0x51 /* client asks to update DNS to map its FQDN to its new IP */
//#define DHCP_DOMAIN_SEARCH    0x77 /* RFC 3397. set of ASCIZ string, DNS-style compressed */
//#define DHCP_STATIC_ROUTES    0x79 /* RFC 3442. (mask,ip,router) tuples */
//#define DHCP_WPAD             0xfc /* MSIE's Web Proxy Autodiscovery Protocol */
#define DHCP_END                0xff

/* Offsets in option byte sequence */
#define OPT_CODE                0
#define OPT_LEN                 1
#define OPT_DATA                2
/* Bits in "overload" option */
#define OPTION_FIELD            0
#define FILE_FIELD              1
#define SNAME_FIELD             2

/* DHCP_MESSAGE_TYPE values */
#define DHCPDISCOVER            1 /* client -> server */
#define DHCPOFFER               2 /* client <- server */
#define DHCPREQUEST             3 /* client -> server */
#define DHCPDECLINE             4 /* client -> server */
#define DHCPACK                 5 /* client <- server */
#define DHCPNAK                 6 /* client <- server */
#define DHCPRELEASE             7 /* client -> server */
#define DHCPINFORM              8 /* client -> server */
#define DHCP_MINTYPE DHCPDISCOVER
#define DHCP_MAXTYPE DHCPINFORM


struct dhcp_option {
	uint8_t flags;
	uint8_t code;
};

extern const struct dhcp_option dhcp_options[];
extern const char dhcp_option_strings[];
extern const uint8_t dhcp_option_lengths[];

uint8_t *get_option(struct dhcp_packet *packet, int code) FAST_FUNC;
int end_option(uint8_t *optionptr) FAST_FUNC;
void add_option_string(uint8_t *optionptr, uint8_t *string) FAST_FUNC;
void add_simple_option(uint8_t *optionptr, uint8_t code, uint32_t data) FAST_FUNC;
#if ENABLE_FEATURE_UDHCP_RFC3397
char *dname_dec(const uint8_t *cstr, int clen, const char *pre) FAST_FUNC;
uint8_t *dname_enc(const uint8_t *cstr, int clen, const char *src, int *retlen) FAST_FUNC;
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
