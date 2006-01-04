/* vi:set ts=4:
 * 
 * mdev - Mini udev for busybox
 * 
 * Copyright 2005 Rob Landley <rob@landley.net>
 * Copyright 2005 Frank Sorenson <frank@tuxrocks.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "busybox.h"
#include "xregex.h"

#define DEV_PATH	"/dev"

#include <busybox.h>

/* mknod in /dev based on a path like "/sys/block/hda/hda1" */
static void make_device(char *path)
{
	char *device_name,*s;
	int major,minor,type,len,fd;
	int mode=0660;
	uid_t uid=0;
	gid_t gid=0;

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
	for (s = temp; *s; s++) {
		if (*s == ':') {
			major = minor;
			minor = 0;
		} else {
			minor *= 10;
			minor += (*s) - '0';
		}
	}

	/* If we have a config file, look up permissions for this device */
	
	if (ENABLE_FEATURE_MDEV_CONF) {
		char *conf,*pos,*end;

		/* mmap the config file */
		if (-1!=(fd=open("/etc/mdev.conf",O_RDONLY))) {
			len=lseek(fd,0,SEEK_END);
			conf=mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
			if (conf) {
				int line=0;

				/* Loop through lines in mmaped file*/
				for (pos=conf;pos-conf<len;) {
					int field;
					char *end2;
					
					line++;
					/* find end of this line */
					for(end=pos;end-conf<len && *end!='\n';end++);

					/* Three fields: regex, uid:gid, mode */
					for (field=3;field;field--) {
						/* Skip whitespace */
						while (pos<end && isspace(*pos)) pos++;
						if (pos==end || *pos=='#') break;
						for (end2=pos;
							end2<end && !isspace(*end2) && *end2!='#'; end2++);
						switch(field) {
							/* Regex to match this device */
							case 3:
							{
								char *regex=strndupa(pos,end2-pos);
								regex_t match;
								regmatch_t off;
								int result;

								/* Is this it? */	
								xregcomp(&match,regex,REG_EXTENDED);
								result=regexec(&match,device_name,1,&off,0);
								regfree(&match);
								
								/* If not this device, skip rest of line */
								if(result || off.rm_so
									|| off.rm_eo!=strlen(device_name))
										goto end_line;

								break;
							}
							/* uid:gid */
							case 2:
							{
								char *s2;

								/* Find : */
								for(s=pos;s<end2 && *s!=':';s++);
								if(s==end2) goto end_line;

								/* Parse UID */
								uid=strtoul(pos,&s2,10);
								if(s!=s2) {
									struct passwd *pass;
									pass=getpwnam(strndupa(pos,s-pos));
									if(!pass) goto end_line;
									uid=pass->pw_uid;
								}
								s++;
								/* parse GID */
								gid=strtoul(s,&s2,10);
								if(end2!=s2) {
									struct group *grp;
									grp=getgrnam(strndupa(s,end2-s));
									if(!grp) goto end_line;
									gid=grp->gr_gid;
								}
								break;
							}
							/* mode */
							case 1:
							{
								mode=strtoul(pos,&pos,8);
								if(pos!=end2) goto end_line;
								goto found_device;
							}
						}
						pos=end2;
					}
end_line:
					/* Did everything parse happily? */
					if (field && field!=3)
							bb_error_msg_and_die("Bad line %d",line);

					/* Next line */
					pos=++end;
				}
found_device:
				munmap(conf,len);
			}
			close(fd);
		}
	}

	sprintf(temp, "%s/%s", DEV_PATH, device_name);
	umask(0);
	if (mknod(temp, mode | type, makedev(major, minor)) && errno != EEXIST)
		bb_perror_msg_and_die("mknod %s failed", temp);

	if (ENABLE_FEATURE_MDEV_CONF) chown(temp,uid,gid);
	
end:
	RELEASE_CONFIG_BUFFER(temp);
}

/* Recursive search of /sys/block or /sys/class.  path must be a writeable
 * buffer of size PATH_MAX containing the directory string to start at. */

static void find_dev(char *path)
{
	DIR *dir;
	int len=strlen(path);

	if (!(dir = opendir(path)))
		bb_perror_msg_and_die("No %s",path);

	for (;;) {
		struct dirent *entry = readdir(dir);
		
		if (!entry) break;

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
		if (argc == 2 && !strcmp(argv[1],"-s")) {
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
