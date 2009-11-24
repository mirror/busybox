/*
 * NTP client/server, based on OpenNTPD 3.9p1
 *
 * Author: Adam Tkac <vonsch@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <netinet/ip.h> /* For IPTOS_LOWDELAY definition */

#ifndef IP_PKTINFO
# error "Sorry, your kernel has to support IP_PKTINFO"
#endif

#define INTERVAL_QUERY_NORMAL		30	/* sync to peers every n secs */
#define INTERVAL_QUERY_PATHETIC		60
#define INTERVAL_QUERY_AGRESSIVE	5

#define TRUSTLEVEL_BADPEER		6	/* bad if *less than* TRUSTLEVEL_BADPEER */
#define TRUSTLEVEL_PATHETIC		2
#define TRUSTLEVEL_AGRESSIVE		8
#define TRUSTLEVEL_MAX			10

#define QSCALE_OFF_MIN			0.05
#define QSCALE_OFF_MAX			0.50

#define QUERYTIME_MAX		15	/* single query might take n secs max */
#define OFFSET_ARRAY_SIZE	8
#define SETTIME_MIN_OFFSET	180	/* min offset for settime at start */
#define SETTIME_TIMEOUT		15	/* max seconds to wait with -s */

/* Style borrowed from NTP ref/tcpdump and updated for SNTPv4 (RFC2030). */

/*
 * RFC Section 3
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         Integer Part                          |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                         Fraction Part                         |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |            Integer Part       |     Fraction Part             |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef struct {
	uint32_t int_partl;
	uint32_t fractionl;
} l_fixedpt_t;

typedef struct {
	uint16_t int_parts;
	uint16_t fractions;
} s_fixedpt_t;

enum {
	NTP_DIGESTSIZE     = 16,
	NTP_MSGSIZE_NOAUTH = 48,
	NTP_MSGSIZE        = (NTP_MSGSIZE_NOAUTH + 4 + NTP_DIGESTSIZE),
};

typedef struct {
	uint8_t     status;     /* status of local clock and leap info */
	uint8_t     stratum;    /* stratum level */
	uint8_t     ppoll;      /* poll value */
	int8_t      precision;
	s_fixedpt_t rootdelay;
	s_fixedpt_t dispersion;
	uint32_t    refid;
	l_fixedpt_t reftime;
	l_fixedpt_t orgtime;
	l_fixedpt_t rectime;
	l_fixedpt_t xmttime;
	uint32_t    keyid;
	uint8_t     digest[NTP_DIGESTSIZE];
} ntp_msg_t;

typedef struct {
	int       fd;
	ntp_msg_t msg;
	double    xmttime;
} ntp_query_t;

enum {
	NTP_VERSION	= 4,
	NTP_MAXSTRATUM	= 15,
	/* Leap Second Codes (high order two bits) */
	LI_NOWARNING	= (0 << 6),	/* no warning */
	LI_PLUSSEC	= (1 << 6),	/* add a second (61 seconds) */
	LI_MINUSSEC	= (2 << 6),	/* minus a second (59 seconds) */
	LI_ALARM	= (3 << 6),	/* alarm condition */

	/* Status Masks */
	MODE_MASK	= (7 << 0),
	VERSION_MASK	= (7 << 3),
	VERSION_SHIFT	= 3,
	LI_MASK		= (3 << 6),

	/* Mode values */
	MODE_RES0	= 0,	/* reserved */
	MODE_SYM_ACT	= 1,	/* symmetric active */
	MODE_SYM_PAS	= 2,	/* symmetric passive */
	MODE_CLIENT	= 3,	/* client */
	MODE_SERVER	= 4,	/* server */
	MODE_BROADCAST	= 5,	/* broadcast */
	MODE_RES1	= 6,	/* reserved for NTP control message */
	MODE_RES2	= 7,	/* reserved for private use */
};

#define OFFSET_1900_1970 2208988800UL  /* 1970 - 1900 in seconds */

enum client_state {
	STATE_NONE,
	STATE_QUERY_SENT,
	STATE_REPLY_RECEIVED,
};

typedef struct {
	double		rootdelay;
	double		rootdispersion;
	double		reftime;
	uint32_t	refid;
	uint32_t	refid4;
	uint8_t		synced;
	uint8_t		leap;
	int8_t		precision;
	uint8_t		poll;
	uint8_t		stratum;
} ntp_status_t;

typedef struct {
	ntp_status_t	status;
	double		offset;
	double		delay;
	double		error;
	time_t		rcvd;
	uint8_t		good;
} ntp_offset_t;

