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
	/* opt_name[12] flags                                   code */
	{"subnet",      OPTION_IP | OPTION_REQ,                 0x01},   /* DHCP_SUBNET         */
	{"timezone",    OPTION_S32,                             0x02},   /* DHCP_TIME_OFFSET    */
	{"router",      OPTION_IP | OPTION_LIST | OPTION_REQ,   0x03},   /* DHCP_ROUTER         */
	{"timesvr",     OPTION_IP | OPTION_LIST,                0x04},   /* DHCP_TIME_SERVER    */
	{"namesvr",     OPTION_IP | OPTION_LIST,                0x05},   /* DHCP_NAME_SERVER    */
	{"dns",         OPTION_IP | OPTION_LIST | OPTION_REQ,   0x06},   /* DHCP_DNS_SERVER     */
	{"logsvr",      OPTION_IP | OPTION_LIST,                0x07},   /* DHCP_LOG_SERVER     */
	{"cookiesvr",   OPTION_IP | OPTION_LIST,                0x08},   /* DHCP_COOKIE_SERVER  */
	{"lprsvr",      OPTION_IP | OPTION_LIST,                0x09},   /* DHCP_LPR_SERVER     */
	{"hostname",    OPTION_STRING | OPTION_REQ,             0x0c},   /* DHCP_HOST_NAME      */
	{"bootsize",    OPTION_U16,                             0x0d},   /* DHCP_BOOT_SIZE      */
	{"domain",      OPTION_STRING | OPTION_LIST | OPTION_REQ, 0x0f}, /* DHCP_DOMAIN_NAME    */
	{"swapsvr",     OPTION_IP,                              0x10},   /* DHCP_SWAP_SERVER    */
	{"rootpath",    OPTION_STRING,                          0x11},   /* DHCP_ROOT_PATH      */
	{"ipttl",       OPTION_U8,                              0x17},   /* DHCP_IP_TTL         */
	{"mtu",         OPTION_U16,                             0x1a},   /* DHCP_MTU            */
	{"broadcast",   OPTION_IP | OPTION_REQ,                 0x1c},   /* DHCP_BROADCAST      */
	{"nisdomain",   OPTION_STRING | OPTION_REQ,             0x28},   /* DHCP_NTP_SERVER     */
	{"nissrv",      OPTION_IP | OPTION_LIST | OPTION_REQ,   0x29},   /* DHCP_WINS_SERVER    */
	{"ntpsrv",      OPTION_IP | OPTION_LIST | OPTION_REQ,   0x2a},   /* DHCP_REQUESTED_IP   */
	{"wins",        OPTION_IP | OPTION_LIST,                0x2c},   /* DHCP_LEASE_TIME     */
	{"requestip",   OPTION_IP,                              0x32},   /* DHCP_OPTION_OVER    */
	{"lease",       OPTION_U32,                             0x33},   /* DHCP_MESSAGE_TYPE   */
	{"dhcptype",    OPTION_U8,                              0x35},   /* DHCP_SERVER_ID      */
	{"serverid",    OPTION_IP,                              0x36},   /* DHCP_PARAM_REQ      */
	{"message",     OPTION_STRING,                          0x38},   /* DHCP_MESSAGE        */
// TODO: 1) some options should not be parsed & passed to script -
// maxsize sure should not, since it cannot appear in server responses!
// grep for opt_name is fix the mess.
// 2) Using fixed-sized char[] vector wastes space.
	{"maxsize",     OPTION_U16,                             0x39},   /* DHCP_MAX_SIZE       */
	{"vendorclass", OPTION_STRING,                          0x3C},   /* DHCP_VENDOR         */
	{"clientid",    OPTION_STRING,                          0x3D},   /* DHCP_CLIENT_ID      */
	{"tftp",        OPTION_STRING,                          0x42},
	{"bootfile",    OPTION_STRING,                          0x43},
	{"userclass",   OPTION_STRING,                          0x4D},
#if ENABLE_FEATURE_RFC3397
	{"search",      OPTION_STR1035 | OPTION_LIST | OPTION_REQ, 0x77},
#endif
	/* MSIE's "Web Proxy Autodiscovery Protocol" support */
	{"wpad",        OPTION_STRING,                          0xfc},
	{} /* zero-padded terminating entry */
};


/* Lengths of the different option types */
const unsigned char option_lengths[] ALIGN1 = {
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
		if (optionptr[i] == DHCP_PADDING) i++;
		else i += optionptr[i + OPT_LEN] + 2;
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
			len = option_lengths[dh->flags & TYPE_MASK];
			option[OPT_LEN] = len;
			if (BB_BIG_ENDIAN) data <<= 8 * (4 - len);
			/* This memcpy is for broken processors which can't
			 * handle a simple unaligned 32-bit assignment */
			memcpy(&option[OPT_DATA], &data, 4);
			return add_option_string(optionptr, option);
		}
	}

	bb_error_msg("cannot add option 0x%02x", code);
	return 0;
}
