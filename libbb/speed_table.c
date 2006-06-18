/* vi: set sw=4 ts=4: */
/*
 * compact speed_t <-> speed functions for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

static const unsigned short speeds[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600,
   	19200, 38400, 57600>>8, 115200>>8, 230400>>8, 460800>>8, 500000>>8,
	576000>>8, 921600>>8, 1000000>>8, 1152000>>8, 1500000>>8, 2000000>>8,
	3000000>>8, 3500000>>8, 4000000>>8
};

unsigned int tty_baud_to_value(speed_t speed)
{
	int i;

	for (i=0; i<sizeof(speeds) / sizeof(*speeds); i++)
		if (speed == speeds[i] * (i>15 ? 256 : 1))
			return i>15 ? (i+4096-14) : i;

	return 0;
}

speed_t tty_value_to_baud(unsigned int value)
{
	int i;

	for (i=0; i<sizeof(speeds) / sizeof(*speeds); i++)
		if (value == (i>15 ? (i+4096-14) : i))
			return speeds[i] * (i>15 ? 256 : 1);

	return -1;
}
