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

#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/version.h>

#define PERROR(ctx)   do { perror(ctx); exit(1); } while(0)

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#define DEFAULTFBDEV  "/dev/fb0"
#define DEFAULTFBMODE "/etc/fb.modes"

#define OPT_CHANGE    1
#define OPT_INFO      (1 << 1)
#define OPT_READMODE  (1 << 2)

#define CMD_HELP        0
#define CMD_FB		1
#define CMD_DB		2
#define CMD_GEOMETRY	3
#define CMD_TIMING	4
#define CMD_ACCEL	5
#define CMD_HSYNC	6
#define CMD_VSYNC	7
#define CMD_LACED	8
#define CMD_DOUBLE	9
/* #define CMD_XCOMPAT     10 */
#define CMD_ALL         11
#define CMD_INFO        12
#define CMD_CHANGE      13

#ifdef BB_FEATURE_FBSET_FANCY
#define CMD_XRES	100
#define CMD_YRES	101
#define CMD_VXRES	102
#define CMD_VYRES	103
#define CMD_DEPTH	104
#define CMD_MATCH	105
#define CMD_PIXCLOCK	106
#define CMD_LEFT	107
#define CMD_RIGHT	108
#define CMD_UPPER	109
#define CMD_LOWER	110
#define CMD_HSLEN	111
#define CMD_VSLEN	112
#define CMD_CSYNC	113
#define CMD_GSYNC	114
#define CMD_EXTSYNC	115
#define CMD_BCAST	116
#define CMD_RGBA	117
#define CMD_STEP	118
#define CMD_MOVE	119
#endif

static unsigned int g_options = 0;

struct cmdoptions_t {
	char *name;
	unsigned char param_count;
	unsigned char code;
} g_cmdoptions[] = {
	{
	"-h", 0, CMD_HELP}, {
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
	"-help", 0, CMD_HELP}, {
	"-n", 0, CMD_CHANGE}, {
#ifdef BB_FEATURE_FBSET_FANCY
	"-help", 0, CMD_HELP}, {
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

static int readmode(struct fb_var_screeninfo *base, const char *fn,
					const char *mode)
{
#ifdef BB_FBSET_READMODE
	FILE *f;
	char buf[256];
	char *p = buf;

	if ((f = fopen(fn, "r")) == NULL)
		PERROR("readmode(fopen)");
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
					if (!strstr(buf, "endmode"))
						return 1;
				}
			}
		}
	}
#else
	errorMsg( "mode reading not compiled in\n");
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	printf("\ttimings %u %u %u %u %u %u %u\n", v->pixclock, v->left_margin,
		   v->right_margin, v->upper_margin, v->lower_margin, v->hsync_len,
		   v->vsync_len);
	printf("\taccel %s\n", (v->accel_flags > 0 ? "true" : "false"));
#else
	printf("\ttimings %lu %lu %lu %lu %lu %lu %lu\n", v->pixclock,
		   v->left_margin, v->right_margin, v->upper_margin,
		   v->lower_margin, v->hsync_len, v->vsync_len);
#endif
	printf("\trgba %u/%u,%u/%u,%u/%u,%u/%u\n", v->red.length,
		   v->red.offset, v->green.length, v->green.offset, v->blue.length,
		   v->blue.offset, v->transp.length, v->transp.offset);
	printf("endmode\n\n");
}

static void fbset_usage(void)
{
#ifndef BB_FEATURE_TRIVIAL_HELP
	int i;
#endif

#ifndef STANDALONE
	fprintf(stderr, "BusyBox v%s (%s) multi-call binary -- GPL2\n\n",
			BB_VER, BB_BT);
#endif
	fprintf(stderr, "Usage: fbset [options] [mode]\n");
#ifndef BB_FEATURE_TRIVIAL_HELP
	fprintf(stderr, "\nShows and modifies frame buffer device settings\n\n");
	fprintf(stderr, "The following options are recognized:\n");
	for (i = 0; g_cmdoptions[i].name; i++)
		fprintf(stderr, "\t%s\n", g_cmdoptions[i].name);
#endif
	exit(-1);
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
					fbset_usage();
				switch (g_cmdoptions[i].code) {
				case CMD_HELP:
					fbset_usage();
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
				fbset_usage();
			}
		}
	}

	if ((fh = open(fbdev, O_RDONLY)) < 0)
		PERROR("fbset(open)");
	if (ioctl(fh, FBIOGET_VSCREENINFO, &var))
		PERROR("fbset(ioctl)");
	if (g_options & OPT_READMODE) {
		if (!readmode(&var, modefile, mode)) {
			fprintf(stderr, "Unknown video mode `%s'\n", mode);
			exit(1);
		}
	}

	setmode(&var, &varset);
	if (g_options & OPT_CHANGE)
		if (ioctl(fh, FBIOPUT_VSCREENINFO, &var))
			PERROR("fbset(ioctl)");
	showmode(&var);
	/* Don't close the file, as exiting will take care of that */
	/* close(fh); */

	return (TRUE);
}
