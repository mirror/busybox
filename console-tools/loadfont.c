/* vi: set sw=4 ts=4: */
/*
 * loadfont.c - Eugene Crosser & Andries Brouwer
 *
 * Version 0.96bb
 *
 * Loads the console font, and possibly the corresponding screen map(s).
 * (Adapted for busybox by Matej Vela.)
 */
#include "internal.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <endian.h>

#define PSF_MAGIC1	0x36
#define PSF_MAGIC2	0x04

#define PSF_MODE512    0x01
#define PSF_MODEHASTAB 0x02
#define PSF_MAXMODE    0x03
#define PSF_SEPARATOR  0xFFFF

struct psf_header {
	unsigned char magic1, magic2;	/* Magic number */
	unsigned char mode;			/* PSF font mode */
	unsigned char charsize;		/* Character size */
};

#define PSF_MAGIC_OK(x)	((x).magic1 == PSF_MAGIC1 && (x).magic2 == PSF_MAGIC2)

static void loadnewfont(int fd);

extern int loadfont_main(int argc, char **argv)
{
	int fd;

	if (argc>=2 && *argv[1]=='-') {
		usage(loadfont_usage);
	}

	fd = open("/dev/tty0", O_RDWR);
	if (fd < 0) {
		errorMsg("Error opening /dev/tty0: %s\n", strerror(errno));
		return( FALSE);
	}
	loadnewfont(fd);

	return( TRUE);
}

static void do_loadfont(int fd, char *inbuf, int unit, int fontsize)
{
	char buf[16384];
	int i;

	memset(buf, 0, sizeof(buf));

	if (unit < 1 || unit > 32) {
		errorMsg("Bad character size %d\n", unit);
		exit(1);
	}

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
		perror("PIO_FONTX ioctl error (trying PIO_FONT)");
	}
#endif
	if (ioctl(fd, PIO_FONT, buf)) {
		perror("PIO_FONT ioctl error");
		exit(1);
	}
}

static void
do_loadtable(int fd, unsigned char *inbuf, int tailsz, int fontsize)
{
	struct unimapinit advice;
	struct unimapdesc ud;
	struct unipair *up;
	int ct = 0, maxct;
	int glyph;
	u_short unicode;

	maxct = tailsz;				/* more than enough */
	up = (struct unipair *) malloc(maxct * sizeof(struct unipair));

	if (!up) {
		errorMsg("Out of memory?\n");
		exit(1);
	}
	for (glyph = 0; glyph < fontsize; glyph++) {
		while (tailsz >= 2) {
			unicode = (((u_short) inbuf[1]) << 8) + inbuf[0];
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
			errorMsg("It seems this kernel is older than 1.1.92\n");
			errorMsg("No Unicode mapping table loaded.\n");
		} else
#endif
			perror("PIO_UNIMAPCLR");
		exit(1);
	}
	ud.entry_ct = ct;
	ud.entries = up;
	if (ioctl(fd, PIO_UNIMAP, &ud)) {
#if 0
		if (errno == ENOMEM) {
			/* change advice parameters */
		}
#endif
		perror("PIO_UNIMAP");
		exit(1);
	}
}

static void loadnewfont(int fd)
{
	int unit;
	char inbuf[32768];			/* primitive */
	unsigned int inputlth, offset;

	/*
	 * We used to look at the length of the input file
	 * with stat(); now that we accept compressed files,
	 * just read the entire file.
	 */
	inputlth = fread(inbuf, 1, sizeof(inbuf), stdin);
	if (ferror(stdin)) {
		perror("Error reading input font");
		exit(1);
	}
	/* use malloc/realloc in case of giant files;
	   maybe these do not occur: 16kB for the font,
	   and 16kB for the map leaves 32 unicode values
	   for each font position */
	if (!feof(stdin)) {
		perror("Font too large");
		exit(1);
	}

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

		if (psfhdr.mode > PSF_MAXMODE) {
			errorMsg("Unsupported psf file mode\n");
			exit(1);
		}
		fontsize = ((psfhdr.mode & PSF_MODE512) ? 512 : 256);
#if !defined( PIO_FONTX ) || defined( __sparc__ )
		if (fontsize != 256) {
			errorMsg("Only fontsize 256 supported\n");
			exit(1);
		}
#endif
		hastable = (psfhdr.mode & PSF_MODEHASTAB);
		unit = psfhdr.charsize;
		head0 = sizeof(struct psf_header);

		head = head0 + fontsize * unit;
		if (head > inputlth || (!hastable && head != inputlth)) {
			errorMsg("Input file: bad length\n");
			exit(1);
		}
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
		if (inputlth & 0377) {
			errorMsg("Bad input file size\n");
			exit(1);
		}
		offset = 0;
		unit = inputlth / 256;
	}
	do_loadfont(fd, inbuf + offset, unit, 256);
}
