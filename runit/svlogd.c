/*
Copyright (c) 2001-2006, Gerrit Pape
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Busyboxed by Denis Vlasenko <vda.linux@googlemail.com> */
/* TODO: depends on runit_lib.c - review and reduce/eliminate */

#include <sys/poll.h>
#include <sys/file.h>
#include "busybox.h"
#include "runit_lib.h"

static unsigned verbose;
static int linemax = 1000;
////static int buflen = 1024;
static int linelen;

static char **fndir;
static int fdwdir;
static int wstat;
static struct taia trotate;

static char *line;
static smallint exitasap;
static smallint rotateasap;
static smallint reopenasap;
static smallint linecomplete = 1;

static smallint tmaxflag;

static char repl;
static const char *replace = "";

static sigset_t *blocked_sigset;
static iopause_fd input;
static int fl_flag_0;

static struct logdir {
	////char *btmp;
	/* pattern list to match, in "aa\0bb\0\cc\0\0" form */
	char *inst;
	char *processor;
	char *name;
	unsigned size;
	unsigned sizemax;
	unsigned nmax;
	unsigned nmin;
	/* int (not long) because of taia_uint() usage: */
	unsigned tmax;
	int ppid;
	int fddir;
	int fdcur;
	FILE* filecur; ////
	int fdlock;
	struct taia trotate;
	char fnsave[FMT_PTIME];
	char match;
	char matcherr;
} *dir;
static unsigned dirn;

#define FATAL "fatal: "
#define WARNING "warning: "
#define PAUSE "pausing: "
#define INFO "info: "

#define usage() bb_show_usage()
static void fatalx(const char *m0)
{
	bb_error_msg_and_die(FATAL"%s", m0);
}
static void warn(const char *m0) {
	bb_perror_msg(WARNING"%s", m0);
}
static void warn2(const char *m0, const char *m1)
{
	bb_perror_msg(WARNING"%s: %s", m0, m1);
}
static void warnx(const char *m0, const char *m1)
{
	bb_error_msg(WARNING"%s: %s", m0, m1);
}
static void pause_nomem(void)
{
	bb_error_msg(PAUSE"out of memory");
	sleep(3);
}
static void pause1cannot(const char *m0)
{
	bb_perror_msg(PAUSE"cannot %s", m0);
	sleep(3);
}
static void pause2cannot(const char *m0, const char *m1)
{
	bb_perror_msg(PAUSE"cannot %s %s", m0, m1);
	sleep(3);
}

static char* wstrdup(const char *str)
{
	char *s;
	while (!(s = strdup(str)))
		pause_nomem();
	return s;
}

static unsigned processorstart(struct logdir *ld)
{
	int pid;

	if (!ld->processor) return 0;
	if (ld->ppid) {
		warnx("processor already running", ld->name);
		return 0;
	}
	while ((pid = fork()) == -1)
		pause2cannot("fork for processor", ld->name);
	if (!pid) {
		char *prog[4];
		int fd;

		/* child */
		sig_uncatch(SIGTERM);
		sig_uncatch(SIGALRM);
		sig_uncatch(SIGHUP);
		sig_unblock(SIGTERM);
		sig_unblock(SIGALRM);
		sig_unblock(SIGHUP);

		if (verbose)
			bb_error_msg(INFO"processing: %s/%s", ld->name, ld->fnsave);
		fd = xopen(ld->fnsave, O_RDONLY|O_NDELAY);
		if (fd_move(0, fd) == -1)
			bb_perror_msg_and_die(FATAL"cannot %s processor %s", "move filedescriptor for", ld->name);
		ld->fnsave[26] = 't';
		fd = xopen(ld->fnsave, O_WRONLY|O_NDELAY|O_TRUNC|O_CREAT);
		if (fd_move(1, fd) == -1)
			bb_perror_msg_and_die(FATAL"cannot %s processor %s", "move filedescriptor for", ld->name);
		fd = open_read("state");
		if (fd == -1) {
			if (errno != ENOENT)
				bb_perror_msg_and_die(FATAL"cannot %s processor %s", "open state for", ld->name);
			close(xopen("state", O_WRONLY|O_NDELAY|O_TRUNC|O_CREAT));
			fd = xopen("state", O_RDONLY|O_NDELAY);
		}
		if (fd_move(4, fd) == -1)
			bb_perror_msg_and_die(FATAL"cannot %s processor %s", "move filedescriptor for", ld->name);
		fd = xopen("newstate", O_WRONLY|O_NDELAY|O_TRUNC|O_CREAT);
		if (fd_move(5, fd) == -1)
			bb_perror_msg_and_die(FATAL"cannot %s processor %s", "move filedescriptor for", ld->name);

// getenv("SHELL")?
		prog[0] = (char*)"sh";
		prog[1] = (char*)"-c";
		prog[2] = ld->processor;
		prog[3] = '\0';
		execve("/bin/sh", prog, environ);
		bb_perror_msg_and_die(FATAL"cannot %s processor %s", "run", ld->name);
	}
	ld->ppid = pid;
	return 1;
}

