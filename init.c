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
#ifdef BB_SYSLOGD
#include <sys/syslog.h>
#endif

#define DEV_CONSOLE      "/dev/console"	/* Logical system console */
#define VT_PRIMARY      "/dev/tty1"	/* Primary virtual console */
#define VT_SECONDARY    "/dev/tty2"	/* Virtual console */
#define VT_LOG          "/dev/tty3"	/* Virtual console */
#define SERIAL_CON0     "/dev/ttyS0"    /* Primary serial console */
#define SERIAL_CON1     "/dev/ttyS1"    /* Serial console */
#define SHELL           "/bin/sh"	/* Default shell */
#define INITSCRIPT      "/etc/init.d/rcS"	/* Initscript. */
#define PATH_DEFAULT    "PATH=/usr/local/sbin:/sbin:/bin:/usr/sbin:/usr/bin"

#define LOG             0x1
#define CONSOLE         0x2
static char *console = DEV_CONSOLE;
static char *second_console = VT_SECONDARY;
static char *log = VT_LOG;
static int kernel_version = 0;


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
    /* Reset original flags. */
    if (m != mode)
	fcntl(fd, F_SETFL, mode);
    return fd;
}

/* print a message to the specified device:
 * device may be bitwise-or'd from LOG | CONSOLE */
