/* `time' utility to display resource usage of processes.
   Copyright (C) 1990, 91, 92, 93, 96 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* Originally written by David Keppel <pardo@cs.washington.edu>.
   Heavily modified by David MacKenzie <djm@gnu.ai.mit.edu>.  
   Heavily modified for busybox by Erik Andersen <andersen@codepoet.org>
   */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>		/* For pid_t. */
#include <sys/wait.h>
#include <sys/param.h>		/* For getpagesize, maybe.  */

#define TV_MSEC tv_usec / 1000
#include <sys/resource.h>
#include "busybox.h"

/* Information on the resources used by a child process.  */
typedef struct
{
  int waitstatus;
  struct rusage ru;
  struct timeval start, elapsed; /* Wallclock time of process.  */
} resource_t;

/* msec = milliseconds = 1/1,000 (1*10e-3) second.
   usec = microseconds = 1/1,000,000 (1*10e-6) second.  */

#ifndef TICKS_PER_SEC
#define TICKS_PER_SEC 100
#endif

/* The number of milliseconds in one `tick' used by the `rusage' structure.  */
#define MSEC_PER_TICK (1000 / TICKS_PER_SEC)

/* Return the number of clock ticks that occur in M milliseconds.  */
#define MSEC_TO_TICKS(m) ((m) / MSEC_PER_TICK)

#define UL unsigned long

static const char *const default_format = "real\t%E\nuser\t%u\nsys\t%T";

/* The output format for the -p option .*/
static const char *const posix_format = "real %e\nuser %U\nsys %S";


/* Format string for printing all statistics verbosely.
   Keep this output to 24 lines so users on terminals can see it all.*/
static const char *const long_format =
  "\tCommand being timed: \"%C\"\n"
  "\tUser time (seconds): %U\n"
  "\tSystem time (seconds): %S\n"
  "\tPercent of CPU this job got: %P\n"
  "\tElapsed (wall clock) time (h:mm:ss or m:ss): %E\n"
  "\tAverage shared text size (kbytes): %X\n"
  "\tAverage unshared data size (kbytes): %D\n"
  "\tAverage stack size (kbytes): %p\n"
  "\tAverage total size (kbytes): %K\n"
  "\tMaximum resident set size (kbytes): %M\n"
  "\tAverage resident set size (kbytes): %t\n"
  "\tMajor (requiring I/O) page faults: %F\n"
  "\tMinor (reclaiming a frame) page faults: %R\n"
  "\tVoluntary context switches: %w\n"
  "\tInvoluntary context switches: %c\n"
  "\tSwaps: %W\n"
  "\tFile system inputs: %I\n"
  "\tFile system outputs: %O\n"
  "\tSocket messages sent: %s\n"
  "\tSocket messages received: %r\n"
  "\tSignals delivered: %k\n"
  "\tPage size (bytes): %Z\n"
  "\tExit status: %x";


  /* Wait for and fill in data on child process PID.
   Return 0 on error, 1 if ok.  */

/* pid_t is short on BSDI, so don't try to promote it.  */
static int resuse_end (pid_t pid, resource_t *resp)
{
    int status;

    pid_t caught;

    /* Ignore signals, but don't ignore the children.  When wait3
       returns the child process, set the time the command finished. */
    while ((caught = wait3 (&status, 0, &resp->ru)) != pid)
    {
	if (caught == -1)
	    return 0;
    }

    gettimeofday (&resp->elapsed, (struct timezone *) 0);
    resp->elapsed.tv_sec -= resp->start.tv_sec;
    if (resp->elapsed.tv_usec < resp->start.tv_usec)
    {
	/* Manually carry a one from the seconds field.  */
	resp->elapsed.tv_usec += 1000000;
	--resp->elapsed.tv_sec;
    }
    resp->elapsed.tv_usec -= resp->start.tv_usec;

    resp->waitstatus = status;

    return 1;
}

/* Print ARGV to FP, with each entry in ARGV separated by FILLER.  */
static void fprintargv (FILE *fp, char *const *argv, const char *filler)
{
    char *const *av;

    av = argv;
    fputs (*av, fp);
    while (*++av)
    {
	fputs (filler, fp);
	fputs (*av, fp);
    }
    if (ferror (fp))
	bb_error_msg_and_die("write error");
}

/* Return the number of kilobytes corresponding to a number of pages PAGES.
   (Actually, we use it to convert pages*ticks into kilobytes*ticks.)

   Try to do arithmetic so that the risk of overflow errors is minimized.
   This is funky since the pagesize could be less than 1K.
   Note: Some machines express getrusage statistics in terms of K,
   others in terms of pages.  */