typedef struct {
//TODO:
// (1) store dotted addr str, to avoid constant translations
// (2) periodically re-resolve DNS names
	len_and_sockaddr	*lsa;
	ntp_query_t		query;
	ntp_offset_t		reply[OFFSET_ARRAY_SIZE];
	ntp_offset_t		update;
	enum client_state	state;
	time_t			next;
	time_t			deadline;
	uint8_t			shift;
	uint8_t			trustlevel;
} ntp_peer_t;

enum {
	OPT_n = (1 << 0),
	OPT_g = (1 << 1),
	OPT_q = (1 << 2),
	/* Insert new options above this line. */
	/* Non-compat options: */
	OPT_p = (1 << 3),
	OPT_l = (1 << 4),
};


struct globals {
	unsigned	verbose;
#if ENABLE_FEATURE_NTPD_SERVER
	int		listen_fd;
#endif
	unsigned	peer_cnt;
	llist_t		*ntp_peers;
	ntp_status_t	status;
	uint32_t	scale;
	uint8_t		settime;
	uint8_t		firstadj;
};
#define G (*ptr_to_globals)


static const int const_IPTOS_LOWDELAY = IPTOS_LOWDELAY;


static void
set_next(ntp_peer_t *p, time_t t)
{
	p->next = time(NULL) + t;
	p->deadline = 0;
}

static void
add_peers(const char *s)
{
	ntp_peer_t *p;

	p = xzalloc(sizeof(*p));
//TODO: big ntpd uses all IPs, not just 1st, do we need to mimic that?
	p->lsa = xhost2sockaddr(s, 123);
	p->query.fd = -1;
	p->query.msg.status = MODE_CLIENT | (NTP_VERSION << 3);
	if (STATE_NONE != 0)
		p->state = STATE_NONE;
	p->trustlevel = TRUSTLEVEL_PATHETIC;
	p->query.fd = -1;
	set_next(p, 0);

	llist_add_to(&G.ntp_peers, p);
	G.peer_cnt++;
}

static double
gettime1900fp(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL); /* never fails */
	return (tv.tv_sec + 1.0e-6 * tv.tv_usec + OFFSET_1900_1970);
}

static void
d_to_tv(double d, struct timeval *tv)
{
	tv->tv_sec = (long)d;
	tv->tv_usec = (d - tv->tv_sec) * 1000000;
}

static double
lfp_to_d(l_fixedpt_t lfp)
{
	double ret;
	lfp.int_partl = ntohl(lfp.int_partl);
	lfp.fractionl = ntohl(lfp.fractionl);
	ret = (double)lfp.int_partl + ((double)lfp.fractionl / UINT_MAX);
	return ret;
}

static double
sfp_to_d(s_fixedpt_t sfp)
{
	double ret;
	sfp.int_parts = ntohs(sfp.int_parts);
	sfp.fractions = ntohs(sfp.fractions);
	ret = (double)sfp.int_parts + ((double)sfp.fractions / USHRT_MAX);
	return ret;
}

#if ENABLE_FEATURE_NTPD_SERVER
static l_fixedpt_t
d_to_lfp(double d)
{
	l_fixedpt_t lfp;
	lfp.int_partl = (uint32_t)d;
	lfp.fractionl = (uint32_t)((d - lfp.int_partl) * UINT_MAX);
	lfp.int_partl = htonl(lfp.int_partl);
	lfp.fractionl = htonl(lfp.fractionl);
	return lfp;
}

static s_fixedpt_t
d_to_sfp(double d)
{
	s_fixedpt_t sfp;
	sfp.int_parts = (uint16_t)d;
	sfp.fractions = (uint16_t)((d - sfp.int_parts) * USHRT_MAX);
	sfp.int_parts = htons(sfp.int_parts);
	sfp.fractions = htons(sfp.fractions);
	return sfp;
}
#endif

static void
set_deadline(ntp_peer_t *p, time_t t)
{
	p->deadline = time(NULL) + t;
	p->next = 0;
}

static time_t
error_interval(void)
{
	time_t interval, r;
	interval = INTERVAL_QUERY_PATHETIC * QSCALE_OFF_MAX / QSCALE_OFF_MIN;
	r = (unsigned)random() % (unsigned long)(interval / 10);
	return (interval + r);
}

