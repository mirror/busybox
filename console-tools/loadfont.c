/* vi: set sw=4 ts=4: */
/*
 * loadfont.c - Eugene Crosser & Andries Brouwer
 *
 * Version 0.96bb
 *
 * Loads the console font, and possibly the corresponding screen map(s).
 * (Adapted for busybox by Matej Vela.)
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <sys/kd.h>

#ifndef KDFONTOP
#define KDFONTOP 0x4B72
struct console_font_op {
	unsigned op;            /* KD_FONT_OP_* */
	unsigned flags;         /* KD_FONT_FLAG_* */
	unsigned width, height;
	unsigned charcount;
	unsigned char *data;    /* font data with height fixed to 32 */
};

#define KD_FONT_OP_SET          0  /* Set font */
#define KD_FONT_OP_GET          1  /* Get font */
#define KD_FONT_OP_SET_DEFAULT  2  /* Set font to default,
                                         data points to name / NULL */
#define KD_FONT_OP_COPY         3  /* Copy from another console */

#define KD_FONT_FLAG_OLD        0x80000000 /* Invoked via old interface */
#define KD_FONT_FLAG_DONT_RECALC 1 /* Don't call adjust_height() */
                                   /* (Used internally for PIO_FONT support) */
#endif /* KDFONTOP */


enum {
	PSF_MAGIC1 = 0x36,
	PSF_MAGIC2 = 0x04,

	PSF_MODE512 = 0x01,
	PSF_MODEHASTAB = 0x02,
	PSF_MAXMODE = 0x03,
	PSF_SEPARATOR = 0xffff
};

struct psf_header {
	unsigned char magic1, magic2;   /* Magic number */
	unsigned char mode;             /* PSF font mode */
	unsigned char charsize;         /* Character size */
};

#define PSF_MAGIC_OK(x)	((x)->magic1 == PSF_MAGIC1 && (x)->magic2 == PSF_MAGIC2)

static void do_loadfont(int fd, unsigned char *inbuf, int unit, int fontsize)
{
	char *buf;
	int i;

	if (unit < 1 || unit > 32)
		bb_error_msg_and_die("bad character size %d", unit);

	buf = xzalloc(16 * 1024);
	for (i = 0; i < fontsize; i++)
		memcpy(buf + (32 * i), inbuf + (unit * i), unit);

	{ /* KDFONTOP */
		struct console_font_op cfo;

		cfo.op = KD_FONT_OP_SET;
		cfo.flags = 0;
		cfo.width = 8;
		cfo.height = unit;
		cfo.charcount = fontsize;
		cfo.data = (void*)buf;
#if 0
		if (!ioctl_or_perror(fd, KDFONTOP, &cfo, "KDFONTOP ioctl failed (will try PIO_FONTX)"))
			goto ret;  /* success */
#else
		xioctl(fd, KDFONTOP, &cfo);
#endif
	}

#if 0
/* These ones do not honour -C tty (they set font on current tty regardless)
 * On x86, this distinction is visible on framebuffer consoles
 * (regular character consoles may have only one shared font anyway)
 */
#if defined(PIO_FONTX) && !defined(__sparc__)
	{
		struct consolefontdesc cfd;

		cfd.charcount = fontsize;
		cfd.charheight = unit;
		cfd.chardata = buf;

		if (!ioctl_or_perror(fd, PIO_FONTX, &cfd, "PIO_FONTX ioctl failed (will try PIO_FONT)"))
			goto ret;  /* success */
	}
#endif
	xioctl(fd, PIO_FONT, buf);
 ret:
#endif /* 0 */
	free(buf);
}

