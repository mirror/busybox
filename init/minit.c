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

#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <linux/kd.h>

#include "busybox.h"

#define MINITROOT "/etc/minit"

static int i_am_init;
static int infd, outfd;

extern char **environ;

static struct process {
	char *name;
	pid_t pid;
	char respawn;
	char circular;
	time_t startedat;
	int __stdin, __stdout;
	int logservice;
} *root;

static int maxprocess = -1;

static int processalloc = 0;

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

/* split buf into n strings that are separated by c.  return n as *len.
 * Allocate plus more slots and leave the first ofs of them alone. */
static char **split(char *buf, int c, int *len, int plus, int ofs)
{
	int n = 1;
	char **v = 0;
	char **w;

	/* step 1: count tokens */
	char *s;

	for (s = buf; *s; s++)
		if (*s == c)
			n++;
	/* step 2: allocate space for pointers */
	v = (char **) malloc((n + plus) * sizeof(char *));
	if (!v)
		return 0;
	w = v + ofs;
	*w++ = buf;
	for (s = buf;; s++) {
		while (*s && *s != c)
			s++;
		if (*s == 0)
			break;
		if (*s == c) {
			*s = 0;
			*w++ = s + 1;
		}
	}
	*len = w - v;
	return v;
}

static int openreadclose(char *fn, char **buf, unsigned long *len)
{
	int fd = open(fn, O_RDONLY);

	if (fd < 0)
		return -1;
	if (!*buf) {
		*len = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		*buf = (char *) malloc(*len + 1);
		if (!*buf) {
			close(fd);
			return -1;
		}
	}
	*len = read(fd, *buf, *len);
	if (*len != (unsigned long) -1)
		(*buf)[*len] = 0;
	return close(fd);
}

/* return index of service in process data structure or -1 if not found */
static int findservice(char *service)
{
	int i;

	for (i = 0; i <= maxprocess; i++) {
		if (!strcmp(root[i].name, service))
			return i;
	}
	return -1;
}

/* look up process index in data structure by PID */
static int findbypid(pid_t pid)
{
	int i;

	for (i = 0; i <= maxprocess; i++) {
		if (root[i].pid == pid)
			return i;
	}
	return -1;
}

/* clear circular dependency detection flags */
static void circsweep(void)
{
	int i;

	for (i = 0; i <= maxprocess; i++)
		root[i].circular = 0;
}

/* add process to data structure, return index or -1 */
static int addprocess(struct process *p)
{
	if (maxprocess + 1 >= processalloc) {
		struct process *fump;

		processalloc += 8;
		if ((fump =
			 (struct process *) xrealloc(root,
										 processalloc *
										 sizeof(struct process))) == 0)
			return -1;
		root = fump;
	}
	memmove(&root[++maxprocess], p, sizeof(struct process));
	return maxprocess;
}

/* load a service into the process data structure and return index or -1
 * if failed */
static int loadservice(char *service)
{
	struct process tmp;
	int fd;

	if (*service == 0)
		return -1;
	fd = findservice(service);
	if (fd >= 0)
		return fd;
	if (chdir(MINITROOT) || chdir(service))
		return -1;
	if (!(tmp.name = strdup(service)))
		return -1;
	tmp.pid = 0;
	fd = open("respawn", O_RDONLY);
	if (fd >= 0) {
		tmp.respawn = 1;
		close(fd);
	} else
		tmp.respawn = 0;
	tmp.startedat = 0;
	tmp.circular = 0;
	tmp.__stdin = 0;
	tmp.__stdout = 1;
	{
		char *logservice = alloca(strlen(service) + 5);

		strcpy(logservice, service);
		strcat(logservice, "/log");
		tmp.logservice = loadservice(logservice);
		if (tmp.logservice >= 0) {
			int pipefd[2];

			if (pipe(pipefd))
				return -1;
			root[tmp.logservice].__stdin = pipefd[0];
			tmp.__stdout = pipefd[1];
		}
	}
	return (addprocess(&tmp));
}

/* usage: isup(findservice("sshd")).
 * returns nonzero if process is up */
static int isup(int service)
{
	if (service < 0)
		return 0;
	return (root[service].pid != 0);
}

static void opendevconsole(void)
{
	int fd;

	if ((fd = open("/dev/console", O_RDWR | O_NOCTTY)) >= 0) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd);
	}
}

