/*
 * Copyright (C) 2017 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config HEXEDIT
//config:	bool "hexedit"
//config:	default y
//config:	help
//config:	Edit file in hexadecimal.

//applet:IF_HEXEDIT(APPLET(hexedit, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_HEXEDIT) += hexedit.o

#include "libbb.h"

#define ESC	"\033"
#define HOME	ESC"[H"
#define CLEAR	ESC"[H"ESC"[J"

struct globals {
	smallint half;
	int fd;
	unsigned height;
	uint8_t *addr;
	uint8_t *current_byte;
	uint8_t *eof_byte;
	off_t size;
	off_t offset;
	struct termios orig_termios;
};
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

/* Hopefully there aren't arches with PAGE_SIZE > 64k */
#define G_mapsize (64*1024)

/* "12ef5670 (xx )*16 _1_3_5_7_9abcdef\n"NUL */
#define LINEBUF_SIZE (8 + 1 + 3*16 + 16 + 1 + 1 /*paranoia:*/ + 13)

static int format_line(char *hex, uint8_t *data, off_t offset)
{
	int ofs_pos;
	char *text;
	uint8_t *end, *end1;

#if 1
	/* Can be more than 4Gb, thus >8 chars, thus use a variable - don't assume 8! */
	ofs_pos = sprintf(hex, "%08"OFF_FMT"x ", offset);
#else
	if (offset <= 0xffff)
		ofs_pos = sprintf(hex, "%04"OFF_FMT"x ", offset);
	else
		ofs_pos = sprintf(hex, "%08"OFF_FMT"x ", offset);
#endif
	hex += ofs_pos;

	text = hex + 16*3;
	end1 = data + 15;
	if ((G.size - offset) > 0) {
		end = end1;
		if ((G.size - offset) <= 15)
			end = data + (G.size - offset) - 1;
		while (data <= end) {
			uint8_t c = *data++;
			*hex++ = bb_hexdigits_upcase[c >> 4];
			*hex++ = bb_hexdigits_upcase[c & 0xf];
			*hex++ = ' ';
			if (c < ' ' || c > 0x7e)
				c = '.';
			*text++ = c;
		}
	}
	while (data <= end1) {
		*hex++ = ' ';
		*hex++ = ' ';
		*hex++ = ' ';
		*text++ = ' ';
		data++;
	}
	*text = '\0';

	return ofs_pos;
}

static void redraw(void)
{
	uint8_t *data;
	off_t offset;
	unsigned i;

	data = G.addr;
	offset = 0;
	i = 0;
	while (i < G.height) {
		char buf[LINEBUF_SIZE];
		format_line(buf, data, offset);
		printf(
			"\r\n%s" + (!i)*2, /* print \r\n only on 2nd line and later */
			buf
		);
		data += 16;
		offset += 16;
		i++;
	}
}

static void redraw_cur_line(void)
{
	char buf[LINEBUF_SIZE];
	uint8_t *data;
	off_t offset;
	int column;

	column = (0xf & (uintptr_t)G.current_byte);
	data = G.current_byte - column;
	offset = G.offset + (data - G.addr);

	column = column*3 + G.half;
	column += format_line(buf, data, offset);
	printf("%s"
		"\r"
		"%.*s",
		buf + column,
		column, buf
	);
}

static void remap(unsigned cur_pos)
{
	if (G.addr)
		munmap(G.addr, G_mapsize);

	G.addr = mmap(NULL,
		G_mapsize,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		G.fd,
		G.offset
	);
	if (G.addr == MAP_FAILED)
//TODO: restore termios?
		bb_perror_msg_and_die("mmap");

	G.current_byte = G.addr + cur_pos;

	G.eof_byte = G.addr + G_mapsize;
	if ((G.size - G.offset) < G_mapsize) {
		/* mapping covers tail of the file */
		/* we do have a mapped byte which is past eof */
		G.eof_byte = G.addr + (G.size - G.offset);
	}
}
static void move_mapping_further(void)
{
	unsigned pos;
	unsigned pagesize;

	if ((G.size - G.offset) < G_mapsize)
		return; /* can't move mapping even further, it's at the end already */

	pagesize = getpagesize(); /* constant on most arches */
	pos = G.current_byte - G.addr;
	if (pos >= pagesize) {
		/* move offset up until current position is in 1st page */
		do {
			G.offset += pagesize;
			if (G.offset == 0) { /* whoops */
				G.offset -= pagesize;
				break;
			}
			pos -= pagesize;
		} while (pos >= pagesize);
		remap(pos);
	}
}
static void move_mapping_lower(void)
{
	unsigned pos;
	unsigned pagesize;

	if (G.offset == 0)
		return; /* we are at 0 already */

	pagesize = getpagesize(); /* constant on most arches */
	pos = G.current_byte - G.addr;

	/* move offset down until current position is in last page */
	pos += pagesize;
	while (pos < G_mapsize) {
		pos += pagesize;
		G.offset -= pagesize;
		if (G.offset == 0)
			break;
	}
	pos -= pagesize;

	remap(pos);
}

static void sig_catcher(int sig)
{
	tcsetattr_stdin_TCSANOW(&G.orig_termios);
	kill_myself_with_sig(sig);
}

