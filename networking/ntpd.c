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

#define	INTERVAL_QUERY_NORMAL		30	/* sync to peers every n secs */
#define	INTERVAL_QUERY_PATHETIC		60
#define	INTERVAL_QUERY_AGRESSIVE	5

#define	TRUSTLEVEL_BADPEER		6
#define	TRUSTLEVEL_PATHETIC		2
#define	TRUSTLEVEL_AGRESSIVE		8
#define	TRUSTLEVEL_MAX			10

#define	QSCALE_OFF_MIN			0.05
#define	QSCALE_OFF_MAX			0.50

#define	QUERYTIME_MAX		15	/* single query might take n secs max */
#define	OFFSET_ARRAY_SIZE	8
#define	SETTIME_MIN_OFFSET	180	/* min offset for settime at start */
#define	SETTIME_TIMEOUT		15	/* max seconds to wait with -s */

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

#define	NTP_DIGESTSIZE		16
#define	NTP_MSGSIZE_NOAUTH	48
#define	NTP_MSGSIZE		(NTP_MSGSIZE_NOAUTH + 4 + NTP_DIGESTSIZE)

typedef struct {
	uint8_t status;	/* status of local clock and leap info */
	uint8_t stratum;	/* Stratum level */
	uint8_t ppoll;		/* poll value */
	int8_t precision;
	s_fixedpt_t rootdelay;
	s_fixedpt_t dispersion;
	uint32_t refid;
	l_fixedpt_t reftime;
	l_fixedpt_t orgtime;
	l_fixedpt_t rectime;
	l_fixedpt_t xmttime;
	uint32_t keyid;
	uint8_t digest[NTP_DIGESTSIZE];
} ntp_msg_t;

typedef struct {
	int			fd;
	ntp_msg_t		msg;
	double			xmttime;
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
	LI_MASK		= (3 << 6),

	/* Mode values */
	MODE_RES0	= 0,	/* reserved */
	MODE_SYM_ACT	= 1,	/* symmetric active */
	MODE_SYM_PAS	= 2,	/* symmetric passive */
	MODE_CLIENT	= 3,	/* client */
	MODE_SERVER	= 4,	/* server */
	MODE_BROADCAST	= 5,	/* broadcast */
	MODE_RES1	= 6,	/* reserved for NTP control message */
	MODE_RES2	= 7	/* reserved for private use */
};

#define	JAN_1970	2208988800UL	/* 1970 - 1900 in seconds */

enum client_state {
	STATE_NONE,
	STATE_QUERY_SENT,
	STATE_REPLY_RECEIVED
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
	ntp_status_t		status;
	double			offset;
	double			delay;
	double			error;
	time_t			rcvd;
	uint8_t			good;
} ntp_offset_t;

typedef struct {
	len_and_sockaddr	*lsa;
	ntp_query_t		 query;
	ntp_offset_t		 reply[OFFSET_ARRAY_SIZE];
	ntp_offset_t		 update;
	enum client_state	 state;
	time_t			 next;
	time_t			 deadline;
	uint8_t			 shift;
	uint8_t			 trustlevel;
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
	llist_t		*ntp_peers;
	ntp_status_t	status;
	uint32_t	scale;
	uint8_t		settime;
	uint8_t		firstadj;
	smallint	peer_cnt;
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
gettime(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL); /* never fails */
	return (tv.tv_sec + JAN_1970 + 1.0e-6 * tv.tv_usec);
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
	double	ret;

	lfp.int_partl = ntohl(lfp.int_partl);
	lfp.fractionl = ntohl(lfp.fractionl);
	ret = (double)(lfp.int_partl) + ((double)lfp.fractionl / UINT_MAX);
	return ret;
}

#if ENABLE_FEATURE_NTPD_SERVER
static l_fixedpt_t
d_to_lfp(double d)
{
	l_fixedpt_t	lfp;

	lfp.int_partl = htonl((uint32_t)d);
	lfp.fractionl = htonl((uint32_t)((d - (u_int32_t)d) * UINT_MAX));
	return lfp;
}
#endif

static double
sfp_to_d(s_fixedpt_t sfp)
{
	double	ret;

	sfp.int_parts = ntohs(sfp.int_parts);
	sfp.fractions = ntohs(sfp.fractions);
	ret = (double)(sfp.int_parts) + ((double)sfp.fractions / USHRT_MAX);
	return ret;
}

