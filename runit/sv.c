/* Busyboxed by Denis Vlasenko <vda.linux@googlemail.com> */
/* TODO: depends on runit_lib.c - review and reduce/eliminate */

#include <sys/poll.h>
#include <sys/file.h>
#include "busybox.h"
#include "runit_lib.h"

static char *action;
static char *acts;
static char *varservice = "/var/service/";
static char **service;
static char **servicex;
static unsigned services;
static unsigned rc = 0;
static unsigned verbose = 0;
static unsigned long waitsec = 7;
static unsigned kll = 0;
static struct taia tstart, tnow, tdiff;
static struct tai tstatus;

static int (*act)(char*) = 0;
static int (*cbk)(char*) = 0;

static int curdir, fd, r;
static char svstatus[20];

#define usage() bb_show_usage()

static void fatal_cannot(char *m1)
{
	bb_perror_msg("fatal: cannot %s", m1);
	_exit(151);
}

static void out(char *p, char *m1)
{
	printf("%s%s: %s", p, *service, m1);
	if (errno) {
		printf(": %s", strerror(errno));
	}
	puts(""); /* will also flush the output */
}

#define FAIL    "fail: "
#define WARN    "warning: "
#define OK      "ok: "
#define RUN     "run: "
#define FINISH  "finish: "
#define DOWN    "down: "
#define TIMEOUT "timeout: "
#define KILL    "kill: "

static void fail(char *m1) { ++rc; out(FAIL, m1); }
static void failx(char *m1) { errno = 0; fail(m1); }
static void warn_cannot(char *m1) { ++rc; out("warning: cannot ", m1); }
static void warnx_cannot(char *m1) { errno = 0; warn_cannot(m1); }
static void ok(char *m1) { errno = 0; out(OK, m1); }

static int svstatus_get(void)
{
	if ((fd = open_write("supervise/ok")) == -1) {
		if (errno == ENODEV) {
			*acts == 'x' ? ok("runsv not running")
			             : failx("runsv not running");
			return 0;
		}
		warn_cannot("open supervise/ok");
		return -1;
	}
	close(fd);
	if ((fd = open_read("supervise/status")) == -1) {
		warn_cannot("open supervise/status");
		return -1;
	}
	r = read(fd, svstatus, 20);
	close(fd);
	switch (r) {
	case 20: break;
	case -1: warn_cannot("read supervise/status"); return -1;
	default: warnx_cannot("read supervise/status: bad format"); return -1;
	}
	return 1;
}

static unsigned svstatus_print(char *m)
{
	int pid;
	int normallyup = 0;
	struct stat s;

	if (stat("down", &s) == -1) {
		if (errno != ENOENT) {
			bb_perror_msg(WARN"cannot stat %s/down", *service);
			return 0;
		}
		normallyup = 1;
	}
	pid = (unsigned char) svstatus[15];
	pid <<= 8; pid += (unsigned char)svstatus[14];
	pid <<= 8; pid += (unsigned char)svstatus[13];
	pid <<= 8; pid += (unsigned char)svstatus[12];
	tai_unpack(svstatus, &tstatus);
	if (pid) {
		switch (svstatus[19]) {
		case 1: printf(RUN); break;
		case 2: printf(FINISH); break;
		}
		printf("%s: (pid %d) ", m, pid);
	}
	else {
		printf(DOWN"%s: ", m);
	}
	printf("%lus", (unsigned long)(tnow.sec.x < tstatus.x ? 0 : tnow.sec.x-tstatus.x));
	if (pid && !normallyup) printf(", normally down");
	if (!pid && normallyup) printf(", normally up");
	if (pid && svstatus[16]) printf(", paused");
	if (!pid && (svstatus[17] == 'u')) printf(", want up");
	if (pid && (svstatus[17] == 'd')) printf(", want down");
	if (pid && svstatus[18]) printf(", got TERM");
	return pid ? 1 : 2;
}

static int status(char *unused)
{
	r = svstatus_get();
	switch (r) { case -1: case 0: return 0; }
	r = svstatus_print(*service);
	if (chdir("log") == -1) {
		if (errno != ENOENT) {
			printf("; log: "WARN"cannot change to log service directory: %s",
					strerror(errno));
		}
	} else if (svstatus_get()) {
		printf("; ");
		svstatus_print("log");
	}
	puts(""); /* will also flush the output */
	return r;
}

static int checkscript(void)
{
	char *prog[2];
	struct stat s;
	int pid, w;

	if (stat("check", &s) == -1) {
		if (errno == ENOENT) return 1;
		bb_perror_msg(WARN"cannot stat %s/check", *service);
		return 0;
	}
	/* if (!(s.st_mode & S_IXUSR)) return 1; */
	if ((pid = fork()) == -1) {
		bb_perror_msg(WARN"cannot fork for %s/check", *service);
		return 0;
	}
	if (!pid) {
		prog[0] = "./check";
		prog[1] = 0;
		close(1);
		execve("check", prog, environ);
		bb_perror_msg(WARN"cannot run %s/check", *service);
		_exit(0);
	}
	while (wait_pid(&w, pid) == -1) {
		if (errno == EINTR) continue;
		bb_perror_msg(WARN"cannot wait for child %s/check", *service);
		return 0;
	}
	return !wait_exitcode(w);
}

