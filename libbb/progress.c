/* vi: set sw=4 ts=4: */
/*
 * Progress bar code.
 */
/* Original copyright notice which applies to the CONFIG_FEATURE_WGET_STATUSBAR stuff,
 * much of which was blatantly stolen from openssh.
 */
/*-
 * Copyright (c) 1992, 1993
 * The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. BSD Advertising Clause omitted per the July 22, 1999 licensing change
 *    ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "libbb.h"
#include "unicode.h"

enum {
	/* Seconds when xfer considered "stalled" */
	STALLTIME = 5
};

static unsigned int get_tty2_width(void)
{
	unsigned width;
	get_terminal_width_height(2, &width, NULL);
	return width;
}

void FAST_FUNC bb_progress_init(bb_progress_t *p)
{
	p->start_sec = monotonic_sec();
	p->lastupdate_sec = p->start_sec;
	p->lastsize = 0;
	p->inited = 1;
}

void FAST_FUNC bb_progress_update(bb_progress_t *p,
		const char *curfile,
		uoff_t beg_range,
		uoff_t transferred,
		uoff_t totalsize)
{
	uoff_t beg_and_transferred;
	unsigned since_last_update, elapsed;
	unsigned ratio;
	int barlength;
	int kiloscale;

	/* totalsize == 0 if it is unknown */

	beg_and_transferred = beg_range + transferred;

	elapsed = monotonic_sec();
	since_last_update = elapsed - p->lastupdate_sec;
	/* Do not update on every call
	 * (we can be called on every network read!) */
	if (since_last_update == 0 && beg_and_transferred < totalsize)
		return;

	/* Scale sizes down if they are close to overflowing.
	 * If off_t is only 32 bits, this allows calculations
	 * like (100 * transferred / totalsize) without risking overflow.
	 * Introduced error is < 0.1%
	 */
	kiloscale = 0;
	if (totalsize >= (1 << 20)) {
		totalsize >>= 10;
		beg_range >>= 10;
		transferred >>= 10;
		beg_and_transferred >>= 10;
		kiloscale++;
	}

	if (beg_and_transferred >= totalsize)
		beg_and_transferred = totalsize;

	ratio = 100 * beg_and_transferred / totalsize;
#if ENABLE_UNICODE_SUPPORT
	init_unicode();
	{
		char *buf = unicode_conv_to_printable_fixedwidth(/*NULL,*/ curfile, 20);
		fprintf(stderr, "\r%s%4u%% ", buf, ratio);
		free(buf);
	}
#else
	fprintf(stderr, "\r%-20.20s%4u%% ", curfile, ratio);
#endif

	barlength = get_tty2_width() - 49;
	if (barlength > 0) {
		/* god bless gcc for variable arrays :) */
		char buf[barlength + 1];
		unsigned stars = (unsigned)barlength * beg_and_transferred / totalsize;
		memset(buf, ' ', barlength);
		buf[barlength] = '\0';
		memset(buf, '*', stars);
		fprintf(stderr, "|%s|", buf);
	}

	while (beg_and_transferred >= 100000) {
		kiloscale++;
		beg_and_transferred >>= 10;
	}
	/* see http://en.wikipedia.org/wiki/Tera */
	fprintf(stderr, "%6u%c ", (unsigned)beg_and_transferred, " kMGTPEZY"[kiloscale]);
#define beg_and_transferred dont_use_beg_and_transferred_below()

	if (transferred > p->lastsize) {
		p->lastupdate_sec = elapsed;
		p->lastsize = transferred;
		if (since_last_update >= STALLTIME) {
			/* We "cut off" these seconds from elapsed time
			 * by adjusting start time */
			p->start_sec += since_last_update;
		}
		since_last_update = 0; /* we are un-stalled now */
	}
	elapsed -= p->start_sec; /* now it's "elapsed since start" */

	if (since_last_update >= STALLTIME) {
		fprintf(stderr, " - stalled -");
	} else {
		uoff_t to_download = totalsize - beg_range;
		if (!totalsize || (int)elapsed <= 0 || transferred > to_download) {
			fprintf(stderr, "--:--:-- ETA");
		} else {
			/* to_download / (transferred/elapsed) - elapsed: */
			unsigned eta = to_download * elapsed / transferred - elapsed;
			unsigned secs = eta % 3600;
			unsigned hours = eta / 3600;
			fprintf(stderr, "%02u:%02u:%02u ETA", hours, secs / 60, secs % 60);
		}
	}
}
