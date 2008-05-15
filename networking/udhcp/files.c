/* vi: set sw=4 ts=4: */
/*
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include <netinet/ether.h>

#include "common.h"
#include "dhcpd.h"
#include "options.h"


/* on these functions, make sure your datatype matches */
static int read_ip(const char *line, void *arg)
{
	len_and_sockaddr *lsa;

	lsa = host_and_af2sockaddr(line, 0, AF_INET);
	if (!lsa)
		return 0;
	*(uint32_t*)arg = lsa->u.sin.sin_addr.s_addr;
	free(lsa);
	return 1;
}

static int read_mac(const char *line, void *arg)
{
	struct ether_addr *temp_ether_addr;

	temp_ether_addr = ether_aton_r(line, (struct ether_addr *)arg);
	if (temp_ether_addr == NULL)
		return 0;
	return 1;
}


static int read_str(const char *line, void *arg)
{
	char **dest = arg;

	free(*dest);
	*dest = xstrdup(line);
	return 1;
}


static int read_u32(const char *line, void *arg)
{
	*(uint32_t*)arg = bb_strtou32(line, NULL, 10);
	return errno == 0;
}


static int read_yn(const char *line, void *arg)
{
	char *dest = arg;

	if (!strcasecmp("yes", line)) {
		*dest = 1;
		return 1;
	}
	if (!strcasecmp("no", line)) {
		*dest = 0;
		return 1;
	}
	return 0;
}


/* find option 'code' in opt_list */
struct option_set *find_option(struct option_set *opt_list, uint8_t code)
{
	while (opt_list && opt_list->data[OPT_CODE] < code)
		opt_list = opt_list->next;

	if (opt_list && opt_list->data[OPT_CODE] == code)
		return opt_list;
	return NULL;
}


/* add an option to the opt_list */
static void attach_option(struct option_set **opt_list,
		const struct dhcp_option *option, char *buffer, int length)
{
	struct option_set *existing, *new, **curr;

	existing = find_option(*opt_list, option->code);
	if (!existing) {
		DEBUG("Attaching option %02x to list", option->code);

#if ENABLE_FEATURE_RFC3397
		if ((option->flags & TYPE_MASK) == OPTION_STR1035)
			/* reuse buffer and length for RFC1035-formatted string */
			buffer = (char *)dname_enc(NULL, 0, buffer, &length);
#endif

		/* make a new option */
		new = xmalloc(sizeof(*new));
		new->data = xmalloc(length + 2);
		new->data[OPT_CODE] = option->code;
		new->data[OPT_LEN] = length;
		memcpy(new->data + 2, buffer, length);

		curr = opt_list;
		while (*curr && (*curr)->data[OPT_CODE] < option->code)
			curr = &(*curr)->next;

		new->next = *curr;
		*curr = new;
#if ENABLE_FEATURE_RFC3397
		if ((option->flags & TYPE_MASK) == OPTION_STR1035 && buffer != NULL)
			free(buffer);
#endif
		return;
	}

	/* add it to an existing option */
	DEBUG("Attaching option %02x to existing member of list", option->code);
	if (option->flags & OPTION_LIST) {
#if ENABLE_FEATURE_RFC3397
		if ((option->flags & TYPE_MASK) == OPTION_STR1035)
			/* reuse buffer and length for RFC1035-formatted string */
			buffer = (char *)dname_enc(existing->data + 2,
					existing->data[OPT_LEN], buffer, &length);
#endif
		if (existing->data[OPT_LEN] + length <= 255) {
			existing->data = xrealloc(existing->data,
					existing->data[OPT_LEN] + length + 3);
			if ((option->flags & TYPE_MASK) == OPTION_STRING) {
				/* ' ' can bring us to 256 - bad */
				if (existing->data[OPT_LEN] + length >= 255)
					return;
				/* add space separator between STRING options in a list */
				existing->data[existing->data[OPT_LEN] + 2] = ' ';
				existing->data[OPT_LEN]++;
			}
			memcpy(existing->data + existing->data[OPT_LEN] + 2, buffer, length);
			existing->data[OPT_LEN] += length;
		} /* else, ignore the data, we could put this in a second option in the future */
#if ENABLE_FEATURE_RFC3397
		if ((option->flags & TYPE_MASK) == OPTION_STR1035 && buffer != NULL)
			free(buffer);
#endif
	} /* else, ignore the new data */
}


