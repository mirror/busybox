#include "internal.h"
/*
 * mkswap.c - set up a linux swap device
 *
 * (C) 1991 Linus Torvalds. This file may be redistributed as per
 * the Linux copyright.
 */

/*
 * 20.12.91  -	time began. Got VM working yesterday by doing this by hand.
 *
 * Usage: mkswap [-c] device [size-in-blocks]
 *
 *	-c for readablility checking (use it unless you are SURE!)
 *
 * The device may be a block device or a image of one, but this isn't
 * enforced (but it's not much fun on a character device :-).
 *
 * Patches from jaggy@purplet.demon.co.uk (Mike Jagdis) to make the
 * size-in-blocks parameter optional added Wed Feb  8 10:33:43 1995.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <asm/page.h>
#include <linux/fs.h>

#ifndef __linux__
# define volatile
#endif

#define TEST_BUFFER_PAGES 8

const char	mkswap_usage[] = "mkswap [-c] partition [block-count]\n"
"\n"
"\tPrepare a disk partition to be used as a swap partition.\n"
"\tThe default block count is the size of the entire partition.\n"
"\n"
"\t-c:\tCheck for read-ability.\n"
"\tblock-count\tUse only this many blocks.\n";

static const char * program_name = "mkswap";
static const char * device_name = NULL;
static int DEV = -1;
static long PAGES = 0;
static int do_check = 0;
static int badpages = 0;


static long bit_test_and_set (unsigned int *addr, unsigned int nr)
{
	unsigned int r, m;

	addr += nr / (8 * sizeof(int));
	r = *addr;
	m = 1 << (nr & (8 * sizeof(int) - 1));
	*addr = r | m;
	return (r & m) != 0;
}

static int bit_test_and_clear (unsigned int *addr, unsigned int nr)
{
	unsigned int r, m;

	addr += nr / (8 * sizeof(int));
	r = *addr;
	m = 1 << (nr & (8 * sizeof(int) - 1));
	*addr = r & ~m;
	return (r & m) != 0;
}

/*
 * Volatile to let gcc know that this doesn't return. When trying
 * to compile this under minix, volatile gives a warning, as
 * exit() isn't defined as volatile under minix.
 */
volatile void fatal_error(const char * fmt_string)
{
	fprintf(stderr,fmt_string,program_name,device_name);
	exit(1);
}

#define die(str) fatal_error("%s: " str "\n")

static void check_blocks(int * signature_page)
{
	unsigned int current_page;
	int do_seek = 1;
	char buffer[PAGE_SIZE];

	current_page = 0;
	while (current_page < PAGES) {
		if (!do_check) {
			bit_test_and_set(signature_page,current_page++);
			continue;
		} else {
			printf("\r%d", current_page);
		}
		if (do_seek && lseek(DEV,current_page*PAGE_SIZE,SEEK_SET) !=
		current_page*PAGE_SIZE)
			die("seek failed in check_blocks");
		if ( (do_seek = (PAGE_SIZE != read(DEV, buffer, PAGE_SIZE))) ) {
			bit_test_and_clear(signature_page,current_page++);
			badpages++;
			continue;
		}
		bit_test_and_set(signature_page,current_page++);
	}
	if (do_check)
		printf("\n");
	if (badpages)
		printf("%d bad page%s\n",badpages,(badpages>1)?"s":"");
}

static long valid_offset (int fd, int offset)
{
	char ch;

	if (lseek (fd, offset, 0) < 0)
		return 0;
	if (read (fd, &ch, 1) < 1)
		return 0;
	return 1;
}

static int count_blocks (int fd)
{
	int high, low;

	low = 0;
	for (high = 1; valid_offset (fd, high); high *= 2)
		low = high;
	while (low < high - 1)
	{
		const int mid = (low + high) / 2;

		if (valid_offset (fd, mid))
			low = mid;
		else
			high = mid;
	}
	valid_offset (fd, 0);
	return (low + 1);
}

static int get_size(const char  *file)
{
	int	fd;
	int	size;

	fd = open(file, O_RDWR);
	if (fd < 0) {
		perror(file);
		exit(1);
	}
	if (ioctl(fd, BLKGETSIZE, &size) >= 0) {
		close(fd);
		return (size * 512);
	}
		
	size = count_blocks(fd);
	close(fd);
	return size;
}

int
mkswap(char *device_name, int pages, int check)
  {
	struct stat statbuf;
	int goodpages;
	int signature_page[PAGE_SIZE/sizeof(int)];

	PAGES = pages;
	do_check = check;

	memset(signature_page,0,PAGE_SIZE);

	if (device_name && !PAGES) {
		PAGES = get_size(device_name) / PAGE_SIZE;
	}
	if (!device_name || PAGES<10) {
		fprintf(stderr,
			"%s: error: swap area needs to be at least %ldkB\n",
			program_name, 10 * PAGE_SIZE / 1024);
		/*		usage(mkswap_usage); */
		exit(1);
	}
	if (PAGES > 8 * (PAGE_SIZE - 10)) {
	        PAGES = 8 * (PAGE_SIZE - 10);
		fprintf(stderr, "%s: warning: truncating swap area to %ldkB\n",
			program_name, PAGES * PAGE_SIZE / 1024);
	}
	DEV = open(device_name,O_RDWR);
	if (DEV < 0 || fstat(DEV, &statbuf) < 0) {
		perror(device_name);
		exit(1);
	}
	if (!S_ISBLK(statbuf.st_mode))
		do_check=0;
	else if (statbuf.st_rdev == 0x0300 || statbuf.st_rdev == 0x0340)
		die("Will not try to make swapdevice on '%s'");
	check_blocks(signature_page);
	if (!bit_test_and_clear(signature_page,0))
		die("fatal: first page unreadable");
	goodpages = PAGES - badpages - 1;
	if (goodpages <= 0)
		die("Unable to set up swap-space: unreadable");
	printf("Setting up swapspace, size = %ld bytes\n",goodpages*PAGE_SIZE);
	strncpy((char*)signature_page+PAGE_SIZE-10,"SWAP-SPACE",10);
	if (lseek(DEV, 0, SEEK_SET))
		die("unable to rewind swap-device");
	if (PAGE_SIZE != write(DEV, signature_page, PAGE_SIZE))
		die("unable to write signature page");

	close(DEV);
	return 0;
}

int mkswap_main(struct FileInfo * unnecessary, int argc, char ** argv)
{
	char * tmp;
	long int pages=0;
	int check=0;

	if (argc && *argv)
		program_name = *argv;
	while (argc > 1) {
		argv++;
		argc--;
		if (argv[0][0] != '-')
			if (device_name) {
				pages = strtol(argv[0],&tmp,0)>>(PAGE_SHIFT-10);
				if (*tmp) {
				  	usage(mkswap_usage);
					exit(1);
				}
			} else
				device_name = argv[0];
		else while (*++argv[0])
			switch (argv[0][0]) {
				case 'c': check=1; break;
				default: usage(mkswap_usage);
					exit(1);
			}
	}
	return mkswap(device_name, pages, check);
}
