/* vi: set sw=4 ts=4: */
/*
 * A text-mode VNC like program for Linux virtual terminals.
 *
 * pascal.bellard@ads-lu.com
 *
 * Based on Russell Stuart's conspy.c
 *   http://ace-host.stuart.id.au/russell/files/conspy.c
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 *
 * example :	conspy num		shared access to console num
 * or		conspy -d num		screenshot of console num
 * or		conspy -cs num		poor man's GNU screen like
 */

//applet:IF_CONSPY(APPLET(conspy, _BB_DIR_BIN, _BB_SUID_DROP))

//kbuild:lib-$(CONFIG_CONSPY) += conspy.o

//config:config CONSPY
//config:	bool "conspy"
//config:	default n
//config:	help
//config:	  A text-mode VNC like program for Linux virtual terminals.
//config:	  example : conspy num      shared access to console num
//config:	  or        conspy -d num   screenshot of console num
//config:	  or        conspy -cs num  poor man's GNU screen like

//usage:#define conspy_trivial_usage
//usage:     "[-vcsndf] [-x ROW] [-y LINE] [CONSOLE_NO]"
//usage:#define conspy_full_usage "\n\n"
//usage:     "A text-mode VNC like program for Linux virtual consoles."
//usage:     "\nTo exit, quickly press ESC 3 times."
//usage:     "\n"
//usage:     "\nOptions:"
//usage:     "\n	-v	Don't send keystrokes to the console"
//usage:     "\n	-c	Create missing devices in /dev"
//usage:     "\n	-s	Open a SHELL session"
//usage:     "\n	-n	Black & white"
//usage:     "\n	-d	Dump console to stdout"
//usage:     "\n	-f	Follow cursor"
//usage:     "\n	-x ROW	Starting row"
//usage:     "\n	-y LINE	Starting line"

#include "libbb.h"
#include <sys/kd.h>

struct screen_info {
	unsigned char lines, rows, cursor_x, cursor_y;
};

#define CHAR(x) ((x)[0])
#define ATTR(x) ((x)[1])
#define NEXT(x) ((x)+=2)
#define DATA(x) (* (short *) (x))

struct globals {
	char* data;
	int size;
	int x, y;
	int kbd_fd;
	unsigned width;
	unsigned height;
	char last_attr;
	int ioerror_count;
	int key_count;
	int escape_count;
	int nokeys;
	int current;
	int vcsa_fd;
	struct screen_info info;
	struct termios term_orig;
};

#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

enum {
	FLAG_v,  // view only
	FLAG_c,  // create device if need
	FLAG_s,  // session
	FLAG_n,  // no colors
	FLAG_d,  // dump screen
	FLAG_f,  // follow cursor
};
#define FLAG(x) (1 << FLAG_##x)
#define BW (option_mask32 & FLAG(n))

static void screen_read_close(void)
{
	unsigned i, j;
	char *data = G.data + G.current;

	xread(G.vcsa_fd, data, G.size);
	G.last_attr = 0;
	for (i = 0; i < G.info.lines; i++) {
		for (j = 0; j < G.info.rows; j++, NEXT(data)) {
			unsigned x = j - G.x; // if will catch j < G.x too
			unsigned y = i - G.y; // if will catch i < G.y too

			if (CHAR(data) < ' ')
				CHAR(data) = ' ';
			if (y >= G.height || x >= G.width)
				DATA(data) = 0;
		}
	}
	close(G.vcsa_fd);
}

static void screen_char(char *data)
{
	if (!BW && G.last_attr != ATTR(data)) {
		//                            BLGCRMOW
		static const char color[8] = "04261537";

		printf("\033[%c;4%c;3%cm",
			(ATTR(data) & 8) ? '1'  // bold
					 : '0', // defaults
			color[(ATTR(data) >> 4) & 7], color[ATTR(data) & 7]);
		G.last_attr = ATTR(data);
	}
	bb_putchar(CHAR(data));
}

#define clrscr()  printf("\033[1;1H" "\033[0J")
#define curoff()  printf("\033[?25l")

static void curon(void)
{
	printf("\033[?25h");
}

static void gotoxy(int row, int line)
{
	printf("\033[%u;%uH", line + 1, row + 1);
}

static void screen_dump(void)
{
	int linefeed_cnt;
	int line, row;
	int linecnt = G.info.lines - G.y;
	char *data = G.data + G.current + (2 * G.y * G.info.rows);

	linefeed_cnt = 0;
	for (line = 0; line < linecnt && line < G.height; line++) {
		int space_cnt = 0;
		for (row = 0; row < G.info.rows; row++, NEXT(data)) {
			unsigned tty_row = row - G.x; // if will catch row < G.x too

			if (tty_row >= G.width)
				continue;
			space_cnt++;
			if (BW && (CHAR(data) | ' ') == ' ')
				continue;
			while (linefeed_cnt != 0) {
				bb_putchar('\r');
				bb_putchar('\n');
				linefeed_cnt--;
			}
			while (--space_cnt)
				bb_putchar(' ');
			screen_char(data);
		}
		linefeed_cnt++;
	}
}

