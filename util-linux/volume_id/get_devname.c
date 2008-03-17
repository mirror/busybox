/* vi: set sw=4 ts=4: */
/*
 * Support functions for mounting devices by label/uuid
 *
 * Copyright (C) 2006 by Jason Schoon <floydpink@gmail.com>
 * Some portions cribbed from e2fsprogs, util-linux, dosfstools
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "volume_id_internal.h"

#define BLKGETSIZE64 _IOR(0x12,114,size_t)

#define PROC_PARTITIONS "/proc/partitions"
#define PROC_CDROMS	"/proc/sys/dev/cdrom/info"
#define DEVLABELDIR	"/dev"
#define SYS_BLOCK	"/sys/block"

static struct uuidCache_s {
	struct uuidCache_s *next;
	char uuid[16];
	char *device;
	char *label;
	int major, minor;
} *uuidCache;

/* for now, only ext2, ext3 and xfs are supported */
#if !ENABLE_FEATURE_VOLUMEID_ISO9660
#define get_label_uuid(device, label, uuid, iso_only) \
	get_label_uuid(device, label, uuid)
#endif
static int
get_label_uuid(const char *device, char **label, char **uuid, int iso_only)
{
	int rv = 1;
	uint64_t size;
	struct volume_id *vid;

	vid = volume_id_open_node(device);

	if (ioctl(vid->fd, BLKGETSIZE64, &size) != 0) {
		size = 0;
	}

#if ENABLE_FEATURE_VOLUMEID_ISO9660
	if (iso_only ?
	    volume_id_probe_iso9660(vid, 0) != 0 :
	    volume_id_probe_all(vid, 0, size) != 0) {
		goto ret;
	}
#else
	if (volume_id_probe_all(vid, 0, size) != 0) {
		goto ret;
	}
#endif

	if (vid->label[0] != '\0') {
		*label = xstrndup(vid->label, sizeof(vid->label));
		*uuid  = xstrndup(vid->uuid, sizeof(vid->uuid));
		printf("Found label %s on %s (uuid:%s)\n", *label, device, *uuid);
		rv = 0;
	}
 ret:
	free_volume_id(vid);
	return rv;
}

static void
uuidcache_addentry(char * device, int major, int minor, char *label, char *uuid)
{
	struct uuidCache_s *last;
    
	if (!uuidCache) {
		last = uuidCache = xzalloc(sizeof(*uuidCache));
	} else {
		for (last = uuidCache; last->next; last = last->next)
			continue;
		last->next = xzalloc(sizeof(*uuidCache));
		last = last->next;
	}
	/*last->next = NULL; - xzalloc did it*/
	last->label = label;
	last->device = device;
	last->major = major;
	last->minor = minor;
	memcpy(last->uuid, uuid, sizeof(last->uuid));
}

#if !ENABLE_FEATURE_VOLUMEID_ISO9660
#define uuidcache_check_device(device_name, ma, mi, iso_only) \
	uuidcache_check_device(device_name, ma, mi)
#endif
static void
uuidcache_check_device(const char *device_name, int ma, int mi, int iso_only)
{
	char device[110];
	char *uuid = NULL, *label = NULL;
	char *ptr;
	char *deviceDir = NULL;
	int mustRemove = 0;
	int mustRemoveDir = 0;
	int i;

	sprintf(device, "%s/%s", DEVLABELDIR, device_name);
	if (access(device, F_OK)) {
		ptr = device;
		i = 0;
		while (*ptr)
			if (*ptr++ == '/')
				i++;
		if (i > 2) {
			deviceDir = alloca(strlen(device) + 1);
			strcpy(deviceDir, device);
			ptr = deviceDir + (strlen(device) - 1);
			while (*ptr != '/')
				*ptr-- = '\0';
			if (mkdir(deviceDir, 0644)) {
				printf("mkdir: cannot create directory %s: %d\n", deviceDir, errno);
			} else {
				mustRemoveDir = 1;
			}
		}

		mknod(device, S_IFBLK | 0600, makedev(ma, mi));
		mustRemove = 1;
	}
	if (!get_label_uuid(device, &label, &uuid, iso_only))
		uuidcache_addentry(strdup(device), ma, mi, 
				   label, uuid);

	if (mustRemove) unlink(device);
	if (mustRemoveDir) rmdir(deviceDir);
}

static void
uuidcache_init_partitions(void)
{
	char line[100];
	int ma, mi;
	unsigned long long sz;
	FILE *procpt;
	int firstPass;
	int handleOnFirst;
	char *chptr;

	procpt = xfopen(PROC_PARTITIONS, "r");
/*
# cat /proc/partitions
major minor  #blocks  name

   8     0  293036184 sda
   8     1    6835626 sda1
   8     2          1 sda2
   8     5     979933 sda5
   8     6   15623181 sda6
   8     7   97659103 sda7
   8     8  171935631 sda8
*/
	for (firstPass = 1; firstPass >= 0; firstPass--) {
		fseek(procpt, 0, SEEK_SET);

		while (fgets(line, sizeof(line), procpt)) {
			/* The original version of this code used sscanf, but
			   diet's sscanf is quite limited */
			chptr = line;
			if (*chptr != ' ') continue;
			chptr++;

			ma = bb_strtou(chptr, &chptr, 0);
			if (ma < 0) continue;
			chptr = skip_whitespace(chptr);

			mi = bb_strtou(chptr, &chptr, 0);
			if (mi < 0) continue;
			chptr = skip_whitespace(chptr);

			sz = bb_strtoull(chptr, &chptr, 0);
			if ((long long)sz == -1LL) continue;
			chptr = skip_whitespace(chptr);

			/* skip extended partitions (heuristic: size 1) */
			if (sz == 1)
				continue;

			*strchrnul(chptr, '\n') = '\0';
			/* now chptr => device name */
			if (!chptr[0])
				continue;

			/* look only at md devices on first pass */
			handleOnFirst = (chptr[0] == 'm' && chptr[1] == 'd');
			if (firstPass != handleOnFirst)
				continue;

			/* heuristic: partition name ends in a digit */
			if (isdigit(chptr[strlen(chptr) - 1])) {
				uuidcache_check_device(chptr, ma, mi, 0);
			}
		}
	}

	fclose(procpt);
}

