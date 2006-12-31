/* vi: set sw=4 ts=4: */
#ifndef __LINUX_PKT_SCHED_H
#define __LINUX_PKT_SCHED_H

/* Logical priority bands not depending on specific packet scheduler.
   Every scheduler will map them to real traffic classes, if it has
   no more precise mechanism to classify packets.

   These numbers have no special meaning, though their coincidence
   with obsolete IPv6 values is not occasional :-). New IPv6 drafts
   preferred full anarchy inspired by diffserv group.

   Note: TC_PRIO_BESTEFFORT does not mean that it is the most unhappy
   class, actually, as rule it will be handled with more care than
   filler or even bulk.
 */

//#include <asm/types.h>

#define TC_PRIO_BESTEFFORT		0
#define TC_PRIO_FILLER			1
#define TC_PRIO_BULK			2
#define TC_PRIO_INTERACTIVE_BULK	4
#define TC_PRIO_INTERACTIVE		6
#define TC_PRIO_CONTROL			7

#define TC_PRIO_MAX			15

/* Generic queue statistics, available for all the elements.
   Particular schedulers may have also their private records.
 */

struct tc_stats
{
	uint64_t	bytes;		/* Nnmber of enqueued bytes */
	uint32_t	packets;	/* Number of enqueued packets */
	uint32_t	drops;		/* Packets dropped because of lack of resources */
	uint32_t	overlimits;	/* Number of throttle events when this
					 * flow goes out of allocated bandwidth */
	uint32_t	bps;		/* Current flow byte rate */
	uint32_t	pps;		/* Current flow packet rate */
	uint32_t	qlen;
	uint32_t	backlog;
#ifdef __KERNEL__
	spinlock_t *lock;
#endif
};

struct tc_estimator
{
	char		interval;
	unsigned char	ewma_log;
};

/* "Handles"
   ---------

    All the traffic control objects have 32bit identifiers, or "handles".

    They can be considered as opaque numbers from user API viewpoint,
    but actually they always consist of two fields: major and
    minor numbers, which are interpreted by kernel specially,
    that may be used by applications, though not recommended.

    F.e. qdisc handles always have minor number equal to zero,
    classes (or flows) have major equal to parent qdisc major, and
    minor uniquely identifying class inside qdisc.

    Macros to manipulate handles:
 */

#define TC_H_MAJ_MASK (0xFFFF0000U)
#define TC_H_MIN_MASK (0x0000FFFFU)
#define TC_H_MAJ(h) ((h)&TC_H_MAJ_MASK)
#define TC_H_MIN(h) ((h)&TC_H_MIN_MASK)
#define TC_H_MAKE(maj,min) (((maj)&TC_H_MAJ_MASK)|((min)&TC_H_MIN_MASK))

#define TC_H_UNSPEC	(0U)
#define TC_H_ROOT	(0xFFFFFFFFU)
#define TC_H_INGRESS    (0xFFFFFFF1U)

struct tc_ratespec
{
	unsigned char	cell_log;
	unsigned char	__reserved;
	unsigned short	feature;
	short		addend;
	unsigned short	mpu;
	uint32_t	rate;
};

/* FIFO section */

struct tc_fifo_qopt
{
	uint32_t	limit;	/* Queue length: bytes for bfifo, packets for pfifo */
};

/* PRIO section */

#define TCQ_PRIO_BANDS	16

struct tc_prio_qopt
{
	int	bands;			/* Number of bands */
	uint8_t	priomap[TC_PRIO_MAX+1];	/* Map: logical priority -> PRIO band */
};

/* CSZ section */

struct tc_csz_qopt
{
	int		flows;		/* Maximal number of guaranteed flows */
	unsigned char	R_log;		/* Fixed point position for round number */
	unsigned char	delta_log;	/* Log of maximal managed time interval */
	uint8_t		priomap[TC_PRIO_MAX+1];	/* Map: logical priority -> CSZ band */
};

struct tc_csz_copt
{
	struct tc_ratespec slice;
	struct tc_ratespec rate;
	struct tc_ratespec peakrate;
	uint32_t limit;
	uint32_t buffer;
	uint32_t mtu;
};