void message(int device, char *fmt, ...)
{
    int fd;
    va_list arguments;
#ifdef BB_SYSLOGD

    /* Log the message to syslogd */
    if (device & LOG ) {
	char msg[1024];
	va_start(arguments, fmt);
	vsnprintf(msg, sizeof(msg), fmt, arguments);
	va_end(arguments);
	syslog(LOG_DAEMON|LOG_NOTICE, msg);
    }

#else
    static int log_fd=-1;

    /* Take full control of the log tty, and never close it.
     * It's mine, all mine!  Muhahahaha! */
    if (log_fd < 0) {
	if (log == NULL) {
	/* don't even try to log, because there is no such console */
	log_fd = -2;
	/* log to main console instead */
	device = CONSOLE;
    }
    else if ((log_fd = device_open(log, O_RDWR|O_NDELAY)) < 0) {
	    log_fd=-1;
	    fprintf(stderr, "Bummer, can't write to log on %s!\r\n", log);
	    fflush(stderr);
	    return;
	}
    }
    if ( (device & LOG) && (log_fd >= 0) ) {
	va_start(arguments, fmt);
	vdprintf(log_fd, fmt, arguments);
	va_end(arguments);
    }
#endif

    if (device & CONSOLE) {
	/* Always send console messages to /dev/console so people will see them. */
	if ((fd = device_open(DEV_CONSOLE, O_WRONLY|O_NOCTTY|O_NDELAY)) >= 0) {
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
}


/* Set terminal settings to reasonable defaults */
void set_term( int fd)
{
    struct termios tty;
#if 0
    static const char control_characters[] = {
	'\003', '\034', '\177', '\030', '\004', '\0',
	'\1', '\0', '\021', '\023', '\032', '\0', '\022',
	'\017', '\027', '\026', '\0'
	};
#else	
    static const char control_characters[] = {
	'\003', '\034', '\177', '\025', '\004', '\0',
	'\1', '\0', '\021', '\023', '\032', '\0', '\022',
	'\017', '\027', '\026', '\0'
	};
#endif

    tcgetattr(fd, &tty);

    /* set control chars */
    memcpy(tty.c_cc, control_characters, sizeof(control_characters));

    /* use line dicipline 0 */
    tty.c_line = 0;

    /* Make it be sane */
    //tty.c_cflag &= CBAUD|CBAUDEX|CSIZE|CSTOPB|PARENB|PARODD;
    //tty.c_cflag |= HUPCL|CLOCAL;

    /* input modes */
    tty.c_iflag = ICRNL|IXON|IXOFF;

    /* output modes */
    tty.c_oflag = OPOST|ONLCR;

    /* local modes */
    tty.c_lflag = ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE|IEXTEN;

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
	message(LOG, "Error opening %s: %s\n", p, strerror( errno));
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
    struct serial_struct sr;
    char *s;

    if ((s = getenv("CONSOLE")) != NULL) {
	console = s;
    }
#if #cpu(sparc)
    /* sparc kernel supports console=tty[ab] parameter which is also 
     * passed to init, so catch it here */
    else if ((s = getenv("console")) != NULL) {
	/* remap tty[ab] to /dev/ttyS[01] */
	if (strcmp( s, "ttya" )==0)
	    console = SERIAL_CON0;
	else if (strcmp( s, "ttyb" )==0)
	    console = SERIAL_CON1;
    }
#endif
    else {
	struct vt_stat vt;
	static char the_console[13];

	console = the_console;
	/* 2.2 kernels: identify the real console backend and try to use it */
	if (ioctl(0, TIOCGSERIAL, &sr) == 0) {
	    /* this is a serial console */
	    snprintf( the_console, sizeof the_console, "/dev/ttyS%d", sr.line );
	}
	else if (ioctl(0, VT_GETSTATE, &vt) == 0) {
	    /* this is linux virtual tty */
	    snprintf( the_console, sizeof the_console, "/dev/tty%d", vt.v_active );
	} else {
	    console = DEV_CONSOLE;
	    tried_devcons++;
	}
    }

    while ((fd = open(console, O_RDONLY | O_NONBLOCK)) < 0) {
	/* Can't open selected console -- try /dev/console */
	if (!tried_devcons) {
	    tried_devcons++;
	    console = DEV_CONSOLE;
	    continue;
	}
	/* Can't open selected console -- try vt1 */
	if (!tried_vtprimary) {
	    tried_vtprimary++;
	    console = VT_PRIMARY;
	    continue;
	}
	break;
    }
    if (fd < 0)
	/* Perhaps we should panic here? */
	console = "/dev/null";
    else {
	/* check for serial console and disable logging to tty3 & running a
	* shell to tty2 */
	if (ioctl(0,TIOCGSERIAL,&sr) == 0) {
	    message(LOG|CONSOLE, "serial console detected.  Disabling 2nd virtual terminal.\r\n", console );
	    log = NULL;
	    second_console = NULL;
	}
	close(fd);
    }
    message(LOG, "console=%s\n", console );
}

static int waitfor(int pid)
{
    int status, wpid;

    message(LOG, "Waiting for process %d.\n", pid);
    while ((wpid = wait(&status)) != pid) {
	if (wpid > 0)
	    message(LOG, "pid %d exited, status=0x%x.\n", wpid, status);
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
	    message(LOG, "Bummer, can't open %s\r\n", terminal);
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
	    message(LOG, "Waiting for enter to start '%s' (pid %d, console %s)\r\n", 
		    *cmd, getpid(), terminal );
	    write(1, press_enter, sizeof(press_enter) - 1);
	    read(0, &c, 1);
	}

	/* Log the process name and args */
	message(LOG, "Starting pid %d, console %s: '", getpid(), terminal);
	while ( *cmd) message(LOG, "%s ", *cmd++);
	message(LOG, "'\r\n");
	
	/* Now run it.  The new program will take over this PID, 
	 * so nothing further in init.c should be run. */
	execvp(*command, (char**)command+1);

	message(LOG, "Bummer, could not run '%s'\n", command);
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
	message(CONSOLE, "Sorry, your computer does not have enough memory.\r\n");
	while (1) sleep(1);
}

static void shutdown_system(void)
{
    const char* const swap_off_cmd[] = { "swapoff", "swapoff", "-a", 0};
    const char* const umount_cmd[] = { "umount", "umount", "-a", "-n", 0};

#ifndef DEBUG_INIT
    /* Allow Ctrl-Alt-Del to reboot system. */
    reboot(RB_ENABLE_CAD);
#endif
    message(CONSOLE, "\r\nThe system is going down NOW !!\r\n");
    sync();
    /* Send signals to every process _except_ pid 1 */
    message(CONSOLE, "Sending SIGHUP to all processes.\r\n");
#ifndef DEBUG_INIT
    kill(-1, SIGHUP);
#endif
    sleep(2);
    sync();
    message(CONSOLE, "Sending SIGKILL to all processes.\r\n");
#ifndef DEBUG_INIT
    kill(-1, SIGKILL);
#endif
    sleep(1);
    message(CONSOLE, "Disabling swap.\r\n");
    waitfor(run( swap_off_cmd, console, FALSE));
    message(CONSOLE, "Unmounting filesystems.\r\n");
    waitfor(run( umount_cmd, console, FALSE));
    sync();
    if (kernel_version > 0 && kernel_version <= 2 * 65536 + 2 * 256 + 11) {
	/* bdflush, kupdate not needed for kernels >2.2.11 */
	message(CONSOLE, "Flushing buffers.\r\n");
	bdflush(1, 0);
	sync();
    }
}

static void halt_signal(int sig)
{
    shutdown_system();
    message(CONSOLE,
	    "The system is halted. Press CTRL-ALT-DEL or turn off power\r\n");
    sync();
#ifndef DEBUG_INIT
    reboot(RB_HALT_SYSTEM);
    //reboot(RB_POWER_OFF);
#endif
    exit(0);
}

static void reboot_signal(int sig)
{
    shutdown_system();
    message(CONSOLE, "Please stand by while rebooting the system.\r\n");
    sync();
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
    const char* const rc_script_command[] = { INITSCRIPT, INITSCRIPT, 0};
    const char* const shell_command[] = { SHELL, "-" SHELL, 0};
    const char* const* tty0_command = shell_command;
    const char* const* tty1_command = shell_command;
#ifdef BB_INIT_CMD_IF_RC_SCRIPT_EXITS
    const char* const rc_exit_command[] = { "BB_INIT_CMD_IF_RC_SCRIPT_EXITS", 
					    "BB_INIT_CMD_IF_RC_SCRIPT_EXITS", 0 };
#endif

#ifdef DEBUG_INIT
    char *hello_msg_format =
	"init(%d) started:  BusyBox v%s (%s) multi-call binary\r\n";
#else
    char *hello_msg_format =
	"init started:  BusyBox v%s (%s) multi-call binary\r\n";
#endif

    
#ifndef DEBUG_INIT
    /* Expect to be PID 1 iff we are run as init (not linuxrc) */
    if (getpid() != 1 && strstr(argv[0], "init")!=NULL ) {
	usage( "init\n\nInit is the parent of all processes.\n\n"
		"This version of init is designed to be run only by the kernel\n");
    }
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
    message(CONSOLE|LOG, hello_msg_format, BB_VER, BB_BT);
#else
    message(CONSOLE|LOG, hello_msg_format, getpid(), BB_VER, BB_BT);
#endif

    
    /* Mount /proc */
    if (mount ("proc", "/proc", "proc", 0, 0) == 0) {
	message(CONSOLE|LOG, "Mounting /proc: done.\n");
	kernel_version = get_kernel_revision();
    } else
	message(CONSOLE|LOG, "Mounting /proc: failed!\n");

    /* Make sure there is enough memory to do something useful. */
    check_memory();


    /* Make sure an init script exists before trying to run it */
    if (run_rc == TRUE && stat(INITSCRIPT, &statbuf)==0) {
	wait_for_enter = FALSE;
	tty0_command = rc_script_command;
    }


    /* Ok, now launch the rc script and/or prepare to 
     * start up some VTs if somebody hits enter... 
     */
    for (;;) {
	pid_t wpid;
	int status;

	if (pid1 == 0 && tty0_command) {
	    pid1 = run(tty0_command, console, wait_for_enter);
	}
	if (pid2 == 0 && tty1_command && second_console) {
	    pid2 = run(tty1_command, second_console, TRUE);
	}
	wpid = wait(&status);
	if (wpid > 0 ) {
	    message(LOG, "pid %d exited, status=%x.\n", wpid, status);
	}
	/* Don't respawn init script if it exits */
	if (wpid == pid1) {
	    if (run_rc == FALSE) {
		pid1 = 0;
	    }
#ifdef BB_INIT_CMD_IF_RC_SCRIPT_EXITS
	    else {
		pid1 = 0;
		run_rc=FALSE;
		wait_for_enter=TRUE;
		tty0_command=rc_exit_command;
	    }
#endif
	}
	if (wpid == pid2) {
	    pid2 = 0;
	}
	sleep(1);
    }
}
