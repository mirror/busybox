/* vi: set sw=4 ts=4: */
/*
 * public domain -- Dave 'Kill a Cop' Cinege <dcinege@psychosis.com>
 *
 * makedevs
 * Make ranges of device files quickly.
 * known bugs: can't deal with alpha ranges
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysmacros.h>     /* major() and minor() */
#include "busybox.h"

int makedevs_main(int argc, char **argv)
{
	mode_t mode;
	char *basedev, *type, *nodname, buf[255];
	int Smajor, Sminor, S, E;

	if (argc < 7 || *argv[1]=='-')
		bb_show_usage();

	basedev = argv[1];
	type = argv[2];
	Smajor = atoi(argv[3]);
	Sminor = atoi(argv[4]);
	S = atoi(argv[5]);
	E = atoi(argv[6]);
	nodname = argc == 8 ? basedev : buf;

	mode = 0660;

	switch (type[0]) {
	case 'c':
		mode |= S_IFCHR;
		break;
	case 'b':
		mode |= S_IFBLK;
		break;
	case 'f':
		mode |= S_IFIFO;
		break;
	default:
		bb_show_usage();
	}

	while (S <= E) {
		int sz;

		sz = snprintf(buf, sizeof(buf), "%s%d", basedev, S);
		if(sz<0 || sz>=sizeof(buf))  /* libc different */
			bb_error_msg_and_die("%s too large", basedev);

	/* if mode != S_IFCHR and != S_IFBLK third param in mknod() ignored */

		if (mknod(nodname, mode, makedev(Smajor, Sminor)))
			bb_error_msg("Failed to create: %s", nodname);

		if (nodname == basedev) /* ex. /dev/hda - to /dev/hda1 ... */
			nodname = buf;
		S++;
		Sminor++;
	}

	return 0;
}

/*
And this is what this program replaces. The shell is too slow!

makedev () {
local basedev=$1; local S=$2; local E=$3
local major=$4; local Sminor=$5; local type=$6
local sbase=$7

	if [ ! "$sbase" = "" ]; then
		mknod "$basedev" $type $major $Sminor
		S=`expr $S + 1`
		Sminor=`expr $Sminor + 1`
	fi

	while [ $S -le $E ]; do
		mknod "$basedev$S" $type $major $Sminor
		S=`expr $S + 1`
		Sminor=`expr $Sminor + 1`
	done
}
*/
