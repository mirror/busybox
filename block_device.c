#include "internal.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>

const char	block_device_usage[] = "block_device mount-point";

static dev_t *my_device;
static char *my_device_name;

int 
match_mount(const struct FileInfo * i) {
	if ( S_ISBLK(i->stat.st_mode) 
	 && (i->stat.st_rdev == *my_device)) {
		my_device_name=strdup(i->source);
		return 1;
	} else
		return 0;
}

extern int
block_device_main(struct FileInfo * i, int argc, char * * argv)
{
	char *device_name = block_device(argv[1],i);
	if ( device_name == NULL )
		return -1;
	printf("%s\n", device_name);
	exit(0);
}

char * block_device(const char *name, struct FileInfo *i)
{
	struct stat	s;
        char *buf;
	int dinam=0;

	if ( stat(name, &s) ) return (char *) NULL;
	if (!i) {
	  i=(struct FileInfo*)malloc(sizeof(struct FileInfo));
	  dinam = 1;
	}
	memset((void *)i, 0, sizeof(struct FileInfo));
	my_device=(dev_t *)malloc(sizeof(dev_t));
	*my_device = s.st_dev;
	my_device_name = NULL;
	i->source = "/dev";
	i->stat = s;
	i->processDirectoriesAfterTheirContents=1;
	descend(i, match_mount);
	if (dinam) free(i);
	if ( my_device_name ) {
                buf = strdup(my_device_name);
		free(my_device);
		free(my_device_name);
		return buf;
        } else {
		fprintf( stderr
		,"Can't find special file for block device %d, %d.\n"
		,(int) *my_device >> 8 & 0xff
		,(int) *my_device & 0xff);
		free(my_device);
		return (char *) NULL;
	}
}
