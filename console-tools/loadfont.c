/* vi: set sw=4 ts=4: */
/*
 * loadfont.c - Eugene Crosser & Andries Brouwer
 *
 * Version 0.96bb
 *
 * Loads the console font, and possibly the corresponding screen map(s).
 * (Adapted for busybox by Matej Vela.)
 */
#include "libbb.h"
#include <sys/kd.h>

enum {
	PSF_MAGIC1 = 0x36,
	PSF_MAGIC2 = 0x04,

	PSF_MODE512 = 0x01,
	PSF_MODEHASTAB = 0x02,
	PSF_MAXMODE = 0x03,
	PSF_SEPARATOR = 0xFFFF
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

#if defined(PIO_FONTX) && !defined(__sparc__)
	{
		struct consolefontdesc cfd;

		cfd.charcount = fontsize;
		cfd.charheight = unit;
		cfd.chardata = buf;

		if (!ioctl_or_perror(fd, PIO_FONTX, &cfd, "PIO_FONTX ioctl failed (will try PIO_FONT)"))
			goto ret;			/* success */
	}
#endif
	xioctl(fd, PIO_FONT, buf);
 ret:
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
	psfhdr = (struct psf_header *) xmalloc_read(STDIN_FILENO, &len);
	// xmalloc_open_zipped_read_close(filename, &len);
	if (!psfhdr)
		bb_perror_msg_and_die("error reading input font");
	do_load(get_console_fd_or_die(), psfhdr, len);

	return EXIT_SUCCESS;
}
#endif

#if ENABLE_SETFONT
int setfont_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setfont_main(int argc UNUSED_PARAM, char **argv)
{
	size_t len;
	struct psf_header *psfhdr;
	char *mapfilename;
	int fd;

	opt_complementary = "=1";
	getopt32(argv, "m:", &mapfilename);
	argv += optind;

	// load font
	len = 32*1024; // can't be larger
	psfhdr = (struct psf_header *) xmalloc_open_zipped_read_close(*argv, &len);
	fd = get_console_fd_or_die();
	do_load(fd, psfhdr, len);

	// load the screen map, if any
	if (option_mask32 & 1) { // -m
		void *map = xmalloc_open_zipped_read_close(mapfilename, &len);
		if (len == E_TABSZ || len == 2*E_TABSZ) {
			xioctl(fd, (len == 2*E_TABSZ) ? PIO_UNISCRNMAP : PIO_SCRNMAP, map);
		}
	}

	return EXIT_SUCCESS;
}
#endif