static unsigned processorstop(struct logdir *ld)
{
	char f[28];

	if (ld->ppid) {
		sig_unblock(SIGHUP);
		while (wait_pid(&wstat, ld->ppid) == -1)
			pause2cannot("wait for processor", ld->name);
		sig_block(SIGHUP);
		ld->ppid = 0;
	}
	if (ld->fddir == -1) return 1;
	while (fchdir(ld->fddir) == -1)
		pause2cannot("change directory, want processor", ld->name);
	if (wait_exitcode(wstat) != 0) {
		warnx("processor failed, restart", ld->name);
		ld->fnsave[26] = 't';
		unlink(ld->fnsave);
		ld->fnsave[26] = 'u';
		processorstart(ld);
		while (fchdir(fdwdir) == -1)
			pause1cannot("change to initial working directory");
		return ld->processor ? 0 : 1;
	}
	ld->fnsave[26] = 't';
	memcpy(f, ld->fnsave, 26);
	f[26] = 's';
	f[27] = '\0';
	while (rename(ld->fnsave, f) == -1)
		pause2cannot("rename processed", ld->name);
	while (chmod(f, 0744) == -1)
		pause2cannot("set mode of processed", ld->name);
	ld->fnsave[26] = 'u';
	if (unlink(ld->fnsave) == -1)
		bb_error_msg(WARNING"cannot unlink: %s/%s", ld->name, ld->fnsave);
	while (rename("newstate", "state") == -1)
		pause2cannot("rename state", ld->name);
	if (verbose)
		bb_error_msg(INFO"processed: %s/%s", ld->name, f);
	while (fchdir(fdwdir) == -1)
		pause1cannot("change to initial working directory");
	return 1;
}

static void rmoldest(struct logdir *ld)
{
	DIR *d;
	struct dirent *f;
	char oldest[FMT_PTIME];
	int n = 0;

	oldest[0] = 'A'; oldest[1] = oldest[27] = 0;
	while (!(d = opendir(".")))
		pause2cannot("open directory, want rotate", ld->name);
	errno = 0;
	while ((f = readdir(d))) {
		if ((f->d_name[0] == '@') && (strlen(f->d_name) == 27)) {
			if (f->d_name[26] == 't') {
				if (unlink(f->d_name) == -1)
					warn2("cannot unlink processor leftover", f->d_name);
			} else {
				++n;
				if (strcmp(f->d_name, oldest) < 0)
					memcpy(oldest, f->d_name, 27);
			}
			errno = 0;
		}
	}
	if (errno)
		warn2("cannot read directory", ld->name);
	closedir(d);

	if (ld->nmax && (n > ld->nmax)) {
		if (verbose)
			bb_error_msg(INFO"delete: %s/%s", ld->name, oldest);
		if ((*oldest == '@') && (unlink(oldest) == -1))
			warn2("cannot unlink oldest logfile", ld->name);
	}
}

