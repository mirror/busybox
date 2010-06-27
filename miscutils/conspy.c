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
 */

//applet:IF_CONSPY(APPLET(conspy, _BB_DIR_BIN, _BB_SUID_DROP))

//kbuild:lib-$(CONFIG_CONSPY) += conspy.o

//config:config CONSPY
//config:	bool "conspy"
//config:	default n
//config:	help
//config:	  A text-mode VNC like program for Linux virtual terminals.
//config:	  example:  conspy NUM      shared access to console num
//config:	  or        conspy -nd NUM  screenshot of console num
//config:	  or        conspy -cs NUM  poor man's GNU screen like

//usage:#define conspy_trivial_usage
//usage:	"[-vcsndf] [-x COL] [-y LINE] [CONSOLE_NO]"
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
//usage:     "\n	-x COL	Starting column"
//usage:     "\n	-y LINE	Starting line"

#include "libbb.h"
#include <sys/kd.h>

struct screen_info {
	unsigned char lines, cols, cursor_x, cursor_y;
};

#define CHAR(x) ((uint8_t)((x)[0]))
#define ATTR(x) ((uint8_t)((x)[1]))
#define NEXT(x) ((x)+=2)
#define DATA(x) (*(uint16_t*)(x))

struct globals {
	char* data;
	int size;
	int x, y;
	int kbd_fd;
	unsigned width;
	unsigned height;
	unsigned col;
	unsigned line;
	int ioerror_count;
	int key_count;
	int escape_count;
	int nokeys;
	int current;
	int vcsa_fd;
	uint16_t last_attr;
	uint8_t last_bold;
	uint8_t last_blink;
	uint8_t last_fg;
	uint8_t last_bg;
	char attrbuf[sizeof("\033[0;1;5;30;40m")];
	smallint curoff;
	struct screen_info remote;
	struct termios term_orig;
};

#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	strcpy((char*)&G.last_attr, "\xff\xff\xff\xff\xff\xff" "\033["); \
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
	for (i = 0; i < G.remote.lines; i++) {
		for (j = 0; j < G.remote.cols; j++, NEXT(data)) {
			unsigned x = j - G.x; // if will catch j < G.x too
			unsigned y = i - G.y; // if will catch i < G.y too

			if (CHAR(data) < ' ')
				*data = ' '; // CHAR(data) = ' ';
			if (y >= G.height || x >= G.width)
				DATA(data) = 0;
		}
	}
	close(G.vcsa_fd);
}

static void screen_char(char *data)
{
	uint8_t attr = ATTR(data);

	if (!BW && G.last_attr != attr) {
// Attribute layout for VGA compatible text videobuffer:
// blinking text
// |red bkgd
// ||green bkgd
// |||blue bkgd
// vvvv
// 00000000 <- lsb bit on the right
//     bold text / text 8th bit
//      red text
//       green text
//        blue text
// TODO: apparently framebuffer-based console uses different layout
// (bug? attempt to get 8th text bit in better position?)
// red bkgd
// |green bkgd
// ||blue bkgd
// vvv
// 00000000 <- lsb bit on the right
//    bold text
//     red text
//      green text
//       blue text
//        text 8th bit
		// converting RGB color bit triad to BGR:
		static const char color[8] = "04261537";
		char *ptr;
		uint8_t fg, bold, bg, blink;

		G.last_attr = attr;

		//attr >>= 1; // for framebuffer console
		ptr = G.attrbuf + sizeof("\033[")-1;
		fg    = (attr & 0x07);
		bold  = (attr & 0x08);
		bg    = (attr & 0x70);
		blink = (attr & 0x80);
		if (G.last_bold > bold || G.last_blink > blink) {
			G.last_bold = G.last_blink = 0;
			G.last_bg = 0xff;
			*ptr++ = '0';
			*ptr++ = ';';
		}
		if (G.last_bold != bold) {
			G.last_bold = bold;
			*ptr++ = '1';
			*ptr++ = ';';
		}
		if (G.last_blink != blink) {
			G.last_blink = blink;
			*ptr++ = '5';
			*ptr++ = ';';
		}
		if (G.last_fg != fg) {
			G.last_fg = fg;
			*ptr++ = '3';
			*ptr++ = color[fg];
			*ptr++ = ';';
		}
		if (G.last_bg != bg) {
			G.last_bg = bg;
			*ptr++ = '4';
			*ptr++ = color[bg >> 4];
			*ptr++ = ';';
		}
		if (ptr != G.attrbuf + sizeof("\033[")-1) {
			ptr[-1] = 'm';
			*ptr = '\0';
			fputs(G.attrbuf, stdout);
		}
	}
	putchar(CHAR(data));
	G.col++;
}

static void clrscr(void)
{
	// Home, clear till end of src, cursor on
	fputs("\033[1;1H" "\033[J" "\033[?25h", stdout);
	G.curoff = G.col = G.line = 0;
}

static void curoff(void)
{
	if (!G.curoff) {
		G.curoff = 1;
		fputs("\033[?25l", stdout);
	}
}

static void curon(void)
{
	if (G.curoff) {
		G.curoff = 0;
		fputs("\033[?25h", stdout);
	}
}

