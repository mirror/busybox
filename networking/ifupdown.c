/* vi: set sw=4 ts=4: */
/*
 *  ifupdown for busybox
 *  Copyright (c) 2002 Glenn McGrath <bug1@optushome.com.au>
 *  Copyright (c) 2003 Erik Andersen <andersen@codepoet.org>
 *
 *  Based on ifupdown v 0.6.4 by Anthony Towns
 *  Copyright (c) 1999 Anthony Towns <aj@azure.humbug.org.au>
 *
 *  Changes to upstream version
 *  Remove checks for kernel version, assume kernel version 2.2.0 or better.
 *  Lines in the interfaces file cannot wrap.
 *  To adhere to the FHS, the default state file is /var/run/ifstate.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libbb.h"

#define MAX_OPT_DEPTH 10
#define EUNBALBRACK 10001
#define EUNDEFVAR   10002
#define EUNBALPER   10000

#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
#define MAX_INTERFACE_LENGTH 10
#endif

#if 0
#define debug_noise(fmt, args...) printf(fmt, ## args)
#else
#define debug_noise(fmt, args...) 
#endif

/* Forward declaration */
struct interface_defn_t;

typedef int (execfn)(char *command);
typedef int (command_set)(struct interface_defn_t *ifd, execfn *e);

extern llist_t *llist_add_to_end(llist_t *list_head, char *data)
{
	llist_t *new_item, *tmp, *prev;

	new_item = xmalloc(sizeof(llist_t));
	new_item->data = data;
	new_item->link = NULL;
	
	prev = NULL;
	tmp = list_head;
	while(tmp) {
		prev = tmp;
		tmp = tmp->link;
	}
	if (prev) {
		prev->link = new_item; 
	} else {
		list_head = new_item;
	}

	return(list_head);
}

struct method_t
{
	char *name;
	command_set *up;
	command_set *down;
};

struct address_family_t
{
	char *name;
	int n_methods;
	struct method_t *method;
};

struct mapping_defn_t
{
	struct mapping_defn_t *next;

	int max_matches;
	int n_matches;
	char **match;

	char *script;

	int max_mappings;
	int n_mappings;
	char **mapping;
};

struct variable_t
{
	char *name;
	char *value;
};

struct interface_defn_t 
{
	struct interface_defn_t *prev;
	struct interface_defn_t *next;

	char *iface;
	struct address_family_t *address_family;
	struct method_t *method;

	int automatic;

	int max_options;
	int n_options;
	struct variable_t *option;
};

struct interfaces_file_t
{
	llist_t *autointerfaces;
	llist_t *ifaces;
	struct mapping_defn_t *mappings;
};

static char no_act = 0;
static char verbose = 0;
static char **environ = NULL;

#ifdef CONFIG_FEATURE_IFUPDOWN_IP

static unsigned int count_bits(unsigned int a)
{
	unsigned int result;
	result = (a & 0x55) + ((a >> 1) & 0x55);
	result = (result & 0x33) + ((result >> 2) & 0x33);
	return((result & 0x0F) + ((result >> 4) & 0x0F));
}

