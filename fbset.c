/* vi: set sw=4 ts=4: */
/*
 * Mini fbset implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * This is a from-scratch implementation of fbset; but the de facto fbset
 * implementation was a good reference. fbset (original) is released under
 * the GPL, and is (c) 1995-1999 by: 
 *     Geert Uytterhoeven (Geert.Uytterhoeven@cs.kuleuven.ac.be)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include "busybox.h"

#define DEFAULTFBDEV  "/dev/fb0"
#define DEFAULTFBMODE "/etc/fb.modes"

static const int OPT_CHANGE   = (1 << 0);
static const int OPT_INFO     = (1 << 1);
static const int OPT_READMODE = (1 << 2);

enum {
	CMD_FB = 1,
	CMD_DB = 2,
	CMD_GEOMETRY = 3,
	CMD_TIMING = 4,
	CMD_ACCEL = 5,
	CMD_HSYNC = 6,
	CMD_VSYNC = 7,
	CMD_LACED = 8,
	CMD_DOUBLE = 9,
/* 	CMD_XCOMPAT =     10, */
	CMD_ALL = 11,
	CMD_INFO = 12,
	CMD_CHANGE = 13,

#ifdef BB_FEATURE_FBSET_FANCY
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

static unsigned int g_options = 0;

/* Stuff stolen from the kernel's fb.h */
static const int FBIOGET_VSCREENINFO = 0x4600;
static const int FBIOPUT_VSCREENINFO = 0x4601;
#define __u32			u_int32_t
struct fb_bitfield {
	__u32 offset;			/* beginning of bitfield	*/
	__u32 length;			/* length of bitfield		*/
	__u32 msb_right;		/* != 0 : Most significant bit is */ 
					/* right */ 
};
struct fb_var_screeninfo {
	__u32 xres;			/* visible resolution		*/
	__u32 yres;
	__u32 xres_virtual;		/* virtual resolution		*/
	__u32 yres_virtual;
	__u32 xoffset;			/* offset from virtual to visible */
	__u32 yoffset;			/* resolution			*/

	__u32 bits_per_pixel;		/* guess what			*/
	__u32 grayscale;		/* != 0 Graylevels instead of colors */

	struct fb_bitfield red;		/* bitfield in fb mem if true color, */
	struct fb_bitfield green;	/* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency			*/	

	__u32 nonstd;			/* != 0 Non standard pixel format */

	__u32 activate;			/* see FB_ACTIVATE_*		*/

	__u32 height;			/* height of picture in mm    */
	__u32 width;			/* width of picture in mm     */

	__u32 accel_flags;		/* acceleration flags (hints)	*/

	/* Timing: All values in pixclocks, except pixclock (of course) */
	__u32 pixclock;			/* pixel clock in ps (pico seconds) */
	__u32 left_margin;		/* time from sync to picture	*/
	__u32 right_margin;		/* time from picture to sync	*/
	__u32 upper_margin;		/* time from sync to picture	*/
	__u32 lower_margin;
	__u32 hsync_len;		/* length of horizontal sync	*/
	__u32 vsync_len;		/* length of vertical sync	*/
	__u32 sync;			/* see FB_SYNC_*		*/
	__u32 vmode;			/* see FB_VMODE_*		*/
	__u32 reserved[6];		/* Reserved for future compatibility */
};


static struct cmdoptions_t {
	char *name;
	unsigned char param_count;
	unsigned char code;
} g_cmdoptions[] = {
	{
	"-fb", 1, CMD_FB}, {
	"-db", 1, CMD_DB}, {
	"-a", 0, CMD_ALL}, {
	"-i", 0, CMD_INFO}, {
	"-g", 5, CMD_GEOMETRY}, {
	"-t", 7, CMD_TIMING}, {
	"-accel", 1, CMD_ACCEL}, {
	"-hsync", 1, CMD_HSYNC}, {
	"-vsync", 1, CMD_VSYNC}, {
	"-laced", 1, CMD_LACED}, {
	"-double", 1, CMD_DOUBLE}, {
	"-n", 0, CMD_CHANGE}, {
#ifdef BB_FEATURE_FBSET_FANCY
	"-all", 0, CMD_ALL}, {
	"-xres", 1, CMD_XRES}, {
	"-yres", 1, CMD_YRES}, {
	"-vxres", 1, CMD_VXRES}, {
	"-vyres", 1, CMD_VYRES}, {
	"-depth", 1, CMD_DEPTH}, {
	"-match", 0, CMD_MATCH}, {
	"-geometry", 5, CMD_GEOMETRY}, {
	"-pixclock", 1, CMD_PIXCLOCK}, {
	"-left", 1, CMD_LEFT}, {
	"-right", 1, CMD_RIGHT}, {
	"-upper", 1, CMD_UPPER}, {
	"-lower", 1, CMD_LOWER}, {
	"-hslen", 1, CMD_HSLEN}, {
	"-vslen", 1, CMD_VSLEN}, {
	"-timings", 7, CMD_TIMING}, {
	"-csync", 1, CMD_CSYNC}, {
	"-gsync", 1, CMD_GSYNC}, {
	"-extsync", 1, CMD_EXTSYNC}, {
	"-bcast", 1, CMD_BCAST}, {
	"-rgba", 1, CMD_RGBA}, {
	"-step", 1, CMD_STEP}, {
	"-move", 1, CMD_MOVE}, {
#endif
	0, 0, 0}
};

