/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2008 Rob Landley <rob@landley.net>
 * Copyright (C) 2008 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

int FAST_FUNC read_key(int fd, smalluint *nbuffered, char *buffer)
{
	struct pollfd pfd;
	const char *seq;
	int n;
	int c;

	/* Known escape sequences for cursor and function keys */
	static const char esccmds[] ALIGN1 = {
		'O','A'        |0x80,KEYCODE_UP      ,
		'O','B'        |0x80,KEYCODE_DOWN    ,
		'O','C'        |0x80,KEYCODE_RIGHT   ,
		'O','D'        |0x80,KEYCODE_LEFT    ,
		'O','H'        |0x80,KEYCODE_HOME    ,
		'O','F'        |0x80,KEYCODE_END     ,
#if 0
		'O','P'        |0x80,KEYCODE_FUN1    ,
		/* [ESC] ESC O [2] P - [Alt-][Shift-]F1 */
		/* Ctrl- seems to not affect sequences */
		'O','Q'        |0x80,KEYCODE_FUN2    ,
		'O','R'        |0x80,KEYCODE_FUN3    ,
		'O','S'        |0x80,KEYCODE_FUN4    ,
#endif
		'[','A'        |0x80,KEYCODE_UP      ,
		'[','B'        |0x80,KEYCODE_DOWN    ,
		'[','C'        |0x80,KEYCODE_RIGHT   ,
		'[','D'        |0x80,KEYCODE_LEFT    ,
		'[','H'        |0x80,KEYCODE_HOME    , /* xterm */
		/* [ESC] ESC [ [2] H - [Alt-][Shift-]Home */
		'[','F'        |0x80,KEYCODE_END     , /* xterm */
		'[','1','~'    |0x80,KEYCODE_HOME    , /* vt100? linux vt? or what? */
		'[','2','~'    |0x80,KEYCODE_INSERT  ,
		'[','3','~'    |0x80,KEYCODE_DELETE  ,
		/* [ESC] ESC [ 3 [;2] ~ - [Alt-][Shift-]Delete */
		'[','4','~'    |0x80,KEYCODE_END     , /* vt100? linux vt? or what? */
		'[','5','~'    |0x80,KEYCODE_PAGEUP  ,
		'[','6','~'    |0x80,KEYCODE_PAGEDOWN,
		'[','7','~'    |0x80,KEYCODE_HOME    , /* vt100? linux vt? or what? */
		'[','8','~'    |0x80,KEYCODE_END     , /* vt100? linux vt? or what? */
#if 0
		'[','1','1','~'|0x80,KEYCODE_FUN1    ,
		'[','1','2','~'|0x80,KEYCODE_FUN2    ,
		'[','1','3','~'|0x80,KEYCODE_FUN3    ,
		'[','1','4','~'|0x80,KEYCODE_FUN4    ,
		'[','1','5','~'|0x80,KEYCODE_FUN5    ,
		/* [ESC] ESC [ 1 5 [;2] ~ - [Alt-][Shift-]F5 */
		'[','1','7','~'|0x80,KEYCODE_FUN6    ,
		'[','1','8','~'|0x80,KEYCODE_FUN7    ,
		'[','1','9','~'|0x80,KEYCODE_FUN8    ,
		'[','2','0','~'|0x80,KEYCODE_FUN9    ,
		'[','2','1','~'|0x80,KEYCODE_FUN10   ,
		'[','2','3','~'|0x80,KEYCODE_FUN11   ,
		'[','2','4','~'|0x80,KEYCODE_FUN12   ,
#endif
		0
	};

	n = 0;
	if (nbuffered)
		n = *nbuffered;
	if (n == 0) {
		/* If no data, block waiting for input. If we read more
		 * than the minimal ESC sequence size, the "n=0" below
		 * would instead have to figure out how much to keep,
		 * resulting in larger code. */
		n = safe_read(fd, buffer, 3);
		if (n <= 0)
			return -1;
	}

	/* Grab character to return from buffer */
	c = (unsigned char)buffer[0];
	n--;
	if (n)
		memmove(buffer, buffer + 1, n);

	/* Only ESC starts ESC sequences */
	if (c != 27)
		goto ret;

	/* Loop through known ESC sequences */
	pfd.fd = fd;
	pfd.events = POLLIN;
	seq = esccmds;
	while (*seq != '\0') {
		/* n - position in sequence we did not read yet */
		int i = 0; /* position in sequence to compare */

		/* Loop through chars in this sequence */
		while (1) {
			/* So far escape sequence matched up to [i-1] */
			if (n <= i) {
				/* Need more chars, read another one if it wouldn't block.
				 * Note that escape sequences come in as a unit,
				 * so if we block for long it's not really an escape sequence.
				 * Timeout is needed to reconnect escape sequences
				 * split up by transmission over a serial console. */
				if (safe_poll(&pfd, 1, 50) == 0) {
					/* No more data!
					 * Array is sorted from shortest to longest,
					 * we can't match anything later in array,
					 * break out of both loops. */
					goto ret;
				}
				errno = 0;
				if (safe_read(fd, buffer + n, 1) <= 0) {
					/* If EAGAIN, then fd is O_NONBLOCK and poll lied:
					 * in fact, there is no data. */
					if (errno != EAGAIN)
						c = -1; /* otherwise it's EOF/error */
					goto ret;
				}
				n++;
			}
			if (buffer[i] != (seq[i] & 0x7f)) {
				/* This seq doesn't match, go to next */
				seq += i;
				/* Forward to last char */
				while (!(*seq & 0x80))
					seq++;
				/* Skip it and the keycode which follows */
				seq += 2;
				break;
			}
			if (seq[i] & 0x80) {
				/* Entire seq matched */
				c = (signed char)seq[i+1];
				n = 0;
				/* n -= i; memmove(...);
				 * would be more correct,
				 * but we never read ahead that much,
				 * and n == i here. */
				goto ret;
			}
			i++;
		}
	}
	/* We did not find matching sequence, it was a bare ESC.
	 * We possibly read and stored more input in buffer[]
	 * by now. */

 ret:
	if (nbuffered)
		*nbuffered = n;
	return c;
}
