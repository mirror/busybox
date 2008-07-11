/* vi: set sw=4 ts=4: */
/*
 * Mini cpio implementation for busybox
 *
 * Copyright (C) 2001 by Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Limitations:
 * Doesn't check CRC's
 * Only supports new ASCII and CRC formats
 *
 */
#include "libbb.h"
#include "unarchive.h"

#if ENABLE_FEATURE_CPIO_O
static off_t cpio_pad4(off_t size)
{
	int i;

	i = (- size) & 3;
	size += i;
	while (--i >= 0)
		bb_putchar('\0');
	return size;
}

/* Return value will become exit code.
 * It's ok to exit instead of return. */
static int cpio_o(void)
{
	struct name_s {
		struct name_s *next;
		char name[1];
	};
	struct inodes_s {
		struct inodes_s *next;
		struct name_s *names;
		struct stat st;
	};

	struct inodes_s *links = NULL;
	off_t bytes = 0; /* output bytes count */

	while (1) {
		const char *name;
		char *line;
		struct stat st;

		line = xmalloc_fgetline(stdin);

		if (line) {
			/* Strip leading "./[./]..." from the filename */
			name = line;
			while (name[0] == '.' && name[1] == '/') {
				while (*++name == '/')
					continue;
			}
			if (!*name) { /* line is empty */
				free(line);
				continue;
			}
			if (lstat(name, &st)) {
 abort_cpio_o:
				bb_simple_perror_msg_and_die(name);
			}

			if (!(S_ISLNK(st.st_mode) || S_ISREG(st.st_mode)))
				st.st_size = 0; /* paranoia */

			/* Store hardlinks for later processing, dont output them */
			if (!S_ISDIR(st.st_mode) && st.st_nlink > 1) {
				struct name_s *n;
				struct inodes_s *l;

				/* Do we have this hardlink remembered? */
				l = links;
				while (1) {
					if (l == NULL) {
						/* Not found: add new item to "links" list */
						l = xzalloc(sizeof(*l));
						l->st = st;
						l->next = links;
						links = l;
						break;
					}
					if (l->st.st_ino == st.st_ino) {
						/* found */
						break;
					}
					l = l->next;
				}
				/* Add new name to "l->names" list */
				n = xmalloc(sizeof(*n) + strlen(name));
				strcpy(n->name, name);
				n->next = l->names;
				l->names = n;

				free(line);
				continue;
			}

		} else { /* line == NULL: EOF */
 next_link:
			if (links) {
				/* Output hardlink's data */
				st = links->st;
				name = links->names->name;
				links->names = links->names->next;
				/* GNU cpio is reported to emit file data
				 * only for the last instance. Mimic that. */
				if (links->names == NULL)
					links = links->next;
				else
					st.st_size = 0;
				/* NB: we leak links->names and/or links,
				 * this is intended (we exit soon anyway) */
			} else {
				/* If no (more) hardlinks to output,
				 * output "trailer" entry */
				name = "TRAILER!!!";
				/* st.st_size == 0 is a must, but for uniformity
				 * in the output, we zero out everything */
				memset(&st, 0, sizeof(st));
				/* st.st_nlink = 1; - GNU cpio does this */
			}
		}

		bytes += printf("070701"
		                "%08X%08X%08X%08X%08X%08X%08X"
		                "%08X%08X%08X%08X" /* GNU cpio uses uppercase hex */
				/* strlen+1: */ "%08X"
				/* chksum: */   "00000000" /* (only for "070702" files) */
				/* name,NUL: */ "%s%c",
		                (unsigned)(uint32_t) st.st_ino,
		                (unsigned)(uint32_t) st.st_mode,
		                (unsigned)(uint32_t) st.st_uid,
		                (unsigned)(uint32_t) st.st_gid,
		                (unsigned)(uint32_t) st.st_nlink,
		                (unsigned)(uint32_t) st.st_mtime,
		                (unsigned)(uint32_t) st.st_size,
		                (unsigned)(uint32_t) major(st.st_dev),
		                (unsigned)(uint32_t) minor(st.st_dev),
		                (unsigned)(uint32_t) major(st.st_rdev),
		                (unsigned)(uint32_t) minor(st.st_rdev),
		                (unsigned)(strlen(name) + 1),
		                name, '\0');
		bytes = cpio_pad4(bytes);

		if (st.st_size) {
			if (S_ISLNK(st.st_mode)) {
				char *lpath = xmalloc_readlink_or_warn(name);
				if (!lpath)
					goto abort_cpio_o;
				bytes += printf("%s", lpath);
				free(lpath);
			} else { /* S_ISREG */
				int fd = xopen(name, O_RDONLY);
				fflush(stdout);
				/* We must abort if file got shorter too! */
				bb_copyfd_exact_size(fd, STDOUT_FILENO, st.st_size);
				bytes += st.st_size;
				close(fd);
			}
			bytes = cpio_pad4(bytes);
		}

		if (!line) {
			if (links)
				goto next_link;
			/* TODO: GNU cpio pads trailer to 512 bytes, do we want that? */
			return EXIT_SUCCESS;
		}

		free(line);
	} /* end of "while (1)" */
}
#endif

