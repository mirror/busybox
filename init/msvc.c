/*
 *  minit version 0.9.1 by Felix von Leitner
 *  ported to busybox by Glenn McGrath <bug1@optushome.com.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busybox.h"

static int infd, outfd;

static char buf[1500];

static unsigned int fmt_ulong(char *dest, unsigned long i)
{
	register unsigned long len, tmp, len2;

	/* first count the number of bytes needed */
	for (len = 1, tmp = i; tmp > 9; ++len)
		tmp /= 10;
	if (dest)
		for (tmp = i, dest += len, len2 = len + 1; --len2; tmp /= 10)
			*--dest = (tmp % 10) + '0';
	return len;
}

static void addservice(char *service)
{
	char *service_ptr;

	if (strncmp(service, "/etc/minit/", 11) == 0) {
		service += 11;
	}

	while ((service_ptr = last_char_is(service, '/')) != NULL) {
		*service_ptr = 0;
	}
	strncpy(buf + 1, service, 1400);
	buf[1400] = 0;
}

static int addreadwrite(char *service)
{
	addservice(service);
	write(infd, buf, strlen(buf));
	return read(outfd, buf, 1500);
}

/* return PID, 0 if error */
static pid_t __readpid(char *service)
{
	int len;

	buf[0] = 'p';
	len = addreadwrite(service);
	if (len < 0)
		return 0;
	buf[len] = 0;
	return atoi(buf);
}

/* return nonzero if error */
static int respawn(char *service, int yesno)
{
	int len;

	buf[0] = yesno ? 'R' : 'r';
	len = addreadwrite(service);
	return (len != 1 || buf[0] == '0');
}

/* return nonzero if error */
static int startservice(char *service)
{
	int len;

	buf[0] = 's';
	len = addreadwrite(service);
	return (len != 1 || buf[0] == '0');
}

extern int msvc_main(int argc, char **argv)
{
	if (argc < 2) {
		bb_show_usage();
	}
	infd = bb_xopen("/etc/minit/in", O_WRONLY);
	outfd = bb_xopen("/etc/minit/out", O_RDONLY);

	while (lockf(infd, F_LOCK, 1)) {
		bb_perror_msg("could not aquire lock!\n");
		sleep(1);
	}

	if (argc == 2) {
		pid_t pid = __readpid(argv[1]);

		if (buf[0] != '0') {
			int len;
			int up_time;

			printf("%s: ", argv[1]);
			if (pid == 0)
				printf("down ");
			else if (pid == 1)
				printf("finished ");
			else {
				printf("up (pid %d) ", pid);
			}

			buf[0] = 'u';
			len = addreadwrite(argv[1]);
			if (len < 0) {
				up_time = 0;
			} else {
				buf[len] = 0;
				up_time = atoi(buf);
			}
			printf("%d seconds\n", up_time);

			if (pid == 0)
				return 2;
			else if (pid == 1)
				return 3;
			else
				return 0;
		} else {
			bb_error_msg_and_die("no such service");
		}
	} else {
		int i;
		int ret = 0;
		int sig = 0;
		pid_t pid;

		if (argv[1][0] == '-') {
			switch (argv[1][1]) {
			case 'g':
				for (i = 2; i < argc; ++i) {
					pid = __readpid(argv[i]);
					if (pid < 2) {
						if (pid == 1) {
							bb_error_msg("%s, service termination", argv[i]);
						} else {
							bb_error_msg("%s, no such service", argv[i]);
						}
						ret = 1;
					}
					printf("%d\n", pid);
				}
				break;
			case 'p':
				sig = SIGSTOP;
				goto dokill;
				break;
			case 'c':
				sig = SIGCONT;
				goto dokill;
				break;
			case 'h':
				sig = SIGHUP;
				goto dokill;
				break;
			case 'a':
				sig = SIGALRM;
				goto dokill;
				break;
			case 'i':
				sig = SIGINT;
				goto dokill;
				break;
			case 't':
				sig = SIGTERM;
				goto dokill;
				break;
			case 'k':
				sig = SIGKILL;
				goto dokill;
				break;
			case 'o':
				for (i = 2; i < argc; ++i)
					if (startservice(argv[i]) || respawn(argv[i], 0)) {
						bb_error_msg("Couldnt not start %s\n", argv[i]);
						ret = 1;
					}
				break;
			case 'd':
				for (i = 2; i < argc; ++i) {
					pid = __readpid(argv[i]);
					if (pid == 0) {
						bb_error_msg("%s, no such service\n", argv[i]);
						ret = 1;
					} else if (pid == 1)
						continue;
					if (respawn(argv[i], 0) || kill(pid, SIGTERM)
						|| kill(pid, SIGCONT));
				}
				break;
			case 'u':
				for (i = 2; i < argc; ++i) {
					if (startservice(argv[i]) || respawn(argv[i], 1)) {
						bb_error_msg("Couldnt not start %s\n", argv[i]);
						ret = 1;
					}
					break;
				}
			case 'C':
				for (i = 2; i < argc; ++i) {
					int len;

					buf[0] = 'C';
					len = addreadwrite(argv[i]);
					if (len != 1 || buf[0] == '0') {
						bb_error_msg("%s has terminated or was killed\n",
									 argv[i]);
						ret = 1;
					}
				}
				break;
			case 'P':
				pid = atoi(argv[1] + 2);
				if (pid > 1) {
					char *tmp;
					int len;

					buf[0] = 'P';
					addservice(argv[2]);
					tmp = buf + strlen(buf) + 1;
					tmp[fmt_ulong(tmp, pid)] = 0;
					write(infd, buf, strlen(buf) + strlen(tmp) + 2);
					len = read(outfd, buf, 1500);
					if (len != 1 || buf[0] == '0') {
						bb_error_msg_and_die("Couldnt not set pid of service %s\n", argv[2]);
					}
				}
				break;
			default:
				bb_show_usage();
			}
		} else {
			bb_show_usage();
		}
		return ret;
dokill:
		for (i = 2; i < argc; i++) {
			pid = __readpid(argv[i]);
			if (!pid) {
				bb_error_msg("%s no such service\n", argv[i]);
				ret = 1;
			}
			if (kill(pid, sig)) {
				bb_error_msg("%s, could not send signal %d to PID %d\n",
							 argv[i], sig, pid);
				ret = 1;
			}
		}
		return ret;
	}
}

/*
  -u   Up.  If the service is not running, start it.  If the service stops,
       restart it.
  -d   Down.  If the service is running, send it a TERM signal and then a CONT
       signal.  After it stops, do not restart it.
  -o   Once.  If the service is not running, start it.  Do not restart it if it
       stops.
  -r   Tell supervise that the service is normally running; this affects status
       messages.
  -s   Tell supervise that the service is normally stopped; this affects status
       messages.
  -p   Pause.  Send the service a STOP signal.
  -c   Continue.  Send the service a CONT signal.
  -h   Hangup.  Send the service a HUP signal.
  -a   Alarm.  Send the service an ALRM signal.
  -i   Interrupt.  Send the service an INT signal.
  -t   Terminate.  Send the service a TERM signal.
  -k   Kill.  Send the service a KILL signal.
  -x   Exit.  supervise will quit as soon as the service is down.
*/