static unsigned long ptok (unsigned long pages)
{
    static unsigned long ps = 0;
    unsigned long tmp;
    static long size = LONG_MAX;

    /* Initialization.  */
    if (ps == 0)
	ps = (long) getpagesize ();

    /* Conversion.  */
    if (pages > (LONG_MAX / ps))
    {				/* Could overflow.  */
	tmp = pages / 1024;	/* Smaller first, */
	size = tmp * ps;		/* then larger.  */
    }
    else
    {				/* Could underflow.  */
	tmp = pages * ps;		/* Larger first, */
	size = tmp / 1024;	/* then smaller.  */
    }
    return size;
}

/* summarize: Report on the system use of a command.

   Copy the FMT argument to FP except that `%' sequences
   have special meaning, and `\n' and `\t' are translated into
   newline and tab, respectively, and `\\' is translated into `\'.

   The character following a `%' can be:
   (* means the tcsh time builtin also recognizes it)
   % == a literal `%'
   C == command name and arguments
*  D == average unshared data size in K (ru_idrss+ru_isrss)
*  E == elapsed real (wall clock) time in [hour:]min:sec
*  F == major page faults (required physical I/O) (ru_majflt)
*  I == file system inputs (ru_inblock)
*  K == average total mem usage (ru_idrss+ru_isrss+ru_ixrss)
*  M == maximum resident set size in K (ru_maxrss)
*  O == file system outputs (ru_oublock)
*  P == percent of CPU this job got (total cpu time / elapsed time)
*  R == minor page faults (reclaims; no physical I/O involved) (ru_minflt)
*  S == system (kernel) time (seconds) (ru_stime)
*  T == system time in [hour:]min:sec
*  U == user time (seconds) (ru_utime)
*  u == user time in [hour:]min:sec
*  W == times swapped out (ru_nswap)
*  X == average amount of shared text in K (ru_ixrss)
   Z == page size
*  c == involuntary context switches (ru_nivcsw)
   e == elapsed real time in seconds
*  k == signals delivered (ru_nsignals)
   p == average unshared stack size in K (ru_isrss)
*  r == socket messages received (ru_msgrcv)
*  s == socket messages sent (ru_msgsnd)
   t == average resident set size in K (ru_idrss)
*  w == voluntary context switches (ru_nvcsw)
   x == exit status of command

   Various memory usages are found by converting from page-seconds
   to kbytes by multiplying by the page size, dividing by 1024,
   and dividing by elapsed real time.

   FP is the stream to print to.
   FMT is the format string, interpreted as described above.
   COMMAND is the command and args that are being summarized.
   RESP is resource information on the command.  */