#if ENABLE_FEATURE_NTPD_SERVER
static s_fixedpt_t
d_to_sfp(double d)
{
	s_fixedpt_t	sfp;

	sfp.int_parts = htons((uint16_t)d);
	sfp.fractions = htons((uint16_t)((d - (u_int16_t)d) * USHRT_MAX));
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
	r = random() % (interval / 10);
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
		ret = sendto(fd, msg, len, 0, to, addrlen);
	} else {
		ret = send_to_from(fd, msg, len, 0, to, from, addrlen);
	}
	if (ret != len) {
		bb_perror_msg("send failed");
		return -1;
	}
	return 0;
}

static int
client_query(ntp_peer_t *p)
{
	if (p->query.fd == -1) {
		p->query.fd = xsocket(p->lsa->u.sa.sa_family, SOCK_DGRAM, 0);
#if ENABLE_FEATURE_IPV6
		if (p->lsa->u.sa.sa_family == AF_INET)
#endif
			setsockopt(p->query.fd, IPPROTO_IP, IP_TOS, &const_IPTOS_LOWDELAY, sizeof(const_IPTOS_LOWDELAY));
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
	p->query.xmttime = gettime();

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
	const ntp_peer_t * const *a;
	const ntp_peer_t * const *b;

	a = aa;
	b = bb;

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

	peers = xzalloc(sizeof(ntp_peer_t *) * offset_cnt);
	for (item = G.ntp_peers; item != NULL; item = item->link) {
		p = (ntp_peer_t *) item->data;
		if (p->trustlevel < TRUSTLEVEL_BADPEER)
			continue;
		peers[i++] = p;
	}

	qsort(peers, offset_cnt, sizeof(ntp_peer_t *), offset_compare);

	if (offset_cnt != 0) {
		if ((offset_cnt & 1) == 0) {
//TODO: try offset_cnt /= 2...
			offset_median =
			    (peers[offset_cnt / 2 - 1]->update.offset +
			    peers[offset_cnt / 2]->update.offset) / 2;
			G.status.rootdelay =
			    (peers[offset_cnt / 2 - 1]->update.delay +
			    peers[offset_cnt / 2]->update.delay) / 2;
			G.status.stratum = MAX(
			    peers[offset_cnt / 2 - 1]->update.status.stratum,
			    peers[offset_cnt / 2]->update.status.stratum);
		} else {
			offset_median = peers[offset_cnt / 2]->update.offset;
			G.status.rootdelay = peers[offset_cnt / 2]->update.delay;
			G.status.stratum = peers[offset_cnt / 2]->update.status.stratum;
		}
		G.status.leap = peers[offset_cnt / 2]->update.status.leap;

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
		G.status.reftime = gettime();
		G.status.stratum++;	/* one more than selected peer */
		G.scale = updated_scale(offset_median);

		G.status.refid4 = peers[offset_cnt / 2]->update.status.refid4;

		lsa = peers[offset_cnt / 2]->lsa;
		G.status.refid =
#if ENABLE_FEATURE_IPV6
			lsa->u.sa.sa_family != AF_INET ?
				G.status.refid4 :
#endif
				lsa->u.sin.sin_addr.s_addr;
	}

	free(peers);

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
client_update(ntp_peer_t *p)
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
	r = random() % MAX(5, interval / 10);
	return (interval + r);
}

static void
client_dispatch(ntp_peer_t *p)
{
	char			 *addr;
	ssize_t			 size;
	ntp_msg_t		 msg;
	double			 T1, T2, T3, T4;
	time_t			 interval;
	ntp_offset_t		*offset;

	addr = xmalloc_sockaddr2dotted_noport(&p->lsa->u.sa);

	size = recvfrom(p->query.fd, &msg, sizeof(msg), 0, NULL, NULL);
	if (size == -1) {
		bb_perror_msg("recvfrom(%s) error", addr);
		if (errno == EHOSTUNREACH || errno == EHOSTDOWN
		 || errno == ENETUNREACH || errno == ENETDOWN
		 || errno == ECONNREFUSED || errno == EADDRNOTAVAIL
		) {
//TODO: always do this?
			set_next(p, error_interval());
			goto bail;
		}
		xfunc_die();
	}

	T4 = gettime();

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
		if (p->trustlevel < TRUSTLEVEL_BADPEER
		 && p->trustlevel + 1 >= TRUSTLEVEL_BADPEER
		) {
			bb_info_msg("peer %s now valid", addr);
		}
		p->trustlevel++;
	}

	bb_info_msg("reply from %s: offset %f delay %f, next query %ds", addr,
			offset->offset, offset->delay, (int) interval);

	client_update(p);
	settime(offset->offset);

	if (++p->shift >= OFFSET_ARRAY_SIZE)
		p->shift = 0;

 bail:
	free(addr);
}

