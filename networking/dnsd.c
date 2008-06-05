/* vi: set sw=4 ts=4: */
/*
 * Mini DNS server implementation for busybox
 *
 * Copyright (C) 2005 Roberto A. Foglietta (me@roberto.foglietta.name)
 * Copyright (C) 2005 Odd Arild Olsen (oao at fibula dot no)
 * Copyright (C) 2003 Paul Sheer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Odd Arild Olsen started out with the sheerdns [1] of Paul Sheer and rewrote
 * it into a shape which I believe is both easier to understand and maintain.
 * I also reused the input buffer for output and removed services he did not
 * need.  [1] http://threading.2038bug.com/sheerdns/
 *
 * Some bugfix and minor changes was applied by Roberto A. Foglietta who made
 * the first porting of oao' scdns to busybox also.
 */

#include "libbb.h"
#include <syslog.h>

//#define DEBUG 1
#define DEBUG 0

enum {
	MAX_HOST_LEN = 16,      // longest host name allowed is 15
	IP_STRING_LEN = 18,     // .xxx.xxx.xxx.xxx\0

//must be strlen('.in-addr.arpa') larger than IP_STRING_LEN
	MAX_NAME_LEN = (IP_STRING_LEN + 13),

/* Cannot get bigger packets than 512 per RFC1035
   In practice this can be set considerably smaller:
   Length of response packet is  header (12B) + 2*type(4B) + 2*class(4B) +
   ttl(4B) + rlen(2B) + r (MAX_NAME_LEN =21B) +
   2*querystring (2 MAX_NAME_LEN= 42B), all together 90 Byte
*/
	MAX_PACK_LEN = 512,

	DEFAULT_TTL = 30,       // increase this when not testing?

	REQ_A = 1,
	REQ_PTR = 12
};

struct dns_head {		// the message from client and first part of response mag
	uint16_t id;
	uint16_t flags;
	uint16_t nquer;		// accepts 0
	uint16_t nansw;		// 1 in response
	uint16_t nauth;		// 0
	uint16_t nadd;		// 0
};
struct dns_prop {
	uint16_t type;
	uint16_t class;
};
struct dns_entry {		// element of known name, ip address and reversed ip address
	struct dns_entry *next;
	char ip[IP_STRING_LEN];		// dotted decimal IP
	char rip[IP_STRING_LEN];	// length decimal reversed IP
	char name[MAX_HOST_LEN];
};

static struct dns_entry *dnsentry;
static uint32_t ttl = DEFAULT_TTL;

static const char *fileconf = "/etc/dnsd.conf";

// Must match getopt32 call
#define OPT_daemon  (option_mask32 & 0x10)
#define OPT_verbose (option_mask32 & 0x20)


/*
 * Convert host name from C-string to dns length/string.
 */
static void convname(char *a, uint8_t *q)
{
	int i = (q[0] == '.') ? 0 : 1;
	for (; i < MAX_HOST_LEN-1 && *q; i++, q++)
		a[i] = tolower(*q);
	a[0] = i - 1;
	a[i] = 0;
}

/*
 * Insert length of substrings instead of dots
 */
static void undot(uint8_t * rip)
{
	int i = 0, s = 0;
	while (rip[i])
		i++;
	for (--i; i >= 0; i--) {
		if (rip[i] == '.') {
			rip[i] = s;
			s = 0;
		} else s++;
	}
}

/*
 * Read one line of hostname/IP from file
 * Returns 0 for each valid entry read, -1 at EOF
 * Assumes all host names are lower case only
 * Hostnames with more than one label are not handled correctly.
 * Presently the dot is copied into name without
 * converting to a length/string substring for that label.
 */
static int getfileentry(FILE * fp, struct dns_entry *s)
{
	unsigned int a,b,c,d;
	char *line, *r, *name;

 restart:
	line = r = xmalloc_fgets(fp);
	if (!r)
		return -1;
	while (*r == ' ' || *r == '\t') {
		r++;
		if (!*r || *r == '#' || *r == '\n') {
			free(line);
			goto restart; /* skipping empty/blank and commented lines  */
		}
	}
	name = r;
	while (*r != ' ' && *r != '\t')
		r++;
	*r++ = '\0';
	if (sscanf(r, ".%u.%u.%u.%u"+1, &a, &b, &c, &d) != 4) {
		free(line);
		goto restart; /* skipping wrong lines */
	}

	sprintf(s->ip, ".%u.%u.%u.%u"+1, a, b, c, d);
	sprintf(s->rip, ".%u.%u.%u.%u", d, c, b, a);
	undot((uint8_t*)s->rip);
	convname(s->name, (uint8_t*)name);

	if (OPT_verbose)
		fprintf(stderr, "\tname:%s, ip:%s\n", &(s->name[1]),s->ip);

	free(line);
	return 0;
}

