/* vi: set sw=4 ts=4: */
/*
 * Mini ps implementation(s) for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#if ENABLE_DESKTOP

/* Print value to buf, max size+1 chars (including trailing '\0') */

static void func_user(char *buf, int size, const procps_status_t *ps)
{
	safe_strncpy(buf, get_cached_username(ps->uid), size+1);
}

static void func_comm(char *buf, int size, const procps_status_t *ps)
{
	safe_strncpy(buf, ps->comm, size+1);
}

static void func_args(char *buf, int size, const procps_status_t *ps)
{
	buf[0] = '\0';
	if (ps->cmd)
		safe_strncpy(buf, ps->cmd, size+1);
	else if (size >= 2)
		sprintf(buf, "[%.*s]", size-2, ps->comm);
}

static void func_pid(char *buf, int size, const procps_status_t *ps)
{
	sprintf(buf, "%*u", size, ps->pid);
}

static void func_ppid(char *buf, int size, const procps_status_t *ps)
{
	sprintf(buf, "%*u", size, ps->ppid);
}

static void func_pgid(char *buf, int size, const procps_status_t *ps)
{
	sprintf(buf, "%*u", size, ps->pgid);
}

static void put_u(char *buf, int size, unsigned u)
{
	char buf5[5];
	smart_ulltoa5( ((unsigned long long)u) << 10, buf5);
	sprintf(buf, "%.*s", size, buf5);
}

static void func_vsz(char *buf, int size, const procps_status_t *ps)
{
	put_u(buf, size, ps->vsz);
}

static void func_rss(char *buf, int size, const procps_status_t *ps)
{
	put_u(buf, size, ps->rss);
}

static void func_tty(char *buf, int size, const procps_status_t *ps)
{
	safe_strncpy(buf, ps->tty_str, size+1);
}
/*
static void func_nice(char *buf, int size, const procps_status_t *ps)
{
	ps->???
}

static void func_etime(char *buf, int size, const procps_status_t *ps)
{
	elapled time [[dd-]hh:]mm:ss
}

static void func_time(char *buf, int size, const procps_status_t *ps)
{
	cumulative time [[dd-]hh:]mm:ss
}

static void func_pcpu(char *buf, int size, const procps_status_t *ps)
{
}
*/

typedef struct {
	uint16_t width;
	char name[6];
	const char *header;
	void (*f)(char *buf, int size, const procps_status_t *ps);
	int ps_flags;
} ps_out_t;

static const ps_out_t out_spec[] = {
// Mandated by POSIX:
	{ 8                  , "user"  ,"USER"   ,func_user  ,PSSCAN_UIDGID          },
	{ 16                 , "comm"  ,"COMMAND",func_comm  ,PSSCAN_COMM            },
	{ 256                , "args"  ,"COMMAND",func_args  ,PSSCAN_CMD|PSSCAN_COMM },
	{ 5                  , "pid"   ,"PID"    ,func_pid   ,PSSCAN_PID             },
	{ 5                  , "ppid"  ,"PPID"   ,func_ppid  ,PSSCAN_PPID            },
	{ 5                  , "pgid"  ,"PGID"   ,func_pgid  ,PSSCAN_PGID            },
//	{ sizeof("ELAPSED")-1, "etime" ,"ELAPSED",func_etime ,PSSCAN_                },
//	{ sizeof("GROUP"  )-1, "group" ,"GROUP"  ,func_group ,PSSCAN_UIDGID          },
//	{ sizeof("NI"     )-1, "nice"  ,"NI"     ,func_nice  ,PSSCAN_                },
//	{ sizeof("%CPU"   )-1, "pcpu"  ,"%CPU"   ,func_pcpu  ,PSSCAN_                },
//	{ sizeof("RGROUP" )-1, "rgroup","RGROUP" ,func_rgroup,PSSCAN_UIDGID          },
//	{ sizeof("RUSER"  )-1, "ruser" ,"RUSER"  ,func_ruser ,PSSCAN_UIDGID          },
//	{ sizeof("TIME"   )-1, "time"  ,"TIME"   ,func_time  ,PSSCAN_                },
	{ 6                  , "tty"   ,"TT"     ,func_tty   ,PSSCAN_TTY             },
	{ 4                  , "vsz"   ,"VSZ"    ,func_vsz   ,PSSCAN_VSZ             },
// Not mandated by POSIX, but useful:
	{ 4                  , "rss"   ,"RSS"    ,func_rss   ,PSSCAN_RSS             },
};