static void summarize (FILE *fp, const char *fmt, char **command, resource_t *resp)
{
    unsigned long r;		/* Elapsed real milliseconds.  */
    unsigned long v;		/* Elapsed virtual (CPU) milliseconds.  */

    if (WIFSTOPPED (resp->waitstatus))
	fprintf (fp, "Command stopped by signal %d\n", WSTOPSIG (resp->waitstatus));
    else if (WIFSIGNALED (resp->waitstatus))
	fprintf (fp, "Command terminated by signal %d\n", WTERMSIG (resp->waitstatus));
    else if (WIFEXITED (resp->waitstatus) && WEXITSTATUS (resp->waitstatus))
	fprintf (fp, "Command exited with non-zero status %d\n", WEXITSTATUS (resp->waitstatus));

    /* Convert all times to milliseconds.  Occasionally, one of these values
       comes out as zero.  Dividing by zero causes problems, so we first
       check the time value.  If it is zero, then we take `evasive action'
       instead of calculating a value.  */

    r = resp->elapsed.tv_sec * 1000 + resp->elapsed.tv_usec / 1000;

    v = resp->ru.ru_utime.tv_sec * 1000 + resp->ru.ru_utime.TV_MSEC +
	resp->ru.ru_stime.tv_sec * 1000 + resp->ru.ru_stime.TV_MSEC;

    while (*fmt)
    {
	switch (*fmt)
	{
	    case '%':
		switch (*++fmt)
		{
		    case '%':		/* Literal '%'.  */
			putc ('%', fp);
			break;
		    case 'C':		/* The command that got timed.  */
			fprintargv (fp, command, " ");
			break;
		    case 'D':		/* Average unshared data size.  */
			fprintf (fp, "%lu",
				MSEC_TO_TICKS (v) == 0 ? 0 :
				ptok ((UL) resp->ru.ru_idrss) / MSEC_TO_TICKS (v) +
				ptok ((UL) resp->ru.ru_isrss) / MSEC_TO_TICKS (v));
			break;
		    case 'E':		/* Elapsed real (wall clock) time.  */
			if (resp->elapsed.tv_sec >= 3600)	/* One hour -> h:m:s.  */
			    fprintf (fp, "%ldh %ldm %02lds",
				    resp->elapsed.tv_sec / 3600,
				    (resp->elapsed.tv_sec % 3600) / 60,
				    resp->elapsed.tv_sec % 60);
			else
			    fprintf (fp, "%ldm %ld.%02lds",	/* -> m:s.  */
				    resp->elapsed.tv_sec / 60,
				    resp->elapsed.tv_sec % 60,
				    resp->elapsed.tv_usec / 10000);
			break;
		    case 'F':		/* Major page faults.  */
			fprintf (fp, "%ld", resp->ru.ru_majflt);
			break;
		    case 'I':		/* Inputs.  */
			fprintf (fp, "%ld", resp->ru.ru_inblock);
			break;
		    case 'K':		/* Average mem usage == data+stack+text.  */
			fprintf (fp, "%lu",
				MSEC_TO_TICKS (v) == 0 ? 0 :
				ptok ((UL) resp->ru.ru_idrss) / MSEC_TO_TICKS (v) +
				ptok ((UL) resp->ru.ru_isrss) / MSEC_TO_TICKS (v) +
				ptok ((UL) resp->ru.ru_ixrss) / MSEC_TO_TICKS (v));
			break;
		    case 'M':		/* Maximum resident set size.  */
			fprintf (fp, "%lu", ptok ((UL) resp->ru.ru_maxrss));
			break;
		    case 'O':		/* Outputs.  */
			fprintf (fp, "%ld", resp->ru.ru_oublock);
			break;
		    case 'P':		/* Percent of CPU this job got.  */
			/* % cpu is (total cpu time)/(elapsed time).  */
			if (r > 0)
			    fprintf (fp, "%lu%%", (v * 100 / r));
			else
			    fprintf (fp, "?%%");
			break;
		    case 'R':		/* Minor page faults (reclaims).  */
			fprintf (fp, "%ld", resp->ru.ru_minflt);
			break;
		    case 'S':		/* System time.  */
			fprintf (fp, "%ld.%02ld",
				resp->ru.ru_stime.tv_sec,
				resp->ru.ru_stime.TV_MSEC / 10);
			break;
		    case 'T':		/* System time.  */
			if (resp->ru.ru_stime.tv_sec >= 3600)	/* One hour -> h:m:s.  */
			    fprintf (fp, "%ldh %ldm %02lds",
				    resp->ru.ru_stime.tv_sec / 3600,
				    (resp->ru.ru_stime.tv_sec % 3600) / 60,
				    resp->ru.ru_stime.tv_sec % 60);
			else
			    fprintf (fp, "%ldm %ld.%02lds",	/* -> m:s.  */
				    resp->ru.ru_stime.tv_sec / 60,
				    resp->ru.ru_stime.tv_sec % 60,
				    resp->ru.ru_stime.tv_usec / 10000);
			break;
		    case 'U':		/* User time.  */
			fprintf (fp, "%ld.%02ld",
				resp->ru.ru_utime.tv_sec,
				resp->ru.ru_utime.TV_MSEC / 10);
			break;
		    case 'u':		/* User time.  */
			if (resp->ru.ru_utime.tv_sec >= 3600)	/* One hour -> h:m:s.  */
			    fprintf (fp, "%ldh %ldm %02lds",
				    resp->ru.ru_utime.tv_sec / 3600,
				    (resp->ru.ru_utime.tv_sec % 3600) / 60,
				    resp->ru.ru_utime.tv_sec % 60);
			else
			    fprintf (fp, "%ldm %ld.%02lds",	/* -> m:s.  */
				    resp->ru.ru_utime.tv_sec / 60,
				    resp->ru.ru_utime.tv_sec % 60,
				    resp->ru.ru_utime.tv_usec / 10000);
			break;
		    case 'W':		/* Times swapped out.  */
			fprintf (fp, "%ld", resp->ru.ru_nswap);
			break;
		    case 'X':		/* Average shared text size.  */
			fprintf (fp, "%lu",
				MSEC_TO_TICKS (v) == 0 ? 0 :
				ptok ((UL) resp->ru.ru_ixrss) / MSEC_TO_TICKS (v));
			break;
		    case 'Z':		/* Page size.  */
			fprintf (fp, "%d", getpagesize ());
			break;
		    case 'c':		/* Involuntary context switches.  */
			fprintf (fp, "%ld", resp->ru.ru_nivcsw);
			break;
		    case 'e':		/* Elapsed real time in seconds.  */
			fprintf (fp, "%ld.%02ld",
				resp->elapsed.tv_sec,
				resp->elapsed.tv_usec / 10000);
			break;
		    case 'k':		/* Signals delivered.  */
			fprintf (fp, "%ld", resp->ru.ru_nsignals);
			break;
		    case 'p':		/* Average stack segment.  */
			fprintf (fp, "%lu",
				MSEC_TO_TICKS (v) == 0 ? 0 :
				ptok ((UL) resp->ru.ru_isrss) / MSEC_TO_TICKS (v));
			break;
		    case 'r':		/* Incoming socket messages received.  */
			fprintf (fp, "%ld", resp->ru.ru_msgrcv);
			break;
		    case 's':		/* Outgoing socket messages sent.  */
			fprintf (fp, "%ld", resp->ru.ru_msgsnd);
			break;
		    case 't':		/* Average resident set size.  */
			fprintf (fp, "%lu",
				MSEC_TO_TICKS (v) == 0 ? 0 :
				ptok ((UL) resp->ru.ru_idrss) / MSEC_TO_TICKS (v));
			break;
		    case 'w':		/* Voluntary context switches.  */
			fprintf (fp, "%ld", resp->ru.ru_nvcsw);
			break;
		    case 'x':		/* Exit status.  */
			fprintf (fp, "%d", WEXITSTATUS (resp->waitstatus));
			break;
		    case '\0':
			putc ('?', fp);
			return;
		    default:
			putc ('?', fp);
			putc (*fmt, fp);
		}
		++fmt;
		break;

	    case '\\':		/* Format escape.  */
		switch (*++fmt)
		{
		    case 't':
			putc ('\t', fp);
			break;
		    case 'n':
			putc ('\n', fp);
			break;
		    case '\\':
			putc ('\\', fp);
			break;
		    default:
			putc ('?', fp);
			putc ('\\', fp);
			putc (*fmt, fp);
		}
		++fmt;
		break;

	    default:
		putc (*fmt++, fp);
	}

	if (ferror (fp))
	    bb_error_msg_and_die("write error");
    }
    putc ('\n', fp);

    if (ferror (fp))
	bb_error_msg_and_die("write error");
}

