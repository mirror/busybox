/*
 * losetup.c - setup and control loop devices
 */


#include "internal.h"

const char	losetup_usage[] = "losetup\n"
"\n"
"\tlosetup loop_device				give info\n"
"\tlosetup -d loop_device				delete\n"
"\tlosetup [ -o offset ] loop_device file		setup\n";

/* from mount-2.6d */

/*
 * losetup.c - setup and control loop devices
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
/* #include "loop.h" */

/*
 * include/linux/loop.h
 *
 * Written by Theodore Ts'o, 3/29/93.
 *
 * Copyright 1993 by Theodore Ts'o.  Redistribution of this file is
 * permitted under the GNU Public License.
 */

#define LO_NAME_SIZE	64
#define LO_KEY_SIZE	32
       
struct loop_info {
	int		lo_number;	/* ioctl r/o */
	dev_t		lo_device; 	/* ioctl r/o */
	unsigned long	lo_inode; 	/* ioctl r/o */
	dev_t		lo_rdevice; 	/* ioctl r/o */
	int		lo_offset;
	int		lo_encrypt_type;
	int		lo_encrypt_key_size; 	/* ioctl w/o */
	int		lo_flags;	/* ioctl r/o */
	char		lo_name[LO_NAME_SIZE];
	unsigned char	lo_encrypt_key[LO_KEY_SIZE]; /* ioctl w/o */
	unsigned long	lo_init[2];
	char		reserved[4];
};

/*
 * IOCTL commands --- we will commandeer 0x4C ('L')
 */

#define LOOP_SET_FD	0x4C00
#define LOOP_CLR_FD	0x4C01
#define LOOP_SET_STATUS	0x4C02
#define LOOP_GET_STATUS	0x4C03

/* #include "lomount.h" */

extern int set_loop (const char *, const char *, int, int *);
extern int del_loop (const char *);

static void show_loop(const char *device)
{
	struct	loop_info	loopinfo;
	int			fd;
	
	if ((fd = open(device, O_RDWR)) < 0) {
		perror(device);
		return;
	}
	if (ioctl(fd, LOOP_GET_STATUS, &loopinfo) < 0) {
		perror("Cannot get loop info");
		close(fd);
		return;
	}
	printf("%s: [%04x]:%ld (%s) offset %d\n",
	       device, (unsigned int)loopinfo.lo_device, loopinfo.lo_inode,
	       loopinfo.lo_name, loopinfo.lo_offset);
	close(fd);
}


int set_loop(const char *device, const char *file, int offset, int *loopro)
{
	struct loop_info loopinfo;
	int	fd, ffd, mode;
	
	mode = *loopro ? O_RDONLY : O_RDWR;
	if ((ffd = open (file, mode)) < 0 && !*loopro
	    && (errno != EROFS || (ffd = open (file, mode = O_RDONLY)) < 0)) {
	  perror (file);
	  return 1;
	}
	if ((fd = open (device, mode)) < 0) {
	  close(ffd);
	  perror (device);
	  return 1;
	}
	*loopro = (mode == O_RDONLY);

	memset(&loopinfo, 0, sizeof(loopinfo));
	strncpy(loopinfo.lo_name, file, LO_NAME_SIZE);
	loopinfo.lo_name[LO_NAME_SIZE-1] = 0;

	loopinfo.lo_offset = offset;

	loopinfo.lo_encrypt_key_size = 0;
	if (ioctl(fd, LOOP_SET_FD, ffd) < 0) {
		perror("ioctl: LOOP_SET_FD");
		exit(1);
	}
	if (ioctl(fd, LOOP_SET_STATUS, &loopinfo) < 0) {
		(void) ioctl(fd, LOOP_CLR_FD, 0);
		perror("ioctl: LOOP_SET_STATUS");
		exit(1);
	}
	close(fd);
	close(ffd);
	return 0;
}

int del_loop(const char *device)
{
	int fd;

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror(device);
		exit(1);
	}
	if (ioctl(fd, LOOP_CLR_FD, 0) < 0) {
		perror("ioctl: LOOP_CLR_FD");
		exit(1);
	}
	close(fd);
	return(0);
}


static int losetup_usage_fn(void)
{
	fprintf(stderr, losetup_usage);
	exit(1);
}

int losetup_main(struct FileInfo * i, int argc, char * * argv)
{
	char *offset;
	int delete,off,c;
	int ro = 0;

	delete = off = 0;
	offset = NULL;
	while ((c = getopt(argc,argv,"do:")) != EOF) {
		switch (c) {
			case 'd':
				delete = 1;
				break;
			case 'o':
				offset = optarg;
				break;
			default:
				losetup_usage_fn();
		}
	}
	if (argc == 1) losetup_usage_fn();
	if ((delete && (argc != optind+1 || offset)) ||
	    (!delete && (argc < optind+1 || argc > optind+2)))
		losetup_usage_fn();
	if (argc == optind+1)
		if (delete)
			del_loop(argv[optind]);
		else
			show_loop(argv[optind]);
	else {
		if (offset && sscanf(offset,"%d",&off) != 1)
			losetup_usage_fn();
		set_loop(argv[optind],argv[optind+1],off,&ro);
	}
	return 0;
}
