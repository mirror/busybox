/*
 * Mini mount implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
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
 * 3/21/1999	Charles P. Wright <cpwright@cpwright.com>
 *		searches through fstab when -a is passed
 *		will try mounting stuff with all fses when passed -t auto
 *
 * 1999-04-17	Dave Cinege...Rewrote -t auto. Fixed ro mtab.
 *
 * 1999-10-07	Erik Andersen <andersen@lineo.com>, <andersee@debian.org>.
 *              Rewrote of a lot of code. Removed mtab usage (I plan on
 *              putting it back as a compile-time option some time), 
 *              major adjustments to option parsing, and some serious 
 *              dieting all around.
*/

#include "internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/mount.h>
#include <ctype.h>
#include <fstab.h>

extern const char mtab_file[]; /* Defined in utility.c */

static const char mount_usage[] = "Usage:\tmount [flags]\n"
    "\tmount [flags] device directory [-o options,more-options]\n"
    "\n"
    "Flags:\n"
    "\t-a:\tMount all file systems in fstab.\n"
#ifdef BB_MTAB
    "\t-f:\t\"Fake\" mount. Add entry to mount table but don't mount it.\n"
    "\t-n:\tDon't write a mount table entry.\n"
#endif
    "\t-o option:\tOne of many filesystem options, listed below.\n"
    "\t-r:\tMount the filesystem read-only.\n"
    "\t-t filesystem-type:\tSpecify the filesystem type.\n"
    "\t-w:\tMount for reading and writing (default).\n"
    "\n"
    "Options for use with the \"-o\" flag:\n"
    "\tasync / sync:\tWrites are asynchronous / synchronous.\n"
    "\tdev / nodev:\tAllow use of special device files / disallow them.\n"
    "\texec / noexec:\tAllow use of executable files / disallow them.\n"
    "\tsuid / nosuid:\tAllow set-user-id-root programs / disallow them.\n"
    "\tremount: Re-mount a currently-mounted filesystem, changing its flags.\n"
    "\tro / rw: Mount for read-only / read-write.\n"
    "\t"
    "There are EVEN MORE flags that are specific to each filesystem.\n"
    "You'll have to see the written documentation for those.\n";


struct mount_options {
    const char *name;
    unsigned long and;
    unsigned long or;
};

static const struct mount_options mount_options[] = {
    {"async", ~MS_SYNCHRONOUS, 0},
    {"defaults", ~0, 0},
    {"dev", ~MS_NODEV, 0},
    {"exec", ~MS_NOEXEC, 0},
    {"nodev", ~0, MS_NODEV},
    {"noexec", ~0, MS_NOEXEC},
    {"nosuid", ~0, MS_NOSUID},
    {"remount", ~0, MS_REMOUNT},
    {"ro", ~0, MS_RDONLY},
    {"rw", ~MS_RDONLY, 0},
    {"suid", ~MS_NOSUID, 0},
    {"sync", ~0, MS_SYNCHRONOUS},
    {0, 0, 0}
};

#if ! defined BB_MTAB
#define do_mount(specialfile, dir, filesystemtype, flags, string_flags, useMtab, fakeIt) \
	mount(specialfile, dir, filesystemtype, flags, string_flags)
#else
static int
do_mount(char* specialfile, char* dir, char* filesystemtype, 
	long flags, void* string_flags, int useMtab, int fakeIt)
{
    int status=0;

    if (fakeIt==FALSE)
	status=mount(specialfile, dir, filesystemtype, flags, string_flags);

    if ( status == 0 ) {
	if ( useMtab==TRUE )
	    write_mtab(specialfile, dir, filesystemtype, flags, string_flags);
	return 0;
    }
    else 
	return( status);
}
#endif


/* Seperate standard mount options from the nonstandard string options */
static void
parse_mount_options ( char *options, unsigned long *flags, char *strflags)
{
    while (options) {
	int gotone=FALSE;
	char *comma = strchr (options, ',');
	const struct mount_options* f = mount_options;
	if (comma)
	    *comma = '\0';

	while (f->name != 0) {
	    if (strcasecmp (f->name, options) == 0) {

		*flags &= f->and;
		*flags |= f->or;
		gotone=TRUE;
		break;
	    }
	    f++;
	}
	if (*strflags && strflags!= '\0' && gotone==FALSE) {
	    char *temp=strflags;
	    temp += strlen (strflags);
	    *temp++ = ',';
	    *temp++ = '\0';
	}
	if (gotone==FALSE) {
	    strcat (strflags, options);
	    gotone=FALSE;
	}
	if (comma) {
	    *comma = ',';
	    options = ++comma;
	} else {
	    break;
	}
    }
}