static int
sendmsg_wrap(int fd,
		const struct sockaddr *from, const struct sockaddr *to, socklen_t addrlen,
		ntp_msg_t *msg, ssize_t len)
{
	ssize_t ret;

	errno = 0;
	if (!from) {
		ret = sendto(fd, msg, len, MSG_DONTWAIT, to, addrlen);
	} else {
		ret = send_to_from(fd, msg, len, MSG_DONTWAIT, to, from, addrlen);
	}
	if (ret != len) {
		bb_perror_msg("send failed");
		return -1;
	}
	return 0;
}

static int
send_query_to_peer(ntp_peer_t *p)
{
	// Why do we need to bind()?
	// See what happens when we don't bind:
	//
	// socket(PF_INET, SOCK_DGRAM, IPPROTO_IP) = 3
	// setsockopt(3, SOL_IP, IP_TOS, [16], 4) = 0
	// gettimeofday({1259071266, 327885}, NULL) = 0
	// sendto(3, "xxx", 48, MSG_DONTWAIT, {sa_family=AF_INET, sin_port=htons(123), sin_addr=inet_addr("10.34.32.125")}, 16) = 48
	// ^^^ we sent it from some source port picked by kernel.
	// time(NULL)              = 1259071266
	// write(2, "ntpd: entering poll 15 secs\n", 28) = 28
	// poll([{fd=3, events=POLLIN}], 1, 15000) = 1 ([{fd=3, revents=POLLIN}])
	// recv(3, "yyy", 68, MSG_DONTWAIT) = 48
	// ^^^ this recv will receive packets to any local port!
	//
	// Uncomment this and use strace to see it in action:
#define PROBE_LOCAL_ADDR // { len_and_sockaddr lsa; lsa.len = LSA_SIZEOF_SA; getsockname(p->query.fd, &lsa.u.sa, &lsa.len); }

	if (p->query.fd == -1) {
		int fd, family;
		len_and_sockaddr *local_lsa;

		family = p->lsa->u.sa.sa_family;
		//was: p->query.fd = xsocket(family, SOCK_DGRAM, 0);
		p->query.fd = fd = xsocket_type(&local_lsa, family, SOCK_DGRAM);
		/* local_lsa has "null" address and port 0 now.
		 * bind() ensures we have a *particular port* selected by kernel
		 * and remembered in p->query.fd, thus later recv(p->query.fd)
		 * receives only packets sent to this port.
		 */
		PROBE_LOCAL_ADDR
		xbind(fd, &local_lsa->u.sa, local_lsa->len);
		PROBE_LOCAL_ADDR
#if ENABLE_FEATURE_IPV6
		if (family == AF_INET)
#endif
			setsockopt(fd, IPPROTO_IP, IP_TOS, &const_IPTOS_LOWDELAY, sizeof(const_IPTOS_LOWDELAY));
		free(local_lsa);
	}

	/*
	 * Send out a random 64-bit number as our transmit time.  The NTP
	 * server will copy said number into the originate field on the
	 * response that it sends us.  This is totally legal per the SNTP spec.
	 *
	 * The impact of this is two fold: we no longer send out the current
	 * system time for the world to see (which may aid an attacker), and
	 * it gives us a (not very secure) way of knowing that we're not
	 * getting spoofed by an attacker that can't capture our traffic
	 * but can spoof packets from the NTP server we're communicating with.
	 *
	 * Save the real transmit timestamp locally.
	 */

	p->query.msg.xmttime.int_partl = random();
	p->query.msg.xmttime.fractionl = random();
	p->query.xmttime = gettime1900fp();

	if (sendmsg_wrap(p->query.fd, /*from:*/ NULL, /*to:*/ &p->lsa->u.sa, /*addrlen:*/ p->lsa->len,
			&p->query.msg, NTP_MSGSIZE_NOAUTH) == -1) {
		set_next(p, INTERVAL_QUERY_PATHETIC);
		return -1;
	}

	p->state = STATE_QUERY_SENT;
	set_deadline(p, QUERYTIME_MAX);

	return 0;
}

static int
offset_compare(const void *aa, const void *bb)
{
	const ntp_peer_t *const *a = aa;
	const ntp_peer_t *const *b = bb;
	if ((*a)->update.offset < (*b)->update.offset)
		return -1;
	return ((*a)->update.offset > (*b)->update.offset);
}

static uint32_t
updated_scale(double offset)
{
	if (offset < 0)
		offset = -offset;
	if (offset > QSCALE_OFF_MAX)
		return 1;
	if (offset < QSCALE_OFF_MIN)
		return QSCALE_OFF_MAX / QSCALE_OFF_MIN;
	return QSCALE_OFF_MAX / offset;
}

