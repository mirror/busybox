#include <sys/swap.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>
#include "internal.h"

const char	swapoff_usage[] = "swapoff block-device\n"
"\n"
"\tStop swapping virtual memory pages on the given device.\n";

extern int
swapoff_fn(const struct FileInfo * i)
{
	struct mntent   entries[100];
	int	     count = 0;
	FILE *	  swapsTable = setmntent("/etc/swaps", "r");
	struct mntent * m;

	if (!(swapoff(i->source))) {
		if ( swapsTable == 0 ) {
			fprintf(stderr, "/etc/swaps: %s\n", strerror(errno));
			return 1;
		}
		while ( (m = getmntent(swapsTable)) != 0 ) {
			entries[count].mnt_fsname = strdup(m->mnt_fsname);
			entries[count].mnt_dir = strdup(m->mnt_dir);
			entries[count].mnt_type = strdup(m->mnt_type);
			entries[count].mnt_opts = strdup(m->mnt_opts);
			entries[count].mnt_freq = m->mnt_freq;
			entries[count].mnt_passno = m->mnt_passno;
			count++;
		}
		endmntent(swapsTable);
		if ( (swapsTable = setmntent("/etc/swaps", "w")) ) {
			int     id;
			for ( id = 0; id < count; id++ ) {
				int result = 
				 (strcmp(entries[id].mnt_fsname, i->source)==0
				 ||strcmp(entries[id].mnt_dir, i->source)==0);
				if ( result )
					continue;
				else
					addmntent(swapsTable, &entries[id]);
			}
			endmntent(swapsTable);
		}
		else if ( errno != EROFS )
			fprintf(stderr, "/etc/swaps: %s\n", strerror(errno));
		return (0);
	}
	return (-1);
}
