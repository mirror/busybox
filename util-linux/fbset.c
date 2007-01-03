/* vi: set sw=4 ts=4: */
/*
 * Mini fbset implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * This is a from-scratch implementation of fbset; but the de facto fbset
 * implementation was a good reference. fbset (original) is released under
 * the GPL, and is (c) 1995-1999 by:
 *     Geert Uytterhoeven (Geert.Uytterhoeven@cs.kuleuven.ac.be)
 */

#include "busybox.h"

#define DEFAULTFBDEV  FB_0
#define DEFAULTFBMODE "/etc/fb.modes"

enum {
	OPT_CHANGE   = (1 << 0),
	OPT_INFO     = (1 << 1),
	OPT_READMODE = (1 << 2),
	OPT_ALL      = (1 << 9),

	CMD_FB = 1,
	CMD_DB = 2,
	CMD_GEOMETRY = 3,
	CMD_TIMING = 4,
	CMD_ACCEL = 5,
	CMD_HSYNC = 6,
	CMD_VSYNC = 7,
	CMD_LACED = 8,
	CMD_DOUBLE = 9,
/*	CMD_XCOMPAT =     10, */
	CMD_ALL = 11,
	CMD_INFO = 12,
	CMD_CHANGE = 13,

#ifdef CONFIG_FEATURE_FBSET_FANCY
	CMD_XRES = 100,
	CMD_YRES = 101,
	CMD_VXRES = 102,
	CMD_VYRES = 103,
	CMD_DEPTH = 104,
	CMD_MATCH = 105,
	CMD_PIXCLOCK = 106,
	CMD_LEFT = 107,
	CMD_RIGHT = 108,
	CMD_UPPER = 109,
	CMD_LOWER = 110,
	CMD_HSLEN = 111,
	CMD_VSLEN = 112,
	CMD_CSYNC = 113,
	CMD_GSYNC = 114,
	CMD_EXTSYNC = 115,
	CMD_BCAST = 116,
	CMD_RGBA = 117,
	CMD_STEP = 118,
	CMD_MOVE = 119,
#endif
};

static unsigned g_options;

/* Stuff stolen from the kernel's fb.h */
#define FB_ACTIVATE_ALL 64
enum {
	FBIOGET_VSCREENINFO = 0x4600,
	FBIOPUT_VSCREENINFO = 0x4601
};
struct fb_bitfield {
	uint32_t offset;                /* beginning of bitfield */
	uint32_t length;		/* length of bitfield */
	uint32_t msb_right;             /* !=0: Most significant bit is right */
};
struct fb_var_screeninfo {
	uint32_t xres;                  /* visible resolution */
	uint32_t yres;
	uint32_t xres_virtual;          /* virtual resolution */
	uint32_t yres_virtual;
	uint32_t xoffset;               /* offset from virtual to visible */
	uint32_t yoffset;               /* resolution */

	uint32_t bits_per_pixel;
	uint32_t grayscale;             /* !=0 Graylevels instead of colors */

	struct fb_bitfield red;         /* bitfield in fb mem if true color, */
	struct fb_bitfield green;       /* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;      /* transparency */

	uint32_t nonstd;                /* !=0 Non standard pixel format */

	uint32_t activate;              /* see FB_ACTIVATE_x */

	uint32_t height;                /* height of picture in mm */
	uint32_t width;                 /* width of picture in mm */

	uint32_t accel_flags;		/* acceleration flags (hints)	*/

	/* Timing: All values in pixclocks, except pixclock (of course) */
	uint32_t pixclock;              /* pixel clock in ps (pico seconds) */
	uint32_t left_margin;           /* time from sync to picture */
	uint32_t right_margin;          /* time from picture to sync */
	uint32_t upper_margin;          /* time from sync to picture */
	uint32_t lower_margin;
	uint32_t hsync_len;             /* length of horizontal sync */
	uint32_t vsync_len;             /* length of vertical sync */
	uint32_t sync;                  /* see FB_SYNC_x */
	uint32_t vmode;                 /* see FB_VMODE_x */
	uint32_t reserved[6];           /* Reserved for future compatibility */
};