static unsigned rotate(struct logdir *ld)
{
	struct stat st;
	struct taia now;

	if (ld->fddir == -1) {
		ld->tmax = 0;
		return 0;
	}
	if (ld->ppid)
		while (!processorstop(ld))
			/* wait */;

	while (fchdir(ld->fddir) == -1)
		pause2cannot("change directory, want rotate", ld->name);

	/* create new filename */
	ld->fnsave[25] = '.';
	ld->fnsave[26] = 's';
	if (ld->processor)
		ld->fnsave[26] = 'u';
	ld->fnsave[27] = '\0';
	do {
		taia_now(&now);
		fmt_taia25(ld->fnsave, &now);
		errno = 0;
		stat(ld->fnsave, &st);
	} while (errno != ENOENT);

	if (ld->tmax && taia_less(&ld->trotate, &now)) {
		taia_uint(&ld->trotate, ld->tmax);
		taia_add(&ld->trotate, &now, &ld->trotate);
		if (taia_less(&ld->trotate, &trotate))
			trotate = ld->trotate;
	}

	if (ld->size > 0) {
		while (fflush(ld->filecur) || fsync(ld->fdcur) == -1)
			pause2cannot("fsync current logfile", ld->name);
		while (fchmod(ld->fdcur, 0744) == -1)
			pause2cannot("set mode of current", ld->name);
		////close(ld->fdcur);
		fclose(ld->filecur);

		if (verbose) {
			bb_error_msg(INFO"rename: %s/current %s %u", ld->name,
					ld->fnsave, ld->size);
		}
		while (rename("current", ld->fnsave) == -1)
			pause2cannot("rename current", ld->name);
		while ((ld->fdcur = open("current", O_WRONLY|O_NDELAY|O_APPEND|O_CREAT, 0600)) == -1)
			pause2cannot("create new current", ld->name);
		/* we presume this cannot fail */
		ld->filecur = fdopen(ld->fdcur, "a"); ////
		setvbuf(ld->filecur, NULL, _IOFBF, linelen); ////
		coe(ld->fdcur);
		ld->size = 0;
		while (fchmod(ld->fdcur, 0644) == -1)
			pause2cannot("set mode of current", ld->name);
		rmoldest(ld);
		processorstart(ld);
	}

	while (fchdir(fdwdir) == -1)
		pause1cannot("change to initial working directory");
	return 1;
}

static int buffer_pwrite(int n, char *s, unsigned len)
{
	int i;
	struct logdir *ld = &dir[n];

	if (ld->sizemax) {
		if (ld->size >= ld->sizemax)
			rotate(ld);
		if (len > (ld->sizemax - ld->size))
			len = ld->sizemax - ld->size;
	}
	while (1) {
		////i = full_write(ld->fdcur, s, len);
		////if (i != -1) break;
		i = fwrite(s, 1, len, ld->filecur);
		if (i == len) break;

		if ((errno == ENOSPC) && (ld->nmin < ld->nmax)) {
			DIR *d;
			struct dirent *f;
			char oldest[FMT_PTIME];
			int j = 0;

			while (fchdir(ld->fddir) == -1)
				pause2cannot("change directory, want remove old logfile",
							 ld->name);
			oldest[0] = 'A';
			oldest[1] = oldest[27] = '\0';
			while (!(d = opendir(".")))
				pause2cannot("open directory, want remove old logfile",
							 ld->name);
			errno = 0;
			while ((f = readdir(d)))
				if ((f->d_name[0] == '@') && (strlen(f->d_name) == 27)) {
					++j;
					if (strcmp(f->d_name, oldest) < 0)
						memcpy(oldest, f->d_name, 27);
				}
			if (errno) warn2("cannot read directory, want remove old logfile",
					ld->name);
			closedir(d);
			errno = ENOSPC;
			if (j > ld->nmin) {
				if (*oldest == '@') {
					bb_error_msg(WARNING"out of disk space, delete: %s/%s",
							ld->name, oldest);
					errno = 0;
					if (unlink(oldest) == -1) {
						warn2("cannot unlink oldest logfile", ld->name);
						errno = ENOSPC;
					}
					while (fchdir(fdwdir) == -1)
						pause1cannot("change to initial working directory");
				}
			}
		}
		if (errno)
			pause2cannot("write to current", ld->name);
	}

	ld->size += i;
	if (ld->sizemax)
		if (s[i-1] == '\n')
			if (ld->size >= (ld->sizemax - linemax))
				rotate(ld);
	return i;
}

static void logdir_close(struct logdir *ld)
{
	if (ld->fddir == -1)
		return;
	if (verbose)
		bb_error_msg(INFO"close: %s", ld->name);
	close(ld->fddir);
	ld->fddir = -1;
	if (ld->fdcur == -1)
		return; /* impossible */
	while (fflush(ld->filecur) || fsync(ld->fdcur) == -1)
		pause2cannot("fsync current logfile", ld->name);
	while (fchmod(ld->fdcur, 0744) == -1)
		pause2cannot("set mode of current", ld->name);
	////close(ld->fdcur);
	fclose(ld->filecur);
	ld->fdcur = -1;
	if (ld->fdlock == -1)
		return; /* impossible */
	close(ld->fdlock);
	ld->fdlock = -1;
	free(ld->processor);
	ld->processor = NULL;
}

