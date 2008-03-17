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

#define PSF_MAGIC_OK(x)	((x).magic1 == PSF_MAGIC1 && (x).magic2 == PSF_MAGIC2)

static void do_loadfont(int fd, unsigned char *inbuf, int unit, int fontsize)
{
	char *buf;
	int i;

	if (unit < 1 || unit > 32)
		bb_error_msg_and_die("bad character size %d", unit);

	buf = xzalloc(16 * 1024);
	/*memset(buf, 0, 16 * 1024);*/
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

static void
do_loadtable(int fd, unsigned char *inbuf, int tailsz, int fontsize)
{
	struct unimapinit advice;
	struct unimapdesc ud;
	struct unipair *up;
	int ct = 0, maxct;
	int glyph;
	uint16_t unicode;

	maxct = tailsz;				/* more than enough */
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

static void loadnewfont(int fd)
{
	enum { INBUF_SIZE = 32*1024 + 1 };

	int unit;
	unsigned inputlth, offset;
	/* Was on stack, but 32k is a bit too much: */
	unsigned char *inbuf = xmalloc(INBUF_SIZE);

	/*
	 * We used to look at the length of the input file
	 * with stat(); now that we accept compressed files,
	 * just read the entire file.
	 */
	inputlth = full_read(STDIN_FILENO, inbuf, INBUF_SIZE);
	if (inputlth < 0)
		bb_perror_msg_and_die("error reading input font");
	if (inputlth >= INBUF_SIZE)
		bb_error_msg_and_die("font too large");

	/* test for psf first */
	{
		struct psf_header psfhdr;
		int fontsize;
		int hastable;
		unsigned head0, head;

		if (inputlth < sizeof(struct psf_header))
			goto no_psf;

		psfhdr = *(struct psf_header *) &inbuf[0];

		if (!PSF_MAGIC_OK(psfhdr))
			goto no_psf;

		if (psfhdr.mode > PSF_MAXMODE)
			bb_error_msg_and_die("unsupported psf file mode");
		fontsize = ((psfhdr.mode & PSF_MODE512) ? 512 : 256);
#if !defined(PIO_FONTX) || defined(__sparc__)
		if (fontsize != 256)
			bb_error_msg_and_die("only fontsize 256 supported");
#endif
		hastable = (psfhdr.mode & PSF_MODEHASTAB);
		unit = psfhdr.charsize;
		head0 = sizeof(struct psf_header);

		head = head0 + fontsize * unit;
		if (head > inputlth || (!hastable && head != inputlth))
			bb_error_msg_and_die("input file: bad length");
		do_loadfont(fd, inbuf + head0, unit, fontsize);
		if (hastable)
			do_loadtable(fd, inbuf + head, inputlth - head, fontsize);
		return;
	}

 no_psf:
	/* file with three code pages? */
	if (inputlth == 9780) {
		offset = 40;
		unit = 16;
	} else {
		/* bare font */
		if (inputlth & 0377)
			bb_error_msg_and_die("bad input file size");
		offset = 0;
		unit = inputlth / 256;
	}
	do_loadfont(fd, inbuf + offset, unit, 256);
}

int loadfont_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int loadfont_main(int argc, char **argv ATTRIBUTE_UNUSED)
{
	int fd;

	if (argc != 1)
		bb_show_usage();

	fd = xopen(CURRENT_VC, O_RDWR);
	loadnewfont(fd);

	return EXIT_SUCCESS;
}