enum
{
	TCA_CSZ_UNSPEC,
	TCA_CSZ_PARMS,
	TCA_CSZ_RTAB,
	TCA_CSZ_PTAB,
};

/* TBF section */

struct tc_tbf_qopt
{
	struct tc_ratespec rate;
	struct tc_ratespec peakrate;
	uint32_t limit;
	uint32_t buffer;
	uint32_t mtu;
};

enum
{
	TCA_TBF_UNSPEC,
	TCA_TBF_PARMS,
	TCA_TBF_RTAB,
	TCA_TBF_PTAB,
};


/* TEQL section */

/* TEQL does not require any parameters */

/* SFQ section */

struct tc_sfq_qopt
{
	unsigned	quantum;	/* Bytes per round allocated to flow */
	int		perturb_period;	/* Period of hash perturbation */
	uint32_t	limit;		/* Maximal packets in queue */
	unsigned	divisor;	/* Hash divisor  */
	unsigned	flows;		/* Maximal number of flows  */
};

/*
 *  NOTE: limit, divisor and flows are hardwired to code at the moment.
 *
 *	limit=flows=128, divisor=1024;
 *
 *	The only reason for this is efficiency, it is possible
 *	to change these parameters in compile time.
 */

/* RED section */

enum
{
	TCA_RED_UNSPEC,
	TCA_RED_PARMS,
	TCA_RED_STAB,
};

struct tc_red_qopt
{
	uint32_t	limit;		/* HARD maximal queue length (bytes)	*/
	uint32_t	qth_min;	/* Min average length threshold (bytes) */
	uint32_t	qth_max;	/* Max average length threshold (bytes) */
	unsigned char   Wlog;		/* log(W)		*/
	unsigned char   Plog;		/* log(P_max/(qth_max-qth_min))	*/
	unsigned char   Scell_log;	/* cell size for idle damping */
	unsigned char	flags;
#define TC_RED_ECN	1
};

struct tc_red_xstats
{
	uint32_t        early;          /* Early drops */
	uint32_t        pdrop;          /* Drops due to queue limits */
	uint32_t        other;          /* Drops due to drop() calls */
	uint32_t        marked;         /* Marked packets */
};

/* GRED section */

#define MAX_DPs 16

enum
{
       TCA_GRED_UNSPEC,
       TCA_GRED_PARMS,
       TCA_GRED_STAB,
       TCA_GRED_DPS,
};

#define TCA_SET_OFF TCA_GRED_PARMS
struct tc_gred_qopt
{
       uint32_t        limit;   /* HARD maximal queue length (bytes) */
       uint32_t        qth_min; /* Min average length threshold (bytes) */
       uint32_t        qth_max; /* Max average length threshold (bytes) */
       uint32_t        DP;      /* upto 2^32 DPs */
       uint32_t        backlog;
       uint32_t        qave;
       uint32_t        forced;
       uint32_t        early;
       uint32_t        other;
       uint32_t        pdrop;

       unsigned char   Wlog;           /* log(W) */
       unsigned char   Plog;           /* log(P_max/(qth_max-qth_min)) */
       unsigned char   Scell_log;      /* cell size for idle damping */
       uint8_t         prio;           /* prio of this VQ */
       uint32_t	packets;
       uint32_t	bytesin;
};
/* gred setup */
struct tc_gred_sopt
{
       uint32_t        DPs;
       uint32_t        def_DP;
       uint8_t         grio;
};

/* HTB section */
#define TC_HTB_NUMPRIO		4
#define TC_HTB_MAXDEPTH		4

struct tc_htb_opt
{
	struct tc_ratespec rate;
	struct tc_ratespec ceil;
	uint32_t buffer;
	uint32_t cbuffer;
	uint32_t quantum;	/* out only */
	uint32_t level;		/* out only */
	uint8_t  prio;
	uint8_t  injectd;	/* inject class distance */
	uint8_t  pad[2];
};
struct tc_htb_glob
{
	uint32_t rate2quantum;	/* bps->quantum divisor */
	uint32_t defcls;		/* default class number */
	uint32_t use_dcache;	/* use dequeue cache ? */
	uint32_t debug;		/* debug flags */