static int check(char *a)
{
	unsigned pid;

	if ((r = svstatus_get()) == -1) return -1;
	if (r == 0) { if (*a == 'x') return 1; return -1; }
	pid = (unsigned char)svstatus[15];
	pid <<= 8; pid += (unsigned char)svstatus[14];
	pid <<= 8; pid += (unsigned char)svstatus[13];
	pid <<= 8; pid += (unsigned char)svstatus[12];
	switch (*a) {
	case 'x': return 0;
	case 'u':
		if (!pid || svstatus[19] != 1) return 0;
		if (!checkscript()) return 0;
		break;
	case 'd': if (pid) return 0; break;
	case 'c': if (pid) if (!checkscript()) return 0; break;
	case 't':
		if (!pid && svstatus[17] == 'd') break;
		tai_unpack(svstatus, &tstatus);
		if ((tstart.sec.x > tstatus.x) || !pid || svstatus[18] || !checkscript())
			return 0;
		break;
	case 'o':
		tai_unpack(svstatus, &tstatus);
		if ((!pid && tstart.sec.x > tstatus.x) || (pid && svstatus[17] != 'd'))
			return 0;
	}
	printf(OK); svstatus_print(*service); puts(""); /* will also flush the output */
	return 1;
}

static int control(char *a)
{
	if (svstatus_get() <= 0) return -1;
	if (svstatus[17] == *a) return 0;
	if ((fd = open_write("supervise/control")) == -1) {
		if (errno != ENODEV)
			warn_cannot("open supervise/control");
		else
			*a == 'x' ? ok("runsv not running") : failx("runsv not running");
		return -1;
	}
	r = write(fd, a, strlen(a));
	close(fd);
	if (r != strlen(a)) {
		warn_cannot("write to supervise/control");
		return -1;
	}
	return 1;
}

int sv_main(int argc, char **argv)
{
	unsigned opt;
	unsigned i, want_exit;
	char *x;

	for (i = strlen(*argv); i; --i)
		if ((*argv)[i-1] == '/')
			break;
	*argv += i;
	service = argv;
	services = 1;
	if ((x = getenv("SVDIR"))) varservice = x;
	if ((x = getenv("SVWAIT"))) waitsec = xatoul(x);
	/* TODO: V can be handled internally by getopt_ulflags */
	opt = getopt32(argc, argv, "w:vV", &x);
	if (opt & 1) waitsec = xatoul(x);
	if (opt & 2) verbose = 1;
	if (opt & 4) usage();
	if (!(action = *argv++)) usage();
	--argc;
	service = argv; services = argc;
	if (!*service) usage();

	taia_now(&tnow); tstart = tnow;
	if ((curdir = open_read(".")) == -1)
		fatal_cannot("open current directory");

	act = &control; acts = "s";
	if (verbose) cbk = &check;
	switch (*action) {
	case 'x': case 'e':
		acts = "x"; break;
	case 'X': case 'E':
		acts = "x"; kll = 1; cbk = &check; break;
	case 'D':
		acts = "d"; kll = 1; cbk = &check; break;
	case 'T':
		acts = "tc"; kll = 1; cbk = &check; break;
	case 'c':
		if (!str_diff(action, "check")) {
			act = 0;
			acts = "c";
			cbk = &check;
			break;
		}
	case 'u': case 'd': case 'o': case 't': case 'p': case 'h':
	case 'a': case 'i': case 'k': case 'q': case '1': case '2':
		action[1] = 0; acts = action; break;
	case 's':
		if (!str_diff(action, "shutdown")) {
			acts = "x";
			cbk = &check;
			break;
		}
		if (!str_diff(action, "start")) {
			acts = "u";
			cbk = &check;
			break;
		}
		if (!str_diff(action, "stop")) {
			acts = "d";
			cbk = &check;
			break;
		}
		act = &status; cbk = 0; break;
	case 'r':
		if (!str_diff(action, "restart")) {
			acts = "tcu";
			cbk = &check;
			break;
		}
		usage();
	case 'f':
		if (!str_diff(action, "force-reload"))
			{ acts = "tc"; kll = 1; cbk = &check; break; }
		if (!str_diff(action, "force-restart"))
			{ acts = "tcu"; kll = 1; cbk = &check; break; }
		if (!str_diff(action, "force-shutdown"))
			{ acts = "x"; kll = 1; cbk = &check; break; }
		if (!str_diff(action, "force-stop"))
			{ acts = "d"; kll = 1; cbk = &check; break; }
	default:
		usage();
	}

	servicex = service;
	for (i = 0; i < services; ++i) {
		if ((**service != '/') && (**service != '.')) {
			if ((chdir(varservice) == -1) || (chdir(*service) == -1)) {
				fail("cannot change to service directory");
				*service = 0;
			}
		} else if (chdir(*service) == -1) {
			fail("cannot change to service directory");
			*service = 0;
		}
		if (*service) if (act && (act(acts) == -1)) *service = 0;
		if (fchdir(curdir) == -1) fatal_cannot("change to original directory");
		service++;
	}

	if (*cbk)
		for (;;) {
			taia_sub(&tdiff, &tnow, &tstart);
			service = servicex; want_exit = 1;
			for (i = 0; i < services; ++i, ++service) {
				if (!*service) continue;
				if ((**service != '/') && (**service != '.')) {
					if ((chdir(varservice) == -1) || (chdir(*service) == -1)) {
						fail("cannot change to service directory");
						*service = 0;
					}
				} else if (chdir(*service) == -1) {
					fail("cannot change to service directory");
					*service = 0;
				}
				if (*service) { if (cbk(acts) != 0) *service = 0; else want_exit = 0; }
				if (*service && taia_approx(&tdiff) > waitsec) {
					kll ? printf(KILL) : printf(TIMEOUT);
					if (svstatus_get() > 0) { svstatus_print(*service); ++rc; }
					puts(""); /* will also flush the output */
					if (kll) control("k");
					*service = 0;
				}
				if (fchdir(curdir) == -1)
					fatal_cannot("change to original directory");
			}
			if (want_exit) break;
			usleep(420000);
			taia_now(&tnow);
		}
	return rc > 99 ? 99 : rc;
}
