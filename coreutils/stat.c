/*
 * stat -- display file or file system status
 *
 * Copyright (C) 2001, 2002, 2003, 2004, 2005 Free Software Foundation.
 * Copyright (C) 2005 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Mike Frysinger <vapier@gentoo.org>
 *
 * Written by Michael Meskes
 * Taken from coreutils and turned into a busybox applet by Mike Frysinger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/vfs.h>
#include <time.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include "busybox.h"

/* vars to control behavior */
static int follow_links = 0;
static int terse = 0;

static char const *file_type(struct stat const *st)
{
	/* See POSIX 1003.1-2001 XCU Table 4-8 lines 17093-17107 
	 * for some of these formats.
	 * To keep diagnostics grammatical in English, the 
	 * returned string must start with a consonant.
	 */
	if (S_ISREG(st->st_mode))  return st->st_size == 0 ? "regular empty file" : "regular file";
	if (S_ISDIR(st->st_mode))  return "directory";
	if (S_ISBLK(st->st_mode))  return "block special file";
	if (S_ISCHR(st->st_mode))  return "character special file";
	if (S_ISFIFO(st->st_mode)) return "fifo";
	if (S_ISLNK(st->st_mode))  return "symbolic link";
	if (S_ISSOCK(st->st_mode)) return "socket";
	if (S_TYPEISMQ(st))        return "message queue";
	if (S_TYPEISSEM(st))       return "semaphore";
	if (S_TYPEISSHM(st))       return "shared memory object";
#ifdef S_TYPEISTMO
	if (S_TYPEISTMO(st))       return "typed memory object";
#endif
	return "weird file";
}

static char const *human_time(time_t t)
{
	static char *str;
	str = ctime(&t);
	str[strlen(str)-1] = '\0';
	return str;
}

/* Return the type of the specified file system.
 * Some systems have statfvs.f_basetype[FSTYPSZ]. (AIX, HP-UX, and Solaris)
 * Others have statfs.f_fstypename[MFSNAMELEN]. (NetBSD 1.5.2)
 * Still others have neither and have to get by with f_type (Linux).
 */