/* read a dhcp option and add it to opt_list */
static int read_opt(const char *const_line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	char *line;
	const struct dhcp_option *option;
	int retval, length, idx;
	char buffer[8] __attribute__((aligned(4)));
	uint16_t *result_u16 = (uint16_t *) buffer;
	uint32_t *result_u32 = (uint32_t *) buffer;

	/* Cheat, the only const line we'll actually get is "" */
	line = (char *) const_line;
	opt = strtok(line, " \t=");
	if (!opt)
		return 0;

	idx = index_in_strings(dhcp_option_strings, opt); /* NB: was strcasecmp! */
	if (idx < 0)
		return 0;
	option = &dhcp_options[idx];

	retval = 0;
	do {
		val = strtok(NULL, ", \t");
		if (!val) break;
		length = dhcp_option_lengths[option->flags & TYPE_MASK];
		retval = 0;
		opt = buffer; /* new meaning for variable opt */
		switch (option->flags & TYPE_MASK) {
		case OPTION_IP:
			retval = read_ip(val, buffer);
			break;
		case OPTION_IP_PAIR:
			retval = read_ip(val, buffer);
			val = strtok(NULL, ", \t/-");
			if (!val)
				retval = 0;
			if (retval)
				retval = read_ip(val, buffer + 4);
			break;
		case OPTION_STRING:
#if ENABLE_FEATURE_RFC3397
		case OPTION_STR1035:
#endif
			length = strlen(val);
			if (length > 0) {
				if (length > 254) length = 254;
				opt = val;
				retval = 1;
			}
			break;
		case OPTION_BOOLEAN:
			retval = read_yn(val, buffer);
			break;
		case OPTION_U8:
			buffer[0] = strtoul(val, &endptr, 0);
			retval = (endptr[0] == '\0');
			break;
		/* htonX are macros in older libc's, using temp var
		 * in code below for safety */
		/* TODO: use bb_strtoX? */
		case OPTION_U16: {
			unsigned long tmp = strtoul(val, &endptr, 0);
			*result_u16 = htons(tmp);
			retval = (endptr[0] == '\0' /*&& tmp < 0x10000*/);
			break;
		}
		case OPTION_S16: {
			long tmp = strtol(val, &endptr, 0);
			*result_u16 = htons(tmp);
			retval = (endptr[0] == '\0');
			break;
		}
		case OPTION_U32: {
			unsigned long tmp = strtoul(val, &endptr, 0);
			*result_u32 = htonl(tmp);
			retval = (endptr[0] == '\0');
			break;
		}
		case OPTION_S32: {
			long tmp = strtol(val, &endptr, 0);
			*result_u32 = htonl(tmp);
			retval = (endptr[0] == '\0');
			break;
		}
		default:
			break;
		}
		if (retval)
			attach_option(opt_list, option, opt, length);
	} while (retval && option->flags & OPTION_LIST);
	return retval;
}

static int read_staticlease(const char *const_line, void *arg)
{
	char *line;
	char *mac_string;
	char *ip_string;
	uint8_t *mac_bytes;
	uint32_t *ip;

	/* Allocate memory for addresses */
	mac_bytes = xmalloc(sizeof(unsigned char) * 8);
	ip = xmalloc(sizeof(uint32_t));

	/* Read mac */
	line = (char *) const_line;
	mac_string = strtok(line, " \t");
	read_mac(mac_string, mac_bytes);

	/* Read ip */
	ip_string = strtok(NULL, " \t");
	read_ip(ip_string, ip);

	addStaticLease(arg, mac_bytes, ip);

	if (ENABLE_FEATURE_UDHCP_DEBUG) printStaticLeases(arg);

	return 1;
}


struct config_keyword {
	const char *keyword;
	int (*handler)(const char *line, void *var);
	void *var;
	const char *def;
};

static const struct config_keyword keywords[] = {
	/* keyword       handler   variable address               default */
	{"start",        read_ip,  &(server_config.start_ip),     "192.168.0.20"},
	{"end",          read_ip,  &(server_config.end_ip),       "192.168.0.254"},
	{"interface",    read_str, &(server_config.interface),    "eth0"},
	/* Avoid "max_leases value not sane" warning by setting default
	 * to default_end_ip - default_start_ip + 1: */
	{"max_leases",   read_u32, &(server_config.max_leases),   "235"},
	{"remaining",    read_yn,  &(server_config.remaining),    "yes"},
	{"auto_time",    read_u32, &(server_config.auto_time),    "7200"},
	{"decline_time", read_u32, &(server_config.decline_time), "3600"},
	{"conflict_time",read_u32, &(server_config.conflict_time),"3600"},
	{"offer_time",   read_u32, &(server_config.offer_time),   "60"},
	{"min_lease",    read_u32, &(server_config.min_lease),    "60"},
	{"lease_file",   read_str, &(server_config.lease_file),   LEASES_FILE},
	{"pidfile",      read_str, &(server_config.pidfile),      "/var/run/udhcpd.pid"},
	{"siaddr",       read_ip,  &(server_config.siaddr),       "0.0.0.0"},
	/* keywords with no defaults must be last! */
	{"option",       read_opt, &(server_config.options),      ""},
	{"opt",          read_opt, &(server_config.options),      ""},
	{"notify_file",  read_str, &(server_config.notify_file),  ""},
	{"sname",        read_str, &(server_config.sname),        ""},
	{"boot_file",    read_str, &(server_config.boot_file),    ""},
	{"static_lease", read_staticlease, &(server_config.static_leases), ""},
	/* ADDME: static lease */
};
enum { KWS_WITH_DEFAULTS = ARRAY_SIZE(keywords) - 6 };