static const struct cmdoptions_t {
	const char name[10];
	const unsigned char param_count;
	const unsigned char code;
} g_cmdoptions[] = {
	{ "-fb", 1, CMD_FB },
	{ "-db", 1, CMD_DB },
	{ "-a", 0, CMD_ALL },
	{ "-i", 0, CMD_INFO },
	{ "-g", 5, CMD_GEOMETRY },
	{ "-t", 7, CMD_TIMING },
	{ "-accel", 1, CMD_ACCEL },
	{ "-hsync", 1, CMD_HSYNC },
	{ "-vsync", 1, CMD_VSYNC },
	{ "-laced", 1, CMD_LACED },
	{ "-double", 1, CMD_DOUBLE },
	{ "-n", 0, CMD_CHANGE },
#ifdef CONFIG_FEATURE_FBSET_FANCY
	{ "-all", 0, CMD_ALL },
	{ "-xres", 1, CMD_XRES },
	{ "-yres", 1, CMD_YRES },
	{ "-vxres", 1, CMD_VXRES },
	{ "-vyres", 1, CMD_VYRES },
	{ "-depth", 1, CMD_DEPTH },
	{ "-match", 0, CMD_MATCH },
	{ "-geometry", 5, CMD_GEOMETRY },
	{ "-pixclock", 1, CMD_PIXCLOCK },
	{ "-left", 1, CMD_LEFT },
	{ "-right", 1, CMD_RIGHT },
	{ "-upper", 1, CMD_UPPER },
	{ "-lower", 1, CMD_LOWER },
	{ "-hslen", 1, CMD_HSLEN },
	{ "-vslen", 1, CMD_VSLEN },
	{ "-timings", 7, CMD_TIMING },
	{ "-csync", 1, CMD_CSYNC },
	{ "-gsync", 1, CMD_GSYNC },
	{ "-extsync", 1, CMD_EXTSYNC },
	{ "-bcast", 1, CMD_BCAST },
	{ "-rgba", 1, CMD_RGBA },
	{ "-step", 1, CMD_STEP },
	{ "-move", 1, CMD_MOVE },
#endif
	{ "", 0, 0 }
};

#ifdef CONFIG_FEATURE_FBSET_READMODE
/* taken from linux/fb.h */
enum {
	FB_VMODE_INTERLACED = 1,	/* interlaced	*/
	FB_VMODE_DOUBLE = 2,	/* double scan */
	FB_SYNC_HOR_HIGH_ACT = 1,	/* horizontal sync high active	*/
	FB_SYNC_VERT_HIGH_ACT = 2,	/* vertical sync high active	*/
	FB_SYNC_EXT = 4,	/* external sync		*/
	FB_SYNC_COMP_HIGH_ACT = 8	/* composite sync high active   */
};
#endif