static void do_loadtable(int fd, unsigned char *inbuf, int tailsz, int fontsize)
{
	struct unimapinit advice;
	struct unimapdesc ud;
	struct unipair *up;
	int ct = 0, maxct;
	int glyph;
	uint16_t unicode;

	maxct = tailsz;	/* more than enough */
	up = xmalloc(maxct * sizeof(struct unipair));

	for (glyph = 0; glyph < fontsize; glyph++) {
		while (tailsz >= 2) {
			unicode = (((uint16_t) inbuf[1]) << 8) + inbuf[0];
			tailsz -= 2;
			inbuf += 2;
			if (unicode == PSF_SEPARATOR)
				break;
			up[ct].unicode = unicode;
			up[ct].fontpos = glyph;
			ct++;
		}
	}

	/* Note: after PIO_UNIMAPCLR and before PIO_UNIMAP
	   this printf did not work on many kernels */

	advice.advised_hashsize = 0;
	advice.advised_hashstep = 0;
	advice.advised_hashlevel = 0;
	xioctl(fd, PIO_UNIMAPCLR, &advice);
	ud.entry_ct = ct;
	ud.entries = up;
	xioctl(fd, PIO_UNIMAP, &ud);
}

static void do_load(int fd, struct psf_header *psfhdr, size_t len)
{
	int unit;
	int fontsize;
	int hastable;
	unsigned head0, head = head;

	/* test for psf first */
	if (len >= sizeof(struct psf_header) && PSF_MAGIC_OK(psfhdr)) {
		if (psfhdr->mode > PSF_MAXMODE)
			bb_error_msg_and_die("unsupported psf file mode");
		fontsize = ((psfhdr->mode & PSF_MODE512) ? 512 : 256);
#if !defined(PIO_FONTX) || defined(__sparc__)
		if (fontsize != 256)
			bb_error_msg_and_die("only fontsize 256 supported");
#endif
		hastable = (psfhdr->mode & PSF_MODEHASTAB);
		unit = psfhdr->charsize;
		head0 = sizeof(struct psf_header);

		head = head0 + fontsize * unit;
		if (head > len || (!hastable && head != len))
			bb_error_msg_and_die("input file: bad length");
	} else {
		/* file with three code pages? */
		if (len == 9780) {
			head0 = 40;
			unit = 16;
		} else {
			/* bare font */
			if (len & 0377)
				bb_error_msg_and_die("input file: bad length");
			head0 = 0;
			unit = len / 256;
		}
		fontsize = 256;
		hastable = 0;
	}

	do_loadfont(fd, (unsigned char *)psfhdr + head0, unit, fontsize);
	if (hastable)
		do_loadtable(fd, (unsigned char *)psfhdr + head, len - head, fontsize);
}

#if ENABLE_LOADFONT
int loadfont_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int loadfont_main(int argc UNUSED_PARAM, char **argv)
{
	size_t len;
	struct psf_header *psfhdr;

	// no arguments allowed!
	opt_complementary = "=0";
	getopt32(argv, "");

	/*
	 * We used to look at the length of the input file
	 * with stat(); now that we accept compressed files,
	 * just read the entire file.
	 */
	len = 32*1024; // can't be larger
	psfhdr = xmalloc_read(STDIN_FILENO, &len);
	// xmalloc_open_zipped_read_close(filename, &len);
	if (!psfhdr)
		bb_perror_msg_and_die("error reading input font");
	do_load(get_console_fd_or_die(), psfhdr, len);

	return EXIT_SUCCESS;
}
#endif

#if ENABLE_SETFONT

/*
kbd-1.12:

setfont [-O font+umap.orig] [-o font.orig] [-om cmap.orig]
[-ou umap.orig] [-N] [font.new ...] [-m cmap] [-u umap] [-C console]
[-hNN] [-v] [-V]

-h NN  Override font height
-o file
       Save previous font in file
-O file
       Save previous font and Unicode map in file
-om file
       Store console map in file
-ou file
       Save previous Unicode map in file
-m file
       Load console map or Unicode console map from file
-u file
       Load Unicode table describing the font from file
       Example:
       # cp866
       0x00-0x7f       idem
       #
       0x80    U+0410  # CYRILLIC CAPITAL LETTER A
       0x81    U+0411  # CYRILLIC CAPITAL LETTER BE
       0x82    U+0412  # CYRILLIC CAPITAL LETTER VE
-C console
       Set the font for the indicated console
-v     Verbose
-V     Version
*/

