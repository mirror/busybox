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
#include <sys/sem.h>
#include <sys/shm.h>

#define DEBUG 0

enum { KEY_ID = 0x414e4547 }; /* "GENA" */

static struct shbuf_ds {
	int32_t size;           // size of data written
	int32_t head;           // start of message list
	int32_t tail;           // end of message list
	char data[1];           // data/messages
} *buf;                         // shared memory pointer

// Semaphore operation structures
static struct sembuf SMrup[1] = {{0, -1, IPC_NOWAIT | SEM_UNDO}}; // set SMrup
static struct sembuf SMrdn[2] = {{1, 0}, {0, +1, SEM_UNDO}}; // set SMrdn


static void error_exit(const char *str) ATTRIBUTE_NORETURN;
static void error_exit(const char *str)
{
	//release all acquired resources
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

static void interrupted(int sig ATTRIBUTE_UNUSED)
{
	signal(SIGINT, SIG_IGN);
	shmdt(buf);
	exit(0);
}

int logread_main(int argc, char **argv);
int logread_main(int argc, char **argv)
{
	int cur;
	int log_semid; /* ipc semaphore id */
	int log_shmid; /* ipc shared memory id */
	smallint follow = getopt32(argc, argv, "f");

	log_shmid = shmget(KEY_ID, 0, 0);
	if (log_shmid == -1)
		bb_perror_msg_and_die("can't find syslogd buffer");

	// Attach shared memory to our char*
	buf = shmat(log_shmid, NULL, SHM_RDONLY);
	if (buf == NULL)
		bb_perror_msg_and_die("can't access syslogd buffer");

	log_semid = semget(KEY_ID, 0, 0);
	if (log_semid == -1)
		error_exit("can't get access to semaphores for syslogd buffer");

	// attempt to redefine ^C signal
	signal(SIGINT, interrupted);

	// Suppose atomic memory move
	cur = follow ? buf->tail : buf->head;

	do {
#if ENABLE_FEATURE_LOGREAD_REDUCED_LOCKING
		char *buf_data;
		int log_len, j;
#endif
		if (semop(log_semid, SMrdn, 2) == -1)
			error_exit("semop[SMrdn]");

		if (DEBUG)
			printf("head:%i cur:%d tail:%i size:%i\n",
					buf->head, cur, buf->tail, buf->size);

		if (buf->head == buf->tail || cur == buf->tail) {
			if (follow) {
				sem_up(log_semid);
				fflush(stdout);
				sleep(1); /* TODO: replace me with a sleep_on */
				continue;
			}
			puts("<empty syslog>");
		}

		// Read Memory
#if ENABLE_FEATURE_LOGREAD_REDUCED_LOCKING
		log_len = buf->tail - cur;
		if (log_len < 0)
			log_len += buf->size;
		buf_data = xmalloc(log_len);

		if (buf->tail >= cur)
			j = log_len;
		else
			j = buf->size - cur;
		memcpy(buf_data, buf->data + cur, j);

		if (buf->tail < cur)
			memcpy(buf_data + buf->size - cur, buf->data, buf->tail);
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

#if ENABLE_FEATURE_LOGREAD_REDUCED_LOCKING
		for (j = 0; j < log_len; j += strlen(buf_data+j) + 1) {
			fputs(buf_data + j, stdout);
		}
		free(buf_data);
#endif
	} while (follow);

	shmdt(buf);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