static void
adjtime_wrap(void)
{
	ntp_peer_t	 *p;
	unsigned	  offset_cnt;
	unsigned	  middle;
	int		  i = 0;
	ntp_peer_t	**peers;
	double		  offset_median;
	llist_t		 *item;
	len_and_sockaddr *lsa;
	struct timeval	  tv, olddelta;

	offset_cnt = 0;
	for (item = G.ntp_peers; item != NULL; item = item->link) {
		p = (ntp_peer_t *) item->data;
		if (p->trustlevel < TRUSTLEVEL_BADPEER)
			continue;
		if (!p->update.good)
			return;
		offset_cnt++;
	}

	if (offset_cnt == 0)
		goto clear_good;

	peers = xzalloc(sizeof(peers[0]) * offset_cnt);
	for (item = G.ntp_peers; item != NULL; item = item->link) {
		p = (ntp_peer_t *) item->data;
		if (p->trustlevel < TRUSTLEVEL_BADPEER)
			continue;
		peers[i++] = p;
	}

	qsort(peers, offset_cnt, sizeof(peers[0]), offset_compare);

	middle = offset_cnt / 2;
	if ((offset_cnt & 1) == 0 && middle != 0) {
		offset_median = (peers[middle-1]->update.offset + peers[middle]->update.offset) / 2;
		G.status.rootdelay = (peers[middle-1]->update.delay + peers[middle]->update.delay) / 2;
		G.status.stratum = MAX(peers[middle-1]->update.status.stratum, peers[middle]->update.status.stratum);
	} else {
		offset_median = peers[middle]->update.offset;
		G.status.rootdelay = peers[middle]->update.delay;
		G.status.stratum = peers[middle]->update.status.stratum;
	}
	G.status.leap = peers[middle]->update.status.leap;

	bb_info_msg("adjusting local clock by %fs", offset_median);

	d_to_tv(offset_median, &tv);
	if (adjtime(&tv, &olddelta) == -1)
		bb_error_msg("adjtime failed");
	else if (!G.firstadj
	 && olddelta.tv_sec == 0
	 && olddelta.tv_usec == 0
	 && !G.status.synced
	) {
		bb_info_msg("clock synced");
		G.status.synced = 1;
	} else if (G.status.synced) {
		bb_info_msg("clock unsynced");
		G.status.synced = 0;
	}

	G.firstadj = 0;
	G.status.reftime = gettime1900fp();
	G.status.stratum++;	/* one more than selected peer */
	G.scale = updated_scale(offset_median);

	G.status.refid4 = peers[middle]->update.status.refid4;

	lsa = peers[middle]->lsa;
	G.status.refid =
#if ENABLE_FEATURE_IPV6
		lsa->u.sa.sa_family != AF_INET ?
			G.status.refid4 :
#endif
			lsa->u.sin.sin_addr.s_addr;

	free(peers);

 clear_good:
	for (item = G.ntp_peers; item != NULL; item = item->link) {
		p = (ntp_peer_t *) item->data;
		p->update.good = 0;
	}
}

static void
settime(double offset)
{
	ntp_peer_t *p;
	llist_t		*item;
	struct timeval	tv, curtime;
	char		buf[80];
	time_t		tval;

	if (!G.settime)
		goto bail;

	G.settime = 0;

	/* if the offset is small, don't call settimeofday */
	if (offset < SETTIME_MIN_OFFSET && offset > -SETTIME_MIN_OFFSET)
		goto bail;

	gettimeofday(&curtime, NULL); /* never fails */

	d_to_tv(offset, &tv);
	curtime.tv_usec += tv.tv_usec + 1000000;
	curtime.tv_sec += tv.tv_sec - 1 + (curtime.tv_usec / 1000000);
	curtime.tv_usec %= 1000000;

	if (settimeofday(&curtime, NULL) == -1) {
		bb_error_msg("settimeofday");
		goto bail;
	}

	tval = curtime.tv_sec;
	strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Z %Y", localtime(&tval));

	/* Do we want to print message below to system log when daemonized? */
	bb_info_msg("set local clock to %s (offset %fs)", buf, offset);

	for (item = G.ntp_peers; item != NULL; item = item->link) {
		p = (ntp_peer_t *) item->data;
		if (p->next)
			p->next -= offset;
		if (p->deadline)
			p->deadline -= offset;
	}

 bail:
	if (option_mask32 & OPT_q)
		exit(0);
}

