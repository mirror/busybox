/* vi: set sw=4 ts=4: */
/*
 * circular buffer syslog implementation for busybox
 *
 * Copyright (C) 2000 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <sys/ipc.h>
//#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
//#include <signal.h>
//#include <setjmp.h>

#define DEBUG 0

static const long KEY_ID = 0x414e4547; /* "GENA" */

static struct shbuf_ds {
	int32_t size;		// size of data written
	int32_t head;		// start of message list
	int32_t tail;		// end of message list
	char data[1];		// data/messages
} *buf;				// shared memory pointer


// Semaphore operation structures
static struct sembuf SMrup[1] = {{0, -1, IPC_NOWAIT | SEM_UNDO}}; // set SMrup
static struct sembuf SMrdn[2] = {{1, 0}, {0, +1, SEM_UNDO}}; // set SMrdn

static int log_shmid = -1;	// ipc shared memory id
static int log_semid = -1;	// ipc semaphore id

static void error_exit(const char *str) ATTRIBUTE_NORETURN;
static void error_exit(const char *str)
{
	//release all acquired resources
	if (log_shmid != -1)
		shmdt(buf);
	bb_perror_msg_and_die(str);
}

/*
 * sem_up - up()'s a semaphore.
 */
static void sem_up(int semid)
{
	if (semop(semid, SMrup, 1) == -1)
		error_exit("semop[SMrup]");
}

/*
 * sem_down - down()'s a semaphore
 */
static void sem_down(int semid)
{
	if (semop(semid, SMrdn, 2) == -1)
		error_exit("semop[SMrdn]");
}

static void interrupted(int sig)
{
	signal(SIGINT, SIG_IGN);
	shmdt(buf);
	exit(0);
}

int logread_main(int argc, char **argv)
{
	int cur;
	int follow = 1;

	if (argc != 2 || argv[1][0]!='-' || argv[1][1]!='f' ) {
		follow = 0;
		/* no options, no getopt */
		if (argc > 1)
			bb_show_usage();
	}

	log_shmid = shmget(KEY_ID, 0, 0);
	if (log_shmid == -1)
		error_exit("can't find circular buffer");

	// Attach shared memory to our char*
	buf = shmat(log_shmid, NULL, SHM_RDONLY);
	if (buf == NULL)
		error_exit("can't get access to syslogd buffer");

	log_semid = semget(KEY_ID, 0, 0);
	if (log_semid == -1)
		error_exit("can't get access to semaphores for syslogd buffer");

	// attempt to redefine ^C signal
	signal(SIGINT, interrupted);

	// Suppose atomic memory move
	cur = follow ? buf->tail : buf->head;

	do {
#ifdef CONFIG_FEATURE_LOGREAD_REDUCED_LOCKING
		char *buf_data;
		int log_len, j;
#endif
		sem_down(log_semid);

		if (DEBUG)
			printf("head:%i cur:%d tail:%i size:%i\n", buf->head, cur, buf->tail, buf->size);

		if (buf->head == buf->tail || cur == buf->tail) {
			if (follow) {
				sem_up(log_semid);
				sleep(1);	/* TODO: replace me with a sleep_on */
				continue;
			} else {
				printf("<empty syslog>\n");
			}
		}

		// Read Memory
#ifdef CONFIG_FEATURE_LOGREAD_REDUCED_LOCKING
		log_len = buf->tail - cur;
		if (log_len < 0)
			log_len += buf->size;
		buf_data = xmalloc(log_len);

		if (buf->tail >= cur) {
			memcpy(buf_data, buf->data + cur, log_len);
		} else {
			memcpy(buf_data, buf->data + cur, buf->size - cur);
			memcpy(buf_data + buf->size - cur, buf->data, buf->tail);
		}
		cur = buf->tail;
#else
		while (cur != buf->tail) {
			fputs(buf->data + cur, stdout);
			cur += strlen(buf->data + cur) + 1;
			if (cur >= buf->size)
				cur = 0;
		}
#endif
		// release the lock on the log chain
		sem_up(log_semid);

#ifdef CONFIG_FEATURE_LOGREAD_REDUCED_LOCKING
		for (j = 0; j < log_len; j += strlen(buf_data+j) + 1) {
			fputs(buf_data + j, stdout);
		}
		free(buf_data);
#endif
		fflush(stdout);
	} while (follow);

	shmdt(buf);

	return EXIT_SUCCESS;
}
