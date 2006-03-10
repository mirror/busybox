/*
 * Mini DNS server implementation for busybox
 *
 * Copyright (C) 2005 Roberto A. Foglietta (me@roberto.foglietta.name)
 * Copyright (C) 2005 Odd Arild Olsen (oao at fibula dot no)
 * Copyright (C) 2003 Paul Sheer
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

#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#include "libbb.h"

static char *fileconf = "/etc/dnsd.conf";
#define LOCK_FILE       "/var/run/dnsd.lock"
#define LOG_FILE        "/var/log/dnsd.log"

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
	MAX_PACK_LEN = 512 + 1,

	DEFAULT_TTL = 30,       // increase this when not testing?

	REQ_A = 1,
	REQ_PTR = 12
};

struct dns_repl {		// resource record, add 0 or 1 to accepted dns_msg in resp
	uint16_t rlen;
	uint8_t *r;		// resource
	uint16_t flags;
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

static struct dns_entry *dnsentry = NULL;
static int daemonmode = 0;
static uint32_t ttl = DEFAULT_TTL;

/*
 * Convert host name from C-string to dns length/string.
 */
static void
convname(char *a, uint8_t *q)
{
	int i = (q[0] == '.') ? 0 : 1;
	for(; i < MAX_HOST_LEN-1 && *q; i++, q++)
		a[i] = tolower(*q);
	a[0] = i - 1;
	a[i] = 0;
}

/*
 * Insert length of substrings insetad of dots
 */
static void
undot(uint8_t * rip)
{
	int i = 0, s = 0;
	while(rip[i]) i++;
	for(--i; i >= 0; i--) {
		if(rip[i] == '.') {
			rip[i] = s;
			s = 0;
		} else s++;
	}
}

/*
 * Append message to log file
 */
static void
log_message(char *filename, char *message)
{
	FILE *logfile;
	if (!daemonmode)
		return;
	logfile = fopen(filename, "a");
	if (!logfile)
		return;
	fprintf(logfile, "%s\n", message);
	fclose(logfile);
}

/*
 * Read one line of hostname/IP from file
 * Returns 0 for each valid entry read, -1 at EOF
 * Assumes all host names are lower case only
 * Hostnames with more than one label is not handled correctly.
 * Presently the dot is copied into name without
 * converting to a length/string substring for that label.
 */

static int
getfileentry(FILE * fp, struct dns_entry *s, int verb)
{
	unsigned int a,b,c,d;
	char *r, *name;

restart:
	if(!(r = bb_get_line_from_file(fp)))
		return -1;
	while(*r == ' ' || *r == '\t') {
		r++;
		if(!*r || *r == '#' || *r == '\n')
			goto restart; /* skipping empty/blank and commented lines  */
	}
	name = r;
	while(*r != ' ' && *r != '\t')
		r++;
	*r++ = 0;
	if(sscanf(r,"%u.%u.%u.%u",&a,&b,&c,&d) != 4)
			goto restart; /* skipping wrong lines */

	sprintf(s->ip,"%u.%u.%u.%u",a,b,c,d);
	sprintf(s->rip,".%u.%u.%u.%u",d,c,b,a);
	undot((uint8_t*)s->rip);
	convname(s->name,(uint8_t*)name);

	if(verb)
		fprintf(stderr,"\tname:%s, ip:%s\n",&(s->name[1]),s->ip);

	return 0; /* warningkiller */
}

/*
 * Read hostname/IP records from file
 */