static void curmove(void)
{
	unsigned cx = G.info.cursor_x - G.x;
	unsigned cy = G.info.cursor_y - G.y;

	if (cx >= G.width || cy >= G.height) {
		curoff();
	} else {
		curon();
		gotoxy(cx, cy);
	}
	fflush_all();
}

static void cleanup(int code)
{
	curon();
	fflush_all();
	tcsetattr(G.kbd_fd, TCSANOW, &G.term_orig);
	if (ENABLE_FEATURE_CLEAN_UP) {
		free(ptr_to_globals);
		close(G.kbd_fd);
	}
	// Reset attributes
	if (!BW)
		printf("\033[0m");
	bb_putchar('\n');
	if (code > 1)
		kill_myself_with_sig(code); // does not return
	exit(code);
}

static void get_initial_data(const char* vcsa_name)
{
	int size;
	G.vcsa_fd = xopen(vcsa_name, O_RDONLY);
	xread(G.vcsa_fd, &G.info, 4);
	G.size = size = G.info.rows * G.info.lines * 2;
	G.width = G.height = UINT_MAX;
	G.data = xzalloc(2 * size);
	screen_read_close();
}

static void create_cdev_if_doesnt_exist(const char* name, dev_t dev)
{
	int fd = open(name, O_RDONLY);
	if (fd != -1)
		close(fd);
	else if (errno == ENOENT)
		mknod(name, S_IFCHR | 0660, dev);
}

static NOINLINE void start_shell_in_child(const char* tty_name)
{
	int pid = vfork();
	if (pid < 0) {
		bb_perror_msg_and_die("vfork");
	}
	if (pid == 0) {
		struct termios termchild;
		char *shell = getenv("SHELL");

		if (!shell)
			shell = (char *) DEFAULT_SHELL;
		signal(SIGHUP, SIG_IGN);
		// set tty as a controlling tty
		setsid();
		// make tty to be input, output, error
		close(0);
		xopen(tty_name, O_RDWR); // uses fd 0
		xdup2(0, 1);
		xdup2(0, 2);
		ioctl(0, TIOCSCTTY, 1);
		tcsetpgrp(0, getpid());
		tcgetattr(0, &termchild);
		termchild.c_lflag |= ECHO;
		termchild.c_oflag |= ONLCR | XTABS;
		termchild.c_iflag |= ICRNL;
		termchild.c_iflag &= ~IXOFF;
		tcsetattr_stdin_TCSANOW(&termchild);
		execl(shell, shell, "-i", (char *) NULL);
		bb_simple_perror_msg_and_die(shell);
	}
}

