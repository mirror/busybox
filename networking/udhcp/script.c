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
static const uint8_t len_of_option_as_string[] = {
	[OPTION_IP] =		sizeof("255.255.255.255 "),
	[OPTION_IP_PAIR] =	sizeof("255.255.255.255 ") * 2,
	[OPTION_STATIC_ROUTES]= sizeof("255.255.255.255/32 255.255.255.255 "),
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


/* note: ip is a pointer to an IP in network order, possibly misaliged */
static int sprint_nip(char *dest, const char *pre, const uint8_t *ip)
{
	return sprintf(dest, "%s%u.%u.%u.%u", pre, ip[0], ip[1], ip[2], ip[3]);
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


/* Create "opt_name=opt_value" string */
static NOINLINE char *xmalloc_optname_optval(uint8_t *option, const struct dhcp_option *type_p, const char *opt_name)
{
	unsigned upper_length;
	int len, type, optlen;
	uint16_t val_u16;
	int16_t val_s16;
	uint32_t val_u32;
	int32_t val_s32;
	char *dest, *ret;

	/* option points to OPT_DATA, need to go back and get OPT_LEN */
	len = option[OPT_LEN - OPT_DATA];
	type = type_p->flags & TYPE_MASK;
	optlen = dhcp_option_lengths[type];
	upper_length = len_of_option_as_string[type] * (len / optlen);

	dest = ret = xmalloc(upper_length + strlen(opt_name) + 2);
	dest += sprintf(ret, "%s=", opt_name);

	while (len >= optlen) {
		switch (type) {
		case OPTION_IP_PAIR:
			dest += sprint_nip(dest, "", option);
			*dest++ = '/';
			option += 4;
			optlen = 4;
		case OPTION_IP:	/* Works regardless of host byte order. */
			dest += sprint_nip(dest, "", option);
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
		case OPTION_STATIC_ROUTES: {
			/* Option binary format:
			 * mask [one byte, 0..32]
			 * ip [big endian, 0..4 bytes depending on mask]
			 * router [big endian, 4 bytes]
			 * may be repeated
			 *
			 * We convert it to a string "IP/MASK ROUTER IP2/MASK2 ROUTER2"
			 */
			const char *pfx = "";

			while (len >= 1 + 4) { /* mask + 0-byte ip + router */
				uint32_t nip;
				uint8_t *p;
				unsigned mask;
				int bytes;

				mask = *option++;
				if (mask > 32)
					break;
				len--;

				nip = 0;
				p = (void*) &nip;
				bytes = (mask + 7) / 8; /* 0 -> 0, 1..8 -> 1, 9..16 -> 2 etc */
				while (--bytes >= 0) {
					*p++ = *option++;
					len--;
				}
				if (len < 4)
					break;

				/* print ip/mask */
				dest += sprint_nip(dest, pfx, (void*) &nip);
				pfx = " ";
				dest += sprintf(dest, "/%u ", mask);
				/* print router */
				dest += sprint_nip(dest, "", option);
				option += 4;
				len -= 4;
			}

			return ret;
		}
#if ENABLE_FEATURE_UDHCP_RFC3397
		case OPTION_STR1035:
			/* unpack option into dest; use ret for prefix (i.e., "optname=") */
			dest = dname_dec(option, len, ret);
			if (dest) {
				free(ret);
				return dest;
			}
			/* error. return "optname=" string */
			return ret;
#endif
		}
		option += optlen;
		len -= optlen;
		if (len <= 0)
			break;
		*dest++ = ' ';
		*dest = '\0';
	}
	return ret;
}


/* put all the parameters into an environment */
static char **fill_envp(struct dhcp_packet *packet)
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
		if (packet->siaddr_nip)
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
	sprint_nip(*curr, "ip=", (uint8_t *) &packet->yiaddr);
	putenv(*curr++);

	opt_name = dhcp_option_strings;
	i = 0;
	while (*opt_name) {
		temp = get_option(packet, dhcp_options[i].code);
		if (!temp)
			goto next;
		*curr = xmalloc_optname_optval(temp, &dhcp_options[i], opt_name);
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
	if (packet->siaddr_nip) {
		*curr = xmalloc(sizeof("siaddr=255.255.255.255"));
		sprint_nip(*curr, "siaddr=", (uint8_t *) &packet->siaddr_nip);
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
void FAST_FUNC udhcp_run_script(struct dhcp_packet *packet, const char *name)
{
	char **envp, **curr;
	char *argv[3];

	if (client_config.script == NULL)
		return;

	envp = fill_envp(packet);

	/* call script */
	log1("Executing %s %s", client_config.script, name);
	argv[0] = (char*) client_config.script;
	argv[1] = (char*) name;
	argv[2] = NULL;
	wait4pid(spawn(argv));

	for (curr = envp; *curr; curr++) {
		log2(" %s", *curr);
		bb_unsetenv(*curr);
		free(*curr);
	}
	free(envp);
}