static unsigned logdir_open(struct logdir *ld, const char *fn)
{
	char buf[128];
	struct taia now;
	char *new, *s, *np;
	int i;
	struct stat st;

	ld->fddir = open(fn, O_RDONLY|O_NDELAY);
	if (ld->fddir == -1) {
		warn2("cannot open log directory", (char*)fn);
		return 0;
	}
	coe(ld->fddir);
	if (fchdir(ld->fddir) == -1) {
		logdir_close(ld);
		warn2("cannot change directory", (char*)fn);
		return 0;
	}
	ld->fdlock = open("lock", O_WRONLY|O_NDELAY|O_APPEND|O_CREAT, 0600);
	if ((ld->fdlock == -1)
	 || (lock_exnb(ld->fdlock) == -1)
	) {
		logdir_close(ld);
		warn2("cannot lock directory", (char*)fn);
		while (fchdir(fdwdir) == -1)
			pause1cannot("change to initial working directory");
		return 0;
	}
	coe(ld->fdlock);

	ld->size = 0;
	ld->sizemax = 1000000;
	ld->nmax = ld->nmin = 10;
	ld->tmax = 0;
	ld->name = (char*)fn;
	ld->ppid = 0;
	ld->match = '+';
	free(ld->inst); ld->inst = NULL;
	free(ld->processor); ld->processor = NULL;

	/* read config */
	i = open_read_close("config", buf, sizeof(buf));
	if (i < 0)
		warn2("cannot read config", ld->name);
	if (i > 0) {
		if (verbose) bb_error_msg(INFO"read: %s/config", ld->name);
		s = buf;
		while (s) {
			np = strchr(s, '\n');
			if (np) *np++ = '\0';
			switch (s[0]) {
			case '+':
			case '-':
			case 'e':
			case 'E':
				while (1) {
					int l = asprintf(&new, "%s%s\n", ld->inst?:"", s);
					if (l >= 0 && new) break;
					pause_nomem();
				}
				free(ld->inst);
				ld->inst = new;
				break;
			case 's': {
				static const struct suffix_mult km_suffixes[] = {
						{ "k", 1024 },
						{ "m", 1024*1024 },
						{ NULL, 0 }
				};
				ld->sizemax = xatou_sfx(&s[1], km_suffixes);
				break;
			}
			case 'n':
				ld->nmax = xatoi_u(&s[1]);
				break;
			case 'N':
				ld->nmin = xatoi_u(&s[1]);
				break;
			case 't': {
				static const struct suffix_mult mh_suffixes[] = {
						{ "m", 60 },
						{ "h", 60*60 },
						/*{ "d", 24*60*60 },*/
						{ NULL, 0 }
				};
				ld->tmax = xatou_sfx(&s[1], mh_suffixes);
				if (ld->tmax) {
					taia_uint(&ld->trotate, ld->tmax);
					taia_add(&ld->trotate, &now, &ld->trotate);
					if (!tmaxflag || taia_less(&ld->trotate, &trotate))
						trotate = ld->trotate;
					tmaxflag = 1;
				}
				break;
			}
			case '!':
				if (s[1]) {
					free(ld->processor);
					ld->processor = wstrdup(s);
				}
				break;
			}
			s = np;
		}
		/* Convert "aa\nbb\ncc\n\0" to "aa\0bb\0cc\0\0" */
		s = ld->inst;
		while (s) {
			np = strchr(s, '\n');
			if (np) *np++ = '\0';
			s = np;
		}
	}

	/* open current */
	i = stat("current", &st);
	if (i != -1) {
		if (st.st_size && ! (st.st_mode & S_IXUSR)) {
			ld->fnsave[25] = '.';
			ld->fnsave[26] = 'u';
			ld->fnsave[27] = '\0';
			do {
				taia_now(&now);
				fmt_taia25(ld->fnsave, &now);
				errno = 0;
				stat(ld->fnsave, &st);
			} while (errno != ENOENT);
			while (rename("current", ld->fnsave) == -1)
				pause2cannot("rename current", ld->name);
			rmoldest(ld);
			i = -1;
		} else {
			/* Be paranoid: st.st_size can be not just bigger, but WIDER! */
			/* (bug in original svlogd. remove this comment when fixed there) */
			ld->size = (st.st_size > ld->sizemax) ? ld->sizemax : st.st_size;
		}
	} else {
		if (errno != ENOENT) {
			logdir_close(ld);
			warn2("cannot stat current", ld->name);
			while (fchdir(fdwdir) == -1)
				pause1cannot("change to initial working directory");
			return 0;
		}
	}
	while ((ld->fdcur = open("current", O_WRONLY|O_NDELAY|O_APPEND|O_CREAT, 0600)) == -1)
		pause2cannot("open current", ld->name);
	/* we presume this cannot fail */
	ld->filecur = fdopen(ld->fdcur, "a"); ////
	setvbuf(ld->filecur, NULL, _IOFBF, linelen); ////

	coe(ld->fdcur);
	while (fchmod(ld->fdcur, 0644) == -1)
		pause2cannot("set mode of current", ld->name);

	if (verbose) {
		if (i == 0) bb_error_msg(INFO"append: %s/current", ld->name);
		else bb_error_msg(INFO"new: %s/current", ld->name);
	}

	while (fchdir(fdwdir) == -1)
		pause1cannot("change to initial working directory");
	return 1;
}