static int readmode(struct fb_var_screeninfo *base, const char *fn,
					const char *mode)
{
#ifdef CONFIG_FEATURE_FBSET_READMODE
	FILE *f;
	char buf[256];
	char *p = buf;

	f = xfopen(fn, "r");
	while (!feof(f)) {
		fgets(buf, sizeof(buf), f);
		if (!(p = strstr(buf, "mode ")) && !(p = strstr(buf, "mode\t")))
			continue;
		p += 5;
		if (!(p = strstr(buf, mode)))
			continue;
		p += strlen(mode);
		if (!isspace(*p) && (*p != 0) && (*p != '"')
				&& (*p != '\r') && (*p != '\n'))
			continue;	/* almost, but not quite */

		while (!feof(f)) {
			fgets(buf, sizeof(buf), f);
			if ((p = strstr(buf, "geometry "))) {
				p += 9;
				/* FIXME: catastrophic on arches with 64bit ints */
				sscanf(p, "%d %d %d %d %d",
					&(base->xres), &(base->yres),
					&(base->xres_virtual), &(base->yres_virtual),
					&(base->bits_per_pixel));
			} else if ((p = strstr(buf, "timings "))) {
				p += 8;
				sscanf(p, "%d %d %d %d %d %d %d",
					&(base->pixclock),
					&(base->left_margin), &(base->right_margin),
					&(base->upper_margin), &(base->lower_margin),
					&(base->hsync_len), &(base->vsync_len));
			} else if ((p = strstr(buf, "laced "))) {
				//p += 6;
				if (strstr(buf, "false")) {
					base->vmode &= ~FB_VMODE_INTERLACED;
				} else {
					base->vmode |= FB_VMODE_INTERLACED;
				}
			} else if ((p = strstr(buf, "double "))) {
				//p += 7;
				if (strstr(buf, "false")) {
					base->vmode &= ~FB_VMODE_DOUBLE;
				} else {
					base->vmode |= FB_VMODE_DOUBLE;
				}
			} else if ((p = strstr(buf, "vsync "))) {
				//p += 6;
				if (strstr(buf, "low")) {
					base->sync &= ~FB_SYNC_VERT_HIGH_ACT;
				} else {
					base->sync |= FB_SYNC_VERT_HIGH_ACT;
				}
			} else if ((p = strstr(buf, "hsync "))) {
				//p += 6;
				if (strstr(buf, "low")) {
					base->sync &= ~FB_SYNC_HOR_HIGH_ACT;
				} else {
					base->sync |= FB_SYNC_HOR_HIGH_ACT;
				}
			} else if ((p = strstr(buf, "csync "))) {
				//p += 6;
				if (strstr(buf, "low")) {
					base->sync &= ~FB_SYNC_COMP_HIGH_ACT;
				} else {
					base->sync |= FB_SYNC_COMP_HIGH_ACT;
				}
			} else if ((p = strstr(buf, "extsync "))) {
				//p += 8;
				if (strstr(buf, "false")) {
					base->sync &= ~FB_SYNC_EXT;
				} else {
					base->sync |= FB_SYNC_EXT;
				}
			}

			if (strstr(buf, "endmode"))
				return 1;
		}
	}
#else
	bb_error_msg("mode reading not compiled in");
#endif
	return 0;
}

static inline void setmode(struct fb_var_screeninfo *base,
					struct fb_var_screeninfo *set)
{
	if ((int) set->xres > 0)
		base->xres = set->xres;
	if ((int) set->yres > 0)
		base->yres = set->yres;
	if ((int) set->xres_virtual > 0)
		base->xres_virtual = set->xres_virtual;
	if ((int) set->yres_virtual > 0)
		base->yres_virtual = set->yres_virtual;
	if ((int) set->bits_per_pixel > 0)
		base->bits_per_pixel = set->bits_per_pixel;
}

static inline void showmode(struct fb_var_screeninfo *v)
{
	double drate = 0, hrate = 0, vrate = 0;

	if (v->pixclock) {
		drate = 1e12 / v->pixclock;
		hrate = drate / (v->left_margin + v->xres + v->right_margin + v->hsync_len);
		vrate = hrate / (v->upper_margin + v->yres + v->lower_margin + v->vsync_len);
	}
	printf("\nmode \"%ux%u-%u\"\n"
#ifdef CONFIG_FEATURE_FBSET_FANCY
	"\t# D: %.3f MHz, H: %.3f kHz, V: %.3f Hz\n"
#endif
	"\tgeometry %u %u %u %u %u\n"
	"\ttimings %u %u %u %u %u %u %u\n"
	"\taccel %s\n"
	"\trgba %u/%u,%u/%u,%u/%u,%u/%u\n"
	"endmode\n\n",
		v->xres, v->yres, (int) (vrate + 0.5),
#ifdef CONFIG_FEATURE_FBSET_FANCY
		drate / 1e6, hrate / 1e3, vrate,
#endif
		v->xres, v->yres, v->xres_virtual, v->yres_virtual, v->bits_per_pixel,
		v->pixclock, v->left_margin, v->right_margin, v->upper_margin, v->lower_margin,
			v->hsync_len, v->vsync_len,
		(v->accel_flags > 0 ? "true" : "false"),
		v->red.length, v->red.offset, v->green.length, v->green.offset,
			v->blue.length, v->blue.offset, v->transp.length, v->transp.offset);
}

