/*
 * ipcs.c -- provides information on allocated ipc resources.
 *
 * 01 Sept 2004 - Rodney Radford <rradford@mindspring.com>
 * Adapted for busybox from util-linux-2.12a.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

/* X/OPEN tells us to use <sys/{types,ipc,sem}.h> for semctl() */
/* X/OPEN tells us to use <sys/{types,ipc,msg}.h> for msgctl() */
/* X/OPEN tells us to use <sys/{types,ipc,shm}.h> for shmctl() */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "busybox.h"

/*-------------------------------------------------------------------*/
/* SHM_DEST and SHM_LOCKED are defined in kernel headers,
   but inside #ifdef __KERNEL__ ... #endif */
#ifndef SHM_DEST
/* shm_mode upper byte flags */
#define SHM_DEST        01000   /* segment will be destroyed on last detach */
#define SHM_LOCKED      02000   /* segment will not be swapped */
#endif

/* For older kernels the same holds for the defines below */
#ifndef MSG_STAT
#define MSG_STAT	11
#define MSG_INFO	12
#endif

#ifndef SHM_STAT
#define SHM_STAT        13
#define SHM_INFO        14
struct shm_info {
	 int   used_ids;
	 ulong shm_tot; /* total allocated shm */
	 ulong shm_rss; /* total resident shm */
	 ulong shm_swp; /* total swapped shm */
	 ulong swap_attempts;
	 ulong swap_successes;
};
#endif

#ifndef SEM_STAT
#define SEM_STAT	18
#define SEM_INFO	19
#endif

/* Some versions of libc only define IPC_INFO when __USE_GNU is defined. */
#ifndef IPC_INFO
#define IPC_INFO        3
#endif
/*-------------------------------------------------------------------*/

/* The last arg of semctl is a union semun, but where is it defined?
   X/OPEN tells us to define it ourselves, but until recently
   Linux include files would also define it. */
#if defined (__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif

/* X/OPEN (Jan 1987) does not define fields key, seq in struct ipc_perm;
   libc 4/5 does not mention struct ipc_term at all, but includes
   <linux/ipc.h>, which defines a struct ipc_perm with such fields.
   glibc-1.09 has no support for sysv ipc.
   glibc 2 uses __key, __seq */
#if defined (__GNU_LIBRARY__) && __GNU_LIBRARY__ > 1
#define KEY __key
#else
#define KEY key
#endif

#define LIMITS 1
#define STATUS 2
#define CREATOR 3
#define TIME 4
#define PID 5


static void print_perms (int id, struct ipc_perm *ipcp) {
	struct passwd *pw;
	struct group *gr;

	bb_printf ("%-10d %-10o", id, ipcp->mode & 0777);

	if ((pw = getpwuid(ipcp->cuid)))
		bb_printf(" %-10s", pw->pw_name);
	else
		bb_printf(" %-10d", ipcp->cuid);
	if ((gr = getgrgid(ipcp->cgid)))
		bb_printf(" %-10s", gr->gr_name);
	else
		bb_printf(" %-10d", ipcp->cgid);

	if ((pw = getpwuid(ipcp->uid)))
		bb_printf(" %-10s", pw->pw_name);
	else
		bb_printf(" %-10d", ipcp->uid);
	if ((gr = getgrgid(ipcp->gid)))
		bb_printf(" %-10s\n", gr->gr_name);
	else
		bb_printf(" %-10d\n", ipcp->gid);
}


