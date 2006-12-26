/* vi: set sw=4 ts=4: */
/*
 * See README for additional information
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

#include "libbb.h"
#include "e2fs_lib.h"

#define HAVE_EXT2_IOCTLS 1

#if INT_MAX == LONG_MAX
#define IF_LONG_IS_SAME(x) x
#define IF_LONG_IS_WIDER(x)
#else
#define IF_LONG_IS_SAME(x)
#define IF_LONG_IS_WIDER(x) x
#endif

static void close_silently(int fd)
{
	int e = errno;
	close(fd);
	errno = e;
}


/* Iterate a function on each entry of a directory */
int iterate_on_dir(const char *dir_name,
		int (*func)(const char *, struct dirent *, void *),
		void * private)
{
	DIR *dir;
	struct dirent *de, *dep;
	int max_len, len;

	max_len = PATH_MAX + sizeof(struct dirent);
	de = xmalloc(max_len+1);
	memset(de, 0, max_len+1);

	dir = opendir(dir_name);
	if (dir == NULL) {
		free(de);
		return -1;
	}
	while ((dep = readdir(dir))) {
		len = sizeof(struct dirent);
		if (len < dep->d_reclen)
			len = dep->d_reclen;
		if (len > max_len)
			len = max_len;
		memcpy(de, dep, len);
		func(dir_name, de, private);
	}
	closedir(dir);
	free(de);
	return 0;
}


/* Get/set a file version on an ext2 file system */
int fgetsetversion(const char *name, unsigned long *get_version, unsigned long set_version)
{
#if HAVE_EXT2_IOCTLS
	int fd, r;
	IF_LONG_IS_WIDER(int ver;)

	fd = open(name, O_NONBLOCK);
	if (fd == -1)
		return -1;
	if (!get_version) {
		IF_LONG_IS_WIDER(
			ver = (int) set_version;
			r = ioctl(fd, EXT2_IOC_SETVERSION, &ver);
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_SETVERSION, (void*)&set_version);
		)
	} else {
		IF_LONG_IS_WIDER(
			r = ioctl(fd, EXT2_IOC_GETVERSION, &ver);
			*get_version = ver;
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_GETVERSION, (void*)get_version);
		)
	}
	close_silently(fd);
	return r;
#else /* ! HAVE_EXT2_IOCTLS */
	errno = EOPNOTSUPP;
	return -1;
#endif /* ! HAVE_EXT2_IOCTLS */
}


/* Get/set a file flags on an ext2 file system */
int fgetsetflags(const char *name, unsigned long *get_flags, unsigned long set_flags)
{
#if HAVE_EXT2_IOCTLS
	struct stat buf;
	int fd, r;
	IF_LONG_IS_WIDER(int f;)

	if (stat(name, &buf) == 0 /* stat is ok */
	 && !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode)
	) {
		goto notsupp;
	}
	fd = open(name, O_NONBLOCK); /* neither read nor write asked for */
	if (fd == -1)
		return -1;

	if (!get_flags) {
		IF_LONG_IS_WIDER(
			f = (int) set_flags;
			r = ioctl(fd, EXT2_IOC_SETFLAGS, &f);
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_SETFLAGS, (void*)&set_flags);
		)
	} else {
		IF_LONG_IS_WIDER(
			r = ioctl(fd, EXT2_IOC_GETFLAGS, &f);
			*get_flags = f;
		)
		IF_LONG_IS_SAME(
			r = ioctl(fd, EXT2_IOC_GETFLAGS, (void*)get_flags);
		)
	}

	close_silently(fd);
	return r;
 notsupp:
#endif /* HAVE_EXT2_IOCTLS */
	errno = EOPNOTSUPP;
	return -1;
}


/* Print file attributes on an ext2 file system */
struct flags_name {
	unsigned long	flag;
	char		short_name;
	const char	*long_name;
};

static const struct flags_name flags_array[] = {
	{ EXT2_SECRM_FL, 's', "Secure_Deletion" },
	{ EXT2_UNRM_FL, 'u' , "Undelete" },
	{ EXT2_SYNC_FL, 'S', "Synchronous_Updates" },
	{ EXT2_DIRSYNC_FL, 'D', "Synchronous_Directory_Updates" },
	{ EXT2_IMMUTABLE_FL, 'i', "Immutable" },
	{ EXT2_APPEND_FL, 'a', "Append_Only" },
	{ EXT2_NODUMP_FL, 'd', "No_Dump" },
	{ EXT2_NOATIME_FL, 'A', "No_Atime" },
	{ EXT2_COMPR_FL, 'c', "Compression_Requested" },
#ifdef ENABLE_COMPRESSION
	{ EXT2_COMPRBLK_FL, 'B', "Compressed_File" },
	{ EXT2_DIRTY_FL, 'Z', "Compressed_Dirty_File" },
	{ EXT2_NOCOMPR_FL, 'X', "Compression_Raw_Access" },
	{ EXT2_ECOMPR_FL, 'E', "Compression_Error" },
#endif
	{ EXT3_JOURNAL_DATA_FL, 'j', "Journaled_Data" },
	{ EXT2_INDEX_FL, 'I', "Indexed_directory" },
	{ EXT2_NOTAIL_FL, 't', "No_Tailmerging" },
	{ EXT2_TOPDIR_FL, 'T', "Top_of_Directory_Hierarchies" },
	{ 0, '\0', NULL }
};

void print_flags(FILE *f, unsigned long flags, unsigned options)
{
	const struct flags_name *fp;

	if (options & PFOPT_LONG) {
		int first = 1;
		for (fp = flags_array; fp->short_name; fp++) {
			if (flags & fp->flag) {
				if (!first)
					fputs(", ", f);
				fputs(fp->long_name, f);
				first = 0;
			}
		}
		if (first)
			fputs("---", f);
	} else {
		for (fp = flags_array; fp->short_name; fp++) {
			char c = '-';
			if (flags & fp->flag)
				c = fp->short_name;
			fputc(c, f);
		}
	}
}
