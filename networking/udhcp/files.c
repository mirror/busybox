/* vi: set sw=4 ts=4: */
/*
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include <netinet/ether.h>

#include "common.h"
#include "dhcpd.h"
#include "options.h"

#if BB_LITTLE_ENDIAN
static inline uint64_t hton64(uint64_t v)
{
        return (((uint64_t)htonl(v)) << 32) | htonl(v >> 32);
}
#else
#define hton64(v) (v)
#endif
#define ntoh64(v) hton64(v)


/* on these functions, make sure your datatype matches */
static int FAST_FUNC read_nip(const char *line, void *arg)
{
	len_and_sockaddr *lsa;

	lsa = host_and_af2sockaddr(line, 0, AF_INET);
	if (!lsa)
		return 0;
	*(uint32_t*)arg = lsa->u.sin.sin_addr.s_addr;
	free(lsa);
	return 1;
}


static int FAST_FUNC read_mac(const char *line, void *arg)
{
	return NULL == ether_aton_r(line, (struct ether_addr *)arg);
}


static int FAST_FUNC read_str(const char *line, void *arg)
{
	char **dest = arg;

	free(*dest);
	*dest = xstrdup(line);
	return 1;
}


static int FAST_FUNC read_u32(const char *line, void *arg)
{
	*(uint32_t*)arg = bb_strtou32(line, NULL, 10);
	return errno == 0;
}


static int FAST_FUNC read_yn(const char *line, void *arg)
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
struct option_set* FAST_FUNC find_option(struct option_set *opt_list, uint8_t code)
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
		log2("Attaching option %02x to list", option->code);

#if ENABLE_FEATURE_UDHCP_RFC3397
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
#if ENABLE_FEATURE_UDHCP_RFC3397
		if ((option->flags & TYPE_MASK) == OPTION_STR1035 && buffer != NULL)
			free(buffer);
#endif
		return;
	}

	/* add it to an existing option */
	log1("Attaching option %02x to existing member of list", option->code);
	if (option->flags & OPTION_LIST) {
#if ENABLE_FEATURE_UDHCP_RFC3397
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
#if ENABLE_FEATURE_UDHCP_RFC3397
		if ((option->flags & TYPE_MASK) == OPTION_STR1035 && buffer != NULL)
			free(buffer);
#endif
	} /* else, ignore the new data */
}