static void do_shm (char format)
{
	int maxid, shmid, id;
	struct shmid_ds shmseg;
	struct shm_info shm_info;
	struct shminfo shminfo;
	struct ipc_perm *ipcp = &shmseg.shm_perm;
	struct passwd *pw;

	maxid = shmctl (0, SHM_INFO, (struct shmid_ds *) (void *) &shm_info);
	if (maxid < 0) {
		bb_printf ("kernel not configured for shared memory\n");
		return;
	}

	switch (format) {
	case LIMITS:
		bb_printf ("------ Shared Memory Limits --------\n");
		if ((shmctl (0, IPC_INFO, (struct shmid_ds *) (void *) &shminfo)) < 0 )
			return;
		/* glibc 2.1.3 and all earlier libc's have ints as fields
		   of struct shminfo; glibc 2.1.91 has unsigned long; ach */
		bb_printf ("max number of segments = %lu\n"
			"max seg size (kbytes) = %lu\n"
			"max total shared memory (pages) = %lu\n"
			"min seg size (bytes) = %lu\n",
			(unsigned long) shminfo.shmmni,
			(unsigned long) (shminfo.shmmax >> 10),
			(unsigned long) shminfo.shmall,
			(unsigned long) shminfo.shmmin);
		return;

	case STATUS:
		bb_printf ("------ Shared Memory Status --------\n"
			"segments allocated %d\n"
			"pages allocated %ld\n"
			"pages resident  %ld\n"
			"pages swapped   %ld\n"
			"Swap performance: %ld attempts\t %ld successes\n",
			shm_info.used_ids,
			shm_info.shm_tot,
			shm_info.shm_rss,
			shm_info.shm_swp,
			shm_info.swap_attempts, shm_info.swap_successes);
		return;

	case CREATOR:
		bb_printf ("------ Shared Memory Segment Creators/Owners --------\n"
			"%-10s %-10s %-10s %-10s %-10s %-10s\n",
			"shmid","perms","cuid","cgid","uid","gid");
		break;

	case TIME:
		bb_printf ("------ Shared Memory Attach/Detach/Change Times --------\n"
			"%-10s %-10s %-20s %-20s %-20s\n",
			"shmid","owner","attached","detached","changed");
		break;

	case PID:
		bb_printf ("------ Shared Memory Creator/Last-op --------\n"
			"%-10s %-10s %-10s %-10s\n",
			"shmid","owner","cpid","lpid");
		break;

	default:
		bb_printf ("------ Shared Memory Segments --------\n"
			"%-10s %-10s %-10s %-10s %-10s %-10s %-12s\n",
			"key","shmid","owner","perms","bytes","nattch","status");
		break;
	}

	for (id = 0; id <= maxid; id++) {
		shmid = shmctl (id, SHM_STAT, &shmseg);
		if (shmid < 0)
			continue;
		if (format == CREATOR)  {
			print_perms (shmid, ipcp);
			continue;
		}
		pw = getpwuid(ipcp->uid);
		switch (format) {
		case TIME:
			if (pw)
				bb_printf ("%-10d %-10.10s", shmid, pw->pw_name);
			else
				bb_printf ("%-10d %-10d", shmid, ipcp->uid);
			/* ctime uses static buffer: use separate calls */
			bb_printf("  %-20.16s", shmseg.shm_atime
				   ? ctime(&shmseg.shm_atime) + 4 : "Not set");
			bb_printf(" %-20.16s", shmseg.shm_dtime
				   ? ctime(&shmseg.shm_dtime) + 4 : "Not set");
			bb_printf(" %-20.16s\n", shmseg.shm_ctime
				   ? ctime(&shmseg.shm_ctime) + 4 : "Not set");
			break;
		case PID:
			if (pw)
				bb_printf ("%-10d %-10.10s", shmid, pw->pw_name);
			else
				bb_printf ("%-10d %-10d", shmid, ipcp->uid);
			bb_printf (" %-10d %-10d\n",
				shmseg.shm_cpid, shmseg.shm_lpid);
			break;

		default:
				bb_printf("0x%08x ",ipcp->KEY );
			if (pw)
				bb_printf ("%-10d %-10.10s", shmid, pw->pw_name);
			else
				bb_printf ("%-10d %-10d", shmid, ipcp->uid);
			bb_printf ("%-10o %-10lu %-10ld %-6s %-6s\n",
				ipcp->mode & 0777,
				/*
				 * earlier: int, Austin has size_t
				 */
				(unsigned long) shmseg.shm_segsz,
				/*
				 * glibc-2.1.3 and earlier has unsigned short;
				 * Austin has shmatt_t
				 */
				(long) shmseg.shm_nattch,
				ipcp->mode & SHM_DEST ? "dest" : " ",
				ipcp->mode & SHM_LOCKED ? "locked" : " ");
			break;
		}
	}
	return;
}