/*
 * Read hostname/IP records from file
 */
static void dnsentryinit(void)
{
	FILE *fp;
	struct dns_entry *m, *prev;

	prev = dnsentry = NULL;
	fp = xfopen(fileconf, "r");

	while (1) {
		m = xzalloc(sizeof(*m));
		/*m->next = NULL;*/
		if (getfileentry(fp, m))
			break;

		if (prev == NULL)
			dnsentry = m;
		else
			prev->next = m;
		prev = m;
	}
	fclose(fp);
}

/*
 * Look query up in dns records and return answer if found
 * qs is the query string, first byte the string length
 */
static int table_lookup(uint16_t type, uint8_t * as, uint8_t * qs)
{
	int i;
	struct dns_entry *d = dnsentry;

	do {
#if DEBUG
		char *p,*q;
		q = (char *)&(qs[1]);
		p = &(d->name[1]);
		fprintf(stderr, "\n%s: %d/%d p:%s q:%s %d",
			__FUNCTION__, (int)strlen(p), (int)(d->name[0]),
			p, q, (int)strlen(q));
#endif
		if (type == REQ_A) { /* search by host name */
			for (i = 1; i <= (int)(d->name[0]); i++)
				if (tolower(qs[i]) != d->name[i])
					break;
			if (i > (int)(d->name[0]) ||
			    (d->name[0] == 1 && d->name[1] == '*')) {
				strcpy((char *)as, d->ip);
#if DEBUG
				fprintf(stderr, " OK as:%s\n", as);
#endif
				return 0;
			}
		} else if (type == REQ_PTR) { /* search by IP-address */
			if ((d->name[0] != 1 || d->name[1] != '*') &&
			    !strncmp((char*)&d->rip[1], (char*)&qs[1], strlen(d->rip)-1)) {
				strcpy((char *)as, d->name);
				return 0;
			}
		}
		d = d->next;
	} while (d);
	return -1;
}

/*
 * Decode message and generate answer
 */
static int process_packet(uint8_t *buf)
{
	uint8_t answstr[MAX_NAME_LEN + 1];
	struct dns_head *head;
	struct dns_prop *qprop;
	uint8_t *from, *answb;
	uint16_t outr_rlen;
	uint16_t outr_flags;
	uint16_t flags;
	int lookup_result, type, packet_len;
	int querystr_len;

	answstr[0] = '\0';

	head = (struct dns_head *)buf;
	if (head->nquer == 0) {
		bb_error_msg("no queries");
		return -1;
	}

	if (head->flags & 0x8000) {
		bb_error_msg("ignoring response packet");
		return -1;
	}

	from = (void *)&head[1];	//  start of query string
//FIXME: strlen of untrusted data??!
	querystr_len = strlen((char *)from) + 1 + sizeof(struct dns_prop);
	answb = from + querystr_len;   // where to append answer block

	outr_rlen = 0;
	outr_flags = 0;

	qprop = (struct dns_prop *)(answb - 4);
	type = ntohs(qprop->type);

	// only let REQ_A and REQ_PTR pass
	if (!(type == REQ_A || type == REQ_PTR)) {
		goto empty_packet;	/* we can't handle the query type */
	}

	if (ntohs(qprop->class) != 1 /* class INET */ ) {
		outr_flags = 4; /* not supported */
		goto empty_packet;
	}
	/* we only support standard queries */

	if ((ntohs(head->flags) & 0x7800) != 0)
		goto empty_packet;

	// We have a standard query
	bb_info_msg("%s", (char *)from);
	lookup_result = table_lookup(type, answstr, from);
	if (lookup_result != 0) {
		outr_flags = 3 | 0x0400;	// name do not exist and auth
		goto empty_packet;
	}
	if (type == REQ_A) {    // return an address
		struct in_addr a; // NB! its "struct { unsigned __long__ s_addr; }"
		uint32_t v32;
		if (!inet_aton((char*)answstr, &a)) { //dotted dec to long conv
			outr_flags = 1; /* Frmt err */
			goto empty_packet;
		}
		v32 = a.s_addr; /* in case long != int */
		memcpy(answstr, &v32, 4);
		outr_rlen = 4;			// uint32_t IP
	} else
		outr_rlen = strlen((char *)answstr) + 1;	// a host name
	outr_flags |= 0x0400;			/* authority-bit */
	// we have an answer
	head->nansw = htons(1);

	// copy query block to answer block
	memcpy(answb, from, querystr_len);
	answb += querystr_len;

	// and append answer rr
// FIXME: unaligned accesses??
	*(uint32_t *) answb = htonl(ttl);
	answb += 4;
	*(uint16_t *) answb = htons(outr_rlen);
	answb += 2;
	memcpy(answb, answstr, outr_rlen);
	answb += outr_rlen;

 empty_packet:

	flags = ntohs(head->flags);
	// clear rcode and RA, set responsebit and our new flags
	flags |= (outr_flags & 0xff80) | 0x8000;
	head->flags = htons(flags);
	head->nauth = head->nadd = 0;
	head->nquer = htons(1);

	packet_len = answb - buf;
	return packet_len;
}

