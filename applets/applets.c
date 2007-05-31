/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell
 * Permission has been granted to redistribute this code under the GPL.
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include <assert.h>
#include "busybox.h"

/* Apparently uclibc defines __GLIBC__ (compat trick?). Oh well. */
#if ENABLE_STATIC && defined(__GLIBC__) && !defined(__UCLIBC__)
#warning Static linking against glibc produces buggy executables
#warning (glibc does not cope well with ld --gc-sections).
#warning See sources.redhat.com/bugzilla/show_bug.cgi?id=3400
#warning Note that glibc is unsuitable for static linking anyway.
#warning If you still want to do it, remove -Wl,--gc-sections
#warning from top-level Makefile and remove this warning.
#error Aborting compilation.
#endif


/* Declare <applet>_main() */
#define PROTOTYPES
#include "applets.h"
#undef PROTOTYPES

#if ENABLE_SHOW_USAGE && !ENABLE_FEATURE_COMPRESS_USAGE
/* Define usage_messages[] */
static const char usage_messages[] = ""
#define MAKE_USAGE
#include "usage.h"
#include "applets.h"
;
#undef MAKE_USAGE
#else
#define usage_messages 0
#endif /* SHOW_USAGE */

/* Define struct bb_applet applets[] */
#include "applets.h"
/* The -1 arises because of the {0,NULL,0,-1} entry. */
const unsigned short NUM_APPLETS = sizeof(applets) / sizeof(applets[0]) - 1;

const struct bb_applet *current_applet;
const char *applet_name ATTRIBUTE_EXTERNALLY_VISIBLE;
#if !BB_MMU
bool re_execed;
#endif

USE_FEATURE_SUID(static uid_t ruid;)  /* real uid */

#if ENABLE_FEATURE_SUID_CONFIG

/* applets[] is const, so we have to define this "override" structure */
static struct BB_suid_config {
	const struct bb_applet *m_applet;
	uid_t m_uid;
	gid_t m_gid;
	mode_t m_mode;
	struct BB_suid_config *m_next;
} *suid_config;

static bool suid_cfg_readable;

/* check if u is member of group g */
static int ingroup(uid_t u, gid_t g)
{
	struct group *grp = getgrgid(g);

	if (grp) {
		char **mem;

		for (mem = grp->gr_mem; *mem; mem++) {
			struct passwd *pwd = getpwnam(*mem);

			if (pwd && (pwd->pw_uid == u))
				return 1;
		}
	}
	return 0;
}

/* This should probably be a libbb routine.  In that case,
 * I'd probably rename it to something like bb_trimmed_slice.
 */
static char *get_trimmed_slice(char *s, char *e)
{
	/* First, consider the value at e to be nul and back up until we
	 * reach a non-space char.  Set the char after that (possibly at
	 * the original e) to nul. */
	while (e-- > s) {
		if (!isspace(*e)) {
			break;
		}
	}
	e[1] = '\0';

	/* Next, advance past all leading space and return a ptr to the
	 * first non-space char; possibly the terminating nul. */
	return skip_whitespace(s);
}

/* Don't depend on the tools to combine strings. */
static const char config_file[] = "/etc/busybox.conf";

/* We don't supply a value for the nul, so an index adjustment is
 * necessary below.  Also, we use unsigned short here to save some
 * space even though these are really mode_t values. */
static const unsigned short mode_mask[] = {
	/*  SST     sst                 xxx         --- */
	S_ISUID,    S_ISUID|S_IXUSR,    S_IXUSR,    0,	/* user */
	S_ISGID,    S_ISGID|S_IXGRP,    S_IXGRP,    0,	/* group */
	0,          S_IXOTH,            S_IXOTH,    0	/* other */
};

#define parse_error(x)  do { errmsg = x; goto pe_label; } while (0)

