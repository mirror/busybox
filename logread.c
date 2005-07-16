/* vi: set sw=4 ts=4: */
/*
 * circular buffer syslog implementation for busybox
 *
 * Copyright (C) 2000 by Gennady Feldman <gfeldman@cachier.com>
 *
 * Maintainer: Gennady Feldman <gena01@cachier.com> as of Mar 12, 2001
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
#include "busybox.h"

#if __GNU_LIBRARY__ < 5
#error Sorry.  Looks like you are using libc5.  
#error libc5 shm support isnt good enough.
#error Please disable BB_FEATURE_IPC_SYSLOG 
#endif	


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
	
	/* no options, no getopt */
	if (argc > 1)
		show_usage();
	
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

	sem_down(log_semid);	
	// Read Memory 
	i=buf->head;

	//printf("head: %i tail: %i size: %i\n",buf->head,buf->tail,buf->size);
	if (buf->head == buf->tail) {
		printf("<empty syslog>\n");
	}
	
	while ( i != buf->tail) {
		printf("%s", buf->data+i);
		i+= strlen(buf->data+i) + 1;
		if (i >= buf->size )
			i=0;
	}
	sem_up(log_semid);

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
