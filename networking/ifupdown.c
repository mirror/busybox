/* vi: set sw=4 ts=4: */
/*
 *  ifupdown for busybox
 *  Copyright (c) 2002 Glenn McGrath <bug1@iinet.net.au>
 *  Copyright (c) 2003-2004 Erik Andersen <andersen@codepoet.org>
 *
 *  Based on ifupdown v 0.6.4 by Anthony Towns
 *  Copyright (c) 1999 Anthony Towns <aj@azure.humbug.org.au>
 *
 *  Changes to upstream version
 *  Remove checks for kernel version, assume kernel version 2.2.0 or better.
 *  Lines in the interfaces file cannot wrap.
 *  To adhere to the FHS, the default state file is /var/run/ifstate.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "busybox.h"
#include <sys/utsname.h>
#include <fnmatch.h>
#include <getopt.h>

#define MAX_OPT_DEPTH 10
#define EUNBALBRACK 10001
#define EUNDEFVAR   10002
#define EUNBALPER   10000

#if ENABLE_FEATURE_IFUPDOWN_MAPPING
#define MAX_INTERFACE_LENGTH 10
#endif

#define debug_noise(args...) /*fprintf(stderr, args)*/

/* Forward declaration */
struct interface_defn_t;

typedef int execfn(char *command);

struct method_t
{
	char *name;
	int (*up)(struct interface_defn_t *ifd, execfn *e);
	int (*down)(struct interface_defn_t *ifd, execfn *e);
};

struct address_family_t
{
	char *name;
	int n_methods;
	const struct method_t *method;
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
	const struct address_family_t *address_family;
	const struct method_t *method;

