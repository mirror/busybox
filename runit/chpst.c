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

/* Busyboxed by Denys Vlasenko <vda.linux@googlemail.com> */
/* Dependencies on runit_lib.c removed */

#include "libbb.h"

#include <dirent.h>

// Must match constants in chpst_main!
#define OPT_verbose  (option_mask32 & 0x2000)
#define OPT_pgrp     (option_mask32 & 0x4000)
#define OPT_nostdin  (option_mask32 & 0x8000)
#define OPT_nostdout (option_mask32 & 0x10000)
#define OPT_nostderr (option_mask32 & 0x20000)

struct globals {
	char *set_user;
	char *env_user;
	const char *env_dir;
	const char *root;
	long limitd; /* limitX are initialized to -2 */
	long limits;
	long limitl;
	long limita;
	long limito;
	long limitp;
	long limitf;
	long limitc;
	long limitr;
	long limitt;
	int nicelvl;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define set_user (G.set_user)
#define env_user (G.env_user)
#define env_dir  (G.env_dir )
#define root     (G.root    )
#define limitd   (G.limitd  )
#define limits   (G.limits  )
#define limitl   (G.limitl  )
#define limita   (G.limita  )
#define limito   (G.limito  )
#define limitp   (G.limitp  )
#define limitf   (G.limitf  )
#define limitc   (G.limitc  )
#define limitr   (G.limitr  )
#define limitt   (G.limitt  )
#define nicelvl  (G.nicelvl )
#define INIT_G() do { \
	long *p = &limitd; \
	do *p++ = -2; while (p <= &limitt); \
} while (0)

static void suidgid(char *user)
{
	struct bb_uidgid_t ugid;

	if (!get_uidgid(&ugid, user, 1)) {
		bb_error_msg_and_die("unknown user/group: %s", user);
	}
	if (setgroups(1, &ugid.gid) == -1)
		bb_perror_msg_and_die("setgroups");
	xsetgid(ugid.gid);
	xsetuid(ugid.uid);
}

static void euidgid(char *user)
{
	struct bb_uidgid_t ugid;

	if (!get_uidgid(&ugid, user, 1)) {
		bb_error_msg_and_die("unknown user/group: %s", user);
	}
	xsetenv("GID", utoa(ugid.gid));
	xsetenv("UID", utoa(ugid.uid));
}

static void edir(const char *directory_name)
{
	int wdir;
	DIR *dir;
	struct dirent *d;
	int fd;

	wdir = xopen(".", O_RDONLY | O_NDELAY);
	xchdir(directory_name);
	dir = opendir(".");
	if (!dir)
		bb_perror_msg_and_die("opendir %s", directory_name);
	for (;;) {
		char buf[256];
		char *tail;
		int size;

		errno = 0;
		d = readdir(dir);
		if (!d) {
			if (errno)
				bb_perror_msg_and_die("readdir %s",
						directory_name);
			break;
		}
		if (d->d_name[0] == '.')
			continue;
		fd = open(d->d_name, O_RDONLY | O_NDELAY);
		if (fd < 0) {
			if ((errno == EISDIR) && env_dir) {
				if (OPT_verbose)
					bb_perror_msg("warning: %s/%s is a directory",
						directory_name,	d->d_name);
				continue;
			} else
				bb_perror_msg_and_die("open %s/%s",
						directory_name, d->d_name);
		}
		size = full_read(fd, buf, sizeof(buf)-1);
		close(fd);
		if (size < 0)
			bb_perror_msg_and_die("read %s/%s",
					directory_name, d->d_name);
		if (size == 0) {
			unsetenv(d->d_name);
			continue;
		}
		buf[size] = '\n';
		tail = strchr(buf, '\n');
		/* skip trailing whitespace */
		while (1) {
			*tail = '\0';
			tail--;
			if (tail < buf || !isspace(*tail))
				break;
		}
		xsetenv(d->d_name, buf);
	}
	closedir(dir);
	if (fchdir(wdir) == -1)
		bb_perror_msg_and_die("fchdir");
	close(wdir);
}

