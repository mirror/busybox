/* vi: set sw=4 ts=4: */
/* script.c
 *
 * Functions to call the DHCP client notification scripts
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "common.h"
#include "dhcpc.h"
#include "options.h"


/* get a rough idea of how long an option will be (rounding up...) */
static const uint8_t max_option_length[] = {
	[OPTION_IP] =		sizeof("255.255.255.255 "),
	[OPTION_IP_PAIR] =	sizeof("255.255.255.255 ") * 2,
	[OPTION_STRING] =	1,
#if ENABLE_FEATURE_UDHCP_RFC3397
	[OPTION_STR1035] =	1,
#endif
	[OPTION_BOOLEAN] =	sizeof("yes "),
	[OPTION_U8] =		sizeof("255 "),
	[OPTION_U16] =		sizeof("65535 "),
	[OPTION_S16] =		sizeof("-32768 "),
	[OPTION_U32] =		sizeof("4294967295 "),
	[OPTION_S32] =		sizeof("-2147483684 "),
};


static inline int upper_length(int length, int opt_index)
{
	return max_option_length[opt_index] *
		(length / dhcp_option_lengths[opt_index]);
}


static int sprintip(char *dest, const char *pre, const uint8_t *ip)
{
	return sprintf(dest, "%s%d.%d.%d.%d", pre, ip[0], ip[1], ip[2], ip[3]);
}


/* really simple implementation, just count the bits */
static int mton(uint32_t mask)
{
	int i = 0;
	mask = ntohl(mask); /* 111110000-like bit pattern */
	while (mask) {
		i++;
		mask <<= 1;
	}
	return i;
}


/* Allocate and fill with the text of option 'option'. */
static char *alloc_fill_opts(uint8_t *option, const struct dhcp_option *type_p, const char *opt_name)
{
	int len, type, optlen;
	uint16_t val_u16;
	int16_t val_s16;
	uint32_t val_u32;
	int32_t val_s32;
	char *dest, *ret;

	len = option[OPT_LEN - 2];
	type = type_p->flags & TYPE_MASK;
	optlen = dhcp_option_lengths[type];

	dest = ret = xmalloc(upper_length(len, type) + strlen(opt_name) + 2);
	dest += sprintf(ret, "%s=", opt_name);

	for (;;) {
		switch (type) {
		case OPTION_IP_PAIR:
			dest += sprintip(dest, "", option);
			*dest++ = '/';
			option += 4;
			optlen = 4;
		case OPTION_IP:	/* Works regardless of host byte order. */
			dest += sprintip(dest, "", option);
			break;
		case OPTION_BOOLEAN:
			dest += sprintf(dest, *option ? "yes" : "no");
			break;
		case OPTION_U8:
			dest += sprintf(dest, "%u", *option);
			break;
		case OPTION_U16:
			move_from_unaligned16(val_u16, option);
			dest += sprintf(dest, "%u", ntohs(val_u16));
			break;
		case OPTION_S16:
			move_from_unaligned16(val_s16, option);
			dest += sprintf(dest, "%d", ntohs(val_s16));
			break;
		case OPTION_U32:
			move_from_unaligned32(val_u32, option);
			dest += sprintf(dest, "%lu", (unsigned long) ntohl(val_u32));
			break;
		case OPTION_S32:
			move_from_unaligned32(val_s32, option);
			dest += sprintf(dest, "%ld", (long) ntohl(val_s32));
			break;
		case OPTION_STRING:
			memcpy(dest, option, len);
			dest[len] = '\0';
			return ret;	 /* Short circuit this case */
#if ENABLE_FEATURE_UDHCP_RFC3397
		case OPTION_STR1035:
			/* unpack option into dest; use ret for prefix (i.e., "optname=") */
			dest = dname_dec(option, len, ret);
			free(ret);
			return dest;
#endif
		}
		option += optlen;
		len -= optlen;
		if (len <= 0)
			break;
		dest += sprintf(dest, " ");
	}
	return ret;
}


/* put all the parameters into an environment */
static char **fill_envp(struct dhcpMessage *packet)
{
	int num_options = 0;
	int i;
	char **envp, **curr;
	const char *opt_name;
	uint8_t *temp;
	uint8_t over = 0;

	if (packet) {
		for (i = 0; dhcp_options[i].code; i++) {
			if (get_option(packet, dhcp_options[i].code)) {
				num_options++;
				if (dhcp_options[i].code == DHCP_SUBNET)
					num_options++; /* for mton */
			}
		}
		if (packet->siaddr)
			num_options++;
		temp = get_option(packet, DHCP_OPTION_OVERLOAD);
		if (temp)
			over = *temp;
		if (!(over & FILE_FIELD) && packet->file[0])
			num_options++;
		if (!(over & SNAME_FIELD) && packet->sname[0])
			num_options++;
	}

	curr = envp = xzalloc(sizeof(char *) * (num_options + 3));
	*curr = xasprintf("interface=%s", client_config.interface);
	putenv(*curr++);

	if (packet == NULL)
		return envp;

	*curr = xmalloc(sizeof("ip=255.255.255.255"));
	sprintip(*curr, "ip=", (uint8_t *) &packet->yiaddr);
	putenv(*curr++);

	opt_name = dhcp_option_strings;
	i = 0;
	while (*opt_name) {
		temp = get_option(packet, dhcp_options[i].code);
		if (!temp)
			goto next;
		*curr = alloc_fill_opts(temp, &dhcp_options[i], opt_name);
		putenv(*curr++);

		/* Fill in a subnet bits option for things like /24 */
		if (dhcp_options[i].code == DHCP_SUBNET) {
			uint32_t subnet;
			move_from_unaligned32(subnet, temp);
			*curr = xasprintf("mask=%d", mton(subnet));
			putenv(*curr++);
		}
 next:
		opt_name += strlen(opt_name) + 1;
		i++;
	}
	if (packet->siaddr) {
		*curr = xmalloc(sizeof("siaddr=255.255.255.255"));
		sprintip(*curr, "siaddr=", (uint8_t *) &packet->siaddr);
		putenv(*curr++);
	}
	if (!(over & FILE_FIELD) && packet->file[0]) {
		/* watch out for invalid packets */
		packet->file[sizeof(packet->file) - 1] = '\0';
		*curr = xasprintf("boot_file=%s", packet->file);
		putenv(*curr++);
	}
	if (!(over & SNAME_FIELD) && packet->sname[0]) {
		/* watch out for invalid packets */
		packet->sname[sizeof(packet->sname) - 1] = '\0';
		*curr = xasprintf("sname=%s", packet->sname);
		putenv(*curr++);
	}
	return envp;
}


/* Call a script with a par file and env vars */
void FAST_FUNC udhcp_run_script(struct dhcpMessage *packet, const char *name)
{
	char **envp, **curr;
	char *argv[3];

	if (client_config.script == NULL)
		return;

	DEBUG("vfork'ing and exec'ing %s", client_config.script);

	envp = fill_envp(packet);

	/* call script */
	argv[0] = (char*) client_config.script;
	argv[1] = (char*) name;
	argv[2] = NULL;
	wait4pid(spawn(argv));

	for (curr = envp; *curr; curr++) {
		bb_unsetenv(*curr);
		free(*curr);
	}
	free(envp);
}
