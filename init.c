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

#define DEBUG_INIT

#define CONSOLE         "/dev/console"	/* Logical system console */
#define VT_PRIMARY      "/dev/tty0"	/* Virtual console master */
#define VT_SECONDARY    "/dev/tty1"	/* Virtual console master */
#define VT_LOG          "/dev/tty2"	/* Virtual console master */
#define SHELL           "/bin/sh"	/* Default shell */
#define INITSCRIPT      "/etc/init.d/rcS"	/* Initscript. */
#define PATH_DEFAULT    "PATH=/usr/local/sbin:/sbin:/bin:/usr/sbin:/usr/bin"

static char *console = CONSOLE;
//static char *first_terminal = "/dev/tty1";
static char *second_terminal = "/dev/tty2";
static char *log = "/dev/tty3";



/* try to open up the specified device */
int device_open(char *device, int mode)
{
    int m, f, fd = -1;

    mode = m | O_NONBLOCK;

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

    tcgetattr(fd, &tty);
    tty.c_cflag &= CBAUD|CBAUDEX|CSIZE|CSTOPB|PARENB|PARODD;
    tty.c_cflag |= HUPCL|CLOCAL;

    tty.c_cc[VINTR] = 3;
    tty.c_cc[VQUIT] = 28;
    tty.c_cc[VERASE] = 127;
    tty.c_cc[VKILL] = 24;
    tty.c_cc[VEOF] = 4;
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VSTART] = 17;
    tty.c_cc[VSTOP] = 19;
    tty.c_cc[VSUSP] = 26;

    tty.c_iflag = IGNPAR|ICRNL|IXON|IXANY;
    tty.c_oflag = OPOST|ONLCR;
    tty.c_lflag = ISIG|ICANON|ECHO|ECHOCTL|ECHOPRT|ECHOKE;

    tcsetattr(fd, TCSANOW, &tty);
}

static int mem_total()
{
    char s[80];
    char *p;
    FILE *f;
    const char pattern[] = "MemTotal:";

    f = fopen("/proc/meminfo", "r");
    while (NULL != fgets(s, 79, f)) {
	p = strstr(s, pattern);
	if (NULL != p) {
	    fclose(f);
	    return (atoi(p + strlen(pattern)));
	}
    }
    return -1;
}

static void set_free_pages()
{
    char s[80];
    FILE *f;

    f = fopen("/proc/sys/vm/freepages", "r");
    fgets(s, 79, f);
    if (atoi(s) < 32) {
	fclose(f);
	f = fopen("/proc/sys/vm/freepages", "w");
	fprintf(f, "30\t40\t50\n");
	printf("\nIncreased /proc/sys/vm/freepages values to 30/40/50\n");
    }
    fclose(f);
}


static void console_init()
{
    int fd;
    struct stat statbuf;
    int tried_devcons = 0;
    int tried_vtmaster = 0;
    char *s;

    fprintf(stderr, "entering console_init()\n");
    if ((s = getenv("CONSOLE")) != NULL)
	console = s;
    else {
	console = CONSOLE;
	tried_devcons++;
    }

    if ( stat(CONSOLE, &statbuf) && S_ISLNK(statbuf.st_mode)) {
	fprintf(stderr, "Yikes! /dev/console does not exist or is a symlink.\n");
    }
    while ((fd = open(console, O_RDONLY | O_NONBLOCK)) < 0) {
	if (!tried_devcons) {
	    tried_devcons++;
	    console = CONSOLE;
	    continue;
	}
	if (!tried_vtmaster) {
	    tried_vtmaster++;
	    console = VT_PRIMARY;
	    continue;
	}
	break;
    }
    if (fd < 0)
	console = "/dev/null";
    else
	close(fd);
    fprintf(stderr, "console=%s\n", console);
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
    int i;
    pid_t pid;
    static const char press_enter[] =
	"\nPlease press Enter to activate this console. ";

    if ((pid = fork()) == 0) {
	/* Clean up */
	close(0);
	close(1);
	close(2);
	setsid();

	open(terminal, O_RDWR);
	dup(0);
	dup(0);
	tcsetpgrp(0, getpgrp());
	set_term(0);

	if (get_enter) {
	    /*
	     * Save memory by not exec-ing anything large (like a shell)
	     * before the user wants it. This is critical if swap is not
	     * enabled and the system has low memory. Generally this will
	     * be run on the second virtual console, and the first will
	     * be allowed to start a shell or whatever an init script 
	     * specifies.
	     */
	    char c;
	    write(0, press_enter, sizeof(press_enter) - 1);
	    read(0, &c, 1);
	}

	/* Log the process name and args */

	/* Now run it.  The new program will take over this PID, 
	 * so nothing further in init.c should be run. */
	message(log, "Executing '%s', pid(%d)\r\n", *command, getpid());
	execvp(*command, (char**)command+1);

	message(log, "Hmm.  Trying as a script.\r\n");
	/* If shell scripts are not executed, force the issue */
	if (errno == ENOEXEC) {
	    char * args[16];
	    args[0] = SHELL;
	    args[1] = "-c";
	    args[2] = "exec";
	    for( i=0 ; i<16 && command[i]; i++)
		args[3+i] = (char*)command[i];
	    args[i] = NULL;
	    execvp(*args, (char**)args+1);
	}
	message(log, "Could not execute '%s'\n", command);
	exit(-1);
    }
    return pid;
}