static void
dnsentryinit(int verb)
{
	FILE *fp;
	struct dns_entry *m, *prev;
	prev = dnsentry = NULL;

	if(!(fp = fopen(fileconf, "r")))
		bb_perror_msg_and_die("open %s",fileconf);

	while (1) {
		if(!(m = (struct dns_entry *)malloc(sizeof(struct dns_entry))))
			bb_perror_msg_and_die("malloc dns_entry");

		m->next = NULL;
		if (getfileentry(fp, m, verb))
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
 * Set up UDP socket
 */
static int
listen_socket(char *iface_addr, int listen_port)
{
	struct sockaddr_in a;
	char msg[100];
	int s;
	int yes = 1;
	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
		bb_perror_msg_and_die("socket() failed");
#ifdef SO_REUSEADDR
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0)
		bb_perror_msg_and_die("setsockopt() failed");
#endif
	memset(&a, 0, sizeof(a));
	a.sin_port = htons(listen_port);
	a.sin_family = AF_INET;
	if (!inet_aton(iface_addr, &a.sin_addr))
		bb_perror_msg_and_die("bad iface address");
	if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0)
		bb_perror_msg_and_die("bind() failed");
	listen(s, 50);
	sprintf(msg, "accepting UDP packets on addr:port %s:%d\n",
		iface_addr, (int)listen_port);
	log_message(LOG_FILE, msg);
	return s;
}

/*
 * Look query up in dns records and return answer if found
 * qs is the query string, first byte the string length
 */
static int
table_lookup(uint16_t type, uint8_t * as, uint8_t * qs)
{
	int i;
	struct dns_entry *d=dnsentry;

	do {
#ifdef DEBUG
		char *p,*q;
		q = (char *)&(qs[1]);
		p = &(d->name[1]);
		fprintf(stderr, "\ntest: %d <%s> <%s> %d", strlen(p), p, q, strlen(q));
#endif
		if (type == REQ_A) { /* search by host name */
			for(i = 1; i <= (int)(d->name[0]); i++)
				if(tolower(qs[i]) != d->name[i])
					continue;
#ifdef DEBUG
			fprintf(stderr, " OK");
#endif
			strcpy((char *)as, d->ip);
#ifdef DEBUG
			fprintf(stderr, " %s ", as);
#endif
					return 0;
				}
		else if (type == REQ_PTR) { /* search by IP-address */
			if (!strncmp((char*)&d->rip[1], (char*)&qs[1], strlen(d->rip)-1)) {
				strcpy((char *)as, d->name);
				return 0;
			}
		}
	} while ((d = d->next) != NULL);
	return -1;
}


/*
 * Decode message and generate answer
 */
#define eret(s) do { fprintf (stderr, "%s\n", s); return -1; } while (0)
static int
process_packet(uint8_t * buf)
{
	struct dns_head *head;
	struct dns_prop *qprop;
	struct dns_repl outr;
	void *next, *from, *answb;

	uint8_t answstr[MAX_NAME_LEN + 1];
	int lookup_result, type, len, packet_len;
	uint16_t flags;

	answstr[0] = '\0';

	head = (struct dns_head *)buf;
	if (head->nquer == 0)
		eret("no queries");

	if ((head->flags & 0x8000))
		eret("ignoring response packet");

	from = (void *)&head[1];	//  start of query string
	next = answb = from + strlen((char *)&head[1]) + 1 + sizeof(struct dns_prop);   // where to append answer block

	outr.rlen = 0;			// may change later
	outr.r = NULL;
	outr.flags = 0;

	qprop = (struct dns_prop *)(answb - 4);
	type = ntohs(qprop->type);

	// only let REQ_A and REQ_PTR pass
	if (!(type == REQ_A || type == REQ_PTR)) {
		goto empty_packet;	/* we can't handle the query type */
	}

	if (ntohs(qprop->class) != 1 /* class INET */ ) {
		outr.flags = 4; /* not supported */
		goto empty_packet;
	}
	/* we only support standard queries */

	if ((ntohs(head->flags) & 0x7800) != 0)
		goto empty_packet;

	// We have a standard query

	log_message(LOG_FILE, (char *)head);
	lookup_result = table_lookup(type, answstr, (uint8_t*)(&head[1]));
	if (lookup_result != 0) {
		outr.flags = 3 | 0x0400;	//name do not exist and auth
		goto empty_packet;
	}
	if (type == REQ_A) {    // return an address
		struct in_addr a;
		if (!inet_aton((char*)answstr, &a)) {//dotted dec to long conv
			outr.flags = 1; /* Frmt err */
			goto empty_packet;
		}
		memcpy(answstr, &a.s_addr, 4);	// save before a disappears
		outr.rlen = 4;			// uint32_t IP
	}
	else
		outr.rlen = strlen((char *)answstr) + 1;	// a host name
	outr.r = answstr;			// 32 bit ip or a host name
	outr.flags |= 0x0400;			/* authority-bit */
	// we have an answer
	head->nansw = htons(1);

	// copy query block to answer block
	len = answb - from;
	memcpy(answb, from, len);
	next += len;

	// and append answer rr
	*(uint32_t *) next = htonl(ttl);
	next += 4;
	*(uint16_t *) next = htons(outr.rlen);
	next += 2;
	memcpy(next, (void *)answstr, outr.rlen);
	next += outr.rlen;

      empty_packet:

	flags = ntohs(head->flags);
	// clear rcode and RA, set responsebit and our new flags
	flags |= (outr.flags & 0xff80) | 0x8000;
	head->flags = htons(flags);
	head->nauth = head->nadd = htons(0);
	head->nquer = htons(1);

	packet_len = next - (void *)buf;
	return packet_len;
}