static void
update_peer_data(ntp_peer_t *p)
{
	int i, best = 0, good = 0;

	/*
	 * clock filter
	 * find the offset which arrived with the lowest delay
	 * use that as the peer update
	 * invalidate it and all older ones
	 */

	for (i = 0; good == 0 && i < OFFSET_ARRAY_SIZE; i++) {
		if (p->reply[i].good) {
			good++;
			best = i;
		}
	}

	for (; i < OFFSET_ARRAY_SIZE; i++) {
		if (p->reply[i].good) {
			good++;
			if (p->reply[i].delay < p->reply[best].delay)
				best = i;
		}
	}

	if (good < 8)
		return;

	memcpy(&p->update, &p->reply[best], sizeof(p->update));
	adjtime_wrap();

	for (i = 0; i < OFFSET_ARRAY_SIZE; i++)
		if (p->reply[i].rcvd <= p->reply[best].rcvd)
			p->reply[i].good = 0;
}

static time_t
scale_interval(time_t requested)
{
	time_t interval, r;
	interval = requested * G.scale;
	r = (unsigned)random() % (unsigned long)(MAX(5, interval / 10));
	return (interval + r);
}

static void
recv_and_process_peer_pkt(ntp_peer_t *p)
{
	char			 *addr;
	ssize_t			 size;
	ntp_msg_t		 msg;
	double			 T1, T2, T3, T4;
	time_t			 interval;
	ntp_offset_t		*offset;

	addr = xmalloc_sockaddr2dotted_noport(&p->lsa->u.sa);

	/* We can recvfrom here and check from.IP, but some multihomed
	 * ntp servers reply from their *other IP*.
	 * TODO: maybe we should check at least what we can: from.port == 123?
	 */
	size = recv(p->query.fd, &msg, sizeof(msg), MSG_DONTWAIT);
	if (size == -1) {
		bb_perror_msg("recv(%s) error", addr);
		if (errno == EHOSTUNREACH || errno == EHOSTDOWN
		 || errno == ENETUNREACH || errno == ENETDOWN
		 || errno == ECONNREFUSED || errno == EADDRNOTAVAIL
		 || errno == EAGAIN
		) {
//TODO: always do this?
			set_next(p, error_interval());
			goto bail;
		}
		xfunc_die();
	}

	T4 = gettime1900fp();

	if (size != NTP_MSGSIZE_NOAUTH && size != NTP_MSGSIZE) {
		bb_error_msg("malformed packet received from %s", addr);
		goto bail;
	}

	if (msg.orgtime.int_partl != p->query.msg.xmttime.int_partl
	 || msg.orgtime.fractionl != p->query.msg.xmttime.fractionl
	) {
		goto bail;
	}

	if ((msg.status & LI_ALARM) == LI_ALARM
	 || msg.stratum == 0
	 || msg.stratum > NTP_MAXSTRATUM
	) {
		interval = error_interval();
		bb_info_msg("reply from %s: not synced, next query %ds", addr, (int) interval);
		goto bail;
	}

	/*
	 * From RFC 2030 (with a correction to the delay math):
	 *
	 *     Timestamp Name          ID   When Generated
	 *     ------------------------------------------------------------
	 *     Originate Timestamp     T1   time request sent by client
	 *     Receive Timestamp       T2   time request received by server
	 *     Transmit Timestamp      T3   time reply sent by server
	 *     Destination Timestamp   T4   time reply received by client
	 *
	 *  The roundtrip delay d and local clock offset t are defined as
	 *
	 *    d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.
	 */

	T1 = p->query.xmttime;
	T2 = lfp_to_d(msg.rectime);
	T3 = lfp_to_d(msg.xmttime);

	offset = &p->reply[p->shift];

	offset->offset = ((T2 - T1) + (T3 - T4)) / 2;
	offset->delay = (T4 - T1) - (T3 - T2);
	if (offset->delay < 0) {
		interval = error_interval();
		set_next(p, interval);
		bb_info_msg("reply from %s: negative delay %f", addr, p->reply[p->shift].delay);
		goto bail;
	}
	offset->error = (T2 - T1) - (T3 - T4);
// Can we use (T4 - OFFSET_1900_1970) instead of time(NULL)?
	offset->rcvd = time(NULL);
	offset->good = 1;

	offset->status.leap = (msg.status & LI_MASK);
	offset->status.precision = msg.precision;
	offset->status.rootdelay = sfp_to_d(msg.rootdelay);
	offset->status.rootdispersion = sfp_to_d(msg.dispersion);
	offset->status.refid = ntohl(msg.refid);
	offset->status.refid4 = msg.xmttime.fractionl;
	offset->status.reftime = lfp_to_d(msg.reftime);
	offset->status.poll = msg.ppoll;
	offset->status.stratum = msg.stratum;

	if (p->trustlevel < TRUSTLEVEL_PATHETIC)
		interval = scale_interval(INTERVAL_QUERY_PATHETIC);
	else if (p->trustlevel < TRUSTLEVEL_AGRESSIVE)
		interval = scale_interval(INTERVAL_QUERY_AGRESSIVE);
	else
		interval = scale_interval(INTERVAL_QUERY_NORMAL);

	set_next(p, interval);
	p->state = STATE_REPLY_RECEIVED;

	/* every received reply which we do not discard increases trust */
	if (p->trustlevel < TRUSTLEVEL_MAX) {
		p->trustlevel++;
		if (p->trustlevel == TRUSTLEVEL_BADPEER)
			bb_info_msg("peer %s now valid", addr);
	}

	bb_info_msg("reply from %s: offset %f delay %f, next query %ds", addr,
			offset->offset, offset->delay, (int) interval);

	update_peer_data(p);
	settime(offset->offset);

	p->shift++;
	if (p->shift >= OFFSET_ARRAY_SIZE)
		p->shift = 0;

 bail:
	free(addr);
}

