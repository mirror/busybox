#include <stdio.h>
#include <mntent.h>
#include <sys/swap.h>
#include "internal.h"

const char	swapon_usage[] = "swapon block-device\n"
"\n"
"\tSwap virtual memory pages on the given device.\n";

extern int
swapon_fn(const struct FileInfo * i)
{
	FILE *swapsTable;
	struct mntent m;

        if (!(swapon(i->source, 0))) {
		if ((swapsTable = setmntent("/etc/swaps", "a+"))) {
			/* Needs the cast to avoid warning about conversion from
			 * const char* to just  char*
			 */
			m.mnt_fsname = (char*)i->source;
			m.mnt_dir = "none";
			m.mnt_type = "swap";
			m.mnt_opts = "sw";
			m.mnt_freq = 0;
			m.mnt_passno = 0;
			addmntent(swapsTable, &m);
			endmntent(swapsTable);
		}
		return (0);
	}
	return (-1);
}