/*
 * Domain names may have 254 chars, and string options can be 254
 * chars long. However, 80 bytes will be enough for most, and won't
 * hog up memory. If you have a special application, change it
 */
#define READ_CONFIG_BUF_SIZE 80

void read_config(const char *file)
{
	FILE *in;
	char buffer[READ_CONFIG_BUF_SIZE], *token, *line;
	unsigned i, lineno;

	for (i = 0; i < KWS_WITH_DEFAULTS; i++)
		keywords[i].handler(keywords[i].def, keywords[i].var);

	in = fopen_or_warn(file, "r");
	if (!in)
		return;

	lineno = 0;
	while (fgets(buffer, READ_CONFIG_BUF_SIZE, in)) {
		lineno++;
		/* *strchrnul(buffer, '\n') = '\0'; - trim() will do it */
		*strchrnul(buffer, '#') = '\0';

		token = strtok(buffer, " \t");
		if (!token) continue;
		line = strtok(NULL, "");
		if (!line) continue;

		trim(line); /* remove leading/trailing whitespace */

		for (i = 0; i < ARRAY_SIZE(keywords); i++) {
			if (!strcasecmp(token, keywords[i].keyword)) {
				if (!keywords[i].handler(line, keywords[i].var)) {
					bb_error_msg("can't parse line %u in %s at '%s'",
							lineno, file, line);
					/* reset back to the default value */
					keywords[i].handler(keywords[i].def, keywords[i].var);
				}
				break;
			}
		}
	}
	fclose(in);

	server_config.start_ip = ntohl(server_config.start_ip);
	server_config.end_ip = ntohl(server_config.end_ip);
}


void write_leases(void)
{
	int fp;
	unsigned i;
	time_t curr = time(0);
	unsigned long tmp_time;

	fp = open_or_warn(server_config.lease_file, O_WRONLY|O_CREAT|O_TRUNC);
	if (fp < 0) {
		return;
	}

	for (i = 0; i < server_config.max_leases; i++) {
		if (leases[i].yiaddr != 0) {

			/* screw with the time in the struct, for easier writing */
			tmp_time = leases[i].expires;

			if (server_config.remaining) {
				if (lease_expired(&(leases[i])))
					leases[i].expires = 0;
				else leases[i].expires -= curr;
			} /* else stick with the time we got */
			leases[i].expires = htonl(leases[i].expires);
			// FIXME: error check??
			full_write(fp, &leases[i], sizeof(leases[i]));

			/* then restore it when done */
			leases[i].expires = tmp_time;
		}
	}
	close(fp);

	if (server_config.notify_file) {
// TODO: vfork-based child creation
		char *cmd = xasprintf("%s %s", server_config.notify_file, server_config.lease_file);
		system(cmd);
		free(cmd);
	}
}


void read_leases(const char *file)
{
	int fp;
	unsigned i;
	struct dhcpOfferedAddr lease;

	fp = open_or_warn(file, O_RDONLY);
	if (fp < 0) {
		return;
	}

	i = 0;
	while (i < server_config.max_leases
	 && full_read(fp, &lease, sizeof(lease)) == sizeof(lease)
	) {
		/* ADDME: is it a static lease */
		uint32_t y = ntohl(lease.yiaddr);
		if (y >= server_config.start_ip && y <= server_config.end_ip) {
			lease.expires = ntohl(lease.expires);
			if (!server_config.remaining)
				lease.expires -= time(NULL);
			if (!(add_lease(lease.chaddr, lease.yiaddr, lease.expires))) {
				bb_error_msg("too many leases while loading %s", file);
				break;
			}
			i++;
		}
	}
	DEBUG("Read %d leases", i);
	close(fp);
}
