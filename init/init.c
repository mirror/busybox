/*
 * Mini init implementation for busybox
 *
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Adjusted by so many folks, it's impossible to keep track.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/kdaemon.h>
#include <sys/sysmacros.h>
#include <linux/serial.h>	/* for serial_struct */
#include <sys/vt.h>		/* for vt_stat */
#include <sys/ioctl.h>

/* Turn this on to debug init so it won't reboot when killed */
//#define DEBUG_INIT

#define CONSOLE         "/dev/console"	/* Logical system console */
#define VT_PRIMARY      "/dev/tty0"	/* Primary virtual console */
#define VT_SECONDARY    "/dev/tty1"	/* Virtual console */
#define VT_LOG          "/dev/tty2"	/* Virtual console */
#define SERIAL_CON0     "/dev/ttyS0"    /* Primary serial console */
#define SERIAL_CON1     "/dev/ttyS1"    /* Serial console */
#define SHELL           "/bin/sh"	/* Default shell */
#define INITSCRIPT      "/etc/init.d/rcS"	/* Initscript. */
#define PATH_DEFAULT    "PATH=/usr/local/sbin:/sbin:/bin:/usr/sbin:/usr/bin"

static char *console = VT_PRIMARY;
static char *second_terminal = VT_SECONDARY;
static char *log = "/dev/tty3";



/* try to open up the specified device */
int device_open(char *device, int mode)
{
    int m, f, fd = -1;

    m = mode | O_NONBLOCK;

    /* Retry up to 5 times */
    for (f = 0; f < 5; f++)
	if ((fd = open(device, m)) >= 0)
	    break;
    if (fd < 0)
	return fd;
    /* Set original flags. */
    if (m != mode)
	fcntl(fd, F_SETFL, mode);
    return fd;
}

/* print a message to the specified device */
void message(char *device, char *fmt, ...)
{
    int fd;
    va_list arguments;

    if ((fd = device_open(device, O_WRONLY|O_NOCTTY|O_NDELAY)) >= 0) {
	va_start(arguments, fmt);
	vdprintf(fd, fmt, arguments);
	va_end(arguments);
	close(fd);
    } else {
	fprintf(stderr, "Bummer, can't print: ");
	va_start(arguments, fmt);
	vfprintf(stderr, fmt, arguments);
	fflush(stderr);
	va_end(arguments);
    }
}

/* Set terminal settings to reasonable defaults */
void set_term( int fd)
{
    struct termios tty;
    static const char control_characters[] = {
	'\003', '\034', '\177', '\025', '\004', '\0',
	'\1', '\0', '\021', '\023', '\032', '\0', '\022',
	'\017', '\027', '\026', '\0'
	};

    tcgetattr(fd, &tty);

    /* input modes */
    tty.c_iflag = IGNPAR|ICRNL|IXON|IXOFF|IXANY;

    /* use line dicipline 0 */
    tty.c_line = 0;

    /* output modes */
    tty.c_oflag = OPOST|ONLCR;

    /* local modes */
    tty.c_lflag = ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHOCTL|ECHOPRT|ECHOKE|IEXTEN;

    /* control chars */
    memcpy(tty.c_cc, control_characters, sizeof(control_characters));

    tcsetattr(fd, TCSANOW, &tty);
}

/* How much memory does this machine have? */
static int mem_total()
{
    char s[80];
    char *p = "/proc/meminfo";
    FILE *f;
    const char pattern[] = "MemTotal:";

    if ((f = fopen(p, "r")) < 0) {
	message(log, "Error opening %s: %s\n", p, strerror( errno));
	return -1;
    }
    while (NULL != fgets(s, 79, f)) {
	p = strstr(s, pattern);
	if (NULL != p) {
	    fclose(f);
	    return (atoi(p + strlen(pattern)));
	}
    }
    return -1;
}