/*
 * Exit on signal
 */
static void
interrupt(int x)
{
	unlink(LOCK_FILE);
	write(2, "interrupt exiting\n", 18);
	exit(2);
}

#define is_daemon()  (flags&16)
#define is_verbose() (flags&32)
//#define DEBUG 1

int dnsd_main(int argc, char **argv)
{
	int udps;
	uint16_t port = 53;
	uint8_t buf[MAX_PACK_LEN];
	unsigned long flags = 0;
	char *listen_interface = "0.0.0.0";
	char *sttl=NULL, *sport=NULL;

	if(argc > 1)
		flags = bb_getopt_ulflags(argc, argv, "i:c:t:p:dv", &listen_interface, &fileconf, &sttl, &sport);
	if(sttl)
		if(!(ttl = atol(sttl)))
			bb_show_usage();
	if(sport)
		if(!(port = atol(sport)))
			bb_show_usage();

	if(is_verbose()) {
		fprintf(stderr,"listen_interface: %s\n", listen_interface);
		fprintf(stderr,"ttl: %d, port: %d\n", ttl, port);
		fprintf(stderr,"fileconf: %s\n", fileconf);
	}

	if(is_daemon())
#if defined(__uClinux__)
		/* reexec for vfork() do continue parent */
		vfork_daemon_rexec(1, 0, argc, argv, "-d");
#else							/* uClinux */
		if (daemon(1, 0) < 0) {
			bb_perror_msg_and_die("daemon");
		}
#endif							/* uClinuvx */

	dnsentryinit(is_verbose());

	signal(SIGINT, interrupt);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
#ifdef SIGURG
	signal(SIGURG, SIG_IGN);
#endif

	udps = listen_socket(listen_interface, port);
	if (udps < 0)
		exit(1);

	while (1) {
		fd_set fdset;
		int r;

		FD_ZERO(&fdset);
		FD_SET(udps, &fdset);
		// Block until a message arrives
		if((r = select(udps + 1, &fdset, NULL, NULL, NULL)) < 0)
			bb_perror_msg_and_die("select error");
		else
		if(r == 0)
			bb_perror_msg_and_die("select spurious return");

		/* Can this test ever be false? */
		if (FD_ISSET(udps, &fdset)) {
			struct sockaddr_in from;
			int fromlen = sizeof(from);
			r = recvfrom(udps, buf, sizeof(buf), 0,
				     (struct sockaddr *)&from,
				     (void *)&fromlen);
			if(is_verbose())
				fprintf(stderr, "\n--- Got UDP  ");
			log_message(LOG_FILE, "\n--- Got UDP  ");

			if (r < 12 || r > 512) {
				bb_error_msg("invalid packet size");
				continue;
			}
			if (r > 0) {
				r = process_packet(buf);
				if (r > 0)
					sendto(udps, buf,
					       r, 0, (struct sockaddr *)&from,
					       fromlen);
			}
		}		// end if
	}			// end while
	return 0;
}


