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
#include <linux/serial.h>      /* for serial_struct */
#include <sys/vt.h>            /* for vt_stat */
#include <sys/ioctl.h>

static const char  init_usage[] = "Used internally by the system.";
static char        console[16] = "";
static const char* default_console = "/dev/tty2";
static char*       first_terminal = NULL;
static const char* second_terminal = "/dev/tty2";
static const char* log = "/dev/tty3";
static char* term_ptr = NULL;

static void
message(const char* terminal, const char * pattern, ...)
{
	int	fd;
	FILE *	con = 0;
	va_list	arguments;

	/*
	 * Open the console device each time a message is printed. If the user
	 * has switched consoles, the open will get the new console. If we kept
	 * the console open, we'd always print to the same one.
	 */
	if ( !terminal
	 ||  ((fd = open(terminal, O_WRONLY|O_NOCTTY)) < 0)
	 ||  ((con = fdopen(fd, "w")) == NULL) )
		return;

	va_start(arguments, pattern);
	vfprintf(con, pattern, arguments);
	va_end(arguments);
	fclose(con);
}

static int
waitfor(int pid)
{
	int	status;
	int	wpid;
	
	message(log, "Waiting for process %d.\n", pid);
	while ( (wpid = wait(&status)) != pid ) {
		if ( wpid > 0 ) {
			message(
			 log
			,"pid %d exited, status=%x.\n"
			,wpid
			,status);
		}
	}
	return wpid;
}

static int
run(const char* program, const char* const* arguments, 
	const char* terminal, int get_enter)
{
	static const char	control_characters[] = {
		'\003',
		'\034',
		'\177',
		'\025',
		'\004',
		'\0',
		'\1',
		'\0',
		'\021',
		'\023',
		'\032',
		'\0',
		'\022',
		'\017',
		'\027',
		'\026',
		'\0'
	};

	static char * environment[] = {
		"HOME=/",
		"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
		"SHELL=/bin/sh",
		0,
		"USER=root",
		0
	};

	static const char	press_enter[] =
 "\nPlease press Enter to activate this console. ";

	int	pid;

	environment[3]=term_ptr;

	pid = fork();
	if ( pid == 0 ) {
		struct termios	t;
		const char * const * arg;

		close(0);
		close(1);
		close(2);
		setsid();

		open(terminal, O_RDWR);
		dup(0);
		dup(0);
		tcsetpgrp(0, getpgrp());

		tcgetattr(0, &t);
		memcpy(t.c_cc, control_characters, sizeof(control_characters));
		t.c_line = 0;
		t.c_iflag = ICRNL|IXON|IXOFF;
		t.c_oflag = OPOST|ONLCR;
		t.c_lflag = ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE|IEXTEN;
		tcsetattr(0, TCSANOW, &t);

		if ( get_enter ) {
			/*
			 * Save memory by not exec-ing anything large (like a shell)
			 * before the user wants it. This is critical if swap is not
			 * enabled and the system has low memory. Generally this will
			 * be run on the second virtual console, and the first will
			 * be allowed to start a shell or whatever an init script 
			 * specifies.
			 */
			char	c;
			write(1, press_enter, sizeof(press_enter) - 1);
			read(0, &c, 1);
		}
		
		message(log, "Executing ");
		arg = arguments;
		while ( *arg != 0 )
			message(log, "%s ", *arg++);
		message(log, "\n");

		execve(program, (char * *)arguments, (char * *)environment);
		message(log, "%s: could not execute: %s.\r\n", program, strerror(errno));
		exit(-1);
	}
	return pid;
}

static int
mem_total()
{
    char s[80];
    char *p;
    FILE *f;
    const char pattern[]="MemTotal:";

    f=fopen("/proc/meminfo","r");
    while (NULL != fgets(s,79,f)) {
	p=strstr(s, pattern);
	if (NULL != p) {
	    fclose(f);
	    return(atoi(p+strlen(pattern)));
	}
    }
    return -1;
}