	char *iface;
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

#define OPTION_STR "anvf" USE_FEATURE_IFUPDOWN_MAPPING("m") "i:"
enum {
	OPT_do_all = 0x1,
	OPT_no_act = 0x2,
	OPT_verbose = 0x4,
	OPT_force = 0x8,
	OPT_no_mappings = 0x10,
};
#define DO_ALL (option_mask32 & OPT_do_all)
#define NO_ACT (option_mask32 & OPT_no_act)
#define VERBOSE (option_mask32 & OPT_verbose)
#define FORCE (option_mask32 & OPT_force)
#define NO_MAPPINGS (option_mask32 & OPT_no_mappings)

static char **my_environ;

static char *startup_PATH;

#if ENABLE_FEATURE_IFUPDOWN_IPV4 || ENABLE_FEATURE_IFUPDOWN_IPV6

#if ENABLE_FEATURE_IFUPDOWN_IP

static unsigned count_bits(unsigned a)
{
	unsigned result;
	result = (a & 0x55) + ((a >> 1) & 0x55);
	result = (result & 0x33) + ((result >> 2) & 0x33);
	return (result & 0x0F) + ((result >> 4) & 0x0F);
}

static int count_netmask_bits(char *dotted_quad)
{
	unsigned result, a, b, c, d;
	/* Found a netmask...  Check if it is dotted quad */
	if (sscanf(dotted_quad, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
		return -1;
	// FIXME: will be confused by e.g. 255.0.255.0
	result = count_bits(a);
	result += count_bits(b);
	result += count_bits(c);
	result += count_bits(d);
	return (int)result;
}
#endif

static void addstr(char **bufp, const char *str, size_t str_length)
{
	/* xasprintf trick will be smaller, but we are often
	 * called with str_length == 1 - don't want to have
	 * THAT much of malloc/freeing! */
	char *buf = *bufp;
	int len = (buf ? strlen(buf) : 0);
	str_length++;
	buf = xrealloc(buf, len + str_length);
	/* copies at most str_length-1 chars! */
	safe_strncpy(buf + len, str, str_length);
	*bufp = buf;
}

static int strncmpz(const char *l, const char *r, size_t llen)
{
	int i = strncmp(l, r, llen);

	if (i == 0)
		return -r[llen];
	return i;
}

static char *get_var(const char *id, size_t idlen, struct interface_defn_t *ifd)
{
	int i;

	if (strncmpz(id, "iface", idlen) == 0) {
		char *result;
		static char label_buf[20];
		safe_strncpy(label_buf, ifd->iface, sizeof(label_buf));
		result = strchr(label_buf, ':');
		if (result) {
			*result = '\0';
		}
		return label_buf;
	}
	if (strncmpz(id, "label", idlen) == 0) {
		return ifd->iface;
	}
	for (i = 0; i < ifd->n_options; i++) {
		if (strncmpz(id, ifd->option[i].name, idlen) == 0) {
			return ifd->option[i].value;
		}
	}
	return NULL;
}

static char *parse(const char *command, struct interface_defn_t *ifd)
{
	size_t old_pos[MAX_OPT_DEPTH] = { 0 };
	int okay[MAX_OPT_DEPTH] = { 1 };
	int opt_depth = 1;
	char *result = NULL;

	while (*command) {
		switch (*command) {
		default:
			addstr(&result, command, 1);
			command++;
			break;
		case '\\':
			if (command[1]) {
				addstr(&result, command + 1, 1);
				command += 2;
			} else {
				addstr(&result, command, 1);
				command++;
			}
			break;
		case '[':
			if (command[1] == '[' && opt_depth < MAX_OPT_DEPTH) {
				old_pos[opt_depth] = result ? strlen(result) : 0;
				okay[opt_depth] = 1;
				opt_depth++;
				command += 2;
			} else {
				addstr(&result, "[", 1);
				command++;
			}
			break;
		case ']':
			if (command[1] == ']' && opt_depth > 1) {
				opt_depth--;
				if (!okay[opt_depth]) {
					result[old_pos[opt_depth]] = '\0';
				}
				command += 2;
			} else {
				addstr(&result, "]", 1);
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
					return NULL;
				}

				varvalue = get_var(command, nextpercent - command, ifd);

				if (varvalue) {
					addstr(&result, varvalue, strlen(varvalue));
				} else {
#if ENABLE_FEATURE_IFUPDOWN_IP
					/* Sigh...  Add a special case for 'ip' to convert from
					 * dotted quad to bit count style netmasks.  */
					if (strncmp(command, "bnmask", 6) == 0) {
						unsigned res;
						varvalue = get_var("netmask", 7, ifd);
						if (varvalue && (res = count_netmask_bits(varvalue)) > 0) {
							const char *argument = utoa(res);
							addstr(&result, argument, strlen(argument));
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
		return NULL;
	}

	if (!okay[0]) {
		errno = EUNDEFVAR;
		free(result);
		return NULL;
	}

	return result;
}

/* execute() returns 1 for success and 0 for failure */
static int execute(const char *command, struct interface_defn_t *ifd, execfn *exec)
{
	char *out;
	int ret;

	out = parse(command, ifd);
	if (!out) {
		/* parse error? */
		return 0;
	}
	/* out == "": parsed ok but not all needed variables known, skip */
	ret = out[0] ? (*exec)(out) : 1;

	free(out);
	if (ret != 1) {
		return 0;
	}
	return 1;
}
#endif

#if ENABLE_FEATURE_IFUPDOWN_IPV6
static int loopback_up6(struct interface_defn_t *ifd, execfn *exec)
{
#if ENABLE_FEATURE_IFUPDOWN_IP
	int result;
	result = execute("ip addr add ::1 dev %iface%", ifd, exec);
	result += execute("ip link set %iface% up", ifd, exec);
	return ((result == 2) ? 2 : 0);
#else
	return execute("ifconfig %iface% add ::1", ifd, exec);
#endif
}

static int loopback_down6(struct interface_defn_t *ifd, execfn *exec)
{
#if ENABLE_FEATURE_IFUPDOWN_IP
	return execute("ip link set %iface% down", ifd, exec);
#else
	return execute("ifconfig %iface% del ::1", ifd, exec);
#endif
}

static int static_up6(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
#if ENABLE_FEATURE_IFUPDOWN_IP
	result = execute("ip addr add %address%/%netmask% dev %iface%[[ label %label%]]", ifd, exec);
	result += execute("ip link set[[ mtu %mtu%]][[ address %hwaddress%]] %iface% up", ifd, exec);
	/* Was: "[[ ip ....%gateway% ]]". Removed extra spaces w/o checking */
	result += execute("[[ip route add ::/0 via %gateway%]]", ifd, exec);
#else
	result = execute("ifconfig %iface%[[ media %media%]][[ hw %hwaddress%]][[ mtu %mtu%]] up", ifd, exec);
	result += execute("ifconfig %iface% add %address%/%netmask%", ifd, exec);
	result += execute("[[route -A inet6 add ::/0 gw %gateway%]]", ifd, exec);
#endif
	return ((result == 3) ? 3 : 0);
}

static int static_down6(struct interface_defn_t *ifd, execfn *exec)
{
#if ENABLE_FEATURE_IFUPDOWN_IP
	return execute("ip link set %iface% down", ifd, exec);
#else
	return execute("ifconfig %iface% down", ifd, exec);
#endif
}

#if ENABLE_FEATURE_IFUPDOWN_IP
static int v4tunnel_up(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
	result = execute("ip tunnel add %iface% mode sit remote "
			"%endpoint%[[ local %local%]][[ ttl %ttl%]]", ifd, exec);
	result += execute("ip link set %iface% up", ifd, exec);
	result += execute("ip addr add %address%/%netmask% dev %iface%", ifd, exec);
	result += execute("[[ip route add ::/0 via %gateway%]]", ifd, exec);
	return ((result == 4) ? 4 : 0);
}

static int v4tunnel_down(struct interface_defn_t * ifd, execfn * exec)
{
	return execute("ip tunnel del %iface%", ifd, exec);
}
#endif

static const struct method_t methods6[] = {
#if ENABLE_FEATURE_IFUPDOWN_IP
	{ "v4tunnel", v4tunnel_up, v4tunnel_down, },
#endif
	{ "static", static_up6, static_down6, },
	{ "loopback", loopback_up6, loopback_down6, },
};

static const struct address_family_t addr_inet6 = {
	"inet6",
	sizeof(methods6) / sizeof(struct method_t),
	methods6
};
#endif /* FEATURE_IFUPDOWN_IPV6 */

#if ENABLE_FEATURE_IFUPDOWN_IPV4
static int loopback_up(struct interface_defn_t *ifd, execfn *exec)
{
#if ENABLE_FEATURE_IFUPDOWN_IP
	int result;
	result = execute("ip addr add 127.0.0.1/8 dev %iface%", ifd, exec);
	result += execute("ip link set %iface% up", ifd, exec);
	return ((result == 2) ? 2 : 0);
#else
	return execute("ifconfig %iface% 127.0.0.1 up", ifd, exec);
#endif
}

static int loopback_down(struct interface_defn_t *ifd, execfn *exec)
{
#if ENABLE_FEATURE_IFUPDOWN_IP
	int result;
	result = execute("ip addr flush dev %iface%", ifd, exec);
	result += execute("ip link set %iface% down", ifd, exec);
	return ((result == 2) ? 2 : 0);
#else
	return execute("ifconfig %iface% 127.0.0.1 down", ifd, exec);
#endif
}

static int static_up(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
#if ENABLE_FEATURE_IFUPDOWN_IP
	result = execute("ip addr add %address%/%bnmask%[[ broadcast %broadcast%]] "
			"dev %iface%[[ peer %pointopoint%]][[ label %label%]]", ifd, exec);
	result += execute("ip link set[[ mtu %mtu%]][[ address %hwaddress%]] %iface% up", ifd, exec);
	result += execute("[[ip route add default via %gateway% dev %iface%]]", ifd, exec);
	return ((result == 3) ? 3 : 0);
#else
	/* ifconfig said to set iface up before it processes hw %hwaddress%,
	 * which then of course fails. Thus we run two separate ifconfig */
	result = execute("ifconfig %iface%[[ hw %hwaddress%]][[ media %media%]][[ mtu %mtu%]] up",
				ifd, exec);
	result += execute("ifconfig %iface% %address% netmask %netmask%"
				"[[ broadcast %broadcast%]][[ pointopoint %pointopoint%]] ",
				ifd, exec);
 	result += execute("[[route add default gw %gateway% %iface%]]", ifd, exec);
	return ((result == 3) ? 3 : 0);
#endif
}

static int static_down(struct interface_defn_t *ifd, execfn *exec)
{
	int result;
#if ENABLE_FEATURE_IFUPDOWN_IP
	result = execute("ip addr flush dev %iface%", ifd, exec);
	result += execute("ip link set %iface% down", ifd, exec);
#else
	result = execute("[[route del default gw %gateway% %iface%]]", ifd, exec);
	result += execute("ifconfig %iface% down", ifd, exec);
#endif
	return ((result == 2) ? 2 : 0);
}

#if !ENABLE_APP_UDHCPC
struct dhcp_client_t
{
	const char *name;
	const char *startcmd;
	const char *stopcmd;
};

static const struct dhcp_client_t ext_dhcp_clients[] = {
	{ "udhcpc",
		"udhcpc -R -n -p /var/run/udhcpc.%iface%.pid -i %iface%[[ -H %hostname%]][[ -c %clientid%]][[ -s %script%]]",
		"kill -TERM `cat /var/run/udhcpc.%iface%.pid` 2>/dev/null",
	},
	{ "pump",
		"pump -i %iface%[[ -h %hostname%]][[ -l %leasehours%]]",
		"pump -i %iface% -k",
	},
	{ "dhclient",
		"dhclient -pf /var/run/dhclient.%iface%.pid %iface%",
		"kill -9 `cat /var/run/dhclient.%iface%.pid` 2>/dev/null",
	},
	{ "dhcpcd",
		"dhcpcd[[ -h %hostname%]][[ -i %vendor%]][[ -I %clientid%]][[ -l %leasetime%]] %iface%",
		"dhcpcd -k %iface%",
	},
};
#endif

static int dhcp_up(struct interface_defn_t *ifd, execfn *exec)
{
#if ENABLE_APP_UDHCPC
	return execute("udhcpc -R -n -p /var/run/udhcpc.%iface%.pid "
			"-i %iface%[[ -H %hostname%]][[ -c %clientid%]][[ -s %script%]]",
			ifd, exec);
#else
	int i, nclients = sizeof(ext_dhcp_clients) / sizeof(ext_dhcp_clients[0]);
	for (i = 0; i < nclients; i++) {
		if (exists_execable(ext_dhcp_clients[i].name))
			return execute(ext_dhcp_clients[i].startcmd, ifd, exec);
	}
	bb_error_msg("no dhcp clients found");
	return 0;
#endif
}

static int dhcp_down(struct interface_defn_t *ifd, execfn *exec)
{
#if ENABLE_APP_UDHCPC
	return execute("kill -TERM "
	               "`cat /var/run/udhcpc.%iface%.pid` 2>/dev/null", ifd, exec);
#else
	int i, nclients = sizeof(ext_dhcp_clients) / sizeof(ext_dhcp_clients[0]);
	for (i = 0; i < nclients; i++) {
		if (exists_execable(ext_dhcp_clients[i].name))
			return execute(ext_dhcp_clients[i].stopcmd, ifd, exec);
	}
	bb_error_msg("no dhcp clients found, using static interface shutdown");
	return static_down(ifd, exec);
#endif
}

static int manual_up_down(struct interface_defn_t *ifd, execfn *exec)
{
	return 1;
}

static int bootp_up(struct interface_defn_t *ifd, execfn *exec)
{
	return execute("bootpc[[ --bootfile %bootfile%]] --dev %iface%"
			"[[ --server %server%]][[ --hwaddr %hwaddr%]] "
			"--returniffail --serverbcast", ifd, exec);
}

static int ppp_up(struct interface_defn_t *ifd, execfn *exec)
{
	return execute("pon[[ %provider%]]", ifd, exec);
}

static int ppp_down(struct interface_defn_t *ifd, execfn *exec)
{
	return execute("poff[[ %provider%]]", ifd, exec);
}

static int wvdial_up(struct interface_defn_t *ifd, execfn *exec)
{
	return execute("start-stop-daemon --start -x wvdial "
		"-p /var/run/wvdial.%iface% -b -m --[[ %provider%]]", ifd, exec);
}

static int wvdial_down(struct interface_defn_t *ifd, execfn *exec)
{
	return execute("start-stop-daemon --stop -x wvdial "
			"-p /var/run/wvdial.%iface% -s 2", ifd, exec);
}

static const struct method_t methods[] = {
	{ "manual", manual_up_down, manual_up_down, },
	{ "wvdial", wvdial_up, wvdial_down, },
	{ "ppp", ppp_up, ppp_down, },
	{ "static", static_up, static_down, },
	{ "bootp", bootp_up, static_down, },
	{ "dhcp", dhcp_up, dhcp_down, },
	{ "loopback", loopback_up, loopback_down, },
};

static const struct address_family_t addr_inet = {
	"inet",
	sizeof(methods) / sizeof(struct method_t),
	methods
};

#endif	/* if ENABLE_FEATURE_IFUPDOWN_IPV4 */

static char *next_word(char **buf)
{
	unsigned short length;
	char *word;

	if (!buf || !*buf || !**buf) {
		return NULL;
	}

	/* Skip over leading whitespace */
	word = skip_whitespace(*buf);

	/* Skip over comments */
	if (*word == '#') {
		return NULL;
	}

	/* Find the length of this word */
	length = strcspn(word, " \t\n");
	if (length == 0) {
		return NULL;
	}
	*buf = word + length;
	/*DBU:[dave@cray.com] if we are already at EOL dont't increment beyond it */
	if (**buf) {
		**buf = '\0';
		(*buf)++;
	}

	return word;
}

static const struct address_family_t *get_address_family(const struct address_family_t *const af[], char *name)
{
	int i;

	if (!name)
		return NULL;

	for (i = 0; af[i]; i++) {
		if (strcmp(af[i]->name, name) == 0) {
			return af[i];
		}
	}
	return NULL;
}

static const struct method_t *get_method(const struct address_family_t *af, char *name)
{
	int i;

	if (!name)
		return NULL;

	for (i = 0; i < af->n_methods; i++) {
		if (strcmp(af->method[i].name, name) == 0) {
			return &af->method[i];
		}
	}
	return NULL;
}

static const llist_t *find_list_string(const llist_t *list, const char *string)
{
	if (string == NULL)
		return NULL;

	while (list) {
		if (strcmp(list->data, string) == 0) {
			return list;
		}
		list = list->link;
	}
	return NULL;
}

static struct interfaces_file_t *read_interfaces(const char *filename)
{
#if ENABLE_FEATURE_IFUPDOWN_MAPPING
	struct mapping_defn_t *currmap = NULL;
#endif
	struct interface_defn_t *currif = NULL;
	struct interfaces_file_t *defn;
	FILE *f;
	char *firstword;
	char *buf;

	enum { NONE, IFACE, MAPPING } currently_processing = NONE;

	defn = xzalloc(sizeof(struct interfaces_file_t));

	f = xfopen(filename, "r");

	while ((buf = xmalloc_getline(f)) != NULL) {
		char *buf_ptr = buf;

		firstword = next_word(&buf_ptr);
		if (firstword == NULL) {
			free(buf);
			continue;	/* blank line */
		}

		if (strcmp(firstword, "mapping") == 0) {
#if ENABLE_FEATURE_IFUPDOWN_MAPPING
			currmap = xzalloc(sizeof(struct mapping_defn_t));

			while ((firstword = next_word(&buf_ptr)) != NULL) {
				if (currmap->max_matches == currmap->n_matches) {
					currmap->max_matches = currmap->max_matches * 2 + 1;
					currmap->match = xrealloc(currmap->match, sizeof(currmap->match) * currmap->max_matches);
				}

				currmap->match[currmap->n_matches++] = xstrdup(firstword);
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
			static const struct address_family_t *const addr_fams[] = {
#if ENABLE_FEATURE_IFUPDOWN_IPV4
				&addr_inet,
#endif
#if ENABLE_FEATURE_IFUPDOWN_IPV6
				&addr_inet6,
#endif
				NULL
			};

			char *iface_name;
			char *address_family_name;
			char *method_name;
			llist_t *iface_list;

			currif = xzalloc(sizeof(struct interface_defn_t));
			iface_name = next_word(&buf_ptr);
			address_family_name = next_word(&buf_ptr);
			method_name = next_word(&buf_ptr);

			if (buf_ptr == NULL) {
				bb_error_msg("too few parameters for line \"%s\"", buf);
				return NULL;
			}

			/* ship any trailing whitespace */
			buf_ptr = skip_whitespace(buf_ptr);

			if (buf_ptr[0] != '\0') {
				bb_error_msg("too many parameters \"%s\"", buf);
				return NULL;
			}

			currif->iface = xstrdup(iface_name);

			currif->address_family = get_address_family(addr_fams, address_family_name);
			if (!currif->address_family) {
				bb_error_msg("unknown address type \"%s\"", address_family_name);
				return NULL;
			}

			currif->method = get_method(currif->address_family, method_name);
			if (!currif->method) {
				bb_error_msg("unknown method \"%s\"", method_name);
				return NULL;
			}

			for (iface_list = defn->ifaces; iface_list; iface_list = iface_list->link) {
				struct interface_defn_t *tmp = (struct interface_defn_t *) iface_list->data;
				if ((strcmp(tmp->iface, currif->iface) == 0) &&
					(tmp->address_family == currif->address_family)) {
					bb_error_msg("duplicate interface \"%s\"", tmp->iface);
					return NULL;
				}
			}
			llist_add_to_end(&(defn->ifaces), (char*)currif);

			debug_noise("iface %s %s %s\n", currif->iface, address_family_name, method_name);
			currently_processing = IFACE;
		} else if (strcmp(firstword, "auto") == 0) {
			while ((firstword = next_word(&buf_ptr)) != NULL) {

				/* Check the interface isnt already listed */
				if (find_list_string(defn->autointerfaces, firstword)) {
					bb_perror_msg_and_die("interface declared auto twice \"%s\"", buf);
				}

				/* Add the interface to the list */
				llist_add_to_end(&(defn->autointerfaces), xstrdup(firstword));
				debug_noise("\nauto %s\n", firstword);
			}
			currently_processing = NONE;
		} else {
			switch (currently_processing) {
			case IFACE:
				{
					int i;

					if (strlen(buf_ptr) == 0) {
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
				currif->option[currif->n_options].name = xstrdup(firstword);
				currif->option[currif->n_options].value = xstrdup(buf_ptr);
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
#if ENABLE_FEATURE_IFUPDOWN_MAPPING
				if (strcmp(firstword, "script") == 0) {
					if (currmap->script != NULL) {
						bb_error_msg("duplicate script in mapping \"%s\"", buf);
						return NULL;
					} else {
						currmap->script = xstrdup(next_word(&buf_ptr));
					}
				} else if (strcmp(firstword, "map") == 0) {
					if (currmap->max_mappings == currmap->n_mappings) {
						currmap->max_mappings = currmap->max_mappings * 2 + 1;
						currmap->mapping = xrealloc(currmap->mapping, sizeof(char *) * currmap->max_mappings);
					}
					currmap->mapping[currmap->n_mappings] = xstrdup(next_word(&buf_ptr));
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

static char *setlocalenv(char *format, const char *name, const char *value)
{
	char *result;
	char *here;
	char *there;

	result = xasprintf(format, name, value);

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
	memmove(here, there, strlen(there) + 1);

	return result;
}

static void set_environ(struct interface_defn_t *iface, const char *mode)
{
	char **environend;
	int i;
	const int n_env_entries = iface->n_options + 5;
	char **ppch;

	if (my_environ != NULL) {
		for (ppch = my_environ; *ppch; ppch++) {
			free(*ppch);
			*ppch = NULL;
		}
		free(my_environ);
	}
	my_environ = xzalloc(sizeof(char *) * (n_env_entries + 1 /* for final NULL */ ));
	environend = my_environ;

	for (i = 0; i < iface->n_options; i++) {
		if (strcmp(iface->option[i].name, "up") == 0
				|| strcmp(iface->option[i].name, "down") == 0
				|| strcmp(iface->option[i].name, "pre-up") == 0
				|| strcmp(iface->option[i].name, "post-down") == 0) {
			continue;
		}
		*(environend++) = setlocalenv("IF_%s=%s", iface->option[i].name, iface->option[i].value);
	}

	*(environend++) = setlocalenv("%s=%s", "IFACE", iface->iface);
	*(environend++) = setlocalenv("%s=%s", "ADDRFAM", iface->address_family->name);
	*(environend++) = setlocalenv("%s=%s", "METHOD", iface->method->name);
	*(environend++) = setlocalenv("%s=%s", "MODE", mode);
	*(environend++) = setlocalenv("%s=%s", "PATH", startup_PATH);
}

static int doit(char *str)
{
	if (option_mask32 & (OPT_no_act|OPT_verbose)) {
		puts(str);
	}
	if (!(option_mask32 & OPT_no_act)) {
		pid_t child;
		int status;

		fflush(NULL);
		child = fork();
		switch (child) {
		case -1: /* failure */
			return 0;
		case 0: /* child */
			execle(DEFAULT_SHELL, DEFAULT_SHELL, "-c", str, NULL, my_environ);
			exit(127);
		}
		waitpid(child, &status, 0);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			return 0;
		}
	}
	return 1;
}

static int execute_all(struct interface_defn_t *ifd, const char *opt)
{
	int i;
	char *buf;
	for (i = 0; i < ifd->n_options; i++) {
		if (strcmp(ifd->option[i].name, opt) == 0) {
			if (!doit(ifd->option[i].value)) {
				return 0;
			}
		}
	}

	buf = xasprintf("run-parts /etc/network/if-%s.d", opt);
	/* heh, we don't bother free'ing it */
	return doit(buf);
}

static int check(char *str)
{
	return str != NULL;
}

static int iface_up(struct interface_defn_t *iface)
{
	if (!iface->method->up(iface, check)) return -1;
	set_environ(iface, "start");
	if (!execute_all(iface, "pre-up")) return 0;
	if (!iface->method->up(iface, doit)) return 0;
	if (!execute_all(iface, "up")) return 0;
	return 1;
}

static int iface_down(struct interface_defn_t *iface)
{
	if (!iface->method->down(iface,check)) return -1;
	set_environ(iface, "stop");
	if (!execute_all(iface, "down")) return 0;
	if (!iface->method->down(iface, doit)) return 0;
	if (!execute_all(iface, "post-down")) return 0;
	return 1;
}

#if ENABLE_FEATURE_IFUPDOWN_MAPPING
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

static char *run_mapping(char *physical, struct mapping_defn_t * map)
{
	FILE *in, *out;
	int i, status;
	pid_t pid;

	char *logical = xstrdup(physical);

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
		char *new_logical = xmalloc(MAX_INTERFACE_LENGTH);

		if (fgets(new_logical, MAX_INTERFACE_LENGTH, out)) {
			/* If we are able to read a line of output from the script,
			 * remove any trailing whitespace and use this value
			 * as the name of the logical interface. */
			char *pch = new_logical + strlen(new_logical) - 1;

			while (pch >= new_logical && isspace(*pch))
				*(pch--) = '\0';

			free(logical);
			logical = new_logical;
		} else {
			/* If we are UNABLE to read a line of output, discard our
			 * freshly allocated memory. */
			free(new_logical);
		}
	}

	fclose(out);

	return logical;
}
#endif /* FEATURE_IFUPDOWN_MAPPING */

static llist_t *find_iface_state(llist_t *state_list, const char *iface)
{
	unsigned short iface_len = strlen(iface);
	llist_t *search = state_list;

	while (search) {
		if ((strncmp(search->data, iface, iface_len) == 0) &&
				(search->data[iface_len] == '=')) {
			return search;
		}
		search = search->link;
	}
	return NULL;
}

int ifupdown_main(int argc, char **argv)
{
	int (*cmds)(struct interface_defn_t *) = NULL;
	struct interfaces_file_t *defn;
	llist_t *state_list = NULL;
	llist_t *target_list = NULL;
	const char *interfaces = "/etc/network/interfaces";
	int any_failures = 0;

	cmds = iface_down;
	if (applet_name[2] == 'u') {
		/* ifup command */
		cmds = iface_up;
	}

	getopt32(argc, argv, OPTION_STR, &interfaces);
	if (argc - optind > 0) {
		if (DO_ALL) bb_show_usage();
	} else {
		if (!DO_ALL) bb_show_usage();
	}

	debug_noise("reading %s file:\n", interfaces);
	defn = read_interfaces(interfaces);
	debug_noise("\ndone reading %s\n\n", interfaces);

	if (!defn) {
		return EXIT_FAILURE;
	}

	startup_PATH = getenv("PATH");
	if (!startup_PATH) startup_PATH = "";

	/* Create a list of interfaces to work on */
	if (DO_ALL) {
		if (cmds == iface_up) {
			target_list = defn->autointerfaces;
		} else {
			/* iface_down */
			const llist_t *list = state_list;
			while (list) {
				llist_add_to_end(&target_list, xstrdup(list->data));
				list = list->link;
			}
			target_list = defn->autointerfaces;
		}
	} else {
		llist_add_to_end(&target_list, argv[optind]);
	}

	/* Update the interfaces */
	while (target_list) {
		llist_t *iface_list;
		struct interface_defn_t *currif;
		char *iface;
		char *liface;
		char *pch;
		int okay = 0;
		int cmds_ret;

		iface = xstrdup(target_list->data);
		target_list = target_list->link;

		pch = strchr(iface, '=');
		if (pch) {
			*pch = '\0';
			liface = xstrdup(pch + 1);
		} else {
			liface = xstrdup(iface);
		}

		if (!FORCE) {
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

#if ENABLE_FEATURE_IFUPDOWN_MAPPING
		if ((cmds == iface_up) && !NO_MAPPINGS) {
			struct mapping_defn_t *currmap;

			for (currmap = defn->mappings; currmap; currmap = currmap->next) {
				int i;
				for (i = 0; i < currmap->n_matches; i++) {
					if (fnmatch(currmap->match[i], liface, 0) != 0)
						continue;
					if (VERBOSE) {
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
				cmds_ret = cmds(currif);
				if (cmds_ret == -1) {
					bb_error_msg("don't seem to have all the variables for %s/%s",
							liface, currif->address_family->name);
					any_failures = 1;
				} else if (cmds_ret == 0) {
					any_failures = 1;
				}

				currif->iface = oldiface;
			}
			iface_list = iface_list->link;
		}
		if (VERBOSE) {
			puts("");
		}

		if (!okay && !FORCE) {
			bb_error_msg("ignoring unknown interface %s", liface);
			any_failures = 1;
		} else {
			llist_t *iface_state = find_iface_state(state_list, iface);

			if (cmds == iface_up) {
				char *newiface = xasprintf("%s=%s", iface, liface);
				if (iface_state == NULL) {
					llist_add_to_end(&state_list, newiface);
				} else {
					free(iface_state->data);
					iface_state->data = newiface;
				}
			} else {
				/* Remove an interface from the linked list */
				free(llist_pop(&iface_state));
			}
		}
	}

	/* Actually write the new state */
	if (!NO_ACT) {
		FILE *state_fp;

		state_fp = xfopen("/var/run/ifstate", "w");
		while (state_list) {
			if (state_list->data) {
				fprintf(state_fp, "%s\n", state_list->data);
			}
			state_list = state_list->link;
		}
		fclose(state_fp);
	}

	return any_failures;
}