int conspy_main(int argc UNUSED_PARAM, char **argv) MAIN_EXTERNALLY_VISIBLE;
int conspy_main(int argc UNUSED_PARAM, char **argv)
{
	char vcsa_name[sizeof("/dev/vcsa") + 2];
	char tty_name[sizeof("/dev/tty") + 2];
#define keybuf bb_common_bufsiz1
	struct termios termbuf;
	unsigned opts;
	unsigned ttynum;
	int poll_timeout_ms;
#if ENABLE_LONG_OPTS
	static const char getopt_longopts[] ALIGN1 =
		"viewonly\0"     No_argument "v"
		"createdevice\0" No_argument "c"
		"session\0"      No_argument "s"
		"nocolors\0"     No_argument "n"
		"dump\0"         No_argument "d"
		"follow\0"       No_argument "f"
		;

	applet_long_options = getopt_longopts;
#endif
	INIT_G();

	opt_complementary = "x+:y+"; // numeric params
	opts = getopt32(argv, "vcsndfx:y:", &G.x, &G.y);
	argv += optind;
	ttynum = 0;
	if (argv[0]) {
		ttynum = xatou_range(argv[0], 0, 63);
	}
	sprintf(vcsa_name, "/dev/vcsa%u", ttynum);
	sprintf(tty_name, "%s%u", "/dev/tty", ttynum);
	if (opts & FLAG(c)) {
		if ((opts & (FLAG(s)|FLAG(v))) != FLAG(v))
			create_cdev_if_doesnt_exist(tty_name, makedev(4, ttynum));
		create_cdev_if_doesnt_exist(vcsa_name, makedev(7, 128 + ttynum));
	}
	if ((opts & FLAG(s)) && ttynum) {
		start_shell_in_child(tty_name);
	}

	get_initial_data(vcsa_name);
	G.kbd_fd = xopen(CURRENT_TTY, O_RDONLY);
	if (opts & FLAG(d)) {
		screen_dump();
		bb_putchar('\n');
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(ptr_to_globals);
			close(G.kbd_fd);
		}
		return 0;
	}

	bb_signals(BB_FATAL_SIGS, cleanup);
	// All characters must be passed through to us unaltered
	tcgetattr(G.kbd_fd, &G.term_orig);
	termbuf = G.term_orig;
	termbuf.c_iflag &= ~(BRKINT|INLCR|ICRNL|IXON|IXOFF|IUCLC|IXANY|IMAXBEL);
	termbuf.c_oflag &= ~(OPOST);
	termbuf.c_lflag &= ~(ISIG|ICANON|ECHO);
	termbuf.c_cc[VMIN] = 1;
	termbuf.c_cc[VTIME] = 0;
	tcsetattr(G.kbd_fd, TCSANOW, &termbuf);
	poll_timeout_ms = 250;
	while (1) {
		struct pollfd pfd;
		int bytes_read;
		int i, j;
		char *data, *old;

		old = G.data + G.current;
		G.current = G.size - G.current;
		data = G.data + G.current;
		
		// Close & re-open vcsa in case they have
		// swapped virtual consoles
		G.vcsa_fd = xopen(vcsa_name, O_RDONLY);
		xread(G.vcsa_fd, &G.info, 4);
		if (G.size != (G.info.rows * G.info.lines * 2)) {
			cleanup(1);
		}
		i = G.width;
		j = G.height;
		get_terminal_width_height(G.kbd_fd, &G.width, &G.height);
		if ((option_mask32 & FLAG(f))) {
			int nx = G.info.cursor_x - G.width + 1;
			int ny = G.info.cursor_y - G.height + 1;

			if (G.info.cursor_x < G.x) {
				G.x = G.info.cursor_x;
				i = 0;	// force refresh
			}
			if (nx > G.x) {
				G.x = nx;
				i = 0;	// force refresh
			}
			if (G.info.cursor_y < G.y) {
				G.y = G.info.cursor_y;
				i = 0;	// force refresh
			}
			if (ny > G.y) {
				G.y = ny;
				i = 0;	// force refresh
			}
		}

		// Scan console data and redraw our tty where needed
		screen_read_close();
		if (i != G.width || j != G.height) {
			clrscr();
			screen_dump();
		}
		else for (i = 0; i < G.info.lines; i++) {
			char *last = last;
			char *first = NULL;
			int iy = i - G.y;

			if (iy >= (int) G.height)
				break;
			for (j = 0; j < G.info.rows; j++) {
				last = data;
				if (DATA(data) != DATA(old) && iy >= 0) {
					unsigned jx = j - G.x;

					last = NULL;
					if (first == NULL && jx < G.width) {
						first = data;
						gotoxy(jx, iy);
					}
				}
				NEXT(old);
				NEXT(data);
			}
			if (first == NULL)
				continue;
			if (last == NULL)
				last = data;

			// Write the data to the screen
			for (; first < last; NEXT(first))
				screen_char(first);
		}
		curmove();

		// Wait for local user keypresses
		pfd.fd = G.kbd_fd;
		pfd.events = POLLIN;
		bytes_read = 0;
		switch (poll(&pfd, 1, poll_timeout_ms)) {
			char *k;
		case -1:
			if (errno != EINTR)
				cleanup(1);
			break;
		case 0:
			if (++G.nokeys >= 4)
				G.nokeys = G.escape_count = 0;
			break;
		default:
			// Read the keys pressed
			k = keybuf + G.key_count;
			bytes_read = read(G.kbd_fd, k, sizeof(keybuf) - G.key_count);
			if (bytes_read < 0)
				cleanup(1);

			// Do exit processing
			for (i = 0; i < bytes_read; i++) {
				if (k[i] != '\033') G.escape_count = 0;
				else if (++G.escape_count >= 3)
					cleanup(0);
			}
		}
		poll_timeout_ms = 250;

		// Insert all keys pressed into the virtual console's input
		// buffer.  Don't do this if the virtual console is in scan
		// code mode - giving ASCII characters to a program expecting
		// scan codes will confuse it.
		if (!(option_mask32 & FLAG(v)) && G.escape_count == 0) {
			int handle, result;
			long kbd_mode;

			G.key_count += bytes_read;
			handle = xopen(tty_name, O_WRONLY);
			result = ioctl(handle, KDGKBMODE, &kbd_mode);
			if (result == -1)
				/* nothing */;
			else if (kbd_mode != K_XLATE && kbd_mode != K_UNICODE)
				G.key_count = 0; // scan code mode
			else {
				for (i = 0; i < G.key_count && result != -1; i++)
					result = ioctl(handle, TIOCSTI, keybuf + i);
				G.key_count -= i;
				if (G.key_count)
					memmove(keybuf, keybuf + i, G.key_count);
				// If there is an application on console which reacts
				// to keypresses, we need to make our first sleep
				// shorter to quickly redraw whatever it printed there.
				poll_timeout_ms = 20;
			}
			// Close & re-open tty in case they have
			// swapped virtual consoles
			close(handle);

			// We sometimes get spurious IO errors on the TTY
			// as programs close and re-open it
			if (result != -1)
				G.ioerror_count = 0;
			else if (errno != EIO || ++G.ioerror_count > 4)
				cleanup(1);
		}
	}
}