#if ENABLE_FEATURE_NTPD_SERVER
static void
recv_and_process_client_pkt(void /*int fd*/)
{
	ssize_t          size;
	uint8_t          version;
	double           rectime;
	len_and_sockaddr *to;
	struct sockaddr  *from;
	ntp_msg_t        msg;
	uint8_t          query_status;
	uint8_t          query_ppoll;
	l_fixedpt_t      query_xmttime;

	to = get_sock_lsa(G.listen_fd);
	from = xzalloc(to->len);

	size = recv_from_to(G.listen_fd, &msg, sizeof(msg), MSG_DONTWAIT, from, &to->u.sa, to->len);
	if (size != NTP_MSGSIZE_NOAUTH && size != NTP_MSGSIZE) {
		char *addr;
		if (size < 0) {
			if (errno == EAGAIN)
				goto bail;
			bb_perror_msg_and_die("recv_from_to");
		}
		addr = xmalloc_sockaddr2dotted_noport(from);
		bb_error_msg("malformed packet received from %s", addr);
		free(addr);
		goto bail;
	}

	query_status = msg.status;
	query_ppoll = msg.ppoll;
	query_xmttime = msg.xmttime;

	/* Build a reply packet */
	memset(&msg, 0, sizeof(msg));
	msg.status = G.status.synced ? G.status.leap : LI_ALARM;
	msg.status |= (query_status & VERSION_MASK);
	msg.status |= ((query_status & MODE_MASK) == MODE_CLIENT) ?
			 MODE_SERVER : MODE_SYM_PAS;
	msg.stratum = G.status.stratum;
	msg.ppoll = query_ppoll;
	msg.precision = G.status.precision;
	rectime = gettime1900fp();
	msg.xmttime = msg.rectime = d_to_lfp(rectime);
	msg.reftime = d_to_lfp(G.status.reftime);
	//msg.xmttime = d_to_lfp(gettime1900fp()); // = msg.rectime
	msg.orgtime = query_xmttime;
	msg.rootdelay = d_to_sfp(G.status.rootdelay);
	version = (query_status & VERSION_MASK); /* ... >> VERSION_SHIFT - done below instead */
	msg.refid = (version > (3 << VERSION_SHIFT)) ? G.status.refid4 : G.status.refid;

	/* We reply from the local address packet was sent to,
	 * this makes to/from look swapped here: */
	sendmsg_wrap(G.listen_fd,
		/*from:*/ &to->u.sa, /*to:*/ from, /*addrlen:*/ to->len,
		&msg, size);

 bail:
	free(to);
	free(from);
}
#endif

