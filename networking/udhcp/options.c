/* vi: set sw=4 ts=4: */
/*
 * options.c -- DHCP server option packet tools
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include "common.h"
#include "dhcpd.h"
#include "options.h"


/* supported options are easily added here */
const struct dhcp_option dhcp_options[] = {
	/* name[10]     flags                                   code */
	{"subnet",      OPTION_IP | OPTION_REQ,                 0x01},
	{"timezone",    OPTION_S32,                             0x02},
	{"router",      OPTION_IP | OPTION_LIST | OPTION_REQ,   0x03},
	{"timesvr",     OPTION_IP | OPTION_LIST,                0x04},
	{"namesvr",     OPTION_IP | OPTION_LIST,                0x05},
	{"dns",         OPTION_IP | OPTION_LIST | OPTION_REQ,   0x06},
	{"logsvr",      OPTION_IP | OPTION_LIST,                0x07},
	{"cookiesvr",   OPTION_IP | OPTION_LIST,                0x08},
	{"lprsvr",      OPTION_IP | OPTION_LIST,                0x09},
	{"hostname",    OPTION_STRING | OPTION_REQ,             0x0c},
	{"bootsize",    OPTION_U16,                             0x0d},
	{"domain",      OPTION_STRING | OPTION_REQ,             0x0f},
	{"swapsvr",     OPTION_IP,                              0x10},
	{"rootpath",    OPTION_STRING,                          0x11},
	{"ipttl",       OPTION_U8,                              0x17},
	{"mtu",         OPTION_U16,                             0x1a},
	{"broadcast",   OPTION_IP | OPTION_REQ,                 0x1c},
	{"nisdomain",   OPTION_STRING | OPTION_REQ,             0x28},
	{"nissrv",      OPTION_IP | OPTION_LIST | OPTION_REQ,   0x29},
	{"ntpsrv",      OPTION_IP | OPTION_LIST | OPTION_REQ,   0x2a},
	{"wins",        OPTION_IP | OPTION_LIST,                0x2c},
	{"requestip",   OPTION_IP,                              0x32},
	{"lease",       OPTION_U32,                             0x33},
	{"dhcptype",    OPTION_U8,                              0x35},
	{"serverid",    OPTION_IP,                              0x36},
	{"message",     OPTION_STRING,                          0x38},
	{"vendorclass", OPTION_STRING,                          0x3C},
	{"clientid",    OPTION_STRING,                          0x3D},
	{"tftp",        OPTION_STRING,                          0x42},
	{"bootfile",    OPTION_STRING,                          0x43},
	{"userclass",   OPTION_STRING,                          0x4D},
	/* MSIE's "Web Proxy Autodiscovery Protocol" support */
	{"wpad",        OPTION_STRING,                          0xfc},
	{"",            0x00,                                   0x00}
};

/* Lengths of the different option types */
const unsigned char option_lengths[] = {
	[OPTION_IP] =      4,
	[OPTION_IP_PAIR] = 8,
	[OPTION_BOOLEAN] = 1,
	[OPTION_STRING] =  1,
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
	int over = 0, done = 0, curr = OPTION_FIELD;

	optionptr = packet->options;
	i = 0;
	length = 308;
	while (!done) {
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
			if (curr == OPTION_FIELD && over & FILE_FIELD) {
				optionptr = packet->file;
				i = 0;
				length = 128;
				curr = FILE_FIELD;
			} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
				optionptr = packet->sname;
				i = 0;
				length = 64;
				curr = SNAME_FIELD;
			} else done = 1;
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
	if (end + string[OPT_LEN] + 2 + 1 >= 308) {
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
