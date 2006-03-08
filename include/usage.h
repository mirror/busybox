#ifndef __BB_USAGE_H__
#define __BB_USAGE_H__

#define addgroup_trivial_usage \
	"[-g GID] group_name [user_name]"
#define addgroup_full_usage \
	"Adds a group to the system\n\n" \
	"Options:\n" \
	"\t-g GID\t\tspecify gid"

#define adduser_trivial_usage \
	"[OPTIONS] user_name"
#define adduser_full_usage \
	"Adds a user to the system\n\n" \
	"Options:\n" \
	"\t-h DIR\t\tAssign home directory DIR\n" \
	"\t-g GECOS\tAssign gecos field GECOS\n" \
	"\t-s SHELL\tAssign login shell SHELL\n" \
	"\t-G\t\tAdd the user to existing group GROUP\n" \
	"\t-S\t\tcreate a system user (ignored)\n" \
	"\t-D\t\tDo not assign a password (logins still possible via ssh)\n" \
	"\t-H\t\tDo not create the home directory"

#define adjtimex_trivial_usage \
	"[-q] [-o offset] [-f frequency] [-p timeconstant] [-t tick]"
#define adjtimex_full_usage \
	"Reads and optionally sets system timebase parameters.\n" \
	"See adjtimex(2).\n\n" \
	"Options:\n" \
	"\t-q\t\tquiet mode - do not print\n" \
	"\t-o offset\ttime offset, microseconds\n" \
	"\t-f frequency\tfrequency adjust, integer kernel units (65536 is 1ppm)\n" \
	"\t\t\t(positive values make the system clock run fast)\n" \
	"\t-t tick\t\tmicroseconds per tick, usually 10000\n" \
	"\t-p timeconstant"

#define ar_trivial_usage \
	"[-o] [-v] [-p] [-t] [-x] ARCHIVE FILES"
#define ar_full_usage \
	"Extract or list FILES from an ar archive.\n\n" \
	"Options:\n" \
	"\t-o\t\tpreserve original dates\n" \
	"\t-p\t\textract to stdout\n" \
	"\t-t\t\tlist\n" \
	"\t-x\t\textract\n" \
	"\t-v\t\tverbosely list files processed"

#define arping_trivial_usage \
	"[-fqbDUA] [-c count] [-w timeout] [-I device] [-s sender] target"
#define arping_full_usage \
	"Ping hosts by ARP requests/replies.\n\n" \
	"Options:\n" \
	"\t-f\t\tQuit on first ARP reply\n" \
	"\t-q\t\tBe quiet\n" \
	"\t-b\t\tKeep broadcasting, don't go unicast\n" \
	"\t-D\t\tDuplicated address detection mode\n" \
	"\t-U\t\tUnsolicited ARP mode, update your neighbours\n" \
	"\t-A\t\tARP answer mode, update your neighbours\n" \
	"\t-c count\tStop after sending count ARP request packets\n" \
	"\t-w timeout\tTime to wait for ARP reply, in seconds\n" \
	"\t-I device\tOutgoing interface name, default is eth0\n" \
	"\t-s sender\tSet specific sender IP address\n" \
	"\ttarget\t\tTarget IP address of ARP request"

#define ash_trivial_usage \
	"[FILE]...\n" \
	"or: ash -c command [args]..."
#define ash_full_usage \
	"The ash shell (command interpreter)"

#define awk_trivial_usage \
	"[OPTION]... [program-text] [FILE ...]"
#define awk_full_usage \
	"Options:\n" \
	"\t-v var=val\tassign value 'val' to variable 'var'\n" \
	"\t-F sep\t\tuse 'sep' as field separator\n" \
	"\t-f progname\tread program source from file 'progname'"

#define basename_trivial_usage \
	"FILE [SUFFIX]"
#define basename_full_usage \
	"Strips directory path and suffixes from FILE.\n" \
	"If specified, also removes any trailing SUFFIX."
#define basename_example_usage \
	"$ basename /usr/local/bin/foo\n" \
	"foo\n" \
	"$ basename /usr/local/bin/\n" \
	"bin\n" \
	"$ basename /foo/bar.txt .txt\n" \
	"bar"

#define bunzip2_trivial_usage \
	"[OPTION]... [FILE]"
#define bunzip2_full_usage \
	"Uncompress FILE (or standard input if FILE is '-' or omitted).\n\n" \
	"Options:\n" \
	"\t-c\tWrite output to standard output\n" \
	"\t-f\tForce"

#define busybox_notes_usage \
	"Hello world!\n"

#define bzcat_trivial_usage \
	"FILE"
#define bzcat_full_usage \
	"Uncompress to stdout."

#define unlzma_trivial_usage \
	"[OPTION]... [FILE]"
#define unlzma_full_usage \
	"Uncompress FILE (or standard input if FILE is '-' or omitted).\n\n" \
	"Options:\n" \
	"\t-c\tWrite output to standard output\n" \
	"\t-f\tForce"

#define lzmacat_trivial_usage \
	"FILE"
#define lzmacat_full_usage \
	"Uncompress to stdout."

#define cal_trivial_usage \
	"[-jy] [[month] year]"
#define cal_full_usage \
	"Display a calendar.\n" \
	"\nOptions:\n" \
	"\t-j\tUse julian dates\n" \
	"\t-y\tDisplay the entire year"

#define cat_trivial_usage \
	"[-u] [FILE]..."
#define cat_full_usage \
	"Concatenates FILE(s) and prints them to stdout.\n\n" \
	"Options:\n" \
	"\t-u\tignored since unbuffered i/o is always used"
#define cat_example_usage \
	"$ cat /proc/uptime\n" \
	"110716.72 17.67"

#define chattr_trivial_usage \
	"[-R] [-+=AacDdijsStTu] [-v version] files..."
#define chattr_full_usage \
	"change file attributes on an ext2 fs\n\n" \
	"Modifiers:\n" \
	"\t-\tremove attributes\n" \
	"\t+\tadd attributes\n" \
	"\t=\tset attributes\n" \
	"Attributes:\n" \
	"\tA\tdon't track atime\n" \
	"\ta\tappend mode only\n" \
	"\tc\tenable compress\n" \
	"\tD\twrite dir contents synchronously\n" \
	"\td\tdo not backup with dump\n" \
	"\ti\tcannot be modified (immutable)\n" \
	"\tj\twrite all data to journal first\n" \
	"\ts\tzero disk storage when deleted\n" \
	"\tS\twrite file contents synchronously\n" \
	"\tt\tdisable tail-merging of partial blocks with other files\n" \
	"\tu\tallow file to be undeleted\n" \
	"Options:\n" \
	"\t-R\trecursively list subdirectories\n" \
	"\t-v\tset the file's version/generation number"

#define chgrp_trivial_usage \
	"[OPTION]... GROUP FILE..."
#define chgrp_full_usage \
	"Change the group membership of each FILE to GROUP.\n" \
	"\nOptions:\n" \
	"\t-R\tChanges files and directories recursively"
#define chgrp_example_usage \
	"$ ls -l /tmp/foo\n" \
	"-r--r--r--    1 andersen andersen        0 Apr 12 18:25 /tmp/foo\n" \
	"$ chgrp root /tmp/foo\n" \
	"$ ls -l /tmp/foo\n" \
	"-r--r--r--    1 andersen root            0 Apr 12 18:25 /tmp/foo\n"

#define chmod_trivial_usage \
	"[-R] MODE[,MODE]... FILE..."
#define chmod_full_usage \
	"Each MODE is one or more of the letters ugoa, one of the\n" \
	"symbols +-= and one or more of the letters rwxst.\n\n" \
	"Options:\n" \
	"\t-R\tChanges files and directories recursively"
#define chmod_example_usage \
	"$ ls -l /tmp/foo\n" \
	"-rw-rw-r--    1 root     root            0 Apr 12 18:25 /tmp/foo\n" \
	"$ chmod u+x /tmp/foo\n" \
	"$ ls -l /tmp/foo\n" \
	"-rwxrw-r--    1 root     root            0 Apr 12 18:25 /tmp/foo*\n" \
	"$ chmod 444 /tmp/foo\n" \
	"$ ls -l /tmp/foo\n" \
	"-r--r--r--    1 root     root            0 Apr 12 18:25 /tmp/foo\n"

#define chown_trivial_usage \
	"[ -Rh ]...  OWNER[<.|:>[GROUP]] FILE..."
#define chown_full_usage \
	"Change the owner and/or group of each FILE to OWNER and/or GROUP.\n" \
	"\nOptions:\n" \
	"\t-R\tChanges files and directories recursively\n" \
	"\t-h\tDo not dereference symbolic links"
#define chown_example_usage \
	"$ ls -l /tmp/foo\n" \
	"-r--r--r--    1 andersen andersen        0 Apr 12 18:25 /tmp/foo\n" \
	"$ chown root /tmp/foo\n" \
	"$ ls -l /tmp/foo\n" \
	"-r--r--r--    1 root     andersen        0 Apr 12 18:25 /tmp/foo\n" \
	"$ chown root.root /tmp/foo\n" \
	"ls -l /tmp/foo\n" \
	"-r--r--r--    1 root     root            0 Apr 12 18:25 /tmp/foo\n"

#define chroot_trivial_usage \
	"NEWROOT [COMMAND...]"
#define chroot_full_usage \
	"Run COMMAND with root directory set to NEWROOT."
#define chroot_example_usage \
	"$ ls -l /bin/ls\n" \
	"lrwxrwxrwx    1 root     root          12 Apr 13 00:46 /bin/ls -> /BusyBox\n" \
	"# mount /dev/hdc1 /mnt -t minix\n" \
	"# chroot /mnt\n" \
	"# ls -l /bin/ls\n" \
	"-rwxr-xr-x    1 root     root        40816 Feb  5 07:45 /bin/ls*\n"

#define chvt_trivial_usage \
	"N"
#define chvt_full_usage \
	"Changes the foreground virtual terminal to /dev/ttyN"

#define clear_trivial_usage \
	""
#define clear_full_usage \
	"Clear screen."

#define cmp_trivial_usage \
	"[-l] [-s] FILE1 [FILE2]"
#define cmp_full_usage \
	"Compares FILE1 vs stdin if FILE2 is not specified.\n\n" \
	"Options:\n" \
	"\t-l\tWrite the byte numbers (decimal) and values (octal)\n" \
	"\t\t  for all differing bytes\n" \
	"\t-s\tquiet mode - do not print"

#define comm_trivial_usage \
	"[-123] FILE1 FILE2"
#define comm_full_usage \
	"Compares FILE1 to FILE2, or to stdin if = is specified.\n\n" \
	"Options:\n" \
	"\t-1\tSuppress lines unique to FILE1\n" \
	"\t-2\tSuppress lines unique to FILE2\n" \
	"\t-3\tSuppress lines common to both files"

#define bbconfig_trivial_usage \
	""
#define bbconfig_full_usage \
	"Print the config file which built busybox"

#define cp_trivial_usage \
	"[OPTION]... SOURCE DEST"
#define cp_full_usage \
	"Copies SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n" \
	"\n" \
	"\t-a\tSame as -dpR\n" \
	"\t-d,-P\tPreserves links\n" \
	"\t-H,-L\tDereference all symlinks (implied by default)\n" \
	"\t-p\tPreserves file attributes if possible\n" \
	"\t-f\tforce (implied; ignored) - always set\n" \
	"\t-i\tinteractive, prompt before overwrite\n" \
	"\t-R,-r\tCopies directories recursively"

#define cpio_trivial_usage \
	"-[dimtuv][F cpiofile]"
#define cpio_full_usage \
	"Extract or list files from a cpio archive\n" \
	"Main operation mode:\n" \
	"\td\t\tmake leading directories\n" \
	"\ti\t\textract\n" \
	"\tm\t\tpreserve mtime\n" \
	"\tt\t\tlist\n" \
	"\tv\t\tverbose\n" \
	"\tu\t\tunconditional overwrite\n" \
	"\tF\t\tinput from file"

#define crond_trivial_usage \
	"-d[#] -c <crondir> -f -b"
#define crond_full_usage \
	"\t-d [#] -l [#] -S -L logfile -f -b -c dir\n" \
	"\t-d num\tdebug level\n" \
	"\t-l num\tlog level (8 - default)\n" \
	"\t-S\tlog to syslogd (default)\n" \
	"\t-L file\tlog to file\n" \
	"\t-f\trun in fordeground\n" \
	"\t-b\trun in background (default)\n" \
	"\t-c dir\tworking dir"

#define crontab_trivial_usage \
	"[-c dir] {file|-}|[-u|-l|-e|-d user]"
#define crontab_full_usage \
	"\tfile <opts>  replace crontab from file\n" \
	"\t-    <opts>  replace crontab from stdin\n" \
	"\t-u user      specify user\n" \
	"\t-l [user]    list crontab for user\n" \
	"\t-e [user]    edit crontab for user\n" \
	"\t-d [user]    delete crontab for user\n" \
	"\t-c dir       specify crontab directory"


#define cut_trivial_usage \
	"[OPTION]... [FILE]..."
#define cut_full_usage \
	"Prints selected fields from each input FILE to standard output.\n\n" \
	"Options:\n" \
	"\t-b LIST\t\tOutput only bytes from LIST\n" \
	"\t-c LIST\t\tOutput only characters from LIST\n" \
	"\t-d CHAR\t\tUse CHAR instead of tab as the field delimiter\n" \
	"\t-s\t\tOutput only the lines containing delimiter\n" \
	"\t-f N\t\tPrint only these fields\n" \
	"\t-n\t\tIgnored"
#define cut_example_usage \
	"$ echo \"Hello world\" | cut -f 1 -d ' '\n" \
	"Hello\n" \
	"$ echo \"Hello world\" | cut -f 2 -d ' '\n" \
	"world\n"

#ifdef CONFIG_FEATURE_DATE_ISOFMT
#define USAGE_DATE_ISOFMT(a) a
#else
#define USAGE_DATE_ISOFMT(a)
#endif

#define date_trivial_usage \
	"[OPTION]... [MMDDhhmm[[CC]YY][.ss]] [+FORMAT]"
#define date_full_usage \
	"Displays the current time in the given FORMAT, or sets the system date.\n" \
	"\nOptions:\n" \
	"\t-R\t\tOutputs RFC-822 compliant date string\n" \
	"\t-d STRING\tDisplays time described by STRING, not `now'\n" \
	USAGE_DATE_ISOFMT( \
		"\t-I[TIMESPEC]\tOutputs an ISO-8601 compliant date/time string\n" \
		"\t\t\tTIMESPEC=`date' (or missing) for date only,\n" \
		"\t\t\t`hours', `minutes', or `seconds' for date and,\n" \
		"\t\t\ttime to the indicated precision\n" \
		"\t-D hint\t\tUse 'hint' as date format, via strptime()\n" \
	) \
	"\t-s\t\tSets time described by STRING\n" \
	"\t-r FILE\t\tDisplays the last modification time of FILE\n" \
	"\t-u\t\tPrints or sets Coordinated Universal Time"
#define date_example_usage \
	"$ date\n" \
	"Wed Apr 12 18:52:41 MDT 2000\n"

#define dc_trivial_usage \
	"expression ..."
#define dc_full_usage \
	"This is a Tiny RPN calculator that understands the\n" \
	"following operations: +, add, -, sub, *, mul, /, div, %, mod, " \
	"**, exp, and, or, not, eor.\n" \
	"For example: 'dc 2 2 add' -> 4, and 'dc 8 8 \\* 2 2 + /' -> 16.\n" \
	"\nOptions:\n" \
	"p - Prints the value on the top of the stack, without altering the stack\n" \
	"f - Prints the entire contents of the stack without altering anything\n" \
	"o - Pops the value off the top of the stack and uses it to set the output radix\n" \
	"    Only 10 and 16 are supported"
#define dc_example_usage \
	"$ dc 2 2 + p\n" \
	"4\n" \
	"$ dc 8 8 \\* 2 2 + / p\n" \
	"16\n" \
	"$ dc 0 1 and p\n" \
	"0\n" \
	"$ dc 0 1 or p\n" \
	"1\n" \
	"$ echo 72 9 div 8 mul p | dc\n" \
	"64\n"

#define dd_trivial_usage \
	"[if=FILE] [of=FILE] [bs=N] [count=N] [skip=N]\n" \
	"\t  [seek=N] [conv=notrunc|noerror|sync]"
#define dd_full_usage \
	"Copy a file, converting and formatting according to options\n\n" \
	"\tif=FILE\t\tread from FILE instead of stdin\n" \
	"\tof=FILE\t\twrite to FILE instead of stdout\n" \
	"\tbs=N\t\tread and write N bytes at a time\n" \
	"\tcount=N\t\tcopy only N input blocks\n" \
	"\tskip=N\t\tskip N input blocks\n" \
	"\tseek=N\t\tskip N output blocks\n" \
	"\tconv=notrunc\tdon't truncate output file\n" \
	"\tconv=noerror\tcontinue after read errors\n" \
	"\tconv=sync\tpad blocks with zeros\n" \
	"\n" \
	"Numbers may be suffixed by c (x1), w (x2), b (x512), kD (x1000), k (x1024),\n" \
	"MD (x1000000), M (x1048576), GD (x1000000000) or G (x1073741824)"
#define dd_example_usage \
	"$ dd if=/dev/zero of=/dev/ram1 bs=1M count=4\n" \
	"4+0 records in\n" \
	"4+0 records out\n"

#define deallocvt_trivial_usage \
	"[N]"
#define deallocvt_full_usage \
	 "Deallocate unused virtual terminal /dev/ttyN"

#define delgroup_trivial_usage \
	"GROUP"
#define delgroup_full_usage \
	 "Deletes group GROUP from the system"

#define deluser_trivial_usage \
	"USER"
#define deluser_full_usage \
	 "Deletes user USER from the system"

#ifdef CONFIG_DEVFSD_FG_NP
#  define USAGE_DEVFSD_FG_NP(a) a
#else
#  define USAGE_DEVFSD_FG_NP(a)
#endif

#define devfsd_trivial_usage \
	"mntpnt [-v]" \
	USAGE_DEVFSD_FG_NP("[-fg][-np]" )
#define devfsd_full_usage \
	"Optional daemon for managing devfs permissions and old device name symlinks.\n" \
	"\nOptions:\n" \
	"\tmntpnt\tThe mount point where devfs is mounted.\n\n" \
	"\t-v\tPrint the protocol version numbers for devfsd\n" \
	"\t\tand the kernel-side protocol version and exits." \
	USAGE_DEVFSD_FG_NP( "\n\n\t-fg\tRun the daemon in the foreground.\n\n" \
	"\t-np\tExit  after  parsing  the configuration file\n" \
	"\t\tand processing synthetic REGISTER events.\n" \
	"\t\tDo not poll for events.")

#ifdef CONFIG_FEATURE_HUMAN_READABLE
#  define USAGE_HUMAN_READABLE(a) a
#  define USAGE_NOT_HUMAN_READABLE(a)
#else
#  define USAGE_HUMAN_READABLE(a)
#  define USAGE_NOT_HUMAN_READABLE(a) a
#endif
#define df_trivial_usage \
	"[-" USAGE_HUMAN_READABLE("hm") USAGE_NOT_HUMAN_READABLE("") "k] [FILESYSTEM ...]"
#define df_full_usage \
	"Print the filesystem space used and space available.\n\n" \
	"Options:\n" \
	USAGE_HUMAN_READABLE( \
	"\n\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G )\n" \
	"\t-m\tprint sizes in megabytes\n" \
	"\t-k\tprint sizes in kilobytes(default)") USAGE_NOT_HUMAN_READABLE( \
	"\n\t-k\tprint sizes in kilobytes(compatibility)")
#define df_example_usage \
	"$ df\n" \
	"Filesystem           1k-blocks      Used Available Use% Mounted on\n" \
	"/dev/sda3              8690864   8553540    137324  98% /\n" \
	"/dev/sda1                64216     36364     27852  57% /boot\n" \
	"$ df /dev/sda3\n" \
	"Filesystem           1k-blocks      Used Available Use% Mounted on\n" \
	"/dev/sda3              8690864   8553540    137324  98% /\n"

#define dirname_trivial_usage \
	"FILENAME"