#ifdef BB_FEATURE_FBSET_READMODE
/* taken from linux/fb.h */
static const int FB_VMODE_INTERLACED = 1;	/* interlaced	*/
static const int FB_VMODE_DOUBLE = 2;	/* double scan */
static const int FB_SYNC_HOR_HIGH_ACT = 1;	/* horizontal sync high active	*/
static const int FB_SYNC_VERT_HIGH_ACT = 2;	/* vertical sync high active	*/
static const int FB_SYNC_EXT = 4;	/* external sync		*/
static const int FB_SYNC_COMP_HIGH_ACT = 8;	/* composite sync high active   */
#endif
static int readmode(struct fb_var_screeninfo *base, const char *fn,
					const char *mode)
{
#ifdef BB_FEATURE_FBSET_READMODE
	FILE *f;
	char buf[256];
	char *p = buf;

	f = xfopen(fn, "r");
	while (!feof(f)) {
		fgets(buf, sizeof(buf), f);
		if ((p = strstr(buf, "mode ")) || (p = strstr(buf, "mode\t"))) {
			p += 5;
			if ((p = strstr(buf, mode))) {
				p += strlen(mode);
				if (!isspace(*p) && (*p != 0) && (*p != '"')
					&& (*p != '\r') && (*p != '\n'))
					continue;	/* almost, but not quite */
				while (!feof(f)) {
					fgets(buf, sizeof(buf), f);

                    if ((p = strstr(buf, "geometry "))) {
                        p += 9;

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
                        p += 6;

                        if (strstr(buf, "false")) {
                            base->vmode &= ~FB_VMODE_INTERLACED;
                        } else {
                            base->vmode |= FB_VMODE_INTERLACED;
                        }
                    } else if ((p = strstr(buf, "double "))) {
                        p += 7;

                        if (strstr(buf, "false")) {
                            base->vmode &= ~FB_VMODE_DOUBLE;
                        } else {
                            base->vmode |= FB_VMODE_DOUBLE;
                        }
                    } else if ((p = strstr(buf, "vsync "))) {
                        p += 6;

                        if (strstr(buf, "low")) {
                            base->sync &= ~FB_SYNC_VERT_HIGH_ACT;
                        } else {
                            base->sync |= FB_SYNC_VERT_HIGH_ACT;
                        }
                    } else if ((p = strstr(buf, "hsync "))) {
                        p += 6;

                        if (strstr(buf, "low")) {
                            base->sync &= ~FB_SYNC_HOR_HIGH_ACT;
                        } else {
                            base->sync |= FB_SYNC_HOR_HIGH_ACT;
                        }
                    } else if ((p = strstr(buf, "csync "))) {
                        p += 6;

                        if (strstr(buf, "low")) {
                            base->sync &= ~FB_SYNC_COMP_HIGH_ACT;
                        } else {
                            base->sync |= FB_SYNC_COMP_HIGH_ACT;
                        }
                    } else if ((p = strstr(buf, "extsync "))) {
                        p += 8;

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
		}
	}
#else
	error_msg( "mode reading not compiled in");
#endif
	return 0;
}

static void setmode(struct fb_var_screeninfo *base,
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

static void showmode(struct fb_var_screeninfo *v)
{
	double drate = 0, hrate = 0, vrate = 0;

	if (v->pixclock) {
		drate = 1e12 / v->pixclock;
		hrate =
			drate / (v->left_margin + v->xres + v->right_margin +
					 v->hsync_len);
		vrate =
			hrate / (v->upper_margin + v->yres + v->lower_margin +
					 v->vsync_len);
	}
	printf("\nmode \"%ux%u-%u\"\n", v->xres, v->yres, (int) (vrate + 0.5));
#ifdef BB_FEATURE_FBSET_FANCY
	printf("\t# D: %.3f MHz, H: %.3f kHz, V: %.3f Hz\n", drate / 1e6,
		   hrate / 1e3, vrate);
#endif
	printf("\tgeometry %u %u %u %u %u\n", v->xres, v->yres,
		   v->xres_virtual, v->yres_virtual, v->bits_per_pixel);
	printf("\ttimings %u %u %u %u %u %u %u\n", v->pixclock, v->left_margin,
		   v->right_margin, v->upper_margin, v->lower_margin, v->hsync_len,
		   v->vsync_len);
	printf("\taccel %s\n", (v->accel_flags > 0 ? "true" : "false"));
	printf("\trgba %u/%u,%u/%u,%u/%u,%u/%u\n", v->red.length,
		   v->red.offset, v->green.length, v->green.offset, v->blue.length,
		   v->blue.offset, v->transp.length, v->transp.offset);
	printf("endmode\n\n");
}

#ifdef STANDALONE
int main(int argc, char **argv)
#else
extern int fbset_main(int argc, char **argv)
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
		for (i = 0; g_cmdoptions[i].name; i++) {
			if (!strcmp(thisarg, g_cmdoptions[i].name)) {
				if (argc - 1 < g_cmdoptions[i].param_count)
					show_usage();
				switch (g_cmdoptions[i].code) {
				case CMD_FB:
					fbdev = argv[1];
					break;
				case CMD_DB:
					modefile = argv[1];
					break;
				case CMD_GEOMETRY:
					varset.xres = strtoul(argv[1], 0, 0);
					varset.yres = strtoul(argv[2], 0, 0);
					varset.xres_virtual = strtoul(argv[3], 0, 0);
					varset.yres_virtual = strtoul(argv[4], 0, 0);
					varset.bits_per_pixel = strtoul(argv[5], 0, 0);
					break;
				case CMD_TIMING:
					varset.pixclock = strtoul(argv[1], 0, 0);
					varset.left_margin = strtoul(argv[2], 0, 0);
					varset.right_margin = strtoul(argv[3], 0, 0);
					varset.upper_margin = strtoul(argv[4], 0, 0);
					varset.lower_margin = strtoul(argv[5], 0, 0);
					varset.hsync_len = strtoul(argv[6], 0, 0);
					varset.vsync_len = strtoul(argv[7], 0, 0);
					break;
                case CMD_CHANGE:
                    g_options |= OPT_CHANGE;
                    break;
#ifdef BB_FEATURE_FBSET_FANCY
				case CMD_XRES:
					varset.xres = strtoul(argv[1], 0, 0);
					break;
				case CMD_YRES:
					varset.yres = strtoul(argv[1], 0, 0);
					break;
				case CMD_DEPTH:
					varset.bits_per_pixel = strtoul(argv[1], 0, 0);
					break;
#endif
				}
				argc -= g_cmdoptions[i].param_count;
				argv += g_cmdoptions[i].param_count;
				break;
			}
		}
		if (!g_cmdoptions[i].name) {
			if (argc == 1) {
				mode = *argv;
				g_options |= OPT_READMODE;
			} else {
				show_usage();
			}
		}
	}

	if ((fh = open(fbdev, O_RDONLY)) < 0)
		perror_msg_and_die("fbset(open)");
	if (ioctl(fh, FBIOGET_VSCREENINFO, &var))
		perror_msg_and_die("fbset(ioctl)");
	if (g_options & OPT_READMODE) {
		if (!readmode(&var, modefile, mode)) {
			error_msg("Unknown video mode `%s'", mode);
			return EXIT_FAILURE;
		}
	}

	setmode(&var, &varset);
	if (g_options & OPT_CHANGE)
		if (ioctl(fh, FBIOPUT_VSCREENINFO, &var))
			perror_msg_and_die("fbset(ioctl)");
	showmode(&var);
	/* Don't close the file, as exiting will take care of that */
	/* close(fh); */

	return EXIT_SUCCESS;
}