static void parse_config_file(void)
{
	struct BB_suid_config *sct_head;
	struct BB_suid_config *sct;
	const struct bb_applet *applet;
	FILE *f;
	const char *errmsg;
	char *s;
	char *e;
	int i;
	unsigned lc;
	smallint section;
	char buffer[256];
	struct stat st;

	assert(!suid_config); /* Should be set to NULL by bss init. */

	ruid = getuid();
	if (ruid == 0) /* run by root - don't need to even read config file */
		return;

	if ((stat(config_file, &st) != 0)       /* No config file? */
	 || !S_ISREG(st.st_mode)                /* Not a regular file? */
	 || (st.st_uid != 0)                    /* Not owned by root? */
	 || (st.st_mode & (S_IWGRP | S_IWOTH))  /* Writable by non-root? */
	 || !(f = fopen(config_file, "r"))      /* Cannot open? */
	) {
		return;
	}

	suid_cfg_readable = 1;
	sct_head = NULL;
	section = lc = 0;

	while (1) {
		s = buffer;

		if (!fgets(s, sizeof(buffer), f)) { /* Are we done? */
			if (ferror(f)) {   /* Make sure it wasn't a read error. */
				parse_error("reading");
			}
			fclose(f);
			suid_config = sct_head;	/* Success, so set the pointer. */
			return;
		}

		lc++;					/* Got a (partial) line. */

		/* If a line is too long for our buffer, we consider it an error.
		 * The following test does mistreat one corner case though.
		 * If the final line of the file does not end with a newline and
		 * yet exactly fills the buffer, it will be treated as too long
		 * even though there isn't really a problem.  But it isn't really
		 * worth adding code to deal with such an unlikely situation, and
		 * we do err on the side of caution.  Besides, the line would be
		 * too long if it did end with a newline. */
		if (!strchr(s, '\n') && !feof(f)) {
			parse_error("line too long");
		}

		/* Trim leading and trailing whitespace, ignoring comments, and
		 * check if the resulting string is empty. */
		s = get_trimmed_slice(s, strchrnul(s, '#'));
		if (!*s) {
			continue;
		}

		/* Check for a section header. */

		if (*s == '[') {
			/* Unlike the old code, we ignore leading and trailing
			 * whitespace for the section name.  We also require that
			 * there are no stray characters after the closing bracket. */
			e = strchr(s, ']');
			if (!e   /* Missing right bracket? */
			 || e[1] /* Trailing characters? */
			 || !*(s = get_trimmed_slice(s+1, e)) /* Missing name? */
			) {
				parse_error("section header");
			}
			/* Right now we only have one section so just check it.
			 * If more sections are added in the future, please don't
			 * resort to cascading ifs with multiple strcasecmp calls.
			 * That kind of bloated code is all too common.  A loop
			 * and a string table would be a better choice unless the
			 * number of sections is very small. */
			if (strcasecmp(s, "SUID") == 0) {
				section = 1;
				continue;
			}
			section = -1;	/* Unknown section so set to skip. */
			continue;
		}

		/* Process sections. */

		if (section == 1) {		/* SUID */
			/* Since we trimmed leading and trailing space above, we're
			 * now looking for strings of the form
			 *    <key>[::space::]*=[::space::]*<value>
			 * where both key and value could contain inner whitespace. */

			/* First get the key (an applet name in our case). */
			e = strchr(s, '=');
			if (e) {
				s = get_trimmed_slice(s, e);
			}
			if (!e || !*s) {	/* Missing '=' or empty key. */
				parse_error("keyword");
			}

			/* Ok, we have an applet name.  Process the rhs if this
			 * applet is currently built in and ignore it otherwise.
			 * Note: this can hide config file bugs which only pop
			 * up when the busybox configuration is changed. */
			applet = find_applet_by_name(s);
			if (applet) {
				/* Note: We currently don't check for duplicates!
				 * The last config line for each applet will be the
				 * one used since we insert at the head of the list.
				 * I suppose this could be considered a feature. */
				sct = xmalloc(sizeof(struct BB_suid_config));
				sct->m_applet = applet;
				sct->m_mode = 0;
				sct->m_next = sct_head;
				sct_head = sct;

				/* Get the specified mode. */

				e = skip_whitespace(e+1);

				for (i = 0; i < 3; i++) {
					/* There are 4 chars + 1 nul for each of user/group/other. */
					static const char mode_chars[] = "Ssx-\0" "Ssx-\0" "Ttx-";

					const char *q;
					q = strchrnul(mode_chars + 5*i, *e++);
					if (!*q) {
						parse_error("mode");
					}
					/* Adjust by -i to account for nul. */
					sct->m_mode |= mode_mask[(q - mode_chars) - i];
				}

				/* Now get the the user/group info. */

				s = skip_whitespace(e);

				/* Note: we require whitespace between the mode and the
				 * user/group info. */
				if ((s == e) || !(e = strchr(s, '.'))) {
					parse_error("<uid>.<gid>");
				}
				*e++ = '\0';

				/* We can't use get_ug_id here since it would exit()
				 * if a uid or gid was not found.  Oh well... */
				sct->m_uid = bb_strtoul(s, NULL, 10);
				if (errno) {
					struct passwd *pwd = getpwnam(s);
					if (!pwd) {
						parse_error("user");
					}
					sct->m_uid = pwd->pw_uid;
				}

				sct->m_gid = bb_strtoul(e, NULL, 10);
				if (errno) {
					struct group *grp;
					grp = getgrnam(e);
					if (!grp) {
						parse_error("group");
					}
					sct->m_gid = grp->gr_gid;
				}
			}
			continue;
		}

		/* Unknown sections are ignored. */

		/* Encountering configuration lines prior to seeing a
		 * section header is treated as an error.  This is how
		 * the old code worked, but it may not be desirable.
		 * We may want to simply ignore such lines in case they
		 * are used in some future version of busybox. */
		if (!section) {
			parse_error("keyword outside section");
		}

	} /* while (1) */

 pe_label:
	fprintf(stderr, "Parse error in %s, line %d: %s\n",
			config_file, lc, errmsg);

	fclose(f);
	/* Release any allocated memory before returning. */
	while (sct_head) {
		sct = sct_head->m_next;
		free(sct_head);
		sct_head = sct;
	}
}
#else
static inline void parse_config_file(void)
{
	USE_FEATURE_SUID(ruid = getuid();)
}
#endif /* FEATURE_SUID_CONFIG */


