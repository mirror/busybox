/* vi:set ts=4:
 * 
 * mdev - Mini udev for busybox
 * 
 * Copyright 2005 Rob Landley <rob@landley.net>
 * Copyright 2005 Frank Sorenson <frank@tuxrocks.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEV_PATH	"/dev"
#define DEV_MODE	0660

#include <busybox.h>

/* mknod in /dev based on a path like "/sys/block/hda/hda1" */
void make_device(char *path)
{
	char *device_name, *s;
	int major,minor,type,len,fd;

	RESERVE_CONFIG_BUFFER(temp,PATH_MAX);

	/* Try to read major/minor string */
	
	snprintf(temp, PATH_MAX, "%s/dev", path);
	fd = open(temp, O_RDONLY);
	len = read(fd, temp, PATH_MAX-1);
	if (len<1) goto end;
	temp[len] = 0;
	close(fd);
	
	/* Determine device name, type, major and minor */
	
	device_name = strrchr(path, '/') + 1;
	type = strncmp(path+5, "block/" ,6) ? S_IFCHR : S_IFBLK;
	major = minor = 0;
	for(s = temp; *s; s++) {
		if(*s == ':') {
			major = minor;
			minor = 0;
		} else {
			minor *= 10;
			minor += (*s) - '0';
		}
	}

/* Open config file here, look up permissions */	

	sprintf(temp, "%s/%s", DEV_PATH, device_name);
	if(mknod(temp, DEV_MODE | type, makedev(major, minor)) && errno != EEXIST)
		bb_perror_msg_and_die("mknod %s failed", temp);
	
/* Perform shellout here */

end:
	RELEASE_CONFIG_BUFFER(temp);
}

/* Recursive search of /sys/block or /sys/class.  path must be a writeable
 * buffer of size PATH_MAX containing the directory string to start at. */

void find_dev(char *path)
{
	DIR *dir;
	int len=strlen(path);

	if(!(dir = opendir(path)))
		bb_perror_msg_and_die("No %s",path);

	for(;;) {
		struct dirent *entry = readdir(dir);
		
		if(!entry) break;

		/* Skip "." and ".." (also skips hidden files, which is ok) */

		if (entry->d_name[0]=='.') continue;

		if (entry->d_type == DT_DIR) {
			snprintf(path+len, PATH_MAX-len, "/%s", entry->d_name);
			find_dev(path);
			path[len] = 0;
		}
		
		/* If there's a dev entry, mknod it */
		
		if (strcmp(entry->d_name, "dev")) make_device(path);
	}
	
	closedir(dir);
}

int mdev_main(int argc, char *argv[])
{
	if (argc > 1) {
		if(argc == 2 && !strcmp(argv[1],"-s")) {
			RESERVE_CONFIG_BUFFER(temp,PATH_MAX);
			strcpy(temp,"/sys/block");
			find_dev(temp);
			strcpy(temp,"/sys/class");
			find_dev(temp);
			if(ENABLE_FEATURE_CLEAN_UP)
				RELEASE_CONFIG_BUFFER(temp);
			return 0;
		} else bb_show_usage();
	} 
	
/* hotplug support goes here */
	
	return 0;
}