//usage:#define hexedit_trivial_usage
//usage:	"FILE"
//usage:#define hexedit_full_usage "\n\n"
//usage:	"Edit FILE in hexadecimal"
int hexedit_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hexedit_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned row = 0;

	INIT_G();

	get_terminal_width_height(-1, NULL, &G.height);
	if (1) {
		/* reduce number of write() syscalls while PgUp/Down: fully buffered output */
		unsigned sz = (G.height | 0xf) * LINEBUF_SIZE;
		setvbuf(stdout, xmalloc(sz), _IOFBF, sz);
	}

	getopt32(argv, "^" "" "\0" "=1"/*one arg*/);
	argv += optind;

	G.fd = xopen(*argv, O_RDWR);
	G.size = xlseek(G.fd, 0, SEEK_END);

	/* TERMIOS_RAW_CRNL suppresses \n -> \r\n translation, helps with down-arrow */
	set_termios_to_raw(STDIN_FILENO, &G.orig_termios, TERMIOS_RAW_CRNL);
	bb_signals(BB_FATAL_SIGS, sig_catcher);

	remap(0);

	printf(CLEAR);
	redraw();
	printf(ESC"[1;10H"); /* position on 1st hex byte in first line */

//TODO: //Home/End: start/end of line; '<'/'>': start/end of file
	//Backspace: undo
	//Enter: goto specified position
	//Ctrl-L: redraw
	//Ctrl-X: save and exit (maybe also Q?)
	//Ctrl-Z: suspend
	//'/', Ctrl-S: search
//TODO: go to end-of-screen on exit (for this, sighandler should interrupt read_key())
//TODO: detect window resize
//TODO: read-only mode if open(O_RDWR) fails? hide cursor in this case?

	for (;;) {
		char read_key_buffer[KEYCODE_BUFFER_SIZE];
		unsigned cnt;
		int32_t key;
		uint8_t byte;

		fflush_all();
		key = read_key(STDIN_FILENO, read_key_buffer, -1);

		cnt = 1;
		switch (key) {
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			/* lowercase, then convert to '0'+10...15 */
			key = (key | 0x20) - ('a' - '0' - 10);
			/* fall through */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (G.current_byte == G.eof_byte) {
				move_mapping_further();
				if (G.current_byte == G.eof_byte) {
					/* extend the file */
					if (++G.size <= 0 /* overflow? */
					 || ftruncate(G.fd, G.size) != 0 /* error extending? (e.g. block dev) */
					) {
						G.size--;
						break;
					}
					G.eof_byte++;
				}
			}
			key -= '0';
			byte = *G.current_byte & 0xf0;
			if (!G.half) {
				byte = *G.current_byte & 0x0f;
				key <<= 4;
			}
			*G.current_byte = byte + key;
			/* can't just print one updated hex char: need to update right-hand ASCII too */
			redraw_cur_line();
			/* fall through */
		case KEYCODE_RIGHT:
			if (G.current_byte == G.eof_byte)
				break; /* eof - don't allow going past it */
			byte = *G.current_byte;
			if (!G.half) {
				G.half = 1;
				putchar(bb_hexdigits_upcase[byte >> 4]);
			} else {
				G.half = 0;
				G.current_byte++;
				if ((0xf & (uintptr_t)G.current_byte) == 0) {
					/* rightmost pos, wrap to next line */
					if (G.current_byte == G.eof_byte)
						move_mapping_further();
					printf(ESC"[46D"); /* cursor left 3*15 + 1 chars */
					goto down;
				}
				putchar(bb_hexdigits_upcase[byte & 0xf]);
				putchar(' ');
			}
			break;
		case KEYCODE_PAGEDOWN:
			cnt = G.height;
		case KEYCODE_DOWN:
 k_down:
			G.current_byte += 16;
			if (G.current_byte >= G.eof_byte) {
				move_mapping_further();
				if (G.current_byte > G.eof_byte) {
					/* eof - don't allow going past it */
					G.current_byte -= 16;
					break;
				}
			}
 down:
			putchar('\n'); /* down one line, possibly scroll screen */
			row++;
			if (row >= G.height) {
				row--;
				redraw_cur_line();
			}
			if (--cnt)
				goto k_down;
			break;

		case KEYCODE_LEFT:
			if (G.half) {
				G.half = 0;
				printf(ESC"[D");
				break;
			}
			if ((0xf & (uintptr_t)G.current_byte) == 0) {
				/* leftmost pos, wrap to prev line */
				if (G.current_byte == G.addr) {
					move_mapping_lower();
					if (G.current_byte == G.addr)
						break; /* first line, don't do anything */
				}
				G.half = 1;
				G.current_byte--;
				printf(ESC"[46C"); /* cursor right 3*15 + 1 chars */
				goto up;
			}
			G.half = 1;
			G.current_byte--;
			printf(ESC"[2D");
			break;
		case KEYCODE_PAGEUP:
			cnt = G.height;
		case KEYCODE_UP:
 k_up:
			if ((G.current_byte - G.addr) < 16) {
				move_mapping_lower();
				if ((G.current_byte - G.addr) < 16)
					break;
			}
			G.current_byte -= 16;
 up:
			if (row != 0) {
				row--;
				printf(ESC"[A"); /* up (won't scroll) */
			} else {
				//printf(ESC"[T"); /* scroll up */ - not implemented on Linux VT!
				printf(ESC"M"); /* scroll up */
				redraw_cur_line();
			}
			if (--cnt)
				goto k_up;
			break;
		}
	}

	return EXIT_SUCCESS;
}
