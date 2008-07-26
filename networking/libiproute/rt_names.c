/* vi: set sw=4 ts=4: */
/*
 * rt_names.c		rtnetlink names DB.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include "libbb.h"
#include "rt_names.h"

/* so far all callers have size == 256 */
#define rtnl_tab_initialize(file, tab, size) rtnl_tab_initialize(file, tab)
#define size 256
static void rtnl_tab_initialize(const char *file, const char **tab, int size)
{
	char *token[2];
	parser_t *parser = config_open2(file, fopen_for_read);
	while (config_read(parser, token, 2, 2, "# \t", PARSE_NORMAL)) {
		int id = bb_strtou(token[0], NULL, 0);
		if (id < 0 || id > size) {
			bb_error_msg("database %s is corrupted at line %d",
				file, parser->lineno);
			break;
		}
		tab[id] = xstrdup(token[1]);
	}
	config_close(parser);
}
#undef size

static const char **rtnl_rtprot_tab; /* [256] */

static void rtnl_rtprot_initialize(void)
{
	static const char *const init_tab[] = {
		"none",
		"redirect",
		"kernel",
		"boot",
		"static",
		NULL,
		NULL,
		NULL,
		"gated",
		"ra",
		"mrt",
		"zebra",
		"bird",
	};
	if (rtnl_rtprot_tab) return;
	rtnl_rtprot_tab = xzalloc(256 * sizeof(rtnl_rtprot_tab[0]));
	memcpy(rtnl_rtprot_tab, init_tab, sizeof(init_tab));
	rtnl_tab_initialize("/etc/iproute2/rt_protos",
			    rtnl_rtprot_tab, 256);
}


const char* rtnl_rtprot_n2a(int id, char *buf, int len)
{
	if (id < 0 || id >= 256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}

	rtnl_rtprot_initialize();

	if (rtnl_rtprot_tab[id])
		return rtnl_rtprot_tab[id];
	snprintf(buf, len, "%d", id);
	return buf;
}