#ifndef DEBUG_INIT
static void shutdown_system(void)
{
    const char* const swap_off_cmd[] = { "/bin/swapoff", "-a", 0};
    const char* const umount_cmd[] = { "/bin/umount", "-a", "-n", 0};

    message(console, "The system is going down NOW !!\r\n");
    sync();
    /* Allow Ctrl-Alt-Del to reboot system. */
    reboot(RB_ENABLE_CAD);

    /* Send signals to every process _except_ pid 1 */
    message(console, "Sending SIGHUP to all processes.\r\n");
    kill(-1, SIGHUP);
    sleep(2);
    sync();
    message(console, "Sending SIGKILL to all processes.\r\n");
    kill(-1, SIGKILL);
    sleep(1);
    waitfor(run( swap_off_cmd, console, 0));
    waitfor(run( umount_cmd, console, 0));
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
    reboot(RB_POWER_OFF);
    exit(0);
}

static void reboot_signal(int sig)
{
    shutdown_system();
    message(console, "Please stand by while rebooting the system.\r\n");
    reboot(RB_AUTOBOOT);
    exit(0);
}

#endif

extern int init_main(int argc, char **argv)
{
    int run_rc = TRUE;
    pid_t pid = 0;
    pid_t pid1 = 0;
    pid_t pid2 = 0;
    struct stat statbuf;
    const char* const swap_on_cmd[] = { "/bin/swapon", " -a ", 0};
    const char* const init_commands[] = { SHELL, " -c", " exec ", INITSCRIPT, 0};
    const char* const shell_commands[] = { SHELL, " -", 0};
    const char* const* tty0_commands = init_commands;
    const char* const* tty1_commands = shell_commands;
    char *hello_msg_format =
	"init(%d) started:  BusyBox v%s (%s) multi-call binary\r\n";
    const char *no_memory =
	"Sorry, your computer does not have enough memory.\r\n";

    pid = getpid();

#ifndef DEBUG_INIT
    /* Set up sig handlers */
    signal(SIGUSR1, halt_signal);
    signal(SIGSEGV, halt_signal);
    signal(SIGPWR, halt_signal);
    signal(SIGALRM, halt_signal);
    signal(SIGHUP, halt_signal);
    signal(SIGUSR2, reboot_signal);
    signal(SIGINT, reboot_signal);
    signal(SIGTERM, reboot_signal);

    /* Turn off rebooting via CTL-ALT-DEL -- we get a 
     * SIGINT on CAD so we can shut things down gracefully... */
    reboot(RB_DISABLE_CAD);
#endif
    
    /* Figure out where the default console should be */
    console_init();

    /* Close whatever files are open, and reset the console. */
    close(0);
    close(1);
    close(2);
    //set_term(0);
    setsid();

    /* Make sure PATH is set to something sane */
    if (getenv("PATH") == NULL)
	putenv(PATH_DEFAULT);

    /* Hello world */
    message(log, hello_msg_format, pid, BB_VER, BB_BT);
    fprintf(stderr, hello_msg_format, pid, BB_VER, BB_BT);

    
    /* Mount /proc */
    if (mount("/proc", "/proc", "proc", 0, 0) == 0) {
	fprintf(stderr, "Mounting /proc: done.\n");
	message(log, "Mounting /proc: done.\n");
    } else {
	fprintf(stderr, "Mounting /proc: failed!\n");
	message(log, "Mounting /proc: failed!\n");
    }

    /* Make sure there is enough memory to do something useful */
    set_free_pages();
    if (mem_total() < 2000) {
	int retval;
	retval = stat("/etc/fstab", &statbuf);
	if (retval) {
	    fprintf(stderr, "%s", no_memory);
	    while (1) {
		sleep(1);
	    }
	} else {
	    /* Try to turn on swap */
	    waitfor(run(swap_on_cmd, console, 0));
	    if (mem_total() < 2000) {
		fprintf(stderr, "%s", no_memory);
		while (1) {
		    sleep(1);
		}
	    }
	}
    }

    /* Check if we are supposed to be in single user mode */
    if ( argc > 1 && (!strcmp(argv[1], "single") || 
		!strcmp(argv[1], "-s") || !strcmp(argv[1], "1"))) {
	run_rc = FALSE;
	tty0_commands = shell_commands;
	tty1_commands = shell_commands;
    }

    /* Make sure an init script exists before trying to run it */
    if (run_rc == TRUE && stat(INITSCRIPT, &statbuf)) {
	tty0_commands = shell_commands;
	tty1_commands = shell_commands;
    }

    /* Ok, now launch the rc script and/or prepare to 
     * start up some VTs if somebody hits enter... 
     */
    for (;;) {
	pid_t wpid;
	int status;

	if (pid1 == 0 && tty0_commands) {
	    pid1 = run(tty0_commands, console, 1);
	}
	if (pid2 == 0 && tty1_commands) {
	    pid2 = run(tty1_commands, second_terminal, 1);
	}
	wpid = wait(&status);
	if (wpid > 0 ) {
	    message(log, "pid %d exited, status=%x.\n", wpid, status);
	}
	/* Don't respawn an init script if it exits */
	if (run_rc == FALSE && wpid == pid1) {
	    pid1 = 0;
	}
	if (wpid == pid2) {
	    pid2 = 0;
	}
	sleep(1);
    }
}