int
mount_one(char *blockDevice, char *directory, char *filesystemType,
	   unsigned long flags, char *string_flags, int useMtab, int fakeIt)
{
    int status = 0;

    char buf[255];

    if (strcmp(filesystemType, "auto") == 0) {
	FILE *f = fopen ("/proc/filesystems", "r");

	if (f == NULL)
	    return( FALSE);

	while (fgets (buf, sizeof (buf), f) != NULL) {
	    filesystemType = buf;
	    if (*filesystemType == '\t') {	// Not a nodev filesystem

		// Add NULL termination to each line
		while (*filesystemType && *filesystemType != '\n')
		    filesystemType++;
		*filesystemType = '\0';

		filesystemType = buf;
		filesystemType++;	// hop past tab

		status = do_mount (blockDevice, directory, filesystemType,
				flags | MS_MGC_VAL, string_flags, useMtab, fakeIt);
		if (status == 0)
		    break;
	    }
	}
	fclose (f);
    } else {
	status = do_mount (blockDevice, directory, filesystemType,
			flags | MS_MGC_VAL, string_flags, useMtab, fakeIt);
    }

    if (status) {
	fprintf (stderr, "Mounting %s on %s failed: %s\n",
		 blockDevice, directory, strerror(errno));
	return (FALSE);
    }
    return (TRUE);
}

extern int mount_main (int argc, char **argv)
{
    char string_flags[1024]="";
    unsigned long flags = 0;
    char *filesystemType = "auto";
    char *device = NULL;
    char *directory = NULL;
    int all = FALSE;
    int fakeIt = FALSE;
    int useMtab = TRUE;
    int i;

    /* Only compiled in if BB_MTAB is not defined */
    whine_if_fstab_is_missing();

    if (argc == 1) {
	FILE *mountTable = setmntent (mtab_file, "r");
	if (mountTable) {
	    struct mntent *m;
	    while ((m = getmntent (mountTable)) != 0) {
		struct fstab* fstabItem;
		char *blockDevice = m->mnt_fsname;
		/* Note that if /etc/fstab is missing, libc can't fix up /dev/root for us */
		if (strcmp (blockDevice, "/dev/root") == 0) {
		    fstabItem = getfsfile ("/");
		    if (fstabItem != NULL)
			blockDevice = fstabItem->fs_spec;
		}
		printf ("%s on %s type %s (%s)\n", blockDevice, m->mnt_dir,
			m->mnt_type, m->mnt_opts);
	    }
	    endmntent (mountTable);
	} else {
	    perror(mtab_file);
	}
	exit( TRUE);
    }


    /* Parse options */
    i = --argc;
    argv++;
    while (i > 0 && **argv) {
	if (**argv == '-') {
	    while (i>0 && *++(*argv)) switch (**argv) {
	    case 'o':
		if (--i == 0) {
		    fprintf (stderr, "%s\n", mount_usage);
		    exit( FALSE);
		}
		parse_mount_options (*(++argv), &flags, string_flags);
		--i;
		++argv;
		break;
	    case 'r':
		flags |= MS_RDONLY;
		break;
	    case 't':
		if (--i == 0) {
		    fprintf (stderr, "%s\n", mount_usage);
		    exit( FALSE);
		}
		filesystemType = *(++argv);
		--i;
		++argv;
		break;
	    case 'w':
		flags &= ~MS_RDONLY;
		break;
	    case 'a':
		all = TRUE;
		break;
#ifdef BB_MTAB
	    case 'f':
		fakeIt = TRUE;
		break;
	    case 'n':
		useMtab = FALSE;
		break;
#endif
	    case 'v':
	    case 'h':
	    case '-':
		fprintf (stderr, "%s\n", mount_usage);
		exit( TRUE);
		break;
	    }
	} else {
	    if (device == NULL)
		device=*argv;
	    else if (directory == NULL)
		directory=*argv;
	    else {
		fprintf (stderr, "%s\n", mount_usage);
		exit( TRUE);
	    }
	}
	i--;
	argv++;
    }

    if (all == TRUE) {
	struct mntent *m;
	FILE *f = setmntent ("/etc/fstab", "r");

	if (f == NULL) {
	    perror("/etc/fstab");
	    exit( FALSE); 
	}
	while ((m = getmntent (f)) != NULL) {
	    // If the file system isn't noauto, and isn't mounted on /, 
	    // and isn't swap or nfs, then mount it
	    if ((!strstr (m->mnt_opts, "noauto")) &&
		    (m->mnt_dir[1] != '\0') && 
		    (!strstr (m->mnt_type, "swap")) && 
		    (!strstr (m->mnt_type, "nfs"))) 
	    {
		flags = 0;
		*string_flags = '\0';
		parse_mount_options(m->mnt_opts, &flags, string_flags);
		mount_one (m->mnt_fsname, m->mnt_dir, m->mnt_type, 
			flags, string_flags, useMtab, fakeIt);
	    }
	}
	endmntent (f);
    } else {
	if (device && directory) {
	    exit (mount_one (device, directory, filesystemType, 
			flags, string_flags, useMtab, fakeIt));
	} else {
	    fprintf (stderr, "%s\n", mount_usage);
	    exit( FALSE);
	}
    }
    exit( TRUE);
}