static void logdirs_reopen(void)
{
	struct taia now;
	int l;
	int ok = 0;

	tmaxflag = 0;
	taia_now(&now);
	for (l = 0; l < dirn; ++l) {
		logdir_close(&dir[l]);
		if (logdir_open(&dir[l], fndir[l])) ok = 1;
	}
	if (!ok) fatalx("no functional log directories");
}

/* Will look good in libbb one day */
static ssize_t ndelay_read(int fd, void *buf, size_t count)
{
	if (!(fl_flag_0 & O_NONBLOCK))
		fcntl(fd, F_SETFL, fl_flag_0 | O_NONBLOCK);
	count = safe_read(fd, buf, count);
	if (!(fl_flag_0 & O_NONBLOCK))
		fcntl(fd, F_SETFL, fl_flag_0);
	return count;
}

/* Used for reading stdin */
static int buffer_pread(int fd, char *s, unsigned len, struct taia *now)
{
	int i;

	if (rotateasap) {
		for (i = 0; i < dirn; ++i)
			rotate(dir+i);
		rotateasap = 0;
	}
	if (exitasap) {
		if (linecomplete)
			return 0;
		len = 1;
	}
	if (reopenasap) {
		logdirs_reopen();
		reopenasap = 0;
	}
	taia_uint(&trotate, 2744);
	taia_add(&trotate, now, &trotate);
	for (i = 0; i < dirn; ++i)
		if (dir[i].tmax) {
			if (taia_less(&dir[i].trotate, now))
				rotate(dir+i);
			if (taia_less(&dir[i].trotate, &trotate))
				trotate = dir[i].trotate;
		}

	while (1) {
		sigprocmask(SIG_UNBLOCK, blocked_sigset, NULL);
		iopause(&input, 1, &trotate, now);
		sigprocmask(SIG_BLOCK, blocked_sigset, NULL);
		i = ndelay_read(fd, s, len);
		if (i >= 0) break;
		if (errno != EAGAIN) {
			warn("cannot read standard input");
			break;
		}
		/* else: EAGAIN - normal, repeat silently */
	}

	if (i > 0) {
		int cnt;
		linecomplete = (s[i-1] == '\n');
		if (!repl) return i;

		cnt = i;
		while (--cnt >= 0) {
			char ch = *s;
			if (ch != '\n') {
				if (ch < 32 || ch > 126)
					*s = repl;
				else {
					int j;
					for (j = 0; replace[j]; ++j) {
						if (ch == replace[j]) {
							*s = repl;
							break;
						}
					}
				}
			}
			s++;
		}
	}
	return i;
}


static void sig_term_handler(int sig_no)
{
	if (verbose)
		bb_error_msg(INFO"sig%s received", "term");
	exitasap = 1;
}