/* called from inside the service directory, return the PID or 0 on error */
static pid_t forkandexec(int pause_flag, int service)
{
	char **argv = 0;
	int count = 0;
	pid_t p;
	int fd;
	unsigned long len;
	char *s = 0;
	int argc;
	char *argv0 = 0;

  again:
	switch (p = fork()) {
	case (pid_t) - 1:
		if (count > 3)
			return 0;
		sleep(++count * 2);
		goto again;
	case 0:
		/* child */

		if (i_am_init) {
			ioctl(0, TIOCNOTTY, 0);
			setsid();
			opendevconsole();
			tcsetpgrp(0, getpgrp());
		}
		close(infd);
		close(outfd);
		if (pause_flag) {
			struct timespec req;

			req.tv_sec = 0;
			req.tv_nsec = 500000000;
			nanosleep(&req, 0);
		}
		if (!openreadclose("params", &s, &len)) {
			argv = split(s, '\n', &argc, 2, 1);
			if (argv[argc - 1])
				argv[argc - 1] = 0;
			else
				argv[argc] = 0;
		} else {
			argv = (char **) xmalloc(2 * sizeof(char *));
			argv[1] = 0;
		}
		argv0 = (char *) xmalloc(PATH_MAX + 1);
		if (!argv || !argv0)
			goto abort;
		if (readlink("run", argv0, PATH_MAX) < 0) {
			if (errno != EINVAL)
				goto abort;	/* not a symbolic link */
			argv0 = strdup("./run");
		}
		argv[0] = strrchr(argv0, '/');
		if (argv[0])
			argv[0]++;
		else
			argv[0] = argv0;
		if (root[service].__stdin != 0)
			dup2(root[service].__stdin, 0);
		if (root[service].__stdout != 1) {
			dup2(root[service].__stdout, 1);
			dup2(root[service].__stdout, 2);
		}
		{
			int i;

			for (i = 3; i < 1024; ++i)
				close(i);
		}
		execve(argv0, argv, environ);
		_exit(0);
	  abort:
		free(argv0);
		free(argv);
		_exit(0);
	default:
		fd = open("sync", O_RDONLY);
		if (fd >= 0) {
			pid_t p2;

			close(fd);
			p2 = waitpid(p, 0, 0);
			return 1;
		}
		return p;
	}
}

/* start a service, return nonzero on error */
static int startnodep(int service, int pause_flag)
{
	/* step 1: see if the process is already up */
	if (isup(service))
		return 0;

	/* step 2: fork and exec service, put PID in data structure */
	if (chdir(MINITROOT) || chdir(root[service].name))
		return -1;
	root[service].startedat = time(0);
	root[service].pid = forkandexec(pause_flag, service);
	return root[service].pid;
}

static int startservice(int service, int pause_flag)
{
	int dir = -1;
	unsigned long len;
	char *s = 0;
	pid_t pid;

	if (service < 0)
		return 0;
	if (root[service].circular)
		return 0;
	root[service].circular = 1;
	if (root[service].logservice >= 0)
		startservice(root[service].logservice, pause_flag);
	if (chdir(MINITROOT) || chdir(root[service].name))
		return -1;
	if ((dir = open(".", O_RDONLY)) >= 0) {
		if (!openreadclose("depends", &s, &len)) {
			char **deps;
			int depc, i;

			deps = split(s, '\n', &depc, 0, 0);
			for (i = 0; i < depc; i++) {
				int service_index;

				if (deps[i][0] == '#')
					continue;
				service_index = loadservice(deps[i]);
				if (service_index >= 0 && root[service_index].pid != 1)
					startservice(service_index, 0);
			}
			fchdir(dir);
		}
		pid = startnodep(service, pause_flag);
		close(dir);
		dir = -1;
		return pid;
	}
	return 0;
}

static void sulogin(void)
{
	/* exiting on an initialization failure is not a good idea for init */
	char *argv[] = { "sulogin", 0 };
	execve("/sbin/sulogin", argv, environ);
	exit(1);
}

static void handlekilled(pid_t killed)
{
	int i;

	if (killed == (pid_t) - 1) {
		write(2, "all services exited.\n", 21);
		exit(0);
	}
	if (killed == 0)
		return;
	i = findbypid(killed);
	if (i >= 0) {
		root[i].pid = 0;
		if (root[i].respawn) {
			circsweep();
			startservice(i, time(0) - root[i].startedat < 1);
		} else {
			root[i].startedat = time(0);
			root[i].pid = 1;
		}
	}
}

static void childhandler(void)
{
	int status;
	pid_t killed;

	do {
		killed = waitpid(-1, &status, WNOHANG);
		handlekilled(killed);
	} while (killed && killed != (pid_t) - 1);
}

static volatile int dowinch = 0;
static volatile int doint = 0;

