/* vi: set sw=4 ts=4: */
/* stty -- change and print terminal line settings
   Copyright (C) 1990-1999 Free Software Foundation, Inc.

   Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
*/
/* Usage: stty [-ag] [-F device] [setting...]

   Options:
   -a Write all current settings to stdout in human-readable form.
   -g Write all current settings to stdout in stty-readable form.
   -F Open and use the specified device instead of stdin

   If no args are given, write to stdout the baud rate and settings that
   have been changed from their defaults.  Mode reading and changes
   are done on the specified device, or stdin if none was specified.

   David MacKenzie <djm@gnu.ai.mit.edu>

   Special for busybox ported by Vladimir Oleynik <dzo@simtreas.ru> 2001

   */

#include "busybox.h"

#define STREQ(a, b) (strcmp ((a), (b)) == 0)


#ifndef _POSIX_VDISABLE
# define _POSIX_VDISABLE ((unsigned char) 0)
#endif

#define Control(c) ((c) & 0x1f)
/* Canonical values for control characters. */
#ifndef CINTR
# define CINTR Control ('c')
#endif
#ifndef CQUIT
# define CQUIT 28
#endif
#ifndef CERASE
# define CERASE 127
#endif
#ifndef CKILL
# define CKILL Control ('u')
#endif
#ifndef CEOF
# define CEOF Control ('d')
#endif
#ifndef CEOL
# define CEOL _POSIX_VDISABLE
#endif
#ifndef CSTART
# define CSTART Control ('q')
#endif
#ifndef CSTOP
# define CSTOP Control ('s')
#endif
#ifndef CSUSP
# define CSUSP Control ('z')
#endif
#if defined(VEOL2) && !defined(CEOL2)
# define CEOL2 _POSIX_VDISABLE
#endif
/* ISC renamed swtch to susp for termios, but we'll accept either name.  */
#if defined(VSUSP) && !defined(VSWTCH)
# define VSWTCH VSUSP
# define CSWTCH CSUSP
#endif
#if defined(VSWTCH) && !defined(CSWTCH)
# define CSWTCH _POSIX_VDISABLE
#endif

/* SunOS 5.3 loses (^Z doesn't work) if `swtch' is the same as `susp'.
   So the default is to disable `swtch.'  */
#if defined (__sparc__) && defined (__svr4__)
# undef CSWTCH
# define CSWTCH _POSIX_VDISABLE
#endif

#if defined(VWERSE) && !defined (VWERASE)       /* AIX-3.2.5 */
# define VWERASE VWERSE
#endif
#if defined(VDSUSP) && !defined (CDSUSP)
# define CDSUSP Control ('y')
#endif
#if !defined(VREPRINT) && defined(VRPRNT)       /* Irix 4.0.5 */
# define VREPRINT VRPRNT
#endif
#if defined(VREPRINT) && !defined(CRPRNT)
# define CRPRNT Control ('r')
#endif
#if defined(VWERASE) && !defined(CWERASE)
# define CWERASE Control ('w')
#endif
#if defined(VLNEXT) && !defined(CLNEXT)
# define CLNEXT Control ('v')
#endif
#if defined(VDISCARD) && !defined(VFLUSHO)
# define VFLUSHO VDISCARD
#endif
#if defined(VFLUSH) && !defined(VFLUSHO)        /* Ultrix 4.2 */
# define VFLUSHO VFLUSH
#endif
#if defined(CTLECH) && !defined(ECHOCTL)        /* Ultrix 4.3 */
# define ECHOCTL CTLECH
#endif
#if defined(TCTLECH) && !defined(ECHOCTL)       /* Ultrix 4.2 */
# define ECHOCTL TCTLECH
#endif
#if defined(CRTKIL) && !defined(ECHOKE)         /* Ultrix 4.2 and 4.3 */
# define ECHOKE CRTKIL
#endif
#if defined(VFLUSHO) && !defined(CFLUSHO)
# define CFLUSHO Control ('o')
#endif
#if defined(VSTATUS) && !defined(CSTATUS)
# define CSTATUS Control ('t')
#endif

/* Which speeds to set.  */
enum speed_setting {
	input_speed, output_speed, both_speeds
};

/* Which member(s) of `struct termios' a mode uses.  */
enum mode_type {
	/* Do NOT change the order or values, as mode_type_flag()
	 * depends on them. */
	control, input, output, local, combination
};


static const char evenp     [] = "evenp";
static const char raw       [] = "raw";
static const char stty_min  [] = "min";
static const char stty_time [] = "time";
static const char stty_swtch[] = "swtch";
static const char stty_eol  [] = "eol";
static const char stty_eof  [] = "eof";
static const char parity    [] = "parity";
static const char stty_oddp [] = "oddp";
static const char stty_nl   [] = "nl";
static const char stty_ek   [] = "ek";
static const char stty_sane [] = "sane";
static const char cbreak    [] = "cbreak";
static const char stty_pass8[] = "pass8";
static const char litout    [] = "litout";
static const char cooked    [] = "cooked";
static const char decctlq   [] = "decctlq";
static const char stty_tabs [] = "tabs";
static const char stty_lcase[] = "lcase";
static const char stty_LCASE[] = "LCASE";
static const char stty_crt  [] = "crt";
static const char stty_dec  [] = "dec";


/* Flags for `struct mode_info'. */
#define SANE_SET 1              /* Set in `sane' mode.                  */
#define SANE_UNSET 2            /* Unset in `sane' mode.                */
#define REV 4                   /* Can be turned off by prepending `-'. */
#define OMIT 8                  /* Don't display value.                 */

/* Each mode.  */
struct mode_info {
	const char *name;       /* Name given on command line.           */
	/* enum mode_type type; */
	char type;              /* Which structure element to change.    */
	char flags;             /* Setting and display options.          */
	unsigned short mask;     /* Other bits to turn off for this mode. */
	unsigned long bits;     /* Bits to set for this mode.            */
};