#if ENABLE_FEATURE_SUID
static void check_suid(const struct bb_applet *applet)
{
	gid_t rgid;  /* real gid */

	if (ruid == 0) /* set by parse_config_file() */
		return; /* run by root - no need to check more */
	rgid = getgid();

#if ENABLE_FEATURE_SUID_CONFIG
	if (suid_cfg_readable) {
		uid_t uid;
		struct BB_suid_config *sct;
		mode_t m;

		for (sct = suid_config; sct; sct = sct->m_next) {
			if (sct->m_applet == applet)
				goto found;
		}
		/* default: drop all privileges */
		xsetgid(rgid);
		xsetuid(ruid);
		return;
 found:
		m = sct->m_mode;
		if (sct->m_uid == ruid)
			/* same uid */
			m >>= 6;
		else if ((sct->m_gid == rgid) || ingroup(ruid, sct->m_gid))
			/* same group / in group */
			m >>= 3;

		if (!(m & S_IXOTH))           /* is x bit not set ? */
			bb_error_msg_and_die("you have no permission to run this applet!");

		/* _both_ sgid and group_exec have to be set for setegid */
		if ((sct->m_mode & (S_ISGID | S_IXGRP)) == (S_ISGID | S_IXGRP))
			rgid = sct->m_gid;
		/* else (no setegid) we will set egid = rgid */

		/* We set effective AND saved ids. If saved-id is not set
		 * like we do below, seteiud(0) can still later succeed! */
		if (setresgid(-1, rgid, rgid))
			bb_perror_msg_and_die("setresgid");

		/* do we have to set effective uid? */
		uid = ruid;
		if (sct->m_mode & S_ISUID)
			uid = sct->m_uid;
		/* else (no seteuid) we will set euid = ruid */

		if (setresuid(-1, uid, uid))
			bb_perror_msg_and_die("setresuid");
		return;
	}
#if !ENABLE_FEATURE_SUID_CONFIG_QUIET
	{
		static bool onetime = 0;

		if (!onetime) {
			onetime = 1;
			fprintf(stderr, "Using fallback suid method\n");
		}
	}
#endif
#endif

	if (applet->need_suid == _BB_SUID_ALWAYS) {
		/* Real uid is not 0. If euid isn't 0 too, suid bit
		 * is most probably not set on our executable */
		if (geteuid())
			bb_error_msg_and_die("applet requires root privileges!");
	} else if (applet->need_suid == _BB_SUID_NEVER) {
		xsetgid(rgid);  /* drop all privileges */
		xsetuid(ruid);
	}
}
#else
#define check_suid(x) ((void)0)
#endif /* FEATURE_SUID */