int rtnl_rtprot_a2n(uint32_t *id, char *arg)
{
	static const char *cache = NULL;
	static unsigned long res;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	rtnl_rtprot_initialize();

	for (i = 0; i < 256; i++) {
		if (rtnl_rtprot_tab[i] &&
		    strcmp(rtnl_rtprot_tab[i], arg) == 0) {
			cache = rtnl_rtprot_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = bb_strtoul(arg, NULL, 0);
	if (errno || res > 255)
		return -1;
	*id = res;
	return 0;
}


static const char **rtnl_rtscope_tab; /* [256] */

static void rtnl_rtscope_initialize(void)
{
	if (rtnl_rtscope_tab) return;
	rtnl_rtscope_tab = xzalloc(256 * sizeof(rtnl_rtscope_tab[0]));
	rtnl_rtscope_tab[0] = "global";
	rtnl_rtscope_tab[255] = "nowhere";
	rtnl_rtscope_tab[254] = "host";
	rtnl_rtscope_tab[253] = "link";
	rtnl_rtscope_tab[200] = "site";
	rtnl_tab_initialize("/etc/iproute2/rt_scopes",
			    rtnl_rtscope_tab, 256);
}


const char* rtnl_rtscope_n2a(int id, char *buf, int len)
{
	if (id < 0 || id >= 256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}

	rtnl_rtscope_initialize();

	if (rtnl_rtscope_tab[id])
		return rtnl_rtscope_tab[id];
	snprintf(buf, len, "%d", id);
	return buf;
}

int rtnl_rtscope_a2n(uint32_t *id, char *arg)
{
	static const char *cache = NULL;
	static unsigned long res;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	rtnl_rtscope_initialize();

	for (i = 0; i < 256; i++) {
		if (rtnl_rtscope_tab[i] &&
		    strcmp(rtnl_rtscope_tab[i], arg) == 0) {
			cache = rtnl_rtscope_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = bb_strtoul(arg, NULL, 0);
	if (errno || res > 255)
		return -1;
	*id = res;
	return 0;
}


static const char **rtnl_rtrealm_tab; /* [256] */

static void rtnl_rtrealm_initialize(void)
{
	if (rtnl_rtrealm_tab) return;
	rtnl_rtrealm_tab = xzalloc(256 * sizeof(rtnl_rtrealm_tab[0]));
	rtnl_rtrealm_tab[0] = "unknown";
	rtnl_tab_initialize("/etc/iproute2/rt_realms",
			    rtnl_rtrealm_tab, 256);
}


int rtnl_rtrealm_a2n(uint32_t *id, char *arg)
{
	static const char *cache = NULL;
	static unsigned long res;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	rtnl_rtrealm_initialize();

	for (i = 0; i < 256; i++) {
		if (rtnl_rtrealm_tab[i] &&
		    strcmp(rtnl_rtrealm_tab[i], arg) == 0) {
			cache = rtnl_rtrealm_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = bb_strtoul(arg, NULL, 0);
	if (errno || res > 255)
		return -1;
	*id = res;
	return 0;
}

#if ENABLE_FEATURE_IP_RULE
const char* rtnl_rtrealm_n2a(int id, char *buf, int len)
{
	if (id < 0 || id >= 256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}

	rtnl_rtrealm_initialize();

	if (rtnl_rtrealm_tab[id])
		return rtnl_rtrealm_tab[id];
	snprintf(buf, len, "%d", id);
	return buf;
}
#endif


static const char **rtnl_rtdsfield_tab; /* [256] */

static void rtnl_rtdsfield_initialize(void)
{
	if (rtnl_rtdsfield_tab) return;
	rtnl_rtdsfield_tab = xzalloc(256 * sizeof(rtnl_rtdsfield_tab[0]));
	rtnl_rtdsfield_tab[0] = "0";
	rtnl_tab_initialize("/etc/iproute2/rt_dsfield",
			    rtnl_rtdsfield_tab, 256);
}


const char * rtnl_dsfield_n2a(int id, char *buf, int len)
{
	if (id < 0 || id >= 256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}

	rtnl_rtdsfield_initialize();

	if (rtnl_rtdsfield_tab[id])
		return rtnl_rtdsfield_tab[id];
	snprintf(buf, len, "0x%02x", id);
	return buf;
}


int rtnl_dsfield_a2n(uint32_t *id, char *arg)
{
	static const char *cache = NULL;
	static unsigned long res;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	rtnl_rtdsfield_initialize();

	for (i = 0; i < 256; i++) {
		if (rtnl_rtdsfield_tab[i] &&
		    strcmp(rtnl_rtdsfield_tab[i], arg) == 0) {
			cache = rtnl_rtdsfield_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = bb_strtoul(arg, NULL, 16);
	if (errno || res > 255)
		return -1;
	*id = res;
	return 0;
}


#if ENABLE_FEATURE_IP_RULE
static const char **rtnl_rttable_tab; /* [256] */

static void rtnl_rttable_initialize(void)
{
	if (rtnl_rtdsfield_tab) return;
	rtnl_rttable_tab = xzalloc(256 * sizeof(rtnl_rttable_tab[0]));
	rtnl_rttable_tab[0] = "unspec";
	rtnl_rttable_tab[255] = "local";
	rtnl_rttable_tab[254] = "main";
	rtnl_rttable_tab[253] = "default";
	rtnl_tab_initialize("/etc/iproute2/rt_tables", rtnl_rttable_tab, 256);
}


const char *rtnl_rttable_n2a(int id, char *buf, int len)
{
	if (id < 0 || id >= 256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}

	rtnl_rttable_initialize();

	if (rtnl_rttable_tab[id])
		return rtnl_rttable_tab[id];
	snprintf(buf, len, "%d", id);
	return buf;
}

int rtnl_rttable_a2n(uint32_t * id, char *arg)
{
	static char *cache = NULL;
	static unsigned long res;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	rtnl_rttable_initialize();

	for (i = 0; i < 256; i++) {
		if (rtnl_rttable_tab[i] && strcmp(rtnl_rttable_tab[i], arg) == 0) {
			cache = (char*)rtnl_rttable_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	i = bb_strtoul(arg, NULL, 0);
	if (errno || i > 255)
		return -1;
	*id = i;
	return 0;
}

#endif
