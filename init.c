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
#include <sys/swap.h>
#include <sys/sysmacros.h>

const char		init_usage[] = "Used internally by the system.";
char			console[16] = "";
const char *	default_console = "/dev/tty1";
char *			first_terminal = NULL;
const char *	second_terminal = "/dev/tty2";
const char		log[] = "/dev/tty3";
char * term_ptr = NULL;

static void
message(const char * terminal, const char * pattern, ...)
{
	int	fd;
	FILE *	con = 0;
	va_list	arguments;

	/*
	 * Open the console device each time a message is printed. If the user
	 * has switched consoles, the open will get the new console. If we kept
	 * the console open, we'd always print to the same one.
	 */
	if ( ((fd = open(terminal, O_WRONLY|O_NOCTTY)) < 0)
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
run(
 const char *			program
,const char * const *	arguments
,const char *			terminal
,int					get_enter)
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
 "Please press Enter to activate this console. ";

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
			 * be allowed to start a shell or the installation system.
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
shutdown_system(int do_reboot)
{
	static const char * const umount_args[] = {"/bin/umount", "-a", "-n", 0};

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
	waitfor(run("/bin/umount", umount_args, console, 0));
	sync();
	bdflush(1, 0);
	sync();
	reboot(do_reboot ?RB_AUTOBOOT : RB_HALT_SYSTEM);
	exit(0);
}

static void
halt_signal(int sig)
{
	shutdown_system(0);
}

static void
reboot_signal(int sig)
{
	shutdown_system(1);
}

static void
exit_signal(int sig)
{
  
  /* initrd doesn't work anyway */

  shutdown_system(1);

	/* This is used on the initial ramdisk */

  /*	message(log, "Init exiting.");
	exit(0);
	*/
}

void
configure_terminals( int serial_cons );

extern int
init_main(struct FileInfo * i, int argc, char * * argv)
{
	static const char * const	rc = "etc/rc";
	const char *				arguments[100];
	int							run_rc = 1;
	int							j;
	int							pid1 = 0;
	int							pid2 = 0;
	int							create_swap= -1;
	struct stat					statbuf;
#ifndef INCLUDE_DINSTALL
	const char *				tty_commands[2] = { "bin/sh", "bin/sh"};
#else
	const char *				tty_commands[2] = { "sbin/dinstall", "bin/sh"};
#endif
	char						swap[20];
	int							serial_console = 0;

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
	signal(SIGTERM, exit_signal);

	reboot(RB_DISABLE_CAD);

	message(log, "%s: started. ", argv[0]);

	for ( j = 1; j < argc; j++ ) {
		if ( strcmp(argv[j], "single") == 0 ) {
			run_rc = 0;
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
	configure_terminals( serial_console );

	printf("mounting /proc ...\n");
	if (mount("/proc","/proc","proc",0,0)) {
	  perror("mounting /proc failed\n");
	}
	printf("\tdone.\n");

	set_free_pages();

	if (mem_total() < 3500) { /* not enough memory for standard install */
	  int retval;
	  retval= stat("/etc/swappartition",&statbuf);
	  if (retval) {
	    printf("
You do not have enough RAM, hence you must boot using the Boot Disk
for Low Memory systems.

Read the instructions in the install.html file.
");
	    while (1) {;}
	  } else { /* everything OK */
	    FILE *f;

	    f=fopen("/etc/swappartition","r");
	    fgets(swap,19,f);
	    fclose(f);
	    *(strstr(swap,"\n"))='\0';

	    if (swapon(swap,0)) {
	      perror("swapon failed\n");
	    } else {
	      f=fopen("/etc/swaps","w");
	      fprintf(f,"%s none swap rw 0 0",swap);
		fclose(f);
	      create_swap = 0;
	    }
	  }
	}

	/*
	 * Don't modify **argv directly, it would show up in the "ps" display.
	 * I don't want "init" to look like "rc".
	 */
	arguments[0] = rc;
	for ( j = 1; j < argc; j++ ) {
		arguments[j] = argv[j];
	}
	arguments[j] = 0;

	if ( run_rc )
		waitfor(run(rc, arguments, console, 0));

	if ( 0 == create_swap) {
	  if (unlink("/etc/swappartition")) {
	    perror("unlinking /etc/swappartition");
	  }
	}

	arguments[0] = "-sh";
	arguments[1] = 0;
	for ( ; ; ) {
		int	wpid;
		int	status;

		if ( pid1 == 0 && tty_commands[0] ) {
			/* Ask before starting a shell */
			/*
			 arguments[0] = tty_commands[0];
			 */
			pid1 = run(tty_commands[0], arguments, first_terminal, 0);
		}
		if ( pid2 == 0 && tty_commands[1] )
			pid2 = run(tty_commands[1], arguments, second_terminal, 1);
		wpid = wait(&status);
		if ( wpid > 0 ) {
			/* DEBUGGING */
			message(log, "pid %d exited, status=%x.\n", wpid, status);
		}
		if ( wpid == pid1 ) {
		  pid1 = 0;
		}
		if ( wpid == pid2 )
			pid2 = 0;
	}
}

void
configure_terminals( int serial_cons )
{
	//struct stat statbuf;
	char *tty;

	switch (serial_cons) {
	case 1:
		strcpy( console, "/dev/ttyS0" );
		break;
	case 2:
		strcpy( console, "/dev/ttyS1" );
		break;
	default:
		tty = ttyname(0);
		if (tty) {
			strcpy( console, tty );
			if (!strncmp( tty, "/dev/ttyS", 9 ))
				serial_cons=1;
		}
		else
			/* falls back to /dev/tty1 if an error occurs */
			strcpy( console, default_console );
	}
	if (!first_terminal)
		first_terminal = console;
	if (serial_cons && !strncmp(term_ptr,"TERM=linux",10))
		term_ptr = "TERM=vt100";
}