#if ENABLE_FEATURE_COMPRESS_USAGE

#include "usage_compressed.h"
#include "unarchive.h"

static const char *unpack_usage_messages(void)
{
	char *outbuf = NULL;
	bunzip_data *bd;
	int i;

	i = start_bunzip(&bd,
			/* src_fd: */ -1,
			/* inbuf:  */ packed_usage,
			/* len:    */ sizeof(packed_usage));
	/* read_bunzip can longjmp to start_bunzip, and ultimately
	 * end up here with i != 0 on read data errors! Not trivial */
	if (!i) {
		/* Cannot use xmalloc: will leak bd in NOFORK case! */
		outbuf = malloc_or_warn(SIZEOF_usage_messages);
		if (outbuf)
			read_bunzip(bd, outbuf, SIZEOF_usage_messages);
	}
	dealloc_bunzip(bd);
	return outbuf;
}
#define dealloc_usage_messages(s) free(s)

#else

#define unpack_usage_messages() usage_messages
#define dealloc_usage_messages(s) ((void)(s))

#endif /* FEATURE_COMPRESS_USAGE */


void bb_show_usage(void)
{
	if (ENABLE_SHOW_USAGE) {
		const char *format_string;
		const char *p;
		const char *usage_string = p = unpack_usage_messages();
		int i;

		i = current_applet - applets;
		while (i) {
			while (*p++) continue;
			i--;
		}

		format_string = "%s\n\nUsage: %s %s\n\n";
		if (*p == '\b')
			format_string = "%s\n\nNo help available.\n\n";
		fprintf(stderr, format_string, bb_msg_full_version,
					applet_name, p);
		dealloc_usage_messages((char*)usage_string);
	}
	xfunc_die();
}


static int applet_name_compare(const void *name, const void *vapplet)
{
	const struct bb_applet *applet = vapplet;

	return strcmp(name, applet->name);
}

const struct bb_applet *find_applet_by_name(const char *name)
{
	/* Do a binary search to find the applet entry given the name. */
	return bsearch(name, applets, NUM_APPLETS, sizeof(applets[0]),
				applet_name_compare);
}


#if ENABLE_FEATURE_INSTALLER
/* create (sym)links for each applet */
static void install_links(const char *busybox, int use_symbolic_links)
{
	/* directory table
	 * this should be consistent w/ the enum,
	 * busybox.h::bb_install_loc_t, or else... */
	static const char usr_bin [] = "/usr/bin";
	static const char usr_sbin[] = "/usr/sbin";
	static const char *const install_dir[] = {
		&usr_bin [8], /* "", equivalent to "/" for concat_path_file() */
		&usr_bin [4], /* "/bin" */
		&usr_sbin[4], /* "/sbin" */
		usr_bin,
		usr_sbin
	};

	int (*lf)(const char *, const char *) = link;
	char *fpc;
	int i;
	int rc;

	if (use_symbolic_links)
		lf = symlink;

	for (i = 0; applets[i].name != NULL; i++) {
		fpc = concat_path_file(
				install_dir[applets[i].install_loc],
				applets[i].name);
		rc = lf(busybox, fpc);
		if (rc != 0 && errno != EEXIST) {
			bb_perror_msg("%s", fpc);
		}
		free(fpc);
	}
}
#else
#define install_links(x,y) ((void)0)
#endif /* FEATURE_INSTALLER */