static void sigchild(int whatever)
{
}
static void sigwinch(int sig)
{
	dowinch = 1;
}
static void sigint(int sig)
{
	doint = 1;
}

extern int minit_main(int argc, char *argv[])
{
	/* Schritt 1: argv[1] als Service nehmen und starten */
	struct pollfd pfd;
	time_t last = time(0);
	int nfds = 1;
	int count = 0;
	int i;

	infd = open("/etc/minit/in", O_RDWR);
	outfd = open("/etc/minit/out", O_RDWR | O_NONBLOCK);
	if (getpid() == 1) {
		int fd;

		i_am_init = 1;
		reboot(0);
		if ((fd = open("/dev/console", O_RDWR | O_NOCTTY))) {
			ioctl(fd, KDSIGACCEPT, SIGWINCH);
			close(fd);
		} else
			ioctl(0, KDSIGACCEPT, SIGWINCH);
	}
/*  signal(SIGPWR,sighandler); don't know what to do about it */
/*  signal(SIGHUP,sighandler); ??? */
	{
		struct sigaction sa;

		sigemptyset(&sa.sa_mask);
		sa.sa_sigaction = 0;
		sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
		sa.sa_handler = sigchild;
		sigaction(SIGCHLD, &sa, 0);
		sa.sa_handler = sigint;
		sigaction(SIGINT, &sa, 0);	/* ctrl-alt-del */
		sa.sa_handler = sigwinch;
		sigaction(SIGWINCH, &sa, 0);	/* keyboard request */
	}
	if (infd < 0 || outfd < 0) {
		puts("minit: could not open /etc/minit/in or /etc/minit/out\n");
		sulogin();
		nfds = 0;
	} else
		pfd.fd = infd;
	pfd.events = POLLIN;

	for (i = 1; i < argc; i++) {
		circsweep();
		if (startservice(loadservice(argv[i]), 0))
			count++;
	}
	circsweep();
	if (!count)
		startservice(loadservice("default"), 0);
	for (;;) {
		char buf[1501];
		time_t now;

		if (doint) {
			doint = 0;
			startservice(loadservice("ctrlaltdel"), 0);
		}
		if (dowinch) {
			dowinch = 0;
			startservice(loadservice("kbreq"), 0);
		}
		childhandler();
		now = time(0);
		if (now < last || now - last > 30) {
			/* The system clock was reset.  Compensate. */
			long diff = last - now;
			int j;

			for (j = 0; j <= maxprocess; ++j) {
				root[j].startedat -= diff;
			}
		}
		last = now;
		switch (poll(&pfd, nfds, 5000)) {
		case -1:
			if (errno == EINTR) {
				childhandler();
				break;
			}
			opendevconsole();
			puts("poll failed!\n");
			sulogin();
			/* what should we do if poll fails?! */
			break;
		case 1:
			i = read(infd, buf, 1500);
			if (i > 1) {
				pid_t pid;
				int idx = 0;
				int tmp;

				buf[i] = 0;

				if (buf[0] != 's' && ((idx = findservice(buf + 1)) < 0))
				  error:
					write(outfd, "0", 1);
				else {
					switch (buf[0]) {
					case 'p':
						write(outfd, buf, fmt_ulong(buf, root[idx].pid));
						break;
					case 'r':
						root[idx].respawn = 0;
						goto ok;
					case 'R':
						root[idx].respawn = 1;
						goto ok;
					case 'C':
						if (kill(root[idx].pid, 0)) {	/* check if still active */
							handlekilled(root[idx].pid);	/* no!?! remove form active list */
							goto error;
						}
						goto ok;
						break;
					case 'P':
					{
						unsigned char *x = buf + strlen(buf) + 1;
						unsigned char c;

						tmp = 0;
						while ((c = *x++ - '0') < 10)
							tmp = tmp * 10 + c;
					}
						if (tmp > 0) {
							if (kill(tmp, 0))
								goto error;
							pid = tmp;
						}
						root[idx].pid = tmp;
						goto ok;
					case 's':
						idx = loadservice(buf + 1);
						if (idx < 0)
							goto error;
						if (root[idx].pid < 2) {
							root[idx].pid = 0;
							circsweep();
							idx = startservice(idx, 0);
							if (idx == 0) {
								write(outfd, "0", 1);
								break;
							}
						}
					  ok:
						write(outfd, "1", 1);
						break;
					case 'u':
						write(outfd, buf,
							  fmt_ulong(buf, time(0) - root[idx].startedat));
					}
				}
			}
			break;
		default:
			break;
		}
	}
}