#define dirname_full_usage \
	"Strips non-directory suffix from FILENAME"
#define dirname_example_usage \
	"$ dirname /tmp/foo\n" \
	"/tmp\n" \
	"$ dirname /tmp/foo/\n" \
	"/tmp\n"

#define dmesg_trivial_usage \
	"[-c] [-n LEVEL] [-s SIZE]"
#define dmesg_full_usage \
	"Prints or controls the kernel ring buffer\n\n" \
	"Options:\n" \
	"\t-c\t\tClears the ring buffer's contents after printing\n" \
	"\t-n LEVEL\tSets console logging level\n" \
	"\t-s SIZE\t\tUse a buffer of size SIZE"

#define dnsd_trivial_usage \
        "[-c config] [-t seconds] [-p port] [-i iface-ip] [-d]"
#define dnsd_full_usage \
        "Small and static DNS server daemon\n\n" \
	"Options:\n" \
	"\t-c\t\tconfig filename\n" \
	"\t-t\t\tTTL in seconds\n" \
	"\t-p\t\tlistening port\n" \
	"\t-i\t\tlistening iface ip (default all)\n" \
	"\t-d\t\tdaemonize"

#define dos2unix_trivial_usage \
	"[option] [FILE]"
#define dos2unix_full_usage \
	"Converts FILE from dos format to unix format.  When no option\n" \
	"is given, the input is converted to the opposite output format.\n" \
	"When no file is given, uses stdin for input and stdout for output.\n\n" \
	"Options:\n" \
	"\t-u\toutput will be in UNIX format\n" \
	"\t-d\toutput will be in DOS format"

#define dpkg_trivial_usage \
	"[-ilCPru] [-F option] package_name"
#define dpkg_full_usage \
	"dpkg is a utility to install, remove and manage Debian packages.\n\n" \
	"Options:\n" \
	"\t-i\t\tInstall the package\n" \
	"\t-l\t\tList of installed packages\n" \
	"\t-C\t\tConfigure an unpackaged package\n" \
	"\t-F depends\tIgnore depency problems\n" \
	"\t-P\t\tPurge all files of a package\n" \
	"\t-r\t\tRemove all but the configuration files for a package\n" \
	"\t-u\t\tUnpack a package, but don't configure it"

#define dpkg_deb_trivial_usage \
	"[-cefxX] FILE [argument]"
#define dpkg_deb_full_usage \
	"Perform actions on Debian packages (.debs)\n\n" \
	"Options:\n" \
	"\t-c\tList contents of filesystem tree\n" \
	"\t-e\tExtract control files to [argument] directory\n" \
	"\t-f\tDisplay control field name starting with [argument]\n" \
	"\t-x\tExtract packages filesystem tree to directory\n" \
	"\t-X\tVerbose extract"
#define dpkg_deb_example_usage \
	"$ dpkg-deb -X ./busybox_0.48-1_i386.deb /tmp\n"

#ifdef CONFIG_FEATURE_DU_DEFALT_BLOCKSIZE_1K
#define USAGE_DU_DEFALT_BLOCKSIZE_1k(a) a
#define USAGE_NOT_DU_DEFALT_BLOCKSIZE_1k(a)
#else
#define USAGE_DU_DEFALT_BLOCKSIZE_1k(a)
#define USAGE_NOT_DU_DEFALT_BLOCKSIZE_1k(a) a
#endif

#define du_trivial_usage \
	"[-aHLdclsx" USAGE_HUMAN_READABLE("hm") "k] [FILE]..."
#define du_full_usage \
	"Summarizes disk space used for each FILE and/or directory.\n" \
	"Disk space is printed in units of " \
	USAGE_DU_DEFALT_BLOCKSIZE_1k("1024") USAGE_NOT_DU_DEFALT_BLOCKSIZE_1k("512") \
	" bytes.\n\n" \
	"Options:\n" \
	"\t-a\tshow sizes of files in addition to directories\n" \
	"\t-H\tfollow symbolic links that are FILE command line args\n" \
	"\t-L\tfollow all symbolic links encountered\n" \
	"\t-d N\tlimit output to directories (and files with -a) of depth < N\n" \
	"\t-c\toutput a grand total\n" \
	"\t-l\tcount sizes many times if hard linked\n" \
	"\t-s\tdisplay only a total for each argument\n" \
	"\t-x\tskip directories on different filesystems\n" \
	USAGE_HUMAN_READABLE( \
	"\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G )\n" \
	"\t-m\tprint sizes in megabytes\n" ) \
	"\t-k\tprint sizes in kilobytes" USAGE_DU_DEFALT_BLOCKSIZE_1k("(default)")
#define du_example_usage \
	"$ du\n" \
	"16      ./CVS\n" \
	"12      ./kernel-patches/CVS\n" \
	"80      ./kernel-patches\n" \
	"12      ./tests/CVS\n" \
	"36      ./tests\n" \
	"12      ./scripts/CVS\n" \
	"16      ./scripts\n" \
	"12      ./docs/CVS\n" \
	"104     ./docs\n" \
	"2417    .\n"

#define dumpkmap_trivial_usage \
	"> keymap"
#define dumpkmap_full_usage \
	"Prints out a binary keyboard translation table to standard output."
#define dumpkmap_example_usage \
	"$ dumpkmap > keymap\n"

#define dumpleases_trivial_usage \
	"[-r|-a] [-f LEASEFILE]"
#define dumpleases_full_usage \
	"Displays the DHCP leases granted by udhcpd.\n\n" \
	"Options:\n" \
	"\t-f,\t--file=FILENAME\tLeases file to load\n" \
	"\t-r,\t--remaining\tInterpret lease times as time remaing\n" \
	"\t-a,\t--absolute\tInterpret lease times as expire time"

#define e2fsck_trivial_usage \
	"[-panyrcdfvstDFSV] [-b superblock] [-B blocksize] " \
	"[-I inode_buffer_blocks] [-P process_inode_size] " \
	"[-l|-L bad_blocks_file] [-C fd] [-j external_journal] " \
	"[-E extended-options] device"
#define e2fsck_full_usage \
	"Check a Linux ext2/ext3 file system.\n\n" \
	"Options:\n" \
	"\t-p\tAutomatic repair (no questions)\n" \
	"\t-n\tMake no changes to the filesystem\n" \
	"\t-y\tAssume 'yes' to all questions\n" \
	"\t-c\tCheck for bad blocks and add them to the badblock list\n" \
	"\t-f\tForce checking even if filesystem is marked clean\n" \
	"\t-v\tBe verbose\n" \
	"\t-b superblock\tUse alternative superblock\n" \
	"\t-B blocksize\tForce blocksize when looking for superblock\n" \
	"\t-j journal\tSet location of the external journal\n" \
	"\t-l file\tAdd to badblocks list\n" \
	"\t-L file\tSet badblocks list"

#ifdef CONFIG_FEATURE_FANCY_ECHO
#  define USAGE_FANCY_ECHO(a) a
#else
#  define USAGE_FANCY_ECHO(a)
#endif

#define echo_trivial_usage \
	USAGE_FANCY_ECHO("[-neE] ") "[ARG ...]"
#define echo_full_usage \
	"Prints the specified ARGs to stdout\n\n" \
	USAGE_FANCY_ECHO("Options:\n" \
	"\t-n\tsuppress trailing newline\n" \
	"\t-e\tinterpret backslash-escaped characters (i.e., \\t=tab)\n" \
	"\t-E\tdisable interpretation of backslash-escaped characters")
#define echo_example_usage \
	"$ echo \"Erik is cool\"\n" \
	"Erik is cool\n" \
	USAGE_FANCY_ECHO("$  echo -e \"Erik\\nis\\ncool\"\n" \
	"Erik\n" \
	"is\n" \
	"cool\n" \
	"$ echo \"Erik\\nis\\ncool\"\n" \
	"Erik\\nis\\ncool\n")

#define eject_trivial_usage \
	"[-t] [DEVICE]"
#define eject_full_usage \
	"Eject specified DEVICE (or default /dev/cdrom).\n\n" \
	"Options:\n" \
	"\t-t\tclose tray"

#define env_trivial_usage \
	"[-iu] [-] [name=value]... [command]"
#define env_full_usage \
	"Prints the current environment or runs a program after setting\n" \
	"up the specified environment.\n\n" \
	"Options:\n" \
	"\t-, -i\tstart with an empty environment\n" \
	"\t-u\tremove variable from the environment"

#define ether_wake_trivial_usage \
	"[-b] [-i iface] [-p aa:bb:cc:dd[:ee:ff]] MAC"
#define ether_wake_full_usage \
	"Send a magic packet to wake up sleeping machines.\n" \
	"MAC must be a station address (00:11:22:33:44:55) or\n" \
	"    a hostname with a known 'ethers' entry.\n\n" \
	"Options:\n" \
	"\t-b\t\tSend wake-up packet to the broadcast address\n" \
	"\t-i iface\tUse interface ifname instead of the default \"eth0\"\n" \
	"\t-p pass\tAppend the four or six byte password PW to the packet"

#define expr_trivial_usage \
	"EXPRESSION"
#define expr_full_usage \
	"Prints the value of EXPRESSION to standard output.\n\n" \
	"EXPRESSION may be:\n" \
	"\tARG1 |  ARG2	ARG1 if it is neither null nor 0, otherwise ARG2\n" \
	"\tARG1 &  ARG2	ARG1 if neither argument is null or 0, otherwise 0\n" \
	"\tARG1 <  ARG2	ARG1 is less than ARG2\n" \
	"\tARG1 <= ARG2	ARG1 is less than or equal to ARG2\n" \
	"\tARG1 =  ARG2	ARG1 is equal to ARG2\n" \
	"\tARG1 != ARG2	ARG1 is unequal to ARG2\n" \
	"\tARG1 >= ARG2	ARG1 is greater than or equal to ARG2\n" \
	"\tARG1 >  ARG2	ARG1 is greater than ARG2\n" \
	"\tARG1 +  ARG2	arithmetic sum of ARG1 and ARG2\n" \
	"\tARG1 -  ARG2	arithmetic difference of ARG1 and ARG2\n" \
	"\tARG1 *  ARG2	arithmetic product of ARG1 and ARG2\n" \
	"\tARG1 /  ARG2	arithmetic quotient of ARG1 divided by ARG2\n" \
	"\tARG1 %  ARG2	arithmetic remainder of ARG1 divided by ARG2\n" \
	"\tSTRING : REGEXP             anchored pattern match of REGEXP in STRING\n" \
	"\tmatch STRING REGEXP         same as STRING : REGEXP\n" \
	"\tsubstr STRING POS LENGTH    substring of STRING, POS counted from 1\n" \
	"\tindex STRING CHARS          index in STRING where any CHARS is found,\n" \
	"\t                            or 0\n" \
	"\tlength STRING               length of STRING\n" \
	"\tquote TOKEN                 interpret TOKEN as a string, even if\n" \
	"\t                            it is a keyword like `match' or an\n" \
	"\t                            operator like `/'\n" \
	"\t( EXPRESSION )              value of EXPRESSION\n\n" \
	"Beware that many operators need to be escaped or quoted for shells.\n" \
	"Comparisons are arithmetic if both ARGs are numbers, else\n" \
	"lexicographical.  Pattern matches return the string matched between \n" \
	"\\( and \\) or null; if \\( and \\) are not used, they return the number \n" \
	"of characters matched or 0."

#define fakeidentd_trivial_usage \
	"[-b ip] [STRING]"
#define fakeidentd_full_usage \
	"Returns a set string to auth requests\n\n" \
	"\t-b\tBind to ip address\n" \
	"\tSTRING\tThe ident answer string (default is nobody)"

#define false_trivial_usage \
	""
#define false_full_usage \
	"Return an exit code of FALSE (1)."
#define false_example_usage \
	"$ false\n" \
	"$ echo $?\n" \
	"1\n"

#define fbset_trivial_usage \
	"[options] [mode]"
#define fbset_full_usage \
	"Show and modify frame buffer settings"
#define fbset_example_usage \
	"$ fbset\n" \
	"mode \"1024x768-76\"\n" \
	"\t# D: 78.653 MHz, H: 59.949 kHz, V: 75.694 Hz\n" \
	"\tgeometry 1024 768 1024 768 16\n" \
	"\ttimings 12714 128 32 16 4 128 4\n" \
	"\taccel false\n" \
	"\trgba 5/11,6/5,5/0,0/0\n" \
	"endmode\n"

#define fdflush_trivial_usage \
	"DEVICE"
#define fdflush_full_usage \
	"Forces floppy disk drive to detect disk change"

#define fdformat_trivial_usage \
	"[-n] DEVICE"
#define fdformat_full_usage \
	"Low-level formats a floppy disk\n\n" \
	"Options:\n" \
	"\t-n\tDon't verify after format"

#define fdisk_trivial_usage \
	"[-luv] [-C CYLINDERS] [-H HEADS] [-S SECTORS] [-b SSZ] DISK"
#define fdisk_full_usage \
	"Change partition table\n" \
	"Options:\n" \
	"\t-l  List partition table(s)\n" \
	"\t-u  Give Start and End in sector (instead of cylinder) units\n" \
	"\t-s PARTITION  Give partition size(s) in blocks\n" \
	"\t-b 2048: (for certain MO disks) use 2048-byte sectors\n" \
	"\t-C CYLINDERS  Set the number of cylinders\n" \
	"\t-H HEADS  Set the number of heads\n" \
	"\t-S SECTORS  Set the number of sectors\n" \
	"\t-v  Give fdisk version"

#ifdef CONFIG_FEATURE_FIND_TYPE
#  define USAGE_FIND_TYPE(a) a
#else
#  define USAGE_FIND_TYPE(a)
#endif
#ifdef CONFIG_FEATURE_FIND_PERM
#  define USAGE_FIND_PERM(a) a
#else
#  define USAGE_FIND_PERM(a)
#endif
#ifdef CONFIG_FEATURE_FIND_MTIME
#  define USAGE_FIND_MTIME(a) a
#else
#  define USAGE_FIND_MTIME(a)
#endif
#ifdef CONFIG_FEATURE_FIND_MMIN
  #define USAGE_FIND_MMIN(a) a
#else
  #define USAGE_FIND_MMIN(a)
#endif
#ifdef CONFIG_FEATURE_FIND_NEWER
#  define USAGE_FIND_NEWER(a) a
#else
#  define USAGE_FIND_NEWER(a)
#endif
#ifdef CONFIG_FEATURE_FIND_INUM
#  define USAGE_FIND_INUM(a) a
#else
#  define USAGE_FIND_INUM(a)
#endif
#ifdef CONFIG_FEATURE_FIND_EXEC
#  define USAGE_FIND_EXEC(a) a
#else
#  define USAGE_FIND_EXEC(a)
#endif

#define find_trivial_usage \
	"[PATH...] [EXPRESSION]"
#define find_full_usage \
	"Search for files in a directory hierarchy.  The default PATH is\n" \
	"the current directory; default EXPRESSION is '-print'\n" \
	"\nEXPRESSION may consist of:\n" \
	"\t-follow\t\tDereference symbolic links\n" \
	"\t-name PATTERN\tFile name (leading directories removed) matches PATTERN\n" \
	"\t-print\t\tPrint (default and assumed)\n" \
	USAGE_FIND_TYPE( \
	"\n\t-type X\t\tFiletype matches X (where X is one of: f,d,l,b,c,...)" \
) USAGE_FIND_PERM( \
	"\n\t-perm PERMS\tPermissions match any of (+NNN); all of (-NNN);\n\t\t\tor exactly (NNN)" \
) USAGE_FIND_MTIME( \
	"\n\t-mtime DAYS\tModified time is greater than (+N); less than (-N);\n\t\t\tor exactly (N) days" \
) USAGE_FIND_MMIN( \
	"\n\t-mmin MINS\tModified time is greater than (+N); less than (-N);\n\t\t\tor exactly (N) minutes" \
) USAGE_FIND_NEWER( \
	"\n\t-newer FILE\tModified time is more recent than FILE's" \
) USAGE_FIND_INUM( \
	"\n\t-inum N\t\tFile has inode number N" \
) USAGE_FIND_EXEC( \
	"\n\t-exec CMD\tExecute CMD with all instances of {} replaced by the" \
	"\n\t\t\tfiles matching EXPRESSION")

#define find_example_usage \
	"$ find / -name passwd\n" \
	"/etc/passwd\n"

#define fold_trivial_usage \
	"[-bs] [-w WIDTH] [FILE]"
#define fold_full_usage \
	"Wrap input lines in each FILE (standard input by default), writing to\n" \
	"standard output.\n\n" \
	"Options:\n" \
	"\t-b\tcount bytes rather than columns\n" \
	"\t-s\tbreak at spaces\n" \
	"\t-w\tuse WIDTH columns instead of 80"

#define free_trivial_usage \
	""
#define free_full_usage \
	"Displays the amount of free and used system memory"
#define free_example_usage \
	"$ free\n" \
	"              total         used         free       shared      buffers\n" \
	"  Mem:       257628       248724         8904        59644        93124\n" \
	" Swap:       128516         8404       120112\n" \
	"Total:       386144       257128       129016\n" \

#define freeramdisk_trivial_usage \
	"DEVICE"
#define freeramdisk_full_usage \
	"Frees all memory used by the specified ramdisk."
#define freeramdisk_example_usage \
	"$ freeramdisk /dev/ram2\n"

#define fsck_trivial_usage \
	"[-ANPRTV] [ -C [ fd ] ] [-t fstype] [fs-options] [filesys ...]"
#define fsck_full_usage \
	"Check and repair filesystems.\n\n" \
	"Options:\n" \
	"\t-A\tWalk /etc/fstab and check all filesystems\n" \
	"\t-N\tDon't execute, just show what would be done\n" \
	"\t-P\tWhen using -A, check filesystems in parallel\n" \
	"\t-R\tWhen using -A, skip the root filesystem\n" \
	"\t-T\tDon't show title on startup\n" \
	"\t-V\tVerbose mode\n" \
	"\t-C\tWrite status information to specified filedescriptor\n" \
	"\t-t\tList of filesystem types to check"

#define fsck_minix_trivial_usage \
	"[-larvsmf] /dev/name"
#define fsck_minix_full_usage \
	"Performs a consistency check for MINIX filesystems.\n\n" \
	"Options:\n" \
	"\t-l\tLists all filenames\n" \
	"\t-r\tPerform interactive repairs\n" \
	"\t-a\tPerform automatic repairs\n" \
	"\t-v\tverbose\n" \
	"\t-s\tOutputs super-block information\n" \
	"\t-m\tActivates MINIX-like \"mode not cleared\" warnings\n" \
	"\t-f\tForce file system check"

#define ftpget_trivial_usage \
	"[options] remote-host local-file remote-file"
#define ftpget_full_usage \
	"Retrieve a remote file via FTP.\n\n" \
	"Options:\n" \
	"\t-c, --continue         Continue a previous transfer\n" \
	"\t-v, --verbose          Verbose\n" \
	"\t-u, --username         Username to be used\n" \
	"\t-p, --password         Password to be used\n" \
	"\t-P, --port             Port number to be used"

#define ftpput_trivial_usage \
	"[options] remote-host remote-file local-file"
#define ftpput_full_usage \
	"Store a local file on a remote machine via FTP.\n\n" \
	"Options:\n" \
	"\t-v, --verbose          Verbose\n" \
	"\t-u, --username         Username to be used\n" \
	"\t-p, --password         Password to be used\n" \
	"\t-P, --port             Port number to be used"

#define fuser_trivial_usage \
	"[options] file OR port/proto"
#define fuser_full_usage \
	"Options:\n" \
	"\t-m         Show all processes on the same mounted fs\n" \
	"\t-k         Kill all processes that match.\n" \
	"\t-s         Don't print or kill anything.\n" \
	"\t-4         When using port/proto only search IPv4 space\n" \
	"\t-6         When using port/proto only search IPv6 space\n" \
	"\t-SIGNAL    When used with -k, this signal will be used to kill"

#define getopt_trivial_usage \
	"[OPTIONS]..."