static char const *human_fstype(long f_type)
{
#define S_MAGIC_AFFS 0xADFF
#define S_MAGIC_DEVPTS 0x1CD1
#define S_MAGIC_EXT 0x137D
#define S_MAGIC_EXT2_OLD 0xEF51
#define S_MAGIC_EXT2 0xEF53
#define S_MAGIC_JFS 0x3153464a
#define S_MAGIC_XFS 0x58465342
#define S_MAGIC_HPFS 0xF995E849
#define S_MAGIC_ISOFS 0x9660
#define S_MAGIC_ISOFS_WIN 0x4000
#define S_MAGIC_ISOFS_R_WIN 0x4004
#define S_MAGIC_MINIX 0x137F
#define S_MAGIC_MINIX_30 0x138F
#define S_MAGIC_MINIX_V2 0x2468
#define S_MAGIC_MINIX_V2_30 0x2478
#define S_MAGIC_MSDOS 0x4d44
#define S_MAGIC_FAT 0x4006
#define S_MAGIC_NCP 0x564c
#define S_MAGIC_NFS 0x6969
#define S_MAGIC_PROC 0x9fa0
#define S_MAGIC_SMB 0x517B
#define S_MAGIC_XENIX 0x012FF7B4
#define S_MAGIC_SYSV4 0x012FF7B5
#define S_MAGIC_SYSV2 0x012FF7B6
#define S_MAGIC_COH 0x012FF7B7
#define S_MAGIC_UFS 0x00011954
#define S_MAGIC_XIAFS 0x012FD16D
#define S_MAGIC_NTFS 0x5346544e
#define S_MAGIC_TMPFS 0x1021994
#define S_MAGIC_REISERFS 0x52654973
#define S_MAGIC_CRAMFS 0x28cd3d45
#define S_MAGIC_ROMFS 0x7275
#define S_MAGIC_RAMFS 0x858458f6
#define S_MAGIC_SQUASHFS 0x73717368
#define S_MAGIC_SYSFS 0x62656572
	switch (f_type) {
		case S_MAGIC_AFFS:        return "affs";
		case S_MAGIC_DEVPTS:      return "devpts";
		case S_MAGIC_EXT:         return "ext";
		case S_MAGIC_EXT2_OLD:    return "ext2";
		case S_MAGIC_EXT2:        return "ext2/ext3";
		case S_MAGIC_JFS:         return "jfs";
		case S_MAGIC_XFS:         return "xfs";
		case S_MAGIC_HPFS:        return "hpfs";
		case S_MAGIC_ISOFS:       return "isofs";
		case S_MAGIC_ISOFS_WIN:   return "isofs";
		case S_MAGIC_ISOFS_R_WIN: return "isofs";
		case S_MAGIC_MINIX:       return "minix";
		case S_MAGIC_MINIX_30:    return "minix (30 char.)";
		case S_MAGIC_MINIX_V2:    return "minix v2";
		case S_MAGIC_MINIX_V2_30: return "minix v2 (30 char.)";
		case S_MAGIC_MSDOS:       return "msdos";
		case S_MAGIC_FAT:         return "fat";
		case S_MAGIC_NCP:         return "novell";
		case S_MAGIC_NFS:         return "nfs";
		case S_MAGIC_PROC:        return "proc";
		case S_MAGIC_SMB:         return "smb";
		case S_MAGIC_XENIX:       return "xenix";
		case S_MAGIC_SYSV4:       return "sysv4";
		case S_MAGIC_SYSV2:       return "sysv2";
		case S_MAGIC_COH:         return "coh";
		case S_MAGIC_UFS:         return "ufs";
		case S_MAGIC_XIAFS:       return "xia";
		case S_MAGIC_NTFS:        return "ntfs";
		case S_MAGIC_TMPFS:       return "tmpfs";
		case S_MAGIC_REISERFS:    return "reiserfs";
		case S_MAGIC_CRAMFS:      return "cramfs";
		case S_MAGIC_ROMFS:       return "romfs";
		case S_MAGIC_RAMFS:       return "ramfs";
		case S_MAGIC_SQUASHFS:    return "squashfs";
		case S_MAGIC_SYSFS:       return "sysfs";
		default: {
			static char buf[sizeof("UNKNOWN (0x%lx)") - 3
			                + (sizeof(f_type) * CHAR_BIT + 3) / 4];
			sprintf(buf, "UNKNOWN (0x%lx)", f_type);
			return buf;
		}
	}
}

#ifdef CONFIG_FEATURE_STAT_FORMAT
/* print statfs info */
static void print_statfs(char *pformat, size_t buf_len, char m, 
                         char const *filename, void const *data)
{
	struct statfs const *statfsbuf = data;

	switch (m) {
	case 'n':
		strncat(pformat, "s", buf_len);
		printf(pformat, filename);
		break;
	case 'i':
		strncat(pformat, "Lx", buf_len);
		printf(pformat, statfsbuf->f_fsid);
		break;
	case 'l':
		strncat(pformat, "lu", buf_len);
		printf(pformat, statfsbuf->f_namelen);
		break;
	case 't':
		strncat(pformat, "lx", buf_len);
		printf(pformat, (unsigned long int) (statfsbuf->f_type));  /* no equiv. */
		break;
	case 'T':
		strncat(pformat, "s", buf_len);
		printf(pformat, human_fstype(statfsbuf->f_type));
		break;
	case 'b':
		strncat(pformat, "ld", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_blocks));
		break;
	case 'f':
		strncat(pformat, "ld", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_bfree));
		break;
	case 'a':
		strncat(pformat, "ld", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_bavail));
		break;
	case 's':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) (statfsbuf->f_bsize));
		break;
	case 'S': {
		unsigned long int frsize = statfsbuf->f_frsize;
		if (!frsize)
			frsize = statfsbuf->f_bsize;
		strncat(pformat, "lu", buf_len);
		printf(pformat, frsize);
		break;
	}
	case 'c':
		strncat(pformat, "ld", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_files));
		break;
	case 'd':
		strncat(pformat, "ld", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_ffree));
		break;
	default:
		strncat(pformat, "c", buf_len);
		printf(pformat, m);
		break;
	}
}

