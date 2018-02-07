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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND
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

void FAST_FUNC bb_progress_init(bb_progress_t *p, const char *curfile)
{
#if ENABLE_UNICODE_SUPPORT
	init_unicode();
	p->curfile = unicode_conv_to_printable_fixedwidth(/*NULL,*/ curfile, 20);
#else
	p->curfile = curfile;
#endif
	p->start_sec = monotonic_sec();
	p->last_update_sec = p->start_sec;
	p->last_change_sec = p->start_sec;
	p->last_size = 0;
}

/* File already had beg_size bytes.
 * Then we started downloading.
 * We downloaded "transferred" bytes so far.
 * Download is expected to stop when total size (beg_size + transferred)
 * will be "totalsize" bytes.
 * If totalsize == 0, then it is unknown.
 */
void FAST_FUNC bb_progress_update(bb_progress_t *p,
		uoff_t beg_size,
		uoff_t transferred,
		uoff_t totalsize)
{
	unsigned beg_and_transferred; /* does not need uoff_t, see scaling code below */
	unsigned since_last_update, elapsed;
	int notty;
	int kiloscale;

	//transferred = 1234; /* use for stall detection testing */
	//totalsize = 0; /* use for unknown size download testing */

	elapsed = monotonic_sec();
	since_last_update = elapsed - p->last_update_sec;
	p->last_update_sec = elapsed;

	if (totalsize != 0 && transferred >= totalsize - beg_size) {
		/* Last call. Do not skip this update */
		transferred = totalsize - beg_size; /* sanitize just in case */
	}
	else if (since_last_update == 0) {
		/*
		 * Do not update on every call
		 * (we can be called on every network read!)
		 */
		return;
	}

	kiloscale = 0;
	/*
	 * Scale sizes down if they are close to overflowing.
	 * This allows calculations like (100 * transferred / totalsize)
	 * without risking overflow: we guarantee 10 highest bits to be 0.
	 * Introduced error is less than 1 / 2^12 ~= 0.025%
	 */
	while (totalsize >= (1 << 22)) {
		totalsize >>= 10;
		beg_size >>= 10;
		transferred >>= 10;
		kiloscale++;
	}
	/* If they were huge, now they are scaled down to [4194303,4096] range.
	 * (N * totalsize) won't overflow 32 bits for N up to 1024.
	 * The downside is that files larger than 4194303 kbytes (>4GB)
	 * never show kbytes download size, they show "0M","1M"... right away
	 * since kiloscale is already >1.
	 */
#if ULONG_MAX == 0xffffffff
/* 32-bit CPU, uoff_t arithmetic is complex on it, cast variables to narrower types */
# define totalsize   ((unsigned)totalsize)
# define beg_size    ((unsigned)beg_size)
# define transferred ((unsigned)transferred)
#endif

	notty = !isatty(STDERR_FILENO);

	if (ENABLE_UNICODE_SUPPORT)
		fprintf(stderr, "\r%s" + notty, p->curfile);
	else
		fprintf(stderr, "\r%-20.20s" + notty, p->curfile);

	beg_and_transferred = beg_size + transferred;

	if (totalsize != 0) {
		int barlength;
		unsigned ratio = 100 * beg_and_transferred / totalsize;
		fprintf(stderr, "%4u%%", ratio);

		barlength = get_terminal_width(2) - 49;
		if (barlength > 0) {
			if (barlength > 999)
				barlength = 999;
			{
				/* god bless gcc for variable arrays :) */
				char buf[barlength + 1];
				unsigned stars = (unsigned)barlength * beg_and_transferred / totalsize;
				/* can't overflow ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
				memset(buf, ' ', barlength);
				buf[barlength] = '\0';
				memset(buf, '*', stars);
				fprintf(stderr, " |%s|", buf);
			}
		}
	}

	while (beg_and_transferred >= 100000) {
		beg_and_transferred >>= 10;
		kiloscale++;
	}
	/* see http://en.wikipedia.org/wiki/Tera */
	fprintf(stderr, "%6u%c", (unsigned)beg_and_transferred, " kMGTPEZY"[kiloscale]);
#define beg_and_transferred dont_use_beg_and_transferred_below()

	since_last_update = elapsed - p->last_change_sec;
	if ((unsigned)transferred != p->last_size) {
		p->last_change_sec = elapsed;
		p->last_size = (unsigned)transferred;
		if (since_last_update >= STALLTIME) {
			/* We "cut out" these seconds from elapsed time
			 * by adjusting start time */
			p->start_sec += since_last_update;
		}
		since_last_update = 0; /* we are un-stalled now */
	}

	elapsed -= p->start_sec; /* now it's "elapsed since start" */

	if (since_last_update >= STALLTIME) {
		fprintf(stderr, "  - stalled -");
	} else if (!totalsize || !transferred || (int)elapsed < 0) {
		fprintf(stderr, " --:--:-- ETA");
	} else {
		unsigned eta, secs, hours;
		unsigned bytes;

		bytes = totalsize - beg_size;

		/* Estimated remaining time =
		 * estimated_sec_to_dl_bytes - elapsed_sec =
		 * bytes / average_bytes_sec_so_far - elapsed =
		 * bytes / (transferred/elapsed) - elapsed =
		 * bytes * elapsed / transferred - elapsed
		 */
		eta = (unsigned long)bytes * elapsed / transferred - elapsed;
		/* if 32bit, can overflow ^^^^^^^^^^, but this would only show bad ETA */
		if (eta >= 1000*60*60)
			eta = 1000*60*60 - 1;
		secs = eta % 3600;
		hours = eta / 3600;
		fprintf(stderr, "%3u:%02u:%02u ETA", hours, secs / 60, secs % 60);
	}
	if (notty)
		fputc('\n', stderr);
}
