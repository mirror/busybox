/* vi: set sw=4 ts=4: */
/*
 * circular buffer syslog implementation for busybox
 *
 * Copyright (C) 2000 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include "busybox.h"

static const long KEY_ID = 0x414e4547; /*"GENA"*/

static struct shbuf_ds {
	int size;		// size of data written
	int head;		// start of message list
	int tail;		// end of message list
	char data[1];		// data/messages
} *buf = NULL;			// shared memory pointer


// Semaphore operation structures
static struct sembuf SMrup[1] = {{0, -1, IPC_NOWAIT | SEM_UNDO}}; // set SMrup
static struct sembuf SMrdn[2] = {{1, 0}, {0, +1, SEM_UNDO}}; // set SMrdn

static int	log_shmid = -1;	// ipc shared memory id
static int	log_semid = -1;	// ipc semaphore id
static jmp_buf	jmp_env;

static void error_exit(const char *str);
static void interrupted(int sig);

/*
 * sem_up - up()'s a semaphore.
 */
static inline void sem_up(int semid)
{
	if ( semop(semid, SMrup, 1) == -1 )
		error_exit("semop[SMrup]");
}

/*
 * sem_down - down()'s a semaphore
 */
static inline void sem_down(int semid)
{
	if ( semop(semid, SMrdn, 2) == -1 )
		error_exit("semop[SMrdn]");
}

extern int logread_main(int argc, char **argv)
{
	int i;
	int follow=0;

	if (argc == 2 && strcmp(argv[1],"-f")==0) {
		follow = 1;
	} else {
		/* no options, no getopt */
		if (argc > 1)
			bb_show_usage();
	}

	// handle intrrupt signal
	if (setjmp(jmp_env)) goto output_end;

	// attempt to redefine ^C signal
	signal(SIGINT, interrupted);

	if ( (log_shmid = shmget(KEY_ID, 0, 0)) == -1)
		error_exit("Can't find circular buffer");

	// Attach shared memory to our char*
	if ( (buf = shmat(log_shmid, NULL, SHM_RDONLY)) == NULL)
		error_exit("Can't get access to circular buffer from syslogd");

	if ( (log_semid = semget(KEY_ID, 0, 0)) == -1)
	    	error_exit("Can't get access to semaphone(s) for circular buffer from syslogd");

	// Suppose atomic memory move
	i = follow ? buf->tail : buf->head;

	do {
#ifdef CONFIG_FEATURE_LOGREAD_REDUCED_LOCKING
		char *buf_data;
		int log_len,j;
#endif

		sem_down(log_semid);

		//printf("head: %i tail: %i size: %i\n",buf->head,buf->tail,buf->size);
		if (buf->head == buf->tail || i==buf->tail) {
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
		log_len = buf->tail - i;
		if (log_len < 0)
			log_len += buf->size;
		buf_data = (char *)malloc(log_len);
		if (!buf_data)
			error_exit("malloc failed");

		if (buf->tail < i) {
			memcpy(buf_data, buf->data+i, buf->size-i);
			memcpy(buf_data+buf->size-i, buf->data, buf->tail);
		} else {
			memcpy(buf_data, buf->data+i, buf->tail-i);
		}
		i = buf->tail;

#else
		while ( i != buf->tail) {
			printf("%s", buf->data+i);
			i+= strlen(buf->data+i) + 1;
			if (i >= buf->size )
				i=0;
		}
#endif
		// release the lock on the log chain
		sem_up(log_semid);

#ifdef CONFIG_FEATURE_LOGREAD_REDUCED_LOCKING
		for (j=0; j < log_len; j+=strlen(buf_data+j)+1) {
			printf("%s", buf_data+j);
			if (follow)
				fflush(stdout);
		}
		free(buf_data);
#endif
	} while (follow);

output_end:
	if (log_shmid != -1)
		shmdt(buf);

	return EXIT_SUCCESS;
}

static void interrupted(int sig){
	signal(SIGINT, SIG_IGN);
	longjmp(jmp_env, 1);
}

static void error_exit(const char *str){
	perror(str);
	//release all acquired resources
	if (log_shmid != -1)
		shmdt(buf);

	exit(1);
}