static void gotoxy(int col, int line)
{
	if (G.col != col || G.line != line) {
		G.col = col;
		G.line = line;
		printf("\033[%u;%uH", line + 1, col + 1);
	}
}

static void screen_dump(void)
{
	int linefeed_cnt;
	int line, col;
	int linecnt = G.remote.lines - G.y;
	char *data = G.data + G.current + (2 * G.y * G.remote.cols);

	linefeed_cnt = 0;
	for (line = 0; line < linecnt && line < G.height; line++) {
		int space_cnt = 0;
		for (col = 0; col < G.remote.cols; col++, NEXT(data)) {
			unsigned tty_col = col - G.x; // if will catch col < G.x too

			if (tty_col >= G.width)
				continue;
			space_cnt++;
			if (BW && CHAR(data) == ' ')
				continue;
			while (linefeed_cnt != 0) {
				//bb_putchar('\r'); - tty driver does it for us
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
	unsigned cx = G.remote.cursor_x - G.x;
	unsigned cy = G.remote.cursor_y - G.y;

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
		close(G.kbd_fd);
	}
	// Reset attributes
	if (!BW)
		fputs("\033[0m", stdout);
	bb_putchar('\n');
	if (code > 1)
		kill_myself_with_sig(code); // does not return
	exit(code);
}

static void get_initial_data(const char* vcsa_name)
{
	G.vcsa_fd = xopen(vcsa_name, O_RDONLY);
	xread(G.vcsa_fd, &G.remote, 4);
	G.size = G.remote.cols * G.remote.lines * 2;
	G.width = G.height = UINT_MAX;
	G.data = xzalloc(2 * G.size);
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

int conspy_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int conspy_main(int argc UNUSED_PARAM, char **argv)
{
	char vcsa_name[sizeof("/dev/vcsaNN")];
	char tty_name[sizeof("/dev/ttyNN")];
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
	strcpy(vcsa_name, "/dev/vcsa");

	opt_complementary = "x+:y+"; // numeric params
	opts = getopt32(argv, "vcsndfx:y:", &G.x, &G.y);
	argv += optind;
	ttynum = 0;
	if (argv[0]) {
		ttynum = xatou_range(argv[0], 0, 63);
		sprintf(vcsa_name + sizeof("/dev/vcsa")-1, "%u", ttynum);
	}
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
	if (opts & FLAG(d)) {
		screen_dump();
		bb_putchar('\n');
		return 0;
	}

	bb_signals(BB_FATAL_SIGS, cleanup);

	// All characters must be passed through to us unaltered
	G.kbd_fd = xopen(CURRENT_TTY, O_RDONLY);
	tcgetattr(G.kbd_fd, &G.term_orig);
	termbuf = G.term_orig;
	termbuf.c_iflag &= ~(BRKINT|INLCR|ICRNL|IXON|IXOFF|IUCLC|IXANY|IMAXBEL);
	//termbuf.c_oflag &= ~(OPOST); - no, we still want \n -> \r\n
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

		// Close & re-open vcsa in case they have
		// swapped virtual consoles
		G.vcsa_fd = xopen(vcsa_name, O_RDONLY);
		xread(G.vcsa_fd, &G.remote, 4);
		if (G.size != (G.remote.cols * G.remote.lines * 2)) {
			cleanup(1);
		}
		i = G.width;
		j = G.height;
		get_terminal_width_height(G.kbd_fd, &G.width, &G.height);
		if ((option_mask32 & FLAG(f))) {
			int nx = G.remote.cursor_x - G.width + 1;
			int ny = G.remote.cursor_y - G.height + 1;

			if (G.remote.cursor_x < G.x) {
				G.x = G.remote.cursor_x;
				i = 0;	// force refresh
			}
			if (nx > G.x) {
				G.x = nx;
				i = 0;	// force refresh
			}
			if (G.remote.cursor_y < G.y) {
				G.y = G.remote.cursor_y;
				i = 0;	// force refresh
			}
			if (ny > G.y) {
				G.y = ny;
				i = 0;	// force refresh
			}
		}

		// Scan console data and redraw our tty where needed
		old = G.data + G.current;
		G.current = G.size - G.current;
		data = G.data + G.current;
		screen_read_close();
		if (i != G.width || j != G.height) {
			clrscr();
			screen_dump();
		} else {
			// For each remote line
			old += G.y * G.remote.cols * 2;
			data += G.y * G.remote.cols * 2;
			for (i = G.y; i < G.remote.lines; i++) {
				char *first = NULL; // first char which needs updating
				char *last = last;  // last char which needs updating
				unsigned iy = i - G.y;

				if (iy >= G.height)
					break;
				old += G.x * 2;
				data += G.x * 2;
				for (j = G.x; j < G.remote.cols; j++, NEXT(old), NEXT(data)) {
					unsigned jx = j - G.x;

					if (jx < G.width && DATA(data) != DATA(old)) {
						last = data;
						if (!first) {
							first = data;
							gotoxy(jx, iy);
						}
					}
				}
				if (first) {
					// Rewrite updated data on the local screen
					for (; first <= last; NEXT(first))
						screen_char(first);
				}
			}
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
				if (k[i] != '\033')
					G.escape_count = 0;
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