#define getopt_full_usage \
	"Parse command options\n" \
	"\t-a, --alternative		Allow long options starting with single -\n" \
	"\t-l, --longoptions=longopts	Long options to be recognized\n" \
	"\t-n, --name=progname		The name under which errors are reported\n" \
	"\t-o, --options=optstring	Short options to be recognized\n" \
	"\t-q, --quiet			Disable error reporting by getopt(3)\n" \
	"\t-Q, --quiet-output		No normal output\n" \
	"\t-s, --shell=shell		Set shell quoting conventions\n" \
	"\t-T, --test			Test for getopt(1) version\n" \
	"\t-u, --unquoted		Do not quote the output"
#define getopt_example_usage \
	"$ cat getopt.test\n" \
	"#!/bin/sh\n" \
	"GETOPT=`getopt -o ab:c:: --long a-long,b-long:,c-long:: \\\n" \
	"       -n 'example.busybox' -- \"$@\"`\n" \
	"if [ $? != 0 ] ; then  exit 1 ; fi\n" \
	"eval set -- \"$GETOPT\"\n" \
	"while true ; do\n" \
	" case $1 in\n" \
	"   -a|--a-long) echo \"Option a\" ; shift ;;\n" \
	"   -b|--b-long) echo \"Option b, argument `$2'\" ; shift 2 ;;\n" \
	"   -c|--c-long)\n" \
	"     case \"$2\" in\n" \
	"       \"\") echo \"Option c, no argument\"; shift 2 ;;\n" \
	"       *)  echo \"Option c, argument `$2'\" ; shift 2 ;;\n" \
	"     esac ;;\n" \
	"   --) shift ; break ;;\n" \
	"   *) echo \"Internal error!\" ; exit 1 ;;\n" \
	" esac\n" \
	"done\n"

#define getty_trivial_usage \
	"[OPTIONS]... baud_rate,... line [termtype]"
#define getty_full_usage \
	"Opens a tty, prompts for a login name, then invokes /bin/login\n\n" \
	"Options:\n" \
	"\t-h\t\tEnable hardware (RTS/CTS) flow control\n" \
	"\t-i\t\tDo not display /etc/issue before running login\n" \
	"\t-L\t\tLocal line, so do not do carrier detect\n" \
	"\t-m\t\tGet baud rate from modem's CONNECT status message\n" \
	"\t-w\t\tWait for a CR or LF before sending /etc/issue\n" \
	"\t-n\t\tDo not prompt the user for a login name\n" \
	"\t-f issue_file\tDisplay issue_file instead of /etc/issue\n" \
	"\t-l login_app\tInvoke login_app instead of /bin/login\n" \
	"\t-t timeout\tTerminate after timeout if no username is read\n" \
	"\t-I initstring\tSets the init string to send before anything else\n" \
	"\t-H login_host\tLog login_host into the utmp file as the hostname"

#if ENABLE_FEATURE_GREP_EGREP_ALIAS
#define USAGE_GREP_E(a) a
#else
#define USAGE_GREP_E(a)
#endif
#if ENABLE_FEATURE_GREP_CONTEXT
#define USAGE_GREP_CTX(a) a
#else
#define USAGE_GREP_CTX(a)
#endif
#define grep_trivial_usage \
	"[-ihHnqvs" \
	USAGE_GREP_E("E") \
	USAGE_GREP_CTX("ABC") \
	"] PATTERN [FILEs...]"
#define grep_full_usage \
	"Search for PATTERN in each FILE or standard input.\n\n" \
	"Options:\n" \
	"\t-H\tprefix output lines with filename where match was found\n" \
	"\t-h\tsuppress the prefixing filename on output\n" \
	"\t-i\tignore case distinctions\n" \
	"\t-l\tlist names of files that match\n" \
	"\t-L\tlist names of files that do not match\n" \
	"\t-n\tprint line number with output lines\n" \
	"\t-q\tbe quiet. Returns 0 if PATTERN was found, 1 otherwise\n" \
	"\t-v\tselect non-matching lines\n" \
	"\t-s\tsuppress file open/read error messages\n" \
	"\t-c\tonly print count of matching lines\n" \
	"\t-f\tread PATTERN from file\n" \
	"\t-e\tPATTERN is a regular expression\n" \
	"\t-F\tPATTERN is a set of newline-separated strings" \
	USAGE_GREP_E("\n\t-E\tPATTERN is an extended regular expression") \
	USAGE_GREP_CTX("\n\t-A\tprint NUM lines of trailing context") \
	USAGE_GREP_CTX("\n\t-B\tprint NUM lines of leading context") \
	USAGE_GREP_CTX("\n\t-C\tprint NUM lines of output context")

#define grep_example_usage \
	"$ grep root /etc/passwd\n" \
	"root:x:0:0:root:/root:/bin/bash\n" \
	"$ grep ^[rR]oo. /etc/passwd\n" \
	"root:x:0:0:root:/root:/bin/bash\n"

#define gunzip_trivial_usage \
	"[OPTION]... FILE"
#define gunzip_full_usage \
	"Uncompress FILE (or standard input if FILE is '-').\n\n" \
	"Options:\n" \
	"\t-c\tWrite output to standard output\n" \
	"\t-f\tForce read when source is a terminal\n" \
	"\t-t\tTest compressed file integrity"
#define gunzip_example_usage \
	"$ ls -la /tmp/BusyBox*\n" \
	"-rw-rw-r--    1 andersen andersen   557009 Apr 11 10:55 /tmp/BusyBox-0.43.tar.gz\n" \
	"$ gunzip /tmp/BusyBox-0.43.tar.gz\n" \
	"$ ls -la /tmp/BusyBox*\n" \
	"-rw-rw-r--    1 andersen andersen  1761280 Apr 14 17:47 /tmp/BusyBox-0.43.tar\n"

#define gzip_trivial_usage \
	"[OPTION]... [FILE]..."
#define gzip_full_usage \
	"Compress FILE(s) with maximum compression.\n" \
	"When FILE is '-' or unspecified, reads standard input.  Implies -c.\n\n" \
	"Options:\n" \
	"\t-c\tWrite output to standard output instead of FILE.gz\n" \
	"\t-d\tDecompress\n" \
	"\t-f\tForce write when destination is a terminal"
#define gzip_example_usage \
	"$ ls -la /tmp/busybox*\n" \
	"-rw-rw-r--    1 andersen andersen  1761280 Apr 14 17:47 /tmp/busybox.tar\n" \
	"$ gzip /tmp/busybox.tar\n" \
	"$ ls -la /tmp/busybox*\n" \
	"-rw-rw-r--    1 andersen andersen   554058 Apr 14 17:49 /tmp/busybox.tar.gz\n"

#define halt_trivial_usage \
	"[-d<delay>] [-n<nosync>] [-f<force>]"
#define halt_full_usage \
	"Halt the system.\n" \
	"Options:\n" \
	"\t-d\t\tdelay interval for halting\n" \
	"\t-n\t\tno call to sync()\n" \
	"\t-f\t\tforce halt (don't go through init)"

#ifdef CONFIG_FEATURE_HDPARM_GET_IDENTITY
#define USAGE_HDPARM_IDENT(a) a
#else
#define USAGE_HDPARM_IDENT(a)
#endif

#ifdef CONFIG_FEATURE_HDPARM_HDIO_SCAN_HWIF
#define USAGE_SCAN_HWIF(a) a
#else
#define USAGE_SCAN_HWIF(a)
#endif

#ifdef CONFIG_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF
#define USAGE_UNREGISTER_HWIF(a) a
#else
#define USAGE_UNREGISTER_HWIF(a)
#endif

#ifdef CONFIG_FEATURE_HDPARM_HDIO_DRIVE_RESET
#define USAGE_DRIVE_RESET(a) a
#else
#define USAGE_DRIVE_RESET(a)
#endif

#ifdef CONFIG_FEATURE_HDPARM_HDIO_TRISTATE_HWIF
#define USAGE_TRISTATE_HWIF(a) a
#else
#define USAGE_TRISTATE_HWIF(a)
#endif

#ifdef CONFIG_FEATURE_HDPARM_HDIO_GETSET_DMA
#define USAGE_GETSET_DMA(a) a
#else
#define USAGE_GETSET_DMA(a)
#endif

#define hdparm_trivial_usage \
	"[options] [device] .."
#define hdparm_full_usage \
	"Options:" \
	"\t-a   get/set fs readahead\n" \
	"\t-A   set drive read-lookahead flag (0/1)\n" \
	"\t-b   get/set bus state (0 == off, 1 == on, 2 == tristate)\n" \
	"\t-B   set Advanced Power Management setting (1-255)\n" \
	"\t-c   get/set IDE 32-bit IO setting\n" \
	"\t-C   check IDE power mode status\n" \
	USAGE_GETSET_DMA("\t-d   get/set using_dma flag\n") \
	"\t-D   enable/disable drive defect-mgmt\n" \
	"\t-f   flush buffer cache for device on exit\n" \
	"\t-g   display drive geometry\n" \
	"\t-h   display terse usage information\n" \
	"\t-i   display drive identification\n" \
	USAGE_HDPARM_IDENT("\t-I   detailed/current information directly from drive\n") \
	USAGE_HDPARM_IDENT("\t-Istdin  similar to -I, but wants /proc/ide/" "*" "/hd?/identify as input\n") \
	"\t-k   get/set keep_settings_over_reset flag (0/1)\n" \
	"\t-K   set drive keep_features_over_reset flag (0/1)\n" \
	"\t-L   set drive doorlock (0/1) (removable harddisks only)\n" \
	"\t-m   get/set multiple sector count\n" \
	"\t-n   get/set ignore-write-errors flag (0/1)\n" \
	"\t-p   set PIO mode on IDE interface chipset (0,1,2,3,4,...)\n" \
	"\t-P   set drive prefetch count\n" \
	"\t-q   change next setting quietly\n" \
	"\t-Q   get/set DMA tagged-queuing depth (if supported)\n" \
	"\t-r   get/set readonly flag (DANGEROUS to set)\n" \
	USAGE_SCAN_HWIF("\t-R   register an IDE interface (DANGEROUS)\n") \
	"\t-S   set standby (spindown) timeout\n" \
	"\t-t   perform device read timings\n" \
	"\t-T   perform cache read timings\n" \
	"\t-u   get/set unmaskirq flag (0/1)\n" \
	USAGE_UNREGISTER_HWIF("\t-U   un-register an IDE interface (DANGEROUS)\n") \
	"\t-v   defaults; same as -mcudkrag for IDE drives\n" \
	"\t-V   display program version and exit immediately\n" \
	USAGE_DRIVE_RESET("\t-w   perform device reset (DANGEROUS)\n") \
	"\t-W   set drive write-caching flag (0/1) (DANGEROUS)\n" \
	USAGE_TRISTATE_HWIF("\t-x   tristate device for hotswap (0/1) (DANGEROUS)\n") \
	"\t-X   set IDE xfer mode (DANGEROUS)\n" \
	"\t-y   put IDE drive in standby mode\n" \
	"\t-Y   put IDE drive to sleep\n" \
	"\t-Z   disable Seagate auto-powersaving mode\n" \
	"\t-z   re-read partition table"

#ifdef CONFIG_FEATURE_FANCY_HEAD
#define USAGE_FANCY_HEAD(a) a
#else
#define USAGE_FANCY_HEAD(a)
#endif

#define head_trivial_usage \
	"[OPTION]... [FILE]..."
#define head_full_usage \
	"Print first 10 lines of each FILE to standard output.\n" \
	"With more than one FILE, precede each with a header giving the\n" \
	"file name. With no FILE, or when FILE is -, read standard input.\n\n" \
	"Options:\n" \
	"\t-n NUM\t\tPrint first NUM lines instead of first 10" \
	USAGE_FANCY_HEAD( \
	"\n\t-c NUM\t\toutput the first NUM bytes\n" \
	"\t-q\t\tnever output headers giving file names\n" \
	"\t-v\t\talways output headers giving file names" )
#define head_example_usage \
	"$ head -n 2 /etc/passwd\n" \
	"root:x:0:0:root:/root:/bin/bash\n" \
	"daemon:x:1:1:daemon:/usr/sbin:/bin/sh\n"

#define hexdump_trivial_usage \
	"[-[bcCdefnosvx]] [OPTION] FILE"
#define hexdump_full_usage \
	"The hexdump utility is a filter which displays the specified files,\n" \
	"or the standard input, if no files are specified, in a user specified\n" \
	"format\n" \
	"\t-b\t\tOne-byte octal display\n" \
	"\t-c\t\tOne-byte character display\n" \
	"\t-C\t\tCanonical hex+ASCII, 16 bytes per line\n" \
	"\t-d\t\tTwo-byte decimal display\n" \
	"\t-e FORMAT STRING\n" \
	"\t-f FORMAT FILE\n" \
	"\t-n LENGTH\tInterpret only length bytes of input\n" \
	"\t-o\t\tTwo-byte octal display\n" \
	"\t-s OFFSET\tSkip offset byte\n" \
	"\t-v\t\tdisplay all input data\n" \
	"\t-x\t\tTwo-byte hexadecimal display"

#define hostid_trivial_usage \
	""
#define hostid_full_usage \
	"Print out a unique 32-bit identifier for the machine."

#define hostname_trivial_usage \
	"[OPTION] {hostname | -F FILE}"
#define hostname_full_usage \
	"Get or set the hostname or DNS domain name. If a hostname is given\n" \
	"(or FILE with the -F parameter), the host name will be set.\n\n" \
	"Options:\n" \
	"\t-s\tShort\n" \
	"\t-i\tAddresses for the hostname\n" \
	"\t-d\tDNS domain name\n" \
	"\t-f\tFully qualified domain name\n" \
	"\t-F FILE\tUse the contents of FILE to specify the hostname"
#define hostname_example_usage \
	"$ hostname\n" \
	"sage\n"

#ifdef CONFIG_FEATURE_HTTPD_BASIC_AUTH
#  define USAGE_HTTPD_BASIC_AUTH(a) a
#  ifdef CONFIG_FEATURE_HTTPD_AUTH_MD5
#    define USAGE_HTTPD_AUTH_MD5(a) a
#  else
#    define USAGE_HTTPD_AUTH_MD5(a)
#  endif
#else
#  define USAGE_HTTPD_BASIC_AUTH(a)
#  define USAGE_HTTPD_AUTH_MD5(a)
#endif
#ifdef CONFIG_FEATURE_HTTPD_USAGE_FROM_INETD_ONLY
#  define USAGE_HTTPD_STANDALONE(a)
#  define USAGE_HTTPD_SETUID(a)
#else
#  define USAGE_HTTPD_STANDALONE(a) a
#  ifdef CONFIG_FEATURE_HTTPD_SETUID
#    define USAGE_HTTPD_SETUID(a) a
#  else
#    define USAGE_HTTPD_SETUID(a)
#  endif
#endif
#define httpd_trivial_usage \
	"[-c <conf file>]" \
	USAGE_HTTPD_STANDALONE(" [-p <port>]") \
	USAGE_HTTPD_SETUID(" [-u user]") \
	USAGE_HTTPD_BASIC_AUTH(" [-r <realm>]") \
	USAGE_HTTPD_AUTH_MD5(" [-m pass]") \
	" [-h home]" \
	" [-d/-e <string>]"
#define httpd_full_usage \
	"Listens for incoming http server requests.\n\n" \
	"Options:\n" \
	"\t-c FILE\t\tSpecifies configuration file. (default httpd.conf)\n" \
	USAGE_HTTPD_STANDALONE("\t-p PORT\tServer port (default 80)\n") \
	USAGE_HTTPD_SETUID("\t-u USER\tSet uid to USER after listening privileges port\n") \
	USAGE_HTTPD_BASIC_AUTH("\t-r REALM\tAuthentication Realm for Basic Authentication\n") \
	USAGE_HTTPD_AUTH_MD5("\t-m PASS\t\tCrypt PASS with md5 algorithm\n") \
	"\t-h HOME  \tSpecifies http HOME directory (default ./)\n" \
	"\t-e STRING\tHtml encode STRING\n" \
	"\t-d STRING\tURL decode STRING"

#define hwclock_trivial_usage \
	"[-r|--show] [-s|--hctosys] [-w|--systohc] [-l|--localtime] [-u|--utc]"
#define hwclock_full_usage \
	"Query and set the hardware clock (RTC)\n\n" \
	"Options:\n" \
	"\t-r\tread hardware clock and print result\n" \
	"\t-s\tset the system time from the hardware clock\n" \
	"\t-w\tset the hardware clock to the current system time\n" \
	"\t-u\tthe hardware clock is kept in coordinated universal time\n" \
	"\t-l\tthe hardware clock is kept in local time"

#ifdef CONFIG_SELINUX
#  define USAGE_SELINUX(a) a
#else
#  define USAGE_SELINUX(a)
#endif

#define id_trivial_usage \
	"[OPTIONS]... [USERNAME]"
#define id_full_usage \
	"Print information for USERNAME or the current user\n\n" \
	"Options:\n" \
	USAGE_SELINUX("\t-c\tprints only the security context\n") \
	"\t-g\tprints only the group ID\n" \
	"\t-u\tprints only the user ID\n" \
	"\t-n\tprint a name instead of a number\n" \
	"\t-r\tprints the real user ID instead of the effective ID"
#define id_example_usage \
	"$ id\n" \
	"uid=1000(andersen) gid=1000(andersen)\n"

#ifdef CONFIG_FEATURE_IFCONFIG_SLIP
#  define USAGE_SIOCSKEEPALIVE(a) a
#else
#  define USAGE_SIOCSKEEPALIVE(a)
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ
#  define USAGE_IFCONFIG_MII(a) a
#else
#  define USAGE_IFCONFIG_MII(a)
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_HW
#  define USAGE_IFCONFIG_HW(a) a
#else
#  define USAGE_IFCONFIG_HW(a)
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_STATUS
#  define USAGE_IFCONFIG_OPT_A(a) a
#else
#  define USAGE_IFCONFIG_OPT_A(a)
#endif
#ifdef CONFIG_FEATURE_IPV6
#  define USAGE_IPV6(a) a
#else
#  define USAGE_IPV6(a)
#endif

#define ifconfig_trivial_usage \
	USAGE_IFCONFIG_OPT_A("[-a]") " <interface> [<address>]"
#define ifconfig_full_usage \
	"configure a network interface\n\n" \
	"Options:\n" \
	USAGE_IPV6("[add <address>[/<prefixlen>]]\n") \
	USAGE_IPV6("[del <address>[/<prefixlen>]]\n") \
	"\t[[-]broadcast [<address>]]  [[-]pointopoint [<address>]]\n" \
	"\t[netmask <address>]  [dstaddr <address>]\n" \
	USAGE_SIOCSKEEPALIVE("\t[outfill <NN>] [keepalive <NN>]\n") \
	"\t" USAGE_IFCONFIG_HW("[hw ether <address>]  ") \
	"[metric <NN>]  [mtu <NN>]\n" \
	"\t[[-]trailers]  [[-]arp]  [[-]allmulti]\n" \
	"\t[multicast]  [[-]promisc]  [txqueuelen <NN>]  [[-]dynamic]\n" \
	USAGE_IFCONFIG_MII("\t[mem_start <NN>]  [io_addr <NN>]  [irq <NN>]\n") \
	"\t[up|down] ..."

#define ifup_trivial_usage \
	"<-ahinv> <ifaces...>"
#define ifup_full_usage \
	"ifup <options> <ifaces...>\n\n" \
	"Options:\n" \
	"\t-h\tthis help\n" \
	"\t-a\tde/configure all interfaces automatically\n" \
	"\t-i FILE\tuse FILE for interface definitions\n" \
	"\t-n\tprint out what would happen, but don't do it\n" \
	"\t\t\t(note that this option doesn't disable mappings)\n" \
	"\t-v\tprint out what would happen before doing it\n" \
	"\t-m\tdon't run any mappings\n" \
	"\t-f\tforce de/configuration"

#define ifdown_trivial_usage \
	"<-ahinv> <ifaces...>"
#define ifdown_full_usage \
	"ifdown <options> <ifaces...>\n\n" \
	"Options:\n" \
	"\t-h\tthis help\n" \
	"\t-a\tde/configure all interfaces automatically\n" \
	"\t-i FILE\tuse FILE for interface definitions\n" \
	"\t-n\tprint out what would happen, but don't do it\n" \
	"\t\t(note that this option doesn't disable mappings)\n" \
	"\t-v\tprint out what would happen before doing it\n" \
	"\t-m\tdon't run any mappings\n" \
	"\t-f\tforce de/configuration"

