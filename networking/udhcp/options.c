/* vi: set sw=4 ts=4: */
/*
 * options.c -- DHCP server option packet tools
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include "common.h"
#include "dhcpd.h"
#include "options.h"


/* Supported options are easily added here */
const struct dhcp_option dhcp_options[] = {
	/* flags                                    code */
	{ OPTION_IP                   | OPTION_REQ, 0x01 }, /* DHCP_SUBNET        */
	{ OPTION_S32                              , 0x02 }, /* DHCP_TIME_OFFSET   */
	{ OPTION_IP | OPTION_LIST     | OPTION_REQ, 0x03 }, /* DHCP_ROUTER        */
	{ OPTION_IP | OPTION_LIST                 , 0x04 }, /* DHCP_TIME_SERVER   */
	{ OPTION_IP | OPTION_LIST                 , 0x05 }, /* DHCP_NAME_SERVER   */
	{ OPTION_IP | OPTION_LIST     | OPTION_REQ, 0x06 }, /* DHCP_DNS_SERVER    */
	{ OPTION_IP | OPTION_LIST                 , 0x07 }, /* DHCP_LOG_SERVER    */
	{ OPTION_IP | OPTION_LIST                 , 0x08 }, /* DHCP_COOKIE_SERVER */
	{ OPTION_IP | OPTION_LIST                 , 0x09 }, /* DHCP_LPR_SERVER    */
	{ OPTION_STRING               | OPTION_REQ, 0x0c }, /* DHCP_HOST_NAME     */
	{ OPTION_U16                              , 0x0d }, /* DHCP_BOOT_SIZE     */
	{ OPTION_STRING | OPTION_LIST | OPTION_REQ, 0x0f }, /* DHCP_DOMAIN_NAME   */
	{ OPTION_IP                               , 0x10 }, /* DHCP_SWAP_SERVER   */
	{ OPTION_STRING                           , 0x11 }, /* DHCP_ROOT_PATH     */
	{ OPTION_U8                               , 0x17 }, /* DHCP_IP_TTL        */
	{ OPTION_U16                              , 0x1a }, /* DHCP_MTU           */
	{ OPTION_IP                   | OPTION_REQ, 0x1c }, /* DHCP_BROADCAST     */
	{ OPTION_STRING                           , 0x28 }, /* nisdomain          */
	{ OPTION_IP | OPTION_LIST                 , 0x29 }, /* nissrv             */
	{ OPTION_IP | OPTION_LIST     | OPTION_REQ, 0x2a }, /* DHCP_NTP_SERVER    */
	{ OPTION_IP | OPTION_LIST                 , 0x2c }, /* DHCP_WINS_SERVER   */
	{ OPTION_IP                               , 0x32 }, /* DHCP_REQUESTED_IP  */
	{ OPTION_U32                              , 0x33 }, /* DHCP_LEASE_TIME    */
	{ OPTION_U8                               , 0x35 }, /* dhcptype           */
	{ OPTION_IP                               , 0x36 }, /* DHCP_SERVER_ID     */
	{ OPTION_STRING                           , 0x38 }, /* DHCP_MESSAGE       */
	{ OPTION_STRING                           , 0x3C }, /* DHCP_VENDOR        */
	{ OPTION_STRING                           , 0x3D }, /* DHCP_CLIENT_ID     */
	{ OPTION_STRING                           , 0x42 }, /* tftp               */
	{ OPTION_STRING                           , 0x43 }, /* bootfile           */
	{ OPTION_STRING                           , 0x4D }, /* userclass          */
#if ENABLE_FEATURE_RFC3397
	{ OPTION_STR1035 | OPTION_LIST            , 0x77 }, /* search             */
#endif
	/* MSIE's "Web Proxy Autodiscovery Protocol" support */
	{ OPTION_STRING                           , 0xfc }, /* wpad               */

	/* Options below have no match in dhcp_option_strings[],
	 * are not passed to dhcpc scripts, and cannot be specified
	 * with "option XXX YYY" syntax in dhcpd config file. */

	{ OPTION_U16                              , 0x39 }, /* DHCP_MAX_SIZE      */
	{ } /* zeroed terminating entry */
};

/* Used for converting options from incoming packets to env variables
 * for udhcpc stript */