/* print stat info */
static void print_stat(char *pformat, size_t buf_len, char m, 
                       char const *filename, void const *data)
{
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
	struct stat *statbuf = (struct stat *) data;
	struct passwd *pw_ent;
	struct group *gw_ent;

	switch (m) {
	case 'n':
		strncat(pformat, "s", buf_len);
		printf(pformat, filename);
		break;
	case 'N':
		strncat(pformat, "s", buf_len);
		if (S_ISLNK(statbuf->st_mode)) {
			char *linkname = xreadlink(filename);
			if (linkname == NULL) {
				bb_perror_msg("cannot read symbolic link '%s'", filename);
				return;
			}
			/*printf("\"%s\" -> \"%s\"", filename, linkname); */
			printf(pformat, filename);
			printf(" -> ");
			printf(pformat, linkname);
		} else {
			printf(pformat, filename);
		}
		break;
	case 'd':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (uintmax_t) statbuf->st_dev);
		break;
	case 'D':
		strncat(pformat, "lx", buf_len);
		printf(pformat, (uintmax_t) statbuf->st_dev);
		break;
	case 'i':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (uintmax_t) statbuf->st_ino);
		break;
	case 'a':
		strncat(pformat, "lo", buf_len);
		printf(pformat, (unsigned long int) (statbuf->st_mode & (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)));
		break;
	case 'A':
		strncat(pformat, "s", buf_len);
		printf(pformat, bb_mode_string(statbuf->st_mode));
		break;
	case 'f':
		strncat(pformat, "lx", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_mode);
		break;
	case 'F':
		strncat(pformat, "s", buf_len);
		printf(pformat, file_type(statbuf));
		break;
	case 'h':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_nlink);
		break;
	case 'u':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_uid);
		break;
	case 'U':
		strncat(pformat, "s", buf_len);
		setpwent();
		pw_ent = getpwuid(statbuf->st_uid);
		printf(pformat, (pw_ent != 0L) ? pw_ent->pw_name : "UNKNOWN");
		break;
	case 'g':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_gid);
		break;
	case 'G':
		strncat(pformat, "s", buf_len);
		setgrent();
		gw_ent = getgrgid(statbuf->st_gid);
		printf(pformat, (gw_ent != 0L) ? gw_ent->gr_name : "UNKNOWN");
		break;
	case 't':
		strncat(pformat, "lx", buf_len);
		printf(pformat, (unsigned long int) major(statbuf->st_rdev));
		break;
	case 'T':
		strncat(pformat, "lx", buf_len);
		printf(pformat, (unsigned long int) minor(statbuf->st_rdev));
		break;
	case 's':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (uintmax_t) (statbuf->st_size));
		break;
	case 'B':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) 512); //ST_NBLOCKSIZE
		break;
	case 'b':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (uintmax_t) statbuf->st_blocks);
		break;
	case 'o':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_blksize);
		break;
	case 'x':
		strncat(pformat, "s", buf_len);
		printf(pformat, human_time(statbuf->st_atime));
		break;
	case 'X':
		strncat(pformat, TYPE_SIGNED(time_t) ? "ld" : "lu", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_atime);
		break;
	case 'y':
		strncat(pformat, "s", buf_len);
		printf(pformat, human_time(statbuf->st_mtime));
		break;
	case 'Y':
		strncat(pformat, TYPE_SIGNED(time_t) ? "ld" : "lu", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_mtime);
		break;
	case 'z':
		strncat(pformat, "s", buf_len);
		printf(pformat, human_time(statbuf->st_ctime));
		break;
	case 'Z':
		strncat(pformat, TYPE_SIGNED(time_t) ? "ld" : "lu", buf_len);
		printf(pformat, (unsigned long int) statbuf->st_ctime);
		break;
	default:
		strncat(pformat, "c", buf_len);
		printf(pformat, m);
		break;
	}
}