#define inetd_trivial_usage \
	"[-f] [-q len] [conf]"
#define inetd_full_usage \
	"Listens for network connections and launches programs\n\n" \
	"Option:\n" \
	"\t-f\tRun as a foreground progress\n" \
	"\t-q\tSets the size of the socket listen queue to\n" \
	"\t\tthe specified value. Default is 128"

#define init_trivial_usage \
	""
#define init_full_usage \
	"Init is the parent of all processes."
#define init_notes_usage \
"This version of init is designed to be run only by the kernel.\n" \
"\n" \
"BusyBox init doesn't support multiple runlevels.  The runlevels field of\n" \
"the /etc/inittab file is completely ignored by BusyBox init. If you want \n" \
"runlevels, use sysvinit.\n" \
"\n" \
"BusyBox init works just fine without an inittab.  If no inittab is found, \n" \
"it has the following default behavior:\n" \
"\n" \
"	::sysinit:/etc/init.d/rcS\n" \
"	::askfirst:/bin/sh\n" \
"	::ctrlaltdel:/sbin/reboot\n" \
"	::shutdown:/sbin/swapoff -a\n" \
"	::shutdown:/bin/umount -a -r\n" \
"	::restart:/sbin/init\n" \
"\n" \
"if it detects that /dev/console is _not_ a serial console, it will also run:\n" \
"\n" \
"	tty2::askfirst:/bin/sh\n" \
"	tty3::askfirst:/bin/sh\n" \
"	tty4::askfirst:/bin/sh\n" \
"\n" \
"If you choose to use an /etc/inittab file, the inittab entry format is as follows:\n" \
"\n" \
"	<id>:<runlevels>:<action>:<process>\n" \
"\n" \
"	<id>:\n" \
"\n" \
"		WARNING: This field has a non-traditional meaning for BusyBox init!\n" \
"		The id field is used by BusyBox init to specify the controlling tty for\n" \
"		the specified process to run on.  The contents of this field are\n" \
"		appended to \"/dev/\" and used as-is.  There is no need for this field to\n" \
"		be unique, although if it isn't you may have strange results.  If this\n" \
"		field is left blank, the controlling tty is set to the console.  Also\n" \
"		note that if BusyBox detects that a serial console is in use, then only\n" \
"		entries whose controlling tty is either the serial console or /dev/null\n" \
"		will be run.  BusyBox init does nothing with utmp.  We don't need no\n" \
"		stinkin' utmp.\n" \
"\n" \
"	<runlevels>:\n" \
"\n" \
"		The runlevels field is completely ignored.\n" \
"\n" \
"	<action>:\n" \
"\n" \
"		Valid actions include: sysinit, respawn, askfirst, wait,\n" \
"		once, restart, ctrlaltdel, and shutdown.\n" \
"\n" \
"		The available actions can be classified into two groups: actions\n" \
"		that are run only once, and actions that are re-run when the specified\n" \
"		process exits.\n" \
"\n" \
"		Run only-once actions:\n" \
"\n" \
"			'sysinit' is the first item run on boot.  init waits until all\n" \
"			sysinit actions are completed before continuing.  Following the\n" \
"			completion of all sysinit actions, all 'wait' actions are run.\n" \
"			'wait' actions, like  'sysinit' actions, cause init to wait until\n" \
"			the specified task completes.  'once' actions are asynchronous,\n" \
"			therefore, init does not wait for them to complete.  'restart' is\n" \
"			the action taken to restart the init process.  By default this should\n" \
"			simply run /sbin/init, but can be a script which runs pivot_root or it\n" \
"			can do all sorts of other interesting things.  The 'ctrlaltdel' init\n" \
"			actions are run when the system detects that someone on the system\n" \
"                       console has pressed the CTRL-ALT-DEL key combination.  Typically one\n" \
"                       wants to run 'reboot' at this point to cause the system to reboot.\n" \
"			Finally the 'shutdown' action specifies the actions to taken when\n" \
"                       init is told to reboot.  Unmounting filesystems and disabling swap\n" \
"                       is a very good here\n" \
"\n" \
"		Run repeatedly actions:\n" \
"\n" \
"			'respawn' actions are run after the 'once' actions.  When a process\n" \
"			started with a 'respawn' action exits, init automatically restarts\n" \
"			it.  Unlike sysvinit, BusyBox init does not stop processes from\n" \
"			respawning out of control.  The 'askfirst' actions acts just like\n" \
"			respawn, except that before running the specified process it\n" \
"			displays the line \"Please press Enter to activate this console.\"\n" \
"			and then waits for the user to press enter before starting the\n" \
"			specified process.\n" \
"\n" \
"		Unrecognized actions (like initdefault) will cause init to emit an\n" \
"		error message, and then go along with its business.  All actions are\n" \
"		run in the order they appear in /etc/inittab.\n" \
"\n" \
"	<process>:\n" \
"\n" \
"		Specifies the process to be executed and its command line.\n" \
"\n" \
"Example /etc/inittab file:\n" \
"\n" \
"	# This is run first except when booting in single-user mode.\n" \
"	#\n" \
"	::sysinit:/etc/init.d/rcS\n" \
"	\n" \
"	# /bin/sh invocations on selected ttys\n" \
"	#\n" \
"	# Start an \"askfirst\" shell on the console (whatever that may be)\n" \
"	::askfirst:-/bin/sh\n" \
"	# Start an \"askfirst\" shell on /dev/tty2-4\n" \
"	tty2::askfirst:-/bin/sh\n" \
"	tty3::askfirst:-/bin/sh\n" \
"	tty4::askfirst:-/bin/sh\n" \
"	\n" \
"	# /sbin/getty invocations for selected ttys\n" \
"	#\n" \
"	tty4::respawn:/sbin/getty 38400 tty4\n" \
"	tty5::respawn:/sbin/getty 38400 tty5\n" \
"	\n" \
"	\n" \
"	# Example of how to put a getty on a serial line (for a terminal)\n" \
"	#\n" \
"	#::respawn:/sbin/getty -L ttyS0 9600 vt100\n" \
"	#::respawn:/sbin/getty -L ttyS1 9600 vt100\n" \
"	#\n" \
"	# Example how to put a getty on a modem line.\n" \
"	#::respawn:/sbin/getty 57600 ttyS2\n" \
"	\n" \
"	# Stuff to do when restarting the init process\n" \
"	::restart:/sbin/init\n" \
"	\n" \
"	# Stuff to do before rebooting\n" \
"	::ctrlaltdel:/sbin/reboot\n" \
"	::shutdown:/bin/umount -a -r\n" \
"	::shutdown:/sbin/swapoff -a\n"

#ifdef CONFIG_FEATURE_INSMOD_LOAD_MAP
#  define USAGE_INSMOD_MAP(a) a
#else
#  define USAGE_INSMOD_MAP(a)
#endif
#define insmod_trivial_usage \
	"[OPTION]... MODULE [symbol=value]..."
#define insmod_full_usage \
	"Loads the specified kernel modules into the kernel.\n\n" \
	"Options:\n" \
	"\t-f\tForce module to load into the wrong kernel version\n" \
	"\t-k\tMake module autoclean-able\n" \
	"\t-v\tverbose output\n"  \
	"\t-q\tquiet output\n" \
	"\t-L\tLock to prevent simultaneous loads of a module\n" \
	USAGE_INSMOD_MAP("\t-m\tOutput load map to stdout\n") \
	"\t-o NAME\tSet internal module name to NAME\n" \
	"\t-x\tdo not export externs"

#define install_trivial_usage \
	"[-cgmops] [sources] <dest|directory>"
#define install_full_usage \
	"Copies files and set attributes\n\n" \
	"Options:\n" \
	"\t-c\tcopy the file, default\n" \
	"\t-d\tcreate directories\n" \
	"\t-g\tset group ownership\n" \
	"\t-m\tset permission modes\n" \
	"\t-o\tset ownership\n" \
	"\t-p\tpreserve date\n" \
	"\t-s\tstrip symbol tables"

#define ip_trivial_usage \
	"[ OPTIONS ] { address | link | route | tunnel } { COMMAND | help }"
#define ip_full_usage \
	"ip [ OPTIONS ] OBJECT { COMMAND | help }\n" \
	"where  OBJECT := { link | addr | route | tunnel }\n" \
	"OPTIONS := { -f[amily] { inet | inet6 | link } | -o[neline] }"

#define ipaddr_trivial_usage \
	"{ {add|del} IFADDR dev STRING | {show|flush}\n" \
	"\t\t[ dev STRING ] [ to PREFIX ] }"
#define ipaddr_full_usage \
	"ipaddr {add|del} IFADDR dev STRING\n" \
	"ipaddr {show|flush} [ dev STRING ] [ scope SCOPE-ID ]\n" \
	"\t\t\t[ to PREFIX ] [ label PATTERN ]\n" \
	"\t\t\tIFADDR := PREFIX | ADDR peer PREFIX\n" \
	"\t\t\t[ broadcast ADDR ] [ anycast ADDR ]\n" \
	"\t\t\t[ label STRING ] [ scope SCOPE-ID ]\n" \
	"\t\t\tSCOPE-ID := [ host | link | global | NUMBER ]"

#ifdef CONFIG_FEATURE_IPCALC_FANCY
#  define XUSAGE_IPCALC_FANCY(a) a
#else
#  define XUSAGE_IPCALC_FANCY(a)
#endif
#define ipcalc_trivial_usage \
	"[OPTION]... <ADDRESS>[[/]<NETMASK>] [NETMASK]"
#define ipcalc_full_usage \
	"Calculate IP network settings from a IP address\n\n" \
	"Options:\n" \
	"\t-b\t--broadcast\tDisplay calculated broadcast address\n" \
	"\t-n\t--network\tDisplay calculated network address\n" \
	"\t-m\t--netmask\tDisplay default netmask for IP" \
	XUSAGE_IPCALC_FANCY( \
	"\n\t-p\t--prefix\tDisplay the prefix for IP/NETMASK\n" \
	"\t-h\t--hostname\tDisplay first resolved host name\n" \
	"\t-s\t--silent\tDon't ever display error messages")

#define ipcrm_trivial_usage \
	"[-[MQS] key] [-[mqs] id]"
#define ipcrm_full_usage \
	"The upper-case options MQS are used to remove a shared memory\n" \
	"segment by an shmkey value. The lower-case options mqs are used\n" \
	"to remove a segment by shmid value.\n" \
	"\t-m | -M\tRemove the memory segment after the last detatch\n" \
	"\t-q | -Q\tRemove the message queue\n" \
	"\t-s | -S\tRemove the semaphore"

#define ipcs_trivial_usage \
	"[[-smq] -i shmid] | [[-asmq] [-tclup]]"
#define ipcs_full_usage \
	"\t-i\tspecify a specific resource id\n" \
	"Resource specification:\n" \
	"\t-m\tshared memory segments\n" \
	"\t-q\tmessage queues\n" \
	"\t-s\tsempahore arrays\n" \
	"\t-a\tall (default)\n" \
	"Output format:\n" \
	"\t-t\ttime\n" \
	"\t-p\tpid\n" \
	"\t-s\tcreator\n" \
	"\t-a\tlimits\n" \
	"\t-i\tsummary"

#define iplink_trivial_usage \
	"{ set DEVICE { up | down | arp { on | off } | show [ DEVICE ] }"
#define iplink_full_usage \
	"iplink set DEVICE { up | down | arp { on | off } |\n" \
	"\t\t\tdynamic { on | off } |\n" \
	"\t\t\tmtu MTU }\n" \
	"\tiplink show [ DEVICE ]"

#define iproute_trivial_usage \
	"{ list | flush | { add | del | change | append |\n" \
	"\t\treplace | monitor } ROUTE }"
#define iproute_full_usage \
	"iproute { list | flush } SELECTOR\n" \
	"iproute get ADDRESS [ from ADDRESS iif STRING ]\n" \
	"\t\t\t[ oif STRING ]  [ tos TOS ]\n" \
	"\tiproute { add | del | change | append | replace | monitor } ROUTE\n" \
	"\t\t\tSELECTOR := [ root PREFIX ] [ match PREFIX ] [ proto RTPROTO ]\n" \
	"\t\t\tROUTE := [ TYPE ] PREFIX [ tos TOS ] [ proto RTPROTO ]"

#define iptunnel_trivial_usage \
	"{ add | change | del | show } [ NAME ]\n" \
	"\t\t[ mode { ipip | gre | sit } ]\n" \
	"\t\t[ remote ADDR ] [ local ADDR ] [ ttl TTL ]"
#define iptunnel_full_usage \
	"iptunnel { add | change | del | show } [ NAME ]\n" \
	"\t\t\t[ mode { ipip | gre | sit } ] [ remote ADDR ] [ local ADDR ]\n" \
	"\t\t\t[ [i|o]seq ] [ [i|o]key KEY ] [ [i|o]csum ]\n" \
	"\t\t\t[ ttl TTL ] [ tos TOS ] [ [no]pmtudisc ] [ dev PHYS_DEV ]"

#define kill_trivial_usage \
	"[-signal] process-id [process-id ...]"
#define kill_full_usage \
	"Send a signal (default is SIGTERM) to the specified process(es).\n\n" \
	"Options:\n" \
	"\t-l\tList all signal names and numbers"
#define kill_example_usage \
	"$ ps | grep apache\n" \
	"252 root     root     S [apache]\n" \
	"263 www-data www-data S [apache]\n" \
	"264 www-data www-data S [apache]\n" \
	"265 www-data www-data S [apache]\n" \
	"266 www-data www-data S [apache]\n" \
	"267 www-data www-data S [apache]\n" \
	"$ kill 252\n"

#define killall_trivial_usage \
	"[-q] [-signal] process-name [process-name ...]"
#define killall_full_usage \
	"Send a signal (default is SIGTERM) to the specified process(es).\n\n" \
	"Options:\n" \
	"\t-l\tList all signal names and numbers\n" \
	"\t-q\tDo not complain if no processes were killed"
#define killall_example_usage \
	"$ killall apache\n"

#define klogd_trivial_usage \
	"[-c n] [-n]"
#define klogd_full_usage \
	"Kernel logger.\n" \
	"Options:\n" \
	"\t-c n\tSets the default log level of console messages to n\n" \
	"\t-n\tRun as a foreground process"

#define length_trivial_usage \
	"STRING"
#define length_full_usage \
	"Prints out the length of the specified STRING."
#define length_example_usage \
	"$ length Hello\n" \
	"5\n"

#define less_trivial_usage \
	"[-EMNmh~?] FILE1 FILE2..."
#define less_full_usage \
	"View a file or list of files. The position within files can be\n" \
	"changed, and files can be manipulated in various ways with the\n" \
	"following options:\n\n" \
	"\t-E\tQuit once the end of a file is reached\n" \
	"\t-M\tDisplay a status line containing the current line numbers\n" \
	"\t\tand the percentage through the file\n" \
	"\t-N\tPrefix line numbers to each line\n" \
	"\t-m\tDisplay a status line containing the percentage through the\n" \
	"\t\tfile\n" \
	"\t-~\tSuppress ~s displayed when input past the end of the file is\n" \
	"\t\treached.\n" \
	"\t-h, -?\tDisplay this help message"

#define setarch_trivial_usage \
	"<personality> <program> [args ...]"
#define setarch_full_usage \
	"Personality may be:\n" \
	"\tlinux32\tSet 32bit uname emulation\n" \
	"\tlinux64\tSet 64bit uname emulation"

#define ln_trivial_usage \
	"[OPTION] TARGET... LINK_NAME|DIRECTORY"
#define ln_full_usage \
	"Create a link named LINK_NAME or DIRECTORY to the specified TARGET\n" \
	"\nYou may use '--' to indicate that all following arguments are non-options.\n\n" \
	"Options:\n" \
	"\t-s\tmake symbolic links instead of hard links\n" \
	"\t-f\tremove existing destination files\n" \
	"\t-n\tno dereference symlinks - treat like normal file\n" \
	"\t-b\tmake a backup of the target (if exists) before link operation\n" \
	"\t-S suffix\tuse suffix instead of ~ when making backup files"
#define ln_example_usage \
	"$ ln -s BusyBox /tmp/ls\n" \
	"$ ls -l /tmp/ls\n" \
	"lrwxrwxrwx    1 root     root            7 Apr 12 18:39 ls -> BusyBox*\n"

#define loadfont_trivial_usage \
	"< font"
#define loadfont_full_usage \
	"Loads a console font from standard input."
#define loadfont_example_usage \
	"$ loadfont < /etc/i18n/fontname\n"

#define loadkmap_trivial_usage \
	"< keymap"
#define loadkmap_full_usage \
	"Loads a binary keyboard translation table from standard input."
#define loadkmap_example_usage \
	"$ loadkmap < /etc/i18n/lang-keymap\n"

#define logger_trivial_usage \
	"[OPTION]... [MESSAGE]"
#define logger_full_usage \
	"Write MESSAGE to the system log.  If MESSAGE is omitted, log stdin.\n\n" \
	"Options:\n" \
	"\t-s\tLog to stderr as well as the system log\n" \
	"\t-t TAG\tLog using the specified tag (defaults to user name)\n" \
	"\t-p PRIORITY\tEnter the message with the specified priority\n" \
	"\t\tThis may be numerical or a ``facility.level'' pair"
#define logger_example_usage \
	"$ logger \"hello\"\n"

#define login_trivial_usage \
	"[OPTION]... [username] [ENV=VAR ...]"
#define login_full_usage \
	"Begin a new session on the system\n\n" \
	"Options:\n" \
	"\t-f\tDo not authenticate (user already authenticated)\n" \
	"\t-h\tName of the remote host for this login\n" \
	"\t-p\tPreserve environment"

#define logname_trivial_usage \
	""
#define logname_full_usage \
	"Print the name of the current user."
#define logname_example_usage \
	"$ logname\n" \
	"root\n"

#define logread_trivial_usage \
	"[OPTION]..."
#define logread_full_usage \
	"Shows the messages from syslogd (using circular buffer).\n\n" \
	"Options:\n" \
	"\t-f\t\toutput data as the log grows"

#define losetup_trivial_usage \
	"[-od] LOOPDEVICE [FILE]"
#define losetup_full_usage \
	"Associate LOOPDEVICE with FILE, or display current association.\n\n" \
	"Options:\n" \
	"\t-d\t\tDisassociate LOOPDEVICE\n" \
	"\t-o OFFSET\tStart OFFSET bytes into FILE"
#define losetup_notes_usage \
	"One argument (losetup /dev/loop1) will display the current association\n" \
	"(if any), or disassociate it (with -d).  The display shows the offset\n" \
	"and filename of the file the loop device is currently bound to.\n\n" \
	"Two arguments (losetup /dev/loop1 file.img) create a new association,\n" \
	"with an optional offset (-o 12345).  Encryption is not yet supported.\n\n"

#ifdef CONFIG_FEATURE_LS_TIMESTAMPS
#  define USAGE_LS_TIMESTAMPS(a) a
#else
#  define USAGE_LS_TIMESTAMPS(a)
#endif
#ifdef CONFIG_FEATURE_LS_FILETYPES
#  define USAGE_LS_FILETYPES(a) a
#else
#  define USAGE_LS_FILETYPES(a)
#endif
#ifdef CONFIG_FEATURE_LS_FOLLOWLINKS
#  define USAGE_LS_FOLLOWLINKS(a) a
#else
#  define USAGE_LS_FOLLOWLINKS(a)
#endif
#ifdef CONFIG_FEATURE_LS_RECURSIVE
#  define USAGE_LS_RECURSIVE(a) a
#else
#  define USAGE_LS_RECURSIVE(a)
#endif
#ifdef CONFIG_FEATURE_LS_SORTFILES
#  define USAGE_LS_SORTFILES(a) a
#else
#  define USAGE_LS_SORTFILES(a)
#endif
#ifdef CONFIG_FEATURE_AUTOWIDTH
#  define USAGE_AUTOWIDTH(a) a
#else
#  define USAGE_AUTOWIDTH(a)
#endif
#ifdef CONFIG_FEATURE_LS_COLOR
  #define USAGE_LS_COLOR(a) a