/* read a dhcp option and add it to opt_list */
static int FAST_FUNC read_opt(const char *const_line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	char *line;
	const struct dhcp_option *option;
	int retval, length, idx;
	char buffer[8] ALIGNED(4);
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
			retval = read_nip(val, buffer);
			break;
		case OPTION_IP_PAIR:
			retval = read_nip(val, buffer);
			val = strtok(NULL, ", \t/-");
			if (!val)
				retval = 0;
			if (retval)
				retval = read_nip(val, buffer + 4);
			break;
		case OPTION_STRING:
#if ENABLE_FEATURE_UDHCP_RFC3397
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

static int FAST_FUNC read_staticlease(const char *const_line, void *arg)
{
	char *line;
	char *mac_string;
	char *ip_string;
	struct ether_addr mac_bytes;
	uint32_t ip;

	/* Read mac */
	line = (char *) const_line;
	mac_string = strtok_r(line, " \t", &line);
	read_mac(mac_string, &mac_bytes);

	/* Read ip */
	ip_string = strtok_r(NULL, " \t", &line);
	read_nip(ip_string, &ip);

	add_static_lease(arg, (uint8_t*) &mac_bytes, ip);

	log_static_leases(arg);

	return 1;
}


struct config_keyword {
	const char *keyword;
	int (*handler)(const char *line, void *var) FAST_FUNC;
	void *var;
	const char *def;
};

static const struct config_keyword keywords[] = {
	/* keyword       handler   variable address               default */
	{"start",        read_nip, &(server_config.start_ip),     "192.168.0.20"},
	{"end",          read_nip, &(server_config.end_ip),       "192.168.0.254"},
	{"interface",    read_str, &(server_config.interface),    "eth0"},
	/* Avoid "max_leases value not sane" warning by setting default
	 * to default_end_ip - default_start_ip + 1: */
	{"max_leases",   read_u32, &(server_config.max_leases),   "235"},
	{"auto_time",    read_u32, &(server_config.auto_time),    "7200"},
	{"decline_time", read_u32, &(server_config.decline_time), "3600"},
	{"conflict_time",read_u32, &(server_config.conflict_time),"3600"},
	{"offer_time",   read_u32, &(server_config.offer_time),   "60"},
	{"min_lease",    read_u32, &(server_config.min_lease_sec),"60"},
	{"lease_file",   read_str, &(server_config.lease_file),   LEASES_FILE},
	{"pidfile",      read_str, &(server_config.pidfile),      "/var/run/udhcpd.pid"},
	{"siaddr",       read_nip, &(server_config.siaddr_nip),   "0.0.0.0"},
	/* keywords with no defaults must be last! */
	{"option",       read_opt, &(server_config.options),      ""},
	{"opt",          read_opt, &(server_config.options),      ""},
	{"notify_file",  read_str, &(server_config.notify_file),  ""},
	{"sname",        read_str, &(server_config.sname),        ""},
	{"boot_file",    read_str, &(server_config.boot_file),    ""},
	{"static_lease", read_staticlease, &(server_config.static_leases), ""},
};
enum { KWS_WITH_DEFAULTS = ARRAY_SIZE(keywords) - 6 };

void FAST_FUNC read_config(const char *file)
{
	parser_t *parser;
	const struct config_keyword *k;
	unsigned i;
	char *token[2];

	for (i = 0; i < KWS_WITH_DEFAULTS; i++)
		keywords[i].handler(keywords[i].def, keywords[i].var);

	parser = config_open(file);
	while (config_read(parser, token, 2, 2, "# \t", PARSE_NORMAL)) {
		for (k = keywords, i = 0; i < ARRAY_SIZE(keywords); k++, i++) {
			if (!strcasecmp(token[0], k->keyword)) {
				if (!k->handler(token[1], k->var)) {
					bb_error_msg("can't parse line %u in %s",
							parser->lineno, file);
					/* reset back to the default value */
					k->handler(k->def, k->var);
				}
				break;
			}
		}
	}
	config_close(parser);

	server_config.start_ip = ntohl(server_config.start_ip);
	server_config.end_ip = ntohl(server_config.end_ip);
}


void FAST_FUNC write_leases(void)
{
	int fd;
	unsigned i;
	leasetime_t curr;
	int64_t written_at;

	fd = open_or_warn(server_config.lease_file, O_WRONLY|O_CREAT|O_TRUNC);
	if (fd < 0)
		return;

	curr = written_at = time(NULL);

	written_at = hton64(written_at);
	full_write(fd, &written_at, sizeof(written_at));

	for (i = 0; i < server_config.max_leases; i++) {
		leasetime_t tmp_time;

		if (g_leases[i].lease_nip == 0)
			continue;

		/* Screw with the time in the struct, for easier writing */
		tmp_time = g_leases[i].expires;

		g_leases[i].expires -= curr;
		if ((signed_leasetime_t) g_leases[i].expires < 0)
			g_leases[i].expires = 0;
		g_leases[i].expires = htonl(g_leases[i].expires);

		/* No error check. If the file gets truncated,
		 * we lose some leases on restart. Oh well. */
		full_write(fd, &g_leases[i], sizeof(g_leases[i]));

		/* Then restore it when done */
		g_leases[i].expires = tmp_time;
	}
	close(fd);

	if (server_config.notify_file) {
// TODO: vfork-based child creation
		char *cmd = xasprintf("%s %s", server_config.notify_file, server_config.lease_file);
		system(cmd);
		free(cmd);
	}
}


void FAST_FUNC read_leases(const char *file)
{
	struct dyn_lease lease;
	int64_t written_at, time_passed;
	int fd;
#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
	unsigned i = 0;
#endif

	fd = open_or_warn(file, O_RDONLY);
	if (fd < 0)
		return;

	if (full_read(fd, &written_at, sizeof(written_at)) != sizeof(written_at))
		goto ret;
	written_at = ntoh64(written_at);

	time_passed = time(NULL) - written_at;
	/* Strange written_at, or lease file from old version of udhcpd
	 * which had no "written_at" field? */
	if ((uint64_t)time_passed > 12 * 60 * 60)
		goto ret;

	while (full_read(fd, &lease, sizeof(lease)) == sizeof(lease)) {
//FIXME: what if it matches some static lease?
		uint32_t y = ntohl(lease.lease_nip);
		if (y >= server_config.start_ip && y <= server_config.end_ip) {
			signed_leasetime_t expires = ntohl(lease.expires) - (signed_leasetime_t)time_passed;
			if (expires <= 0)
				continue;
			/* NB: add_lease takes "relative time", IOW,
			 * lease duration, not lease deadline. */
			if (add_lease(lease.lease_mac, lease.lease_nip,
					expires,
					lease.hostname, sizeof(lease.hostname)
				) == 0
			) {
				bb_error_msg("too many leases while loading %s", file);
				break;
			}
#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
			i++;
#endif
		}
	}
	log1("Read %d leases", i);
 ret:
	close(fd);
}