	/* stats */
	uint32_t deq_rate;	/* dequeue rate */
	uint32_t utilz;	/* dequeue utilization */
	uint32_t trials;	/* deq_prio trials per dequeue */
	uint32_t dcache_hits;
	uint32_t direct_pkts; /* count of non shapped packets */
};
enum
{
	TCA_HTB_UNSPEC,
	TCA_HTB_PARMS,
	TCA_HTB_INIT,
	TCA_HTB_CTAB,
	TCA_HTB_RTAB,
};
struct tc_htb_xstats
{
	uint32_t lends;
	uint32_t borrows;
	uint32_t giants;	/* too big packets (rate will not be accurate) */
	uint32_t injects;	/* how many times leaf used injected bw */
	uint32_t tokens;
	uint32_t ctokens;
};

/* CBQ section */

#define TC_CBQ_MAXPRIO		8
#define TC_CBQ_MAXLEVEL		8
#define TC_CBQ_DEF_EWMA		5

struct tc_cbq_lssopt
{
	unsigned char	change;
	unsigned char	flags;
#define TCF_CBQ_LSS_BOUNDED	1
#define TCF_CBQ_LSS_ISOLATED	2
	unsigned char	ewma_log;
	unsigned char	level;
#define TCF_CBQ_LSS_FLAGS	1
#define TCF_CBQ_LSS_EWMA	2
#define TCF_CBQ_LSS_MAXIDLE	4
#define TCF_CBQ_LSS_MINIDLE	8
#define TCF_CBQ_LSS_OFFTIME	0x10
#define TCF_CBQ_LSS_AVPKT	0x20
	uint32_t	maxidle;
	uint32_t	minidle;
	uint32_t	offtime;
	uint32_t	avpkt;
};

struct tc_cbq_wrropt
{
	unsigned char	flags;
	unsigned char	priority;
	unsigned char	cpriority;
	unsigned char	__reserved;
	uint32_t	allot;
	uint32_t	weight;
};

struct tc_cbq_ovl
{
	unsigned char	strategy;
#define	TC_CBQ_OVL_CLASSIC	0
#define	TC_CBQ_OVL_DELAY	1
#define	TC_CBQ_OVL_LOWPRIO	2
#define	TC_CBQ_OVL_DROP		3
#define	TC_CBQ_OVL_RCLASSIC	4
	unsigned char	priority2;
	uint32_t	penalty;
};

struct tc_cbq_police
{
	unsigned char	police;
	unsigned char	__res1;
	unsigned short	__res2;
};

struct tc_cbq_fopt
{
	uint32_t	split;
	uint32_t	defmap;
	uint32_t	defchange;
};

struct tc_cbq_xstats
{
	uint32_t	borrows;
	uint32_t	overactions;
	int32_t		avgidle;
	int32_t		undertime;
};

enum
{
	TCA_CBQ_UNSPEC,
	TCA_CBQ_LSSOPT,
	TCA_CBQ_WRROPT,
	TCA_CBQ_FOPT,
	TCA_CBQ_OVL_STRATEGY,
	TCA_CBQ_RATE,
	TCA_CBQ_RTAB,
	TCA_CBQ_POLICE,
};

#define TCA_CBQ_MAX	TCA_CBQ_POLICE

/* dsmark section */

enum {
	TCA_DSMARK_UNSPEC,
	TCA_DSMARK_INDICES,
	TCA_DSMARK_DEFAULT_INDEX,
	TCA_DSMARK_SET_TC_INDEX,
	TCA_DSMARK_MASK,
	TCA_DSMARK_VALUE
};

#define TCA_DSMARK_MAX TCA_DSMARK_VALUE

/* ATM  section */

enum {
	TCA_ATM_UNSPEC,
	TCA_ATM_FD,		/* file/socket descriptor */
	TCA_ATM_PTR,		/* pointer to descriptor - later */
	TCA_ATM_HDR,		/* LL header */
	TCA_ATM_EXCESS,		/* excess traffic class (0 for CLP)  */
	TCA_ATM_ADDR,		/* PVC address (for output only) */
	TCA_ATM_STATE		/* VC state (ATM_VS_*; for output only) */
};

#define TCA_ATM_MAX	TCA_ATM_STATE

#endif