static void console_init()
{
    int fd;
    int tried_devcons = 0;
    int tried_vtprimary = 0;
    char *s;

    if ((s = getenv("CONSOLE")) != NULL) {
	console = s;
/* Apparently the sparc does wierd things... */
#if defined (__sparc__)
	if (strncmp( s, "/dev/tty", 8 )==0) {
	    switch( s[8]) {
		case 'a':
		    s=SERIAL_CON0;
		    break;
		case 'b':
		    s=SERIAL_CON1;
	    }
	}
#endif
    } else {
	console = VT_PRIMARY;
	tried_vtprimary++;
    }

    while ((fd = open(console, O_RDONLY | O_NONBLOCK)) < 0) {
	/* Can't open selected console -- try vt1 */
	if (!tried_vtprimary) {
	    tried_vtprimary++;
	    console = VT_PRIMARY;
	    continue;
	}
	/* Can't open selected console -- try /dev/console */
	if (!tried_devcons) {
	    tried_devcons++;
	    console = CONSOLE;
	    continue;
	}
	break;
    }
    if (fd < 0)
	/* Perhaps we should panic here? */
	console = "/dev/null";
    else
	close(fd);
    message(log, "console=%s\n", console );
}

static int waitfor(int pid)
{
    int status, wpid;

    message(log, "Waiting for process %d.\n", pid);
    while ((wpid = wait(&status)) != pid) {
	if (wpid > 0)
	    message(log, "pid %d exited, status=%x.\n", wpid, status);
    }
    return wpid;
}


static pid_t run(const char * const* command, 
	char *terminal, int get_enter)
{
    int fd;
    pid_t pid;
    const char * const* cmd = command+1;
    static const char press_enter[] =
	"\nPlease press Enter to activate this console. ";

    if ((pid = fork()) == 0) {
	/* Clean up */
	close(0);
	close(1);
	close(2);
	setsid();

	/* Reset signal handlers set for parent process */
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	if ((fd = device_open(terminal, O_RDWR)) < 0) {
	    message(log, "Bummer, can't open %s\r\n", terminal);
	    exit(-1);
	}
	dup(fd);
	dup(fd);
	tcsetpgrp(0, getpgrp());
	set_term(0);

	if (get_enter==TRUE) {
	    /*
	     * Save memory by not exec-ing anything large (like a shell)
	     * before the user wants it. This is critical if swap is not
	     * enabled and the system has low memory. Generally this will
	     * be run on the second virtual console, and the first will
	     * be allowed to start a shell or whatever an init script 
	     * specifies.
	     */
	    char c;
	    write(1, press_enter, sizeof(press_enter) - 1);
	    read(0, &c, 1);
	}

	/* Log the process name and args */
	message(log, "Starting pid(%d): '", getpid());
	while ( *cmd) message(log, "%s ", *cmd++);
	message(log, "'\r\n");
	
	/* Now run it.  The new program will take over this PID, 
	 * so nothing further in init.c should be run. */
	execvp(*command, (char**)command+1);

	message(log, "Bummer, could not run '%s'\n", command);
	exit(-1);
    }
    return pid;
}

/* Make sure there is enough memory to do something useful. *
 * Calls swapon if needed so be sure /proc is mounted. */
static void check_memory()
{
    struct stat statbuf;
    const char* const swap_on_cmd[] = 
	    { "/bin/swapon", "swapon", "-a", 0};
    const char *no_memory =
	    "Sorry, your computer does not have enough memory.\r\n";

    if (mem_total() > 3500)
	return;

    if (stat("/etc/fstab", &statbuf) == 0) {
	/* Try to turn on swap */
	waitfor(run(swap_on_cmd, log, FALSE));
	if (mem_total() < 3500)
	    goto goodnight;
    } else
	goto goodnight;
    return;

goodnight:
	message(console, "%s", no_memory);
	while (1) sleep(1);
}

static void shutdown_system(void)
{
    const char* const swap_off_cmd[] = { "swapoff", "swapoff", "-a", 0};
    const char* const umount_cmd[] = { "umount", "umount", "-a", "-n", 0};

    message(console, "The system is going down NOW !!\r\n");
    sync();
#ifndef DEBUG_INIT
    /* Allow Ctrl-Alt-Del to reboot system. */
    reboot(RB_ENABLE_CAD);
#endif
    /* Send signals to every process _except_ pid 1 */
    message(console, "Sending SIGHUP to all processes.\r\n");
#ifndef DEBUG_INIT
    kill(-1, SIGHUP);
#endif
    sleep(2);
    sync();
    message(console, "Sending SIGKILL to all processes.\r\n");
#ifndef DEBUG_INIT
    kill(-1, SIGKILL);
#endif
    sleep(1);
    waitfor(run( swap_off_cmd, log, FALSE));
    waitfor(run( umount_cmd, log, FALSE));
    sync();
    if (get_kernel_revision() <= 2 * 65536 + 2 * 256 + 11) {
	/* Removed  bdflush call, kupdate in kernels >2.2.11 */
	bdflush(1, 0);
	sync();
    }
}