#else
  #define USAGE_LS_COLOR(a)
#endif

#define ls_trivial_usage \
	"[-1Aa" USAGE_LS_TIMESTAMPS("c") "Cd" USAGE_LS_TIMESTAMPS("e") USAGE_LS_FILETYPES("F") "iln" USAGE_LS_FILETYPES("p") USAGE_LS_FOLLOWLINKS("L") USAGE_LS_RECURSIVE("R") USAGE_LS_SORTFILES("rS") "s" USAGE_AUTOWIDTH("T") USAGE_LS_TIMESTAMPS("tu") USAGE_LS_SORTFILES("v") USAGE_AUTOWIDTH("w") "x" USAGE_LS_SORTFILES("X") USAGE_HUMAN_READABLE("h") USAGE_NOT_HUMAN_READABLE("") "k" USAGE_SELINUX("K") "] [filenames...]"
#define ls_full_usage \
	"List directory contents\n\n" \
	"Options:\n" \
	"\t-1\tlist files in a single column\n" \
	"\t-A\tdo not list implied . and ..\n" \
	"\t-a\tdo not hide entries starting with .\n" \
	"\t-C\tlist entries by columns\n" \
	USAGE_LS_TIMESTAMPS("\t-c\twith -l: show ctime\n") \
	USAGE_LS_COLOR("\t--color[={always,never,auto}]\tto control coloring\n") \
	"\t-d\tlist directory entries instead of contents\n" \
	USAGE_LS_TIMESTAMPS("\t-e\tlist both full date and full time\n") \
	USAGE_LS_FILETYPES("\t-F\tappend indicator (one of */=@|) to entries\n") \
	"\t-i\tlist the i-node for each file\n" \
	"\t-l\tuse a long listing format\n" \
	"\t-n\tlist numeric UIDs and GIDs instead of names\n" \
	USAGE_LS_FILETYPES("\t-p\tappend indicator (one of /=@|) to entries\n") \
	USAGE_LS_FOLLOWLINKS("\t-L\tlist entries pointed to by symbolic links\n") \
	USAGE_LS_RECURSIVE("\t-R\tlist subdirectories recursively\n") \
	USAGE_LS_SORTFILES("\t-r\tsort the listing in reverse order\n") \
	USAGE_LS_SORTFILES("\t-S\tsort the listing by file size\n") \
	"\t-s\tlist the size of each file, in blocks\n" \
	USAGE_AUTOWIDTH("\t-T NUM\tassume Tabstop every NUM columns\n") \
	USAGE_LS_TIMESTAMPS("\t-t\twith -l: show modification time\n") \
	USAGE_LS_TIMESTAMPS("\t-u\twith -l: show access time\n") \
	USAGE_LS_SORTFILES("\t-v\tsort the listing by version\n") \
	USAGE_AUTOWIDTH("\t-w NUM\tassume the terminal is NUM columns wide\n") \
	"\t-x\tlist entries by lines instead of by columns\n" \
	USAGE_LS_SORTFILES("\t-X\tsort the listing by extension\n") \
	USAGE_HUMAN_READABLE( \
	"\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G )\n") \
	USAGE_SELINUX("\t-k\tprint security context\n") \
	USAGE_SELINUX("\t-K\tprint security context in long format\n")

#define lsattr_trivial_usage \
	"[-Radlv] [files...]"
#define lsattr_full_usage \
	"list file attributes on an ext2 fs\n\n" \
	"Options:\n" \
	"\t-R\trecursively list subdirectories\n" \
	"\t-a\tdo not hide entries starting with .\n" \
	"\t-d\tlist directory entries instead of contents\n" \
	"\t-l\tprint long flag names\n" \
	"\t-v\tlist the file's version/generation number"

#define lsmod_trivial_usage \
	""
#define lsmod_full_usage \
	"List the currently loaded kernel modules."

#ifdef CONFIG_FEATURE_MAKEDEVS_LEAF
#define makedevs_trivial_usage \
	"NAME TYPE MAJOR MINOR FIRST LAST [s]"
#define makedevs_full_usage \
	"Creates a range of block or character special files\n\n" \
	"TYPEs include:\n" \
	"\tb:\tMake a block (buffered) device.\n" \
	"\tc or u:\tMake a character (un-buffered) device.\n" \
	"\tp:\tMake a named pipe. MAJOR and MINOR are ignored for named pipes.\n\n" \
	"FIRST specifies the number appended to NAME to create the first device.\n" \
	"LAST specifies the number of the last item that should be created\n" \
	"If 's' is the last argument, the base device is created as well.\n\n" \
	"For example:\n" \
	"\tmakedevs /dev/ttyS c 4 66 2 63   ->  ttyS2-ttyS63\n" \
	"\tmakedevs /dev/hda b 3 0 0 8 s    ->  hda,hda1-hda8"
#define makedevs_example_usage \
	"# makedevs /dev/ttyS c 4 66 2 63\n" \
	"[creates ttyS2-ttyS63]\n" \
	"# makedevs /dev/hda b 3 0 0 8 s\n" \
	"[creates hda,hda1-hda8]\n"
#endif

#ifdef CONFIG_FEATURE_MAKEDEVS_TABLE
#define makedevs_trivial_usage \
	"[-d device_table] rootdir"
#define makedevs_full_usage \
	"Creates a range of special files as specified in a device table.\n" \
	"Device table entries take the form of:\n" \
	"<type> <mode> <uid> <gid> <major> <minor> <start> <inc> <count>\n" \
	"Where name is the file name,  type can be one of:\n" \
	"      f       A regular file\n" \
	"      d       Directory\n" \
	"      c       Character special device file\n" \
	"      b       Block special device file\n" \
	"      p       Fifo (named pipe)\n" \
	"uid is the user id for the target file, gid is the group id for the\n" \
	"target file.  The rest of the entries (major, minor, etc) apply to\n" \
	"to device special files.  A '-' may be used for blank entries."
#define makedevs_example_usage \
	"For example:\n" \
	"<name>    <type> <mode><uid><gid><major><minor><start><inc><count>\n" \
	"/dev         d   755    0    0    -      -      -      -    -\n" \
	"/dev/console c   666    0    0    5      1      -      -    -\n" \
	"/dev/null    c   666    0    0    1      3      0      0    -\n" \
	"/dev/zero    c   666    0    0    1      5      0      0    -\n" \
	"/dev/hda     b   640    0    0    3      0      0      0    -\n" \
	"/dev/hda     b   640    0    0    3      1      1      1    15\n\n" \
	"Will Produce:\n" \
	"/dev\n" \
	"/dev/console\n" \
	"/dev/null\n" \
	"/dev/zero\n" \
	"/dev/hda\n" \
	"/dev/hda[0-15]\n"
#endif

#ifdef CONFIG_FEATURE_MD5_SHA1_SUM_CHECK
#define USAGE_MD5_SHA1_SUM_CHECK(a) a
#else
#define USAGE_MD5_SHA1_SUM_CHECK(a)
#endif

#define md5sum_trivial_usage \
	"[OPTION] [FILEs...]" \
	USAGE_MD5_SHA1_SUM_CHECK("\n   or: md5sum [OPTION] -c [FILE]")
#define md5sum_full_usage \
	"Print" USAGE_MD5_SHA1_SUM_CHECK(" or check") " MD5 checksums.\n\n" \
	"Options:\n" \
	"With no FILE, or when FILE is -, read standard input." \
	USAGE_MD5_SHA1_SUM_CHECK("\n\n" \
	"\t-c\tcheck MD5 sums against given list\n" \
	"\nThe following two options are useful only when verifying checksums:\n" \
	"\t-s\tdon't output anything, status code shows success\n" \
	"\t-w\twarn about improperly formated MD5 checksum lines")
#define md5sum_example_usage \
	"$ md5sum < busybox\n" \
	"6fd11e98b98a58f64ff3398d7b324003\n" \
	"$ md5sum busybox\n" \
	"6fd11e98b98a58f64ff3398d7b324003  busybox\n" \
	"$ md5sum -c -\n" \
	"6fd11e98b98a58f64ff3398d7b324003  busybox\n" \
	"busybox: OK\n" \
	"^D\n"

#define mdev_trivial_usage \
	"[-s]"
#define mdev_full_usage \
	"\t-s\tScan /sys and populate /dev during system boot\n\n" \
	"Called with no options (via hotplug) it uses environment variables\n" \
	"to determine which device to add/remove."
#ifdef CONFIG_FEATURE_MDEV_CONFIG
#define mdev_notes_usage \
	"The mdev config file contains lines that look like:\n" \
	"  hd[a-z][0-9]* 0:3 660\n\n" \
	"That's device name (with regex match), uid:gid, and permissions.\n\n" \
	"Optionally, that can be followed (on the same line) by an asterisk\n" \
	"and a command line to run after creating the corresponding device(s),\n"\
	"ala:\n\n" \
	"  hdc root:cdrom 660  *ln -s hdc cdrom\n\n" \
	"Config file parsing stops on the first matching line.  If no config\n"\
	"entry is matched, devices are created with default 0:0 660.  (Make\n"\
	"the last line match .* to override this.)\n\n"
#endif

#define mesg_trivial_usage \
	"[y|n]"
#define mesg_full_usage \
	"mesg controls write access to your terminal\n" \
	"\ty\tAllow write access to your terminal\n" \
	"\tn\tDisallow write access to your terminal"

#define mkdir_trivial_usage \
	"[OPTION] DIRECTORY..."
#define mkdir_full_usage \
	"Create the DIRECTORY(ies) if they do not already exist\n\n" \
	"Options:\n" \
	"\t-m\tset permission mode (as in chmod), not rwxrwxrwx - umask\n" \
	"\t-p\tno error if existing, make parent directories as needed"
#define mkdir_example_usage \
	"$ mkdir /tmp/foo\n" \
	"$ mkdir /tmp/foo\n" \
	"/tmp/foo: File exists\n" \
	"$ mkdir /tmp/foo/bar/baz\n" \
	"/tmp/foo/bar/baz: No such file or directory\n" \
	"$ mkdir -p /tmp/foo/bar/baz\n"

#define mke2fs_trivial_usage \
	"[-c|-l filename] [-b block-size] [-f fragment-size] [-g blocks-per-group] " \
	"[-i bytes-per-inode] [-j] [-J journal-options] [-N number-of-inodes] [-n] " \
	"[-m reserved-blocks-percentage] [-o creator-os] [-O feature[,...]] [-q] " \
	"[r fs-revision-level] [-E extended-options] [-v] [-F] [-L volume-label] " \
	"[-M last-mounted-directory] [-S] [-T filesystem-type] " \
	"device [blocks-count]"
#define mke2fs_full_usage \
	"\t-b size\tblock size in bytes\n" \
	"\t-c\tcheck for bad blocks before creating\n" \
	"\t-E opts\tset extended options\n" \
	"\t-f size\tfragment size in bytes\n" \
	"\t-F\tforce (ignore sanity checks)\n" \
	"\t-g num\tnumber of blocks in a block group\n" \
	"\t-i ratio\tthe bytes/inode ratio\n" \
	"\t-j\tcreate a journal (ext3)\n" \
	"\t-J opts\tset journal options (size/device)\n" \
	"\t-l file\tread bad blocks list from file\n" \
	"\t-L lbl\tset the volume label\n" \
	"\t-m percent\tpercent of fs blocks to reserve for admin\n" \
	"\t-M dir\tset last mounted directory\n" \
	"\t-n\tdo not actually create anything\n" \
	"\t-N num\tnumber of inodes to create\n" \
	"\t-o os\tset the 'creator os' field\n" \
	"\t-O features\tdir_index/filetype/has_journal/journal_dev/sparse_super\n" \
	"\t-q\tquiet execution\n" \
	"\t-r rev\tset filesystem revision\n" \
	"\t-S\twrite superblock and group descriptors only\n" \
	"\t-T fs-type\tset usage type (news/largefile/largefile4)\n" \
	"\t-v\tverbose execution"

#define mkfifo_trivial_usage \
	"[OPTIONS] name"
#define mkfifo_full_usage \
	"Creates a named pipe (identical to 'mknod name p')\n\n" \
	"Options:\n" \
	"\t-m\tcreate the pipe using the specified mode (default a=rw)"

#define mkfs_minix_trivial_usage \
	"[-c | -l filename] [-nXX] [-iXX] /dev/name [blocks]"
#define mkfs_minix_full_usage \
	"Make a MINIX filesystem.\n\n" \
	"Options:\n" \
	"\t-c\t\tCheck the device for bad blocks\n" \
	"\t-n [14|30]\tSpecify the maximum length of filenames\n" \
	"\t-i INODES\tSpecify the number of inodes for the filesystem\n" \
	"\t-l FILENAME\tRead the bad blocks list from FILENAME\n" \
	"\t-v\t\tMake a Minix version 2 filesystem"

#define mknod_trivial_usage \
	"[OPTIONS] NAME TYPE MAJOR MINOR"
#define mknod_full_usage \
	"Create a special file (block, character, or pipe).\n\n" \
	"Options:\n" \
	"\t-m\tcreate the special file using the specified mode (default a=rw)\n\n" \
	"TYPEs include:\n" \
	"\tb:\tMake a block (buffered) device\n" \
	"\tc or u:\tMake a character (un-buffered) device\n" \
	"\tp:\tMake a named pipe. MAJOR and MINOR are ignored for named pipes"
#define mknod_example_usage \
	"$ mknod /dev/fd0 b 2 0\n" \
	"$ mknod -m 644 /tmp/pipe p\n"

#define mkswap_trivial_usage \
	"[-c] [-v0|-v1] device [block-count]"
#define mkswap_full_usage \
	"Prepare a disk partition to be used as a swap partition.\n\n" \
	"Options:\n" \
	"\t-c\t\tCheck for read-ability\n" \
	"\t-v0\t\tMake version 0 swap [max 128 Megs]\n" \
	"\t-v1\t\tMake version 1 swap [big!] (default for kernels >\n\t\t\t2.1.117)\n" \
	"\tblock-count\tNumber of block to use (default is entire partition)"

#define mktemp_trivial_usage \
	"[-dq] TEMPLATE"
#define mktemp_full_usage \
	"Creates a temporary file with its name based on TEMPLATE.\n" \
	"TEMPLATE is any name with six `Xs' (i.e., /tmp/temp.XXXXXX).\n\n" \
	"Options:\n" \
	"\t-d\t\tMake a directory instead of a file\n" \
	"\t-q\t\tFail silently if an error occurs"
#define mktemp_example_usage \
	"$ mktemp /tmp/temp.XXXXXX\n" \
	"/tmp/temp.mWiLjM\n" \
	"$ ls -la /tmp/temp.mWiLjM\n" \
	"-rw-------    1 andersen andersen        0 Apr 25 17:10 /tmp/temp.mWiLjM\n"

#define modprobe_trivial_usage \
	"[-knqrsv] MODULE [symbol=value ...]"
#define modprobe_full_usage \
	"Options:\n" \
	"\t-k\tMake module autoclean-able\n" \
	"\t-n\tJust show what would be done\n" \
	"\t-q\tQuiet output\n" \
	"\t-r\tRemove module (stacks) or do autoclean\n" \
	"\t-s\tReport via syslog instead of stderr\n" \
	"\t-v\tVerbose output"
#define modprobe_notes_usage \
"modprobe can (un)load a stack of modules, passing each module options (when\n" \
"loading). modprobe uses a configuration file to determine what option(s) to\n" \
"pass each module it loads.\n" \
"\n" \
"The configuration file is searched (in order) amongst:\n" \
"\n" \
"    /etc/modprobe.conf (2.6 only)\n" \
"    /etc/modules.conf\n" \
"    /etc/conf.modules (deprecated)\n" \
"\n" \
"They all have the same syntax (see below). If none is present, it is\n" \
"_not_ an error; each loaded module is then expected to load without\n" \
"options. Once a file is found, the others are tested for.\n" \
"\n" \
"/etc/modules.conf entry format:\n" \
"\n" \
"  alias <alias_name> <mod_name>\n" \
"    Makes it possible to modprobe alias_name, when there is no such module.\n" \
"    It makes sense if your mod_name is long, or you want a more reprenstative\n" \
"    name for that module (eg. 'scsi' in place of 'aha7xxx').\n" \
"    This makes it also possible to use a different set of options (below) for\n" \
"    the module and the alias.\n" \
"    A module can be aliased more than once.\n" \
"\n" \
"  options <mod_name|alias_name> <symbol=value ...>\n" \
"    When loading module mod_name (or the module aliased by alias_name), pass\n" \
"    the \"symbol=value\" pairs as option to that module.\n" \
"\n" \
"Sample /etc/modules.conf file:\n" \
"\n" \
"  options tulip irq=3\n" \
"  alias tulip tulip2\n" \
"  options tulip2 irq=4 io=0x308\n" \
"\n" \
"Other functionality offered by 'classic' modprobe is not available in\n" \
"this implementation.\n" \
"\n" \
"If module options are present both in the config file, and on the command line,\n" \
"then the options from the command line will be passed to the module _after_\n" \
"the options from the config file. That way, you can have defaults in the config\n" \
"file, and override them for a specific usage from the command line.\n"
#define modprobe_example_usage \
	"(with the above /etc/modules.conf):\n\n" \
	"$ modprobe tulip\n" \
	"   will load the module 'tulip' with default option 'irq=3'\n\n" \
	"$ modprobe tulip irq=5\n" \
	"   will load the module 'tulip' with option 'irq=5', thus overriding the default\n\n" \
	"$ modprobe tulip2\n" \
	"   will load the module 'tulip' with default options 'irq=4 io=0x308',\n" \
	"   which are the default for alias 'tulip2'\n\n" \
	"$ modprobe tulip2 irq=8\n" \
	"   will load the module 'tulip' with default options 'irq=4 io=0x308 irq=8',\n" \
	"   which are the default for alias 'tulip2' overriden by the option 'irq=8'\n\n" \
	"   from the command line\n\n" \
	"$ modprobe tulip2 irq=2 io=0x210\n" \
	"   will load the module 'tulip' with default options 'irq=4 io=0x308 irq=4 io=0x210',\n" \
	"   which are the default for alias 'tulip2' overriden by the options 'irq=2 io=0x210'\n\n" \
	"   from the command line\n"

#define more_trivial_usage \
	"[FILE ...]"
#define more_full_usage \
	"More is a filter for viewing FILE one screenful at a time."
#define more_example_usage \
	"$ dmesg | more\n"

#ifdef CONFIG_FEATURE_MOUNT_LOOP
#  define USAGE_MOUNT_LOOP(a) a
#else
#  define USAGE_MOUNT_LOOP(a)
#endif
#ifdef CONFIG_FEATURE_MTAB_SUPPORT
#  define USAGE_MTAB(a) a
#else
#  define USAGE_MTAB(a)
#endif
#define mount_trivial_usage \
	"[flags] DEVICE NODE [-o options,more-options]"
#define mount_full_usage \
	"Mount a filesystem.  Filesystem autodetection requires /proc be mounted.\n\n" \
	"Flags:\n"  \
	"\t-a:\t\tMount all filesystems in fstab\n" \
	USAGE_MTAB( \
	"\t-f:\t\t\"Fake\" Add entry to mount table but don't mount it\n" \
	"\t-n:\t\tDon't write a mount table entry\n" \
	) \
	"\t-o option:\tOne of many filesystem options, listed below\n" \
	"\t-r:\t\tMount the filesystem read-only\n" \
	"\t-t fs-type:\tSpecify the filesystem type\n" \
	"\t-w:\t\tMount for reading and writing (default)\n" \
	"\n" \
	"Options for use with the \"-o\" flag:\n" \
	"\tasync/sync:\tWrites are asynchronous / synchronous\n" \
	"\tatime/noatime:\tEnable / disable updates to inode access times\n" \
	"\tdev/nodev:\tAllow use of special device files / disallow them\n" \
	"\texec/noexec:\tAllow use of executable files / disallow them\n" \
	USAGE_MOUNT_LOOP( \
	"\tloop:\t\t Ignored (loop devices are autodetected)\n" \
	) \
	"\tsuid/nosuid:\tAllow set-user-id-root programs / disallow them\n" \
	"\tremount:\tRe-mount a mounted filesystem, changing its flags\n" \
	"\tro/rw:\t\tMount for read-only / read-write\n" \
	"\tbind:\t\tBind a directory to an additional location\n" \
	"\tmove:\t\tRelocate an existing mount point.\n" \
	"\nThere are EVEN MORE flags that are specific to each filesystem\n" \
	"You'll have to see the written documentation for those filesystems"