/* Upstream ntpd's options:
 *
 * -4   Force DNS resolution of host names to the IPv4 namespace.
 * -6   Force DNS resolution of host names to the IPv6 namespace.
 * -a   Require cryptographic authentication for broadcast client,
 *      multicast client and symmetric passive associations.
 *      This is the default.
 * -A   Do not require cryptographic authentication for broadcast client,
 *      multicast client and symmetric passive associations.
 *      This is almost never a good idea.
 * -b   Enable the client to synchronize to broadcast servers.
 * -c conffile
 *      Specify the name and path of the configuration file,
 *      default /etc/ntp.conf
 * -d   Specify debugging mode. This option may occur more than once,
 *      with each occurrence indicating greater detail of display.
 * -D level
 *      Specify debugging level directly.
 * -f driftfile
 *      Specify the name and path of the frequency file.
 *      This is the same operation as the "driftfile FILE"
 *      configuration command.
 * -g   Normally, ntpd exits with a message to the system log
 *      if the offset exceeds the panic threshold, which is 1000 s
 *      by default. This option allows the time to be set to any value
 *      without restriction; however, this can happen only once.
 *      If the threshold is exceeded after that, ntpd will exit
 *      with a message to the system log. This option can be used
 *      with the -q and -x options. See the tinker command for other options.
 * -i jaildir
 *      Chroot the server to the directory jaildir. This option also implies
 *      that the server attempts to drop root privileges at startup
 *      (otherwise, chroot gives very little additional security).
 *      You may need to also specify a -u option.
 * -k keyfile
 *      Specify the name and path of the symmetric key file,
 *      default /etc/ntp/keys. This is the same operation
 *      as the "keys FILE" configuration command.
 * -l logfile
 *      Specify the name and path of the log file. The default
 *      is the system log file. This is the same operation as
 *      the "logfile FILE" configuration command.
 * -L   Do not listen to virtual IPs. The default is to listen.
 * -n   Don't fork.
 * -N   To the extent permitted by the operating system,
 *      run the ntpd at the highest priority.
 * -p pidfile
 *      Specify the name and path of the file used to record the ntpd
 *      process ID. This is the same operation as the "pidfile FILE"
 *      configuration command.
 * -P priority
 *      To the extent permitted by the operating system,
 *      run the ntpd at the specified priority.
 * -q   Exit the ntpd just after the first time the clock is set.
 *      This behavior mimics that of the ntpdate program, which is
 *      to be retired. The -g and -x options can be used with this option.
 *      Note: The kernel time discipline is disabled with this option.
 * -r broadcastdelay
 *      Specify the default propagation delay from the broadcast/multicast
 *      server to this client. This is necessary only if the delay
 *      cannot be computed automatically by the protocol.
 * -s statsdir
 *      Specify the directory path for files created by the statistics
 *      facility. This is the same operation as the "statsdir DIR"
 *      configuration command.
 * -t key
 *      Add a key number to the trusted key list. This option can occur
 *      more than once.
 * -u user[:group]
 *      Specify a user, and optionally a group, to switch to.
 * -v variable
 * -V variable
 *      Add a system variable listed by default.
 * -x   Normally, the time is slewed if the offset is less than the step
 *      threshold, which is 128 ms by default, and stepped if above
 *      the threshold. This option sets the threshold to 600 s, which is
 *      well within the accuracy window to set the clock manually.
 *      Note: since the slew rate of typical Unix kernels is limited
 *      to 0.5 ms/s, each second of adjustment requires an amortization
 *      interval of 2000 s. Thus, an adjustment as much as 600 s
 *      will take almost 14 days to complete. This option can be used
 *      with the -g and -q options. See the tinker command for other options.
 *      Note: The kernel time discipline is disabled with this option.
 */

/* By doing init in a separate function we decrease stack usage
 * in main loop.
 */
static NOINLINE void ntp_init(char **argv)
{
	unsigned opts;
	llist_t *peers;

	srandom(getpid());
	/* tzset(); - why? it's called automatically when needed, no? */

	if (getuid())
		bb_error_msg_and_die("need root privileges");

	peers = NULL;
	opt_complementary = "dd:p::"; /* d: counter, p: list */
	opts = getopt32(argv,
			"ngq" /* compat */
			"p:"IF_FEATURE_NTPD_SERVER("l") /* NOT compat */
			"d" /* compat */
			"46aAbLNx", /* compat, ignored */
			&peers, &G.verbose);
#if ENABLE_FEATURE_NTPD_SERVER
	G.listen_fd = -1;
	if (opts & OPT_l) {
		G.listen_fd = create_and_bind_dgram_or_die(NULL, 123);
		socket_want_pktinfo(G.listen_fd);
		setsockopt(G.listen_fd, IPPROTO_IP, IP_TOS, &const_IPTOS_LOWDELAY, sizeof(const_IPTOS_LOWDELAY));
	}
#endif
	if (opts & OPT_g)
		G.settime = 1;
	while (peers)
		add_peers(llist_pop(&peers));
	if (!(opts & OPT_n)) {
		logmode = LOGMODE_NONE;
		bb_daemonize(DAEMON_DEVNULL_STDIO);
	}

	/* Set some globals */
	{
		int prec = 0;
		int b;
#if 0
		struct timespec	tp;
		/* We can use sys_clock_getres but assuming 10ms tick should be fine */
		clock_getres(CLOCK_REALTIME, &tp);
		tp.tv_sec = 0;
		tp.tv_nsec = 10000000;
		b = 1000000000 / tp.tv_nsec;	/* convert to Hz */
#else
		b = 100; /* b = 1000000000/10000000 = 100 */
#endif
		while (b > 1)
			prec--, b >>= 1;
		G.status.precision = prec;
	}
	G.scale = 1;
	G.firstadj = 1;

	bb_signals((1 << SIGTERM) | (1 << SIGINT), record_signo);
	bb_signals((1 << SIGPIPE) | (1 << SIGHUP), SIG_IGN);
}