static void limit(int what, long l)
{
	struct rlimit r;

	/* Never fails under Linux (except if you pass it bad arguments) */
	getrlimit(what, &r);
	if ((l < 0) || (l > r.rlim_max))
		r.rlim_cur = r.rlim_max;
	else
		r.rlim_cur = l;
	if (setrlimit(what, &r) == -1)
		bb_perror_msg_and_die("setrlimit");
}

static void slimit(void)
{
	if (limitd >= -1) {
#ifdef RLIMIT_DATA
		limit(RLIMIT_DATA, limitd);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"DATA");
#endif
	}
	if (limits >= -1) {
#ifdef RLIMIT_STACK
		limit(RLIMIT_STACK, limits);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"STACK");
#endif
	}
	if (limitl >= -1) {
#ifdef RLIMIT_MEMLOCK
		limit(RLIMIT_MEMLOCK, limitl);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"MEMLOCK");
#endif
	}
	if (limita >= -1) {
#ifdef RLIMIT_VMEM
		limit(RLIMIT_VMEM, limita);
#else
#ifdef RLIMIT_AS
		limit(RLIMIT_AS, limita);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"VMEM");
#endif
#endif
	}
	if (limito >= -1) {
#ifdef RLIMIT_NOFILE
		limit(RLIMIT_NOFILE, limito);
#else
#ifdef RLIMIT_OFILE
		limit(RLIMIT_OFILE, limito);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"NOFILE");
#endif
#endif
	}
	if (limitp >= -1) {
#ifdef RLIMIT_NPROC
		limit(RLIMIT_NPROC, limitp);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"NPROC");
#endif
	}
	if (limitf >= -1) {
#ifdef RLIMIT_FSIZE
		limit(RLIMIT_FSIZE, limitf);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"FSIZE");
#endif
	}
	if (limitc >= -1) {
#ifdef RLIMIT_CORE
		limit(RLIMIT_CORE, limitc);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"CORE");
#endif
	}
	if (limitr >= -1) {
#ifdef RLIMIT_RSS
		limit(RLIMIT_RSS, limitr);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"RSS");
#endif
	}
	if (limitt >= -1) {
#ifdef RLIMIT_CPU
		limit(RLIMIT_CPU, limitt);
#else
		if (OPT_verbose)
			bb_error_msg("system does not support RLIMIT_%s",
				"CPU");
#endif
	}
}

/* argv[0] */
static void setuidgid(int, char **) ATTRIBUTE_NORETURN;
static void envuidgid(int, char **) ATTRIBUTE_NORETURN;
static void envdir(int, char **) ATTRIBUTE_NORETURN;
static void softlimit(int, char **) ATTRIBUTE_NORETURN;