static void print_it(char const *masterformat, char const *filename, 
                     void (*print_func) (char *, size_t, char, char const *, void const *), 
                     void const *data)
{
	char *b;

	/* create a working copy of the format string */
	char *format = bb_xstrdup(masterformat);

	/* Add 2 to accommodate our conversion of the stat `%s' format string
	 * to the printf `%llu' one.  */
	size_t n_alloc = strlen(format) + 2 + 1;
	char *dest = xmalloc(n_alloc);

	b = format;
	while (b) {
		char *p = strchr(b, '%');
		if (p != NULL) {
			size_t len;
			*p++ = '\0';
			fputs(b, stdout);

			len = strspn(p, "#-+.I 0123456789");
			dest[0] = '%';
			memcpy(dest + 1, p, len);
			dest[1 + len] = 0;
			p += len;

			b = p + 1;
			switch (*p) {
				case '\0':
					b = NULL;
					/* fall through */
				case '%':
					putchar('%');
					break;
				default:
					print_func(dest, n_alloc, *p, filename, data);
					break;
			}

		} else {
			fputs(b, stdout);
			b = NULL;
		}
	}

	free(format);
	free(dest);
}
#endif

/* Stat the file system and print what we find.  */
static int do_statfs(char const *filename, char const *format)
{
	struct statfs statfsbuf;

	if (statfs(filename, &statfsbuf) != 0) {
		bb_perror_msg("cannot read file system information for '%s'", filename);
		return 0;
	}

#ifdef CONFIG_FEATURE_STAT_FORMAT
	if (format == NULL)
		format = (terse
			? "%n %i %l %t %s %S %b %f %a %c %d\n"
			: "  File: \"%n\"\n"
			  "    ID: %-8i Namelen: %-7l Type: %T\n"
			  "Block size: %-10s Fundamental block size: %S\n"
			  "Blocks: Total: %-10b Free: %-10f Available: %a\n"
			  "Inodes: Total: %-10c Free: %d\n");
	print_it(format, filename, print_statfs, &statfsbuf);
#else

	format = (terse
		? "%s %Lx %lu "
		: "  File: \"%s\"\n"
		  "    ID: %-8Lx Namelen: %-7lu ");
	printf(format,
	       filename,
	       statfsbuf.f_fsid,
	       statfsbuf.f_namelen);

	if (terse)
		printf("%lx ", (unsigned long int) (statfsbuf.f_type));
	else
		printf("Type: %s\n", human_fstype(statfsbuf.f_type));

	format = (terse
		? "%lu %lu %ld %ld %ld %ld %ld\n"
		: "Block size: %-10lu Fundamental block size: %lu\n"
		  "Blocks: Total: %-10ld Free: %-10ld Available: %ld\n"
		  "Inodes: Total: %-10ld Free: %ld\n");
	printf(format,
	       (unsigned long int) (statfsbuf.f_bsize),
	       statfsbuf.f_frsize ? statfsbuf.f_frsize : statfsbuf.f_bsize,
	       (intmax_t) (statfsbuf.f_blocks),
	       (intmax_t) (statfsbuf.f_bfree),
	       (intmax_t) (statfsbuf.f_bavail),
	       (intmax_t) (statfsbuf.f_files),
	       (intmax_t) (statfsbuf.f_ffree));
#endif

	return 1;
}

/* stat the file and print what we find */
static int do_stat(char const *filename, char const *format)
{
	struct stat statbuf;

	if ((follow_links ? stat : lstat) (filename, &statbuf) != 0) {
		bb_perror_msg("cannot stat '%s'", filename);
		return 0;
	}

#ifdef CONFIG_FEATURE_STAT_FORMAT
	if (format == NULL) {
		if (terse) {
			format = "%n %s %b %f %u %g %D %i %h %t %T %X %Y %Z %o\n";
		} else {
			if (S_ISBLK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode)) {
				format =
					"  File: \"%N\"\n"
					"  Size: %-10s\tBlocks: %-10b IO Block: %-6o %F\n"
					"Device: %Dh/%dd\tInode: %-10i  Links: %-5h"
					" Device type: %t,%T\n"
					"Access: (%04a/%10.10A)  Uid: (%5u/%8U)   Gid: (%5g/%8G)\n"
					"Access: %x\n" "Modify: %y\n" "Change: %z\n";
			} else {
				format =
					"  File: \"%N\"\n"
					"  Size: %-10s\tBlocks: %-10b IO Block: %-6o %F\n"
					"Device: %Dh/%dd\tInode: %-10i  Links: %h\n"
					"Access: (%04a/%10.10A)  Uid: (%5u/%8U)   Gid: (%5g/%8G)\n"
					"Access: %x\n" "Modify: %y\n" "Change: %z\n";
			}
		}
	}
	print_it(format, filename, print_stat, &statbuf);
