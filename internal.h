#ifndef	_INTERNAL_H_
#define	_INTERNAL_H_

#include "busybox.def.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


/* Some useful definitions */
typedef int     BOOL;
#define STDIN	0
#define STDOUT	1
#define FALSE   ((BOOL) 0)
#define TRUE    ((BOOL) 1)

#define PATH_LEN        1024
#define BUF_SIZE        8192
#define EXPAND_ALLOC    1024

#define isBlank(ch)     (((ch) == ' ') || ((ch) == '\t'))
#define isDecimal(ch)   (((ch) >= '0') && ((ch) <= '9'))
#define isOctal(ch)     (((ch) >= '0') && ((ch) <= '7'))
#define isWildCard(ch)  (((ch) == '*') || ((ch) == '?') || ((ch) == '['))



struct FileInfo {
	unsigned int	complainInPostProcess:1;
	unsigned int	changeUserID:1;
	unsigned int	changeGroupID:1;
	unsigned int	changeMode:1;
	unsigned int	create:1;
	unsigned int	force:1;
	unsigned int	recursive:1;
	unsigned int	processDirectoriesAfterTheirContents;
	unsigned int	makeParentDirectories:1;
	unsigned int	didOperation:1;
	unsigned int	isSymbolicLink:1;
	unsigned int	makeSymbolicLink:1;
	unsigned int	dyadic:1;
	const char*	source;
	const char*	destination;
	int				directoryLength;
	uid_t			userID;
	gid_t			groupID;
	mode_t			andWithMode;
	mode_t			orWithMode;
	struct stat		stat;
	const struct Applet *
					applet;
};

struct Applet {
	const	char*	name;
	int	(*main)(int argc, char** argv);
};

extern void	name_and_error(const char*);
extern int	is_a_directory(const char*);
extern char*	join_paths(char *, const char *, const char *);

extern int	descend(
		 struct FileInfo *o
		,int 		(*function)(const struct FileInfo * i));

extern struct mntent *
		findMountPoint(const char*, const char *);

extern void usage(const char*);
extern int busybox_main(int argc, char** argv);
extern int block_device_main(int argc, char** argv);
extern int cat_more_main(int argc, char** argv);
extern int chgrp_main(int argc, char** argv);
extern int chmod_main(int argc, char** argv);
extern int chown_main(int argc, char** argv);
extern int chroot_main(int argc, char** argv);
extern int clear_main(int argc, char** argv);
extern int date_main(int argc, char** argv);
extern int dd_main(int argc, char** argv);
extern int df_main(int argc, char** argv);
extern int dmesg_main(int argc, char** argv);
extern int dyadic_main(int argc, char** argv);
extern int false_main(int argc, char** argv);
extern int fdisk_main(int argc, char** argv);
extern int find_main(int argc, char** argv);
extern int grep_main(int argc, char** argv);
extern int halt_main(int argc, char** argv);
extern int init_main(int argc, char** argv);
extern int kill_main(int argc, char** argv);
extern int length_main(int argc, char** argv);
extern int ln_main(int argc, char** argv);
extern int loadkmap_main(int argc, char** argv);
extern int losetup_main(int argc, char** argv);
extern int ls_main(int argc, char** argv);
extern int makedevs_main(int argc, char** argv);
extern int math_main(int argc, char** argv);
extern int mknod_main(int argc, char** argv);
extern int mkswap_main(int argc, char** argv);
extern int mnc_main(int argc, char** argv);
extern int monadic_main(int argc, char** argv);
extern int mount_main(int argc, char** argv);
extern int mt_main(int argc, char** argv);
extern int printf_main(int argc, char** argv);
extern int pwd_main(int argc, char** argv);
extern int reboot_main(int argc, char** argv);
extern int rm_main(int argc, char** argv);
extern int scan_partitions_main(int argc, char** argv);
extern int sh_main(int argc, char** argv);
extern int sleep_main(int argc, char** argv);
extern int tar_main(int argc, char** argv);
extern int sync_main(int argc, char** argv);
extern int tput_main(int argc, char** argv);
extern int true_main(int argc, char** argv);
extern int tryopen_main(int argc, char** argv);
extern int umount_main(int argc, char** argv);
extern int update_main(int argc, char** argv);
extern int zcat_main(int argc, char** argv);
extern int gzip_main(int argc, char** argv);

extern int
parse_mode(
 const char*	s
,mode_t *		or
,mode_t *		and
,int *			group_execute);

extern int		parse_user_name(const char* string, struct FileInfo * i);

extern const char	block_device_usage[];
extern const char	chgrp_usage[];
extern const char	chmod_usage[];
extern const char	chown_usage[];
extern const char	chroot_usage[];
extern const char	clear_usage[];
extern const char	cp_usage[];
extern const char	date_usage[];
extern const char	dd_usage[];
extern const char	df_usage[];
extern const char	dmesg_usage[];
extern const char	dutmp_usage[];
extern const char	false_usage[];
extern const char	fdflush_usage[];
extern const char	find_usage[];
extern const char	grep_usage[];
extern const char	halt_usage[];
extern const char	init_usage[];
extern const char	kill_usage[];
extern const char	length_usage[];
extern const char	ln_usage[];
extern const char	loadkmap_usage[];
extern const char	losetup_usage[];
extern const char	ls_usage[];
extern const char	math_usage[];
extern const char	makedevs_usage[];
extern const char	mkdir_usage[];
extern const char	mknod_usage[];
extern const char	mkswap_usage[];
extern const char	mnc_usage[];
extern const char	more_usage[];
extern const char	mount_usage[];
extern const char	mt_usage[];
extern const char	mv_usage[];
extern const char	printf_usage[];
extern const char	pwd_usage[];
extern const char	reboot_usage[];
extern const char	rm_usage[];
extern const char	rmdir_usage[];
extern const char	scan_partitions_usage[];
extern const char	sleep_usage[];
extern const char	tar_usage[];
extern const char	swapoff_usage[];
extern const char	swapon_usage[];
extern const char	sync_usage[];
extern const char	touch_usage[];
extern const char	tput_usage[];
extern const char	true_usage[];
extern const char	tryopen_usage[];
extern const char	umount_usage[];
extern const char	update_usage[];
extern const char	zcat_usage[];
extern const char	gzip_usage[];



#endif