#if ENABLE_FEATURE_NTPD_SERVER
static void
server_dispatch(int fd)
{
	ssize_t			size;
	uint8_t			version;
	double			rectime;
	len_and_sockaddr	*to;
	struct sockaddr		*from;
	ntp_msg_t		query, reply;

	to = get_sock_lsa(G.listen_fd);
	from = xzalloc(to->len);

	size = recv_from_to(fd, &query, sizeof(query), 0, from, &to->u.sa, to->len);
	if (size == -1)
		bb_error_msg_and_die("recv_from_to");
	if (size != NTP_MSGSIZE_NOAUTH && size != NTP_MSGSIZE) {
		char *addr = xmalloc_sockaddr2dotted_noport(from);
		bb_error_msg("malformed packet received from %s", addr);
		free(addr);
		goto bail;
	}

	rectime = gettime();
	version = (query.status & VERSION_MASK) >> 3;

	memset(&reply, 0, sizeof(reply));
	reply.status = G.status.synced ? G.status.leap : LI_ALARM;
	reply.status |= (query.status & VERSION_MASK);
	reply.status |= ((query.status & MODE_MASK) == MODE_CLIENT) ?
			 MODE_SERVER : MODE_SYM_PAS;
	reply.stratum =	G.status.stratum;
	reply.ppoll = query.ppoll;
	reply.precision = G.status.precision;
	reply.rectime = d_to_lfp(rectime);
	reply.reftime = d_to_lfp(G.status.reftime);
	reply.xmttime = d_to_lfp(gettime());
	reply.orgtime = query.xmttime;
	reply.rootdelay = d_to_sfp(G.status.rootdelay);
	reply.refid = (version > 3) ? G.status.refid4 : G.status.refid;

	/* We reply from the address packet was sent to,
	 * this makes to/from look swapped here: */
	sendmsg_wrap(fd, /*from:*/ &to->u.sa, /*to:*/ from, /*addrlen:*/ to->len,
			&reply, size);

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

	tzset();

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
		unsigned new_cnt = g.peer_cnt;
		idx2peer = xzalloc(sizeof(void *) * new_cnt);
		/* if ENABLE_FEATURE_NTPD_SERVER, + 1 for listen_fd: */
		pfd = xzalloc(sizeof(pfd[0]) * (new_cnt + ENABLE_FEATURE_NTPD_SERVER));
	}

	while (!bb_got_signal) {
		llist_t *item;
		unsigned i, j, idx_peers;
		unsigned sent_cnt, trial_cnt;
		int nfds, timeout;
		time_t nextaction;

		nextaction = time(NULL) + 3600;

		i = 0;
#if ENABLE_FEATURE_NTPD_SERVER
		if (g.listen_fd != -1) {
			pfd[0].fd = g.listen_fd;
			pfd[0].events = POLLIN;
			i++;
		}
#endif
		idx_peers = i;
		sent_cnt = trial_cnt = 0;
		for (item = g.ntp_peers; item != NULL; item = item->link) {
			ntp_peer_t *p = (ntp_peer_t *) item->data;

			if (p->next > 0 && p->next <= time(NULL)) {
				trial_cnt++;
				if (client_query(p) == 0)
					sent_cnt++;
			}
			if (p->next > 0 && p->next < nextaction)
				nextaction = p->next;
			if (p->deadline > 0 && p->deadline < nextaction)
				nextaction = p->deadline;

			if (p->deadline > 0 && p->deadline <= time(NULL)) {
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

			if (p->state == STATE_QUERY_SENT) {
				pfd[i].fd = p->query.fd;
				pfd[i].events = POLLIN;
				idx2peer[i - idx_peers] = p;
				i++;
			}
		}

		if ((trial_cnt > 0 && sent_cnt == 0) || g.peer_cnt == 0)
			settime(0);	/* no good peers, don't wait */

		timeout = nextaction - time(NULL);
		if (timeout < 0)
			timeout = 0;

		if (g.verbose)
			bb_error_msg("entering poll %u secs", timeout);
		nfds = poll(pfd, i, timeout * 1000);

		j = 0;
#if ENABLE_FEATURE_NTPD_SERVER
		for (; nfds > 0 && j < idx_peers; j++) {
			if (pfd[j].revents & (POLLIN|POLLERR)) {
				nfds--;
				server_dispatch(pfd[j].fd);
			}
		}
#endif
		for (; nfds > 0 && j < i; j++) {
			if (pfd[j].revents & (POLLIN|POLLERR)) {
				nfds--;
				client_dispatch(idx2peer[j - idx_peers]);
			}
		}
	} /* while (!bb_got_signal) */

	kill_myself_with_sig(bb_got_signal);
}