/*
 * Exit on signal
 */
static void interrupt(int sig)
{
	/* unlink("/var/run/dnsd.lock"); */
	bb_error_msg("interrupt, exiting\n");
	kill_myself_with_sig(sig);
}

int dnsd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dnsd_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	const char *listen_interface = "0.0.0.0";
	char *sttl, *sport;
	len_and_sockaddr *lsa, *from, *to;
	unsigned lsa_size;
	int udps;
	uint16_t port = 53;
	/* Paranoid sizing: querystring x2 + ttl + outr_rlen + answstr */
	/* I'd rather see process_packet() fixed instead... */
	uint8_t buf[MAX_PACK_LEN * 2 + 4 + 2 + (MAX_NAME_LEN+1)];

	getopt32(argv, "i:c:t:p:dv", &listen_interface, &fileconf, &sttl, &sport);
	//if (option_mask32 & 0x1) // -i
	//if (option_mask32 & 0x2) // -c
	if (option_mask32 & 0x4) // -t
		ttl = xatou_range(sttl, 1, 0xffffffff);
	if (option_mask32 & 0x8) // -p
		port = xatou_range(sport, 1, 0xffff);

	if (OPT_verbose) {
		bb_info_msg("listen_interface: %s", listen_interface);
		bb_info_msg("ttl: %d, port: %d", ttl, port);
		bb_info_msg("fileconf: %s", fileconf);
	}

	if (OPT_daemon) {
		bb_daemonize_or_rexec(DAEMON_CLOSE_EXTRA_FDS, argv);
		openlog(applet_name, LOG_PID, LOG_DAEMON);
		logmode = LOGMODE_SYSLOG;
	}

	dnsentryinit();

	signal(SIGINT, interrupt);
	bb_signals(0
		/* why? + (1 << SIGPIPE) */
		+ (1 << SIGHUP)
#ifdef SIGTSTP
		+ (1 << SIGTSTP)
#endif
#ifdef SIGURG
		+ (1 << SIGURG)
#endif
		, SIG_IGN);

	lsa = xdotted2sockaddr(listen_interface, port);
	udps = xsocket(lsa->u.sa.sa_family, SOCK_DGRAM, 0);
	xbind(udps, &lsa->u.sa, lsa->len);
	socket_want_pktinfo(udps); /* needed for recv_from_to to work */
	lsa_size = LSA_LEN_SIZE + lsa->len;
	from = xzalloc(lsa_size);
	to = xzalloc(lsa_size);

	bb_info_msg("Accepting UDP packets on %s",
			xmalloc_sockaddr2dotted(&lsa->u.sa));

	while (1) {
		int r;
		/* Try to get *DEST* address (to which of our addresses
		 * this query was directed), and reply from the same address.
		 * Or else we can exhibit usual UDP ugliness:
		 * [ip1.multihomed.ip2] <=  query to ip1  <= peer
		 * [ip1.multihomed.ip2] => reply from ip2 => peer (confused) */
		memcpy(to, lsa, lsa_size);
		r = recv_from_to(udps, buf, MAX_PACK_LEN + 1, 0, &from->u.sa, &to->u.sa, lsa->len);
		if (r < 12 || r > MAX_PACK_LEN) {
			bb_error_msg("invalid packet size");
			continue;
		}
		if (OPT_verbose)
			bb_info_msg("Got UDP packet");
		buf[r] = '\0'; /* paranoia */
		r = process_packet(buf);
		if (r <= 0)
			continue;
		send_to_from(udps, buf, r, 0, &from->u.sa, &to->u.sa, lsa->len);
	}
	return 0;
}
