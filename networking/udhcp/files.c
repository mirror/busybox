/* 
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */
 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "common.h"

/* on these functions, make sure you datatype matches */
static int read_ip(const char *line, void *arg)
{
	struct in_addr *addr = arg;
	struct hostent *host;
	int retval = 1;

	if (!inet_aton(line, addr)) {
		if ((host = gethostbyname(line))) 
			addr->s_addr = *((unsigned long *) host->h_addr_list[0]);
		else retval = 0;
	}
	return retval;
}


static int read_str(const char *line, void *arg)
{
	char **dest = arg;
	
	if (*dest) free(*dest);
	*dest = strdup(line);
	
	return 1;
}


static int read_u32(const char *line, void *arg)
{
	u_int32_t *dest = arg;
	char *endptr;
	*dest = strtoul(line, &endptr, 0);
	return endptr[0] == '\0';
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

#define READ_CONFIG_BUF_SIZE 512        /* domainname may have 254 chars */

/* read a dhcp option and add it to opt_list */
static int read_opt(const char *const_line, void *arg)
{
    char line[READ_CONFIG_BUF_SIZE];
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
    struct dhcp_option *option;
    int retval = 0, length;
    char buffer[256];                       /* max opt length */
	u_int16_t result_u16;
	u_int32_t result_u32;
    void *valptr;
	
    if ((opt = strtok(strcpy(line, const_line), " \t="))) {
		
	for (option = options; option->code; option++)
		if (!strcasecmp(option->name, opt))
			break;
	
	if (option->code) do {
		val = strtok(NULL, ", \t");
		if(!val)
			break;
			length = option_lengths[option->flags & TYPE_MASK];
		valptr = NULL;
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
				endptr = buffer + length;
				endptr[0] = 0;
				valptr = val;
				}
				break;
			case OPTION_BOOLEAN:
				retval = read_yn(val, buffer);
				break;
			case OPTION_U8:
				buffer[0] = strtoul(val, &endptr, 0);
			valptr = buffer;
				break;
			case OPTION_U16:
				result_u16 = htons(strtoul(val, &endptr, 0));
			valptr = &result_u16;
				break;
			case OPTION_S16:
				result_u16 = htons(strtol(val, &endptr, 0));
			valptr = &result_u16;
				break;
			case OPTION_U32:
				result_u32 = htonl(strtoul(val, &endptr, 0));
			valptr = &result_u32;
				break;
			case OPTION_S32:
				result_u32 = htonl(strtol(val, &endptr, 0));	
			valptr = &result_u32;
				break;
			default:
			retval = 0;
				break;
			}
		if(valptr) {
			memcpy(buffer, valptr, length);
			retval = (endptr[0] == '\0');
		}
			if (retval) 
				attach_option(opt_list, option, buffer, length);
		else
			break;
	} while (option->flags & OPTION_LIST);
    }
	return retval;
}


static const struct config_keyword keywords[] = {
	/* keyword      handler   variable address              default     */
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
	{"lease_file",  read_str, &(server_config.lease_file),  leases_file},
	{"pidfile",	read_str, &(server_config.pidfile),	"/var/run/udhcpd.pid"},
	{"notify_file", read_str, &(server_config.notify_file),	""},
	{"siaddr",	read_ip,  &(server_config.siaddr),	"0.0.0.0"},
	{"sname",	read_str, &(server_config.sname),	""},
	{"boot_file",	read_str, &(server_config.boot_file),	""},
	/*ADDME: static lease */
	{"",		NULL, 	  NULL,				""}
};


int read_config(const char *file)
{
	FILE *in;
	char buffer[READ_CONFIG_BUF_SIZE], orig[READ_CONFIG_BUF_SIZE];
	char *token, *line;
	int i;

	for (i = 0; keywords[i].keyword[0]; i++)
		if (keywords[i].def[0])
			keywords[i].handler(keywords[i].def, keywords[i].var);

	if (!(in = fopen(file, "r"))) {
		LOG(LOG_ERR, "unable to open config file: %s", file);
		return 0;
	}
	
	while (fgets(buffer, READ_CONFIG_BUF_SIZE, in)) {
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
		strcpy(orig, buffer);
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';
		token = strtok(buffer, " \t");
		if(!token)
			continue;
		line = strtok(NULL, "");
		if(!line)
			continue;
		while(*line == '=' || isspace(*line))
		line++;
		/* eat trailing whitespace */
		for (i = strlen(line); i > 0 && isspace(line[i - 1]); i--);
		line[i] = '\0';
		if (*line == '\0')
			continue;
		
		for (i = 0; keywords[i].keyword[0]; i++)
			if (!strcasecmp(token, keywords[i].keyword))
				if (!keywords[i].handler(line, keywords[i].var)) {
					LOG(LOG_ERR, "unable to parse '%s'", orig);
					/* reset back to the default value */
					keywords[i].handler(keywords[i].def, keywords[i].var);
				}
	}
	fclose(in);
	return 1;
}


void write_leases(void)
{
	FILE *fp;
	unsigned int i;
	char buf[255];
	time_t curr = time(0);
	unsigned long lease_time;
	
	if (!(fp = fopen(server_config.lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config.lease_file);
		return;
	}
	
	for (i = 0; i < server_config.max_leases; i++) {
		struct dhcpOfferedAddr lease;
		if (leases[i].yiaddr != 0) {
			if (server_config.remaining) {
				if (lease_expired(&(leases[i])))
					lease_time = 0;
				else lease_time = leases[i].expires - curr;
			} else lease_time = leases[i].expires;
			lease.expires = htonl(lease_time);
			memcpy(lease.chaddr, leases[i].chaddr, 16);
			lease.yiaddr = leases[i].yiaddr;
			fwrite(leases, sizeof(lease), 1, fp);
		}
	}
	fclose(fp);
	
	if (server_config.notify_file) {
		sprintf(buf, "%s %s", server_config.notify_file, server_config.lease_file);
		system(buf);
	}
}


void read_leases(const char *file)
{
	FILE *fp;
	unsigned int i = 0;
	struct dhcpOfferedAddr lease;
	
	if (!(fp = fopen(file, "r"))) {
		LOG(LOG_ERR, "Unable to open %s for reading", file);
		return;
	}
	
	while (i < server_config.max_leases && (fread(&lease, sizeof lease, 1, fp) == 1)) {
		/* ADDME: is it a static lease */
		if (lease.yiaddr >= server_config.start && lease.yiaddr <= server_config.end) {
			lease.expires = ntohl(lease.expires);
			if (!server_config.remaining) lease.expires -= time(0);
			if (!(add_lease(lease.chaddr, lease.yiaddr, lease.expires))) {
				LOG(LOG_WARNING, "Too many leases while loading %s\n", file);
				break;
			}				
			i++;
		}
	}
	DEBUG(LOG_INFO, "Read %d leases", i);
	fclose(fp);
}
