/* vi: set sw=4 ts=4: */
/* stty -- change and print terminal line settings
   Copyright (C) 1990-1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

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

//#define TEST

#include <termios.h>
#include <sys/ioctl.h>
#include <getopt.h>

#include <sys/param.h>
#include <unistd.h>

#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <memory.h>
#include <fcntl.h>
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

/* What to output and how.  */
enum output_type {
	changed, all, recoverable       /* Default, -a, -g.  */
};

/* Which member(s) of `struct termios' a mode uses.  */
enum mode_type {
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
	enum mode_type type;    /* Which structure element to change.    */
	char flags;             /* Setting and display options.          */
	unsigned long bits;     /* Bits to set for this mode.            */
	unsigned long mask;     /* Other bits to turn off for this mode. */
};

static const struct  mode_info mode_info[] = {
	{"parenb",   control,     REV,               PARENB,     0 },
	{"parodd",   control,     REV,               PARODD,     0 },
	{"cs5",      control,     0,                 CS5,     CSIZE},
	{"cs6",      control,     0,                 CS6,     CSIZE},
	{"cs7",      control,     0,                 CS7,     CSIZE},
	{"cs8",      control,     0,                 CS8,     CSIZE},
	{"hupcl",    control,     REV,               HUPCL,      0 },
	{"hup",      control,     REV        | OMIT, HUPCL,      0 },
	{"cstopb",   control,     REV,               CSTOPB,     0 },
	{"cread",    control,     SANE_SET   | REV,  CREAD,      0 },
	{"clocal",   control,     REV,               CLOCAL,     0 },
#ifdef CRTSCTS
	{"crtscts",  control,     REV,               CRTSCTS,    0 },
#endif
	{"ignbrk",   input,       SANE_UNSET | REV,  IGNBRK,     0 },
	{"brkint",   input,       SANE_SET   | REV,  BRKINT,     0 },
	{"ignpar",   input,       REV,               IGNPAR,     0 },
	{"parmrk",   input,       REV,               PARMRK,     0 },
	{"inpck",    input,       REV,               INPCK,      0 },
	{"istrip",   input,       REV,               ISTRIP,     0 },
	{"inlcr",    input,       SANE_UNSET | REV,  INLCR,      0 },
	{"igncr",    input,       SANE_UNSET | REV,  IGNCR,      0 },
	{"icrnl",    input,       SANE_SET   | REV,  ICRNL,      0 },
	{"ixon",     input,       REV,               IXON,       0 },
	{"ixoff",    input,       SANE_UNSET | REV,  IXOFF,      0 },
	{"tandem",   input,       REV        | OMIT, IXOFF,      0 },
#ifdef IUCLC
	{"iuclc",    input,       SANE_UNSET | REV,  IUCLC,      0 },
#endif
#ifdef IXANY
	{"ixany",    input,       SANE_UNSET | REV,  IXANY,      0 },
#endif
#ifdef IMAXBEL
	{"imaxbel",  input,       SANE_SET   | REV,  IMAXBEL,    0 },
#endif
	{"opost",    output,      SANE_SET   | REV,  OPOST,      0 },
#ifdef OLCUC
	{"olcuc",    output,      SANE_UNSET | REV,  OLCUC,      0 },
#endif
#ifdef OCRNL
	{"ocrnl",    output,      SANE_UNSET | REV,  OCRNL,      0 },
#endif
#ifdef ONLCR
	{"onlcr",    output,      SANE_SET   | REV,  ONLCR,      0 },
#endif
#ifdef ONOCR
	{"onocr",    output,      SANE_UNSET | REV,  ONOCR,      0 },
#endif
#ifdef ONLRET
	{"onlret",   output,      SANE_UNSET | REV,  ONLRET,     0 },
#endif
#ifdef OFILL
	{"ofill",    output,      SANE_UNSET | REV,  OFILL,      0 },
#endif
#ifdef OFDEL
	{"ofdel",    output,      SANE_UNSET | REV,  OFDEL,      0 },
#endif
#ifdef NLDLY
	{"nl1",      output,      SANE_UNSET,        NL1,     NLDLY},
	{"nl0",      output,      SANE_SET,          NL0,     NLDLY},
#endif
#ifdef CRDLY
	{"cr3",      output,      SANE_UNSET,        CR3,     CRDLY},
	{"cr2",      output,      SANE_UNSET,        CR2,     CRDLY},
	{"cr1",      output,      SANE_UNSET,        CR1,     CRDLY},
	{"cr0",      output,      SANE_SET,          CR0,     CRDLY},
#endif

#ifdef TABDLY
	{"tab3",     output,      SANE_UNSET,        TAB3,   TABDLY},
	{"tab2",     output,      SANE_UNSET,        TAB2,   TABDLY},
	{"tab1",     output,      SANE_UNSET,        TAB1,   TABDLY},
	{"tab0",     output,      SANE_SET,          TAB0,   TABDLY},
#else
# ifdef OXTABS
	{"tab3",     output,      SANE_UNSET,        OXTABS,     0 },
# endif
#endif

#ifdef BSDLY
	{"bs1",      output,      SANE_UNSET,        BS1,     BSDLY},
	{"bs0",      output,      SANE_SET,          BS0,     BSDLY},
#endif
#ifdef VTDLY
	{"vt1",      output,      SANE_UNSET,        VT1,     VTDLY},
	{"vt0",      output,      SANE_SET,          VT0,     VTDLY},
#endif
#ifdef FFDLY
	{"ff1",      output,      SANE_UNSET,        FF1,     FFDLY},
	{"ff0",      output,      SANE_SET,          FF0,     FFDLY},
#endif
	{"isig",     local,       SANE_SET   | REV,  ISIG,       0 },
	{"icanon",   local,       SANE_SET   | REV,  ICANON,     0 },
#ifdef IEXTEN
	{"iexten",   local,       SANE_SET   | REV,  IEXTEN,     0 },
#endif
	{"echo",     local,       SANE_SET   | REV,  ECHO,       0 },
	{"echoe",    local,       SANE_SET   | REV,  ECHOE,      0 },
	{"crterase", local,       REV        | OMIT, ECHOE,      0 },
	{"echok",    local,       SANE_SET   | REV,  ECHOK,      0 },
	{"echonl",   local,       SANE_UNSET | REV,  ECHONL,     0 },
	{"noflsh",   local,       SANE_UNSET | REV,  NOFLSH,     0 },
#ifdef XCASE
	{"xcase",    local,       SANE_UNSET | REV,  XCASE,      0 },
#endif
#ifdef TOSTOP
	{"tostop",   local,       SANE_UNSET | REV,  TOSTOP,     0 },
#endif
#ifdef ECHOPRT
	{"echoprt",  local,       SANE_UNSET | REV,  ECHOPRT,    0 },
	{"prterase", local,       REV | OMIT,        ECHOPRT,    0 },
#endif
#ifdef ECHOCTL
	{"echoctl",  local,       SANE_SET   | REV,  ECHOCTL,    0 },
	{"ctlecho",  local,       REV        | OMIT, ECHOCTL,    0 },
#endif
#ifdef ECHOKE
	{"echoke",   local,       SANE_SET   | REV,  ECHOKE,     0 },
	{"crtkill",  local,       REV        | OMIT, ECHOKE,     0 },
#endif
	{evenp,      combination, REV        | OMIT, 0,          0 },
	{parity,     combination, REV        | OMIT, 0,          0 },
	{stty_oddp,  combination, REV        | OMIT, 0,          0 },
	{stty_nl,    combination, REV        | OMIT, 0,          0 },
	{stty_ek,    combination, OMIT,              0,          0 },
	{stty_sane,  combination, OMIT,              0,          0 },
	{cooked,     combination, REV        | OMIT, 0,          0 },
	{raw,        combination, REV        | OMIT, 0,          0 },
	{stty_pass8, combination, REV        | OMIT, 0,          0 },
	{litout,     combination, REV        | OMIT, 0,          0 },
	{cbreak,     combination, REV        | OMIT, 0,          0 },
#ifdef IXANY
	{decctlq,    combination, REV        | OMIT, 0,          0 },
#endif
#if defined (TABDLY) || defined (OXTABS)
	{stty_tabs,  combination, REV        | OMIT, 0,          0 },
#endif
#if defined(XCASE) && defined(IUCLC) && defined(OLCUC)
	{stty_lcase, combination, REV        | OMIT, 0,          0 },
	{stty_LCASE, combination, REV        | OMIT, 0,          0 },
#endif
	{stty_crt,   combination, OMIT,              0,          0 },
	{stty_dec,   combination, OMIT,              0,          0 },
};