static void do_sem (char format)
{
	int maxid, semid, id;
	struct semid_ds semary;
	struct seminfo seminfo;
	struct ipc_perm *ipcp = &semary.sem_perm;
	struct passwd *pw;
	union semun arg;

	arg.array = (ushort *)  (void *) &seminfo;
	maxid = semctl (0, 0, SEM_INFO, arg);
	if (maxid < 0) {
		bb_printf ("kernel not configured for semaphores\n");
		return;
	}

	switch (format) {
	case LIMITS:
		bb_printf ("------ Semaphore Limits --------\n");
		arg.array = (ushort *) (void *) &seminfo; /* damn union */
		if ((semctl (0, 0, IPC_INFO, arg)) < 0 )
			return;
		bb_printf ("max number of arrays = %d\n"
			"max semaphores per array = %d\n"
			"max semaphores system wide = %d\n"
			"max ops per semop call = %d\n"
			"semaphore max value = %d\n",
			seminfo.semmni,
			seminfo.semmsl,
			seminfo.semmns,
			seminfo.semopm,
			seminfo.semvmx);
		return;

	case STATUS:
		bb_printf ("------ Semaphore Status --------\n"
			"used arrays = %d\n"
			"allocated semaphores = %d\n",
			seminfo.semusz,
			seminfo.semaem);
		return;

	case CREATOR:
		bb_printf ("------ Semaphore Arrays Creators/Owners --------\n"
			"%-10s %-10s %-10s %-10s %-10s %-10s\n",
			"semid","perms","cuid","cgid","uid","gid");
		break;

	case TIME:
		bb_printf ("------ Shared Memory Operation/Change Times --------\n"
			"%-8s %-10s %-26.24s %-26.24s\n",
			"shmid","owner","last-op","last-changed");
		break;

	case PID:
		break;

	default:
		bb_printf ("------ Semaphore Arrays --------\n"
			"%-10s %-10s %-10s %-10s %-10s\n",
			"key","semid","owner","perms","nsems");
		break;
	}

	for (id = 0; id <= maxid; id++) {
		arg.buf = (struct semid_ds *) &semary;
		semid = semctl (id, 0, SEM_STAT, arg);
		if (semid < 0)
			continue;
		if (format == CREATOR)  {
			print_perms (semid, ipcp);
			continue;
		}
		pw = getpwuid(ipcp->uid);
		switch (format) {
		case TIME:
			if (pw)
				bb_printf ("%-8d %-10.10s", semid, pw->pw_name);
			else
				bb_printf ("%-8d %-10d", semid, ipcp->uid);
			bb_printf ("  %-26.24s", semary.sem_otime
				? ctime(&semary.sem_otime) : "Not set");
			bb_printf (" %-26.24s\n", semary.sem_ctime
				? ctime(&semary.sem_ctime) : "Not set");
			break;
		case PID:
			break;

		default:
				bb_printf("0x%08x ", ipcp->KEY);
			if (pw)
				bb_printf ("%-10d %-10.9s", semid, pw->pw_name);
			else
				bb_printf ("%-10d %-9d", semid, ipcp->uid);
					bb_printf ("%-10o %-10ld\n",
				ipcp->mode & 0777,
				/*
				 * glibc-2.1.3 and earlier has unsigned short;
				 * glibc-2.1.91 has variation between
				 * unsigned short and unsigned long
				 * Austin prescribes unsigned short.
				 */
				(long) semary.sem_nsems);
			break;
		}
	}
}