#define mount_example_usage \
	"$ mount\n" \
	"/dev/hda3 on / type minix (rw)\n" \
	"proc on /proc type proc (rw)\n" \
	"devpts on /dev/pts type devpts (rw)\n" \
	"$ mount /dev/fd0 /mnt -t msdos -o ro\n" \
	"$ mount /tmp/diskimage /opt -t ext2 -o loop\n" \
	"$ mount cd_image.iso mydir\n"

#define mountpoint_trivial_usage \
	"[-q] <[-d] DIR | -x DEVICE>"
#define mountpoint_full_usage \
	"mountpoint checks if the directory is a mountpoint\n\n" \
	"Options:\n"  \
	"\t-q:\t\tBe more quiet\n" \
	"\t-d:\t\tPrint major/minor device number of the filesystem\n" \
	"\t-x:\t\tPrint major/minor device number of the blockdevice"
#define mountpoint_example_usage \
	"$ mountpoint /proc\n" \
	"/proc is not a mountpoint\n" \
	"$ mountpoint /sys\n" \
	"/sys is a mountpoint\n"

#define mt_trivial_usage \
	"[-f device] opcode value"
#define mt_full_usage \
	"Control magnetic tape drive operation\n" \
	"\nAvailable Opcodes:\n\n" \
	"bsf bsfm bsr bss datacompression drvbuffer eof eom erase\n" \
	"fsf fsfm fsr fss load lock mkpart nop offline ras1 ras2\n" \
	"ras3 reset retension rewind rewoffline seek setblk setdensity\n" \
	"setpart tell unload unlock weof wset"

#define mv_trivial_usage \
	"[OPTION]... SOURCE DEST\n" \
	"or: mv [OPTION]... SOURCE... DIRECTORY"
#define mv_full_usage \
	"Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n\n" \
	"Options:\n" \
	"\t-f\tdon't prompt before overwriting\n" \
	"\t-i\tinteractive, prompt before overwrite"
#define mv_example_usage \
	"$ mv /tmp/foo /bin/bar\n"

#define nameif_trivial_usage \
	"[-s] [-c FILE] [{IFNAME MACADDR}]"
#define nameif_full_usage \
	"Nameif renaming network interface while it in the down state.\n\n" \
	"Options:\n" \
	"\t-c FILE\t\tUse configuration file (default is /etc/mactab)\n" \
	"\t-s\t\tUse syslog (LOCAL0 facility)\n" \
	"\tIFNAME MACADDR\tnew_interface_name interface_mac_address"
#define nameif_example_usage \
	"$ nameif -s dmz0 00:A0:C9:8C:F6:3F\n" \
	" or\n" \
	"$ nameif -c /etc/my_mactab_file\n" \

#ifdef CONFIG_NC_GAPING_SECURITY_HOLE
#  define USAGE_NC_EXEC(a) a
#else
#  define USAGE_NC_EXEC(a)
#endif
#define nc_trivial_usage \
	"[OPTIONS] [IP] [port]"
#define nc_full_usage \
	"Netcat opens a pipe to IP:port\n\n" \
	"Options:\n" \
	"\t-l\t\tlisten mode, for inbound connects\n" \
	"\t-p PORT\t\tlocal port number\n" \
	"\t-i SECS\t\tdelay interval for lines sent\n" \
	USAGE_NC_EXEC( \
	"\t-e PROG\t\tprogram to exec after connect (dangerous!)\n" \
	) \
	"\t-w SECS\t\ttimeout for connects and final net reads"
#define nc_example_usage \
	"$ nc foobar.somedomain.com 25\n" \
	"220 foobar ESMTP Exim 3.12 #1 Sat, 15 Apr 2000 00:03:02 -0600\n" \
	"help\n" \
	"214-Commands supported:\n" \
	"214-    HELO EHLO MAIL RCPT DATA AUTH\n" \
	"214     NOOP QUIT RSET HELP\n" \
	"quit\n" \
	"221 foobar closing connection\n"

#define netstat_trivial_usage \
	"[-laenrtuwx]"
#define netstat_full_usage \
	"Netstat displays Linux networking information.\n\n" \
	"Options:\n" \
	"\t-l display listening server sockets\n" \
	"\t-a display all sockets (default: connected)\n" \
	"\t-e display other/more information\n" \
	"\t-n don't resolve names\n" \
	"\t-r display routing table\n" \
	"\t-t tcp sockets\n" \
	"\t-u udp sockets\n" \
	"\t-w raw sockets\n" \
	"\t-x unix sockets"

#define nice_trivial_usage \
	"[-n ADJUST] [COMMAND [ARG] ...]"
#define nice_full_usage \
	"Nice runs a program with modified scheduling priority.\n\n" \
	"Options:\n" \
	"\t-n ADJUST\tAdjust the scheduling priority by ADJUST"

#define nohup_trivial_usage \
	"COMMAND [ARGS]"
#define nohup_full_usage \
	"run a command immune to hangups, with output to a non-tty"
#define nohup_example_usage \
	"$ nohup make &"

#define nslookup_trivial_usage \
	"[HOST] [SERVER]"
#define nslookup_full_usage \
	"Queries the nameserver for the IP address of the given HOST\n" \
	"optionally using a specified DNS server"
#define nslookup_example_usage \
	"$ nslookup localhost\n" \
	"Server:     default\n" \
	"Address:    default\n" \
	"\n" \
	"Name:       debian\n" \
	"Address:    127.0.0.1\n"

#define od_trivial_usage \
	"[-aBbcDdeFfHhIiLlOovXx] [FILE]"
#define od_full_usage \
	"Write an unambiguous representation, octal bytes by default, of FILE\n" \
	"to standard output.  With no FILE, or when FILE is -, read standard input."

#define openvt_trivial_usage \
	"<vtnum> <COMMAND> [ARGS...]"
#define openvt_full_usage \
	"Start a command on a new virtual terminal"
#define openvt_example_usage \
	"openvt 2 /bin/ash\n"

#ifdef CONFIG_FEATURE_SHA1_PASSWORDS
#  define PASSWORD_ALG_TYPES(a) a
#else
#  define PASSWORD_ALG_TYPES(a)
#endif
#define passwd_trivial_usage \
	"[OPTION] [name]"
#define passwd_full_usage \
	"Change a user password. If no name is specified,\n" \
	"changes the password for the current user.\n" \
	"Options:\n" \
	"\t-a\tDefine which algorithm shall be used for the password\n" \
	"\t\t\t(Choices: des, md5" \
	PASSWORD_ALG_TYPES(", sha1") \
	")\n\t-d\tDelete the password for the specified user account\n" \
	"\t-l\tLocks (disables) the specified user account\n" \
	"\t-u\tUnlocks (re-enables) the specified user account"

#define patch_trivial_usage \
	"[-p<num>] [-i<patch.diff>]"
#define patch_full_usage \
	"[-p<num>]\n" \
	"[-i<diff>]"
#define patch_example_usage \
	"$ patch -p1 <example.diff\n" \
	"$ patch -p0 -i example.diff"

#if ENABLE_FEATURE_PIDOF_SINGLE
#define USAGE_FEATURE_PIDOF_SINGLE(a) a
#else
#define USAGE_FEATURE_PIDOF_SINGLE(a)
#endif
#if ENABLE_FEATURE_PIDOF_OMIT
#define USAGE_FEATURE_PIDOF_OMIT(a) a
#else
#define USAGE_FEATURE_PIDOF_OMIT(a)
#endif
#if (ENABLE_FEATURE_PIDOF_SINGLE || ENABLE_FEATURE_PIDOF_OMIT)
#define USAGE_PIDOF "Options:"
#else
#define USAGE_PIDOF "\n\tThis version of pidof accepts no options."
#endif

#define pidof_trivial_usage \
	"process-name [OPTION] [process-name ...]"

#define pidof_full_usage \
	"Lists the PIDs of all processes with names that match the\n" \
	"names on the command line.\n" \
	USAGE_PIDOF \
	USAGE_FEATURE_PIDOF_SINGLE("\n\t-s\t\tdisplay only a single PID") \
	USAGE_FEATURE_PIDOF_OMIT("\n\t-o\t\tomit given pid.") \
	USAGE_FEATURE_PIDOF_OMIT("\n\t\t\tUse %PPID to omit the parent pid of pidof itself")
#define pidof_example_usage \
	"$ pidof init\n" \
	"1\n" \
	USAGE_FEATURE_PIDOF_OMIT("$ pidof /bin/sh\n20351 5973 5950\n") \
	USAGE_FEATURE_PIDOF_OMIT("$ pidof /bin/sh -o %PPID\n20351 5950")

#ifndef CONFIG_FEATURE_FANCY_PING
#define ping_trivial_usage "host"
#define ping_full_usage    "Send ICMP ECHO_REQUEST packets to network hosts"
#else
#define ping_trivial_usage \
	"[OPTION]... host"
#define ping_full_usage \
	"Send ICMP ECHO_REQUEST packets to network hosts.\n\n" \
	"Options:\n" \
	"\t-c COUNT\tSend only COUNT pings\n" \
	"\t-s SIZE\t\tSend SIZE data bytes in packets (default=56)\n" \
	"\t-q\t\tQuiet mode, only displays output at start\n" \
	"\t\t\tand when finished"
#endif
#define ping_example_usage \
	"$ ping localhost\n" \
	"PING slag (127.0.0.1): 56 data bytes\n" \
	"64 bytes from 127.0.0.1: icmp_seq=0 ttl=255 time=20.1 ms\n" \
	"\n" \
	"--- debian ping statistics ---\n" \
	"1 packets transmitted, 1 packets received, 0% packet loss\n" \
	"round-trip min/avg/max = 20.1/20.1/20.1 ms\n"

#ifndef CONFIG_FEATURE_FANCY_PING6
#define ping6_trivial_usage "host"
#define ping6_full_usage    "Send ICMP ECHO_REQUEST packets to network hosts"
#else
#define ping6_trivial_usage \
	"[OPTION]... host"
#define ping6_full_usage \
	"Send ICMP ECHO_REQUEST packets to network hosts.\n\n" \
	"Options:\n" \
	"\t-c COUNT\tSend only COUNT pings\n" \
	"\t-s SIZE\t\tSend SIZE data bytes in packets (default=56)\n" \
	"\t-q\t\tQuiet mode, only displays output at start\n" \
	"\t\t\tand when finished"
#endif
#define ping6_example_usage \
	"$ ping6 ip6-localhost\n" \
	"PING ip6-localhost (::1): 56 data bytes\n" \
	"64 bytes from ::1: icmp6_seq=0 ttl=64 time=20.1 ms\n" \
	"\n" \
	"--- ip6-localhost ping statistics ---\n" \
	"1 packets transmitted, 1 packets received, 0% packet loss\n" \
	"round-trip min/avg/max = 20.1/20.1/20.1 ms\n"

#define pivot_root_trivial_usage \
	"NEW_ROOT PUT_OLD"
#define pivot_root_full_usage \
	"Move the current root file system to PUT_OLD and make NEW_ROOT\n" \
	"the new root file system."

#define poweroff_trivial_usage \
	"[-d<delay>] [-n<nosync>] [-f<force>]"
#define poweroff_full_usage \
	"Halt and shut off power.\n" \
	"Options:\n" \
	"\t-d\t\tdelay interval for halting\n" \
	"\t-n\t\tno call to sync()\n" \
	"\t-f\t\tforce power off (don't go through init)"

#define printenv_trivial_usage \
	"[VARIABLES...]"
#define printenv_full_usage \
	"print all or part of environment\n\n" \
	"If no environment VARIABLE specified, print them all."

#define printf_trivial_usage \
	"FORMAT [ARGUMENT...]"
#define printf_full_usage \
	"Formats and prints ARGUMENT(s) according to FORMAT,\n" \
	"Where FORMAT controls the output exactly as in C printf."
#define printf_example_usage \
	"$ printf \"Val=%d\\n\" 5\n" \
	"Val=5\n"

#if !defined CONFIG_SELINUX && !ENABLE_FEATURE_PS_WIDE
#define USAGE_PS "\n\tThis version of ps accepts no options."
#else
#define USAGE_PS "\nOptions:"
#endif
#if ENABLE_FEATURE_PS_WIDE
#define USAGE_PS_WIDE(a) a
#else
#define USAGE_PS_WIDE(a)
#endif

#define ps_trivial_usage \
	""
#define ps_full_usage \
	"Report process status\n" \
	USAGE_PS \
	USAGE_SELINUX("\n\t-c\tshow SE Linux context") \
	USAGE_PS_WIDE("\n\tw\twide output")

#define ps_example_usage \
	"$ ps\n" \
	"  PID  Uid      Gid State Command\n" \
	"    1 root     root     S init\n" \
	"    2 root     root     S [kflushd]\n" \
	"    3 root     root     S [kupdate]\n" \
	"    4 root     root     S [kpiod]\n" \
	"    5 root     root     S [kswapd]\n" \
	"  742 andersen andersen S [bash]\n" \
	"  743 andersen andersen S -bash\n" \
	"  745 root     root     S [getty]\n" \
	" 2990 andersen andersen R ps\n"

#define pwd_trivial_usage \
	""
#define pwd_full_usage \
	"Print the full filename of the current working directory."
#define pwd_example_usage \
	"$ pwd\n" \
	"/root\n"

#define rdate_trivial_usage \
	"[-sp] HOST"
#define rdate_full_usage \
	"Get and possibly set the system date and time from a remote HOST.\n\n" \
	"Options:\n" \
	"\t-s\tSet the system date and time (default)\n" \
	"\t-p\tPrint the date and time"

#ifdef CONFIG_FEATURE_READLINK_FOLLOW
#define USAGE_READLINK_FOLLOW(a) a
#else
#define USAGE_READLINK_FOLLOW(a)
#endif

#define readlink_trivial_usage \
	USAGE_READLINK_FOLLOW("[-f] ") "FILE"
#define readlink_full_usage \
	"Displays the value of a symbolic link." \
	USAGE_READLINK_FOLLOW("\n\nOptions:\n" \
	"\t-f\tcanonicalize by following all symlinks")

#define readprofile_trivial_usage \
	"[OPTIONS]..."
#define readprofile_full_usage \
	"Options:\n" \
	"\t -m <mapfile>  (default: /boot/System.map)\n" \
	"\t -p <profile>  (default: /proc/profile)\n" \
	"\t -M <mult>     set the profiling multiplier to <mult>\n" \
	"\t -i            print only info about the sampling step\n" \
	"\t -v            print verbose data\n" \
	"\t -a            print all symbols, even if count is 0\n" \
	"\t -b            print individual histogram-bin counts\n" \
	"\t -s            print individual counters within functions\n" \
	"\t -r            reset all the counters (root only)\n" \
	"\t -n            disable byte order auto-detection"

#define realpath_trivial_usage \
	"pathname  ..."
#define realpath_full_usage \
	"Returns the absolute pathnames of given argument."

#define reboot_trivial_usage \
	"[-d<delay>] [-n<nosync>] [-f<force>]"
#define reboot_full_usage \
	"Reboot the system.\n" \
	"Options:\n" \
	"\t-d\t\tdelay interval for rebooting\n" \
	"\t-n\t\tno call to sync()\n" \
	"\t-f\t\tforce reboot (don't go through init)"

#define renice_trivial_usage \
	"{{-n INCREMENT} | PRIORITY} [[ -p | -g | -u ] ID ...]"
#define renice_full_usage \
	"Changes priority of running processes.\n\n" \
	"Options:\n" \
	"\t-n\tadjusts current nice value (smaller is faster)\n" \
	"\t-p\tprocess id(s) (default)\n" \
	"\t-g\tprocess group id(s)\n" \
	"\t-u\tprocess user name(s) and/or id(s)"

#define reset_trivial_usage \
	""
#define reset_full_usage \
	"Resets the screen."

#define rm_trivial_usage \
	"[OPTION]... FILE..."
#define rm_full_usage \
	"Remove (unlink) the FILE(s).  You may use '--' to\n" \
	"indicate that all following arguments are non-options.\n\n" \
	"Options:\n" \
	"\t-i\t\talways prompt before removing each destination\n" \
	"\t-f\t\tremove existing destinations, never prompt\n" \
	"\t-r or -R\tremove the contents of directories recursively"
#define rm_example_usage \
	"$ rm -rf /tmp/foo\n"

#define rmdir_trivial_usage \
	"[OPTION]... DIRECTORY..."
#define rmdir_full_usage \
	"Remove the DIRECTORY(ies), if they are empty."
#define rmdir_example_usage \
	"# rmdir /tmp/foo\n"

#define rmmod_trivial_usage \
	"[OPTION]... [MODULE]..."
#define rmmod_full_usage \
	"Unloads the specified kernel modules from the kernel.\n\n" \
	"Options:\n" \
	"\t-a\tRemove all unused modules (recursively)"
#define rmmod_example_usage \
	"$ rmmod tulip\n"

#ifdef CONFIG_FEATURE_IPV6
#  define USAGE_ROUTE_IPV6(a) a
#else
#  define USAGE_ROUTE_IPV6(a) "\t"
#endif


#define route_trivial_usage \
	"[{add|del|delete}]"
#define route_full_usage \
	"Edit the kernel's routing tables.\n\n" \
	"Options:\n" \
	"\t-n\t\tDont resolve names\n" \
	"\t-e\t\tDisplay other/more information\n" \
	"\t-A inet" USAGE_ROUTE_IPV6("{6}") "\tSelect address family"

#define rpm_trivial_usage \
	"-i -q[ildc]p package.rpm"
#define rpm_full_usage \
	"Manipulates RPM packages" \
	"\n\nOptions:" \
	"\n\t-i Install package" \
	"\n\t-q Query package" \
	"\n\t-p Query uninstalled package" \
	"\n\t-i Show information" \
	"\n\t-l List contents" \
	"\n\t-d List documents" \
	"\n\t-c List config files"

#define rpm2cpio_trivial_usage \
	"package.rpm"
#define rpm2cpio_full_usage \
	"Outputs a cpio archive of the rpm file."

#define run_parts_trivial_usage \
	"[-t] [-a ARG] [-u MASK] DIRECTORY"
#define run_parts_full_usage \
	"Run a bunch of scripts in a directory.\n\n" \
	"Options:\n" \
	"\t-t\tPrints what would be run, but does not actually run anything\n" \
	"\t-a ARG\tPass ARG as an argument for every program invoked\n" \
	"\t-u MASK\tSet the umask to MASK before executing every program"

#define runlevel_trivial_usage \
	"[utmp]"
#define runlevel_full_usage \
	"Find the current and previous system runlevel.\n\n" \
	"If no utmp file exists or if no runlevel record can be found,\n" \
	"runlevel prints \"unknown\""
#define runlevel_example_usage \
	"$ runlevel /var/run/utmp\n" \
	"N 2"

#define rx_trivial_usage \
	"FILE"
#define rx_full_usage \
	"Receive a file using the xmodem protocol."
#define rx_example_usage \
	"$ rx /tmp/foo\n"

#define sed_trivial_usage \
	"[-efinr] pattern [files...]"
#define sed_full_usage \
	"Options:\n" \
	"\t-e script\tadd the script to the commands to be executed\n" \
	"\t-f scriptfile\tadd script-file contents to the\n" \
	"\t\t\tcommands to be executed\n" \
	"\t-i\t\tedit files in-place\n" \
	"\t-n\t\tsuppress automatic printing of pattern space\n" \
	"\t-r\t\tuse extended regular expression syntax\n" \
	"\n" \
	"If no -e or -f is given, the first non-option argument is taken as the sed\n" \
	"script to interpret. All remaining arguments are names of input files; if no\n" \
	"input files are specified, then the standard input is read.  Source files\n" \
	"will not be modified unless -i option is given."