static const int NUM_mode_info =

	(sizeof(mode_info) / sizeof(struct mode_info));

/* Control character settings.  */
struct control_info {
	const char *name;                       /* Name given on command line.  */
	unsigned char saneval;          /* Value to set for `stty sane'.  */
	int offset;                                     /* Offset in c_cc.  */
};

/* Control characters. */

static const struct  control_info control_info[] = {
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

static const int NUM_control_info =
	(sizeof(control_info) / sizeof(struct control_info));


static const char *  visible(unsigned int ch);
static unsigned long baud_to_value(speed_t speed);
static int           recover_mode(char *arg, struct termios *mode);
static int           screen_columns(void);
static int           set_mode(const struct mode_info *info,
					int reversed, struct termios *mode);
static speed_t       string_to_baud(const char *arg);
static tcflag_t*     mode_type_flag(enum mode_type type, struct termios *mode);
static void          display_all(struct termios *mode, int fd,
					const char *device_name);
static void          display_changed(struct termios *mode);
static void          display_recoverable(struct termios *mode);
static void          display_settings(enum output_type output_type,
					struct termios *mode, int fd,
					const char *device_name);
static void          display_speed(struct termios *mode, int fancy);
static void          display_window_size(int fancy, int fd,
					const char *device_name);
static void          sane_mode(struct termios *mode);
static void          set_control_char(const struct control_info *info,
					const char *arg, struct termios *mode);
static void          set_speed(enum speed_setting type,
					const char *arg, struct termios *mode);
static void          set_window_size(int rows, int cols, int fd,
					const char *device_name);

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

#ifndef TEST
extern int stty_main(int argc, char **argv)
#else
extern int main(int argc, char **argv)
#endif
{
	struct termios mode;
	enum   output_type output_type;
	int    optc;
	int    require_set_attr;
	int    speed_was_set;
	int    verbose_output;
	int    recoverable_output;
	int    k;
	int    noargs = 1;
	char * file_name = NULL;
	int    fd;
	const char *device_name;

	output_type = changed;
	verbose_output = 0;
	recoverable_output = 0;

	/* Don't print error messages for unrecognized options.  */
	opterr = 0;

	while ((optc = getopt(argc, argv, "agF:")) != -1) {
		switch (optc) {
		case 'a':
			verbose_output = 1;
			output_type = all;
			break;

		case 'g':
			recoverable_output = 1;
			output_type = recoverable;
			break;

		case 'F':
			if (file_name)
				error_msg_and_die("only one device may be specified");
			file_name = optarg;
			break;

		default:                /* unrecognized option */
			noargs = 0;
			break;
		}

		if (noargs == 0)
			break;
	}

	if (optind < argc)
		noargs = 0;

	/* Specifying both -a and -g gets an error.  */
	if (verbose_output && recoverable_output)
		error_msg_and_die ("verbose and stty-readable output styles are mutually exclusive");

	/* Specifying any other arguments with -a or -g gets an error.  */
	if (!noargs && (verbose_output || recoverable_output))
		error_msg_and_die ("modes may not be set when specifying an output style");

	/* FIXME: it'd be better not to open the file until we've verified
	   that all arguments are valid.  Otherwise, we could end up doing
	   only some of the requested operations and then failing, probably
	   leaving things in an undesirable state.  */

	if (file_name) {
		int fdflags;

		device_name = file_name;
		fd = open(device_name, O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			perror_msg_and_die("%s", device_name);
		if ((fdflags = fcntl(fd, F_GETFL)) == -1
			|| fcntl(fd, F_SETFL, fdflags & ~O_NONBLOCK) < 0)
			perror_msg_and_die("%s: couldn't reset non-blocking mode",
							   device_name);
	} else {
		fd = 0;
		device_name = "standard input";
	}

	/* Initialize to all zeroes so there is no risk memcmp will report a
	   spurious difference in an uninitialized portion of the structure.  */
	memset(&mode, 0, sizeof(mode));
	if (tcgetattr(fd, &mode))
		perror_msg_and_die("%s", device_name);

	if (verbose_output || recoverable_output || noargs) {
		max_col = screen_columns();
		current_col = 0;
		display_settings(output_type, &mode, fd, device_name);
		return EXIT_SUCCESS;
	}

	speed_was_set = 0;
	require_set_attr = 0;
	k = 0;
	while (++k < argc) {
		int match_found = 0;
		int reversed = 0;
		int i;

		if (argv[k][0] == '-') {
			char *find_dev_opt;

			++argv[k];

     /* Handle "-a", "-ag", "-aF/dev/foo", "-aF /dev/foo", etc.
	Find the options that have been parsed.  This is really
	gross, but it's needed because stty SETTINGS look like options to
	getopt(), so we need to work around things in a really horrible
	way.  If any new options are ever added to stty, the short option
	MUST NOT be a letter which is the first letter of one of the
	possible stty settings.
     */
			find_dev_opt = strchr(argv[k], 'F'); /* find -*F* */
			if(find_dev_opt) {
				if(find_dev_opt[1]==0)  /* -*F   /dev/foo */
					k++;            /* skip  /dev/foo */
				continue;   /* else -*F/dev/foo - no skip */
			}
			if(argv[k][0]=='a' || argv[k][0]=='g')
				continue;
			/* Is not options - is reverse params */
			reversed = 1;
		}
		for (i = 0; i < NUM_mode_info; ++i)
			if (STREQ(argv[k], mode_info[i].name)) {
				match_found = set_mode(&mode_info[i], reversed, &mode);
				require_set_attr = 1;
				break;
			}

		if (match_found == 0 && reversed)
			error_msg_and_die("invalid argument `%s'", --argv[k]);

		if (match_found == 0)
			for (i = 0; i < NUM_control_info; ++i)
				if (STREQ(argv[k], control_info[i].name)) {
					if (k == argc - 1)
					    error_msg_and_die("missing argument to `%s'", argv[k]);
					match_found = 1;
					++k;
					set_control_char(&control_info[i], argv[k], &mode);
					require_set_attr = 1;
					break;
				}

		if (match_found == 0) {
			if (STREQ(argv[k], "ispeed")) {
				if (k == argc - 1)
				    error_msg_and_die("missing argument to `%s'", argv[k]);
				++k;
				set_speed(input_speed, argv[k], &mode);
				speed_was_set = 1;
				require_set_attr = 1;
			} else if (STREQ(argv[k], "ospeed")) {
				if (k == argc - 1)
				    error_msg_and_die("missing argument to `%s'", argv[k]);
				++k;
				set_speed(output_speed, argv[k], &mode);
				speed_was_set = 1;
				require_set_attr = 1;
			}
#ifdef TIOCGWINSZ
			else if (STREQ(argv[k], "rows")) {
				if (k == argc - 1)
				    error_msg_and_die("missing argument to `%s'", argv[k]);
				++k;
				set_window_size((int) parse_number(argv[k], stty_suffixes),
								-1, fd, device_name);
			} else if (STREQ(argv[k], "cols") || STREQ(argv[k], "columns")) {
				if (k == argc - 1)
				    error_msg_and_die("missing argument to `%s'", argv[k]);
				++k;
				set_window_size(-1,
						(int) parse_number(argv[k], stty_suffixes),
						fd, device_name);
			} else if (STREQ(argv[k], "size")) {
				max_col = screen_columns();
				current_col = 0;
				display_window_size(0, fd, device_name);
			}
#endif
#ifdef HAVE_C_LINE
			else if (STREQ(argv[k], "line")) {
				if (k == argc - 1)
					error_msg_and_die("missing argument to `%s'", argv[k]);
				++k;
				mode.c_line = parse_number(argv[k], stty_suffixes);
				require_set_attr = 1;
			}
#endif
			else if (STREQ(argv[k], "speed")) {
				max_col = screen_columns();
				display_speed(&mode, 0);
			} else if (recover_mode(argv[k], &mode) == 1)
				require_set_attr = 1;
			else if (string_to_baud(argv[k]) != (speed_t) - 1) {
				set_speed(both_speeds, argv[k], &mode);
				speed_was_set = 1;
				require_set_attr = 1;
			} else
				error_msg_and_die("invalid argument `%s'", argv[k]);
		}
	}

	if (require_set_attr) {
		struct termios new_mode;

		if (tcsetattr(fd, TCSADRAIN, &mode))
			perror_msg_and_die("%s", device_name);

		/* POSIX (according to Zlotnick's book) tcsetattr returns zero if
		   it performs *any* of the requested operations.  This means it
		   can report `success' when it has actually failed to perform
		   some proper subset of the requested operations.  To detect
		   this partial failure, get the current terminal attributes and
		   compare them to the requested ones.  */

		/* Initialize to all zeroes so there is no risk memcmp will report a
		   spurious difference in an uninitialized portion of the structure.  */
		memset(&new_mode, 0, sizeof(new_mode));
		if (tcgetattr(fd, &new_mode))
			perror_msg_and_die("%s", device_name);

		/* Normally, one shouldn't use memcmp to compare structures that
		   may have `holes' containing uninitialized data, but we have been
		   careful to initialize the storage of these two variables to all
		   zeroes.  One might think it more efficient simply to compare the
		   modified fields, but that would require enumerating those fields --
		   and not all systems have the same fields in this structure.  */

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
				error_msg_and_die ("%s: unable to perform all requested operations",
					 device_name);
		}
	}

	return EXIT_SUCCESS;
}

/* Return 0 if not applied because not reversible; otherwise return 1.  */

static int
set_mode(const struct mode_info *info, int reversed, struct termios *mode)
{
	tcflag_t *bitsp;

	if (reversed && (info->flags & REV) == 0)
		return 0;

	bitsp = mode_type_flag(info->type, mode);

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
		*bitsp = *bitsp & ~info->mask & ~info->bits;
	else
		*bitsp = (*bitsp & ~info->mask) | info->bits;

	return 1;
}

static void
set_control_char(const struct control_info *info, const char *arg,
				 struct termios *mode)
{
	unsigned char value;

	if (info->name == stty_min || info->name == stty_time)
		value = parse_number(arg, stty_suffixes);
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
		value = parse_number(arg, stty_suffixes);
	mode->c_cc[info->offset] = value;
}

static void
set_speed(enum speed_setting type, const char *arg, struct termios *mode)
{
	speed_t baud;

	baud = string_to_baud(arg);
	if (type == input_speed || type == both_speeds)
		cfsetispeed(mode, baud);
	if (type == output_speed || type == both_speeds)
		cfsetospeed(mode, baud);
}

#ifdef TIOCGWINSZ

static int get_win_size(int fd, struct winsize *win)
{
	int err = ioctl(fd, TIOCGWINSZ, (char *) win);

	return err;
}

static void
set_window_size(int rows, int cols, int fd, const char *device_name)
{
	struct winsize win;

	if (get_win_size(fd, &win)) {
		if (errno != EINVAL)
			perror_msg_and_die("%s", device_name);
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

		win.ws_row = 1;
		win.ws_col = 1;

		if (ioctl(fd, TIOCSWINSZ, (char *) &win))
			perror_msg_and_die("%s", device_name);

		if (ioctl(fd, TIOCSSIZE, (char *) &ttysz))
			perror_msg_and_die("%s", device_name);
		return;
	}
# endif

	if (ioctl(fd, TIOCSWINSZ, (char *) &win))
		perror_msg_and_die("%s", device_name);
}

static void display_window_size(int fancy, int fd, const char *device_name)
{
	struct winsize win;

	if (get_win_size(fd, &win)) {
		if (errno != EINVAL)
			perror_msg_and_die("%s", device_name);
		if (!fancy)
			perror_msg_and_die("%s: no size information for this device",
							   device_name);
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

	if (getenv("COLUMNS"))
		return atoi(getenv("COLUMNS"));
	return 80;
}

static tcflag_t *mode_type_flag(enum mode_type type, struct termios *mode)
{
	switch (type) {
	case control:
		return &mode->c_cflag;

	case input:
		return &mode->c_iflag;

	case output:
		return &mode->c_oflag;

	case local:
		return &mode->c_lflag;

	default:                                        /* combination: */
		return NULL;
	}
}

static void
display_settings(enum output_type output_type, struct termios *mode,
				 int fd, const char *device_name)
{
	switch (output_type) {
	case changed:
		display_changed(mode);
		break;

	case all:
		display_all(mode, fd, device_name);
		break;

	case recoverable:
		display_recoverable(mode);
		break;
	}
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
		if (mode_info[i].type != prev_type) {
			if (empty_line == 0) {
				putchar('\n');
				current_col = 0;
				empty_line = 1;
			}
			prev_type = mode_info[i].type;
		}

		bitsp = mode_type_flag(mode_info[i].type, mode);
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
display_all(struct termios *mode, int fd, const char *device_name)
{
	int i;
	tcflag_t *bitsp;
	unsigned long mask;
	enum mode_type prev_type = control;

	display_speed(mode, 1);
#ifdef TIOCGWINSZ
	display_window_size(1, fd, device_name);
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
		if (mode_info[i].type != prev_type) {
			putchar('\n');
			current_col = 0;
			prev_type = mode_info[i].type;
		}

		bitsp = mode_type_flag(mode_info[i].type, mode);
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
	if (cfgetispeed(mode) == 0 || cfgetispeed(mode) == cfgetospeed(mode))
		wrapf(fancy ? "speed %lu baud;" : "%lu\n",
			  baud_to_value(cfgetospeed(mode)));
	else
		wrapf(fancy ? "ispeed %lu baud; ospeed %lu baud;" : "%lu %lu\n",
			  baud_to_value(cfgetispeed(mode)),
			  baud_to_value(cfgetospeed(mode)));
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

static int recover_mode(char *arg, struct termios *mode)
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

struct speed_map {
	speed_t speed;                          /* Internal form. */
	unsigned long value;            /* Numeric value. */
};

static const struct speed_map speeds[] = {
	{B0, 0},
	{B50, 50},
	{B75, 75},
	{B110, 110},
	{B134, 134},
	{B150, 150},
	{B200, 200},
	{B300, 300},
	{B600, 600},
	{B1200, 1200},
	{B1800, 1800},
	{B2400, 2400},
	{B4800, 4800},
	{B9600, 9600},
	{B19200, 19200},
	{B38400, 38400},
#ifdef B57600
	{B57600, 57600},
#endif
#ifdef B115200
	{B115200, 115200},
#endif
#ifdef B230400
	{B230400, 230400},
#endif
#ifdef B460800
	{B460800, 460800},
#endif
};

static const int NUM_SPEEDS = (sizeof(speeds) / sizeof(struct speed_map));

static speed_t string_to_baud(const char *arg)
{
	int i;

	for (i = 0; i < NUM_SPEEDS; ++i)
		if (parse_number(arg, 0) == speeds[i].value)
			return speeds[i].speed;
	return (speed_t) - 1;
}

static unsigned long baud_to_value(speed_t speed)
{
	int i;

	for (i = 0; i < NUM_SPEEDS; ++i)
		if (speed == speeds[i].speed)
			return speeds[i].value;
	return 0;
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
			bitsp = mode_type_flag(mode_info[i].type, mode);
			*bitsp = (*bitsp & ~mode_info[i].mask) | mode_info[i].bits;
		} else if (mode_info[i].flags & SANE_UNSET) {
			bitsp = mode_type_flag(mode_info[i].type, mode);
			*bitsp = *bitsp & ~mode_info[i].mask & ~mode_info[i].bits;
		}
	}
}

/* Return a string that is the printable representation of character CH.  */
/* Adapted from `cat' by Torbjorn Granlund.  */

static const char *visible(unsigned int ch)
{
	static char buf[10];
	char *bpout = buf;

	if (ch == _POSIX_VDISABLE)
		return "<undef>";

	if (ch >= 32) {
		if (ch < 127)
			*bpout++ = ch;
		else if (ch == 127) {
			*bpout++ = '^';
			*bpout++ = '?';
		} else {
			*bpout++ = 'M', *bpout++ = '-';
			if (ch >= 128 + 32) {
				if (ch < 128 + 127)
					*bpout++ = ch - 128;
				else {
					*bpout++ = '^';
					*bpout++ = '?';
				}
			} else {
				*bpout++ = '^';
				*bpout++ = ch - 128 + 64;
			}
		}
	} else {
		*bpout++ = '^';
		*bpout++ = ch + 64;
	}
	*bpout = '\0';
	return (const char *) buf;
}

#ifdef TEST

const char *applet_name = "stty";

#endif

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