static void do_msg (char format)
{
	int maxid, msqid, id;
	struct msqid_ds msgque;
	struct msginfo msginfo;
	struct ipc_perm *ipcp = &msgque.msg_perm;
	struct passwd *pw;

	maxid = msgctl (0, MSG_INFO, (struct msqid_ds *) (void *) &msginfo);
	if (maxid < 0) {
		bb_printf ("kernel not configured for message queues\n");
		return;
	}

	switch (format) {
	case LIMITS:
		if ((msgctl (0, IPC_INFO, (struct msqid_ds *) (void *) &msginfo)) < 0 )
			return;
		bb_printf ("------ Messages: Limits --------\n"
			"max queues system wide = %d\n"
			"max size of message (bytes) = %d\n"
			"default max size of queue (bytes) = %d\n",
			msginfo.msgmni,
			msginfo.msgmax,
			msginfo.msgmnb);
		return;

	case STATUS:
		bb_printf ("------ Messages: Status --------\n"
			"allocated queues = %d\n"
			"used headers = %d\n"
			"used space = %d bytes\n",
			msginfo.msgpool,
			msginfo.msgmap,
			msginfo.msgtql);
		return;

	case CREATOR:
		bb_printf ("------ Message Queues: Creators/Owners --------\n"
			"%-10s %-10s %-10s %-10s %-10s %-10s\n",
			"msqid","perms","cuid","cgid","uid","gid");
		break;

	case TIME:
		bb_printf ("------ Message Queues Send/Recv/Change Times --------\n"
			"%-8s %-10s %-20s %-20s %-20s\n",
			"msqid","owner","send","recv","change");
		break;

	case PID:
		bb_printf ("------ Message Queues PIDs --------\n"
			"%-10s %-10s %-10s %-10s\n",
			"msqid","owner","lspid","lrpid");
		break;

	default:
		bb_printf ("------ Message Queues --------\n"
			"%-10s %-10s %-10s %-10s %-12s %-12s\n",
			"key","msqid","owner","perms","used-bytes","messages");
		break;
	}

	for (id = 0; id <= maxid; id++) {
		msqid = msgctl (id, MSG_STAT, &msgque);
		if (msqid < 0)
			continue;
		if (format == CREATOR)  {
			print_perms (msqid, ipcp);
			continue;
		}
		pw = getpwuid(ipcp->uid);
		switch (format) {
		case TIME:
			if (pw)
				bb_printf ("%-8d %-10.10s", msqid, pw->pw_name);
			else
				bb_printf ("%-8d %-10d", msqid, ipcp->uid);
			bb_printf (" %-20.16s", msgque.msg_stime
				? ctime(&msgque.msg_stime) + 4 : "Not set");
			bb_printf (" %-20.16s", msgque.msg_rtime
				? ctime(&msgque.msg_rtime) + 4 : "Not set");
			bb_printf (" %-20.16s\n", msgque.msg_ctime
				? ctime(&msgque.msg_ctime) + 4 : "Not set");
			break;
		case PID:
			if (pw)
				bb_printf ("%-8d %-10.10s", msqid, pw->pw_name);
			else
				bb_printf ("%-8d %-10d", msqid, ipcp->uid);
			bb_printf ("  %5d     %5d\n",
				msgque.msg_lspid, msgque.msg_lrpid);
			break;

		default:
				bb_printf( "0x%08x ",ipcp->KEY );
			if (pw)
				bb_printf ("%-10d %-10.10s", msqid, pw->pw_name);
			else
				bb_printf ("%-10d %-10d", msqid, ipcp->uid);
					bb_printf (" %-10o %-12ld %-12ld\n",
				ipcp->mode & 0777,
				/*
				 * glibc-2.1.3 and earlier has unsigned short;
				 * glibc-2.1.91 has variation between
				 * unsigned short, unsigned long
				 * Austin has msgqnum_t
				 */
				(long) msgque.msg_cbytes,
				(long) msgque.msg_qnum);
			break;
		}
	}
	return;
}


static void print_shm (int shmid)
{
	struct shmid_ds shmds;
	struct ipc_perm *ipcp = &shmds.shm_perm;

	if (shmctl (shmid, IPC_STAT, &shmds) == -1) {
		perror ("shmctl ");
		return;
	}

	bb_printf ("\nShared memory Segment shmid=%d\n"
		"uid=%d\tgid=%d\tcuid=%d\tcgid=%d\n"
		"mode=%#o\taccess_perms=%#o\n"
		"bytes=%ld\tlpid=%d\tcpid=%d\tnattch=%ld\n"
		"att_time=%-26.24s\n"
		"det_time=%-26.24s\n"
		"change_time=%-26.24s\n"
		"\n",
		shmid,
		ipcp->uid, ipcp->gid, ipcp->cuid, ipcp->cgid,
		ipcp->mode, ipcp->mode & 0777,
		(long) shmds.shm_segsz, shmds.shm_lpid, shmds.shm_cpid,
		(long) shmds.shm_nattch,
		shmds.shm_atime ? ctime (&shmds.shm_atime) : "Not set",
		shmds.shm_dtime ? ctime (&shmds.shm_dtime) : "Not set",
		ctime (&shmds.shm_ctime));
	return;
}