static void
set_free_pages()
{
    char s[80];
    FILE *f;

    f=fopen("/proc/sys/vm/freepages","r");
    fgets(s,79,f);
    if (atoi(s) < 32) {
	fclose(f);
	f=fopen("/proc/sys/vm/freepages","w");
	fprintf(f,"30\t40\t50\n");
	printf("\nIncreased /proc/sys/vm/freepages values to 30/40/50\n");
    }
    fclose(f);
}

static void
shutdown_system(void)
{
	static const char * const umount_args[] = {"/bin/umount", "-a", "-n", 0};
	static const char * const swapoff_args[] = {"/bin/swapoff", "-a", 0};

	message(console, "The system is going down NOW !!");
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
	waitfor(run("/bin/swapoff", swapoff_args, console, 0));
	waitfor(run("/bin/umount", umount_args, console, 0));
	sync();
	if (get_kernel_revision() <= 2*65536+2*256+11) {
	    /* Removed  bdflush call, kupdate in kernels >2.2.11 */
	    bdflush(1, 0);
	    sync();
	}
}

static void
halt_signal(int sig)
{
    shutdown_system();
    message(console, "The system is halted. Press CTRL-ALT-DEL or turn off power\r\n");
    reboot( RB_HALT_SYSTEM);
    exit(0);
}

static void
reboot_signal(int sig)
{
    shutdown_system();
    message(console, "Please stand by while rebooting the system.\r\n");
    reboot( RB_AUTOBOOT);
    exit(0);
}

static void
configure_terminals( int serial_cons, int single_user_mode )
{
	char *tty;
	struct serial_struct sr;
	struct vt_stat vt;


	switch (serial_cons) {
	case -1:
	    /* 2.2 kernels:
	    * identify the real console backend and try to make use of it */
	    if (ioctl(0,TIOCGSERIAL,&sr) == 0) {
		sprintf( console, "/dev/ttyS%d", sr.line );
		serial_cons = sr.line+1;
	    }
	    else if (ioctl(0, VT_GETSTATE, &vt) == 0) {
		sprintf( console, "/dev/tty%d", vt.v_active );
		serial_cons = 0;
	    }
	    else {
		/* unknown backend: fallback to /dev/console */
		strcpy( console, "/dev/console" );
		serial_cons = 0;
	    }
	    break;

	case 1:
		strcpy( console, "/dev/cua0" );
		break;
	case 2:
		strcpy( console, "/dev/cua1" );
		break;
	default:
		tty = ttyname(0);
		if (tty) {
			strcpy( console, tty );
			if (!strncmp( tty, "/dev/cua", 8 ))
				serial_cons=1;
		}
		else
			/* falls back to /dev/tty1 if an error occurs */
			strcpy( console, default_console );
	}
	if (!first_terminal)
		first_terminal = console;
#if defined (__sparc__)
	if (serial_cons > 0 && !strncmp(term_ptr,"TERM=linux",10))
	    term_ptr = "TERM=vt100";
#endif
	if (serial_cons) {
	    /* disable other but the first terminal:
	    * VT is not initialized anymore on 2.2 kernel when booting from
	    * serial console, therefore modprobe is flooding the display with
	    * "can't locate module char-major-4" messages. */
	    log = 0;
	    second_terminal = 0;
	}
}