static int
dev_get_major_minor(char *device_name, int *major, int *minor)
{
	char * dev_path;
	int fd;
	char dev[7];
	char *major_ptr, *minor_ptr;

	dev_path = alloca(strlen(SYS_BLOCK) + strlen(device_name) + 6);
	sprintf(dev_path, "%s/%s/dev", SYS_BLOCK, device_name);

	fd = open(dev_path, O_RDONLY);
	if (fd < 0) return 1;
	full_read(fd, dev, sizeof(dev));
	close(fd);

	major_ptr = dev;
	minor_ptr = strchr(dev, ':');
	if (!minor_ptr) return 1;
	*minor_ptr++ = '\0';

	*major = strtol(major_ptr, NULL, 10);
	*minor = strtol(minor_ptr, NULL, 10);

	return 0;
}

static void
uuidcache_init_cdroms(void)
{
	char line[100];
	int ma, mi;
	FILE *proccd;

	proccd = fopen(PROC_CDROMS, "r");
	if (!proccd) {
		static smallint warn = 0;
		if (!warn) {
			warn = 1;
			bb_error_msg("mount: could not open %s, so UUID and LABEL "
				"conversion cannot be done for CD-Roms.",
				PROC_CDROMS);
		}
		return;
	}

	while (fgets(line, sizeof(line), proccd)) {
		const char *drive_name_string = "drive name:\t\t";
		if (!strncmp(line, drive_name_string, strlen(drive_name_string))) {
			char *device_name;
			device_name = strtok(line + strlen(drive_name_string), "\t\n");
			while (device_name) {
				dev_get_major_minor(device_name, &ma, &mi);
				uuidcache_check_device(device_name, ma, mi, 1);
				device_name = strtok(NULL, "\t\n");
			}
			break;
		}
	}

	fclose(proccd);
}

static void
uuidcache_init(void)
{
	if (uuidCache)
		return;

	uuidcache_init_partitions();
	uuidcache_init_cdroms();
}

#define UUID   1
#define VOL    2

#ifdef UNUSED
static char *
get_spec_by_x(int n, const char *t, int * majorPtr, int * minorPtr)
{
	struct uuidCache_s *uc;

	uuidcache_init();
	uc = uuidCache;

	while(uc) {
		switch (n) {
		case UUID:
			if (!memcmp(t, uc->uuid, sizeof(uc->uuid))) {
				*majorPtr = uc->major;
				*minorPtr = uc->minor;
				return uc->device;
			}
			break;
		case VOL:
			if (!strcmp(t, uc->label)) {
				*majorPtr = uc->major;
				*minorPtr = uc->minor;
				return uc->device;
			}
			break;
		}
		uc = uc->next;
	}
	return NULL;
}

static unsigned char
fromhex(char c)
{
	if (isdigit(c))
		return (c - '0');
	if (islower(c))
		return (c - 'a' + 10);
	return (c - 'A' + 10);
}

static char *
get_spec_by_uuid(const char *s, int * major, int * minor)
{
	unsigned char uuid[16];
	int i;

	if (strlen(s) != 36 ||
	    s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-')
		goto bad_uuid;
	for (i=0; i<16; i++) {
	    if (*s == '-') s++;
	    if (!isxdigit(s[0]) || !isxdigit(s[1]))
		    goto bad_uuid;
	    uuid[i] = ((fromhex(s[0])<<4) | fromhex(s[1]));
	    s += 2;
	}
	return get_spec_by_x(UUID, (char *)uuid, major, minor);

 bad_uuid:
	fprintf(stderr, _("mount: bad UUID"));
	return 0;
}

static char *
get_spec_by_volume_label(const char *s, int *major, int *minor)
{
	return get_spec_by_x(VOL, s, major, minor);
}

static int display_uuid_cache(void)
{
	struct uuidCache_s *u;
	size_t i;

	uuidcache_init();

	u = uuidCache;
	while (u) {
		printf("%s %s ", u->device, u->label);
		for (i = 0; i < sizeof(u->uuid); i++) {
			if (i == 4 || i == 6 || i == 8 || i == 10)
				printf("-");
			printf("%x", u->uuid[i] & 0xff);
		}
		printf("\n");
		u = u->next;
	}

	return 0;
}
#endif // UNUSED


/* Used by mount and findfs */

char *get_devname_from_label(const char *spec)
{
	struct uuidCache_s *uc;
	int spec_len = strlen(spec);

	uuidcache_init();
	uc = uuidCache;
	while (uc) {
		if (uc->label && !strncmp(spec, uc->label, spec_len)) {
			return xstrdup(uc->device);
		}
		uc = uc->next;
	}
	return NULL;
}

char *get_devname_from_uuid(const char *spec)
{
	struct uuidCache_s *uc;

	uuidcache_init();
	uc = uuidCache;
	while (uc) {
		if (!memcmp(spec, uc->uuid, sizeof(uc->uuid))) {
			return xstrdup(uc->device);
		}
		uc = uc->next;
	}
	return NULL;
}