static void print_msg (int msqid)
{
	struct msqid_ds buf;
	struct ipc_perm *ipcp = &buf.msg_perm;

	if (msgctl (msqid, IPC_STAT, &buf) == -1) {
		perror ("msgctl ");
		return;
	}

	bb_printf ("\nMessage Queue msqid=%d\n"
		"uid=%d\tgid=%d\tcuid=%d\tcgid=%d\tmode=%#o\n"
		"cbytes=%ld\tqbytes=%ld\tqnum=%ld\tlspid=%d\tlrpid=%d\n"
		"send_time=%-26.24s\n"
		"rcv_time=%-26.24s\n"
		"change_time=%-26.24s\n"
		"\n",
		msqid,
		ipcp->uid, ipcp->gid, ipcp->cuid, ipcp->cgid, ipcp->mode,
		/*
		 * glibc-2.1.3 and earlier has unsigned short;
		 * glibc-2.1.91 has variation between
		 * unsigned short, unsigned long
		 * Austin has msgqnum_t (for msg_qbytes)
		 */
		(long) buf.msg_cbytes, (long) buf.msg_qbytes,
		(long) buf.msg_qnum, buf.msg_lspid, buf.msg_lrpid,
		buf.msg_stime ? ctime (&buf.msg_stime) : "Not set",
		buf.msg_rtime ? ctime (&buf.msg_rtime) : "Not set",
		buf.msg_ctime ? ctime (&buf.msg_ctime) : "Not set");
	return;
}

static void print_sem (int semid)
{
	struct semid_ds semds;
	struct ipc_perm *ipcp = &semds.sem_perm;
	union semun arg;
	unsigned int i;

	arg.buf = &semds;
	if (semctl (semid, 0, IPC_STAT, arg) < 0) {
		perror ("semctl ");
		return;
	}

	bb_printf ("\nSemaphore Array semid=%d\n"
		"uid=%d\t gid=%d\t cuid=%d\t cgid=%d\n"
		"mode=%#o, access_perms=%#o\n"
		"nsems = %ld\n"
		"otime = %-26.24s\n"
		"ctime = %-26.24s\n"
		"%-10s %-10s %-10s %-10s %-10s\n",
		semid,
		ipcp->uid, ipcp->gid, ipcp->cuid, ipcp->cgid,
		ipcp->mode, ipcp->mode & 0777,
		(long) semds.sem_nsems,
		semds.sem_otime ? ctime (&semds.sem_otime) : "Not set",
		ctime (&semds.sem_ctime),
		"semnum","value","ncount","zcount","pid");

	arg.val = 0;
	for (i=0; i < semds.sem_nsems; i++) {
		int val, ncnt, zcnt, pid;
		val = semctl (semid, i, GETVAL, arg);
		ncnt = semctl (semid, i, GETNCNT, arg);
		zcnt = semctl (semid, i, GETZCNT, arg);
		pid = semctl (semid, i, GETPID, arg);
		if (val < 0 || ncnt < 0 || zcnt < 0 || pid < 0) {
			perror ("semctl ");
			bb_fflush_stdout_and_exit (1);
		}
		bb_printf ("%-10d %-10d %-10d %-10d %-10d\n",
			i, val, ncnt, zcnt, pid);
	}
	bb_printf ("\n");
	return;
}

int ipcs_main (int argc, char **argv) {
	int opt, msg = 0, sem = 0, shm = 0, id=0, print=0;
	char format = 0;
	char options[] = "atclupsmqi:ih?";

	while ((opt = getopt (argc, argv, options)) != -1) {
		switch (opt) {
		case 'i':
			id = atoi (optarg);
			print = 1;
			break;
		case 'a':
			msg = shm = sem = 1;
			break;
		case 'q':
			msg = 1;
			break;
		case 's':
			sem = 1;
			break;
		case 'm':
			shm = 1;
			break;
		case 't':
			format = TIME;
			break;
		case 'c':
			format = CREATOR;
			break;
		case 'p':
			format = PID;
			break;
		case 'l':
			format = LIMITS;
			break;
		case 'u':
			format = STATUS;
			break;
		case 'h':
		case '?':
			bb_show_usage();
			bb_fflush_stdout_and_exit (0);
		}
	}

	if  (print) {
		if (shm) {
			print_shm (id);
			bb_fflush_stdout_and_exit (0);
		}
		if (sem) {
			print_sem (id);
			bb_fflush_stdout_and_exit (0);
		}
		if (msg) {
			print_msg (id);
			bb_fflush_stdout_and_exit (0);
		}
		bb_show_usage();
		bb_fflush_stdout_and_exit (0);
	}

	if ( !shm && !msg && !sem)
		msg = sem = shm = 1;
	bb_printf ("\n");

	if (shm) {
		do_shm (format);
		bb_printf ("\n");
	}
	if (sem) {
		do_sem (format);
		bb_printf ("\n");
	}
	if (msg) {
		do_msg (format);
		bb_printf ("\n");
	}
	return 0;
}