#ifdef STANDALONE
int main(int argc, char **argv)
#else
int fbset_main(int argc, char **argv)
#endif
{
	struct fb_var_screeninfo var, varset;
	int fh, i;
	char *fbdev = DEFAULTFBDEV;
	char *modefile = DEFAULTFBMODE;
	char *thisarg, *mode = NULL;

	memset(&varset, 0xFF, sizeof(varset));

	/* parse cmd args.... why do they have to make things so difficult? */
	argv++;
	argc--;
	for (; argc > 0 && (thisarg = *argv); argc--, argv++) {
		for (i = 0; g_cmdoptions[i].name[0]; i++) {
			if (strcmp(thisarg, g_cmdoptions[i].name))
				continue;
			if (argc-1 < g_cmdoptions[i].param_count)
				bb_show_usage();

			switch (g_cmdoptions[i].code) {
			case CMD_FB:
				fbdev = argv[1];
				break;
			case CMD_DB:
				modefile = argv[1];
				break;
			case CMD_GEOMETRY:
				varset.xres = xatou32(argv[1]);
				varset.yres = xatou32(argv[2]);
				varset.xres_virtual = xatou32(argv[3]);
				varset.yres_virtual = xatou32(argv[4]);
				varset.bits_per_pixel = xatou32(argv[5]);
				break;
			case CMD_TIMING:
				varset.pixclock = xatou32(argv[1]);
				varset.left_margin = xatou32(argv[2]);
				varset.right_margin = xatou32(argv[3]);
				varset.upper_margin = xatou32(argv[4]);
				varset.lower_margin = xatou32(argv[5]);
				varset.hsync_len = xatou32(argv[6]);
				varset.vsync_len = xatou32(argv[7]);
				break;
			case CMD_ALL:
				g_options |= OPT_ALL;
				break;
			case CMD_CHANGE:
				g_options |= OPT_CHANGE;
				break;
#ifdef CONFIG_FEATURE_FBSET_FANCY
			case CMD_XRES:
				varset.xres = xatou32(argv[1]);
				break;
			case CMD_YRES:
				varset.yres = xatou32(argv[1]);
				break;
			case CMD_DEPTH:
				varset.bits_per_pixel = xatou32(argv[1]);
				break;
#endif
			}
			argc -= g_cmdoptions[i].param_count;
			argv += g_cmdoptions[i].param_count;
			break;
		}
		if (!g_cmdoptions[i].name[0]) {
			if (argc != 1)
				bb_show_usage();
			mode = *argv;
			g_options |= OPT_READMODE;
		}
	}

	fh = xopen(fbdev, O_RDONLY);
	if (ioctl(fh, FBIOGET_VSCREENINFO, &var))
		bb_perror_msg_and_die("ioctl(%sT_VSCREENINFO)", "GE");
	if (g_options & OPT_READMODE) {
		if (!readmode(&var, modefile, mode)) {
			bb_error_msg_and_die("unknown video mode '%s'", mode);
		}
	}

	setmode(&var, &varset);
	if (g_options & OPT_CHANGE) {
		if (g_options & OPT_ALL)
			var.activate = FB_ACTIVATE_ALL;
		if (ioctl(fh, FBIOPUT_VSCREENINFO, &var))
			bb_perror_msg_and_die("ioctl(%sT_VSCREENINFO)", "PU");
	}
	showmode(&var);
	/* Don't close the file, as exiting will take care of that */
	/* close(fh); */

	return EXIT_SUCCESS;
}