#define VEC_SIZE(v) ( sizeof(v) / sizeof((v)[0]) )

#define DEFAULT_O_STR "pid,user" /* TODO: ,vsz,stat */ ",args"

struct globals {
	ps_out_t* out;
	int out_cnt;
	int print_header;
	int need_flags;
	char *buffer;
	unsigned terminal_width;
	char default_o[sizeof(DEFAULT_O_STR)];
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define out            (G.out           )
#define out_cnt        (G.out_cnt       )
#define print_header   (G.print_header  )
#define need_flags     (G.need_flags    )
#define buffer         (G.buffer        )
#define terminal_width (G.terminal_width)
#define default_o      (G.default_o     )

static ps_out_t* new_out_t(void)
{
	int i = out_cnt++;
	out = xrealloc(out, out_cnt * sizeof(*out));
	return &out[i];
}

static const ps_out_t* find_out_spec(const char *name)
{
	int i;
	for (i = 0; i < VEC_SIZE(out_spec); i++) {
		if (!strcmp(name, out_spec[i].name))
			return &out_spec[i];
	}
	bb_error_msg_and_die("bad -o argument '%s'", name);
}

static void parse_o(char* opt)
{
	ps_out_t* new;
	// POSIX: "-o is blank- or comma-separated list" (FIXME)
	char *comma, *equal;
	while (1) {
		comma = strchr(opt, ',');
		equal = strchr(opt, '=');
		if (comma && (!equal || equal > comma)) {
			*comma = '\0';
			*new_out_t() = *find_out_spec(opt);
			*comma = ',';
			opt = comma + 1;
			continue;
		}
		break;
	}
	// opt points to last spec in comma separated list.
	// This one can have =HEADER part.
	new = new_out_t();
	if (equal)
		*equal = '\0';
	*new = *find_out_spec(opt);
	if (equal) {
		*equal = '=';
		new->header = equal + 1;
		// POSIX: the field widths shall be ... at least as wide as
		// the header text (default or overridden value).
		// If the header text is null, such as -o user=,
		// the field width shall be at least as wide as the
		// default header text
		if (new->header[0]) {
			new->width = strlen(new->header);
			print_header = 1;
		}
	} else
		print_header = 1;
}

static void post_process(void)
{
	int i;
	int width = 0;
	for (i = 0; i < out_cnt; i++) {
		need_flags |= out[i].ps_flags;
		if (out[i].header[0]) {
			print_header = 1;
		}
		width += out[i].width + 1; /* "FIELD " */
	}
	buffer = xmalloc(width + 1); /* for trailing \0 */
}

static void format_header(void)
{
	int i;
	ps_out_t* op;
	char *p;

	if (!print_header)
		return;
	p = buffer;
	i = 0;
	if (out_cnt) {
		while (1) {
			op = &out[i];
			if (++i == out_cnt) /* do not pad last field */
				break;
			p += sprintf(p, "%-*s ", op->width, op->header);
		}
		strcpy(p, op->header);
	}
	printf("%.*s\n", terminal_width, buffer);
}

static void format_process(const procps_status_t *ps)
{
	int i, len;
	char *p = buffer;
	i = 0;
	if (out_cnt) while (1) {
		out[i].f(p, out[i].width, ps);
		// POSIX: Any field need not be meaningful in all
		// implementations. In such a case a hyphen ( '-' )
		// should be output in place of the field value.
		if (!p[0]) {
			p[0] = '-';
			p[1] = '\0';
		}
		len = strlen(p);
		p += len;
		len = out[i].width - len + 1;
		if (++i == out_cnt) /* do not pad last field */
			break;
		p += sprintf(p, "%*s", len, "");
	}
	printf("%.*s\n", terminal_width, buffer);
}

int ps_main(int argc, char **argv);
int ps_main(int argc, char **argv)
{
	procps_status_t *p;
	llist_t* opt_o = NULL;

	/* Cannot be const: parse_o() will choke */
	strcpy(default_o, DEFAULT_O_STR);

	// POSIX:
	// -a  Write information for all processes associated with terminals
	//     Implementations may omit session leaders from this list
	// -A  Write information for all processes
	// -d  Write information for all processes, except session leaders
	// -e  Write information for all processes (equivalent to -A.)
	// -f  Generate a full listing
	// -l  Generate a long listing
	// -o col1,col2,col3=header
	//     Select which columns to display
	/* We allow (and ignore) most of the above. FIXME */
	opt_complementary = "o::";
	getopt32(argc, argv, "o:aAdefl", &opt_o);
	if (opt_o) {
		do {
			parse_o(opt_o->data);
			opt_o = opt_o->link;
		} while (opt_o);
	} else
		parse_o(default_o);
	post_process();

	/* Was INT_MAX, but some libc's go belly up with printf("%.*s")
	 * and such large widths */
	terminal_width = 30000;
	if (isatty(1)) {
		get_terminal_width_height(1, &terminal_width, NULL);
		terminal_width--;
	}
	format_header();

	p = NULL;
	while ((p = procps_scan(p, need_flags))) {
		format_process(p);
	}

	return EXIT_SUCCESS;
}


#else /* !ENABLE_DESKTOP */


int ps_main(int argc, char **argv);
int ps_main(int argc, char **argv)
{
	procps_status_t *p = NULL;
	int i, len;
	SKIP_SELINUX(const) int use_selinux = 0;
	USE_SELINUX(security_context_t sid = NULL;)
#if !ENABLE_FEATURE_PS_WIDE
	enum { terminal_width = 79 };
#else
	int terminal_width;
	int w_count = 0;
#endif

#if ENABLE_FEATURE_PS_WIDE || ENABLE_SELINUX
#if ENABLE_FEATURE_PS_WIDE
	opt_complementary = "-:ww";
	USE_SELINUX(i =) getopt32(argc, argv, USE_SELINUX("Z") "w", &w_count);
	/* if w is given once, GNU ps sets the width to 132,
	 * if w is given more than once, it is "unlimited"
	 */
	if (w_count) {
		terminal_width = (w_count==1) ? 132 : INT_MAX;
	} else {
		get_terminal_width_height(1, &terminal_width, NULL);
		/* Go one less... */
		terminal_width--;
	}
#else /* only ENABLE_SELINUX */
	i = getopt32(argc, argv, "Z");
#endif
#if ENABLE_SELINUX
	if ((i & 1) && is_selinux_enabled())
		use_selinux = 1;
#endif
#endif /* ENABLE_FEATURE_PS_WIDE || ENABLE_SELINUX */

	if (use_selinux)
		puts("  PID Context                          Stat Command");
	else
		puts("  PID  Uid        VSZ Stat Command");

	while ((p = procps_scan(p, 0
			| PSSCAN_PID
			| PSSCAN_UIDGID
			| PSSCAN_STATE
			| PSSCAN_VSZ
			| PSSCAN_CMD
	))) {
		char *namecmd = p->cmd;
#if ENABLE_SELINUX
		if (use_selinux) {
			char sbuf[128];
			len = sizeof(sbuf);

			if (is_selinux_enabled()) {
				if (getpidcon(p->pid, &sid) < 0)
					sid = NULL;
			}

			if (sid) {
				/* I assume sid initialized with NULL */
				len = strlen(sid) + 1;
				safe_strncpy(sbuf, sid, len);
				freecon(sid);
				sid = NULL;
			} else {
				safe_strncpy(sbuf, "unknown", 7);
			}
			len = printf("%5u %-32s %s ", p->pid, sbuf, p->state);
		} else
#endif
		{
			const char *user = get_cached_username(p->uid);
			if (p->vsz == 0)
				len = printf("%5u %-8s        %s ",
					p->pid, user, p->state);
			else
				len = printf("%5u %-8s %6u %s ",
					p->pid, user, p->vsz, p->state);
		}

		i = terminal_width-len;

		if (namecmd && namecmd[0]) {
			if (i < 0)
				i = 0;
			if (strlen(namecmd) > (size_t)i)
				namecmd[i] = 0;
			puts(namecmd);
		} else {
			namecmd = p->comm;
			if (i < 2)
				i = 2;
			if (strlen(namecmd) > ((size_t)i-2))
				namecmd[i-2] = 0;
			printf("[%s]\n", namecmd);
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		clear_username_cache();
	return EXIT_SUCCESS;
}

#endif /* ENABLE_DESKTOP */