static void sig_child_handler(int sig_no)
{
	int pid, l;

	if (verbose)
		bb_error_msg(INFO"sig%s received", "child");
	while ((pid = wait_nohang(&wstat)) > 0)
		for (l = 0; l < dirn; ++l)
			if (dir[l].ppid == pid) {
				dir[l].ppid = 0;
				processorstop(&dir[l]);
				break;
			}
}

static void sig_alarm_handler(int sig_no)
{
	if (verbose)
		bb_error_msg(INFO"sig%s received", "alarm");
	rotateasap = 1;
}

static void sig_hangup_handler(int sig_no)
{
	if (verbose)
		bb_error_msg(INFO"sig%s received", "hangup");
	reopenasap = 1;
}

static void logmatch(struct logdir *ld)
{
	char *s;

	ld->match = '+';
	ld->matcherr = 'E';
	s = ld->inst;
	while (s && s[0]) {
		switch (s[0]) {
		case '+':
		case '-':
			if (pmatch(s+1, line, linelen))
				ld->match = s[0];
			break;
		case 'e':
		case 'E':
			if (pmatch(s+1, line, linelen))
				ld->matcherr = s[0];
			break;
		}
		s += strlen(s) + 1;
	}
}

int svlogd_main(int argc, char **argv);
int svlogd_main(int argc, char **argv)
{
	sigset_t ss;
	char *r,*l,*b;
	ssize_t stdin_cnt = 0;
	int i;
	unsigned opt;
	unsigned timestamp = 0;
	void* (*memRchr)(const void *, int, size_t) = memchr;

#define line bb_common_bufsiz1

	opt_complementary = "tt:vv";
	opt = getopt32(argc, argv, "r:R:l:b:tv",
			&r, &replace, &l, &b, &timestamp, &verbose);
	if (opt & 1) { // -r
		repl = r[0];
		if (!repl || r[1]) usage();
	}
	if (opt & 2) if (!repl) repl = '_'; // -R
	if (opt & 4) { // -l
		linemax = xatou_range(l, 0, BUFSIZ-26);
		if (linemax == 0) linemax = BUFSIZ-26;
		if (linemax < 256) linemax = 256;
	}
	////if (opt & 8) { // -b
	////	buflen = xatoi_u(b);
	////	if (buflen == 0) buflen = 1024;
	////}
	//if (opt & 0x10) timestamp++; // -t
	//if (opt & 0x20) verbose++; // -v
	//if (timestamp > 2) timestamp = 2;
	argv += optind;
	argc -= optind;

	dirn = argc;
	if (dirn <= 0) usage();
	////if (buflen <= linemax) usage();
	fdwdir = xopen(".", O_RDONLY|O_NDELAY);
	coe(fdwdir);
	dir = xmalloc(dirn * sizeof(struct logdir));
	for (i = 0; i < dirn; ++i) {
		dir[i].fddir = -1;
		dir[i].fdcur = -1;
		////dir[i].btmp = xmalloc(buflen);
		dir[i].ppid = 0;
	}
	/* line = xmalloc(linemax + (timestamp ? 26 : 0)); */
	fndir = argv;
	input.fd = 0;
	input.events = IOPAUSE_READ;
	/* We cannot set NONBLOCK on fd #0 permanently - this setting
	 * _isn't_ per-process! It is shared among all other processes
	 * with the same stdin */
	fl_flag_0 = fcntl(0, F_GETFL, 0);

	blocked_sigset = &ss;
	sigemptyset(&ss);
	sigaddset(&ss, SIGTERM);
	sigaddset(&ss, SIGCHLD);
	sigaddset(&ss, SIGALRM);
	sigaddset(&ss, SIGHUP);
	sigprocmask(SIG_BLOCK, &ss, NULL);
	sig_catch(SIGTERM, sig_term_handler);
	sig_catch(SIGCHLD, sig_child_handler);
	sig_catch(SIGALRM, sig_alarm_handler);
	sig_catch(SIGHUP, sig_hangup_handler);

	logdirs_reopen();

	/* Without timestamps, we don't have to print each line
	 * separately, so we can look for _last_ newline, not first,
	 * thus batching writes */
	if (!timestamp)
		memRchr = memrchr;

	setvbuf(stderr, NULL, _IOFBF, linelen);

	/* Each iteration processes one or more lines */
	while (1) {
		struct taia now;
		char stamp[FMT_PTIME];
		char *lineptr;
		char *printptr;
		char *np;
		int printlen;
		char ch;

		lineptr = line;
		taia_now(&now);
		/* Prepare timestamp if needed */
		if (timestamp) {
			switch (timestamp) {
			case 1:
				fmt_taia25(stamp, &now);
				break;
			default: /* case 2: */
				fmt_ptime30nul(stamp, &now);
				break;
			}
			lineptr += 26;
		}

		/* lineptr[0..linemax-1] - buffer for stdin */
		/* (possibly has some unprocessed data from prev loop) */

		/* Refill the buffer if needed */
		np = memRchr(lineptr, '\n', stdin_cnt);
		if (!np && !exitasap) {
			i = linemax - stdin_cnt; /* avail. bytes at tail */
			if (i >= 128) {
				i = buffer_pread(0, lineptr + stdin_cnt, i, &now);
				if (i <= 0) /* EOF or error on stdin */
					exitasap = 1;
				else {
					np = memRchr(lineptr + stdin_cnt, '\n', i);
					stdin_cnt += i;
				}
			}
		}
		if (stdin_cnt <= 0 && exitasap)
			break;

		/* Search for '\n' (in fact, np already holds the result) */
		linelen = stdin_cnt;
		if (np) {
 print_to_nl:		/* NB: starting from here lineptr may point
			 * farther out into line[] */
			linelen = np - lineptr + 1;
		}
		/* linelen == no of chars incl. '\n' (or == stdin_cnt) */
		ch = lineptr[linelen-1];

		/* Biggest performance hit was coming from the fact
		 * that we did not buffer writes. We were reading many lines
		 * in one read() above, but wrote one line per write().
		 * We are using stdio to fix that */

		/* write out lineptr[0..linelen-1] to each log destination
		 * (or lineptr[-26..linelen-1] if timestamping) */
		printlen = linelen;
		printptr = lineptr;
		if (timestamp) {
			printlen += 26;
			printptr -= 26;
			memcpy(printptr, stamp, 25);
			printptr[25] = ' ';
		}
		for (i = 0; i < dirn; ++i) {
			struct logdir *ld = &dir[i];
			if (ld->fddir == -1) continue;
			if (ld->inst)
				logmatch(ld);
			if (ld->matcherr == 'e')
				////full_write(2, printptr, printlen);
				fwrite(lineptr, 1, linelen, stderr);
			if (ld->match != '+') continue;
			buffer_pwrite(i, printptr, printlen);
		}

		/* If we didn't see '\n' (long input line), */
		/* read/write repeatedly until we see it */
		while (ch != '\n') {
			/* lineptr is emptied now, safe to use as buffer */
			taia_now(&now);
			stdin_cnt = exitasap ? -1 : buffer_pread(0, lineptr, linemax, &now);
			if (stdin_cnt <= 0) { /* EOF or error on stdin */
				exitasap = 1;
				lineptr[0] = ch = '\n';
				linelen = 1;
				stdin_cnt = 1;
			} else {
				linelen = stdin_cnt;
				np = memRchr(lineptr, '\n', stdin_cnt);
				if (np)
					linelen = np - lineptr + 1;
				ch = lineptr[linelen-1];
			}
			/* linelen == no of chars incl. '\n' (or == stdin_cnt) */
			for (i = 0; i < dirn; ++i) {
				if (dir[i].fddir == -1) continue;
				if (dir[i].matcherr == 'e')
					////full_write(2, lineptr, linelen);
					fwrite(lineptr, 1, linelen, stderr);
				if (dir[i].match != '+') continue;
				buffer_pwrite(i, lineptr, linelen);
			}
		}

		stdin_cnt -= linelen;
		if (stdin_cnt > 0) {
			lineptr += linelen;
			/* If we see another '\n', we don't need to read
			 * next piece of input: can print what we have */
			np = memRchr(lineptr, '\n', stdin_cnt);
			if (np)
				goto print_to_nl;
			/* Move unprocessed data to the front of line */
			memmove((timestamp ? line+26 : line), lineptr, stdin_cnt);
		}
		fflush(NULL);////
	}

	for (i = 0; i < dirn; ++i) {
		if (dir[i].ppid)
			while (!processorstop(&dir[i]))
				/* repeat */;
		logdir_close(&dir[i]);
	}
	return 0;
}