int ntpd_main(int argc UNUSED_PARAM, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ntpd_main(int argc UNUSED_PARAM, char **argv)
{
	struct globals g;
	struct pollfd *pfd;
	ntp_peer_t **idx2peer;

	memset(&g, 0, sizeof(g));
	SET_PTR_TO_GLOBALS(&g);

	ntp_init(argv);

	{
		unsigned cnt = g.peer_cnt;
		/* if ENABLE_FEATURE_NTPD_SERVER, + 1 for listen_fd: */
		idx2peer = xzalloc(sizeof(void *) * (cnt + ENABLE_FEATURE_NTPD_SERVER));
		pfd = xzalloc(sizeof(pfd[0]) * (cnt + ENABLE_FEATURE_NTPD_SERVER));
	}

	while (!bb_got_signal) {
		llist_t *item;
		unsigned i, j;
		unsigned sent_cnt, trial_cnt;
		int nfds, timeout;
		time_t cur_time, nextaction;

		/* Nothing between here and poll() blocks for any significant time */

		cur_time = time(NULL);
		nextaction = cur_time + 3600;

		i = 0;
#if ENABLE_FEATURE_NTPD_SERVER
		if (g.listen_fd != -1) {
			pfd[0].fd = g.listen_fd;
			pfd[0].events = POLLIN;
			i++;
		}
#endif
		/* Pass over peer list, send requests, time out on receives */
		sent_cnt = trial_cnt = 0;
		for (item = g.ntp_peers; item != NULL; item = item->link) {
			ntp_peer_t *p = (ntp_peer_t *) item->data;

			if (p->next != 0 && p->next <= cur_time) {
				/* Time to send new req */
				trial_cnt++;
				if (send_query_to_peer(p) == 0)
					sent_cnt++;
			}
			if (p->deadline != 0 && p->deadline <= cur_time) {
				/* Timed out waiting for reply */
				char *addr = xmalloc_sockaddr2dotted_noport(&p->lsa->u.sa);

				timeout = error_interval();
				bb_info_msg("no reply from %s received in time, "
						"next query %ds", addr, timeout);
				if (p->trustlevel >= TRUSTLEVEL_BADPEER) {
					p->trustlevel /= 2;
					if (p->trustlevel < TRUSTLEVEL_BADPEER)
						bb_info_msg("peer %s now invalid", addr);
				}
				free(addr);

				set_next(p, timeout);
			}

			if (p->next != 0 && p->next < nextaction)
				nextaction = p->next;
			if (p->deadline != 0 && p->deadline < nextaction)
				nextaction = p->deadline;

			if (p->state == STATE_QUERY_SENT) {
				/* Wait for reply from this peer */
				pfd[i].fd = p->query.fd;
				pfd[i].events = POLLIN;
				idx2peer[i] = p;
				i++;
			}
		}

		if ((trial_cnt > 0 && sent_cnt == 0) || g.peer_cnt == 0)
			settime(0); /* no good peers, don't wait */

		timeout = nextaction - cur_time;
		if (timeout < 0)
			timeout = 0;

		/* Here we may block */
		if (g.verbose)
			bb_error_msg("entering poll %u secs", timeout);
		nfds = poll(pfd, i, timeout * 1000);
		if (nfds <= 0)
			continue;

		/* Process any received packets */
		j = 0;
#if ENABLE_FEATURE_NTPD_SERVER
		if (g.listen_fd != -1) {
			if (pfd[0].revents /* & (POLLIN|POLLERR)*/) {
				nfds--;
				recv_and_process_client_pkt(/*g.listen_fd*/);
			}
			j = 1;
		}
#endif
		for (; nfds != 0 && j < i; j++) {
			if (pfd[j].revents /* & (POLLIN|POLLERR)*/) {
				nfds--;
				recv_and_process_peer_pkt(idx2peer[j]);
			}
		}
	} /* while (!bb_got_signal) */

	kill_myself_with_sig(bb_got_signal);
}
