#include "internal.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

const char mknod_usage[] = "mknod file b|c|u|p major minor\n"
"\tMake special files.\n"
"\n"
"\tb:\tMake a block (buffered) device.\n"
"\tc or u:\tMake a character (un-buffered) device.\n"
"\tp:\tMake a named pipe. Major and minor are ignored for named pipes.\n";

int
mknod_main(struct FileInfo * i, int argc, char * * argv)
{
	mode_t	mode = 0;
	dev_t	dev = 0;

	switch(argv[2][0]) {
	case 'c':
	case 'u':
		mode = S_IFCHR;
		break;
	case 'b':
		mode = S_IFBLK;
		break;
	case 'p':
		mode = S_IFIFO;
		break;
	default:
		usage(mknod_usage);
		return 1;
	}

	if ( mode == S_IFCHR || mode == S_IFBLK ) {
		dev = (atoi(argv[3]) << 8) | atoi(argv[4]);
		if ( argc != 5 ) {
			usage(mknod_usage);
			return 1;
		}
	}

	mode |= 0666;

	if ( mknod(argv[1], mode, dev) != 0 ) {
		name_and_error(argv[1]);
		return 1;
	}
	return 0;
}
