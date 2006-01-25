/*
 * ipcrm.c -- utility to allow removal of IPC objects and data structures.
 *
 * 01 Sept 2004 - Rodney Radford <rradford@mindspring.com>
 * Adapted for busybox from util-linux-2.12a.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * --- Pre-busybox history from util-linux-2.12a ------------------------
 *
 * 1999-04-02 frank zago
 * - can now remove several id's in the same call
 *
 * 1999-02-22 Arkadiusz Miÿkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * Original author - krishna balasubramanian 1993
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>

/* X/OPEN tells us to use <sys/{types,ipc,sem}.h> for semctl() */
/* X/OPEN tells us to use <sys/{types,ipc,msg}.h> for msgctl() */
/* for getopt */
#include <unistd.h>

/* for tolower and isupper */
#include <ctype.h>

#include "busybox.h"

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

typedef enum type_id {
	SHM,
	SEM,
	MSG
} type_id;

static int
remove_ids(type_id type, int argc, char **argv) {
	int id;
	int ret = 0;		/* for gcc */
	char *end;
	int nb_errors = 0;
	union semun arg;

	arg.val = 0;

	while(argc) {

		id = strtoul(argv[0], &end, 10);

		if (*end != 0) {
			bb_printf ("invalid id: %s\n", argv[0]);
			nb_errors ++;
		} else {
			switch(type) {
			case SEM:
				ret = semctl (id, 0, IPC_RMID, arg);
				break;

			case MSG:
				ret = msgctl (id, IPC_RMID, NULL);
				break;

			case SHM:
				ret = shmctl (id, IPC_RMID, NULL);
				break;
			}

			if (ret) {
				bb_printf ("cannot remove id %s (%s)\n",
					argv[0], strerror(errno));
				nb_errors ++;
			}
		}
		argc--;
		argv++;
	}

	return(nb_errors);
}

static int deprecated_main(int argc, char **argv)
{
	if (argc < 3) {
		bb_show_usage();
		bb_fflush_stdout_and_exit(1);
	}

	if (!strcmp(argv[1], "shm")) {
		if (remove_ids(SHM, argc-2, &argv[2]))
			bb_fflush_stdout_and_exit(1);
	}
	else if (!strcmp(argv[1], "msg")) {
		if (remove_ids(MSG, argc-2, &argv[2]))
			bb_fflush_stdout_and_exit(1);
	}
	else if (!strcmp(argv[1], "sem")) {
		if (remove_ids(SEM, argc-2, &argv[2]))
			bb_fflush_stdout_and_exit(1);
	}
	else {
		bb_printf ("unknown resource type: %s\n", argv[1]);
		bb_show_usage();
		bb_fflush_stdout_and_exit(1);
	}

	bb_printf ("resource(s) deleted\n");
	return 0;
}


int ipcrm_main(int argc, char **argv)
{
	int   c;
	int   error = 0;
	char *prog = argv[0];

	/* if the command is executed without parameters, do nothing */
	if (argc == 1)
		return 0;

	/* check to see if the command is being invoked in the old way if so
	   then run the old code */
	if (strcmp(argv[1], "shm") == 0 ||
		strcmp(argv[1], "msg") == 0 ||
		strcmp(argv[1], "sem") == 0)
		return deprecated_main(argc, argv);

	/* process new syntax to conform with SYSV ipcrm */
	while ((c = getopt(argc, argv, "q:m:s:Q:M:S:h?")) != -1) {
		int result;
		int id = 0;
		int iskey = isupper(c);

		/* needed to delete semaphores */
		union semun arg;
		arg.val = 0;

		if ((c == '?') || (c == 'h'))
		{
			bb_show_usage();
			return 0;
		}

		/* we don't need case information any more */
		c = tolower(c);

		/* make sure the option is in range */
		if (c != 'q' && c != 'm' && c != 's') {
			bb_show_usage();
			error++;
			return error;
		}

		if (iskey) {
			/* keys are in hex or decimal */
			key_t key = strtoul(optarg, NULL, 0);
			if (key == IPC_PRIVATE) {
				error++;
				bb_fprintf(stderr, "%s: illegal key (%s)\n",
					prog, optarg);
				continue;
			}

			/* convert key to id */
			id = ((c == 'q') ? msgget(key, 0) :
				  (c == 'm') ? shmget(key, 0, 0) :
				  semget(key, 0, 0));

			if (id < 0) {
				char *errmsg;
				error++;
				switch(errno) {
				case EACCES:
					errmsg = "permission denied for key";
					break;
				case EIDRM:
					errmsg = "already removed key";
					break;
				case ENOENT:
					errmsg = "invalid key";
					break;
				default:
					errmsg = "unknown error in key";
					break;
				}
				bb_fprintf(stderr, "%s: %s (%s)\n",
					prog, errmsg, optarg);
				continue;
			}
		} else {
			/* ids are in decimal */
			id = strtoul(optarg, NULL, 10);
		}

		result = ((c == 'q') ? msgctl(id, IPC_RMID, NULL) :
			  (c == 'm') ? shmctl(id, IPC_RMID, NULL) :
			  semctl(id, 0, IPC_RMID, arg));

		if (result < 0) {
			char *errmsg;
			error++;
			switch(errno) {
			case EACCES:
			case EPERM:
				errmsg = iskey
					? "permission denied for key"
					: "permission denied for id";
				break;
			case EINVAL:
				errmsg = iskey
					? "invalid key"
					: "invalid id";
				break;
			case EIDRM:
				errmsg = iskey
					? "already removed key"
					: "already removed id";
				break;
			default:
				errmsg = iskey
					? "unknown error in key"
					: "unknown error in id";
				break;
			}
			bb_fprintf(stderr, "%s: %s (%s)\n",
				prog, errmsg, optarg);
			continue;
		}
	}

	/* print usage if we still have some arguments left over */
	if (optind != argc) {
		bb_show_usage();
	}

	/* exit value reflects the number of errors encountered */
	return error;
}