#else
	if (terse) {
		printf("%s %lu %lu %lx %lu %lu %lx %lu %lu %lx %lx %lu %lu %lu %lu\n",
		       filename,
		       (uintmax_t) (statbuf.st_size),
		       (uintmax_t) statbuf.st_blocks,
		       (unsigned long int) statbuf.st_mode,
		       (unsigned long int) statbuf.st_uid,
		       (unsigned long int) statbuf.st_gid,
		       (uintmax_t) statbuf.st_dev,
		       (uintmax_t) statbuf.st_ino,
		       (unsigned long int) statbuf.st_nlink,
		       (unsigned long int) major(statbuf.st_rdev),
		       (unsigned long int) minor(statbuf.st_rdev),
		       (unsigned long int) statbuf.st_atime,
		       (unsigned long int) statbuf.st_mtime,
		       (unsigned long int) statbuf.st_ctime,
		       (unsigned long int) statbuf.st_blksize
		);
	} else {
		char *linkname = NULL;

		struct passwd *pw_ent;
		struct group *gw_ent;
		setgrent();
		gw_ent = getgrgid(statbuf.st_gid);
		setpwent();
		pw_ent = getpwuid(statbuf.st_uid);

		if (S_ISLNK(statbuf.st_mode))
			linkname = xreadlink(filename);
		if (linkname)
			printf("  File: \"%s\" -> \"%s\"\n", filename, linkname);
		else
			printf("  File: \"%s\"\n", filename);

		printf("  Size: %-10lu\tBlocks: %-10lu IO Block: %-6lu %s\n"
		       "Device: %lxh/%lud\tInode: %-10lu  Links: %-5lu",
		       (uintmax_t) (statbuf.st_size),
		       (uintmax_t) statbuf.st_blocks,
		       (unsigned long int) statbuf.st_blksize,
		       file_type(&statbuf),
		       (uintmax_t) statbuf.st_dev,
		       (uintmax_t) statbuf.st_dev,
		       (uintmax_t) statbuf.st_ino,
		       (unsigned long int) statbuf.st_nlink);
		if (S_ISBLK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode))
			printf(" Device type: %lx,%lx\n",
			       (unsigned long int) major(statbuf.st_rdev),
			       (unsigned long int) minor(statbuf.st_rdev));
		else
			putchar('\n');
		printf("Access: (%04lo/%10.10s)  Uid: (%5lu/%8s)   Gid: (%5lu/%8s)\n"
		       "Access: %s\n" "Modify: %s\n" "Change: %s\n",
		       (unsigned long int) (statbuf.st_mode & (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)),
		       bb_mode_string(statbuf.st_mode),
		       (unsigned long int) statbuf.st_uid,
		       (pw_ent != 0L) ? pw_ent->pw_name : "UNKNOWN",
		       (unsigned long int) statbuf.st_gid,
		       (gw_ent != 0L) ? gw_ent->gr_name : "UNKNOWN",
		       human_time(statbuf.st_atime),
		       human_time(statbuf.st_mtime),
		       human_time(statbuf.st_ctime));
	}
#endif
	return 1;
}

int stat_main(int argc, char **argv)
{
	int i;
	char *format = NULL;
	int ok = 1;
	long flags;
	int (*statfunc)(char const *, char const *) = do_stat;

	flags = bb_getopt_ulflags(argc, argv, "fLlt"
#ifdef CONFIG_FEATURE_STAT_FORMAT
	"c:", &format
#endif
	);

	if (flags & 1)                /* -f */
		statfunc = do_statfs;
	if (flags & 2 || flags & 4)   /* -L, -l */
		follow_links = 1;
	if (flags & 8)                /* -t */
		terse = 1;
	if (argc == optind)           /* files */
		bb_show_usage();

	for (i = optind; i < argc; ++i)
		ok &= statfunc(argv[i], format);

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
