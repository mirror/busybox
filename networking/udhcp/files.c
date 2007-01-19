/* vi: set sw=4 ts=4: */
/*
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */

#include <netinet/ether.h>

#include "common.h"
#include "dhcpd.h"
#include "options.h"


/*
 * Domain names may have 254 chars, and string options can be 254
 * chars long. However, 80 bytes will be enough for most, and won't
 * hog up memory. If you have a special application, change it
 */
#define READ_CONFIG_BUF_SIZE 80

/* on these functions, make sure you datatype matches */
static int read_ip(const char *line, void *arg)
{
	struct in_addr *addr = arg;
	struct hostent *host;
	int retval = 1;

	if (!inet_aton(line, addr)) {
		host = gethostbyname(line);
		if (host)
			addr->s_addr = *((unsigned long *) host->h_addr_list[0]);
		else retval = 0;
	}
	return retval;
}

static int read_mac(const char *line, void *arg)
{
	uint8_t *mac_bytes = arg;
	struct ether_addr *temp_ether_addr;
	int retval = 1;

	temp_ether_addr = ether_aton(line);

	if (temp_ether_addr == NULL)
		retval = 0;
	else
		memcpy(mac_bytes, temp_ether_addr, 6);

	return retval;
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
	*((uint32_t*)arg) = bb_strtou32(line, NULL, 10);
	return errno == 0;
}


static int read_yn(const char *line, void *arg)
{
	char *dest = arg;
	int retval = 1;

	if (!strcasecmp("yes", line))
		*dest = 1;
	else if (!strcasecmp("no", line))
		*dest = 0;
	else retval = 0;

	return retval;
}


/* find option 'code' in opt_list */
struct option_set *find_option(struct option_set *opt_list, char code)
{
	while (opt_list && opt_list->data[OPT_CODE] < code)
		opt_list = opt_list->next;

	if (opt_list && opt_list->data[OPT_CODE] == code) return opt_list;
	else return NULL;
}


/* add an option to the opt_list */
static void attach_option(struct option_set **opt_list,
		const struct dhcp_option *option, char *buffer, int length)
{
	struct option_set *existing, *new, **curr;

	/* add it to an existing option */
	existing = find_option(*opt_list, option->code);
	if (existing) {
		DEBUG("Attaching option %s to existing member of list", option->name);
		if (option->flags & OPTION_LIST) {
			if (existing->data[OPT_LEN] + length <= 255) {
				existing->data = realloc(existing->data,
						existing->data[OPT_LEN] + length + 2);
				memcpy(existing->data + existing->data[OPT_LEN] + 2, buffer, length);
				existing->data[OPT_LEN] += length;
			} /* else, ignore the data, we could put this in a second option in the future */
		} /* else, ignore the new data */
	} else {
		DEBUG("Attaching option %s to list", option->name);

		/* make a new option */
		new = xmalloc(sizeof(struct option_set));
		new->data = xmalloc(length + 2);
		new->data[OPT_CODE] = option->code;
		new->data[OPT_LEN] = length;
		memcpy(new->data + 2, buffer, length);

		curr = opt_list;
		while (*curr && (*curr)->data[OPT_CODE] < option->code)
			curr = &(*curr)->next;

		new->next = *curr;
		*curr = new;
	}
}