#define sed_example_usage \
	"$ echo \"foo\" | sed -e 's/f[a-zA-Z]o/bar/g'\n" \
	"bar\n"

#define seq_trivial_usage \
	"[first [increment]] last"
#define seq_full_usage \
	"Print numbers from FIRST to LAST, in steps of INCREMENT.\n" \
	"FIRST, INCREMENT default to 1\n" \
	"Arguments:\n" \
	"\tLAST\n" \
	"\tFIRST\tLAST\n" \
	"\tFIRST\tINCREMENT\tLAST"

#define setconsole_trivial_usage \
	"[-r|--reset] [DEVICE]"
#define setconsole_full_usage \
	"Redirects system console output to DEVICE (default: /dev/tty).\n\n" \
	"Options:\n" \
	"\t-r\tReset output to /dev/console."

#define setkeycodes_trivial_usage \
	"SCANCODE KEYCODE ..."
#define setkeycodes_full_usage \
	"Set entries into the kernel's scancode-to-keycode map,\n" \
	"allowing unusual keyboards to generate usable keycodes.\n\n" \
	"SCANCODE may be either xx or e0xx (hexadecimal),\n" \
	"and KEYCODE is given in decimal"
#define setkeycodes_example_usage \
	"$ setkeycodes e030 127\n"

#define setsid_trivial_usage \
	"program [arg ...]"
#define setsid_full_usage \
	"Runs any program in a new session by calling setsid() before\n" \
	"exec'ing the rest of its arguments.  See setsid(2) for details."

#define lash_trivial_usage \
	"[FILE]...\n" \
	"or: sh -c command [args]..."
#define lash_full_usage \
	"The BusyBox LAme SHell (command interpreter)"
#define lash_notes_usage \
	"This command does not yet have proper documentation.\n\n" \
	"Use lash just as you would use any other shell.  It properly handles pipes,\n" \
	"redirects, job control, can be used as the shell for scripts, and has a\n" \
	"sufficient set of builtins to do what is needed.  It does not (yet) support\n" \
	"Bourne Shell syntax.  If you need things like \"if-then-else\", \"while\", and such\n" \
	"use ash or bash.  If you just need a very simple and extremely small shell,\n" \
	"this will do the job."

#define last_trivial_usage \
	""
#define last_full_usage \
	"Shows listing of the last users that logged into the system"

#define sha1sum_trivial_usage \
	"[OPTION] [FILEs...]" \
	USAGE_MD5_SHA1_SUM_CHECK("\n   or: sha1sum [OPTION] -c [FILE]")
#define sha1sum_full_usage \
	"Print" USAGE_MD5_SHA1_SUM_CHECK(" or check") " SHA1 checksums.\n\n" \
	"Options:\n" \
	"With no FILE, or when FILE is -, read standard input." \
	USAGE_MD5_SHA1_SUM_CHECK("\n\n" \
	"\t-c\tcheck SHA1 sums against given list\n" \
	"\nThe following two options are useful only when verifying checksums:\n" \
	"\t-s\tdon't output anything, status code shows success\n" \
	"\t-w\twarn about improperly formated SHA1 checksum lines")

#ifdef CONFIG_FEATURE_FANCY_SLEEP
#  define USAGE_FANCY_SLEEP(a) a
#  define USAGE_NOT_FANCY_SLEEP(a)
#else
#  define USAGE_FANCY_SLEEP(a)
#  define USAGE_NOT_FANCY_SLEEP(a) a
#endif

#define sleep_trivial_usage \
	USAGE_FANCY_SLEEP("[") "N" USAGE_FANCY_SLEEP("]...")
#define sleep_full_usage \
	USAGE_NOT_FANCY_SLEEP("Pause for N seconds.") \
	USAGE_FANCY_SLEEP( \
	"Pause for a time equal to the total of the args given, where each arg can\n" \
	"\t\thave an optional suffix of (s)econds, (m)inutes, (h)ours, or (d)ays.")
#define sleep_example_usage \
	"$ sleep 2\n" \
	"[2 second delay results]\n" \
	USAGE_FANCY_SLEEP("$ sleep 1d 3h 22m 8s\n" \
	"[98528 second delay results]\n")

#if ENABLE_FEATURE_SORT_BIG
#  define USAGE_SORT_BIG(a) a
#else
#  define USAGE_SORT_BIG(a)
#endif

#define sort_trivial_usage \
	"[-nru" USAGE_SORT_BIG("gMcszbdfimSTokt] [-o outfile] [-k start[.offset][opts][,end[.offset][opts]] [-t char") "] [FILE]..."
#define sort_full_usage \
	"Sorts lines of text in the specified files\n\n" \
	"Options:\n" \
	USAGE_SORT_BIG( \
		"\t-b\tignore leading blanks\n" \
		"\t-c\tcheck whether input is sorted\n" \
		"\t-d\tdictionary order (blank or alphanumeric only)\n" \
		"\t-f\tignore case\n" \
		"\t-g\tgeneral numerical sort\n" \
		"\t-i\tignore unprintable characters\n" \
		"\t-k\tspecify sort key\n" \
		"\t-M\tsort month\n" \
	) \
	"\t-n\tsort numbers\n" \
	USAGE_SORT_BIG( \
		"\t-o\toutput to file\n" \
		"\t-k\tsort by key\n" \
		"\t-t\tuse key separator other than whitespace\n" \
	) \
	"\t-r\treverse sort order\n" \
	USAGE_SORT_BIG("\t-s\tstable (don't sort ties alphabetically)\n") \
	"\t-u\tsuppress duplicate lines" \
	USAGE_SORT_BIG("\n\t-z\tinput terminated by nulls, not newlines\n") \
	USAGE_SORT_BIG("\t-mST\tignored for GNU compatability") \
	""
#define sort_example_usage \
	"$ echo -e \"e\\nf\\nb\\nd\\nc\\na\" | sort\n" \
	"a\n" \
	"b\n" \
	"c\n" \
	"d\n" \
	"e\n" \
	"f\n" \
	USAGE_SORT_BIG( \
		"$ echo -e \"c 3\\nb 2\\nd 2\" | $SORT -k 2,2n -k 1,1r\n" \
		"d 2\n" \
		"b 2\n" \
		"c 3\n" \
	) \
	""

#define start_stop_daemon_trivial_usage \
	"[OPTIONS] [--start|--stop] ... [-- arguments...]"
#define start_stop_daemon_full_usage \
	"Program to start and stop services." \
	"\n\nOptions:" \
	"\n\t-S|--start\t\t\tstart" \
	"\n\t-K|--stop\t\t\tstop" \
	"\n\t-a|--startas <pathname>\t\tstarts process specified by pathname" \
	"\n\t-b|--background\t\t\tforce process into background" \
	"\n\t-u|--user <username>|<uid>\tstop this user's processes" \
	"\n\t-x|--exec <executable>\t\tprogram to either start or check" \
	"\n\t-m|--make-pidfile <filename>\tcreate the -p file and enter pid in it" \
	"\n\t-n|--name <process-name>\tstop processes with this name" \
	"\n\t-p|--pidfile <pid-file>\t\tsave or load pid using a pid-file" \
	"\n\t-q|--quiet\t\t\tbe quiet" \
	"\n\t-s|--signal <signal>\t\tsignal to send (default TERM)"

#ifdef CONFIG_FEATURE_STAT_FORMAT
#  define USAGE_STAT_FORMAT(a) a
#else
#  define USAGE_STAT_FORMAT(a)
#endif
#define stat_trivial_usage \
	"[OPTION] FILE..."
#define stat_full_usage \
	"display file (default) or filesystem status.\n\n" \
	"Options:\n" \
	USAGE_STAT_FORMAT("\t-c fmt\tuse the specified format\n") \
	"\t-f\tdisplay filesystem status\n" \
	"\t-L,-l\tdereference links\n" \
	"\t-t\tdisplay info in terse form\n" \
	USAGE_STAT_FORMAT( \
	"\nValid format sequences for files:\n" \
	"  %a   Access rights in octal\n" \
	"  %A   Access rights in human readable form\n" \
	"  %b   Number of blocks allocated (see %B)\n" \
	"  %B   The size in bytes of each block reported by %b\n" \
	"  %d   Device number in decimal\n" \
	"  %D   Device number in hex\n" \
	"  %f   Raw mode in hex\n" \
	"  %F   File type\n" \
	"  %g   Group ID of owner\n" \
	"  %G   Group name of owner\n" \
	"  %h   Number of hard links\n" \
	"  %i   Inode number\n" \
	"  %n   File name\n" \
	"  %N   Quoted file name with dereference if symbolic link\n" \
	"  %o   I/O block size\n" \
	"  %s   Total size, in bytes\n" \
	"  %t   Major device type in hex\n" \
	"  %T   Minor device type in hex\n" \
	"  %u   User ID of owner\n" \
	"  %U   User name of owner\n" \
	"  %x   Time of last access\n" \
	"  %X   Time of last access as seconds since Epoch\n" \
	"  %y   Time of last modification\n" \
	"  %Y   Time of last modification as seconds since Epoch\n" \
	"  %z   Time of last change\n" \
	"  %Z   Time of last change as seconds since Epoch\n" \
	"\nValid format sequences for file systems:\n" \
	"  %a   Free blocks available to non-superuser\n" \
	"  %b   Total data blocks in file system\n" \
	"  %c   Total file nodes in file system\n" \
	"  %d   Free file nodes in file system\n" \
	"  %f   Free blocks in file system\n" \
	"  %i   File System ID in hex\n" \
	"  %l   Maximum length of filenames\n" \
	"  %n   File name\n" \
	"  %s   Block size (for faster transfers)\n" \
	"  %S   Fundamental block size (for block counts)\n" \
	"  %t   Type in hex\n" \
	"  %T   Type in human readable form\n" \
	)

#define strings_trivial_usage \
	"[-afo] [-n length] [file ... ]"
#define strings_full_usage \
	"Display printable strings in a binary file." \
	"\n\nOptions:" \
	"\n\t-a\tScan the whole files (this is the default)." \
	"\n\t-f\tPrecede each string with the name of the file where it was found." \
	"\n\t-n N\tSpecifies that at least N characters forms a sequence (default 4)" \
	"\n\t-o\tEach string is preceded by its decimal offset in the file"

#define stty_trivial_usage \
	"[-a|g] [-F DEVICE] [SETTING]..."
#define stty_full_usage \
	"Without arguments, prints baud rate, line discipline," \
	"\nand deviations from stty sane." \
	"\n\nOptions:" \
	"\n\t-F DEVICE\topen device instead of stdin" \
	"\n\t-a\t\tprint all current settings in human-readable form" \
	"\n\t-g\t\tprint in stty-readable form" \
	"\n\t[SETTING]\tsee manpage"

#define su_trivial_usage \
	"[OPTION]... [-] [username]"
#define su_full_usage \
	"Change user id or become root.\n" \
	"Options:\n" \
	"\t-p, -m\tPreserve environment" \
	"\n\t-c\tCommand to pass to 'sh -c'" \
	"\n\t-s\tShell to use instead of default shell"

#define sulogin_trivial_usage \
	"[OPTION]... [tty-device]"
#define sulogin_full_usage \
	"Single user login\n" \
	"Options:\n" \
	"\t-f\tDo not authenticate (user already authenticated)\n" \
	"\t-h\tName of the remote host for this login\n" \
	"\t-p\tPreserve environment"

#define sum_trivial_usage \
	"[rs] [files...]"
#define sum_full_usage \
	"checksum and count the blocks in a file\n\n" \
	"Options:\n" \
	"\t-r\tuse BSD sum algorithm (1K blocks)\n" \
	"\t-s\tuse System V sum algorithm (512byte blocks)"

#define swapoff_trivial_usage \
	"[-a] [DEVICE]"
#define swapoff_full_usage \
	"Stop swapping virtual memory pages on DEVICE.\n\n" \
	"Options:\n" \
	"\t-a\tStop swapping on all swap devices"

#define swapon_trivial_usage \
	"[-a] [DEVICE]"
#define swapon_full_usage \
	"Start swapping virtual memory pages on DEVICE.\n\n" \
	"Options:\n" \
	"\t-a\tStart swapping on all swap devices"

#define switch_root_trivial_usage \
	"[-c /dev/console] NEW_ROOT NEW_INIT [ARGUMENTS_TO_INIT]"
#define switch_root_full_usage \
	"Use from PID 1 under initramfs to free initramfs, chroot to NEW_ROOT,\n" \
	"and exec NEW_INIT.\n\n" \
	"Options:\n" \
	"\t-c\tRedirect console to device on new root"

#define sync_trivial_usage \
	""
#define sync_full_usage \
	"Write all buffered filesystem blocks to disk."


#ifdef CONFIG_FEATURE_ROTATE_LOGFILE
#  define USAGE_ROTATE_LOGFILE(a) a
#else
#  define USAGE_ROTATE_LOGFILE(a)
#endif
#ifdef CONFIG_FEATURE_REMOTE_LOG
#  define USAGE_REMOTE_LOG(a) a
#else
#  define USAGE_REMOTE_LOG(a)
#endif
#ifdef CONFIG_FEATURE_IPC_SYSLOG
#  define USAGE_IPC_LOG(a) a
#else
#  define USAGE_IPC_LOG(a)
#endif

#define sysctl_trivial_usage \
	"[OPTIONS]... [VALUE]..."
#define sysctl_full_usage \
	"configure kernel parameters at runtime\n\n" \
	"Options:\n" \
	"\t-n\tUse this option to disable printing of the key name when printing values\n" \
	"\t-w\tUse this option when you want to change a sysctl setting\n" \
	"\t-p\tLoad in sysctl settings from the file specified or /etc/sysctl.conf if none given\n" \
	"\t-a\tDisplay all values currently available\n" \
	"\t-A\tDisplay all values currently available in table form"
#define sysctl_example_usage \
	"sysctl [-n] variable ...\n" \
	"sysctl [-n] -w variable=value ...\n" \
	"sysctl [-n] -a\n" \
	"sysctl [-n] -p <file>\t(default /etc/sysctl.conf)\n" \
	"sysctl [-n] -A\n"

#define syslogd_trivial_usage \
	"[OPTION]..."
#define syslogd_full_usage \
	"Linux system and kernel logging utility.\n" \
	"Note that this version of syslogd ignores /etc/syslog.conf.\n\n" \
	"Options:\n" \
	"\t-m MIN\t\tMinutes between MARK lines (default=20, 0=off)\n" \
	"\t-n\t\tRun as a foreground process\n" \
	"\t-O FILE\t\tUse an alternate log file (default=/var/log/messages)\n" \
	"\t-S\t\tMake logging output smaller." \
	USAGE_ROTATE_LOGFILE( \
	"\n\t-s SIZE\t\tMax size (KB) before rotate (default=200KB, 0=off)\n" \
	"\t-b NUM\t\tNumber of rotated logs to keep (default=1, max=99, 0=purge)") \
	USAGE_REMOTE_LOG( \
	"\n\t-R HOST[:PORT]\tLog to IP or hostname on PORT (default PORT=514/UDP)\n" \
	"\t-L\t\tLog locally and via network logging (default is network only)") \
	USAGE_IPC_LOG( \
	"\n\t-C [size(KiB)]\tLog to a circular buffer (read the buffer using logread)")
#define syslogd_example_usage \
	"$ syslogd -R masterlog:514\n" \
	"$ syslogd -R 192.168.1.1:601\n"


#ifndef CONFIG_FEATURE_FANCY_TAIL
#  define USAGE_UNSIMPLE_TAIL(a)
#else
#  define USAGE_UNSIMPLE_TAIL(a) a
#endif
#define tail_trivial_usage \
	"[OPTION]... [FILE]..."
#define tail_full_usage \
	"Print last 10 lines of each FILE to standard output.\n" \
	"With more than one FILE, precede each with a header giving the\n" \
	"file name. With no FILE, or when FILE is -, read standard input.\n\n" \
	"Options:\n" \
	USAGE_UNSIMPLE_TAIL("\t-c N[kbm]\toutput the last N bytes\n") \
	"\t-n N[kbm]\tprint last N lines instead of last 10\n" \
	"\t-f\t\toutput data as the file grows" \
	USAGE_UNSIMPLE_TAIL( "\n\t-q\t\tnever output headers giving file names\n" \
	"\t-s SEC\t\twait SEC seconds between reads with -f\n" \
	"\t-v\t\talways output headers giving file names\n\n" \
	"If the first character of N (bytes or lines) is a '+', output begins with \n" \
	"the Nth item from the start of each file, otherwise, print the last N items\n" \
	"in the file. N bytes may be suffixed by k (x1024), b (x512), or m (1024^2)." )
#define tail_example_usage \
	"$ tail -n 1 /etc/resolv.conf\n" \
	"nameserver 10.0.0.1\n"

#ifdef CONFIG_FEATURE_TAR_CREATE
#  define USAGE_TAR_CREATE(a) a
#else
#  define USAGE_TAR_CREATE(a)
#endif
#ifdef CONFIG_FEATURE_TAR_EXCLUDE
#  define USAGE_TAR_EXCLUDE(a) a
#else
#  define USAGE_TAR_EXCLUDE(a)
#endif
#ifdef CONFIG_FEATURE_TAR_GZIP
#  define USAGE_TAR_GZIP(a) a
#else
#  define USAGE_TAR_GZIP(a)
#endif
#ifdef CONFIG_FEATURE_TAR_BZIP2
#  define USAGE_TAR_BZIP2(a) a
#else
#  define USAGE_TAR_BZIP2(a)
#endif
#ifdef CONFIG_FEATURE_TAR_LZMA
#  define USAGE_TAR_LZMA(a) a
#else
#  define USAGE_TAR_LZMA(a)
#endif
#ifdef CONFIG_FEATURE_TAR_COMPRESS
#  define USAGE_TAR_COMPRESS(a) a
#else
#  define USAGE_TAR_COMPRESS(a)
#endif

#define tar_trivial_usage \
	"-[" USAGE_TAR_CREATE("c") USAGE_TAR_GZIP("z") USAGE_TAR_BZIP2("j") USAGE_TAR_LZMA("a") USAGE_TAR_COMPRESS("Z") "xtvO] " \
	USAGE_TAR_EXCLUDE("[-X FILE]") \
	"[-f TARFILE] [-C DIR] [FILE(s)] ..."
#define tar_full_usage \
	"Create, extract, or list files from a tar file.\n\n" \
	"Options:\n" \
	USAGE_TAR_CREATE("\tc\t\tcreate\n") \
	"\tx\t\textract\n" \
	"\tt\t\tlist\n" \
	"\nArchive format selection:\n" \
	USAGE_TAR_GZIP("\tz\t\tFilter the archive through gzip\n") \
	USAGE_TAR_BZIP2("\tj\t\tFilter the archive through bzip2\n") \
	USAGE_TAR_LZMA("\ta\t\tFilter the archive through lzma\n") \
	USAGE_TAR_COMPRESS("\tZ\t\tFilter the archive through compress\n") \
	"\nFile selection:\n" \
	"\tf\t\tname of TARFILE or \"-\" for stdin\n" \
	"\tO\t\textract to stdout\n" \
	USAGE_TAR_EXCLUDE( \
	"\texclude\t\tfile to exclude\n" \
	 "\tX\t\tfile with names to exclude\n" \
	) \
	"\tC\t\tchange to directory DIR before operation\n" \
	"\tv\t\tverbosely list files processed"
#define tar_example_usage \
	"$ zcat /tmp/tarball.tar.gz | tar -xf -\n" \
	"$ tar -cf /tmp/tarball.tar /usr/local\n"

#define tee_trivial_usage \
	"[OPTION]... [FILE]..."
#define tee_full_usage \
	"Copy standard input to each FILE, and also to standard output.\n\n" \
	"Options:\n" \
	"\t-a\tappend to the given FILEs, do not overwrite\n" \
	"\t-i\tignore interrupt signals (SIGINT)"
#define tee_example_usage \
	"$ echo \"Hello\" | tee /tmp/foo\n" \
	"$ cat /tmp/foo\n" \
	"Hello\n"