static int count_netmask_bits(char *dotted_quad)
{
	unsigned int result, a, b, c, d;
	/* Found a netmask...  Check if it is dotted quad */
	if (sscanf(dotted_quad, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
		return -1;
	result = count_bits(a);
	result += count_bits(b);
	result += count_bits(c);
	result += count_bits(d);
	return ((int)result);
}
#endif

static void addstr(char **buf, size_t *len, size_t *pos, char *str, size_t str_length)
{
	if (*pos + str_length >= *len) {
		char *newbuf;

		newbuf = xrealloc(*buf, *len * 2 + str_length + 1);
		*buf = newbuf;
		*len = *len * 2 + str_length + 1;
	}

	while (str_length-- >= 1) {
		(*buf)[(*pos)++] = *str;
		str++;
	}
	(*buf)[*pos] = '\0';
}

static int strncmpz(char *l, char *r, size_t llen)
{
	int i = strncmp(l, r, llen);

	if (i == 0) {
		return(-r[llen]);
	} else {
		return(i);
	}
}

static char *get_var(char *id, size_t idlen, struct interface_defn_t *ifd)
{
	int i;

	if (strncmpz(id, "iface", idlen) == 0) {
		char *result;
		static char label_buf[20];
		strncpy(label_buf, ifd->iface, 19);
		label_buf[19]=0;
		result = strchr(label_buf, ':');
		if (result) {
			*result=0;
		}
		return( label_buf);
	} else if (strncmpz(id, "label", idlen) == 0) {
		return (ifd->iface);
	} else {
		for (i = 0; i < ifd->n_options; i++) {
			if (strncmpz(id, ifd->option[i].name, idlen) == 0) {
				return (ifd->option[i].value);
			}
		}
	}

	return(NULL);
}

static char *parse(char *command, struct interface_defn_t *ifd)
{

	char *result = NULL;
	size_t pos = 0, len = 0;
	size_t old_pos[MAX_OPT_DEPTH] = { 0 };
	int okay[MAX_OPT_DEPTH] = { 1 };
	int opt_depth = 1;

	while (*command) {
		switch (*command) {

			default:
				addstr(&result, &len, &pos, command, 1);
				command++;
				break;
			case '\\':
				if (command[1]) {
					addstr(&result, &len, &pos, command + 1, 1);
					command += 2;
				} else {
					addstr(&result, &len, &pos, command, 1);
					command++;
				}
				break;
			case '[':
				if (command[1] == '[' && opt_depth < MAX_OPT_DEPTH) {
					old_pos[opt_depth] = pos;
					okay[opt_depth] = 1;
					opt_depth++;
					command += 2;
				} else {
					addstr(&result, &len, &pos, "[", 1);
					command++;
				}
				break;
			case ']':
				if (command[1] == ']' && opt_depth > 1) {
					opt_depth--;
					if (!okay[opt_depth]) {
						pos = old_pos[opt_depth];
						result[pos] = '\0';
					}
					command += 2;
				} else {
					addstr(&result, &len, &pos, "]", 1);
					command++;
				}
				break;
			case '%':
				{
					char *nextpercent;
					char *varvalue;

					command++;
					nextpercent = strchr(command, '%');
					if (!nextpercent) {
						errno = EUNBALPER;
						free(result);
						return (NULL);
					}

					varvalue = get_var(command, nextpercent - command, ifd);

					if (varvalue) {
						addstr(&result, &len, &pos, varvalue, bb_strlen(varvalue));
					} else {
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
						/* Sigh...  Add a special case for 'ip' to convert from
						 * dotted quad to bit count style netmasks.  */
						if (strncmp(command, "bnmask", 6)==0) {
							int res;
							varvalue = get_var("netmask", 7, ifd);
							if (varvalue && (res=count_netmask_bits(varvalue)) > 0) {
								char argument[255];
								sprintf(argument, "%d", res);
								addstr(&result, &len, &pos, argument, bb_strlen(argument));
								command = nextpercent + 1;
								break;
							}
						}
#endif
						okay[opt_depth - 1] = 0;
					}

					command = nextpercent + 1;
				}
				break;
		}
	}

	if (opt_depth > 1) {
		errno = EUNBALBRACK;
		free(result);
		return(NULL);
	}

	if (!okay[0]) {
		errno = EUNDEFVAR;
		free(result);
		return(NULL);
	}

	return(result);
}

static int execute(char *command, struct interface_defn_t *ifd, execfn *exec)
{
	char *out;
	int ret;

	out = parse(command, ifd);
	if (!out) {
		return(0);
	}
	ret = (*exec) (out);

	free(out);
	return(1);
}

#ifdef CONFIG_FEATURE_IFUPDOWN_IPX
static int static_up_ipx(struct interface_defn_t *ifd, execfn *exec)
{
	return(execute("ipx_interface add %iface% %frame% %netnum%", ifd, exec));
}

static int static_down_ipx(struct interface_defn_t *ifd, execfn *exec)
{
	return(execute("ipx_interface del %iface% %frame%", ifd, exec));
}

static int dynamic_up(struct interface_defn_t *ifd, execfn *exec)
{
	return(execute("ipx_interface add %iface% %frame%", ifd, exec));
}

static int dynamic_down(struct interface_defn_t *ifd, execfn *exec)
{
	return(execute("ipx_interface del %iface% %frame%", ifd, exec));
}

static struct method_t methods_ipx[] = {
	{ "dynamic", dynamic_up, dynamic_down, },
	{ "static", static_up_ipx, static_down_ipx, },
};

struct address_family_t addr_ipx = {
	"ipx",
	sizeof(methods_ipx) / sizeof(struct method_t),
	methods_ipx
};
#endif /* IFUP_FEATURE_IPX */

#ifdef CONFIG_FEATURE_IFUPDOWN_IPV6
static int loopback_up6(struct interface_defn_t *ifd, execfn *exec)
{
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	int result;
	result =execute("ip addr add ::1 dev %iface%", ifd, exec);
	result += execute("ip link set %iface% up", ifd, exec);
	return( result);
#else
	return( execute("ifconfig %iface% add ::1", ifd, exec));
#endif
}

static int loopback_down6(struct interface_defn_t *ifd, execfn *exec)
{
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	return(execute("ip link set %iface% down", ifd, exec));
#else
	return(execute("ifconfig %iface% del ::1", ifd, exec));
#endif
}

static int static_up6(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	result = execute("ip addr add %address%/%netmask% dev %iface% [[label %label%]]", ifd, exec);
	result += execute("ip link set [[mtu %mtu%]] [[address %hwaddress%]] %iface% up", ifd, exec);
	result += execute("[[ ip route add ::/0 via %gateway% ]]", ifd, exec);
#else
	result = execute("ifconfig %iface% [[media %media%]] [[hw %hwaddress%]] [[mtu %mtu%]] up", ifd, exec);
	result += execute("ifconfig %iface% add %address%/%netmask%", ifd, exec);
	result += execute("[[ route -A inet6 add ::/0 gw %gateway% ]]", ifd, exec);
#endif
	return( result);
}

static int static_down6(struct interface_defn_t *ifd, execfn *exec)
{
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	return(execute("ip link set %iface% down", ifd, exec));
#else
	return(execute("ifconfig %iface% down", ifd, exec));
#endif
}

#ifdef CONFIG_FEATURE_IFUPDOWN_IP
static int v4tunnel_up(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
	result = execute("ip tunnel add %iface% mode sit remote "
				"%endpoint% [[local %local%]] [[ttl %ttl%]]", ifd, exec);
	result += execute("ip link set %iface% up", ifd, exec);
	result += execute("ip addr add %address%/%netmask% dev %iface%", ifd, exec);
	result += execute("[[ ip route add ::/0 via %gateway% ]]", ifd, exec);
	return( result);
}

static int v4tunnel_down(struct interface_defn_t * ifd, execfn * exec)
{
	return( execute("ip tunnel del %iface%", ifd, exec));
}
#endif

static struct method_t methods6[] = {
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	{ "v4tunnel", v4tunnel_up, v4tunnel_down, },
#endif
	{ "static", static_up6, static_down6, },
	{ "loopback", loopback_up6, loopback_down6, },
};

struct address_family_t addr_inet6 = {
	"inet6",
	sizeof(methods6) / sizeof(struct method_t),
	methods6
};
#endif /* CONFIG_FEATURE_IFUPDOWN_IPV6 */

#ifdef CONFIG_FEATURE_IFUPDOWN_IPV4
static int loopback_up(struct interface_defn_t *ifd, execfn *exec)
{
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	int result;
	result = execute("ip addr add 127.0.0.1/8 dev %iface%", ifd, exec);
	result += execute("ip link set %iface% up", ifd, exec);
	return(result);
#else
	return( execute("ifconfig %iface% 127.0.0.1 up", ifd, exec));
#endif
}

static int loopback_down(struct interface_defn_t *ifd, execfn *exec)
{
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	int result;
	result = execute("ip addr flush dev %iface%", ifd, exec);
	result += execute("ip link set %iface% down", ifd, exec);
	return(result);
#else
	return( execute("ifconfig %iface% 127.0.0.1 down", ifd, exec));
#endif
}

static int static_up(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	result = execute("ip addr add %address%/%bnmask% [[broadcast %broadcast%]] "
			"dev %iface% [[peer %pointopoint%]] [[label %label%]]", ifd, exec);
	result += execute("ip link set [[mtu %mtu%]] [[address %hwaddress%]] %iface% up", ifd, exec);
	result += execute("[[ ip route add default via %gateway% dev %iface% ]]", ifd, exec);
#else
	result = execute("ifconfig %iface% %address% netmask %netmask% "
				"[[broadcast %broadcast%]] 	[[pointopoint %pointopoint%]] "
				"[[media %media%]] [[mtu %mtu%]] 	[[hw %hwaddress%]] up",
				ifd, exec);
	result += execute("[[ route add default gw %gateway% %iface% ]]", ifd, exec);
#endif
	return(result);
}

static int static_down(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	result = execute("ip addr flush dev %iface%", ifd, exec);
	result += execute("ip link set %iface% down", ifd, exec);
#else
	result = execute("[[ route del default gw %gateway% %iface% ]]", ifd, exec);
	result += execute("ifconfig %iface% down", ifd, exec);
#endif
	return(result);
}

static int execable(char *program)
{
	struct stat buf;
	if (0 == stat(program, &buf)) {
		if (S_ISREG(buf.st_mode) && (S_IXUSR & buf.st_mode)) {
			return(1);
		}
	}
	return(0);
}

static int dhcp_up(struct interface_defn_t *ifd, execfn *exec)
{
	if (execable("/sbin/udhcpc")) {
		return( execute("udhcpc -n -p /var/run/udhcpc.%iface%.pid -i "
					"%iface% [[-H %hostname%]] [[-c %clientid%]]", ifd, exec));
	} else if (execable("/sbin/pump")) {
		return( execute("pump -i %iface% [[-h %hostname%]] [[-l %leasehours%]]", ifd, exec));
	} else if (execable("/sbin/dhclient")) {
		return( execute("dhclient -pf /var/run/dhclient.%iface%.pid %iface%", ifd, exec));
	} else if (execable("/sbin/dhcpcd")) {
		return( execute("dhcpcd [[-h %hostname%]] [[-i %vendor%]] [[-I %clientid%]] "
					"[[-l %leasetime%]] %iface%", ifd, exec));
	}
	return(0);
}

static int dhcp_down(struct interface_defn_t *ifd, execfn *exec)
{
	int result = 0;
	if (execable("/sbin/udhcpc")) {
		execute("kill -9 `cat /var/run/udhcpc.%iface%.pid` 2>/dev/null", ifd, exec);
	} else if (execable("/sbin/pump")) {
		result = execute("pump -i %iface% -k", ifd, exec);
	} else if (execable("/sbin/dhclient")) {
		execute("kill -9 `cat /var/run/udhcpc.%iface%.pid` 2>/dev/null", ifd, exec);
	} else if (execable("/sbin/dhcpcd")) {
		result = execute("dhcpcd -k %iface%", ifd, exec);
	}
	return (result || execute("ifconfig %iface% down", ifd, exec));
}

static int bootp_up(struct interface_defn_t *ifd, execfn *exec)
{
	return( execute("bootpc [[--bootfile %bootfile%]] --dev %iface% "
				"[[--server %server%]] [[--hwaddr %hwaddr%]] "
				"--returniffail --serverbcast", ifd, exec));
}

static int bootp_down(struct interface_defn_t *ifd, execfn *exec)
{
#ifdef CONFIG_FEATURE_IFUPDOWN_IP
	return(execute("ip link set %iface% down", ifd, exec));
#else
	return(execute("ifconfig %iface% down", ifd, exec));
#endif
}

static int ppp_up(struct interface_defn_t *ifd, execfn *exec)
{
	return( execute("pon [[%provider%]]", ifd, exec));
}

static int ppp_down(struct interface_defn_t *ifd, execfn *exec)
{
	return( execute("poff [[%provider%]]", ifd, exec));
}

static int wvdial_up(struct interface_defn_t *ifd, execfn *exec)
{
	return( execute("/sbin/start-stop-daemon --start -x /usr/bin/wvdial "
				"-p /var/run/wvdial.%iface% -b -m -- [[ %provider% ]]", ifd, exec));
}

static int wvdial_down(struct interface_defn_t *ifd, execfn *exec)
{
	return( execute("/sbin/start-stop-daemon --stop -x /usr/bin/wvdial "
				"-p /var/run/wvdial.%iface% -s 2", ifd, exec));
}

static struct method_t methods[] = 
{
	{ "wvdial", wvdial_up, wvdial_down, },
	{ "ppp", ppp_up, ppp_down, },
	{ "static", static_up, static_down, },
	{ "bootp", bootp_up, bootp_down, },
	{ "dhcp", dhcp_up, dhcp_down, },
	{ "loopback", loopback_up, loopback_down, },
};

struct address_family_t addr_inet = 
{
	"inet",
	sizeof(methods) / sizeof(struct method_t),
	methods
};

#endif	/* ifdef CONFIG_FEATURE_IFUPDOWN_IPV4 */

static char *next_word(char **buf)
{
	unsigned short length;
	char *word;

	if ((buf == NULL) || (*buf == NULL) || (**buf == '\0')) {
		return NULL;
	}

	/* Skip over leading whitespace */
	word = *buf;
	while (isspace(*word)) {
		++word;
	}

	/* Skip over comments */
	if (*word == '#') {
		return(NULL);
	}

	/* Find the length of this word */
	length = strcspn(word, " \t\n");
	if (length == 0) {
		return(NULL);
	}
	*buf = word + length;
	/*DBU:[dave@cray.com] if we are already at EOL dont't increment beyond it */
	if (**buf) {
		**buf = '\0';
		(*buf)++;
	}

	return word;
}

static struct address_family_t *get_address_family(struct address_family_t *af[], char *name)
{
	int i;

	for (i = 0; af[i]; i++) {
		if (strcmp(af[i]->name, name) == 0) {
			return af[i];
		}
	}
	return NULL;
}

static struct method_t *get_method(struct address_family_t *af, char *name)
{
	int i;

	for (i = 0; i < af->n_methods; i++) {
		if (strcmp(af->method[i].name, name) == 0) {
			return &af->method[i];
		}
	}
	return(NULL);
}

static int duplicate_if(struct interface_defn_t *ifa, struct interface_defn_t *ifb)
{
	if (strcmp(ifa->iface, ifb->iface) != 0) {
		return(0);
	}
	if (ifa->address_family != ifb->address_family) {
		return(0);
	}
	return(1);
}

static const llist_t *find_list_string(const llist_t *list, const char *string)
{
	while (list) {
		if (strcmp(list->data, string) == 0) {
			return(list);
		}
		list = list->link;
	}
	return(NULL);
}

static struct interfaces_file_t *read_interfaces(char *filename)
{
#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
	struct mapping_defn_t *currmap = NULL;
#endif
	struct interface_defn_t *currif = NULL;
	struct interfaces_file_t *defn;
	FILE *f;
	char *firstword;
	char *buf;

	enum { NONE, IFACE, MAPPING } currently_processing = NONE;

	defn = xmalloc(sizeof(struct interfaces_file_t));
	defn->autointerfaces = NULL;
	defn->mappings = NULL;
	defn->ifaces = NULL;

	f = bb_xfopen(filename, "r");

	while ((buf = bb_get_chomped_line_from_file(f)) != NULL) {
		char *buf_ptr = buf;

		firstword = next_word(&buf_ptr);
		if (firstword == NULL) {
			free(buf);
			continue;	/* blank line */
		}

		if (strcmp(firstword, "mapping") == 0) {
#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
			currmap = xmalloc(sizeof(struct mapping_defn_t));
			currmap->max_matches = 0;
			currmap->n_matches = 0;
			currmap->match = NULL;

			while ((firstword = next_word(&buf_ptr)) != NULL) {
				if (currmap->max_matches == currmap->n_matches) {
					currmap->max_matches = currmap->max_matches * 2 + 1;
					currmap->match = xrealloc(currmap->match, sizeof(currmap->match) * currmap->max_matches);
				}

				currmap->match[currmap->n_matches++] = bb_xstrdup(firstword);
			}
			currmap->max_mappings = 0;
			currmap->n_mappings = 0;
			currmap->mapping = NULL;
			currmap->script = NULL;
			{
				struct mapping_defn_t **where = &defn->mappings;
				while (*where != NULL) {
					where = &(*where)->next;
				}
				*where = currmap;
				currmap->next = NULL;
			}
			debug_noise("Added mapping\n");
#endif
			currently_processing = MAPPING;
		} else if (strcmp(firstword, "iface") == 0) {
			{
				char *iface_name;
				char *address_family_name;
				char *method_name;
				struct address_family_t *addr_fams[] = {
#ifdef CONFIG_FEATURE_IFUPDOWN_IPV4
					&addr_inet,
#endif
#ifdef CONFIG_FEATURE_IFUPDOWN_IPV6
					&addr_inet6,
#endif
#ifdef CONFIG_FEATURE_IFUPDOWN_IPX
					&addr_ipx,
#endif
					NULL
				};

				currif = xmalloc(sizeof(struct interface_defn_t));
				iface_name = next_word(&buf_ptr);
				address_family_name = next_word(&buf_ptr);
				method_name = next_word(&buf_ptr);

				if (buf_ptr == NULL) {
					bb_error_msg("too few parameters for line \"%s\"", buf);
					return NULL;
				}

				/* ship any trailing whitespace */
				while (isspace(*buf_ptr)) {
					++buf_ptr;
				}

				if (buf_ptr[0] != '\0') {
					bb_error_msg("too many parameters \"%s\"", buf);
					return NULL;
				}

				currif->iface = bb_xstrdup(iface_name);

				currif->address_family = get_address_family(addr_fams, address_family_name);
				if (!currif->address_family) {
					bb_error_msg("unknown address type \"%s\"", buf);
					return NULL;
				}

				currif->method = get_method(currif->address_family, method_name);
				if (!currif->method) {
					bb_error_msg("unknown method \"%s\"", buf);
					return NULL;
				}

				currif->automatic = 1;
				currif->max_options = 0;
				currif->n_options = 0;
				currif->option = NULL;

				{
					struct interface_defn_t *tmp;
					llist_t *iface_list;
					iface_list = defn->ifaces;
					while (iface_list) {
						tmp = (struct interface_defn_t *) iface_list->data;
						if (duplicate_if(tmp, currif)) {
							bb_error_msg("duplicate interface \"%s\"", tmp->iface);
							return NULL;
						}
						iface_list = iface_list->link;
					}

					defn->ifaces = llist_add_to_end(defn->ifaces, (char*)currif);
				}
				debug_noise("iface %s %s %s\n", currif->iface, address_family_name, method_name);
			}
			currently_processing = IFACE;
		} else if (strcmp(firstword, "auto") == 0) {
			while ((firstword = next_word(&buf_ptr)) != NULL) {

				/* Check the interface isnt already listed */
				if (find_list_string(defn->autointerfaces, firstword)) {
					bb_perror_msg_and_die("interface declared auto twice \"%s\"", buf);
				}

				/* Add the interface to the list */
				defn->autointerfaces = llist_add_to_end(defn->autointerfaces, strdup(firstword));
				debug_noise("\nauto %s\n", firstword);
			}
			currently_processing = NONE;
		} else {
			switch (currently_processing) {
				case IFACE:
					{
						int i;

						if (bb_strlen(buf_ptr) == 0) {
							bb_error_msg("option with empty value \"%s\"", buf);
							return NULL;
						}

						if (strcmp(firstword, "up") != 0
								&& strcmp(firstword, "down") != 0
								&& strcmp(firstword, "pre-up") != 0
								&& strcmp(firstword, "post-down") != 0) {
							for (i = 0; i < currif->n_options; i++) {
								if (strcmp(currif->option[i].name, firstword) == 0) {
									bb_error_msg("duplicate option \"%s\"", buf);
									return NULL;
								}
							}
						}
					}
					if (currif->n_options >= currif->max_options) {
						struct variable_t *opt;

						currif->max_options = currif->max_options + 10;
						opt = xrealloc(currif->option, sizeof(*opt) * currif->max_options);
						currif->option = opt;
					}
					currif->option[currif->n_options].name = bb_xstrdup(firstword);
					currif->option[currif->n_options].value = bb_xstrdup(buf_ptr);
					if (!currif->option[currif->n_options].name) {
						perror(filename);
						return NULL;
					}
					if (!currif->option[currif->n_options].value) {
						perror(filename);
						return NULL;
					}
					debug_noise("\t%s=%s\n", currif->option[currif->n_options].name, 
							currif->option[currif->n_options].value);
					currif->n_options++;
					break;
				case MAPPING:
#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
					if (strcmp(firstword, "script") == 0) {
						if (currmap->script != NULL) {
							bb_error_msg("duplicate script in mapping \"%s\"", buf);
							return NULL;
						} else {
							currmap->script = bb_xstrdup(next_word(&buf_ptr));
						}
					} else if (strcmp(firstword, "map") == 0) {
						if (currmap->max_mappings == currmap->n_mappings) {
							currmap->max_mappings = currmap->max_mappings * 2 + 1;
							currmap->mapping = xrealloc(currmap->mapping, sizeof(char *) * currmap->max_mappings);
						}
						currmap->mapping[currmap->n_mappings] = bb_xstrdup(next_word(&buf_ptr));
						currmap->n_mappings++;
					} else {
						bb_error_msg("misplaced option \"%s\"", buf);
						return NULL;
					}
#endif
					break;
				case NONE:
				default:
					bb_error_msg("misplaced option \"%s\"", buf);
					return NULL;
			}
		}
		free(buf);
	}
	if (ferror(f) != 0) {
		bb_perror_msg_and_die("%s", filename);
	}
	fclose(f);

	return defn;
}

static char *setlocalenv(char *format, char *name, char *value)
{
	char *result;
	char *here;
	char *there;

	result = xmalloc(bb_strlen(format) + bb_strlen(name) + bb_strlen(value) + 1);

	sprintf(result, format, name, value);

	for (here = there = result; *there != '=' && *there; there++) {
		if (*there == '-')
			*there = '_';
		if (isalpha(*there))
			*there = toupper(*there);

		if (isalnum(*there) || *there == '_') {
			*here = *there;
			here++;
		}
	}
	memmove(here, there, bb_strlen(there) + 1);

	return result;
}

static void set_environ(struct interface_defn_t *iface, char *mode)
{
	char **environend;
	int i;
	const int n_env_entries = iface->n_options + 5;
	char **ppch;

	if (environ != NULL) {
		for (ppch = environ; *ppch; ppch++) {
			free(*ppch);
			*ppch = NULL;
		}
		free(environ);
		environ = NULL;
	}
	environ = xmalloc(sizeof(char *) * (n_env_entries + 1 /* for final NULL */ ));
	environend = environ;
	*environend = NULL;

	for (i = 0; i < iface->n_options; i++) {
		if (strcmp(iface->option[i].name, "up") == 0
				|| strcmp(iface->option[i].name, "down") == 0
				|| strcmp(iface->option[i].name, "pre-up") == 0
				|| strcmp(iface->option[i].name, "post-down") == 0) {
			continue;
		}
		*(environend++) = setlocalenv("IF_%s=%s", iface->option[i].name, iface->option[i].value);
		*environend = NULL;
	}

	*(environend++) = setlocalenv("%s=%s", "IFACE", iface->iface);
	*environend = NULL;
	*(environend++) = setlocalenv("%s=%s", "ADDRFAM", iface->address_family->name);
	*environend = NULL;
	*(environend++) = setlocalenv("%s=%s", "METHOD", iface->method->name);
	*environend = NULL;
	*(environend++) = setlocalenv("%s=%s", "MODE", mode);
	*environend = NULL;
	*(environend++) = setlocalenv("%s=%s", "PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
	*environend = NULL;
}

static int doit(char *str)
{
	if (verbose || no_act) {
		printf("%s\n", str);
	}
	if (!no_act) {
		pid_t child;
		int status;

		fflush(NULL);
		switch (child = fork()) {
			case -1:		/* failure */
				return 0;
			case 0:		/* child */
				execle(DEFAULT_SHELL, DEFAULT_SHELL, "-c", str, NULL, environ);
				exit(127);
		}
		waitpid(child, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			return 0;
		}
	}
	return (1);
}

static int execute_all(struct interface_defn_t *ifd, execfn *exec, const char *opt)
{
	int i;
	char *buf;
	for (i = 0; i < ifd->n_options; i++) {
		if (strcmp(ifd->option[i].name, opt) == 0) {
			if (!(*exec) (ifd->option[i].value)) {
				return 0;
			}
		}
	}
	
	bb_xasprintf(&buf, "run-parts /etc/network/if-%s.d", opt);
	(*exec)(buf);

	return (1);
}

static int check(char *str) {
	return str != NULL;
}

static int iface_up(struct interface_defn_t *iface)
{
	int result;
	if (!iface->method->up(iface,check)) return -1;
	set_environ(iface, "start");
	result = execute_all(iface, doit, "pre-up");
	result += iface->method->up(iface, doit);
	result += execute_all(iface, doit, "up");
	return(result);
}

static int iface_down(struct interface_defn_t *iface)
{
	int result;
	if (!iface->method->down(iface,check)) return -1;
	set_environ(iface, "stop");
	result = execute_all(iface, doit, "down");
	result += iface->method->down(iface, doit);
	result += execute_all(iface, doit, "post-down");
	return(result);
}

#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
static int popen2(FILE **in, FILE **out, char *command, ...)
{
	va_list ap;
	char *argv[11] = { command };
	int argc;
	int infd[2], outfd[2];
	pid_t pid;

	argc = 1;
	va_start(ap, command);
	while ((argc < 10) && (argv[argc] = va_arg(ap, char *))) {
		argc++;
	}
	argv[argc] = NULL;	/* make sure */
	va_end(ap);

	if (pipe(infd) != 0) {
		return 0;
	}

	if (pipe(outfd) != 0) {
		close(infd[0]);
		close(infd[1]);
		return 0;
	}

	fflush(NULL);
	switch (pid = fork()) {
		case -1:			/* failure */
			close(infd[0]);
			close(infd[1]);
			close(outfd[0]);
			close(outfd[1]);
			return 0;
		case 0:			/* child */
			dup2(infd[0], 0);
			dup2(outfd[1], 1);
			close(infd[0]);
			close(infd[1]);
			close(outfd[0]);
			close(outfd[1]);
			execvp(command, argv);
			exit(127);
		default:			/* parent */
			*in = fdopen(infd[1], "w");
			*out = fdopen(outfd[0], "r");
			close(infd[0]);
			close(outfd[1]);
			return pid;
	}
	/* unreached */
}

static char * run_mapping(char *physical, struct mapping_defn_t * map)
{
	FILE *in, *out;
	int i, status;
	pid_t pid;

	char *logical = bb_xstrdup(physical);

	/* Run the mapping script. */
	pid = popen2(&in, &out, map->script, physical, NULL);

	/* popen2() returns 0 on failure. */
	if (pid == 0)
		return logical;

	/* Write mappings to stdin of mapping script. */
	for (i = 0; i < map->n_mappings; i++) {
		fprintf(in, "%s\n", map->mapping[i]);
	}
	fclose(in);
	waitpid(pid, &status, 0);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		/* If the mapping script exited successfully, try to
		 * grab a line of output and use that as the name of the
		 * logical interface. */
		char *new_logical = (char *)xmalloc(MAX_INTERFACE_LENGTH);

		if (fgets(new_logical, MAX_INTERFACE_LENGTH, out)) {
			/* If we are able to read a line of output from the script,
			 * remove any trailing whitespace and use this value
			 * as the name of the logical interface. */
			char *pch = new_logical + bb_strlen(new_logical) - 1;

			while (pch >= new_logical && isspace(*pch))
				*(pch--) = '\0';

			free(logical);
			logical = new_logical;
		} else {
			/* If we are UNABLE to read a line of output, discard are
			 * freshly allocated memory. */
			free(new_logical);
		}
	}

	fclose(out);

	return logical;
}
#endif /* CONFIG_FEATURE_IFUPDOWN_MAPPING */

static llist_t *find_iface_state(llist_t *state_list, const char *iface)
{
	unsigned short iface_len = bb_strlen(iface);
	llist_t *search = state_list;

	while (search) {
		if ((strncmp(search->data, iface, iface_len) == 0) &&
				(search->data[iface_len] == '=')) {
			return(search);
		}
		search = search->link;
	}
	return(NULL);
}

extern int ifupdown_main(int argc, char **argv)
{
	int (*cmds) (struct interface_defn_t *) = NULL;
	struct interfaces_file_t *defn;
	FILE *state_fp = NULL;
	llist_t *state_list = NULL;
	llist_t *target_list = NULL;
	char *interfaces = "/etc/network/interfaces";
	const char *statefile = "/var/run/ifstate";

#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
	int run_mappings = 1;
#endif
	int do_all = 0;
	int force = 0;
	int i;

	if (bb_applet_name[2] == 'u') {
		/* ifup command */
		cmds = iface_up;
	} else {
		/* ifdown command */
		cmds = iface_down;
	}

#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
	while ((i = getopt(argc, argv, "i:hvnamf")) != -1)
#else
		while ((i = getopt(argc, argv, "i:hvnaf")) != -1) 
#endif
		{
			switch (i) {
				case 'i':	/* interfaces */
					interfaces = bb_xstrdup(optarg);
					break;
				case 'v':	/* verbose */
					verbose = 1;
					break;
				case 'a':	/* all */
					do_all = 1;
					break;
				case 'n':	/* no-act */
					no_act = 1;
					break;
#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
				case 'm':	/* no-mappings */
					run_mappings = 0;
					break;
#endif
				case 'f':	/* force */
					force = 1;
					break;
				default:
					bb_show_usage();
					break;
			}
		}

	if (argc - optind > 0) {
		if (do_all) {
			bb_show_usage();
		}
	} else {
		if (!do_all) {
			bb_show_usage();
		}
	}			

	debug_noise("reading %s file:\n", interfaces);
	defn = read_interfaces(interfaces);
	debug_noise("\ndone reading %s\n\n", interfaces);

	if (!defn) {
		exit(EXIT_FAILURE);
	}

	if (no_act) {
		state_fp = fopen(statefile, "r");
	}

	/* Create a list of interfaces to work on */
	if (do_all) {
		if (cmds == iface_up) {
			target_list = defn->autointerfaces;
		} else {
#if 0
			/* iface_down */
			llist_t *new_item;
			const llist_t *list = state_list;
			while (list) {
				new_item = xmalloc(sizeof(llist_t));
				new_item->data = strdup(list->data);
				new_item->link = NULL;
				list = target_list;
				if (list == NULL)
					target_list = new_item;
				else {
					while (list->link) {
						list = list->link;
					}
					list = new_item;
				}
				list = list->link;
			}
			target_list = defn->autointerfaces;
#else

			/* iface_down */
			const llist_t *list = state_list;
			while (list) {
				target_list = llist_add_to_end(target_list, strdup(list->data));
				list = list->link;
			}
			target_list = defn->autointerfaces;
#endif	
		} 
	} else {
		target_list = llist_add_to_end(target_list, argv[optind]);
	}


	/* Update the interfaces */
	while (target_list) {
		llist_t *iface_list;
		struct interface_defn_t *currif;
		char *iface;
		char *liface;
		char *pch;
		int okay = 0;

		iface = strdup(target_list->data);
		target_list = target_list->link;

		pch = strchr(iface, '=');
		if (pch) {
			*pch = '\0';
			liface = strdup(pch + 1);
		} else {
			liface = strdup(iface);
		}

		if (!force) {
			const llist_t *iface_state = find_iface_state(state_list, iface);

			if (cmds == iface_up) {
				/* ifup */
				if (iface_state) {
					bb_error_msg("interface %s already configured", iface);
					continue;
				}
			} else {
				/* ifdown */
				if (iface_state) {
					bb_error_msg("interface %s not configured", iface);
					continue;
				}
			}
		}

#ifdef CONFIG_FEATURE_IFUPDOWN_MAPPING
		if ((cmds == iface_up) && run_mappings) {
			struct mapping_defn_t *currmap;

			for (currmap = defn->mappings; currmap; currmap = currmap->next) {

				for (i = 0; i < currmap->n_matches; i++) {
					if (fnmatch(currmap->match[i], liface, 0) != 0)
						continue;
					if (verbose) {
						printf("Running mapping script %s on %s\n", currmap->script, liface);
					}
					liface = run_mapping(iface, currmap);
					break;
				}
			}
		}
#endif


		iface_list = defn->ifaces;
		while (iface_list) {
			currif = (struct interface_defn_t *) iface_list->data;
			if (strcmp(liface, currif->iface) == 0) {
				char *oldiface = currif->iface;

				okay = 1;
				currif->iface = iface;

				debug_noise("\nConfiguring interface %s (%s)\n", liface, currif->address_family->name);

				/* Call the cmds function pointer, does either iface_up() or iface_down() */
				if (cmds(currif) == -1) {
					bb_error_msg("Don't seem to be have all the variables for %s/%s.",
							liface, currif->address_family->name);
				}

				currif->iface = oldiface;
			}
			iface_list = iface_list->link;
		}
		if (verbose) {
			printf("\n");
		}

		if (!okay && !force) {
			bb_error_msg("Ignoring unknown interface %s", liface);
		} else {
			llist_t *iface_state = find_iface_state(state_list, iface);

			if (cmds == iface_up) {
				char *newiface = xmalloc(bb_strlen(iface) + 1 + bb_strlen(liface) + 1);
				sprintf(newiface, "%s=%s", iface, liface);
				if (iface_state == NULL) {
					state_list = llist_add_to_end(state_list, newiface);
				} else {
					free(iface_state->data);
					iface_state->data = newiface;
				}
			} else if (cmds == iface_down) {
				/* Remove an interface from the linked list */
				if (iface_state) {
					/* This needs to be done better */
					free(iface_state->data);
					free(iface_state->link);
					if (iface_state->link) {
						iface_state->data = iface_state->link->data;
						iface_state->link = iface_state->link->link;
					} else {
						iface_state->data = NULL;
						iface_state->link = NULL;
					}						
				}
			}
		}
	}

	/* Actually write the new state */
	if (!no_act) {

		if (state_fp)
			fclose(state_fp);
		state_fp = bb_xfopen(statefile, "a+");

		if (ftruncate(fileno(state_fp), 0) < 0) {
			bb_error_msg_and_die("failed to truncate statefile %s: %s", statefile, strerror(errno));
		}

		rewind(state_fp);

		while (state_list) {
			if (state_list->data) {
				fputs(state_list->data, state_fp);
				fputc('\n', state_fp);
			}
			state_list = state_list->link;
		}
		fflush(state_fp);
	}

	/* Cleanup */
	if (state_fp != NULL) {
		fclose(state_fp);
		state_fp = NULL;
	}

	return 0;
}
