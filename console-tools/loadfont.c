/* vi: set sw=4 ts=4: */
/*
 * loadfont.c - Eugene Crosser & Andries Brouwer
 *
 * Version 0.96bb
 *
 * Loads the console font, and possibly the corresponding screen map(s).
 * (Adapted for busybox by Matej Vela.)
 */
#include "busybox.h"
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
	unsigned char magic1, magic2;	/* Magic number */
	unsigned char mode;			/* PSF font mode */
	unsigned char charsize;		/* Character size */
};

#define PSF_MAGIC_OK(x)	((x).magic1 == PSF_MAGIC1 && (x).magic2 == PSF_MAGIC2)

static void loadnewfont(int fd);

int loadfont_main(int argc, char **argv)
{
	int fd;

	if (argc != 1)
		bb_show_usage();

	fd = xopen(CURRENT_VC, O_RDWR);
	loadnewfont(fd);

	return EXIT_SUCCESS;
}

static void do_loadfont(int fd, unsigned char *inbuf, int unit, int fontsize)
{
	char buf[16384];
	int i;

	memset(buf, 0, sizeof(buf));

	if (unit < 1 || unit > 32)
		bb_error_msg_and_die("bad character size %d", unit);

	for (i = 0; i < fontsize; i++)
		memcpy(buf + (32 * i), inbuf + (unit * i), unit);

#if defined( PIO_FONTX ) && !defined( __sparc__ )
	{
		struct consolefontdesc cfd;

		cfd.charcount = fontsize;
		cfd.charheight = unit;
		cfd.chardata = buf;

		if (ioctl(fd, PIO_FONTX, &cfd) == 0)
			return;				/* success */
		bb_perror_msg("PIO_FONTX ioctl error (trying PIO_FONT)");
	}
#endif
	if (ioctl(fd, PIO_FONT, buf))
		bb_perror_msg_and_die("PIO_FONT ioctl error");
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
	if (ioctl(fd, PIO_UNIMAPCLR, &advice)) {
#ifdef ENOIOCTLCMD
		if (errno == ENOIOCTLCMD) {
			bb_error_msg("it seems this kernel is older than 1.1.92");
			bb_error_msg_and_die("no Unicode mapping table loaded");
		} else
#endif
			bb_perror_msg_and_die("PIO_UNIMAPCLR");
	}
	ud.entry_ct = ct;
	ud.entries = up;
	if (ioctl(fd, PIO_UNIMAP, &ud)) {
		bb_perror_msg_and_die("PIO_UNIMAP");
	}
}

static void loadnewfont(int fd)
{
	int unit;
	unsigned char inbuf[32768];			/* primitive */
	unsigned int inputlth, offset;

	/*
	 * We used to look at the length of the input file
	 * with stat(); now that we accept compressed files,
	 * just read the entire file.
	 */
	inputlth = fread(inbuf, 1, sizeof(inbuf), stdin);
	if (ferror(stdin))
		bb_perror_msg_and_die("error reading input font");
	/* use malloc/realloc in case of giant files;
	   maybe these do not occur: 16kB for the font,
	   and 16kB for the map leaves 32 unicode values
	   for each font position */
	if (!feof(stdin))
		bb_perror_msg_and_die("font too large");

	/* test for psf first */
	{
		struct psf_header psfhdr;
		int fontsize;
		int hastable;
		unsigned int head0, head;

		if (inputlth < sizeof(struct psf_header))
			goto no_psf;

		psfhdr = *(struct psf_header *) &inbuf[0];

		if (!PSF_MAGIC_OK(psfhdr))
			goto no_psf;

		if (psfhdr.mode > PSF_MAXMODE)
			bb_error_msg_and_die("unsupported psf file mode");
		fontsize = ((psfhdr.mode & PSF_MODE512) ? 512 : 256);
#if !defined( PIO_FONTX ) || defined( __sparc__ )
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