/* read a dhcp option and add it to opt_list */
static int read_opt(const char *const_line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	const struct dhcp_option *option;
	int retval = 0, length;
	char buffer[8];
	char *line;
	uint16_t *result_u16 = (uint16_t *) buffer;
	uint32_t *result_u32 = (uint32_t *) buffer;

	/* Cheat, the only const line we'll actually get is "" */
	line = (char *) const_line;
	opt = strtok(line, " \t=");
	if (!opt) return 0;

	option = dhcp_options;
	while (1) {
		if (!option->code)
			return 0;
		if (!strcasecmp(option->name, opt))
			break;
		 option++;
	}

	do {
		val = strtok(NULL, ", \t");
		if (!val) break;
		length = option_lengths[option->flags & TYPE_MASK];
		retval = 0;
		opt = buffer; /* new meaning for variable opt */
		switch (option->flags & TYPE_MASK) {
		case OPTION_IP:
			retval = read_ip(val, buffer);
			break;
		case OPTION_IP_PAIR:
			retval = read_ip(val, buffer);
			if (!(val = strtok(NULL, ", \t/-"))) retval = 0;
			if (retval) retval = read_ip(val, buffer + 4);
			break;
		case OPTION_STRING:
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
		case OPTION_U16:
			*result_u16 = htons(strtoul(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_S16:
			*result_u16 = htons(strtol(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_U32:
			*result_u32 = htonl(strtoul(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
		case OPTION_S32:
			*result_u32 = htonl(strtol(val, &endptr, 0));
			retval = (endptr[0] == '\0');
			break;
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


static const struct config_keyword keywords[] = {
	/* keyword	handler   variable address		default */
	{"start",	read_ip,  &(server_config.start),	"192.168.0.20"},
	{"end",		read_ip,  &(server_config.end),		"192.168.0.254"},
	{"interface",	read_str, &(server_config.interface),	"eth0"},
	{"option",	read_opt, &(server_config.options),	""},
	{"opt",		read_opt, &(server_config.options),	""},
	{"max_leases",	read_u32, &(server_config.max_leases),	"254"},
	{"remaining",	read_yn,  &(server_config.remaining),	"yes"},
	{"auto_time",	read_u32, &(server_config.auto_time),	"7200"},
	{"decline_time",read_u32, &(server_config.decline_time),"3600"},
	{"conflict_time",read_u32,&(server_config.conflict_time),"3600"},
	{"offer_time",	read_u32, &(server_config.offer_time),	"60"},
	{"min_lease",	read_u32, &(server_config.min_lease),	"60"},
	{"lease_file",	read_str, &(server_config.lease_file),	LEASES_FILE},
	{"pidfile",	read_str, &(server_config.pidfile),	"/var/run/udhcpd.pid"},
	{"notify_file", read_str, &(server_config.notify_file),	""},
	{"siaddr",	read_ip,  &(server_config.siaddr),	"0.0.0.0"},
	{"sname",	read_str, &(server_config.sname),	""},
	{"boot_file",	read_str, &(server_config.boot_file),	""},
	{"static_lease",read_staticlease, &(server_config.static_leases),	""},
	/*ADDME: static lease */
	{"",		NULL,	  NULL,				""}
};


int read_config(const char *file)
{
	FILE *in;
	char buffer[READ_CONFIG_BUF_SIZE], *token, *line;
	int i, lm = 0;

	for (i = 0; keywords[i].keyword[0]; i++)
		if (keywords[i].def[0])
			keywords[i].handler(keywords[i].def, keywords[i].var);

	in = fopen(file, "r");
	if (!in) {
		bb_error_msg("cannot open config file: %s", file);
		return 0;
	}

	while (fgets(buffer, READ_CONFIG_BUF_SIZE, in)) {
		char debug_orig[READ_CONFIG_BUF_SIZE];
		char *p;

		lm++;
		p = strchr(buffer, '\n');
		if (p) *p = '\0';
		if (ENABLE_FEATURE_UDHCP_DEBUG) strcpy(debug_orig, buffer);
		p = strchr(buffer, '#');
		if (p) *p = '\0';

		if (!(token = strtok(buffer, " \t"))) continue;
		if (!(line = strtok(NULL, ""))) continue;

		/* eat leading whitespace */
		line = skip_whitespace(line);
		/* eat trailing whitespace */
		i = strlen(line) - 1;
		while (i >= 0 && isspace(line[i]))
			line[i--] = '\0';

		for (i = 0; keywords[i].keyword[0]; i++)
			if (!strcasecmp(token, keywords[i].keyword))
				if (!keywords[i].handler(line, keywords[i].var)) {
					bb_error_msg("cannot parse line %d of %s", lm, file);
					if (ENABLE_FEATURE_UDHCP_DEBUG)
						bb_error_msg("cannot parse '%s'", debug_orig);
					/* reset back to the default value */
					keywords[i].handler(keywords[i].def, keywords[i].var);
				}
	}
	fclose(in);
	return 1;
}


void write_leases(void)
{
	int fp;
	unsigned i;
	time_t curr = time(0);
	unsigned long tmp_time;

	fp = open(server_config.lease_file, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fp < 0) {
		bb_error_msg("cannot open %s for writing", server_config.lease_file);
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
		char *cmd = xasprintf("%s %s", server_config.notify_file, server_config.lease_file);
		system(cmd);
		free(cmd);
	}
}


void read_leases(const char *file)
{
	int fp;
	unsigned int i = 0;
	struct dhcpOfferedAddr lease;

	fp = open(file, O_RDONLY);
	if (fp < 0) {
		bb_error_msg("cannot open %s for reading", file);
		return;
	}

	while (i < server_config.max_leases
	 && full_read(fp, &lease, sizeof(lease)) == sizeof(lease)
	) {
		/* ADDME: is it a static lease */
		if (lease.yiaddr >= server_config.start && lease.yiaddr <= server_config.end) {
			lease.expires = ntohl(lease.expires);
			if (!server_config.remaining) lease.expires -= time(0);
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
