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

//#define BLKGETSIZE64 _IOR(0x12,114,size_t)

static struct uuidCache_s {
	struct uuidCache_s *next;
//	int major, minor;
	char *device;
	char *label;
	char *uc_uuid; /* prefix makes it easier to grep for */
} *uuidCache;

/* Returns !0 on error.
 * Otherwise, returns malloc'ed strings for label and uuid
 * (and they can't be NULL, although they can be "") */
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
	if (!vid)
		return rv;

	if (ioctl(vid->fd, BLKGETSIZE64, &size) != 0)
		size = 0;

#if ENABLE_FEATURE_VOLUMEID_ISO9660
	if ((iso_only ?
	     volume_id_probe_iso9660(vid, 0) :
	     volume_id_probe_all(vid, 0, size)
	    ) != 0
	) {
		goto ret;
	}
#else
	if (volume_id_probe_all(vid, 0, size) != 0) {
		goto ret;
	}
#endif

	if (vid->label[0] != '\0' || vid->uuid[0] != '\0') {
		*label = xstrndup(vid->label, sizeof(vid->label));
		*uuid  = xstrndup(vid->uuid, sizeof(vid->uuid));
		dbg("found label '%s', uuid '%s' on %s", *label, *uuid, device);
		rv = 0;
	}
 ret:
	free_volume_id(vid);
	return rv;
}

/* NB: we take ownership of (malloc'ed) label and uuid */
static void
uuidcache_addentry(char *device, /*int major, int minor,*/ char *label, char *uuid)
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
//	last->major = major;
//	last->minor = minor;
	last->device = device;
	last->label = label;
	last->uc_uuid = uuid;
}

/* If get_label_uuid() on device_name returns success,
 * add a cache entry for this device.
 * If device node does not exist, it will be temporarily created. */
#if !ENABLE_FEATURE_VOLUMEID_ISO9660
#define uuidcache_check_device(device_name, ma, mi, iso_only) \
	uuidcache_check_device(device_name, ma, mi)
#endif
static void
uuidcache_check_device(const char *device_name, int ma, int mi, int iso_only)
{
	char *device, *last_slash;
	char *uuid, *label;
	char *ptr;
	int must_remove = 0;
	int added = 0;

	last_slash = NULL;
	device = xasprintf("/dev/%s", device_name);
	if (access(device, F_OK) != 0) {
		/* device does not exist, temporarily create */
		int slash_cnt = 0;

		if ((ma | mi) < 0)
			goto ret; /* we don't know major:minor! */

		ptr = device;
		while (*ptr)
			if (*ptr++ == '/')
				slash_cnt++;
		if (slash_cnt > 2) {
// BUG: handles only slash_cnt == 3 case
			last_slash = strrchr(device, '/');
			*last_slash = '\0';
			if (mkdir(device, 0644)) {
				bb_perror_msg("can't create directory %s", device);
				*last_slash = '/';
				last_slash = NULL; /* prevents rmdir */
			} else {
				*last_slash = '/';
			}
		}
		mknod(device, S_IFBLK | 0600, makedev(ma, mi));
		must_remove = 1;
	}

	uuid = NULL;
	label = NULL;
	if (get_label_uuid(device, &label, &uuid, iso_only) == 0) {
		uuidcache_addentry(device, /*ma, mi,*/ label, uuid);
		/* "device" is owned by cache now, don't free */
		added = 1;
	}

	if (must_remove)
		unlink(device);
	if (last_slash) {
		*last_slash = '\0';
		rmdir(device);
	}
 ret:
	if (!added)
		free(device);
}

/* Run uuidcache_check_device() for every device mentioned
 * in /proc/partitions */
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

	procpt = xfopen("/proc/partitions", "r");
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
			chptr = skip_whitespace(chptr);

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
			dbg("/proc/partitions: maj:%d min:%d sz:%llu name:'%s'",
						ma, mi, sz, chptr);
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

static void
dev_get_major_minor(char *device_name, int *major, int *minor)
{
	char dev[16];
	char *dev_path;
	char *colon;
	int sz;

	dev_path = xasprintf("/sys/block/%s/dev", device_name);
	sz = open_read_close(dev_path, dev, sizeof(dev) - 1);
	if (sz < 0)
		goto ret;
	dev[sz] = '\0';

	colon = strchr(dev, ':');
	if (!colon)
		goto ret;
	*major = bb_strtou(dev, NULL, 10);
	*minor = bb_strtou(colon + 1, NULL, 10);

 ret:
	free(dev_path);
	return;
}

static void
uuidcache_init_cdroms(void)
{
#define PROC_CDROMS "/proc/sys/dev/cdrom/info"
	char line[100];
	int ma, mi;
	FILE *proccd;

	proccd = fopen(PROC_CDROMS, "r");
	if (!proccd) {
//		static smallint warn = 0;
//		if (!warn) {
//			warn = 1;
//			bb_error_msg("can't open %s, UUID and LABEL "
//				"conversion cannot be done for CD-Roms",
//				PROC_CDROMS);
//		}
		return;
	}

	while (fgets(line, sizeof(line), proccd)) {
		static const char drive_name_string[] ALIGN1 = "drive name:";

		if (strncmp(line, drive_name_string, sizeof(drive_name_string) - 1) == 0) {
			char *device_name;

			device_name = strtok(skip_whitespace(line + sizeof(drive_name_string) - 1), " \t\n");
			while (device_name && device_name[0]) {
				ma = mi = -1;
				dev_get_major_minor(device_name, &ma, &mi);
				uuidcache_check_device(device_name, ma, mi, 1);
				device_name = strtok(NULL, " \t\n");
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
get_spec_by_x(int n, const char *t, int *majorPtr, int *minorPtr)
{
	struct uuidCache_s *uc;

	uuidcache_init();
	uc = uuidCache;

	while (uc) {
		switch (n) {
		case UUID:
			if (strcmp(t, uc->uc_uuid) == 0) {
				*majorPtr = uc->major;
				*minorPtr = uc->minor;
				return uc->device;
			}
			break;
		case VOL:
			if (strcmp(t, uc->label) == 0) {
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
	return ((c|0x20) - 'a' + 10);
}

static char *
get_spec_by_uuid(const char *s, int *major, int *minor)
{
	unsigned char uuid[16];
	int i;

	if (strlen(s) != 36 || s[8] != '-' || s[13] != '-'
	 || s[18] != '-' || s[23] != '-'
	) {
		goto bad_uuid;
	}
	for (i = 0; i < 16; i++) {
		if (*s == '-')
			s++;
		if (!isxdigit(s[0]) || !isxdigit(s[1]))
			goto bad_uuid;
		uuid[i] = ((fromhex(s[0]) << 4) | fromhex(s[1]));
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
		printf("%s %s %s\n", u->device, u->label, u->uc_uuid);
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
// FIXME: empty label ("LABEL=") matches anything??!
		if (uc->label[0] && strncmp(spec, uc->label, spec_len) == 0) {
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
		/* case of hex numbers doesn't matter */
		if (strcasecmp(spec, uc->uc_uuid) == 0) {
			return xstrdup(uc->device);
		}
		uc = uc->next;
	}
	return NULL;
}