/* If we were called as "busybox..." */
static int busybox_main(char **argv)
{
	if (!argv[1]) {
		/* Called without arguments */
		const struct bb_applet *a;
		int col, output_width;
 help:
		output_width = 80;
		if (ENABLE_FEATURE_AUTOWIDTH) {
			/* Obtain the terminal width.  */
			get_terminal_width_height(0, &output_width, NULL);
		}
		/* leading tab and room to wrap */
		output_width -= sizeof("start-stop-daemon, ") + 8;

		printf("%s\n"
		       "Copyright (C) 1998-2006  Erik Andersen, Rob Landley, and others.\n"
		       "Licensed under GPLv2.  See source distribution for full notice.\n"
		       "\n"
		       "Usage: busybox [function] [arguments]...\n"
		       "   or: [function] [arguments]...\n"
		       "\n"
		       "\tBusyBox is a multi-call binary that combines many common Unix\n"
		       "\tutilities into a single executable.  Most people will create a\n"
		       "\tlink to busybox for each function they wish to use and BusyBox\n"
		       "\twill act like whatever it was invoked as!\n"
		       "\nCurrently defined functions:\n", bb_msg_full_version);
		col = 0;
		a = applets;
		while (a->name) {
			if (col > output_width) {
				puts(",");
				col = 0;
			}
			col += printf("%s%s", (col ? ", " : "\t"), a->name);
			a++;
		}
		puts("\n");
		return 0;
	}

	if (ENABLE_FEATURE_INSTALLER && strcmp(argv[1], "--install") == 0) {
		int use_symbolic_links = 0;
		char *busybox;

		/* to use symlinks, or not to use symlinks... */
		if (argv[2])
			if (strcmp(argv[2], "-s") == 0)
				use_symbolic_links = 1;

		/* link */
		busybox = xmalloc_readlink_or_warn("/proc/self/exe");
		if (!busybox)
			return 1;
		install_links(busybox, use_symbolic_links);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(busybox);
		return 0;
	}

	if (strcmp(argv[1], "--help") == 0) {
		/* "busybox --help [<applet>]" */
		if (!argv[2])
			goto help;
		/* convert to "<applet> --help" */
		argv[0] = argv[2];
		argv[2] = NULL;
	} else {
		/* "busybox <applet> arg1 arg2 ..." */
		argv++;
	}
	/* we want "<argv[0]>: applet not found", not "busybox: ..." */
	applet_name = argv[0];
	run_applet_and_exit(argv[0], argv);
	bb_error_msg_and_die("applet not found");
}

void run_current_applet_and_exit(char **argv)
{
	int argc = 1;

	while (argv[argc])
		argc++;

	/* Reinit some shared global data */
	optind = 1;
	xfunc_error_retval = EXIT_FAILURE;

	applet_name = current_applet->name;
	if (argc == 2 && !strcmp(argv[1], "--help"))
		bb_show_usage();
	if (ENABLE_FEATURE_SUID)
		check_suid(current_applet);
	exit(current_applet->main(argc, argv));
}

void run_applet_and_exit(const char *name, char **argv)
{
	current_applet = find_applet_by_name(name);
	if (current_applet)
		run_current_applet_and_exit(argv);
	if (!strncmp(name, "busybox", 7))
		exit(busybox_main(argv));
}


#ifdef __GLIBC__
/* Make it reside in R/W memory: */
int *const bb_errno __attribute__ ((section (".data")));
#endif

int main(int argc, char **argv)
{
	const char *s;

#ifdef __GLIBC__
	(*(int **)&bb_errno) = __errno_location();
#endif

#if !BB_MMU
	/* NOMMU re-exec trick sets high-order bit in first byte of name */
	if (argv[0][0] & 0x80) {
		re_execed = 1;
		argv[0][0] &= 0x7f;
	}
#endif
	applet_name = argv[0];
	if (applet_name[0] == '-')
		applet_name++;
	s = strrchr(applet_name, '/');
	if (s)
		applet_name = s + 1;

	parse_config_file(); /* ...maybe, if FEATURE_SUID_CONFIG */

	/* Set locale for everybody except 'init' */
	if (ENABLE_LOCALE_SUPPORT && getpid() != 1)
		setlocale(LC_ALL, "");

	run_applet_and_exit(applet_name, argv);
	bb_error_msg_and_die("applet not found");
}