/* Must match dhcp_options[] order */
const char dhcp_option_strings[] ALIGN1 =
	"subnet" "\0"      /* DHCP_SUBNET         */
	"timezone" "\0"    /* DHCP_TIME_OFFSET    */
	"router" "\0"      /* DHCP_ROUTER         */
	"timesrv" "\0"     /* DHCP_TIME_SERVER    */
	"namesrv" "\0"     /* DHCP_NAME_SERVER    */
	"dns" "\0"         /* DHCP_DNS_SERVER     */
	"logsrv" "\0"      /* DHCP_LOG_SERVER     */
	"cookiesrv" "\0"   /* DHCP_COOKIE_SERVER  */
	"lprsrv" "\0"      /* DHCP_LPR_SERVER     */
	"hostname" "\0"    /* DHCP_HOST_NAME      */
	"bootsize" "\0"    /* DHCP_BOOT_SIZE      */
	"domain" "\0"      /* DHCP_DOMAIN_NAME    */
	"swapsrv" "\0"     /* DHCP_SWAP_SERVER    */
	"rootpath" "\0"    /* DHCP_ROOT_PATH      */
	"ipttl" "\0"       /* DHCP_IP_TTL         */
	"mtu" "\0"         /* DHCP_MTU            */
	"broadcast" "\0"   /* DHCP_BROADCAST      */
	"nisdomain" "\0"   /*                     */
	"nissrv" "\0"      /*                     */
	"ntpsrv" "\0"      /* DHCP_NTP_SERVER     */
	"wins" "\0"        /* DHCP_WINS_SERVER    */
	"requestip" "\0"   /* DHCP_REQUESTED_IP   */
	"lease" "\0"       /* DHCP_LEASE_TIME     */
	"dhcptype" "\0"    /*                     */
	"serverid" "\0"    /* DHCP_SERVER_ID      */
	"message" "\0"     /* DHCP_MESSAGE        */
	"vendorclass" "\0" /* DHCP_VENDOR         */
	"clientid" "\0"    /* DHCP_CLIENT_ID      */
	"tftp" "\0"
	"bootfile" "\0"
	"userclass" "\0"
#if ENABLE_FEATURE_RFC3397
	"search" "\0"
#endif
	/* MSIE's "Web Proxy Autodiscovery Protocol" support */
	"wpad" "\0"
	;


/* Lengths of the different option types */
const uint8_t dhcp_option_lengths[] ALIGN1 = {
	[OPTION_IP] =      4,
	[OPTION_IP_PAIR] = 8,
	[OPTION_BOOLEAN] = 1,
	[OPTION_STRING] =  1,
#if ENABLE_FEATURE_RFC3397
	[OPTION_STR1035] = 1,
#endif
	[OPTION_U8] =      1,
	[OPTION_U16] =     2,
	[OPTION_S16] =     2,
	[OPTION_U32] =     4,
	[OPTION_S32] =     4
};


/* get an option with bounds checking (warning, not aligned). */
uint8_t *get_option(struct dhcpMessage *packet, int code)
{
	int i, length;
	uint8_t *optionptr;
	int over = 0;
	int curr = OPTION_FIELD;

	optionptr = packet->options;
	i = 0;
	length = sizeof(packet->options);
	while (1) {
		if (i >= length) {
			bb_error_msg("bogus packet, option fields too long");
			return NULL;
		}
		if (optionptr[i + OPT_CODE] == code) {
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				bb_error_msg("bogus packet, option fields too long");
				return NULL;
			}
			return optionptr + i + 2;
		}
		switch (optionptr[i + OPT_CODE]) {
		case DHCP_PADDING:
			i++;
			break;
		case DHCP_OPTION_OVER:
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				bb_error_msg("bogus packet, option fields too long");
				return NULL;
			}
			over = optionptr[i + 3];
			i += optionptr[OPT_LEN] + 2;
			break;
		case DHCP_END:
			if (curr == OPTION_FIELD && (over & FILE_FIELD)) {
				optionptr = packet->file;
				i = 0;
				length = sizeof(packet->file);
				curr = FILE_FIELD;
			} else if (curr == FILE_FIELD && (over & SNAME_FIELD)) {
				optionptr = packet->sname;
				i = 0;
				length = sizeof(packet->sname);
				curr = SNAME_FIELD;
			} else
				return NULL;
			break;
		default:
			i += optionptr[OPT_LEN + i] + 2;
		}
	}
	return NULL;
}


/* return the position of the 'end' option (no bounds checking) */
int end_option(uint8_t *optionptr)
{
	int i = 0;

	while (optionptr[i] != DHCP_END) {
		if (optionptr[i] == DHCP_PADDING)
			i++;
		else
			i += optionptr[i + OPT_LEN] + 2;
	}
	return i;
}


/* add an option string to the options (an option string contains an option code,
 * length, then data) */
int add_option_string(uint8_t *optionptr, uint8_t *string)
{
	int end = end_option(optionptr);

	/* end position + string length + option code/length + end option */
	if (end + string[OPT_LEN] + 2 + 1 >= DHCP_OPTIONS_BUFSIZE) {
		bb_error_msg("option 0x%02x did not fit into the packet",
				string[OPT_CODE]);
		return 0;
	}
	DEBUG("adding option 0x%02x", string[OPT_CODE]);
	memcpy(optionptr + end, string, string[OPT_LEN] + 2);
	optionptr[end + string[OPT_LEN] + 2] = DHCP_END;
	return string[OPT_LEN] + 2;
}


/* add a one to four byte option to a packet */
int add_simple_option(uint8_t *optionptr, uint8_t code, uint32_t data)
{
	const struct dhcp_option *dh;

	for (dh = dhcp_options; dh->code; dh++) {
		if (dh->code == code) {
			uint8_t option[6], len;

			option[OPT_CODE] = code;
			len = dhcp_option_lengths[dh->flags & TYPE_MASK];
			option[OPT_LEN] = len;
			if (BB_BIG_ENDIAN)
				data <<= 8 * (4 - len);
			/* This memcpy is for processors which can't
			 * handle a simple unaligned 32-bit assignment */
			memcpy(&option[OPT_DATA], &data, 4);
			return add_option_string(optionptr, option);
		}
	}

	bb_error_msg("cannot add option 0x%02x", code);
	return 0;
}