/* GNU cpio (GNU cpio) 2.9 help (abridged):

 Main operation mode:
  -i, --extract              Extract files from an archive
  -o, --create               Create the archive
  -p, --pass-through         Copy-pass mode (was ist das?!)
  -t, --list                 List the archive

 Operation modifiers valid in any mode:
      --block-size=SIZE      I/O block size = SIZE * 512 bytes
  -B                         I/O block size = 5120 bytes
  -c                         Use the old portable (ASCII) archive format
  -C, --io-size=NUMBER       I/O block size to the given NUMBER bytes
  -f, --nonmatching          Only copy files that do not match given pattern
  -F, --file=FILE            Use FILE instead of standard input or output
  -H, --format=FORMAT        Use given archive FORMAT
  -M, --message=STRING       Print STRING when the end of a volume of the
                             backup media is reached
  -n, --numeric-uid-gid      If -v, show numeric UID and GID
      --quiet                Do not print the number of blocks copied
      --rsh-command=COMMAND  Use remote COMMAND instead of rsh
  -v, --verbose              Verbosely list the files processed
  -V, --dot                  Print a "." for each file processed
  -W, --warning=FLAG         Control warning display: 'none','truncate','all';
                             multiple options accumulate

 Operation modifiers valid only in --extract mode:
  -b, --swap                 Swap both halfwords of words and bytes of
                             halfwords in the data (equivalent to -sS)
  -r, --rename               Interactively rename files
  -s, --swap-bytes           Swap the bytes of each halfword in the files
  -S, --swap-halfwords       Swap the halfwords of each word (4 bytes)
      --to-stdout            Extract files to standard output
  -E, --pattern-file=FILE    Read additional patterns specifying filenames to
                             extract or list from FILE
      --only-verify-crc      Verify CRC's, don't actually extract the files

 Operation modifiers valid only in --create mode:
  -A, --append               Append to an existing archive
  -O FILE                    File to use instead of standard output

 Operation modifiers valid only in --pass-through mode:
  -l, --link                 Link files instead of copying them, when possible

 Operation modifiers valid in --extract and --create modes:
      --absolute-filenames   Do not strip file system prefix components from
                             the file names
      --no-absolute-filenames Create all files relative to the current dir

 Operation modifiers valid in --create and --pass-through modes:
  -0, --null                 A list of filenames is terminated by a NUL
  -a, --reset-access-time    Reset the access times of files after reading them
  -I FILE                    File to use instead of standard input
  -L, --dereference          Dereference symbolic links (copy  the files
                             that they point to instead of copying the links)
  -R, --owner=[USER][:.][GROUP] Set owner of created files

 Operation modifiers valid in --extract and --pass-through modes:
  -d, --make-directories     Create leading directories where needed
  -m, --preserve-modification-time
                             Retain previous file modification times when
                             creating files
      --no-preserve-owner    Do not change the ownership of the files
      --sparse               Write files with blocks of zeros as sparse files
  -u, --unconditional        Replace all files unconditionally
 */