#if ENABLE_FEATURE_SETFONT_TEXTUAL_MAP
static int ctoi(char *s)
{
	if (s[0] == '\'' && s[1] != '\0' && s[2] == '\'' && s[3] == '\0')
		return s[1];
	// U+ means 0x
	if (s[0] == 'U' && s[1] == '+') {
		s[0] = '0';
		s[1] = 'x';
	}
	if (!isdigit(s[0]))
		return -1;
	return xstrtoul(s, 0);
}
#endif

int setfont_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setfont_main(int argc UNUSED_PARAM, char **argv)
{
	size_t len;
	unsigned opts;
	int fd;
	struct psf_header *psfhdr;
	char *mapfilename;
	const char *tty_name = CURRENT_TTY;

	opt_complementary = "=1";
	opts = getopt32(argv, "m:C:", &mapfilename, &tty_name);
	argv += optind;

	fd = xopen_nonblocking(tty_name);

	if (sizeof(CONFIG_DEFAULT_SETFONT_DIR) > 1) { // if not ""
		if (*argv[0] != '/') {
			// goto default fonts location. don't die if doesn't exist
			chdir(CONFIG_DEFAULT_SETFONT_DIR "/consolefonts");
		}
	}
	// load font
	len = 32*1024; // can't be larger
	psfhdr = xmalloc_open_zipped_read_close(*argv, &len);
	if (!psfhdr)
		bb_simple_perror_msg_and_die(*argv);
	do_load(fd, psfhdr, len);

	// load the screen map, if any
	if (opts & 1) { // -m
		unsigned mode = PIO_SCRNMAP;
		void *map;

		if (sizeof(CONFIG_DEFAULT_SETFONT_DIR) > 1) { // if not ""
			if (mapfilename[0] != '/') {
				// goto default keymaps location
				chdir(CONFIG_DEFAULT_SETFONT_DIR "/consoletrans");
			}
		}
		// fetch keymap
		map = xmalloc_open_zipped_read_close(mapfilename, &len);
		if (!map)
			bb_simple_perror_msg_and_die(mapfilename);
		// file size is 256 or 512 bytes? -> assume binary map
		if (len == E_TABSZ || len == 2*E_TABSZ) {
			if (len == 2*E_TABSZ)
				mode = PIO_UNISCRNMAP;
		}
#if ENABLE_FEATURE_SETFONT_TEXTUAL_MAP
		// assume textual Unicode console maps:
		// 0x00 U+0000  #  NULL (NUL)
		// 0x01 U+0001  #  START OF HEADING (SOH)
		// 0x02 U+0002  #  START OF TEXT (STX)
		// 0x03 U+0003  #  END OF TEXT (ETX)
		else {
			int i;
			char *token[2];
			parser_t *parser;

			if (ENABLE_FEATURE_CLEAN_UP)
				free(map);
			map = xmalloc(E_TABSZ * sizeof(unsigned short));

#define unicodes ((unsigned short *)map)
			// fill vanilla map
			for (i = 0; i < E_TABSZ; i++)
				unicodes[i] = 0xf000 + i;

			parser = config_open(mapfilename);
			while (config_read(parser, token, 2, 2, "# \t", PARSE_NORMAL | PARSE_MIN_DIE)) {
				// parse code/value pair
				int a = ctoi(token[0]);
				int b = ctoi(token[1]);
				if (a < 0 || a >= E_TABSZ
				 || b < 0 || b > 65535
				) {
					bb_error_msg_and_die("map format");
				}
				// patch map
				unicodes[a] = b;
				// unicode character is met?
				if (b > 255)
					mode = PIO_UNISCRNMAP;
			}
			if (ENABLE_FEATURE_CLEAN_UP)
				config_close(parser);

			if (mode != PIO_UNISCRNMAP) {
#define asciis ((unsigned char *)map)
				for (i = 0; i < E_TABSZ; i++)
					asciis[i] = unicodes[i];
#undef asciis
			}
#undef unicodes
		}
#endif // ENABLE_FEATURE_SETFONT_TEXTUAL_MAP

		// do set screen map
		xioctl(fd, mode, map);

		if (ENABLE_FEATURE_CLEAN_UP)
			free(map);
	}

	return EXIT_SUCCESS;
}
#endif