static void halt_signal(int sig)
{
    shutdown_system();
    message(console,
	    "The system is halted. Press CTRL-ALT-DEL or turn off power\r\n");
#ifndef DEBUG_INIT
    reboot(RB_POWER_OFF);
#endif
    exit(0);
}

static void reboot_signal(int sig)
{
    shutdown_system();
    message(console, "Please stand by while rebooting the system.\r\n");
#ifndef DEBUG_INIT
    reboot(RB_AUTOBOOT);
#endif
    exit(0);
}

extern int init_main(int argc, char **argv)
{
    int run_rc = TRUE;
    int wait_for_enter = TRUE;
    pid_t pid1 = 0;
    pid_t pid2 = 0;
    struct stat statbuf;
    const char* const init_commands[] = { INITSCRIPT, INITSCRIPT, 0};
    const char* const shell_commands[] = { SHELL, "-" SHELL, 0};
    const char* const* tty0_commands = shell_commands;
    const char* const* tty1_commands = shell_commands;
#ifdef DEBUG_INIT
    char *hello_msg_format =
	"init(%d) started:  BusyBox v%s (%s) multi-call binary\r\n";
#else
    char *hello_msg_format =
	"init started:  BusyBox v%s (%s) multi-call binary\r\n";
#endif

    
    /* Check if we are supposed to be in single user mode */
    if ( argc > 1 && (!strcmp(argv[1], "single") || 
		!strcmp(argv[1], "-s") || !strcmp(argv[1], "1"))) {
	run_rc = FALSE;
    }

    
    /* Set up sig handlers  -- be sure to
     * clear all of these in run() */
    signal(SIGUSR1, halt_signal);
    signal(SIGUSR2, reboot_signal);
    signal(SIGINT, reboot_signal);
    signal(SIGTERM, reboot_signal);

    /* Turn off rebooting via CTL-ALT-DEL -- we get a 
     * SIGINT on CAD so we can shut things down gracefully... */
#ifndef DEBUG_INIT
    reboot(RB_DISABLE_CAD);
#endif 

    /* Figure out where the default console should be */
    console_init();

    /* Close whatever files are open, and reset the console. */
    close(0);
    close(1);
    close(2);
    set_term(0);
    setsid();

    /* Make sure PATH is set to something sane */
    putenv(PATH_DEFAULT);

   
    /* Hello world */
#ifndef DEBUG_INIT
    message(log, hello_msg_format, BB_VER, BB_BT);
    message(console, hello_msg_format, BB_VER, BB_BT);
#else
    message(log, hello_msg_format, getpid(), BB_VER, BB_BT);
    message(console, hello_msg_format, getpid(), BB_VER, BB_BT);
#endif

    
    /* Mount /proc */
    if (mount ("proc", "/proc", "proc", 0, 0) == 0) {
	message(log, "Mounting /proc: done.\n");
	message(console, "Mounting /proc: done.\n");
    } else {
	message(log, "Mounting /proc: failed!\n");
	message(console, "Mounting /proc: failed!\n");
    }

    /* Make sure there is enough memory to do something useful. */
    check_memory();


    /* Make sure an init script exists before trying to run it */
    if (run_rc == TRUE && stat(INITSCRIPT, &statbuf)==0) {
	wait_for_enter = FALSE;
	tty0_commands = init_commands;
    }


    /* Ok, now launch the rc script and/or prepare to 
     * start up some VTs if somebody hits enter... 
     */
    for (;;) {
	pid_t wpid;
	int status;

	if (pid1 == 0 && tty0_commands) {
	    pid1 = run(tty0_commands, console, wait_for_enter);
	}
	if (pid2 == 0 && tty1_commands) {
	    pid2 = run(tty1_commands, second_terminal, TRUE);
	}
	wpid = wait(&status);
	if (wpid > 0 ) {
	    message(log, "pid %d exited, status=%x.\n", wpid, status);
	}
	if (wpid == pid1) {
	    pid1 = 0;
	    if (run_rc == TRUE) {
		/* Don't respawn init script if it exits. */
		run_rc=FALSE;
		wait_for_enter=TRUE;
		tty0_commands=shell_commands;
	    }
	}
	if (wpid == pid2) {
	    pid2 = 0;
	}
	sleep(1);
    }
}
