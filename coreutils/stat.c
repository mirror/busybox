/* vi: set sw=4 ts=4: */
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
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

/* vars to control behavior */
#define OPT_TERSE 2
#define OPT_DEREFERENCE 4
static long flags;

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
	/* Old
	static char *str;
	str = ctime(&t);
	str[strlen(str)-1] = '\0';
	return str;
	*/
	/* coreutils 6.3 compat: */
	static char buf[sizeof("YYYY-MM-DD HH:MM:SS.000000000")];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S.000000000", localtime(&t));
	return buf;
}

/* Return the type of the specified file system.
 * Some systems have statfvs.f_basetype[FSTYPSZ]. (AIX, HP-UX, and Solaris)
 * Others have statfs.f_fstypename[MFSNAMELEN]. (NetBSD 1.5.2)
 * Still others have neither and have to get by with f_type (Linux).
 */
static char const *human_fstype(long f_type)
{
	int i;
	static const struct types {
		long type;
		const char *fs;
	} humantypes[] = {
		{ 0xADFF,     "affs" },
		{ 0x1Cd1,     "devpts" },
		{ 0x137D,     "ext" },
		{ 0xEF51,     "ext2" },
		{ 0xEF53,     "ext2/ext3" },
		{ 0x3153464a, "jfs" },
		{ 0x58465342, "xfs" },
		{ 0xF995E849, "hpfs" },
		{ 0x9660,     "isofs" },
		{ 0x4000,     "isofs" },
		{ 0x4004,     "isofs" },
		{ 0x137F,     "minix" },
		{ 0x138F,     "minix (30 char.)" },
		{ 0x2468,     "minix v2" },
		{ 0x2478,     "minix v2 (30 char.)" },
		{ 0x4d44,     "msdos" },
		{ 0x4006,     "fat" },
		{ 0x564c,     "novell" },
		{ 0x6969,     "nfs" },
		{ 0x9fa0,     "proc" },
		{ 0x517B,     "smb" },
		{ 0x012FF7B4, "xenix" },
		{ 0x012FF7B5, "sysv4" },
		{ 0x012FF7B6, "sysv2" },
		{ 0x012FF7B7, "coh" },
		{ 0x00011954, "ufs" },
		{ 0x012FD16D, "xia" },
		{ 0x5346544e, "ntfs" },
		{ 0x1021994,  "tmpfs" },
		{ 0x52654973, "reiserfs" },
		{ 0x28cd3d45, "cramfs" },
		{ 0x7275,     "romfs" },
		{ 0x858458f6, "romfs" },
		{ 0x73717368, "squashfs" },
		{ 0x62656572, "sysfs" },
		{ 0, "UNKNOWN" }
	};
	for (i=0; humantypes[i].type; ++i)
		if (humantypes[i].type == f_type)
			break;
	return humantypes[i].fs;
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
		strncat(pformat, "jd", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_blocks));
		break;
	case 'f':
		strncat(pformat, "jd", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_bfree));
		break;
	case 'a':
		strncat(pformat, "jd", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_bavail));
		break;
	case 'S':
	case 's':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) (statfsbuf->f_bsize));
		break;
	case 'c':
		strncat(pformat, "jd", buf_len);
		printf(pformat, (intmax_t) (statfsbuf->f_files));
		break;
	case 'd':
		strncat(pformat, "jd", buf_len);
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
		strncat(pformat, "ju", buf_len);
		printf(pformat, (uintmax_t) statbuf->st_dev);
		break;
	case 'D':
		strncat(pformat, "jx", buf_len);
		printf(pformat, (uintmax_t) statbuf->st_dev);
		break;
	case 'i':
		strncat(pformat, "ju", buf_len);
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
		strncat(pformat, "ju", buf_len);
		printf(pformat, (uintmax_t) (statbuf->st_size));
		break;
	case 'B':
		strncat(pformat, "lu", buf_len);
		printf(pformat, (unsigned long int) 512); //ST_NBLOCKSIZE
		break;
	case 'b':
		strncat(pformat, "ju", buf_len);
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
	char *format = xstrdup(masterformat);

	/* Add 2 to accomodate our conversion of the stat '%s' format string
	 * to the printf '%llu' one.  */
	size_t n_alloc = strlen(format) + 2 + 1;
	char *dest = xmalloc(n_alloc);

	b = format;
	while (b) {
		size_t len;
		char *p = strchr(b, '%');
		if (!p) {
			/* coreutils 6.3 always print <cr> at the end */
			/*fputs(b, stdout);*/
			puts(b);
			break;
		}
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
		format = (flags & OPT_TERSE
			? "%n %i %l %t %s %b %f %a %c %d\n"
			: "  File: \"%n\"\n"
			  "    ID: %-8i Namelen: %-7l Type: %T\n"
			  "Block size: %-10s\n"
			  "Blocks: Total: %-10b Free: %-10f Available: %a\n"
			  "Inodes: Total: %-10c Free: %d");
	print_it(format, filename, print_statfs, &statfsbuf);
#else

	format = (flags & OPT_TERSE
		? "%s %llx %lu "
		: "  File: \"%s\"\n"
		  "    ID: %-8Lx Namelen: %-7lu ");
	printf(format,
	       filename,
	       statfsbuf.f_fsid,
	       statfsbuf.f_namelen);

	if (flags & OPT_TERSE)
		printf("%lx ", (unsigned long int) (statfsbuf.f_type));
	else
		printf("Type: %s\n", human_fstype(statfsbuf.f_type));

	format = (flags & OPT_TERSE
		? "%lu %ld %ld %ld %ld %ld\n"
		: "Block size: %-10lu\n"
		  "Blocks: Total: %-10jd Free: %-10jd Available: %jd\n"
		  "Inodes: Total: %-10jd Free: %jd\n");
	printf(format,
	       (unsigned long int) (statfsbuf.f_bsize),
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

	if ((flags & OPT_DEREFERENCE ? stat : lstat) (filename, &statbuf) != 0) {
		bb_perror_msg("cannot stat '%s'", filename);
		return 0;
	}

#ifdef CONFIG_FEATURE_STAT_FORMAT
	if (format == NULL) {
		if (flags & OPT_TERSE) {
			format = "%n %s %b %f %u %g %D %i %h %t %T %X %Y %Z %o";
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
	if (flags & OPT_TERSE) {
		printf("%s %ju %ju %lx %lu %lu %jx %ju %lu %lx %lx %lu %lu %lu %lu\n",
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

		printf("  Size: %-10ju\tBlocks: %-10ju IO Block: %-6lu %s\n"
		       "Device: %jxh/%jud\tInode: %-10ju  Links: %-5lu",
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
	int (*statfunc)(char const *, char const *) = do_stat;

	flags = getopt32(argc, argv, "ftL"
		USE_FEATURE_STAT_FORMAT("c:", &format)
	);

	if (flags & 1)                /* -f */
		statfunc = do_statfs;
	if (argc == optind)           /* files */
		bb_show_usage();

	for (i = optind; i < argc; ++i)
		ok &= statfunc(argv[i], format);

	return (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