#define MI_ENTRY(N,T,F,B,M) { N, T, F, M, B }

static const struct mode_info mode_info[] = {
	MI_ENTRY("parenb",   control,     REV,               PARENB,     0 ),
	MI_ENTRY("parodd",   control,     REV,               PARODD,     0 ),
	MI_ENTRY("cs5",      control,     0,                 CS5,     CSIZE),
	MI_ENTRY("cs6",      control,     0,                 CS6,     CSIZE),
	MI_ENTRY("cs7",      control,     0,                 CS7,     CSIZE),
	MI_ENTRY("cs8",      control,     0,                 CS8,     CSIZE),
	MI_ENTRY("hupcl",    control,     REV,               HUPCL,      0 ),
	MI_ENTRY("hup",      control,     REV        | OMIT, HUPCL,      0 ),
	MI_ENTRY("cstopb",   control,     REV,               CSTOPB,     0 ),
	MI_ENTRY("cread",    control,     SANE_SET   | REV,  CREAD,      0 ),
	MI_ENTRY("clocal",   control,     REV,               CLOCAL,     0 ),
#ifdef CRTSCTS
	MI_ENTRY("crtscts",  control,     REV,               CRTSCTS,    0 ),
#endif
	MI_ENTRY("ignbrk",   input,       SANE_UNSET | REV,  IGNBRK,     0 ),
	MI_ENTRY("brkint",   input,       SANE_SET   | REV,  BRKINT,     0 ),
	MI_ENTRY("ignpar",   input,       REV,               IGNPAR,     0 ),
	MI_ENTRY("parmrk",   input,       REV,               PARMRK,     0 ),
	MI_ENTRY("inpck",    input,       REV,               INPCK,      0 ),
	MI_ENTRY("istrip",   input,       REV,               ISTRIP,     0 ),
	MI_ENTRY("inlcr",    input,       SANE_UNSET | REV,  INLCR,      0 ),
	MI_ENTRY("igncr",    input,       SANE_UNSET | REV,  IGNCR,      0 ),
	MI_ENTRY("icrnl",    input,       SANE_SET   | REV,  ICRNL,      0 ),
	MI_ENTRY("ixon",     input,       REV,               IXON,       0 ),
	MI_ENTRY("ixoff",    input,       SANE_UNSET | REV,  IXOFF,      0 ),
	MI_ENTRY("tandem",   input,       REV        | OMIT, IXOFF,      0 ),
#ifdef IUCLC
	MI_ENTRY("iuclc",    input,       SANE_UNSET | REV,  IUCLC,      0 ),
#endif
#ifdef IXANY
	MI_ENTRY("ixany",    input,       SANE_UNSET | REV,  IXANY,      0 ),
#endif
#ifdef IMAXBEL
	MI_ENTRY("imaxbel",  input,       SANE_SET   | REV,  IMAXBEL,    0 ),
#endif
	MI_ENTRY("opost",    output,      SANE_SET   | REV,  OPOST,      0 ),
#ifdef OLCUC
	MI_ENTRY("olcuc",    output,      SANE_UNSET | REV,  OLCUC,      0 ),
#endif
#ifdef OCRNL
	MI_ENTRY("ocrnl",    output,      SANE_UNSET | REV,  OCRNL,      0 ),
#endif
#ifdef ONLCR
	MI_ENTRY("onlcr",    output,      SANE_SET   | REV,  ONLCR,      0 ),
#endif
#ifdef ONOCR
	MI_ENTRY("onocr",    output,      SANE_UNSET | REV,  ONOCR,      0 ),
#endif
#ifdef ONLRET
	MI_ENTRY("onlret",   output,      SANE_UNSET | REV,  ONLRET,     0 ),
#endif
#ifdef OFILL
	MI_ENTRY("ofill",    output,      SANE_UNSET | REV,  OFILL,      0 ),
#endif
#ifdef OFDEL
	MI_ENTRY("ofdel",    output,      SANE_UNSET | REV,  OFDEL,      0 ),
#endif
#ifdef NLDLY
	MI_ENTRY("nl1",      output,      SANE_UNSET,        NL1,     NLDLY),
	MI_ENTRY("nl0",      output,      SANE_SET,          NL0,     NLDLY),
#endif
#ifdef CRDLY
	MI_ENTRY("cr3",      output,      SANE_UNSET,        CR3,     CRDLY),
	MI_ENTRY("cr2",      output,      SANE_UNSET,        CR2,     CRDLY),
	MI_ENTRY("cr1",      output,      SANE_UNSET,        CR1,     CRDLY),
	MI_ENTRY("cr0",      output,      SANE_SET,          CR0,     CRDLY),
#endif

#ifdef TABDLY
	MI_ENTRY("tab3",     output,      SANE_UNSET,        TAB3,   TABDLY),
	MI_ENTRY("tab2",     output,      SANE_UNSET,        TAB2,   TABDLY),
	MI_ENTRY("tab1",     output,      SANE_UNSET,        TAB1,   TABDLY),
	MI_ENTRY("tab0",     output,      SANE_SET,          TAB0,   TABDLY),
#else
# ifdef OXTABS
	MI_ENTRY("tab3",     output,      SANE_UNSET,        OXTABS,     0 ),
# endif
#endif

#ifdef BSDLY
	MI_ENTRY("bs1",      output,      SANE_UNSET,        BS1,     BSDLY),
	MI_ENTRY("bs0",      output,      SANE_SET,          BS0,     BSDLY),
#endif
#ifdef VTDLY
	MI_ENTRY("vt1",      output,      SANE_UNSET,        VT1,     VTDLY),
	MI_ENTRY("vt0",      output,      SANE_SET,          VT0,     VTDLY),
#endif
#ifdef FFDLY
	MI_ENTRY("ff1",      output,      SANE_UNSET,        FF1,     FFDLY),
	MI_ENTRY("ff0",      output,      SANE_SET,          FF0,     FFDLY),
#endif
	MI_ENTRY("isig",     local,       SANE_SET   | REV,  ISIG,       0 ),
	MI_ENTRY("icanon",   local,       SANE_SET   | REV,  ICANON,     0 ),
#ifdef IEXTEN
	MI_ENTRY("iexten",   local,       SANE_SET   | REV,  IEXTEN,     0 ),
#endif
	MI_ENTRY("echo",     local,       SANE_SET   | REV,  ECHO,       0 ),
	MI_ENTRY("echoe",    local,       SANE_SET   | REV,  ECHOE,      0 ),
	MI_ENTRY("crterase", local,       REV        | OMIT, ECHOE,      0 ),
	MI_ENTRY("echok",    local,       SANE_SET   | REV,  ECHOK,      0 ),
	MI_ENTRY("echonl",   local,       SANE_UNSET | REV,  ECHONL,     0 ),
	MI_ENTRY("noflsh",   local,       SANE_UNSET | REV,  NOFLSH,     0 ),
#ifdef XCASE
	MI_ENTRY("xcase",    local,       SANE_UNSET | REV,  XCASE,      0 ),
#endif
#ifdef TOSTOP
	MI_ENTRY("tostop",   local,       SANE_UNSET | REV,  TOSTOP,     0 ),
#endif
#ifdef ECHOPRT
	MI_ENTRY("echoprt",  local,       SANE_UNSET | REV,  ECHOPRT,    0 ),
	MI_ENTRY("prterase", local,       REV | OMIT,        ECHOPRT,    0 ),
#endif
#ifdef ECHOCTL
	MI_ENTRY("echoctl",  local,       SANE_SET   | REV,  ECHOCTL,    0 ),
	MI_ENTRY("ctlecho",  local,       REV        | OMIT, ECHOCTL,    0 ),
#endif
#ifdef ECHOKE
	MI_ENTRY("echoke",   local,       SANE_SET   | REV,  ECHOKE,     0 ),
	MI_ENTRY("crtkill",  local,       REV        | OMIT, ECHOKE,     0 ),
#endif
	MI_ENTRY(evenp,      combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(parity,     combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(stty_oddp,  combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(stty_nl,    combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(stty_ek,    combination, OMIT,              0,          0 ),
	MI_ENTRY(stty_sane,  combination, OMIT,              0,          0 ),
	MI_ENTRY(cooked,     combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(raw,        combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(stty_pass8, combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(litout,     combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(cbreak,     combination, REV        | OMIT, 0,          0 ),
#ifdef IXANY
	MI_ENTRY(decctlq,    combination, REV        | OMIT, 0,          0 ),
#endif
#if defined (TABDLY) || defined (OXTABS)
	MI_ENTRY(stty_tabs,  combination, REV        | OMIT, 0,          0 ),
#endif
#if defined(XCASE) && defined(IUCLC) && defined(OLCUC)
	MI_ENTRY(stty_lcase, combination, REV        | OMIT, 0,          0 ),
	MI_ENTRY(stty_LCASE, combination, REV        | OMIT, 0,          0 ),
#endif
	MI_ENTRY(stty_crt,   combination, OMIT,              0,          0 ),
	MI_ENTRY(stty_dec,   combination, OMIT,              0,          0 ),
};

enum {
	NUM_mode_info =
	(sizeof(mode_info) / sizeof(struct mode_info))
};

/* Control character settings.  */
struct control_info {
	const char *name;                       /* Name given on command line.  */
	unsigned char saneval;          /* Value to set for `stty sane'.  */
	unsigned char offset;                           /* Offset in c_cc.  */
};

/* Control characters. */

static const struct control_info control_info[] = {
	{"intr",     CINTR,   VINTR},
	{"quit",     CQUIT,   VQUIT},
	{"erase",    CERASE,  VERASE},
	{"kill",     CKILL,   VKILL},
	{stty_eof,   CEOF,    VEOF},
	{stty_eol,   CEOL,    VEOL},
#ifdef VEOL2
	{"eol2",     CEOL2,   VEOL2},
#endif
#ifdef VSWTCH
	{stty_swtch, CSWTCH,  VSWTCH},
#endif
	{"start",    CSTART,  VSTART},
	{"stop",     CSTOP,   VSTOP},
	{"susp",     CSUSP,   VSUSP},
#ifdef VDSUSP
	{"dsusp",    CDSUSP,  VDSUSP},
#endif
#ifdef VREPRINT
	{"rprnt",    CRPRNT,  VREPRINT},
#endif
#ifdef VWERASE
	{"werase",   CWERASE, VWERASE},
#endif
#ifdef VLNEXT
	{"lnext",    CLNEXT,  VLNEXT},
#endif
#ifdef VFLUSHO
	{"flush",    CFLUSHO, VFLUSHO},
#endif
#ifdef VSTATUS
	{"status",   CSTATUS, VSTATUS},
#endif
	/* These must be last because of the display routines. */
	{stty_min,   1,       VMIN},
	{stty_time,  0,       VTIME},
};

enum {
	NUM_control_info =
	(sizeof(control_info) / sizeof(struct control_info))
};

#define EMT(t) ((enum mode_type)(t))

static const char *  visible(unsigned int ch);
static int           recover_mode(const char *arg, struct termios *mode);
static int           screen_columns(void);
static void          set_mode(const struct mode_info *info,
					int reversed, struct termios *mode);
static speed_t       string_to_baud(const char *arg);
static tcflag_t*     mode_type_flag(enum mode_type type, struct termios *mode);
static void          display_all(struct termios *mode);
static void          display_changed(struct termios *mode);
static void          display_recoverable(struct termios *mode);
static void          display_speed(struct termios *mode, int fancy);
static void          display_window_size(int fancy);
static void          sane_mode(struct termios *mode);
static void          set_control_char(const struct control_info *info,
					const char *arg, struct termios *mode);
static void          set_speed(enum speed_setting type,
					const char *arg, struct termios *mode);
static void          set_window_size(int rows, int cols);

static const char *device_name = bb_msg_standard_input;

static ATTRIBUTE_NORETURN void perror_on_device(const char *fmt)
{
	bb_perror_msg_and_die(fmt, device_name);
}


/* The width of the screen, for output wrapping. */
static int max_col;

/* Current position, to know when to wrap. */
static int current_col;

/* Print format string MESSAGE and optional args.
   Wrap to next line first if it won't fit.
   Print a space first unless MESSAGE will start a new line. */

static void wrapf(const char *message, ...)
{
	va_list args;
	char buf[1024];                 /* Plenty long for our needs. */
	int buflen;

	va_start(args, message);
	vsprintf(buf, message, args);
	va_end(args);
	buflen = strlen(buf);
	if (current_col + (current_col > 0) + buflen >= max_col) {
		putchar('\n');
		current_col = 0;
	}
	if (current_col > 0) {
		putchar(' ');
		current_col++;
	}
	fputs(buf, stdout);
	current_col += buflen;
}

static const struct suffix_mult stty_suffixes[] = {
	{"b",  512 },
	{"k",  1024},
	{"B",  1024},
	{NULL, 0   }
};

static const struct mode_info *find_mode(const char *name)
{
	int i;
	for (i = 0; i < NUM_mode_info; ++i)
		if (STREQ(name, mode_info[i].name))
			return &mode_info[i];
	return 0;
}

static const struct control_info *find_control(const char *name)
{
	int i;
	for (i = 0; i < NUM_control_info; ++i)
		if (STREQ(name, control_info[i].name))
			return &control_info[i];
	return 0;
}

enum {
	param_need_arg = 0x80,
	param_line   = 1 | 0x80,
	param_rows   = 2 | 0x80,
	param_cols   = 3 | 0x80,
	param_size   = 4,
	param_ispeed = 5 | 0x80,
	param_ospeed = 6 | 0x80,
	param_speed  = 7,
};

static int find_param(const char *name)
{
#ifdef HAVE_C_LINE
	if (STREQ(name, "line")) return param_line;
#endif
#ifdef TIOCGWINSZ
	if (STREQ(name, "rows")) return param_rows;
	if (STREQ(name, "cols")) return param_cols;
	if (STREQ(name, "columns")) return param_cols;
	if (STREQ(name, "size")) return param_size;
#endif
	if (STREQ(name, "ispeed")) return param_ispeed;
	if (STREQ(name, "ospeed")) return param_ospeed;
	if (STREQ(name, "speed")) return param_speed;
	return 0;
}


int stty_main(int argc, char **argv)
{
	struct termios mode;
	void (*output_func)(struct termios *);
	const char *file_name = NULL;
	int require_set_attr;
	int speed_was_set;
	int verbose_output;
	int recoverable_output;
	int noargs;
	int k;

	output_func = display_changed;
	noargs = 1;
	speed_was_set = 0;
	require_set_attr = 0;
	verbose_output = 0;
	recoverable_output = 0;

	/* First pass: only parse/verify command line params */
	k = 0;
	while (argv[++k]) {
		const struct mode_info *mp;
		const struct control_info *cp;
		const char *arg = argv[k];
		const char *argnext = argv[k+1];
		int param;

		if (arg[0] == '-') {
			int i;
			mp = find_mode(arg+1);
			if (mp) {
				if (!(mp->flags & REV))
					bb_error_msg_and_die("invalid argument '%s'", arg);
				noargs = 0;
				continue;
			}
			/* It is an option - parse it */
			i = 0;
			while (arg[++i]) {
				switch (arg[i]) {
				case 'a':
					verbose_output = 1;
					output_func = display_all;
					break;
				case 'g':
					recoverable_output = 1;
					output_func = display_recoverable;
					break;
				case 'F':
					if (file_name)
						bb_error_msg_and_die("only one device may be specified");
					file_name = &arg[i+1]; /* "-Fdevice" ? */
					if (!file_name[0]) { /* nope, "-F device" */
						int p = k+1; /* argv[p] is argnext */
						file_name = argnext;
						if (!file_name)
							bb_error_msg_and_die(bb_msg_requires_arg, "-F");
						/* remove -F param from arg[vc] */
						--argc;
						while (argv[p+1]) { argv[p] = argv[p+1]; ++p; }
					}
					goto end_option;
				default:
					bb_error_msg_and_die("invalid argument '%s'", arg);
				}
			}
end_option:
			continue;
		}

		mp = find_mode(arg);
		if (mp) {
			noargs = 0;
			continue;
		}

		cp = find_control(arg);
		if (cp) {
			if (!argnext)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			noargs = 0;
			++k;
			continue;
		}

		param = find_param(arg);
		if (param & param_need_arg) {
			if (!argnext)
				bb_error_msg_and_die(bb_msg_requires_arg, arg);
			++k;
		}

		switch (param) {
#ifdef HAVE_C_LINE
		case param_line:
			bb_xparse_number(argnext, stty_suffixes);
			break;
#endif
#ifdef TIOCGWINSZ
		case param_rows:
			bb_xparse_number(argnext, stty_suffixes);
			break;
		case param_cols:
			bb_xparse_number(argnext, stty_suffixes);
			break;
		case param_size:
#endif
		case param_ispeed:
		case param_ospeed:
		case param_speed:
			break;
		default:
			if (recover_mode(arg, &mode) == 1) break;
			if (string_to_baud(arg) != (speed_t) -1) break;
			bb_error_msg_and_die("invalid argument '%s'", arg);
		}
		noargs = 0;
	}

	/* Specifying both -a and -g is an error */
	if (verbose_output && recoverable_output)
		bb_error_msg_and_die("verbose and stty-readable output styles are mutually exclusive");
	/* Specifying -a or -g with non-options is an error */
	if (!noargs && (verbose_output || recoverable_output))
		bb_error_msg_and_die("modes may not be set when specifying an output style");

	/* Now it is safe to start doing things */
	if (file_name) {
		int fd, fdflags;
		device_name = file_name;
		fd = xopen(device_name, O_RDONLY | O_NONBLOCK);
		if (fd != 0) {
			dup2(fd, 0);
			close(fd);
		}
		fdflags = fcntl(STDIN_FILENO, F_GETFL);
		if (fdflags == -1 || fcntl(STDIN_FILENO, F_SETFL, fdflags & ~O_NONBLOCK) < 0)
			perror_on_device("%s: couldn't reset non-blocking mode");
	}

	/* Initialize to all zeroes so there is no risk memcmp will report a
	   spurious difference in an uninitialized portion of the structure.  */
	memset(&mode, 0, sizeof(mode));
	if (tcgetattr(STDIN_FILENO, &mode))
		perror_on_device("%s");

	if (verbose_output || recoverable_output || noargs) {
		max_col = screen_columns();
		current_col = 0;
		output_func(&mode);
		return EXIT_SUCCESS;
	}

	/* Second pass: perform actions */
	k = 0;
	while (argv[++k]) {
		const struct mode_info *mp;
		const struct control_info *cp;
		const char *arg = argv[k];
		const char *argnext = argv[k+1];
		int param;

		if (arg[0] == '-') {
			mp = find_mode(arg+1);
			if (mp) {
				set_mode(mp, 1 /* reversed */, &mode);
			}
			/* It is an option - already parsed. Skip it */
			continue;
		}

		mp = find_mode(arg);
		if (mp) {
			set_mode(mp, 0 /* non-reversed */, &mode);
			continue;
		}

		cp = find_control(arg);
		if (cp) {
			++k;
			set_control_char(cp, argnext, &mode);
			continue;
		}

		param = find_param(arg);
		if (param & param_need_arg) {
			++k;
		}

		switch (param) {
#ifdef HAVE_C_LINE
		case param_line:
			mode.c_line = bb_xparse_number(argnext, stty_suffixes);
			require_set_attr = 1;
			break;
#endif
#ifdef TIOCGWINSZ
		case param_cols:
			set_window_size(-1, (int) bb_xparse_number(argnext, stty_suffixes));
			break;
		case param_size:
			max_col = screen_columns();
			current_col = 0;
			display_window_size(0);
			break;
		case param_rows:
			set_window_size((int) bb_xparse_number(argnext, stty_suffixes), -1);
			break;
#endif
		case param_ispeed:
			set_speed(input_speed, argnext, &mode);
			speed_was_set = 1;
			require_set_attr = 1;
			break;
		case param_ospeed:
			set_speed(output_speed, argnext, &mode);
			speed_was_set = 1;
			require_set_attr = 1;
			break;
		case param_speed:
			max_col = screen_columns();
			display_speed(&mode, 0);
			break;
		default:
			if (recover_mode(arg, &mode) == 1)
				require_set_attr = 1;
			else /* true: if (string_to_baud(arg) != (speed_t) -1) */ {
				set_speed(both_speeds, arg, &mode);
				speed_was_set = 1;
				require_set_attr = 1;
			} /* else - impossible (caught in the first pass):
				bb_error_msg_and_die("invalid argument '%s'", arg); */
		}
	}

	if (require_set_attr) {
		struct termios new_mode;

		if (tcsetattr(STDIN_FILENO, TCSADRAIN, &mode))
			perror_on_device("%s");

		/* POSIX (according to Zlotnick's book) tcsetattr returns zero if
		   it performs *any* of the requested operations.  This means it
		   can report `success' when it has actually failed to perform
		   some proper subset of the requested operations.  To detect
		   this partial failure, get the current terminal attributes and
		   compare them to the requested ones.  */

		/* Initialize to all zeroes so there is no risk memcmp will report a
		   spurious difference in an uninitialized portion of the structure.  */
		memset(&new_mode, 0, sizeof(new_mode));
		if (tcgetattr(STDIN_FILENO, &new_mode))
			perror_on_device("%s");

		if (memcmp(&mode, &new_mode, sizeof(mode)) != 0) {
#ifdef CIBAUD
			/* SunOS 4.1.3 (at least) has the problem that after this sequence,
			   tcgetattr (&m1); tcsetattr (&m1); tcgetattr (&m2);
			   sometimes (m1 != m2).  The only difference is in the four bits
			   of the c_cflag field corresponding to the baud rate.  To save
			   Sun users a little confusion, don't report an error if this
			   happens.  But suppress the error only if we haven't tried to
			   set the baud rate explicitly -- otherwise we'd never give an
			   error for a true failure to set the baud rate.  */

			new_mode.c_cflag &= (~CIBAUD);
			if (speed_was_set || memcmp(&mode, &new_mode, sizeof(mode)) != 0)
#endif
				perror_on_device ("%s: unable to perform all requested operations");
		}
	}

	return EXIT_SUCCESS;
}

/* Return 0 if not applied because not reversible; otherwise return 1.  */

static void
set_mode(const struct mode_info *info, int reversed, struct termios *mode)
{
	tcflag_t *bitsp;

	bitsp = mode_type_flag(EMT(info->type), mode);

	if (bitsp == NULL) {
		/* Combination mode. */
		if (info->name == evenp || info->name == parity) {
			if (reversed)
				mode->c_cflag = (mode->c_cflag & ~PARENB & ~CSIZE) | CS8;
			else
				mode->c_cflag =
					(mode->c_cflag & ~PARODD & ~CSIZE) | PARENB | CS7;
		} else if (info->name == stty_oddp) {
			if (reversed)
				mode->c_cflag = (mode->c_cflag & ~PARENB & ~CSIZE) | CS8;
			else
				mode->c_cflag =
					(mode->c_cflag & ~CSIZE) | CS7 | PARODD | PARENB;
		} else if (info->name == stty_nl) {
			if (reversed) {
				mode->c_iflag = (mode->c_iflag | ICRNL) & ~INLCR & ~IGNCR;
				mode->c_oflag = (mode->c_oflag
#ifdef ONLCR
								 | ONLCR
#endif
					)
#ifdef OCRNL
					& ~OCRNL
#endif
#ifdef ONLRET
					& ~ONLRET
#endif
					;
			} else {
				mode->c_iflag = mode->c_iflag & ~ICRNL;
#ifdef ONLCR
				mode->c_oflag = mode->c_oflag & ~ONLCR;
#endif
			}
		} else if (info->name == stty_ek) {
			mode->c_cc[VERASE] = CERASE;
			mode->c_cc[VKILL] = CKILL;
		} else if (info->name == stty_sane)
			sane_mode(mode);
		else if (info->name == cbreak) {
			if (reversed)
				mode->c_lflag |= ICANON;
			else
				mode->c_lflag &= ~ICANON;
		} else if (info->name == stty_pass8) {
			if (reversed) {
				mode->c_cflag = (mode->c_cflag & ~CSIZE) | CS7 | PARENB;
				mode->c_iflag |= ISTRIP;
			} else {
				mode->c_cflag = (mode->c_cflag & ~PARENB & ~CSIZE) | CS8;
				mode->c_iflag &= ~ISTRIP;
			}
		} else if (info->name == litout) {
			if (reversed) {
				mode->c_cflag = (mode->c_cflag & ~CSIZE) | CS7 | PARENB;
				mode->c_iflag |= ISTRIP;
				mode->c_oflag |= OPOST;
			} else {
				mode->c_cflag = (mode->c_cflag & ~PARENB & ~CSIZE) | CS8;
				mode->c_iflag &= ~ISTRIP;
				mode->c_oflag &= ~OPOST;
			}
		} else if (info->name == raw || info->name == cooked) {
			if ((info->name[0] == 'r' && reversed)
				|| (info->name[0] == 'c' && !reversed)) {
				/* Cooked mode. */
				mode->c_iflag |= BRKINT | IGNPAR | ISTRIP | ICRNL | IXON;
				mode->c_oflag |= OPOST;
				mode->c_lflag |= ISIG | ICANON;
#if VMIN == VEOF
				mode->c_cc[VEOF] = CEOF;
#endif
#if VTIME == VEOL
				mode->c_cc[VEOL] = CEOL;
#endif
			} else {
				/* Raw mode. */
				mode->c_iflag = 0;
				mode->c_oflag &= ~OPOST;
				mode->c_lflag &= ~(ISIG | ICANON
#ifdef XCASE
								   | XCASE
#endif
					);
				mode->c_cc[VMIN] = 1;
				mode->c_cc[VTIME] = 0;
			}
		}
#ifdef IXANY
		else if (info->name == decctlq) {
			if (reversed)
				mode->c_iflag |= IXANY;
			else
				mode->c_iflag &= ~IXANY;
		}
#endif
#ifdef TABDLY
		else if (info->name == stty_tabs) {
			if (reversed)
				mode->c_oflag = (mode->c_oflag & ~TABDLY) | TAB3;
			else
				mode->c_oflag = (mode->c_oflag & ~TABDLY) | TAB0;
		}
#else
# ifdef OXTABS
		else if (info->name == stty_tabs) {
			if (reversed)
				mode->c_oflag = mode->c_oflag | OXTABS;
			else
				mode->c_oflag = mode->c_oflag & ~OXTABS;
		}
# endif
#endif
#if defined(XCASE) && defined(IUCLC) && defined(OLCUC)
		else if (info->name == stty_lcase || info->name == stty_LCASE) {
			if (reversed) {
				mode->c_lflag &= ~XCASE;
				mode->c_iflag &= ~IUCLC;
				mode->c_oflag &= ~OLCUC;
			} else {
				mode->c_lflag |= XCASE;
				mode->c_iflag |= IUCLC;
				mode->c_oflag |= OLCUC;
			}
		}
#endif
		else if (info->name == stty_crt)
			mode->c_lflag |= ECHOE
#ifdef ECHOCTL
				| ECHOCTL
#endif
#ifdef ECHOKE
				| ECHOKE
#endif
				;
		else if (info->name == stty_dec) {
			mode->c_cc[VINTR] = 3;  /* ^C */
			mode->c_cc[VERASE] = 127;       /* DEL */
			mode->c_cc[VKILL] = 21; /* ^U */
			mode->c_lflag |= ECHOE
#ifdef ECHOCTL
				| ECHOCTL
#endif
#ifdef ECHOKE
				| ECHOKE
#endif
				;
#ifdef IXANY
			mode->c_iflag &= ~IXANY;
#endif
		}
	} else if (reversed)
		*bitsp = *bitsp & ~((unsigned long)info->mask) & ~info->bits;
	else
		*bitsp = (*bitsp & ~((unsigned long)info->mask)) | info->bits;
}

static void
set_control_char(const struct control_info *info, const char *arg,
				 struct termios *mode)
{
	unsigned char value;

	if (info->name == stty_min || info->name == stty_time)
		value = bb_xparse_number(arg, stty_suffixes);
	else if (arg[0] == '\0' || arg[1] == '\0')
		value = arg[0];
	else if (STREQ(arg, "^-") || STREQ(arg, "undef"))
		value = _POSIX_VDISABLE;
	else if (arg[0] == '^' && arg[1] != '\0') {     /* Ignore any trailing junk. */
		if (arg[1] == '?')
			value = 127;
		else
			value = arg[1] & ~0140; /* Non-letters get weird results. */
	} else
		value = bb_xparse_number(arg, stty_suffixes);
	mode->c_cc[info->offset] = value;
}

static void
set_speed(enum speed_setting type, const char *arg, struct termios *mode)
{
	speed_t baud;

	baud = string_to_baud(arg);

	if (type != output_speed) {     /* either input or both */
		cfsetispeed(mode, baud);
	}
	if (type != input_speed) {      /* either output or both */
		cfsetospeed(mode, baud);
	}
}

#ifdef TIOCGWINSZ

static int get_win_size(int fd, struct winsize *win)
{
	int err = ioctl(fd, TIOCGWINSZ, (char *) win);

	return err;
}

static void
set_window_size(int rows, int cols)
{
	struct winsize win;

	if (get_win_size(STDIN_FILENO, &win)) {
		if (errno != EINVAL)
			perror_on_device("%s");
		memset(&win, 0, sizeof(win));
	}

	if (rows >= 0)
		win.ws_row = rows;
	if (cols >= 0)
		win.ws_col = cols;

# ifdef TIOCSSIZE
	/* Alexander Dupuy <dupuy@cs.columbia.edu> wrote:
	   The following code deals with a bug in the SunOS 4.x (and 3.x?) kernel.
	   This comment from sys/ttold.h describes Sun's twisted logic - a better
	   test would have been (ts_lines > 64k || ts_cols > 64k || ts_cols == 0).
	   At any rate, the problem is gone in Solaris 2.x. */

	if (win.ws_row == 0 || win.ws_col == 0) {
		struct ttysize ttysz;

		ttysz.ts_lines = win.ws_row;
		ttysz.ts_cols = win.ws_col;

		win.ws_row = win.ws_col = 1;

		if ((ioctl(STDIN_FILENO, TIOCSWINSZ, (char *) &win) != 0)
			|| (ioctl(STDIN_FILENO, TIOCSSIZE, (char *) &ttysz) != 0)) {
			perror_on_device("%s");
		}
		return;
	}
# endif

	if (ioctl(STDIN_FILENO, TIOCSWINSZ, (char *) &win))
		perror_on_device("%s");
}

static void display_window_size(int fancy)
{
	const char *fmt_str = "%s" "\0" "%s: no size information for this device";
	struct winsize win;

	if (get_win_size(STDIN_FILENO, &win)) {
		if ((errno != EINVAL) || ((fmt_str += 2), !fancy)) {
			perror_on_device(fmt_str);
		}
	} else {
		wrapf(fancy ? "rows %d; columns %d;" : "%d %d\n",
			  win.ws_row, win.ws_col);
		if (!fancy)
			current_col = 0;
	}
}
#endif

static int screen_columns(void)
{
	int columns;
	const char *s;

#ifdef TIOCGWINSZ
	struct winsize win;

	/* With Solaris 2.[123], this ioctl fails and errno is set to
	   EINVAL for telnet (but not rlogin) sessions.
	   On ISC 3.0, it fails for the console and the serial port
	   (but it works for ptys).
	   It can also fail on any system when stdout isn't a tty.
	   In case of any failure, just use the default.  */
	if (get_win_size(STDOUT_FILENO, &win) == 0 && win.ws_col > 0)
		return win.ws_col;
#endif

	columns = 80;
	if ((s = getenv("COLUMNS"))) {
		columns = atoi(s);
	}
	return columns;
}

static tcflag_t *mode_type_flag(enum mode_type type, struct termios *mode)
{
	static const unsigned char tcflag_offsets[] = {
		offsetof(struct termios, c_cflag), /* control */
		offsetof(struct termios, c_iflag), /* input */
		offsetof(struct termios, c_oflag), /* output */
		offsetof(struct termios, c_lflag) /* local */
	};

	if (((unsigned int) type) <= local) {
		return (tcflag_t *)(((char *) mode) + tcflag_offsets[(int)type]);
	}
	return NULL;
}

static void display_changed(struct termios *mode)
{
	int i;
	int empty_line;
	tcflag_t *bitsp;
	unsigned long mask;
	enum mode_type prev_type = control;

	display_speed(mode, 1);
#ifdef HAVE_C_LINE
	wrapf("line = %d;", mode->c_line);
#endif
	putchar('\n');
	current_col = 0;

	empty_line = 1;
	for (i = 0; control_info[i].name != stty_min; ++i) {
		if (mode->c_cc[control_info[i].offset] == control_info[i].saneval)
			continue;
		/* If swtch is the same as susp, don't print both.  */
#if VSWTCH == VSUSP
		if (control_info[i].name == stty_swtch)
			continue;
#endif
		/* If eof uses the same slot as min, only print whichever applies.  */
#if VEOF == VMIN
		if ((mode->c_lflag & ICANON) == 0
			&& (control_info[i].name == stty_eof
				|| control_info[i].name == stty_eol)) continue;
#endif

		empty_line = 0;
		wrapf("%s = %s;", control_info[i].name,
			  visible(mode->c_cc[control_info[i].offset]));
	}
	if ((mode->c_lflag & ICANON) == 0) {
		wrapf("min = %d; time = %d;\n", (int) mode->c_cc[VMIN],
			  (int) mode->c_cc[VTIME]);
	} else if (empty_line == 0)
		putchar('\n');
	current_col = 0;

	empty_line = 1;
	for (i = 0; i < NUM_mode_info; ++i) {
		if (mode_info[i].flags & OMIT)
			continue;
		if (EMT(mode_info[i].type) != prev_type) {
			if (empty_line == 0) {
				putchar('\n');
				current_col = 0;
				empty_line = 1;
			}
			prev_type = EMT(mode_info[i].type);
		}

		bitsp = mode_type_flag(EMT(mode_info[i].type), mode);
		mask = mode_info[i].mask ? mode_info[i].mask : mode_info[i].bits;
		if ((*bitsp & mask) == mode_info[i].bits) {
			if (mode_info[i].flags & SANE_UNSET) {
				wrapf("%s", mode_info[i].name);
				empty_line = 0;
			}
		}
			else if ((mode_info[i].flags & (SANE_SET | REV)) ==
					 (SANE_SET | REV)) {
			wrapf("-%s", mode_info[i].name);
			empty_line = 0;
		}
	}
	if (empty_line == 0)
		putchar('\n');
	current_col = 0;
}

static void
display_all(struct termios *mode)
{
	int i;
	tcflag_t *bitsp;
	unsigned long mask;
	enum mode_type prev_type = control;

	display_speed(mode, 1);
#ifdef TIOCGWINSZ
	display_window_size(1);
#endif
#ifdef HAVE_C_LINE
	wrapf("line = %d;", mode->c_line);
#endif
	putchar('\n');
	current_col = 0;

	for (i = 0; control_info[i].name != stty_min; ++i) {
		/* If swtch is the same as susp, don't print both.  */
#if VSWTCH == VSUSP
		if (control_info[i].name == stty_swtch)
			continue;
#endif
		/* If eof uses the same slot as min, only print whichever applies.  */
#if VEOF == VMIN
		if ((mode->c_lflag & ICANON) == 0
			&& (control_info[i].name == stty_eof
				|| control_info[i].name == stty_eol)) continue;
#endif
		wrapf("%s = %s;", control_info[i].name,
			  visible(mode->c_cc[control_info[i].offset]));
	}
#if VEOF == VMIN
	if ((mode->c_lflag & ICANON) == 0)
#endif
		wrapf("min = %d; time = %d;", mode->c_cc[VMIN], mode->c_cc[VTIME]);
	if (current_col != 0)
		putchar('\n');
	current_col = 0;

	for (i = 0; i < NUM_mode_info; ++i) {
		if (mode_info[i].flags & OMIT)
			continue;
		if (EMT(mode_info[i].type) != prev_type) {
			putchar('\n');
			current_col = 0;
			prev_type = EMT(mode_info[i].type);
		}

		bitsp = mode_type_flag(EMT(mode_info[i].type), mode);
		mask = mode_info[i].mask ? mode_info[i].mask : mode_info[i].bits;
		if ((*bitsp & mask) == mode_info[i].bits)
			wrapf("%s", mode_info[i].name);
		else if (mode_info[i].flags & REV)
			wrapf("-%s", mode_info[i].name);
	}
	putchar('\n');
	current_col = 0;
}

static void display_speed(struct termios *mode, int fancy)
{
	unsigned long ispeed, ospeed;
	const char *fmt_str =
		"%lu %lu\n\0"        "ispeed %lu baud; ospeed %lu baud;\0"
		"%lu\n\0" "\0\0\0\0" "speed %lu baud;";

	ospeed = ispeed = cfgetispeed(mode);
	if (ispeed == 0 || ispeed == (ospeed = cfgetospeed(mode))) {
		ispeed = ospeed;                /* in case ispeed was 0 */
		fmt_str += 43;
	}
	if (fancy) {
		fmt_str += 9;
	}
	wrapf(fmt_str, tty_baud_to_value(ispeed), tty_baud_to_value(ospeed));
	if (!fancy)
		current_col = 0;
}

static void display_recoverable(struct termios *mode)
{
	int i;

	printf("%lx:%lx:%lx:%lx",
		   (unsigned long) mode->c_iflag, (unsigned long) mode->c_oflag,
		   (unsigned long) mode->c_cflag, (unsigned long) mode->c_lflag);
	for (i = 0; i < NCCS; ++i)
		printf(":%x", (unsigned int) mode->c_cc[i]);
	putchar('\n');
}

static int recover_mode(const char *arg, struct termios *mode)
{
	int i, n;
	unsigned int chr;
	unsigned long iflag, oflag, cflag, lflag;

	/* Scan into temporaries since it is too much trouble to figure out
	   the right format for `tcflag_t'.  */
	if (sscanf(arg, "%lx:%lx:%lx:%lx%n",
			   &iflag, &oflag, &cflag, &lflag, &n) != 4)
		return 0;
	mode->c_iflag = iflag;
	mode->c_oflag = oflag;
	mode->c_cflag = cflag;
	mode->c_lflag = lflag;
	arg += n;
	for (i = 0; i < NCCS; ++i) {
		if (sscanf(arg, ":%x%n", &chr, &n) != 1)
			return 0;
		mode->c_cc[i] = chr;
		arg += n;
	}

	/* Fail if there are too many fields.  */
	if (*arg != '\0')
		return 0;

	return 1;
}

static speed_t string_to_baud(const char *arg)
{
	return tty_value_to_baud(bb_xparse_number(arg, 0));
}

static void sane_mode(struct termios *mode)
{
	int i;
	tcflag_t *bitsp;

	for (i = 0; i < NUM_control_info; ++i) {
#if VMIN == VEOF
		if (control_info[i].name == stty_min)
			break;
#endif
		mode->c_cc[control_info[i].offset] = control_info[i].saneval;
	}

	for (i = 0; i < NUM_mode_info; ++i) {
		if (mode_info[i].flags & SANE_SET) {
			bitsp = mode_type_flag(EMT(mode_info[i].type), mode);
			*bitsp = (*bitsp & ~((unsigned long)mode_info[i].mask))
				| mode_info[i].bits;
		} else if (mode_info[i].flags & SANE_UNSET) {
			bitsp = mode_type_flag(EMT(mode_info[i].type), mode);
			*bitsp = *bitsp & ~((unsigned long)mode_info[i].mask)
				& ~mode_info[i].bits;
		}
	}
}

/* Return a string that is the printable representation of character CH.  */
/* Adapted from `cat' by Torbjorn Granlund.  */

static const char *visible(unsigned int ch)
{
	static char buf[10];
	char *bpout = buf;

	if (ch == _POSIX_VDISABLE) {
		return "<undef>";
	}

	if (ch >= 128) {
		ch -= 128;
		*bpout++ = 'M';
		*bpout++ = '-';
	}

	if (ch < 32) {
		*bpout++ = '^';
		*bpout++ = ch + 64;
	} else if (ch < 127) {
		*bpout++ = ch;
	} else {
		*bpout++ = '^';
		*bpout++ = '?';
	}

	*bpout = '\0';
	return buf;
}