int cpio_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cpio_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	archive_handle_t *archive_handle;
	char *cpio_filename;
	USE_FEATURE_CPIO_O(const char *cpio_fmt = "";)
	unsigned opt;
	enum {
		CPIO_OPT_EXTRACT            = (1 << 0),
		CPIO_OPT_TEST               = (1 << 1),
		CPIO_OPT_UNCONDITIONAL      = (1 << 2),
		CPIO_OPT_VERBOSE            = (1 << 3),
		CPIO_OPT_FILE               = (1 << 4),
		CPIO_OPT_CREATE_LEADING_DIR = (1 << 5),
		CPIO_OPT_PRESERVE_MTIME     = (1 << 6),
		CPIO_OPT_CREATE             = (1 << 7),
		CPIO_OPT_FORMAT             = (1 << 8),
	};

#if ENABLE_GETOPT_LONG && ENABLE_DESKTOP
	applet_long_options =
		"extract\0"      No_argument       "i"
		"list\0"         No_argument       "t"
#if ENABLE_FEATURE_CPIO_O
		"create\0"       No_argument       "o"
		"format\0"       Required_argument "H"
#endif
		;
#endif

	/* Initialize */
	archive_handle = init_handle();
	archive_handle->src_fd = STDIN_FILENO;
	archive_handle->seek = seek_by_read;
	archive_handle->flags = ARCHIVE_EXTRACT_NEWER;

#if ENABLE_FEATURE_CPIO_O
	opt = getopt32(argv, "ituvF:dmoH:", &cpio_filename, &cpio_fmt);

	if (opt & CPIO_OPT_CREATE) {
		if (*cpio_fmt != 'n')
			bb_show_usage();
		if (opt & CPIO_OPT_FILE) {
			fclose(stdout);
			stdout = fopen(cpio_filename, "w");
			/* Paranoia: I don't trust libc that much */
			xdup2(fileno(stdout), STDOUT_FILENO);
		}
		return cpio_o();
	}
#else
	opt = getopt32(argv, "ituvF:dm", &cpio_filename);
#endif
	argv += optind;

	/* One of either extract or test options must be given */
	if ((opt & (CPIO_OPT_TEST | CPIO_OPT_EXTRACT)) == 0) {
		bb_show_usage();
	}

	if (opt & CPIO_OPT_TEST) {
		/* if both extract and test options are given, ignore extract option */
		if (opt & CPIO_OPT_EXTRACT) {
			opt &= ~CPIO_OPT_EXTRACT;
		}
		archive_handle->action_header = header_list;
	}
	if (opt & CPIO_OPT_EXTRACT) {
		archive_handle->action_data = data_extract_all;
	}
	if (opt & CPIO_OPT_UNCONDITIONAL) {
		archive_handle->flags |= ARCHIVE_EXTRACT_UNCONDITIONAL;
		archive_handle->flags &= ~ARCHIVE_EXTRACT_NEWER;
	}
	if (opt & CPIO_OPT_VERBOSE) {
		if (archive_handle->action_header == header_list) {
			archive_handle->action_header = header_verbose_list;
		} else {
			archive_handle->action_header = header_list;
		}
	}
	if (opt & CPIO_OPT_FILE) { /* -F */
		archive_handle->src_fd = xopen(cpio_filename, O_RDONLY);
		archive_handle->seek = seek_by_jump;
	}
	if (opt & CPIO_OPT_CREATE_LEADING_DIR) {
		archive_handle->flags |= ARCHIVE_CREATE_LEADING_DIRS;
	}
	if (opt & CPIO_OPT_PRESERVE_MTIME) {
		archive_handle->flags |= ARCHIVE_PRESERVE_DATE;
	}

	while (*argv) {
		archive_handle->filter = filter_accept_list;
		llist_add_to(&(archive_handle->accept), *argv);
		argv++;
	}

	while (get_header_cpio(archive_handle) == EXIT_SUCCESS)
		continue;

	return EXIT_SUCCESS;
}
