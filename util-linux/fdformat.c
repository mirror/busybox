/* fdformat.c  -  Low-level formats a floppy disk - Werner Almesberger */

/* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * 1999-03-20 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - more i18n/nls translatable strings marked
 *
 * 5 July 2003 -- modified for Busybox by Erik Andersen
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "busybox.h"


/* Stuff extracted from linux/fd.h */
struct floppy_struct {
	unsigned int	size,		/* nr of sectors total */
			sect,		/* sectors per track */
			head,		/* nr of heads */
			track,		/* nr of tracks */
			stretch;	/* !=0 means double track steps */
#define FD_STRETCH 1
#define FD_SWAPSIDES 2

	unsigned char	gap,		/* gap1 size */

			rate,		/* data rate. |= 0x40 for perpendicular */
#define FD_2M 0x4
#define FD_SIZECODEMASK 0x38
#define FD_SIZECODE(floppy) (((((floppy)->rate&FD_SIZECODEMASK)>> 3)+ 2) %8)
#define FD_SECTSIZE(floppy) ( (floppy)->rate & FD_2M ? \
			     512 : 128 << FD_SIZECODE(floppy) )
#define FD_PERP 0x40

			spec1,		/* stepping rate, head unload time */
			fmt_gap;	/* gap2 size */
	const char	* name; /* used only for predefined formats */
};
struct format_descr {
	unsigned int device,head,track;
};
#define FDFMTBEG _IO(2,0x47)
#define	FDFMTTRK _IOW(2,0x48, struct format_descr)
#define FDFMTEND _IO(2,0x49)
#define FDGETPRM _IOR(2, 0x04, struct floppy_struct)
#define FD_FILL_BYTE 0xF6 /* format fill byte. */



static void format_disk(int ctrl, char *name, struct floppy_struct *param)
{
    struct format_descr descr;
    int track;

    printf("Formatting ... ");
    fflush(stdout);
    if (ioctl(ctrl,FDFMTBEG,NULL) < 0) {
	bb_perror_msg_and_die("FDFMTBEG");
    }
    for (track = 0; track < param->track; track++) 
    {
	descr.track = track;
	descr.head = 0;
	if (ioctl(ctrl,FDFMTTRK,(long) &descr) < 0) {
	    bb_perror_msg_and_die("FDFMTTRK");
	}

	printf("%3d\b\b\b",track);
	fflush(stdout);
	if (param->head == 2) {
	    descr.head = 1;
	    if (ioctl(ctrl,FDFMTTRK,(long) &descr) < 0) {
		bb_perror_msg_and_die("FDFMTTRK");
	    }
	}
    }
    if (ioctl(ctrl,FDFMTEND,NULL) < 0) {
	bb_perror_msg_and_die("FDFMTEND");
    }
    printf("done\n");
}

static void verify_disk(char *name, struct floppy_struct *param)
{
    unsigned char *data;
    int fd,cyl_size,cyl,count,read_bytes;

    cyl_size = param->sect*param->head*512;
    data = xmalloc(cyl_size);
    printf("Verifying ... ");
    fflush(stdout);
    fd = bb_xopen(name,O_RDONLY);
    for (cyl = 0; cyl < param->track; cyl++) 
    {
	printf("%3d\b\b\b",cyl);
	fflush(stdout);
	read_bytes = safe_read(fd,data,cyl_size);
	if(read_bytes != cyl_size) {
	    if(read_bytes < 0) {
		bb_perror_msg("Read: ");
	    }
	    bb_error_msg_and_die("Problem reading cylinder %d, "
		    "expected %d, read %d", cyl, cyl_size, read_bytes);
	}
	for (count = 0; count < cyl_size; count++)
	    if (data[count] != FD_FILL_BYTE) {
		printf("bad data in cyl %d\nContinuing ... ",cyl);
		fflush(stdout);
		break;
	    }
    }
    printf("done\n");
    close(fd);
}

int fdformat_main(int argc,char **argv)
{
    int ctrl;
    int verify;
    struct stat st;
    struct floppy_struct param;

    if (argc < 2) {
	bb_show_usage();
    }
    verify = !bb_getopt_ulflags(argc, argv, "n");
    argv += optind;

    if (stat(*argv,&st) < 0 || access(*argv,W_OK) < 0) {
	bb_perror_msg_and_die(*argv);
    }
    if (!S_ISBLK(st.st_mode)) {
	bb_error_msg_and_die("%s: not a block device",*argv);
	/* do not test major - perhaps this was an USB floppy */
    }

    ctrl = bb_xopen(*argv,O_WRONLY);
    if (ioctl(ctrl,FDGETPRM,(long) &param) < 0) { 
	bb_perror_msg_and_die("Could not determine current format type");
    }
    printf("%s-sided, %d tracks, %d sec/track. Total capacity %d kB.\n",
	    (param.head == 2) ? "Double" : "Single",
	    param.track, param.sect,param.size >> 1);
    format_disk(ctrl, *argv, &param);
    close(ctrl);

    if (verify) {
	verify_disk(*argv, &param);
    }
    return EXIT_SUCCESS;
}