extern int
init_main(int argc, char * * argv)
{
	const char *		    rc_arguments[100];
	const char *		    arguments[100];
	int			    run_rc = TRUE;
	int			    j;
	int			    pid1 = 0;
	int			    pid2 = 0;
	struct stat		    statbuf;
	const char *		    tty_commands[3] = { "etc/init.d/rcS", "bin/sh"};
	int			    serial_console = 0;
	int retval;

	/*
	 * If I am started as /linuxrc instead of /sbin/init, I don't have the
	 * environment that init expects. I can't fix the signal behavior. Try
	 * to divorce from the controlling terminal with setsid(). This won't work
	 * if I am the process group leader.
	 */
	setsid();

	signal(SIGUSR1, halt_signal);
	signal(SIGUSR2, reboot_signal);
	signal(SIGINT, reboot_signal);
	signal(SIGTERM, reboot_signal);

	reboot(RB_DISABLE_CAD);

	message(log, "%s: started. ", argv[0]);

	for ( j = 1; j < argc; j++ ) {
		if ( strcmp(argv[j], "single") == 0 ) {
			run_rc = FALSE;
			tty_commands[0] = "bin/sh";
			tty_commands[1] = 0;
		}
	}
	for ( j = 0; __environ[j] != 0; j++ ) {
		if ( strncmp(__environ[j], "tty", 3) == 0
		 && __environ[j][3] >= '1'
		 && __environ[j][3] <= '2'
		 && __environ[j][4] == '=' ) {
			const char * s = &__environ[j][5];

			if ( *s == 0 || strcmp(s, "off") == 0 )
				s = 0;

			tty_commands[__environ[j][3] - '1'] = s;
		}
		/* Should catch the syntax of Sparc kernel console setting.   */
		/* The kernel does not recognize a serial console when getting*/
		/* console=/dev/ttySX !! */
		else if ( strcmp(__environ[j], "console=ttya") == 0 ) {
			serial_console=1;
		}
		else if ( strcmp(__environ[j], "console=ttyb") == 0 ) {
			serial_console=2;
		}
		/* standard console settings */
		else if ( strncmp(__environ[j], "console=", 8) == 0 ) {
			first_terminal=&(__environ[j][8]);
		}
		else if ( strncmp(__environ[j], "TERM=", 5) == 0) {
			term_ptr=__environ[j];
		}
	}

	printf("mounting /proc ...\n");
	if (mount("/proc","/proc","proc",0,0)) {
	  perror("mounting /proc failed\n");
	}
	printf("\tdone.\n");

	if (get_kernel_revision() >= 2*65536+1*256+71) {
	    /* if >= 2.1.71 kernel, /dev/console is not a symlink anymore:
	    * use it as primary console */
	    serial_console=-1;
	}

	/* Make sure /etc/init.d/rc exists */
	retval= stat(tty_commands[0],&statbuf);
	if (retval)
	    run_rc = FALSE;

	configure_terminals( serial_console,  run_rc);

	set_free_pages();

	/* not enough memory to do anything useful*/
	if (mem_total() < 2000) { 
	    retval= stat("/etc/fstab",&statbuf);
	    if (retval) {
		printf("You do not have enough RAM, sorry.\n");
		while (1) { sleep(1);}
	    } else { 
	      /* Try to turn on swap */
		static const char * const swapon_args[] = {"/bin/swapon", "-a", 0};
		waitfor(run("/bin/swapon", swapon_args, console, 0));
		if (mem_total() < 2000) { 
		    printf("You do not have enough RAM, sorry.\n");
		    while (1) { sleep(1);}
		}
	    }
	}

	/*
	 * Don't modify **argv directly, it would show up in the "ps" display.
	 * I don't want "init" to look like "rc".
	 */
	rc_arguments[0] = tty_commands[0];
	for ( j = 1; j < argc; j++ ) {
		rc_arguments[j] = argv[j];
	}
	rc_arguments[j] = 0;

	arguments[0] = "-sh";
	arguments[1] = 0;
	
	/* Ok, now launch the rc script /etc/init.d/rcS and prepare to start up
	 * some VTs on tty1 and tty2 if somebody hits enter 
	 */
	for ( ; ; ) {
		int	wpid;
		int	status;

		if ( pid1 == 0  && tty_commands[0] ) {
		   if ( run_rc == TRUE ) {
			pid1 = run(tty_commands[0], rc_arguments, first_terminal, 0);
		   } else {
			pid2 = run(tty_commands[1], arguments, first_terminal, 1);
		   }
		}
		if ( pid2 == 0 && second_terminal && tty_commands[1] ) {
			pid2 = run(tty_commands[1], arguments, second_terminal, 1);
		}
		wpid = wait(&status);
		if ( wpid > 0  && wpid != pid1) {
			message(log, "pid %d exited, status=%x.\n", wpid, status);
		}
		if ( wpid == pid2 ) {
			pid2 = 0;
		}
	}
}