/* Run command CMD and return statistics on it.
   Put the statistics in *RESP.  */
static void run_command (char *const *cmd, resource_t *resp)
{
    pid_t pid;			/* Pid of child.  */
    __sighandler_t interrupt_signal, quit_signal;

    gettimeofday (&resp->start, (struct timezone *) 0);
    pid = fork ();		/* Run CMD as child process.  */
    if (pid < 0)
	bb_error_msg_and_die("cannot fork");
    else if (pid == 0)
    {				/* If child.  */
	/* Don't cast execvp arguments; that causes errors on some systems,
	   versus merely warnings if the cast is left off.  */
	execvp (cmd[0], cmd);
	bb_error_msg("cannot run %s", cmd[0]);
	_exit (errno == ENOENT ? 127 : 126);
    }

    /* Have signals kill the child but not self (if possible).  */
    interrupt_signal = signal (SIGINT, SIG_IGN);
    quit_signal = signal (SIGQUIT, SIG_IGN);

    if (resuse_end (pid, resp) == 0)
	bb_error_msg("error waiting for child process");

    /* Re-enable signals.  */
    signal (SIGINT, interrupt_signal);
    signal (SIGQUIT, quit_signal);
}

extern int time_main (int argc, char **argv)
{
    int gotone;
    resource_t res;
    const char *output_format = default_format;

    argc--;
    argv++;
    /* Parse any options  -- don't use getopt() here so we don't
     * consume the args of our client application... */
    while (argc > 0 && **argv == '-') {
	gotone = 0;
	while (gotone==0 && *++(*argv)) {
	    switch (**argv) {
		case 'v':
		    output_format = long_format;
		    break;
		case 'p':
		    output_format = posix_format;
		    break;
		default:
		    bb_show_usage();
	    }
	    argc--;
	    argv++;
	    gotone = 1;
	}
    }

    if (argv == NULL || *argv == NULL)
	bb_show_usage();

    run_command (argv, &res);
    summarize (stderr, output_format, argv, &res);
    fflush (stderr);

    if (WIFSTOPPED (res.waitstatus))
	exit (WSTOPSIG (res.waitstatus));
    else if (WIFSIGNALED (res.waitstatus))
	exit (WTERMSIG (res.waitstatus));
    else if (WIFEXITED (res.waitstatus))
	exit (WEXITSTATUS (res.waitstatus));
    return 0;
}