int chpst_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chpst_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	INIT_G();

	if (applet_name[3] == 'd') envdir(argc, argv);
	if (applet_name[1] == 'o') softlimit(argc, argv);
	if (applet_name[0] == 's') setuidgid(argc, argv);
	if (applet_name[0] == 'e') envuidgid(argc, argv);
	// otherwise we are chpst

	{
		char *m,*d,*o,*p,*f,*c,*r,*t,*n;
		getopt32(argv, "+u:U:e:m:d:o:p:f:c:r:t:/:n:vP012",
				&set_user,&env_user,&env_dir,
				&m,&d,&o,&p,&f,&c,&r,&t,&root,&n);
		// if (option_mask32 & 0x1) // -u
		// if (option_mask32 & 0x2) // -U
		// if (option_mask32 & 0x4) // -e
		if (option_mask32 & 0x8) limits = limitl = limita = limitd = xatoul(m); // -m
		if (option_mask32 & 0x10) limitd = xatoul(d); // -d
		if (option_mask32 & 0x20) limito = xatoul(o); // -o
		if (option_mask32 & 0x40) limitp = xatoul(p); // -p
		if (option_mask32 & 0x80) limitf = xatoul(f); // -f
		if (option_mask32 & 0x100) limitc = xatoul(c); // -c
		if (option_mask32 & 0x200) limitr = xatoul(r); // -r
		if (option_mask32 & 0x400) limitt = xatoul(t); // -t
		// if (option_mask32 & 0x800) // -/
		if (option_mask32 & 0x1000) nicelvl = xatoi(n); // -n
		// The below consts should match #defines at top!
		//if (option_mask32 & 0x2000) OPT_verbose = 1; // -v
		//if (option_mask32 & 0x4000) OPT_pgrp = 1; // -P
		//if (option_mask32 & 0x8000) OPT_nostdin = 1; // -0
		//if (option_mask32 & 0x10000) OPT_nostdout = 1; // -1
		//if (option_mask32 & 0x20000) OPT_nostderr = 1; // -2
	}
	argv += optind;
	if (!argv || !*argv) bb_show_usage();

	if (OPT_pgrp) setsid();
	if (env_dir) edir(env_dir);
	if (root) {
		xchdir(root);
		xchroot(".");
	}
	slimit();
	if (nicelvl) {
		errno = 0;
		if (nice(nicelvl) == -1)
			bb_perror_msg_and_die("nice");
	}
	if (env_user) euidgid(env_user);
	if (set_user) suidgid(set_user);
	if (OPT_nostdin) close(0);
	if (OPT_nostdout) close(1);
	if (OPT_nostderr) close(2);
	BB_EXECVP(argv[0], argv);
	bb_perror_msg_and_die("exec %s", argv[0]);
}

static void setuidgid(int argc ATTRIBUTE_UNUSED, char **argv)
{
	const char *account;

	account = *++argv;
	if (!account) bb_show_usage();
	if (!*++argv) bb_show_usage();
	suidgid((char*)account);
	BB_EXECVP(argv[0], argv);
	bb_perror_msg_and_die("exec %s", argv[0]);
}

static void envuidgid(int argc ATTRIBUTE_UNUSED, char **argv)
{
	const char *account;

	account = *++argv;
	if (!account) bb_show_usage();
	if (!*++argv) bb_show_usage();
	euidgid((char*)account);
	BB_EXECVP(argv[0], argv);
	bb_perror_msg_and_die("exec %s", argv[0]);
}

static void envdir(int argc ATTRIBUTE_UNUSED, char **argv)
{
	const char *dir;

	dir = *++argv;
	if (!dir) bb_show_usage();
	if (!*++argv) bb_show_usage();
	edir(dir);
	BB_EXECVP(argv[0], argv);
	bb_perror_msg_and_die("exec %s", argv[0]);
}

static void softlimit(int argc ATTRIBUTE_UNUSED, char **argv)
{
	char *a,*c,*d,*f,*l,*m,*o,*p,*r,*s,*t;
	getopt32(argv, "+a:c:d:f:l:m:o:p:r:s:t:",
			&a,&c,&d,&f,&l,&m,&o,&p,&r,&s,&t);
	if (option_mask32 & 0x001) limita = xatoul(a); // -a
	if (option_mask32 & 0x002) limitc = xatoul(c); // -c
	if (option_mask32 & 0x004) limitd = xatoul(d); // -d
	if (option_mask32 & 0x008) limitf = xatoul(f); // -f
	if (option_mask32 & 0x010) limitl = xatoul(l); // -l
	if (option_mask32 & 0x020) limits = limitl = limita = limitd = xatoul(m); // -m
	if (option_mask32 & 0x040) limito = xatoul(o); // -o
	if (option_mask32 & 0x080) limitp = xatoul(p); // -p
	if (option_mask32 & 0x100) limitr = xatoul(r); // -r
	if (option_mask32 & 0x200) limits = xatoul(s); // -s
	if (option_mask32 & 0x400) limitt = xatoul(t); // -t
	argv += optind;
	if (!argv[0]) bb_show_usage();
	slimit();
	BB_EXECVP(argv[0], argv);
	bb_perror_msg_and_die("exec %s", argv[0]);
}