#ifdef CONFIG_FEATURE_TELNET_AUTOLOGIN
#define telnet_trivial_usage \
	"[-a] [-l USER] HOST [PORT]"
#define telnet_full_usage \
	"Telnet is used to establish interactive communication with another\n" \
	"computer over a network using the TELNET protocol.\n\n" \
	"Options:\n" \
	"\t-a\t\tAttempt an automatic login with the USER variable\n" \
	"\t-l USER\t\tAttempt an automatic login with the USER argument\n" \
	"\tHOST\t\tThe official name, alias or the IP address of the\n" \
	"\t\t\tremote host.\n" \
	"\tPORT\t\tThe remote port number to connect to.  If it is not\n" \
	"\t\t\tspecified, the default telnet (23) port is used."
#else
#define telnet_trivial_usage \
	"HOST [PORT]"
#define telnet_full_usage \
	"Telnet is used to establish interactive communication with another\n" \
	"computer over a network using the TELNET protocol."
#endif

#ifdef CONFIG_FEATURE_TELNETD_INETD
#define telnetd_trivial_usage \
	"(inetd mode) [OPTION]"
#define telnetd_full_usage \
	"Telnetd uses incoming TELNET connections via inetd.\n" \
	"Options:\n" \
	"\t-l LOGIN\texec LOGIN on connect (default /bin/sh)\n" \
	"\t-f issue_file\tDisplay issue_file instead of /etc/issue"
#else
#define telnetd_trivial_usage \
	"[OPTION]"
#define telnetd_full_usage \
	"Telnetd listens for incoming TELNET connections on PORT.\n" \
	"Options:\n" \
	"\t-p PORT\tlisten for connections on PORT (default 23)\n" \
	"\t-l LOGIN\texec LOGIN on connect (default /bin/sh)\n" \
	"\t-f issue_file\tDisplay issue_file instead of /etc/issue"
#endif

#define test_trivial_usage \
	"EXPRESSION\n  or   [ EXPRESSION ]"
#define test_full_usage \
	"Checks file types and compares values returning an exit\n" \
	"code determined by the value of EXPRESSION."
#define test_example_usage \
	"$ test 1 -eq 2\n" \
	"$ echo $?\n" \
	"1\n" \
	"$ test 1 -eq 1\n" \
	"$ echo $?\n" \
	"0\n" \
	"$ [ -d /etc ]\n" \
	"$ echo $?\n" \
	"0\n" \
	"$ [ -d /junk ]\n" \
	"$ echo $?\n" \
	"1\n"

#ifdef CONFIG_FEATURE_TFTP_GET
#  define USAGE_TFTP_GET(a) a
#else
#  define USAGE_TFTP_GET(a)
#endif
#ifdef CONFIG_FEATURE_TFTP_PUT
#  define USAGE_TFTP_PUT(a) a
#else
#  define USAGE_TFTP_PUT(a)
#endif
#ifdef CONFIG_FEATURE_TFTP_BLOCKSIZE
#  define USAGE_TFTP_BS(a) a
#else
#  define USAGE_TFTP_BS(a)
#endif

#define tftp_trivial_usage \
	"[OPTION]... HOST [PORT]"
#define tftp_full_usage \
	"Transfers a file from/to a tftp server using \"octet\" mode.\n\n" \
	"Options:\n" \
	"\t-l FILE\tLocal FILE\n" \
	"\t-r FILE\tRemote FILE" \
	USAGE_TFTP_GET( \
	"\n\t-g\tGet file" \
	) \
	USAGE_TFTP_PUT( \
	"\n\t-p\tPut file" \
	) \
	USAGE_TFTP_BS( \
	"\n\t-b SIZE\tTransfer blocks of SIZE octets" \
	)
#define time_trivial_usage \
	"[OPTION]... COMMAND [ARGS...]"
#define time_full_usage \
	"Runs the program COMMAND with arguments ARGS.  When COMMAND finishes,\n" \
	"COMMAND's resource usage information is displayed\n\n" \
	"Options:\n" \
	"\t-v\tDisplays verbose resource usage information"

#define top_trivial_usage \
	"[-d <seconds>]"
#define top_full_usage \
	"top provides an view of processor activity in real time.\n" \
	"This utility reads the status for all processes in /proc each <seconds>\n" \
	"and shows the status for however many processes will fit on the screen.\n" \
	"This utility will not show processes that are started after program startup,\n" \
	"but it will show the EXIT status for and PIDs that exit while it is running."

#define touch_trivial_usage \
	"[-c] FILE [FILE ...]"
#define touch_full_usage \
	"Update the last-modified date on the given FILE[s].\n\n" \
	"Options:\n" \
	"\t-c\tDo not create any files"
#define touch_example_usage \
	"$ ls -l /tmp/foo\n" \
	"/bin/ls: /tmp/foo: No such file or directory\n" \
	"$ touch /tmp/foo\n" \
	"$ ls -l /tmp/foo\n" \
	"-rw-rw-r--    1 andersen andersen        0 Apr 15 01:11 /tmp/foo\n"

#define tr_trivial_usage \
	"[-cds] STRING1 [STRING2]"
#define tr_full_usage \
	"Translate, squeeze, and/or delete characters from\n" \
	"standard input, writing to standard output.\n\n" \
	"Options:\n" \
	"\t-c\ttake complement of STRING1\n" \
	"\t-d\tdelete input characters coded STRING1\n" \
	"\t-s\tsqueeze multiple output characters of STRING2 into one character"
#define tr_example_usage \
	"$ echo \"gdkkn vnqkc\" | tr [a-y] [b-z]\n" \
	"hello world\n"

#define traceroute_trivial_usage \
	"[-FIldnrv] [-f 1st_ttl] [-m max_ttl] [-p port#] [-q nqueries]\n" \
	"\t[-s src_addr] [-t tos] [-w wait] [-g gateway] [-i iface]\n" \
	"\t[-z pausemsecs] host [data size]"
#define traceroute_full_usage \
	"trace the route ip packets follow going to \"host\"\n" \
	"Options:\n" \
	"\t-F\tSet the don't fragment bit\n" \
	"\t-I\tUse ICMP ECHO instead of UDP datagrams\n" \
	"\t-l\tDisplay the ttl value of the returned packet\n" \
	"\t-d\tSet SO_DEBUG options to socket\n" \
	"\t-n\tPrint hop addresses numerically rather than symbolically\n" \
	"\t-r\tBypass the normal routing tables and send directly to a host\n" \
	"\t-v\tVerbose output\n" \
	"\t-m max_ttl\tSet the max time-to-live (max number of hops)\n" \
	"\t-p port#\tSet the base UDP port number used in probes\n" \
	"\t\t(default is 33434)\n" \
	"\t-q nqueries\tSet the number of probes per ``ttl'' to nqueries\n" \
	"\t\t(default is 3)\n" \
	"\t-s src_addr\tUse the following IP address as the source address\n" \
	"\t-t tos\tSet the type-of-service in probe packets to the following value\n" \
	"\t\t(default 0)\n" \
	"\t-w wait\tSet the time (in seconds) to wait for a response to a probe\n" \
	"\t\t(default 3 sec)\n" \
	"\t-g\tSpecify a loose source route gateway (8 maximum)"


#define true_trivial_usage \
	""
#define true_full_usage \
	"Return an exit code of TRUE (0)."
#define true_example_usage \
	"$ true\n" \
	"$ echo $?\n" \
	"0\n"

#define tty_trivial_usage \
	""
#define tty_full_usage \
	"Print the file name of the terminal connected to standard input.\n\n" \
	"Options:\n" \
	"\t-s\tprint nothing, only return an exit status"
#define tty_example_usage \
	"$ tty\n" \
	"/dev/tty2\n"

#define tune2fs_trivial_usage \
	"[-c max-mounts-count] [-e errors-behavior] [-g group] " \
	"[-i interval[d|m|w]] [-j] [-J journal-options] [-l] [-s sparse-flag] " \
	"[-m reserved-blocks-percent] [-o [^]mount-options[,...]] " \
	"[-r reserved-blocks-count] [-u user] [-C mount-count] " \
	"[-L volume-label] [-M last-mounted-dir] [-O [^]feature[,...]] " \
	"[-T last-check-time] [-U UUID] device"
#define tune2fs_full_usage \
	"Adjust filesystem options on ext[23] filesystems."

#define udhcpc_trivial_usage \
	"[-Cfbnqtv] [-c CID] [-V VCLS] [-H HOSTNAME] [-i INTERFACE]\n[-p pidfile] [-r IP] [-s script]"
#define udhcpc_full_usage \
	"\t-c,\t--clientid=CLIENTID\tSet client identifier\n" \
	"\t-C,\t--clientid-none\tSuppress default client identifier\n" \
	"\t-V,\t--vendorclass=CLASSID\tSet vendor class identifier\n" \
	"\t-H,\t--hostname=HOSTNAME\tClient hostname\n" \
	"\t-h,\t                   \tAlias for -H\n" \
	"\t-f,\t--foreground\tDo not fork after getting lease\n" \
	"\t-b,\t--background\tFork to background if lease cannot be immediately negotiated\n" \
	"\t-i,\t--interface=INTERFACE\tInterface to use (default: eth0)\n" \
	"\t-n,\t--now\tExit with failure if lease cannot be immediately negotiated\n" \
	"\t-p,\t--pidfile=file\tStore process ID of daemon in file\n" \
	"\t-q,\t--quit\tQuit after obtaining lease\n" \
	"\t-r,\t--request=IP\tIP address to request (default: none)\n" \
	"\t-s,\t--script=file\tRun file at dhcp events (default: /usr/share/udhcpc/default.script)\n" \
	"\t-t,\t--retries=NUM\tSend up to NUM request packets\n"\
	"\t-v,\t--version\tDisplay version"

#define udhcpd_trivial_usage \
	"[configfile]\n" \

#define udhcpd_full_usage \
	""

#ifdef CONFIG_FEATURE_MOUNT_FORCE
#  define USAGE_MOUNT_FORCE(a) a
#else
#  define USAGE_MOUNT_FORCE(a)
#endif
#define umount_trivial_usage \
	"[flags] FILESYSTEM|DIRECTORY"
#define umount_full_usage \
	"Unmount file systems\n" \
	"\nFlags:\n" "\t-a\tUnmount all file systems" \
	USAGE_MTAB(" in /etc/mtab\n\t-n\tDon't erase /etc/mtab entries") \
	"\n\t-r\tTry to remount devices as read-only if mount is busy" \
	"\n\t-l\tLazy umount (detach filesystem)" \
	"\n\t-f\tForce umount (i.e., unreachable NFS server)" \
	USAGE_MOUNT_LOOP("\n\t-D\tDo not free loop device (if a loop device has been used)")
#define umount_example_usage \
	"$ umount /dev/hdc1 \n"

#define uname_trivial_usage \
	"[OPTION]..."
#define uname_full_usage \
	"Print certain system information.  With no OPTION, same as -s.\n\n" \
	"Options:\n" \
	"\t-a\tprint all information\n" \
	"\t-m\tthe machine (hardware) type\n" \
	"\t-n\tprint the machine's network node hostname\n" \
	"\t-r\tprint the operating system release\n" \
	"\t-s\tprint the operating system name\n" \
	"\t-p\tprint the host processor type\n" \
	"\t-v\tprint the operating system version"
#define uname_example_usage \
	"$ uname -a\n" \
	"Linux debian 2.4.23 #2 Tue Dec 23 17:09:10 MST 2003 i686 GNU/Linux\n"

#define uncompress_trivial_usage \
	"[-c] [-f] [ name ... ]"
#define uncompress_full_usage \
	"Uncompress .Z file[s]\n" \
	"Options:\n" \
	"\t-c\textract to stdout\n" \
	"\t-f\tforce overwrite an existing file"

#define uniq_trivial_usage \
	"[-fscdu]... [INPUT [OUTPUT]]"
#define uniq_full_usage \
	"Discard all but one of successive identical lines from INPUT\n" \
	"(or standard input), writing to OUTPUT (or standard output).\n\n" \
	"Options:\n" \
	"\t-c\tprefix lines by the number of occurrences\n" \
	"\t-d\tonly print duplicate lines\n" \
	"\t-u\tonly print unique lines\n" \
	"\t-f N\tskip the first N fields\n" \
	"\t-s N\tskip the first N chars (after any skipped fields)"
#define uniq_example_usage \
	"$ echo -e \"a\\na\\nb\\nc\\nc\\na\" | sort | uniq\n" \
	"a\n" \
	"b\n" \
	"c\n"

#define unix2dos_trivial_usage \
	"[option] [FILE]"
#define unix2dos_full_usage \
	"Converts FILE from unix format to dos format.  When no option\n" \
	"is given, the input is converted to the opposite output format.\n" \
	"When no file is given, uses stdin for input and stdout for output.\n" \
	"Options:\n" \
	"\t-u\toutput will be in UNIX format\n" \
	"\t-d\toutput will be in DOS format"

#define unzip_trivial_usage \
	"[-opts[modifiers]] file[.zip] [list] [-x xlist] [-d exdir]"
#define unzip_full_usage \
	"Extracts files from ZIP archives.\n\n" \
	"Options:\n" \
	"\t-l\tlist archive contents (short form)\n" \
	"\t-n\tnever overwrite existing files (default)\n" \
	"\t-o\toverwrite files without prompting\n" \
	"\t-p\tsend output to stdout\n" \
	"\t-q\tbe quiet\n" \
	"\t-x\texclude these files\n" \
	"\t-d\textract files into this directory"

#define uptime_trivial_usage \
	""
#define uptime_full_usage \
	"Display the time since the last boot."
#define uptime_example_usage \
	"$ uptime\n" \
	"  1:55pm  up  2:30, load average: 0.09, 0.04, 0.00\n"

#define usleep_trivial_usage \
	"N"
#define usleep_full_usage \
	"Pause for N microseconds."
#define usleep_example_usage \
	"$ usleep 1000000\n" \
	"[pauses for 1 second]\n"

#define uudecode_trivial_usage \
	"[FILE]..."
#define uudecode_full_usage \
	"Uudecode a file that is uuencoded.\n\n" \
	"Options:\n" \
	"\t-o FILE\tdirect output to FILE"
#define uudecode_example_usage \
	"$ uudecode -o busybox busybox.uu\n" \
	"$ ls -l busybox\n" \
	"-rwxr-xr-x   1 ams      ams        245264 Jun  7 21:35 busybox\n"

#define uuencode_trivial_usage \
	"[OPTION] [INFILE] REMOTEFILE"
#define uuencode_full_usage \
	"Uuencode a file.\n\n" \
	"Options:\n" \
	"\t-m\tuse base64 encoding per RFC1521"
#define uuencode_example_usage \
	"$ uuencode busybox busybox\n" \
	"begin 755 busybox\n" \
	"<encoded file snipped>\n" \
	"$ uudecode busybox busybox > busybox.uu\n" \
	"$\n"

#define vconfig_trivial_usage \
	"COMMAND [OPTIONS] ..."
#define vconfig_full_usage \
	"vconfig lets you create and remove virtual ethernet devices.\n\n" \
	"Options:\n" \
	"\tadd             [interface-name] [vlan_id]\n" \
	"\trem             [vlan-name]\n" \
	"\tset_flag        [interface-name] [flag-num]       [0 | 1]\n" \
	"\tset_egress_map  [vlan-name]      [skb_priority]   [vlan_qos]\n" \
	"\tset_ingress_map [vlan-name]      [skb_priority]   [vlan_qos]\n" \
	"\tset_name_type   [name-type]"

#define vi_trivial_usage \
	"[OPTION] [FILE]..."
#define vi_full_usage \
	"edit FILE.\n\n" \
	"Options:\n" \
	"\t-R\tRead-only- do not write to the file"

#define vlock_trivial_usage \
	"[OPTIONS]"
#define vlock_full_usage \
	"Lock a virtual terminal.  A password is required to unlock\n" \
	"Options:\n" \
	"\t-a\tLock all VTs"

#define watch_trivial_usage \
	"[-n <seconds>] COMMAND..."
#define watch_full_usage \
	"Executes a program periodically.\n" \
	"Options:\n" \
	"\t-n\tLoop period in seconds - default is 2"
#define watch_example_usage \
	"$ watch date\n" \
	"Mon Dec 17 10:31:40 GMT 2000\n" \
	"Mon Dec 17 10:31:42 GMT 2000\n" \
	"Mon Dec 17 10:31:44 GMT 2000"

#define watchdog_trivial_usage \
	"[-t <seconds>] DEV"
#define watchdog_full_usage \
	"Periodically write to watchdog device DEV.\n" \
	"Options:\n" \
	"\t-t\tTimer period in seconds - default is 30"

#define wc_trivial_usage \
	"[OPTION]... [FILE]..."
#define wc_full_usage \
	"Print line, word, and byte counts for each FILE, and a total line if\n" \
	"more than one FILE is specified.  With no FILE, read standard input.\n\n" \
	"Options:\n" \
	"\t-c\tprint the byte counts\n" \
	"\t-l\tprint the newline counts\n" \
	"\t-L\tprint the length of the longest line\n" \
	"\t-w\tprint the word counts"
#define wc_example_usage \
	"$ wc /etc/passwd\n" \
	"     31      46    1365 /etc/passwd\n"

#define wget_trivial_usage \
	"[-c|--continue] [-q|--quiet] [-O|--output-document file]\n" \
	"\t\t[--header 'header: value'] [-Y|--proxy on/off] [-P DIR] url"
#define wget_full_usage \
	"wget retrieves files via HTTP or FTP\n\n" \
	"Options:\n" \
	"\t-c\tcontinue retrieval of aborted transfers\n" \
	"\t-q\tquiet mode - do not print\n" \
	"\t-P\tSet directory prefix to DIR\n" \
	"\t-O\tsave to filename ('-' for stdout)\n" \
	"\t-Y\tuse proxy ('on' or 'off')"

#define which_trivial_usage \
	"[COMMAND ...]"
#define which_full_usage \
	"Locates a COMMAND."
#define which_example_usage \
	"$ which login\n" \
	"/bin/login\n"

#define who_trivial_usage \
	" "
#define who_full_usage \
	"Prints the current user names and related information"

#define whoami_trivial_usage \
	""
#define whoami_full_usage \
	"Prints the user name associated with the current effective user id."

#ifdef CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION
#define USAGE_XARGS_CONFIRMATION(a) a
#else
#define USAGE_XARGS_CONFIRMATION(a)
#endif
#ifdef CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT
#define USAGE_XARGS_TERMOPT(a) a
#else
#define USAGE_XARGS_TERMOPT(a)
#endif
#ifdef CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM
#define USAGE_XARGS_ZERO_TERM(a) a
#else
#define USAGE_XARGS_ZERO_TERM(a)
#endif


#define xargs_trivial_usage \
	"[COMMAND] [OPTIONS] [ARGS...]"
#define xargs_full_usage \
	"Executes COMMAND on every item given by standard input.\n\n" \
	"Options:\n" \
	USAGE_XARGS_CONFIRMATION("\t-p\tPrompt the user about whether to run each command\n") \
	"\t-r\tDo not run command for empty readed lines\n" \
	USAGE_XARGS_TERMOPT("\t-x\tExit if the size is exceeded\n") \
	USAGE_XARGS_ZERO_TERM("\t-0\tInput filenames are terminated by a null character\n") \
	"\t-t\tPrint the command line on stderr before executing it"
#define xargs_example_usage \
	"$ ls | xargs gzip\n" \
	"$ find . -name '*.c' -print | xargs rm\n"

#define yes_trivial_usage \
	"[OPTION]... [STRING]..."
#define yes_full_usage \
	"Repeatedly outputs a line with all specified STRING(s), or 'y'."

#define zcat_trivial_usage \
	"FILE"
#define zcat_full_usage \
	"Uncompress to stdout."

#define zcip_trivial_usage \
	"[OPTIONS] ifname script"
#define zcip_full_usage \
	"zcip manages a ZeroConf IPv4 link-local address.\n" \
	"Options:\n" \
	"\t-f              foreground mode\n" \
	"\t-q              quit after address (no daemon)\n" \
	"\t-r 169.254.x.x  request this address first\n" \
	"\t-v              verbose; show version"

#endif /* __BB_USAGE_H__ */
