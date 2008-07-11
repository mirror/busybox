/* vi: set sw=8 ts=8: */
/*
 * This file suffers from chronically incorrect tabification
 * of messages. Before editing this file:
 * 1. Switch you editor to 8-space tab mode.
 * 2. Do not use \t in messages, use real tab character.
 * 3. Start each source line with message as follows:
 *    |<7 spaces>"text with tabs"....
 * or
 *    |<5 spaces>"\ntext with tabs"....
 */

#ifndef __BB_USAGE_H__
#define __BB_USAGE_H__


#define NOUSAGE_STR "\b"


#define addgroup_trivial_usage \
       "[-g GID] " USE_FEATURE_ADDUSER_TO_GROUP("[user_name] ") "group_name"
#define addgroup_full_usage "\n\n" \
       "Add a group " USE_FEATURE_ADDUSER_TO_GROUP("or add an user to a group") "\n" \
     "\nOptions:" \
     "\n	-g GID	Group id" \

#define adduser_trivial_usage \
       "[OPTIONS] user_name"
#define adduser_full_usage "\n\n" \
       "Add an user\n" \
     "\nOptions:" \
     "\n	-h DIR		Home directory" \
     "\n	-g GECOS	GECOS field" \
     "\n	-s SHELL	Login shell" \
     "\n	-G GROUP	Add user to existing group" \
     "\n	-S		Create a system user" \
     "\n	-D		Do not assign a password" \
     "\n	-H		Do not create home directory" \

#define adjtimex_trivial_usage \
       "[-q] [-o offset] [-f frequency] [-p timeconstant] [-t tick]"
#define adjtimex_full_usage "\n\n" \
       "Read and optionally set system timebase parameters. See adjtimex(2).\n" \
     "\nOptions:" \
     "\n	-q		Quiet" \
     "\n	-o offset	Time offset, microseconds" \
     "\n	-f frequency	Frequency adjust, integer kernel units (65536 is 1ppm)" \
     "\n			(positive values make clock run faster)" \
     "\n	-t tick		Microseconds per tick, usually 10000" \
     "\n	-p timeconstant" \

#define ar_trivial_usage \
       "[-o] [-v] [-p] [-t] [-x] ARCHIVE FILES"
#define ar_full_usage "\n\n" \
       "Extract or list FILES from an ar archive\n" \
     "\nOptions:" \
     "\n	-o	Preserve original dates" \
     "\n	-p	Extract to stdout" \
     "\n	-t	List" \
     "\n	-x	Extract" \
     "\n	-v	Verbose" \

#define arp_trivial_usage \
       "\n" \
       "[-vn]	[-H type] [-i if] -a [hostname]\n" \
       "[-v]		  [-i if] -d hostname [pub]\n" \
       "[-v]	[-H type] [-i if] -s hostname hw_addr [temp]\n" \
       "[-v]	[-H type] [-i if] -s hostname hw_addr [netmask nm] pub\n" \
       "[-v]	[-H type] [-i if] -Ds hostname ifa [netmask nm] pub\n"
#define arp_full_usage "\n\n" \
       "Manipulate ARP cache\n" \
     "\nOptions:" \
       "\n	-a		Display (all) hosts" \
       "\n	-s		Set new ARP entry" \
       "\n	-d		Delete a specified entry" \
       "\n	-v		Verbose" \
       "\n	-n		Don't resolve names" \
       "\n	-i IF		Network interface" \
       "\n	-D		Read <hwaddr> from given device" \
       "\n	-A, -p AF	Protocol family" \
       "\n	-H HWTYPE	Hardware address type" \

#define arping_trivial_usage \
       "[-fqbDUA] [-c count] [-w timeout] [-I dev] [-s sender] target"
#define arping_full_usage "\n\n" \
       "Send ARP requests/replies\n" \
     "\nOptions:" \
     "\n	-f		Quit on first ARP reply" \
     "\n	-q		Quiet" \
     "\n	-b		Keep broadcasting, don't go unicast" \
     "\n	-D		Duplicated address detection mode" \
     "\n	-U		Unsolicited ARP mode, update your neighbors" \
     "\n	-A		ARP answer mode, update your neighbors" \
     "\n	-c N		Stop after sending N ARP requests" \
     "\n	-w timeout	Time to wait for ARP reply, in seconds" \
     "\n	-I dev		Interface to use (default eth0)" \
     "\n	-s sender	Sender IP address" \
     "\n	target		Target IP address" \

#define sh_trivial_usage NOUSAGE_STR
#define sh_full_usage ""
#define ash_trivial_usage NOUSAGE_STR
#define ash_full_usage ""
#define hush_trivial_usage NOUSAGE_STR
#define hush_full_usage ""
#define msh_trivial_usage NOUSAGE_STR
#define msh_full_usage ""

#define awk_trivial_usage \
       "[OPTION]... [program-text] [FILE...]"
#define awk_full_usage "\n\n" \
       "Options:" \
     "\n	-v var=val	Set variable" \
     "\n	-F sep		Use sep as field separator" \
     "\n	-f file		Read program from file" \

#define basename_trivial_usage \
       "FILE [SUFFIX]"
#define basename_full_usage "\n\n" \
       "Strip directory path and suffixes from FILE.\n" \
       "If specified, also remove any trailing SUFFIX."
#define basename_example_usage \
       "$ basename /usr/local/bin/foo\n" \
       "foo\n" \
       "$ basename /usr/local/bin/\n" \
       "bin\n" \
       "$ basename /foo/bar.txt .txt\n" \
       "bar"

#define fbsplash_trivial_usage \
       "-s IMGFILE [-c] [-d DEV] [-i INIFILE] [-f CMD]"
#define fbsplash_full_usage "\n\n" \
       "Options:\n" \
     "\n	-s	Image" \
     "\n	-c	Hide cursor" \
     "\n	-d	Framebuffer device (default /dev/fb0)" \
     "\n	-i	Config file (var=value):" \
     "\n			BAR_LEFT,BAR_TOP,BAR_WIDTH,BAR_HEIGHT" \
     "\n			BAR_R,BAR_G,BAR_B" \
     "\n	-f	Control pipe (else exit after drawing image)" \
     "\n			commands: 'NN' (% for progress bar) or 'exit'" \

#define brctl_trivial_usage \
       "COMMAND [BRIDGE [INTERFACE]]"
#define brctl_full_usage "\n\n" \
       "Manage ethernet bridges.\n" \
     "\nCommands:" \
	USE_FEATURE_BRCTL_SHOW( \
     "\n	show			Show a list of bridges" \
	) \
     "\n	addbr BRIDGE		Create BRIDGE" \
     "\n	delbr BRIDGE		Delete BRIDGE" \
     "\n	addif BRIDGE IFACE	Add IFACE to BRIDGE" \
     "\n	delif BRIDGE IFACE	Delete IFACE from BRIDGE" \
	USE_FEATURE_BRCTL_FANCY( \
     "\n	setageing BRIDGE TIME		Set ageing time" \
     "\n	setfd BRIDGE TIME		Set bridge forward delay" \
     "\n	sethello BRIDGE TIME		Set hello time" \
     "\n	setmaxage BRIDGE TIME		Set max message age" \
     "\n	setpathcost BRIDGE COST		Set path cost" \
     "\n	setportprio BRIDGE PRIO		Set port priority" \
     "\n	setbridgeprio BRIDGE PRIO	Set bridge priority" \
     "\n	stp BRIDGE [1|0]		STP on/off" \
	) \

#define bunzip2_trivial_usage \
       "[OPTION]... [FILE]"
#define bunzip2_full_usage "\n\n" \
       "Uncompress FILE (or standard input if FILE is '-' or omitted)\n" \
     "\nOptions:" \
     "\n	-c	Write to standard output" \
     "\n	-f	Force" \

#define bzip2_trivial_usage \
       "[OPTION]... [FILE]..."
#define bzip2_full_usage "\n\n" \
       "Compress FILE(s) with bzip2 algorithm.\n" \
       "When FILE is '-' or unspecified, reads standard input. Implies -c.\n" \
     "\nOptions:" \
     "\n	-c	Write to standard output" \
     "\n	-d	Decompress" \
     "\n	-f	Force" \
     "\n	-1..-9	Compression level" \

#define busybox_notes_usage \
       "Hello world!\n"

#define bzcat_trivial_usage \
       "FILE"
#define bzcat_full_usage "\n\n" \
       "Uncompress to stdout"

#define unlzma_trivial_usage \
       "[OPTION]... [FILE]"
#define unlzma_full_usage "\n\n" \
       "Uncompress FILE (or standard input if FILE is '-' or omitted)\n" \
     "\nOptions:" \
     "\n	-c	Write to standard output" \
     "\n	-f	Force" \

#define lzmacat_trivial_usage \
       "FILE"
#define lzmacat_full_usage "\n\n" \
       "Uncompress to stdout"

#define cal_trivial_usage \
       "[-jy] [[month] year]"
#define cal_full_usage "\n\n" \
       "Display a calendar\n" \
     "\nOptions:" \
     "\n	-j	Use julian dates" \
     "\n	-y	Display the entire year" \

#define cat_trivial_usage \
       "[-u] [FILE]..."
#define cat_full_usage "\n\n" \
       "Concatenate FILE(s) and print them to stdout\n" \
     "\nOptions:" \
     "\n	-u	Use unbuffered i/o (ignored)" \

#define cat_example_usage \
       "$ cat /proc/uptime\n" \
       "110716.72 17.67"

#define catv_trivial_usage \
       "[-etv] [FILE]..."
#define catv_full_usage "\n\n" \
       "Display nonprinting characters as ^x or M-x\n" \
     "\nOptions:" \
     "\n	-e	End each line with $" \
     "\n	-t	Show tabs as ^I" \
     "\n	-v	Don't use ^x or M-x escapes" \

#define chat_trivial_usage \
       "EXPECT [SEND [EXPECT [SEND...]]]"
#define chat_full_usage "\n\n" \
       "Useful for interacting with a modem connected to stdin/stdout.\n" \
       "A script consists of one or more \"expect-send\" pairs of strings,\n" \
       "each pair is a pair of arguments. Example:\n" \
       "chat '' ATZ OK ATD123456 CONNECT '' ogin: pppuser word: ppppass '~'" \

#define chattr_trivial_usage \
       "[-R] [-+=AacDdijsStTu] [-v version] files..."
#define chattr_full_usage "\n\n" \
       "Change file attributes on an ext2 fs\n" \
     "\nModifiers:" \
     "\n	-	Remove attributes" \
     "\n	+	Add attributes" \
     "\n	=	Set attributes" \
     "\nAttributes:" \
     "\n	A	Don't track atime" \
     "\n	a	Append mode only" \
     "\n	c	Enable compress" \
     "\n	D	Write dir contents synchronously" \
     "\n	d	Do not backup with dump" \
     "\n	i	Cannot be modified (immutable)" \
     "\n	j	Write all data to journal first" \
     "\n	s	Zero disk storage when deleted" \
     "\n	S	Write file contents synchronously" \
     "\n	t	Disable tail-merging of partial blocks with other files" \
     "\n	u	Allow file to be undeleted" \
     "\nOptions:" \
     "\n	-R	Recursively list subdirectories" \
     "\n	-v	Set the file's version/generation number" \

#define chcon_trivial_usage \
       "[OPTIONS] CONTEXT FILE..." \
       "\n	chcon [OPTIONS] [-u USER] [-r ROLE] [-l RANGE] [-t TYPE] FILE..." \
	USE_FEATURE_CHCON_LONG_OPTIONS( \
       "\n	chcon [OPTIONS] --reference=RFILE FILE..." \
	)
#define chcon_full_usage "\n\n" \
       "Change the security context of each FILE to CONTEXT\n" \
	USE_FEATURE_CHCON_LONG_OPTIONS( \
     "\n	-v,--verbose		Verbose" \
     "\n	-c,--changes		Report changes made" \
     "\n	-h,--no-dereference	Affect symlinks instead of their targets" \
     "\n	-f,--silent,--quiet	Suppress most error messages" \
     "\n	--reference=RFILE	Use RFILE's group instead of using a CONTEXT value" \
     "\n	-u,--user=USER		Set user/role/type/range in the target" \
     "\n	-r,--role=ROLE		security context" \
     "\n	-t,--type=TYPE" \
     "\n	-l,--range=RANGE" \
     "\n	-R,--recursive		Recurse subdirectories" \
	) \
	SKIP_FEATURE_CHCON_LONG_OPTIONS( \
     "\n	-v	Verbose" \
     "\n	-c	Report changes made" \
     "\n	-h	Affect symlinks instead of their targets" \
     "\n	-f	Suppress most error messages" \
     "\n	-u USER	Set user/role/type/range in the target security context" \
     "\n	-r ROLE" \
     "\n	-t TYPE" \
     "\n	-l RNG" \
     "\n	-R	Recurse subdirectories" \
	)

#define chmod_trivial_usage \
       "[-R"USE_DESKTOP("cvf")"] MODE[,MODE]... FILE..."
#define chmod_full_usage "\n\n" \
       "Each MODE is one or more of the letters ugoa, one of the\n" \
       "symbols +-= and one or more of the letters rwxst\n" \
     "\nOptions:" \
     "\n	-R	Recurse directories" \
	USE_DESKTOP( \
     "\n	-c	List changed files" \
     "\n	-v	List all files" \
     "\n	-f	Hide errors" \
	)
#define chmod_example_usage \
       "$ ls -l /tmp/foo\n" \
       "-rw-rw-r--    1 root     root            0 Apr 12 18:25 /tmp/foo\n" \
       "$ chmod u+x /tmp/foo\n" \
       "$ ls -l /tmp/foo\n" \
       "-rwxrw-r--    1 root     root            0 Apr 12 18:25 /tmp/foo*\n" \
       "$ chmod 444 /tmp/foo\n" \
       "$ ls -l /tmp/foo\n" \
       "-r--r--r--    1 root     root            0 Apr 12 18:25 /tmp/foo\n"

#define chgrp_trivial_usage \
       "[-RhLHP"USE_DESKTOP("cvf")"]... GROUP FILE..."
#define chgrp_full_usage "\n\n" \
       "Change the group membership of each FILE to GROUP\n" \
     "\nOptions:" \
     "\n	-R	Recurse directories" \
     "\n	-h	Affect symlinks instead of symlink targets" \
     "\n	-L	Traverse all symlinks to directories" \
     "\n	-H	Traverse symlinks on command line only" \
     "\n	-P	Do not traverse symlinks (default)" \
	USE_DESKTOP( \
     "\n	-c	List changed files" \
     "\n	-v	Verbose" \
     "\n	-f	Hide errors" \
	)
#define chgrp_example_usage \
       "$ ls -l /tmp/foo\n" \
       "-r--r--r--    1 andersen andersen        0 Apr 12 18:25 /tmp/foo\n" \
       "$ chgrp root /tmp/foo\n" \
       "$ ls -l /tmp/foo\n" \
       "-r--r--r--    1 andersen root            0 Apr 12 18:25 /tmp/foo\n"

#define chown_trivial_usage \
       "[-RhLHP"USE_DESKTOP("cvf")"]... OWNER[<.|:>[GROUP]] FILE..."
#define chown_full_usage "\n\n" \
       "Change the owner and/or group of each FILE to OWNER and/or GROUP\n" \
     "\nOptions:" \
     "\n	-R	Recurse directories" \
     "\n	-h	Affect symlinks instead of symlink targets" \
     "\n	-L	Traverse all symlinks to directories" \
     "\n	-H	Traverse symlinks on command line only" \
     "\n	-P	Do not traverse symlinks (default)" \
	USE_DESKTOP( \
     "\n	-c	List changed files" \
     "\n	-v	List all files" \
     "\n	-f	Hide errors" \
	)
#define chown_example_usage \
       "$ ls -l /tmp/foo\n" \
       "-r--r--r--    1 andersen andersen        0 Apr 12 18:25 /tmp/foo\n" \
       "$ chown root /tmp/foo\n" \
       "$ ls -l /tmp/foo\n" \
       "-r--r--r--    1 root     andersen        0 Apr 12 18:25 /tmp/foo\n" \
       "$ chown root.root /tmp/foo\n" \
       "ls -l /tmp/foo\n" \
       "-r--r--r--    1 root     root            0 Apr 12 18:25 /tmp/foo\n"

#define chpst_trivial_usage \
       "[-vP012] [-u user[:group]] [-U user[:group]] [-e dir] " \
       "[-/ dir] [-n nice] [-m bytes] [-d bytes] [-o files] " \
       "[-p processes] [-f bytes] [-c bytes] prog args"
#define chpst_full_usage "\n\n" \
       "Change the process state and run specified program\n" \
     "\nOptions:" \
     "\n	-u USER[:GRP]	Set uid and gid" \
     "\n	-U USER[:GRP]	Set $UID and $GID in environment" \
     "\n	-e DIR		Set environment variables as specified by files" \
     "\n			in DIR: file=1st_line_of_file" \
     "\n	-/ DIR		Chroot to DIR" \
     "\n	-n INC		Add INC to nice value" \
     "\n	-m BYTES	Limit data segment, stack segment, locked physical pages," \
     "\n			and total of all segment per process to BYTES each" \
     "\n	-d BYTES	Limit data segment" \
     "\n	-o N		Limit the number of open file descriptors per process to N" \
     "\n	-p N		Limit number of processes per uid to N" \
     "\n	-f BYTES	Limit output file size to BYTES" \
     "\n	-c BYTES	Limit core file size to BYTES" \
     "\n	-v		Verbose" \
     "\n	-P		Run prog in a new process group" \
     "\n	-0		Close standard input" \
     "\n	-1		Close standard output" \
     "\n	-2		Close standard error" \

#define setuidgid_trivial_usage \
       "account prog args"
#define setuidgid_full_usage "\n\n" \
       "Set uid and gid to account's uid and gid, removing all supplementary\n" \
       "groups, then run prog"
#define envuidgid_trivial_usage \
       "account prog args"
#define envuidgid_full_usage "\n\n" \
       "Set $UID to account's uid and $GID to account's gid, then run prog"
#define envdir_trivial_usage \
       "dir prog args"
#define envdir_full_usage "\n\n" \
       "Set various environment variables as specified by files\n" \
       "in the directory dir, then run prog"
#define softlimit_trivial_usage \
       "[-a allbytes] [-c corebytes] [-d databytes] [-f filebytes] " \
       "[-l lockbytes] [-m membytes] [-o openfiles] [-p processes] " \
       "[-r residentbytes] [-s stackbytes] [-t cpusecs] prog args"
#define softlimit_full_usage "\n\n" \
       "Set soft resource limits, then run prog\n" \
     "\nOptions:" \
     "\n	-m n	Same as -d n -s n -l n -a n" \
     "\n	-d n	Limit the data segment per process to n bytes" \
     "\n	-s n	Limit the stack segment per process to n bytes" \
     "\n	-l n	Limit the locked physical pages per process to n bytes" \
     "\n	-a n	Limit the total of all segments per process to n bytes" \
     "\n	-o n	Limit the number of open file descriptors per process to n" \
     "\n	-p n	Limit the number of processes per uid to n" \
     "\nOptions controlling file sizes:" \
     "\n	-f n	Limit output file sizes to n bytes" \
     "\n	-c n	Limit core file sizes to n bytes" \
     "\nEfficiency opts:" \
     "\n	-r n	Limit the resident set size to n bytes. This limit is not" \
     "\n		enforced unless physical memory is full" \
     "\n	-t n	Limit the CPU time to n seconds. This limit is not enforced" \
     "\n		except that the process receives a SIGXCPU signal after n seconds" \
     "\n" \
     "\nSome options may have no effect on some operating systems" \
     "\nn may be =, indicating that soft limit should be set equal to hard limit" \

#define chroot_trivial_usage \
       "NEWROOT [COMMAND...]"
#define chroot_full_usage "\n\n" \
       "Run COMMAND with root directory set to NEWROOT"
#define chroot_example_usage \
       "$ ls -l /bin/ls\n" \
       "lrwxrwxrwx    1 root     root          12 Apr 13 00:46 /bin/ls -> /BusyBox\n" \
       "# mount /dev/hdc1 /mnt -t minix\n" \
       "# chroot /mnt\n" \
       "# ls -l /bin/ls\n" \
       "-rwxr-xr-x    1 root     root        40816 Feb  5 07:45 /bin/ls*\n"

#define chvt_trivial_usage \
       "N"
#define chvt_full_usage "\n\n" \
       "Change the foreground virtual terminal to /dev/ttyN"

#define cksum_trivial_usage \
       "FILES..."
#define cksum_full_usage "\n\n" \
       "Calculate the CRC32 checksums of FILES"

#define clear_trivial_usage \
       ""
#define clear_full_usage "\n\n" \
       "Clear screen"

#define cmp_trivial_usage \
       "[-l] [-s] FILE1 [FILE2" USE_DESKTOP(" [SKIP1 [SKIP2]") "]]"
#define cmp_full_usage "\n\n" \
       "Compares FILE1 vs stdin if FILE2 is not specified\n" \
     "\nOptions:" \
     "\n	-l	Write the byte numbers (decimal) and values (octal)" \
     "\n		for all differing bytes" \
     "\n	-s	Quiet" \

#define comm_trivial_usage \
       "[-123] FILE1 FILE2"
#define comm_full_usage "\n\n" \
       "Compare FILE1 to FILE2, or to stdin if - is specified\n" \
     "\nOptions:" \
     "\n	-1	Suppress lines unique to FILE1" \
     "\n	-2	Suppress lines unique to FILE2" \
     "\n	-3	Suppress lines common to both files" \

#define bbconfig_trivial_usage \
       ""
#define bbconfig_full_usage "\n\n" \
       "Print the config file which built busybox"

#define bbsh_trivial_usage \
       "[FILE]...\n" \
       "or: bbsh -c command [args]..."
#define bbsh_full_usage "\n\n" \
       "The bbsh shell (command interpreter)"

#define chrt_trivial_usage \
       "[OPTION]... [prio] [pid | command [arg]...]"
#define chrt_full_usage "\n\n" \
       "Manipulate real-time attributes of a process\n" \
     "\nOptions:" \
     "\n	-p	Operate on pid" \
     "\n	-r	Set scheduling policy to SCHED_RR" \
     "\n	-f	Set scheduling policy to SCHED_FIFO" \
     "\n	-o	Set scheduling policy to SCHED_OTHER" \
     "\n	-m	Show min and max priorities" \

#define chrt_example_usage \
       "$ chrt -r 4 sleep 900; x=$!\n" \
       "$ chrt -f -p 3 $x\n" \
       "You need CAP_SYS_NICE privileges to set scheduling attributes of a process"

#define cp_trivial_usage \
       "[OPTION]... SOURCE DEST"
#define cp_full_usage "\n\n" \
       "Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY\n" \
     "\nOptions:" \
     "\n	-a	Same as -dpR" \
	USE_SELINUX( \
     "\n	-c	Preserve security context" \
	) \
     "\n	-d,-P	Preserve links" \
     "\n	-H,-L	Dereference all symlinks (default)" \
     "\n	-p	Preserve file attributes if possible" \
     "\n	-f	Force overwrite" \
     "\n	-i	Prompt before overwrite" \
     "\n	-R,-r	Recurse directories" \
     "\n	-l,-s	Create (sym)links" \

#define cpio_trivial_usage \
       "-[dim" USE_FEATURE_CPIO_O("o") "tuv][F cpiofile]" \
       USE_FEATURE_CPIO_O( "[H newc]" )
#define cpio_full_usage "\n\n" \
       "Extract or list files from a cpio archive" \
	USE_FEATURE_CPIO_O( ", or create a cpio archive" ) \
     "\n" \
       "Main operation mode:" \
     "\n	d	Make leading directories" \
     "\n	i	Extract" \
     "\n	m	Preserve mtime" \
	USE_FEATURE_CPIO_O( \
     "\n	o	Create" \
     "\n	H newc	Define format" \
	) \
     "\n	t	List" \
     "\n	v	Verbose" \
     "\n	u	Unconditional overwrite" \
     "\n	F	Input from file" \

#define crond_trivial_usage \
       "-fbS -l N " USE_DEBUG_CROND_OPTION("-d N ") "-L LOGFILE -c DIR"
#define crond_full_usage "\n\n" \
       "	-f	Foreground" \
     "\n	-b	Background (default)" \
     "\n	-S	Log to syslog (default)" \
     "\n	-l	Set log level. 0 is the most verbose, default 8" \
	USE_DEBUG_CROND_OPTION( \
     "\n	-d	Set log level, log to stderr" \
	) \
     "\n	-L	Log to file" \
     "\n	-c	Working dir" \

#define crontab_trivial_usage \
       "[-c DIR] [-u USER] [-ler]|[FILE]"
#define crontab_full_usage "\n\n" \
       "	-c	Crontab directory" \
     "\n	-u	User" \
     "\n	-l	List crontab" \
     "\n	-e	Edit crontab" \
     "\n	-r	Delete crontab" \
     "\n	FILE	Replace crontab by FILE ('-': stdin)" \

#define cryptpw_trivial_usage \
       "[-a des|md5] [string]"
#define cryptpw_full_usage "\n\n" \
       "Output crypted string.\n" \
       "If string isn't supplied on cmdline, read it from stdin.\n" \
     "\nOptions:" \
     "\n	-a	Algorithm to use (default: md5)" \

#define cttyhack_trivial_usage NOUSAGE_STR
#define cttyhack_full_usage ""

#define cut_trivial_usage \
       "[OPTION]... [FILE]..."
#define cut_full_usage "\n\n" \
       "Print selected fields from each input FILE to standard output\n" \
     "\nOptions:" \
     "\n	-b LIST	Output only bytes from LIST" \
     "\n	-c LIST	Output only characters from LIST" \
     "\n	-d CHAR	Use CHAR instead of tab as the field delimiter" \
     "\n	-s	Output only the lines containing delimiter" \
     "\n	-f N	Print only these fields" \
     "\n	-n	Ignored" \

#define cut_example_usage \
       "$ echo \"Hello world\" | cut -f 1 -d ' '\n" \
       "Hello\n" \
       "$ echo \"Hello world\" | cut -f 2 -d ' '\n" \
       "world\n"

#define date_trivial_usage \
       "[OPTION]... [+FMT] [TIME]"
#define date_full_usage "\n\n" \
       "Display time (using +FMT), or set time\n" \
     "\nOptions:" \
     "\n	-u		Work in UTC (don't convert to local time)" \
     "\n	-R		Output RFC-822 compliant date string" \
	USE_FEATURE_DATE_ISOFMT( \
     "\n	-I[SPEC]	Output ISO-8601 compliant date string" \
     "\n			SPEC='date' (default) for date only," \
     "\n			'hours', 'minutes', or 'seconds' for date and" \
     "\n			time to the indicated precision" \
	) \
     "\n	-d TIME		Display TIME, not 'now'" \
     "\n	-r FILE		Display last modification time of FILE" \
     "\n	[-s] TIME	Set time to TIME" \
	USE_FEATURE_DATE_ISOFMT( \
     "\n	-D FMT		Use FMT for str->date conversion" \
	) \
     "\n" \
     "\nRecognized formats for TIME:" \
     "\n	hh:mm[:ss]" \
     "\n	[YYYY.]MM.DD-hh:mm[:ss]" \
     "\n	YYYY-MM-DD hh:mm[:ss]" \
     "\n	MMDDhhmm[[YY]YY][.ss]" \

#define date_example_usage \
       "$ date\n" \
       "Wed Apr 12 18:52:41 MDT 2000\n"

#define dc_trivial_usage \
       "expression..."
#define dc_full_usage "\n\n" \
       "Tiny RPN calculator. Operations:\n" \
       "+, add, -, sub, *, mul, /, div, %, mod, **, exp, and, or, not, eor,\n" \
       "p - print top of the stack (without altering the stack),\n" \
       "f - print entire stack, o - pop the value and set output radix\n" \
       "(value must be 10 or 16).\n" \
       "Examples: 'dc 2 2 add' -> 4, 'dc 8 8 * 2 2 + /' -> 16.\n" \

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
       "[if=FILE] [of=FILE] " USE_FEATURE_DD_IBS_OBS("[ibs=N] [obs=N] ") "[bs=N] [count=N] [skip=N]\n" \
       "	[seek=N]" USE_FEATURE_DD_IBS_OBS(" [conv=notrunc|noerror|sync|fsync]")
#define dd_full_usage "\n\n" \
       "Copy a file with converting and formatting\n" \
     "\nOptions:" \
     "\n	if=FILE		Read from FILE instead of stdin" \
     "\n	of=FILE		Write to FILE instead of stdout" \
     "\n	bs=N		Read and write N bytes at a time" \
	USE_FEATURE_DD_IBS_OBS( \
     "\n	ibs=N		Read N bytes at a time" \
	) \
	USE_FEATURE_DD_IBS_OBS( \
     "\n	obs=N		Write N bytes at a time" \
	) \
     "\n	count=N		Copy only N input blocks" \
     "\n	skip=N		Skip N input blocks" \
     "\n	seek=N		Skip N output blocks" \
	USE_FEATURE_DD_IBS_OBS( \
     "\n	conv=notrunc	Don't truncate output file" \
     "\n	conv=noerror	Continue after read errors" \
     "\n	conv=sync	Pad blocks with zeros" \
     "\n	conv=fsync	Physically write data out before finishing" \
	) \
     "\n" \
     "\nNumbers may be suffixed by c (x1), w (x2), b (x512), kD (x1000), k (x1024)," \
     "\nMD (x1000000), M (x1048576), GD (x1000000000) or G (x1073741824)" \

#define dd_example_usage \
       "$ dd if=/dev/zero of=/dev/ram1 bs=1M count=4\n" \
       "4+0 records in\n" \
       "4+0 records out\n"

#define deallocvt_trivial_usage \
       "[N]"
#define deallocvt_full_usage "\n\n" \
       "Deallocate unused virtual terminal /dev/ttyN"

#define delgroup_trivial_usage \
	USE_FEATURE_DEL_USER_FROM_GROUP("[USER] ")"GROUP"
#define delgroup_full_usage "\n\n" \
       "Delete group GROUP from the system" \
	USE_FEATURE_DEL_USER_FROM_GROUP(" or user USER from group GROUP")

#define deluser_trivial_usage \
       "USER"
#define deluser_full_usage "\n\n" \
       "Delete USER from the system"

#define depmod_trivial_usage NOUSAGE_STR
#define depmod_full_usage ""

#define devfsd_trivial_usage \
       "mntpnt [-v]" USE_DEVFSD_FG_NP("[-fg][-np]")
#define devfsd_full_usage "\n\n" \
       "Manage devfs permissions and old device name symlinks\n" \
     "\nOptions:" \
     "\n	mntpnt	The mount point where devfs is mounted" \
     "\n	-v	Print the protocol version numbers for devfsd" \
     "\n		and the kernel-side protocol version and exit" \
	USE_DEVFSD_FG_NP( \
     "\n	-fg	Run in foreground" \
     "\n	-np	Exit after parsing the configuration file" \
     "\n		and processing synthetic REGISTER events," \
     "\n		do not poll for events" \
	)

/* -k is accepted but ignored for !HUMAN_READABLE,
 * but we won't mention this (unimportant) */
#if ENABLE_FEATURE_HUMAN_READABLE || ENABLE_FEATURE_DF_INODE
#define DF_HAS_OPTIONS(x) x
#else
#define DF_HAS_OPTIONS(x)
#endif
#define df_trivial_usage \
	DF_HAS_OPTIONS("[-") \
	USE_FEATURE_HUMAN_READABLE("hmk") USE_FEATURE_DF_INODE("i") \
	DF_HAS_OPTIONS("] ") "[FILESYSTEM...]"
#define df_full_usage "\n\n" \
       "Print filesystem usage statistics\n" \
	DF_HAS_OPTIONS("\nOptions:") \
	USE_FEATURE_HUMAN_READABLE( \
     "\n	-h	Human readable (e.g. 1K 243M 2G)" \
     "\n	-m	1024*1024 blocks" \
     "\n	-k	1024 blocks" \
	) \
	USE_FEATURE_DF_INODE( \
     "\n	-i	Inodes" \
	)
#define df_example_usage \
       "$ df\n" \
       "Filesystem           1k-blocks      Used Available Use% Mounted on\n" \
       "/dev/sda3              8690864   8553540    137324  98% /\n" \
       "/dev/sda1                64216     36364     27852  57% /boot\n" \
       "$ df /dev/sda3\n" \
       "Filesystem           1k-blocks      Used Available Use% Mounted on\n" \
       "/dev/sda3              8690864   8553540    137324  98% /\n"

#define dhcprelay_trivial_usage \
       "[client1,client2,...] [server_device]"
#define dhcprelay_full_usage "\n\n" \
       "Relay dhcp requests from client devices to server device.\n" \
       "Pass clients as CSV"

#define diff_trivial_usage \
       "[-abdiNqrTstw] [-L LABEL] [-S FILE] [-U LINES] FILE1 FILE2"
#define diff_full_usage "\n\n" \
       "Compare files line by line and output the differences between them.\n" \
       "This implementation supports unified diffs only.\n" \
     "\nOptions:" \
     "\n	-a	Treat all files as text" \
     "\n	-b	Ignore changes in the amount of whitespace" \
     "\n	-d	Try hard to find a smaller set of changes" \
     "\n	-i	Ignore case differences" \
     "\n	-L	Use LABEL instead of the filename in the unified header" \
     "\n	-N	Treat absent files as empty" \
     "\n	-q	Output only whether files differ" \
     "\n	-r	Recursively compare subdirectories" \
     "\n	-S	Start with FILE when comparing directories" \
     "\n	-T	Make tabs line up by prefixing a tab when necessary" \
     "\n	-s	Report when two files are the same" \
     "\n	-t	Expand tabs to spaces in output" \
     "\n	-U	Output LINES lines of context" \
     "\n	-w	Ignore all whitespace" \

#define dirname_trivial_usage \
       "FILENAME"
#define dirname_full_usage "\n\n" \
       "Strip non-directory suffix from FILENAME"
#define dirname_example_usage \
       "$ dirname /tmp/foo\n" \
       "/tmp\n" \
       "$ dirname /tmp/foo/\n" \
       "/tmp\n"

#define dmesg_trivial_usage \
       "[-c] [-n LEVEL] [-s SIZE]"
#define dmesg_full_usage "\n\n" \
       "Print or control the kernel ring buffer\n" \
     "\nOptions:" \
     "\n	-c		Clear ring buffer after printing" \
     "\n	-n LEVEL	Set console logging level" \
     "\n	-s SIZE		Buffer size" \

#define dnsd_trivial_usage \
       "[-c config] [-t seconds] [-p port] [-i iface-ip] [-d]"
#define dnsd_full_usage "\n\n" \
       "Small static DNS server daemon\n" \
     "\nOptions:" \
     "\n	-c	Config filename" \
     "\n	-t	TTL in seconds" \
     "\n	-p	Listening port" \
     "\n	-i	Listening ip (default all)" \
     "\n	-d	Daemonize" \

#define dos2unix_trivial_usage \
       "[option] [FILE]"
#define dos2unix_full_usage "\n\n" \
       "Convert FILE from dos to unix format.\n" \
       "When no file is given, use stdin/stdout.\n" \
     "\nOptions:" \
     "\n	-u	dos2unix" \
     "\n	-d	unix2dos" \

#define dpkg_trivial_usage \
       "[-ilCPru] [-F option] package_name"
#define dpkg_full_usage "\n\n" \
       "Install, remove and manage Debian packages\n" \
     "\nOptions:" \
     "\n	-i		Install the package" \
     "\n	-l		List of installed packages" \
     "\n	-C		Configure an unpackaged package" \
     "\n	-F depends	Ignore dependency problems" \
     "\n	-P		Purge all files of a package" \
     "\n	-r		Remove all but the configuration files for a package" \
     "\n	-u		Unpack a package, but don't configure it" \

#define dpkg_deb_trivial_usage \
       "[-cefxX] FILE [argument]"
#define dpkg_deb_full_usage "\n\n" \
       "Perform actions on Debian packages (.debs)\n" \
     "\nOptions:" \
     "\n	-c	List contents of filesystem tree" \
     "\n	-e	Extract control files to [argument] directory" \
     "\n	-f	Display control field name starting with [argument]" \
     "\n	-x	Extract packages filesystem tree to directory" \
     "\n	-X	Verbose extract" \

#define dpkg_deb_example_usage \
       "$ dpkg-deb -X ./busybox_0.48-1_i386.deb /tmp\n"

#define du_trivial_usage \
       "[-aHLdclsx" USE_FEATURE_HUMAN_READABLE("hm") "k] [FILE]..."
#define du_full_usage "\n\n" \
       "Summarize disk space used for each FILE and/or directory.\n" \
       "Disk space is printed in units of " \
	USE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K("1024") \
	SKIP_FEATURE_DU_DEFAULT_BLOCKSIZE_1K("512") \
       " bytes.\n" \
     "\nOptions:" \
     "\n	-a	Show file sizes too" \
     "\n	-H	Follow symlinks on command line" \
     "\n	-L	Follow all symlinks" \
     "\n	-d N	Limit output to directories (and files with -a) of depth < N" \
     "\n	-c	Show grand total" \
     "\n	-l	Count sizes many times if hard linked" \
     "\n	-s	Display only a total for each argument" \
     "\n	-x	Skip directories on different filesystems" \
	USE_FEATURE_HUMAN_READABLE( \
     "\n	-h	Sizes in human readable format (e.g., 1K 243M 2G )" \
     "\n	-m	Sizes in megabytes" \
	) \
     "\n	-k	Sizes in kilobytes" \
			USE_FEATURE_DU_DEFAULT_BLOCKSIZE_1K(" (default)") \

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
#define dumpkmap_full_usage "\n\n" \
       "Print a binary keyboard translation table to standard output"
#define dumpkmap_example_usage \
       "$ dumpkmap > keymap\n"

#define dumpleases_trivial_usage \
       "[-r|-a] [-f LEASEFILE]"
#define dumpleases_full_usage "\n\n" \
       "Display DHCP leases granted by udhcpd\n" \
     "\nOptions:" \
	USE_GETOPT_LONG( \
     "\n	-f,--file=FILE	Leases file to load" \
     "\n	-r,--remaining	Interpret lease times as time remaining" \
     "\n	-a,--absolute	Interpret lease times as expire time" \
	) \
	SKIP_GETOPT_LONG( \
     "\n	-f FILE	Leases file to load" \
     "\n	-r	Interpret lease times as time remaining" \
     "\n	-a	Interpret lease times as expire time" \
	)

#define e2fsck_trivial_usage \
       "[-panyrcdfvstDFSV] [-b superblock] [-B blocksize] " \
       "[-I inode_buffer_blocks] [-P process_inode_size] " \
       "[-l|-L bad_blocks_file] [-C fd] [-j external_journal] " \
       "[-E extended-options] device"
#define e2fsck_full_usage "\n\n" \
       "Check ext2/ext3 file system\n" \
     "\nOptions:" \
     "\n	-p		Automatic repair (no questions)" \
     "\n	-n		Make no changes to the filesystem" \
     "\n	-y		Assume 'yes' to all questions" \
     "\n	-c		Check for bad blocks and add them to the badblock list" \
     "\n	-f		Force checking even if filesystem is marked clean" \
     "\n	-v		Verbose" \
     "\n	-b superblock	Use alternative superblock" \
     "\n	-B blocksize	Force blocksize when looking for superblock" \
     "\n	-j journal	Set location of the external journal" \
     "\n	-l file		Add to badblocks list" \
     "\n	-L file		Set badblocks list" \

#define echo_trivial_usage \
	USE_FEATURE_FANCY_ECHO("[-neE] ") "[ARG...]"
#define echo_full_usage "\n\n" \
       "Print the specified ARGs to stdout" \
	USE_FEATURE_FANCY_ECHO( "\n" \
     "\nOptions:" \
     "\n	-n	Suppress trailing newline" \
     "\n	-e	Interpret backslash-escaped characters (i.e., \\t=tab)" \
     "\n	-E	Disable interpretation of backslash-escaped characters" \
	)
#define echo_example_usage \
       "$ echo \"Erik is cool\"\n" \
       "Erik is cool\n" \
	USE_FEATURE_FANCY_ECHO("$ echo -e \"Erik\\nis\\ncool\"\n" \
       "Erik\n" \
       "is\n" \
       "cool\n" \
       "$ echo \"Erik\\nis\\ncool\"\n" \
       "Erik\\nis\\ncool\n")

#define eject_trivial_usage \
       "[-t] [-T] [DEVICE]"
#define eject_full_usage "\n\n" \
       "Eject specified DEVICE (or default /dev/cdrom)\n" \
     "\nOptions:" \
	USE_FEATURE_EJECT_SCSI( \
     "\n	-s	SCSI device" \
	) \
     "\n	-t	Close tray" \
     "\n	-T	Open/close tray (toggle)" \

#define ed_trivial_usage ""
#define ed_full_usage ""

#define env_trivial_usage \
       "[-iu] [-] [name=value]... [command]"
#define env_full_usage "\n\n" \
       "Print the current environment or run a program after setting\n" \
       "up the specified environment\n" \
     "\nOptions:" \
     "\n	-, -i	Start with an empty environment" \
     "\n	-u	Remove variable from the environment" \

#define ether_wake_trivial_usage \
       "[-b] [-i iface] [-p aa:bb:cc:dd[:ee:ff]] MAC"
#define ether_wake_full_usage "\n\n" \
       "Send a magic packet to wake up sleeping machines.\n" \
       "MAC must be a station address (00:11:22:33:44:55) or\n" \
       "a hostname with a known 'ethers' entry.\n" \
     "\nOptions:" \
     "\n	-b		Send wake-up packet to the broadcast address" \
     "\n	-i iface	Interface to use (default eth0)" \
     "\n	-p pass		Append four or six byte password PW to the packet" \

#define expand_trivial_usage \
       "[-i] [-t NUM] [FILE|-]"
#define expand_full_usage "\n\n" \
       "Convert tabs to spaces, writing to standard output.\n" \
     "\nOptions:" \
	USE_FEATURE_EXPAND_LONG_OPTIONS( \
     "\n	-i,--initial	Do not convert tabs after non blanks" \
     "\n	-t,--tabs=N	Tabstops every N chars" \
	) \
	SKIP_FEATURE_EXPAND_LONG_OPTIONS( \
     "\n	-i	Do not convert tabs after non blanks" \
     "\n	-t	Tabstops every N chars" \
	)

#define expr_trivial_usage \
       "EXPRESSION"
#define expr_full_usage "\n\n" \
       "Print the value of EXPRESSION to standard output.\n" \
       "\n" \
       "EXPRESSION may be:\n" \
       "	ARG1 | ARG2	ARG1 if it is neither null nor 0, otherwise ARG2\n" \
       "	ARG1 & ARG2	ARG1 if neither argument is null or 0, otherwise 0\n" \
       "	ARG1 < ARG2	1 if ARG1 is less than ARG2, else 0. Similarly:\n" \
       "	ARG1 <= ARG2\n" \
       "	ARG1 = ARG2\n" \
       "	ARG1 != ARG2\n" \
       "	ARG1 >= ARG2\n" \
       "	ARG1 > ARG2\n" \
       "	ARG1 + ARG2	Sum of ARG1 and ARG2. Similarly:\n" \
       "	ARG1 - ARG2\n" \
       "	ARG1 * ARG2\n" \
       "	ARG1 / ARG2\n" \
       "	ARG1 % ARG2\n" \
       "	STRING : REGEXP		Anchored pattern match of REGEXP in STRING\n" \
       "	match STRING REGEXP	Same as STRING : REGEXP\n" \
       "	substr STRING POS LENGTH Substring of STRING, POS counted from 1\n" \
       "	index STRING CHARS	Index in STRING where any CHARS is found, or 0\n" \
       "	length STRING		Length of STRING\n" \
       "	quote TOKEN		Interpret TOKEN as a string, even if\n" \
       "				it is a keyword like 'match' or an\n" \
       "				operator like '/'\n" \
       "	(EXPRESSION)		Value of EXPRESSION\n" \
       "\n" \
       "Beware that many operators need to be escaped or quoted for shells.\n" \
       "Comparisons are arithmetic if both ARGs are numbers, else\n" \
       "lexicographical. Pattern matches return the string matched between\n" \
       "\\( and \\) or null; if \\( and \\) are not used, they return the number\n" \
       "of characters matched or 0."

#define fakeidentd_trivial_usage \
       "[-fiw] [-b ADDR] [STRING]"
#define fakeidentd_full_usage "\n\n" \
       "Provide fake ident (auth) service\n" \
     "\nOptions:" \
     "\n	-f	Run in foreground" \
     "\n	-i	Inetd mode" \
     "\n	-w	Inetd 'wait' mode" \
     "\n	-b ADDR	Bind to specified address" \
     "\n	STRING	Ident answer string (default is 'nobody')" \

#define false_trivial_usage \
       ""
#define false_full_usage "\n\n" \
       "Return an exit code of FALSE (1)"

#define false_example_usage \
       "$ false\n" \
       "$ echo $?\n" \
       "1\n"

#define fbset_trivial_usage \
       "[options] [mode]"
#define fbset_full_usage "\n\n" \
       "Show and modify frame buffer settings"

#define fbset_example_usage \
       "$ fbset\n" \
       "mode \"1024x768-76\"\n" \
       "	# D: 78.653 MHz, H: 59.949 kHz, V: 75.694 Hz\n" \
       "	geometry 1024 768 1024 768 16\n" \
       "	timings 12714 128 32 16 4 128 4\n" \
       "	accel false\n" \
       "	rgba 5/11,6/5,5/0,0/0\n" \
       "endmode\n"

#define fdflush_trivial_usage \
       "DEVICE"
#define fdflush_full_usage "\n\n" \
       "Force floppy disk drive to detect disk change"

#define fdformat_trivial_usage \
       "[-n] DEVICE"
#define fdformat_full_usage "\n\n" \
       "Format floppy disk\n" \
     "\nOptions:" \
     "\n	-n	Don't verify after format" \

/* Looks like someone forgot to add this to config system */
#ifndef ENABLE_FEATURE_FDISK_BLKSIZE
# define ENABLE_FEATURE_FDISK_BLKSIZE 0
# define USE_FEATURE_FDISK_BLKSIZE(a)
#endif

#define fdisk_trivial_usage \
       "[-ul" USE_FEATURE_FDISK_BLKSIZE("s") "] " \
       "[-C CYLINDERS] [-H HEADS] [-S SECTORS] [-b SSZ] DISK"
#define fdisk_full_usage "\n\n" \
       "Change partition table\n" \
     "\nOptions:" \
     "\n	-u		Start and End are in sectors (instead of cylinders)" \
     "\n	-l		Show partition table for each DISK, then exit" \
	USE_FEATURE_FDISK_BLKSIZE( \
     "\n	-s		Show partition sizes in kb for each DISK, then exit" \
	) \
     "\n	-b 2048		(for certain MO disks) use 2048-byte sectors" \
     "\n	-C CYLINDERS	Set number of cylinders/heads/sectors" \
     "\n	-H HEADS\n" \
     "\n	-S SECTORS" \

#define fetchmail_trivial_usage \
       "[-w timeout] [-U user] -P password [-X] [-t] [-z] server[:port] maildir [prog]"
#define fetchmail_full_usage "\n\n" \
       "Fetch content of remote mailbox to local Maildir.\n" \
     "\nOptions:" \
     "\n	-w timeout	Set timeout on network operations" \
     "\n	-U username	Authenticate with specified username/password" \
     "\n	-P password" \
     "\n	-X		Use openssl connection helper for secured servers" \
     "\n	-t		Get only headers" \
     "\n	-z		Delete messages on server" \
     "\n	prog		Run prog <message_file> on message delivery" \

#define findfs_trivial_usage \
       "LABEL=label or UUID=uuid"
#define findfs_full_usage "\n\n" \
       "Find a filesystem device based on a label or UUID."
#define findfs_example_usage \
       "$ findfs LABEL=MyDevice"

#define find_trivial_usage \
       "[PATH...] [EXPRESSION]"
#define find_full_usage "\n\n" \
       "Search for files. The default PATH is the current directory,\n" \
       "default EXPRESSION is '-print'\n" \
     "\nEXPRESSION may consist of:" \
     "\n	-follow		Dereference symlinks" \
	USE_FEATURE_FIND_XDEV( \
     "\n	-xdev		Don't descend directories on other filesystems") \
	USE_FEATURE_FIND_MAXDEPTH( \
     "\n	-maxdepth N	Descend at most N levels. -maxdepth 0 applies" \
     "\n			tests/actions to command line arguments only") \
     "\n	-name PATTERN	File name (w/o directory name) matches PATTERN" \
     "\n	-iname PATTERN	Case insensitive -name" \
	USE_FEATURE_FIND_PATH( \
     "\n	-path PATTERN	Path matches PATTERN") \
	USE_FEATURE_FIND_REGEX( \
     "\n	-regex PATTERN	Path matches regex PATTERN") \
	USE_FEATURE_FIND_TYPE( \
     "\n	-type X		File type is X (X is one of: f,d,l,b,c,...)") \
	USE_FEATURE_FIND_PERM( \
     "\n	-perm NNN	Permissions match any of (+NNN), all of (-NNN)," \
     "\n			or exactly (NNN)") \
	USE_FEATURE_FIND_MTIME( \
     "\n	-mtime DAYS	Modified time is greater than (+N), less than (-N)," \
     "\n			or exactly (N) days") \
	USE_FEATURE_FIND_MMIN( \
     "\n	-mmin MINS	Modified time is greater than (+N), less than (-N)," \
     "\n			or exactly (N) minutes") \
	USE_FEATURE_FIND_NEWER( \
     "\n	-newer FILE	Modified time is more recent than FILE's") \
	USE_FEATURE_FIND_INUM( \
     "\n	-inum N		File has inode number N") \
	USE_FEATURE_FIND_USER( \
     "\n	-user NAME	File is owned by user NAME (numeric user ID allowed)") \
	USE_FEATURE_FIND_GROUP( \
     "\n	-group NAME	File belongs to group NAME (numeric group ID allowed)") \
	USE_FEATURE_FIND_DEPTH( \
     "\n	-depth		Process directory name after traversing it") \
	USE_FEATURE_FIND_SIZE( \
     "\n	-size N[bck]	File size is N (c:bytes,k:kbytes,b:512 bytes(def.))." \
     "\n			+/-N: file size is bigger/smaller than N") \
     "\n	-print		Print (default and assumed)" \
	USE_FEATURE_FIND_PRINT0( \
     "\n	-print0		Delimit output with null characters rather than" \
     "\n			newlines") \
	USE_FEATURE_FIND_CONTEXT ( \
     "\n	-context	File has specified security context") \
	USE_FEATURE_FIND_EXEC( \
     "\n	-exec CMD ARG ;	Execute CMD with all instances of {} replaced by the" \
     "\n			matching files") \
	USE_FEATURE_FIND_PRUNE( \
     "\n	-prune		Stop traversing current subtree") \
	USE_FEATURE_FIND_DELETE( \
     "\n	-delete		Delete files, turns on -depth option") \
	USE_FEATURE_FIND_PAREN( \
     "\n	(EXPR)		Group an expression") \

#define find_example_usage \
       "$ find / -name passwd\n" \
       "/etc/passwd\n"

#define fold_trivial_usage \
       "[-bs] [-w WIDTH] [FILE]"
#define fold_full_usage "\n\n" \
       "Wrap input lines in each FILE (standard input by default), writing to\n" \
       "standard output\n" \
     "\nOptions:" \
     "\n	-b	Count bytes rather than columns" \
     "\n	-s	Break at spaces" \
     "\n	-w	Use WIDTH columns instead of 80" \

#define free_trivial_usage \
       ""
#define free_full_usage "\n\n" \
       "Display the amount of free and used system memory"
#define free_example_usage \
       "$ free\n" \
       "              total         used         free       shared      buffers\n" \
       "  Mem:       257628       248724         8904        59644        93124\n" \
       " Swap:       128516         8404       120112\n" \
       "Total:       386144       257128       129016\n" \

#define freeramdisk_trivial_usage \
       "DEVICE"
#define freeramdisk_full_usage "\n\n" \
       "Free all memory used by the specified ramdisk"
#define freeramdisk_example_usage \
       "$ freeramdisk /dev/ram2\n"

#define fsck_trivial_usage \
       "[-ANPRTV] [-C fd] [-t fstype] [fs-options] [filesys...]"
#define fsck_full_usage "\n\n" \
       "Check and repair filesystems\n" \
     "\nOptions:" \
     "\n	-A	Walk /etc/fstab and check all filesystems" \
     "\n	-N	Don't execute, just show what would be done" \
     "\n	-P	With -A, check filesystems in parallel" \
     "\n	-R	With -A, skip the root filesystem" \
     "\n	-T	Don't show title on startup" \
     "\n	-V	Verbose" \
     "\n	-C n	Write status information to specified filedescriptor" \
     "\n	-t type	List of filesystem types to check" \

#define fsck_minix_trivial_usage \
       "[-larvsmf] /dev/name"
#define fsck_minix_full_usage "\n\n" \
       "Check MINIX filesystem\n" \
     "\nOptions:" \
     "\n	-l	List all filenames" \
     "\n	-r	Perform interactive repairs" \
     "\n	-a	Perform automatic repairs" \
     "\n	-v	Verbose" \
     "\n	-s	Output super-block information" \
     "\n	-m	Show \"mode not cleared\" warnings" \
     "\n	-f	Force file system check" \

#define ftpget_trivial_usage \
       "[options] remote-host local-file remote-file"
#define ftpget_full_usage "\n\n" \
       "Retrieve a remote file via FTP\n" \
     "\nOptions:" \
	USE_GETOPT_LONG( \
     "\n	-c,--continue	Continue previous transfer" \
     "\n	-v,--verbose	Verbose" \
     "\n	-u,--username	Username" \
     "\n	-p,--password	Password" \
     "\n	-P,--port	Port number" \
	) \
	SKIP_GETOPT_LONG( \
     "\n	-c	Continue previous transfer" \
     "\n	-v	Verbose" \
     "\n	-u	Username" \
     "\n	-p	Password" \
     "\n	-P	Port number" \
	)

#define ftpput_trivial_usage \
       "[options] remote-host remote-file local-file"
#define ftpput_full_usage "\n\n" \
       "Store a local file on a remote machine via FTP\n" \
     "\nOptions:" \
	USE_GETOPT_LONG( \
     "\n	-v,--verbose	Verbose" \
     "\n	-u,--username	Username" \
     "\n	-p,--password	Password" \
     "\n	-P,--port	Port number" \
	) \
	SKIP_GETOPT_LONG( \
     "\n	-v	Verbose" \
     "\n	-u	Username" \
     "\n	-p	Password" \
     "\n	-P	Port number" \
	)

#define fuser_trivial_usage \
       "[options] FILE or PORT/PROTO"
#define fuser_full_usage "\n\n" \
       "Find processes which use FILEs or PORTs\n" \
     "\nOptions:" \
     "\n	-m	Find processes which use same fs as FILEs" \
     "\n	-4	Search only IPv4 space" \
     "\n	-6	Search only IPv6 space" \
     "\n	-s	Silent: just exit with 0 if any processes are found" \
     "\n	-k	Kill found processes (otherwise display PIDs)" \
     "\n	-SIGNAL	Signal to send (default: TERM)" \

#define getenforce_trivial_usage NOUSAGE_STR
#define getenforce_full_usage ""

#define getopt_trivial_usage \
       "[OPTIONS]..."
#define getopt_full_usage "\n\n" \
       "Parse command options\n" \
	USE_GETOPT_LONG( \
     "\n	-a,--alternative		Allow long options starting with single -" \
     "\n	-l,--longoptions=longopts	Long options to be recognized" \
     "\n	-n,--name=progname		The name under which errors are reported" \
     "\n	-o,--options=optstring		Short options to be recognized" \
     "\n	-q,--quiet			Disable error reporting by getopt(3)" \
     "\n	-Q,--quiet-output		No normal output" \
     "\n	-s,--shell=shell		Set shell quoting conventions" \
     "\n	-T,--test			Test for getopt(1) version" \
     "\n	-u,--unquoted			Don't quote the output" \
	) \
	SKIP_GETOPT_LONG( \
     "\n	-a		Allow long options starting with single -" \
     "\n	-l longopts	Long options to be recognized" \
     "\n	-n progname	The name under which errors are reported" \
     "\n	-o optstring	Short options to be recognized" \
     "\n	-q		Disable error reporting by getopt(3)" \
     "\n	-Q		No normal output" \
     "\n	-s shell	Set shell quoting conventions" \
     "\n	-T		Test for getopt(1) version" \
     "\n	-u		Don't quote the output" \
	)
#define getopt_example_usage \
       "$ cat getopt.test\n" \
       "#!/bin/sh\n" \
       "GETOPT=`getopt -o ab:c:: --long a-long,b-long:,c-long:: \\\n" \
       "       -n 'example.busybox' -- \"$@\"`\n" \
       "if [ $? != 0 ]; then  exit 1; fi\n" \
       "eval set -- \"$GETOPT\"\n" \
       "while true; do\n" \
       " case $1 in\n" \
       "   -a|--a-long) echo \"Option a\"; shift;;\n" \
       "   -b|--b-long) echo \"Option b, argument '$2'\"; shift 2;;\n" \
       "   -c|--c-long)\n" \
       "     case \"$2\" in\n" \
       "       \"\") echo \"Option c, no argument\"; shift 2;;\n" \
       "       *)  echo \"Option c, argument '$2'\"; shift 2;;\n" \
       "     esac;;\n" \
       "   --) shift; break;;\n" \
       "   *) echo \"Internal error!\"; exit 1;;\n" \
       " esac\n" \
       "done\n"

#define getsebool_trivial_usage \
       "-a or getsebool boolean..."
#define getsebool_full_usage "\n\n" \
       "	-a	Show all SELinux booleans"

#define getty_trivial_usage \
       "[OPTIONS] BAUD_RATE TTY [TERMTYPE]"
#define getty_full_usage "\n\n" \
       "Open a tty, prompt for a login name, then invoke /bin/login\n" \
     "\nOptions:" \
     "\n	-h		Enable hardware (RTS/CTS) flow control" \
     "\n	-i		Do not display /etc/issue before running login" \
     "\n	-L		Local line, do not do carrier detect" \
     "\n	-m		Get baud rate from modem's CONNECT status message" \
     "\n	-w		Wait for a CR or LF before sending /etc/issue" \
     "\n	-n		Do not prompt the user for a login name" \
     "\n	-f issue_file	Display issue_file instead of /etc/issue" \
     "\n	-l login_app	Invoke login_app instead of /bin/login" \
     "\n	-t timeout	Terminate after timeout if no username is read" \
     "\n	-I initstring	Init string to send before anything else" \
     "\n	-H login_host	Log login_host into the utmp file as the hostname" \

#define grep_trivial_usage \
       "[-HhrilLnqvso" \
	USE_DESKTOP("w") \
       "eF" \
	USE_FEATURE_GREP_EGREP_ALIAS("E") \
	USE_FEATURE_GREP_CONTEXT("ABC") \
       "] PATTERN [FILEs...]"
#define grep_full_usage "\n\n" \
       "Search for PATTERN in each FILE or standard input\n" \
     "\nOptions:" \
     "\n	-H	Prefix output lines with filename where match was found" \
     "\n	-h	Suppress the prefixing filename on output" \
     "\n	-r	Recurse subdirectories" \
     "\n	-i	Ignore case distinctions" \
     "\n	-l	List names of files that match" \
     "\n	-L	List names of files that do not match" \
     "\n	-n	Print line number with output lines" \
     "\n	-q	Quiet. Return 0 if PATTERN is found, 1 otherwise" \
     "\n	-v	Select non-matching lines" \
     "\n	-s	Suppress file open/read error messages" \
     "\n	-c	Only print count of matching lines" \
     "\n	-o	Show only the part of a line that matches PATTERN" \
     "\n	-m MAX	Match up to MAX times per file" \
	USE_DESKTOP( \
     "\n	-w	Match whole words only") \
     "\n	-F	PATTERN is a set of newline-separated strings" \
	USE_FEATURE_GREP_EGREP_ALIAS( \
     "\n	-E	PATTERN is an extended regular expression") \
     "\n	-e PTRN	Pattern to match" \
     "\n	-f FILE	Read pattern from file" \
	USE_FEATURE_GREP_CONTEXT( \
     "\n	-A	Print NUM lines of trailing context" \
     "\n	-B	Print NUM lines of leading context" \
     "\n	-C	Print NUM lines of output context") \

#define grep_example_usage \
       "$ grep root /etc/passwd\n" \
       "root:x:0:0:root:/root:/bin/bash\n" \
       "$ grep ^[rR]oo. /etc/passwd\n" \
       "root:x:0:0:root:/root:/bin/bash\n"

#define egrep_trivial_usage NOUSAGE_STR
#define egrep_full_usage ""

#define fgrep_trivial_usage NOUSAGE_STR
#define fgrep_full_usage ""

#define gunzip_trivial_usage \
       "[OPTION]... [FILE]..."
#define gunzip_full_usage "\n\n" \
       "Uncompress FILEs (or standard input)\n" \
     "\nOptions:" \
     "\n	-c	Write to standard output" \
     "\n	-f	Force" \
     "\n	-t	Test file integrity" \

#define gunzip_example_usage \
       "$ ls -la /tmp/BusyBox*\n" \
       "-rw-rw-r--    1 andersen andersen   557009 Apr 11 10:55 /tmp/BusyBox-0.43.tar.gz\n" \
       "$ gunzip /tmp/BusyBox-0.43.tar.gz\n" \
       "$ ls -la /tmp/BusyBox*\n" \
       "-rw-rw-r--    1 andersen andersen  1761280 Apr 14 17:47 /tmp/BusyBox-0.43.tar\n"

#define gzip_trivial_usage \
       "[OPTION]... [FILE]..."
#define gzip_full_usage "\n\n" \
       "Compress FILEs (or standard input)\n" \
     "\nOptions:" \
     "\n	-c	Write to standard output" \
     "\n	-d	Decompress" \
     "\n	-f	Force" \

#define gzip_example_usage \
       "$ ls -la /tmp/busybox*\n" \
       "-rw-rw-r--    1 andersen andersen  1761280 Apr 14 17:47 /tmp/busybox.tar\n" \
       "$ gzip /tmp/busybox.tar\n" \
       "$ ls -la /tmp/busybox*\n" \
       "-rw-rw-r--    1 andersen andersen   554058 Apr 14 17:49 /tmp/busybox.tar.gz\n"

#define halt_trivial_usage \
       "[-d delay] [-n] [-f]"
#define halt_full_usage "\n\n" \
       "Halt the system\n" \
     "\nOptions:" \
     "\n	-d	Delay interval for halting" \
     "\n	-n	No call to sync()" \
     "\n	-f	Force halt (don't go through init)" \
	USE_FEATURE_WTMP( \
     "\n	-w	Only write a wtmp record" \
	)

#define hdparm_trivial_usage \
       "[options] [device] .."
#define hdparm_full_usage "\n\n" \
       "Options:" \
     "\n	-a	Get/set fs readahead" \
     "\n	-A	Set drive read-lookahead flag (0/1)" \
     "\n	-b	Get/set bus state (0 == off, 1 == on, 2 == tristate)" \
     "\n	-B	Set Advanced Power Management setting (1-255)" \
     "\n	-c	Get/set IDE 32-bit IO setting" \
     "\n	-C	Check IDE power mode status" \
	USE_FEATURE_HDPARM_HDIO_GETSET_DMA( \
     "\n	-d	Get/set using_dma flag") \
     "\n	-D	Enable/disable drive defect-mgmt" \
     "\n	-f	Flush buffer cache for device on exit" \
     "\n	-g	Display drive geometry" \
     "\n	-h	Display terse usage information" \
	USE_FEATURE_HDPARM_GET_IDENTITY( \
     "\n	-i	Display drive identification") \
	USE_FEATURE_HDPARM_GET_IDENTITY( \
     "\n	-I	Detailed/current information directly from drive") \
     "\n	-k	Get/set keep_settings_over_reset flag (0/1)" \
     "\n	-K	Set drive keep_features_over_reset flag (0/1)" \
     "\n	-L	Set drive doorlock (0/1) (removable harddisks only)" \
     "\n	-m	Get/set multiple sector count" \
     "\n	-n	Get/set ignore-write-errors flag (0/1)" \
     "\n	-p	Set PIO mode on IDE interface chipset (0,1,2,3,4,...)" \
     "\n	-P	Set drive prefetch count" \
/*   "\n	-q	Change next setting quietly" - not supported ib bbox */ \
     "\n	-Q	Get/set DMA tagged-queuing depth (if supported)" \
     "\n	-r	Get/set readonly flag (DANGEROUS to set)" \
	USE_FEATURE_HDPARM_HDIO_SCAN_HWIF( \
     "\n	-R	Register an IDE interface (DANGEROUS)") \
     "\n	-S	Set standby (spindown) timeout" \
     "\n	-t	Perform device read timings" \
     "\n	-T	Perform cache read timings" \
     "\n	-u	Get/set unmaskirq flag (0/1)" \
	USE_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF( \
     "\n	-U	Un-register an IDE interface (DANGEROUS)") \
     "\n	-v	Defaults; same as -mcudkrag for IDE drives" \
     "\n	-V	Display program version and exit immediately" \
	USE_FEATURE_HDPARM_HDIO_DRIVE_RESET( \
     "\n	-w	Perform device reset (DANGEROUS)") \
     "\n	-W	Set drive write-caching flag (0/1) (DANGEROUS)" \
	USE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF( \
     "\n	-x	Tristate device for hotswap (0/1) (DANGEROUS)") \
     "\n	-X	Set IDE xfer mode (DANGEROUS)" \
     "\n	-y	Put IDE drive in standby mode" \
     "\n	-Y	Put IDE drive to sleep" \
     "\n	-Z	Disable Seagate auto-powersaving mode" \
     "\n	-z	Re-read partition table" \

#define head_trivial_usage \
       "[OPTION]... [FILE]..."
#define head_full_usage "\n\n" \
       "Print first 10 lines of each FILE to standard output.\n" \
       "With more than one FILE, precede each with a header giving the\n" \
       "file name. With no FILE, or when FILE is -, read standard input.\n" \
     "\nOptions:" \
     "\n	-n NUM	Print first NUM lines instead of first 10" \
	USE_FEATURE_FANCY_HEAD( \
     "\n	-c NUM	Output the first NUM bytes" \
     "\n	-q	Never output headers giving file names" \
     "\n	-v	Always output headers giving file names") \

#define head_example_usage \
       "$ head -n 2 /etc/passwd\n" \
       "root:x:0:0:root:/root:/bin/bash\n" \
       "daemon:x:1:1:daemon:/usr/sbin:/bin/sh\n"

#define hexdump_trivial_usage \
       "[-bcCdefnosvx" USE_FEATURE_HEXDUMP_REVERSE("R") "] FILE..."
#define hexdump_full_usage "\n\n" \
       "Display file(s) or standard input in a user specified format\n" \
     "\nOptions:" \
     "\n	-b		One-byte octal display" \
     "\n	-c		One-byte character display" \
     "\n	-C		Canonical hex+ASCII, 16 bytes per line" \
     "\n	-d		Two-byte decimal display" \
     "\n	-e FORMAT STRING" \
     "\n	-f FORMAT FILE" \
     "\n	-n LENGTH	Interpret only LENGTH bytes of input" \
     "\n	-o		Two-byte octal display" \
     "\n	-s OFFSET	Skip OFFSET bytes" \
     "\n	-v		Display all input data" \
     "\n	-x		Two-byte hexadecimal display" \
	USE_FEATURE_HEXDUMP_REVERSE( \
     "\n	-R		Reverse of 'hexdump -Cv'") \

#define hd_trivial_usage \
       "FILE..."
#define hd_full_usage "\n\n" \
       "hd is an alias for hexdump -C"

#define hostid_trivial_usage \
       ""
#define hostid_full_usage "\n\n" \
       "Print out a unique 32-bit identifier for the machine"

#define hostname_trivial_usage \
       "[OPTION] [hostname | -F FILE]"
#define hostname_full_usage "\n\n" \
       "Get or set hostname or DNS domain name\n" \
     "\nOptions:" \
     "\n	-s	Short" \
     "\n	-i	Addresses for the hostname" \
     "\n	-d	DNS domain name" \
     "\n	-f	Fully qualified domain name" \
     "\n	-F FILE	Use the contents of FILE to specify the hostname" \

#define hostname_example_usage \
       "$ hostname\n" \
       "sage\n"

#define httpd_trivial_usage \
       "[-c conffile]" \
       " [-p [ip:]port]" \
       " [-i] [-f] [-v[v]]" \
	USE_FEATURE_HTTPD_SETUID(" [-u user[:grp]]") \
	USE_FEATURE_HTTPD_BASIC_AUTH(" [-r realm]") \
	USE_FEATURE_HTTPD_AUTH_MD5(" [-m pass]") \
       " [-h home]" \
       " [-d/-e string]"
#define httpd_full_usage "\n\n" \
       "Listen for incoming HTTP requests\n" \
     "\nOptions:" \
     "\n	-c FILE		Configuration file (default httpd.conf)" \
     "\n	-p [IP:]PORT	Bind to ip:port (default *:80)" \
     "\n	-i		Inetd mode" \
     "\n	-f		Do not daemonize" \
     "\n	-v[v]		Verbose" \
	USE_FEATURE_HTTPD_SETUID( \
     "\n	-u USER[:GRP]	Set uid/gid after binding to port") \
	USE_FEATURE_HTTPD_BASIC_AUTH( \
     "\n	-r REALM	Authentication Realm for Basic Authentication") \
	USE_FEATURE_HTTPD_AUTH_MD5( \
     "\n	-m PASS		Crypt PASS with md5 algorithm") \
     "\n	-h HOME		Home directory (default .)" \
     "\n	-e STRING	HTML encode STRING" \
     "\n	-d STRING	URL decode STRING" \

#define hwclock_trivial_usage \
	USE_GETOPT_LONG( \
       "[-r|--show] [-s|--hctosys] [-w|--systohc]" \
       " [-l|--localtime] [-u|--utc]" \
       " [-f FILE]" \
	) \
	SKIP_GETOPT_LONG( \
       "[-r] [-s] [-w] [-l] [-u] [-f FILE]" \
	)
#define hwclock_full_usage "\n\n" \
       "Query and set hardware clock (RTC)\n" \
     "\nOptions:" \
     "\n	-r	Show hardware clock time" \
     "\n	-s	Set system time from hardware clock" \
     "\n	-w	Set hardware clock to system time" \
     "\n	-u	Hardware clock is in UTC" \
     "\n	-l	Hardware clock is in local time" \
     "\n	-f FILE	Use specified device (e.g. /dev/rtc2)" \

#define id_trivial_usage \
       "[OPTIONS]... [USER]"
#define id_full_usage "\n\n" \
       "Print information about USER or the current user\n" \
     "\nOptions:" \
	USE_SELINUX( \
     "\n	-Z	Print the security context" \
	) \
     "\n	-g	Print group ID" \
     "\n	-u	Print user ID" \
     "\n	-n	Print name instead of a number" \
     "\n	-r	Print real user ID instead of effective ID" \

#define id_example_usage \
       "$ id\n" \
       "uid=1000(andersen) gid=1000(andersen)\n"

#define ifconfig_trivial_usage \
	USE_FEATURE_IFCONFIG_STATUS("[-a]") " interface [address]"
#define ifconfig_full_usage "\n\n" \
       "Configure a network interface\n" \
     "\nOptions:" \
     "\n" \
	USE_FEATURE_IPV6( \
       "	[add ADDRESS[/PREFIXLEN]]\n") \
	USE_FEATURE_IPV6( \
       "	[del ADDRESS[/PREFIXLEN]]\n") \
       "	[[-]broadcast [ADDRESS]] [[-]pointopoint [ADDRESS]]\n" \
       "	[netmask ADDRESS] [dstaddr ADDRESS]\n" \
	USE_FEATURE_IFCONFIG_SLIP( \
       "	[outfill NN] [keepalive NN]\n") \
       "	" USE_FEATURE_IFCONFIG_HW("[hw ether" USE_FEATURE_HWIB("|infiniband")" ADDRESS] ") "[metric NN] [mtu NN]\n" \
       "	[[-]trailers] [[-]arp] [[-]allmulti]\n" \
       "	[multicast] [[-]promisc] [txqueuelen NN] [[-]dynamic]\n" \
	USE_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ( \
       "	[mem_start NN] [io_addr NN] [irq NN]\n") \
       "	[up|down] ..."

#define ifenslave_trivial_usage \
       "[-cdf] master-iface <slave-iface...>"
#define ifenslave_full_usage "\n\n" \
       "Configure network interfaces for parallel routing\n" \
     "\nOptions:" \
     "\n	-c, --change-active	Change active slave" \
     "\n	-d, --detach		Remove slave interface from bonding device" \
     "\n	-f, --force		Force, even if interface is not Ethernet" \
/*   "\n	-r, --receive-slave	Create a receive-only slave" */

#define ifenslave_example_usage \
       "To create a bond device, simply follow these three steps :\n" \
       "- ensure that the required drivers are properly loaded :\n" \
       "  # modprobe bonding ; modprobe <3c59x|eepro100|pcnet32|tulip|...>\n" \
       "- assign an IP address to the bond device :\n" \
       "  # ifconfig bond0 <addr> netmask <mask> broadcast <bcast>\n" \
       "- attach all the interfaces you need to the bond device :\n" \
       "  # ifenslave bond0 eth0 eth1 eth2\n" \
       "  If bond0 didn't have a MAC address, it will take eth0's. Then, all\n" \
       "  interfaces attached AFTER this assignment will get the same MAC addr.\n\n" \
       "  To detach a dead interface without setting the bond device down :\n" \
       "   # ifenslave -d bond0 eth1\n\n" \
       "  To set the bond device down and automatically release all the slaves :\n" \
       "   # ifconfig bond0 down\n\n" \
       "  To change active slave :\n" \
       "   # ifenslave -c bond0 eth0\n" \

#define ifup_trivial_usage \
       "[-ain"USE_FEATURE_IFUPDOWN_MAPPING("m")"vf] ifaces..."
#define ifup_full_usage "\n\n" \
       "Options:" \
     "\n	-a	De/configure all interfaces automatically" \
     "\n	-i FILE	Use FILE for interface definitions" \
     "\n	-n	Print out what would happen, but don't do it" \
	USE_FEATURE_IFUPDOWN_MAPPING( \
     "\n		(note: doesn't disable mappings)" \
     "\n	-m	Don't run any mappings" \
	) \
     "\n	-v	Print out what would happen before doing it" \
     "\n	-f	Force de/configuration" \

#define ifdown_trivial_usage \
       "[-ain"USE_FEATURE_IFUPDOWN_MAPPING("m")"vf] ifaces..."
#define ifdown_full_usage "\n\n" \
       "Options:" \
     "\n	-a	De/configure all interfaces automatically" \
     "\n	-i FILE	Use FILE for interface definitions" \
     "\n	-n	Print out what would happen, but don't do it" \
	USE_FEATURE_IFUPDOWN_MAPPING( \
     "\n		(note: doesn't disable mappings)" \
     "\n	-m	Don't run any mappings" \
	) \
     "\n	-v	Print out what would happen before doing it" \
     "\n	-f	Force de/configuration" \

#define inetd_trivial_usage \
       "[-fe] [-q N] [-R N] [CONFFILE]"
#define inetd_full_usage "\n\n" \
       "Listen for network connections and launch programs\n" \
     "\nOptions:" \
     "\n	-f	Run in foreground" \
     "\n	-e	Log to stderr" \
     "\n	-q N    Socket listen queue (default: 128)" \
     "\n	-R N	Pause services after N connects/min" \
     "\n		(default: 0 - disabled)" \

#define init_trivial_usage \
       ""
#define init_full_usage "\n\n" \
       "Init is the parent of all processes"

#define init_notes_usage \
"This version of init is designed to be run only by the kernel.\n" \
"\n" \
"BusyBox init doesn't support multiple runlevels. The runlevels field of\n" \
"the /etc/inittab file is completely ignored by BusyBox init. If you want\n" \
"runlevels, use sysvinit.\n" \
"\n" \
"BusyBox init works just fine without an inittab. If no inittab is found,\n" \
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
"		the specified process to run on. The contents of this field are\n" \
"		appended to \"/dev/\" and used as-is. There is no need for this field to\n" \
"		be unique, although if it isn't you may have strange results. If this\n" \
"		field is left blank, the controlling tty is set to the console. Also\n" \
"		note that if BusyBox detects that a serial console is in use, then only\n" \
"		entries whose controlling tty is either the serial console or /dev/null\n" \
"		will be run. BusyBox init does nothing with utmp. We don't need no\n" \
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
"			'sysinit' is the first item run on boot. init waits until all\n" \
"			sysinit actions are completed before continuing. Following the\n" \
"			completion of all sysinit actions, all 'wait' actions are run.\n" \
"			'wait' actions, like 'sysinit' actions, cause init to wait until\n" \
"			the specified task completes. 'once' actions are asynchronous,\n" \
"			therefore, init does not wait for them to complete. 'restart' is\n" \
"			the action taken to restart the init process. By default this should\n" \
"			simply run /sbin/init, but can be a script which runs pivot_root or it\n" \
"			can do all sorts of other interesting things. The 'ctrlaltdel' init\n" \
"			actions are run when the system detects that someone on the system\n" \
"			console has pressed the CTRL-ALT-DEL key combination. Typically one\n" \
"			wants to run 'reboot' at this point to cause the system to reboot.\n" \
"			Finally the 'shutdown' action specifies the actions to taken when\n" \
"			init is told to reboot. Unmounting filesystems and disabling swap\n" \
"			is a very good here.\n" \
"\n" \
"		Run repeatedly actions:\n" \
"\n" \
"			'respawn' actions are run after the 'once' actions. When a process\n" \
"			started with a 'respawn' action exits, init automatically restarts\n" \
"			it. Unlike sysvinit, BusyBox init does not stop processes from\n" \
"			respawning out of control. The 'askfirst' actions acts just like\n" \
"			respawn, except that before running the specified process it\n" \
"			displays the line \"Please press Enter to activate this console.\"\n" \
"			and then waits for the user to press enter before starting the\n" \
"			specified process.\n" \
"\n" \
"		Unrecognized actions (like initdefault) will cause init to emit an\n" \
"		error message, and then go along with its business. All actions are\n" \
"		run in the order they appear in /etc/inittab.\n" \
"\n" \
"	<process>:\n" \
"\n" \
"		Specifies the process to be executed and its command line.\n" \
"\n" \
"Example /etc/inittab file:\n" \
"\n" \
"	# This is run first except when booting in single-user mode\n" \
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
"	# Example how to put a getty on a modem line\n" \
"	#::respawn:/sbin/getty 57600 ttyS2\n" \
"	\n" \
"	# Stuff to do when restarting the init process\n" \
"	::restart:/sbin/init\n" \
"	\n" \
"	# Stuff to do before rebooting\n" \
"	::ctrlaltdel:/sbin/reboot\n" \
"	::shutdown:/bin/umount -a -r\n" \
"	::shutdown:/sbin/swapoff -a\n"

#define inotifyd_trivial_usage \
	"/user/space/agent dir/or/file/being/watched[:mask] ..."
#define inotifyd_full_usage "\n\n" \
       "Spawn userspace agent on filesystem changes." \
     "\nWhen a filesystem event matching the mask occurs" \
     "\non specified file/directory an userspace agent is spawned" \
     "\nwith the parameters:" \
     "\n1. actual event(s)" \
     "\n2. file/directory name" \
     "\n3. name of subfile (if any), in case of watching a directory" \
     "\n" \
     "\n	a	File is accessed" \
     "\n	c	File is modified" \
     "\n	e	Metadata changed" \
     "\n	w	Writtable file is closed" \
     "\n	0	Unwrittable file is closed" \
     "\n	r	File is opened" \
     "\n	m	File is moved from X" \
     "\n	y	File is moved to Y" \
     "\n	n	Subfile is created" \
     "\n	d	Subfile is deleted" \
     "\n	D	Self is deleted" \
     "\n	M	Self is moved" \

#define insmod_trivial_usage \
	USE_FEATURE_2_4_MODULES("[OPTION]... ") "MODULE [symbol=value]..."
#define insmod_full_usage "\n\n" \
       "Load the specified kernel modules into the kernel" \
	USE_FEATURE_2_4_MODULES( "\n" \
     "\nOptions:" \
     "\n	-f	Force module to load into the wrong kernel version" \
     "\n	-k	Make module autoclean-able" \
     "\n	-v	Verbose" \
     "\n	-q	Quiet" \
     "\n	-L	Lock to prevent simultaneous loads of a module" \
	USE_FEATURE_INSMOD_LOAD_MAP( \
     "\n	-m	Output load map to stdout" \
	) \
     "\n	-o NAME	Set internal module name to NAME" \
     "\n	-x	Do not export externs" \
	)

#define install_trivial_usage \
       "[-cgmops] [sources] dest|directory"
#define install_full_usage "\n\n" \
       "Copy files and set attributes\n" \
     "\nOptions:" \
     "\n	-c	Copy the file, default" \
     "\n	-d	Create directories" \
     "\n	-g	Set group ownership" \
     "\n	-m	Set permissions" \
     "\n	-o	Set ownership" \
     "\n	-p	Preserve date" \
     "\n	-s	Strip symbol tables" \
	USE_SELINUX( \
     "\n	-Z	Set security context of copy" \
	)

/* would need to make the " | " optional depending on more than one selected: */
#define ip_trivial_usage \
       "[OPTIONS] {" \
	USE_FEATURE_IP_ADDRESS("address | ") \
	USE_FEATURE_IP_ROUTE("route | ") \
	USE_FEATURE_IP_LINK("link | ") \
	USE_FEATURE_IP_TUNNEL("tunnel | ") \
	USE_FEATURE_IP_RULE("rule") \
       "} {COMMAND}"
#define ip_full_usage "\n\n" \
       "ip [OPTIONS] OBJECT {COMMAND}\n" \
       "where OBJECT := {" \
	USE_FEATURE_IP_ADDRESS("address | ") \
	USE_FEATURE_IP_ROUTE("route | ") \
	USE_FEATURE_IP_LINK("link | ") \
	USE_FEATURE_IP_TUNNEL("tunnel | ") \
	USE_FEATURE_IP_RULE("rule") \
       "}\n" \
       "OPTIONS := { -f[amily] { inet | inet6 | link } | -o[neline] }" \

#define ipaddr_trivial_usage \
       "{ {add|del} IFADDR dev STRING | {show|flush}\n" \
       "		[dev STRING] [to PREFIX] }"
#define ipaddr_full_usage "\n\n" \
       "ipaddr {add|delete} IFADDR dev STRING\n" \
       "ipaddr {show|flush} [dev STRING] [scope SCOPE-ID]\n" \
       "	[to PREFIX] [label PATTERN]\n" \
       "	IFADDR := PREFIX | ADDR peer PREFIX\n" \
       "	[broadcast ADDR] [anycast ADDR]\n" \
       "	[label STRING] [scope SCOPE-ID]\n" \
       "	SCOPE-ID := [host | link | global | NUMBER]" \

#define ipcalc_trivial_usage \
       "[OPTION]... ADDRESS[[/]NETMASK] [NETMASK]"
#define ipcalc_full_usage "\n\n" \
       "Calculate IP network settings from a IP address\n" \
     "\nOptions:" \
	USE_FEATURE_IPCALC_LONG_OPTIONS( \
     "\n	-b,--broadcast	Display calculated broadcast address" \
     "\n	-n,--network	Display calculated network address" \
     "\n	-m,--netmask	Display default netmask for IP" \
	USE_FEATURE_IPCALC_FANCY( \
     "\n	-p,--prefix	Display the prefix for IP/NETMASK" \
     "\n	-h,--hostname	Display first resolved host name" \
     "\n	-s,--silent	Don't ever display error messages" \
	) \
	) \
	SKIP_FEATURE_IPCALC_LONG_OPTIONS( \
     "\n	-b	Display calculated broadcast address" \
     "\n	-n	Display calculated network address" \
     "\n	-m	Display default netmask for IP" \
	USE_FEATURE_IPCALC_FANCY( \
     "\n	-p	Display the prefix for IP/NETMASK" \
     "\n	-h	Display first resolved host name" \
     "\n	-s	Don't ever display error messages" \
	) \
	)

#define ipcrm_trivial_usage \
       "[-MQS key] [-mqs id]"
#define ipcrm_full_usage "\n\n" \
       "Upper-case options MQS remove an object by shmkey value.\n" \
       "Lower-case options remove an object by shmid value.\n" \
     "\nOptions:" \
     "\n	-mM	Remove memory segment after last detach" \
     "\n	-qQ	Remove message queue" \
     "\n	-sS	Remove semaphore" \

#define ipcs_trivial_usage \
       "[[-smq] -i shmid] | [[-asmq] [-tcplu]]"
#define ipcs_full_usage "\n\n" \
       "	-i	Show specific resource" \
     "\nResource specification:" \
     "\n	-m	Shared memory segments" \
     "\n	-q	Message queues" \
     "\n	-s	Semaphore arrays" \
     "\n	-a	All (default)" \
     "\nOutput format:" \
     "\n	-t	Time" \
     "\n	-c	Creator" \
     "\n	-p	Pid" \
     "\n	-l	Limits" \
     "\n	-u	Summary" \

#define iplink_trivial_usage \
       "{ set DEVICE { up | down | arp { on | off } | show [DEVICE] }"
#define iplink_full_usage "\n\n" \
       "iplink set DEVICE { up | down | arp | multicast { on | off } |\n" \
       "			dynamic { on | off } |\n" \
       "			mtu MTU }\n" \
       "iplink show [DEVICE]" \

#define iproute_trivial_usage \
       "{ list | flush | { add | del | change | append |\n" \
       "		replace | monitor } ROUTE }"
#define iproute_full_usage "\n\n" \
       "iproute { list | flush } SELECTOR\n" \
       "iproute get ADDRESS [from ADDRESS iif STRING]\n" \
       "			[oif STRING]  [tos TOS]\n" \
       "iproute { add | del | change | append | replace | monitor } ROUTE\n" \
       "			SELECTOR := [root PREFIX] [match PREFIX] [proto RTPROTO]\n" \
       "			ROUTE := [TYPE] PREFIX [tos TOS] [proto RTPROTO]" \

#define iprule_trivial_usage \
       "{[list | add | del] RULE}"
#define iprule_full_usage "\n\n" \
       "iprule [list | add | del] SELECTOR ACTION\n" \
       "	SELECTOR := [from PREFIX] [to PREFIX] [tos TOS] [fwmark FWMARK]\n" \
       "			[dev STRING] [pref NUMBER]\n" \
       "	ACTION := [table TABLE_ID] [nat ADDRESS]\n" \
       "			[prohibit | reject | unreachable]\n" \
       "			[realms [SRCREALM/]DSTREALM]\n" \
       "	TABLE_ID := [local | main | default | NUMBER]" \

#define iptunnel_trivial_usage \
       "{ add | change | del | show } [NAME]\n" \
       "	[mode { ipip | gre | sit }]\n" \
       "	[remote ADDR] [local ADDR] [ttl TTL]"
#define iptunnel_full_usage "\n\n" \
       "iptunnel { add | change | del | show } [NAME]\n" \
       "	[mode { ipip | gre | sit }] [remote ADDR] [local ADDR]\n" \
       "	[[i|o]seq] [[i|o]key KEY] [[i|o]csum]\n" \
       "	[ttl TTL] [tos TOS] [[no]pmtudisc] [dev PHYS_DEV]" \

#define kbd_mode_trivial_usage \
       "[-a|k|s|u]"
#define kbd_mode_full_usage "\n\n" \
       "Report or set the keyboard mode\n" \
     "\nOptions set mode:" \
     "\n	-a	Default (ASCII)" \
     "\n	-k	Medium-raw (keyboard)" \
     "\n	-s	Raw (scancode)" \
     "\n	-u	Unicode (utf-8)" \

#define kill_trivial_usage \
       "[-l] [-signal] process-id..."
#define kill_full_usage "\n\n" \
       "Send a signal (default is TERM) to the specified process(es)\n" \
     "\nOptions:" \
     "\n	-l	List all signal names and numbers" \

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
       "[-l] [-q] [-signal] process-name..."
#define killall_full_usage "\n\n" \
       "Send a signal (default is TERM) to the specified process(es)\n" \
     "\nOptions:" \
     "\n	-l	List all signal names and numbers" \
     "\n	-q	Do not complain if no processes were killed" \

#define killall_example_usage \
       "$ killall apache\n"

#define killall5_trivial_usage \
       "[-l] [-signal]"
#define killall5_full_usage "\n\n" \
       "Send a signal (default is TERM) to all processes outside current session\n" \
     "\nOptions:" \
     "\n	-l	List all signal names and numbers" \

#define klogd_trivial_usage \
       "[-c N] [-n]"
#define klogd_full_usage "\n\n" \
       "Kernel logger\n" \
     "\nOptions:" \
     "\n	-c N	Only messages with level < N are printed to console" \
     "\n	-n	Run in foreground" \

#define length_trivial_usage \
       "STRING"
#define length_full_usage "\n\n" \
       "Print STRING's length"

#define length_example_usage \
       "$ length Hello\n" \
       "5\n"

#define less_trivial_usage \
       "[-EMNmh~?] [FILE...]"
#define less_full_usage "\n\n" \
       "View a file or list of files. The position within files can be\n" \
       "changed, and files can be manipulated in various ways.\n" \
     "\nOptions:" \
     "\n	-E	Quit once the end of a file is reached" \
     "\n	-M,-m	Display a status line containing the line numbers" \
     "\n		and percentage through the file" \
     "\n	-N	Prefix line numbers to each line" \
     "\n	-~	Suppress ~s displayed past the end of the file" \

#define linux32_trivial_usage NOUSAGE_STR
#define linux32_full_usage ""
#define linux64_trivial_usage NOUSAGE_STR
#define linux64_full_usage ""

#define linuxrc_trivial_usage NOUSAGE_STR
#define linuxrc_full_usage ""

#define setarch_trivial_usage \
       "personality program [args...]"
#define setarch_full_usage "\n\n" \
       "Personality may be:\n" \
       "	linux32		Set 32bit uname emulation\n" \
       "	linux64		Set 64bit uname emulation" \

#define ln_trivial_usage \
       "[OPTION] TARGET... LINK_NAME|DIRECTORY"
#define ln_full_usage "\n\n" \
       "Create a link named LINK_NAME or DIRECTORY to the specified TARGET.\n" \
       "Use '--' to indicate that all following arguments are non-options.\n" \
     "\nOptions:" \
     "\n	-s	Make symlinks instead of hardlinks" \
     "\n	-f	Remove existing destination files" \
     "\n	-n	Don't dereference symlinks - treat like normal file" \
     "\n	-b	Make a backup of the target (if exists) before link operation" \
     "\n	-S suf	Use suffix instead of ~ when making backup files" \

#define ln_example_usage \
       "$ ln -s BusyBox /tmp/ls\n" \
       "$ ls -l /tmp/ls\n" \
       "lrwxrwxrwx    1 root     root            7 Apr 12 18:39 ls -> BusyBox*\n"

#define load_policy_trivial_usage NOUSAGE_STR

#define load_policy_full_usage ""

#define loadfont_trivial_usage \
       "< font"
#define loadfont_full_usage "\n\n" \
       "Load a console font from standard input"
#define loadfont_example_usage \
       "$ loadfont < /etc/i18n/fontname\n"

#define loadkmap_trivial_usage \
       "< keymap"
#define loadkmap_full_usage "\n\n" \
       "Load a binary keyboard translation table from standard input"
#define loadkmap_example_usage \
       "$ loadkmap < /etc/i18n/lang-keymap\n"

#define logger_trivial_usage \
       "[OPTION]... [MESSAGE]"
#define logger_full_usage "\n\n" \
       "Write MESSAGE to the system log. If MESSAGE is omitted, log stdin.\n" \
     "\nOptions:" \
     "\n	-s	Log to stderr as well as the system log" \
     "\n	-t TAG	Log using the specified tag (defaults to user name)" \
     "\n	-p PRIO	Priority (numeric or facility.level pair)" \

#define logger_example_usage \
       "$ logger \"hello\"\n"

#define login_trivial_usage \
       "[-p] [-h HOST] [[-f] USER]"
#define login_full_usage "\n\n" \
       "Begin a new session on the system\n" \
     "\nOptions:" \
     "\n	-f	Do not authenticate (user already authenticated)" \
     "\n	-h	Name of the remote host" \
     "\n	-p	Preserve environment" \

#define logname_trivial_usage \
       ""
#define logname_full_usage "\n\n" \
       "Print the name of the current user"
#define logname_example_usage \
       "$ logname\n" \
       "root\n"

#define logread_trivial_usage \
       "[OPTION]..."
#define logread_full_usage "\n\n" \
       "Show messages in syslogd's circular buffer\n" \
     "\nOptions:" \
     "\n	-f	Output data as log grows" \

#define losetup_trivial_usage \
       "[-o OFS] LOOPDEV FILE - associate loop devices\n" \
       "	losetup -d LOOPDEV - disassociate\n" \
       "	losetup [-f] - show"
#define losetup_full_usage "\n\n" \
       "Options:" \
     "\n	-o OFS	Start OFS bytes into FILE" \
     "\n	-f	Show first free loop device" \

#define losetup_notes_usage \
       "No arguments will display all current associations.\n" \
       "One argument (losetup /dev/loop1) will display the current association\n" \
       "(if any), or disassociate it (with -d). The display shows the offset\n" \
       "and filename of the file the loop device is currently bound to.\n\n" \
       "Two arguments (losetup /dev/loop1 file.img) create a new association,\n" \
       "with an optional offset (-o 12345). Encryption is not yet supported.\n" \
       "losetup -f will show the first loop free loop device\n\n"

#define lpd_trivial_usage \
       "SPOOLDIR [HELPER [ARGS...]]"
#define lpd_full_usage "\n\n" \
       "Example:" \
     "\n	tcpsvd -E 0 515 softlimit -m 999999 lpd /var/spool ./print"

#define lpq_trivial_usage \
       "[-P queue[@host[:port]]] [-U USERNAME] [-d JOBID...] [-fs]"
#define lpq_full_usage "\n\n" \
       "Options:" \
     "\n	-P	lp service to connect to (else uses $PRINTER)" \
     "\n	-d	Delete jobs" \
     "\n	-f	Force any waiting job to be printed" \
     "\n	-s	Short display" \

#define lpr_trivial_usage \
       "-P queue[@host[:port]] -U USERNAME -J TITLE -Vmh [FILE...]"
/* -C CLASS exists too, not shown.
 * CLASS is supposed to be printed on banner page, if one is requested */
#define lpr_full_usage "\n\n" \
       "Options:" \
     "\n	-P	lp service to connect to (else uses $PRINTER)"\
     "\n	-m	Send mail on completion" \
     "\n	-h	Print banner page too" \
     "\n	-V	Verbose" \

#define ls_trivial_usage \
       "[-1Aa" USE_FEATURE_LS_TIMESTAMPS("c") "Cd" \
	USE_FEATURE_LS_TIMESTAMPS("e") USE_FEATURE_LS_FILETYPES("F") "iln" \
	USE_FEATURE_LS_FILETYPES("p") USE_FEATURE_LS_FOLLOWLINKS("L") \
	USE_FEATURE_LS_RECURSIVE("R") USE_FEATURE_LS_SORTFILES("rS") "s" \
	USE_FEATURE_AUTOWIDTH("T") USE_FEATURE_LS_TIMESTAMPS("tu") \
	USE_FEATURE_LS_SORTFILES("v") USE_FEATURE_AUTOWIDTH("w") "x" \
	USE_FEATURE_LS_SORTFILES("X") USE_FEATURE_HUMAN_READABLE("h") "k" \
	USE_SELINUX("K") "] [filenames...]"
#define ls_full_usage "\n\n" \
       "List directory contents\n" \
     "\nOptions:" \
     "\n	-1	List files in a single column" \
     "\n	-A	Do not list implied . and .." \
     "\n	-a	Do not hide entries starting with ." \
     "\n	-C	List entries by columns" \
	USE_FEATURE_LS_TIMESTAMPS( \
     "\n	-c	With -l: show ctime") \
	USE_FEATURE_LS_COLOR( \
     "\n	--color[={always,never,auto}]	Control coloring") \
     "\n	-d	List directory entries instead of contents" \
	USE_FEATURE_LS_TIMESTAMPS( \
     "\n	-e	List both full date and full time") \
	USE_FEATURE_LS_FILETYPES( \
     "\n	-F	Append indicator (one of */=@|) to entries") \
     "\n	-i	List the i-node for each file" \
     "\n	-l	Use a long listing format" \
     "\n	-n	List numeric UIDs and GIDs instead of names" \
	USE_FEATURE_LS_FILETYPES( \
     "\n	-p	Append indicator (one of /=@|) to entries") \
	USE_FEATURE_LS_FOLLOWLINKS( \
     "\n	-L	List entries pointed to by symlinks") \
	USE_FEATURE_LS_RECURSIVE( \
     "\n	-R	List subdirectories recursively") \
	USE_FEATURE_LS_SORTFILES( \
     "\n	-r	Sort the listing in reverse order") \
	USE_FEATURE_LS_SORTFILES( \
     "\n	-S	Sort the listing by file size") \
     "\n	-s	List the size of each file, in blocks" \
	USE_FEATURE_AUTOWIDTH( \
     "\n	-T NUM	Assume Tabstop every NUM columns") \
	USE_FEATURE_LS_TIMESTAMPS( \
     "\n	-t	With -l: show modification time") \
	USE_FEATURE_LS_TIMESTAMPS( \
     "\n	-u	With -l: show access time") \
	USE_FEATURE_LS_SORTFILES( \
     "\n	-v	Sort the listing by version") \
	USE_FEATURE_AUTOWIDTH( \
     "\n	-w NUM	Assume the terminal is NUM columns wide") \
     "\n	-x	List entries by lines instead of by columns" \
	USE_FEATURE_LS_SORTFILES( \
     "\n	-X	Sort the listing by extension") \
	USE_FEATURE_HUMAN_READABLE( \
     "\n	-h	Print sizes in human readable format (e.g., 1K 243M 2G)") \
	USE_SELINUX( \
     "\n	-k	Print security context") \
	USE_SELINUX( \
     "\n	-K	Print security context in long format") \
	USE_SELINUX( \
     "\n	-Z	Print security context and permission") \

#define lsattr_trivial_usage \
       "[-Radlv] [files...]"
#define lsattr_full_usage "\n\n" \
       "List file attributes on an ext2 fs\n" \
     "\nOptions:" \
     "\n	-R	Recursively list subdirectories" \
     "\n	-a	Do not hide entries starting with ." \
     "\n	-d	List directory entries instead of contents" \
     "\n	-l	Print long flag names" \
     "\n	-v	List the file's version/generation number" \

#define lsmod_trivial_usage \
       ""
#define lsmod_full_usage "\n\n" \
       "List the currently loaded kernel modules"

#if ENABLE_FEATURE_MAKEDEVS_LEAF
#define makedevs_trivial_usage \
       "NAME TYPE MAJOR MINOR FIRST LAST [s]"
#define makedevs_full_usage "\n\n" \
       "Create a range of block or character special files\n\n" \
       "TYPEs include:\n" \
       "	b	Make a block device\n" \
       "	c or u	Make a character device\n" \
       "	p	Make a named pipe. MAJOR and MINOR are ignored\n" \
       "\n" \
       "FIRST specifies the number appended to NAME to create the first device.\n" \
       "LAST specifies the number of the last item that should be created\n" \
       "If 's' is the last argument, the base device is created as well.\n\n" \
       "For example:\n" \
       "	makedevs /dev/ttyS c 4 66 2 63   ->  ttyS2-ttyS63\n" \
       "	makedevs /dev/hda b 3 0 0 8 s    ->  hda,hda1-hda8"
#define makedevs_example_usage \
       "# makedevs /dev/ttyS c 4 66 2 63\n" \
       "[creates ttyS2-ttyS63]\n" \
       "# makedevs /dev/hda b 3 0 0 8 s\n" \
       "[creates hda,hda1-hda8]\n"
#endif

#if ENABLE_FEATURE_MAKEDEVS_TABLE
#define makedevs_trivial_usage \
       "[-d device_table] rootdir"
#define makedevs_full_usage "\n\n" \
       "Create a range of special files as specified in a device table.\n" \
       "Device table entries take the form of:\n" \
       "<type> <mode> <uid> <gid> <major> <minor> <start> <inc> <count>\n" \
       "Where name is the file name, type can be one of:\n" \
       "	f	A regular file\n" \
       "	d	Directory\n" \
       "	c	Character special device file\n" \
       "	b	Block special device file\n" \
       "	p	Fifo (named pipe)\n" \
       "uid is the user id for the target file, gid is the group id for the\n" \
       "target file. The rest of the entries (major, minor, etc) apply to\n" \
       "to device special files. A '-' may be used for blank entries."
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

#define man_trivial_usage \
       "[OPTION]... [MANPAGE]..."
#define man_full_usage "\n\n" \
       "Format and display manual page\n" \
     "\nOptions:" \
     "\n	-a      Display all pages" \
     "\n	-w	Show page locations" \

#define matchpathcon_trivial_usage \
       "[-n] [-N] [-f file_contexts_file] [-p prefix] [-V]"
#define matchpathcon_full_usage "\n\n" \
       "	-n	Do not display path" \
     "\n	-N	Do not use translations" \
     "\n	-f	Use alternate file_context file" \
     "\n	-p	Use prefix to speed translations" \
     "\n	-V	Verify file context on disk matches defaults" \

#define md5sum_trivial_usage \
       "[OPTION] [FILEs...]" \
	USE_FEATURE_MD5_SHA1_SUM_CHECK("\n   or: md5sum [OPTION] -c [FILE]")
#define md5sum_full_usage "\n\n" \
       "Print" USE_FEATURE_MD5_SHA1_SUM_CHECK(" or check") " MD5 checksums" \
	USE_FEATURE_MD5_SHA1_SUM_CHECK( "\n" \
     "\nOptions:" \
     "\n	-c	Check MD5 sums against given list" \
     "\n	-s	Don't output anything, status code shows success" \
     "\n	-w	Warn about improperly formatted MD5 checksum lines") \

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
#define mdev_full_usage "\n\n" \
       "	-s	Scan /sys and populate /dev during system boot\n" \
       "\n" \
       "Called with no options (via hotplug) it uses environment variables\n" \
       "to determine which device to add/remove."

#define mdev_notes_usage "" \
	USE_FEATURE_MDEV_CONFIG( \
       "The mdev config file contains lines that look like:\n" \
       "  hd[a-z][0-9]* 0:3 660\n\n" \
       "That's device name (with regex match), uid:gid, and permissions.\n\n" \
	USE_FEATURE_MDEV_EXEC( \
       "Optionally, that can be followed (on the same line) by a special character\n" \
       "and a command line to run after creating/before deleting the corresponding\n" \
       "device(s). The environment variable $MDEV indicates the active device node\n" \
       "(which is useful if it's a regex match). For example:\n\n" \
       "  hdc root:cdrom 660  *ln -s $MDEV cdrom\n\n" \
       "The special characters are @ (run after creating), $ (run before deleting),\n" \
       "and * (run both after creating and before deleting). The commands run in\n" \
       "the /dev directory, and use system() which calls /bin/sh.\n\n" \
	) \
       "Config file parsing stops on the first matching line. If no config\n" \
       "entry is matched, devices are created with default 0:0 660. (Make\n" \
       "the last line match .* to override this.)\n\n" \
	)

#define mesg_trivial_usage \
       "[y|n]"
#define mesg_full_usage "\n\n" \
       "Control write access to your terminal\n" \
       "	y	Allow write access to your terminal\n" \
       "	n	Disallow write access to your terminal"

#define microcom_trivial_usage \
       "[-d DELAY] [-t TIMEOUT] [-s SPEED] [-X] TTY"
#define microcom_full_usage "\n\n" \
       "Copy bytes for stdin to TTY and from TTY to stdout\n" \
     "\nOptions:" \
     "\n	-d	Wait up to DELAY ms for TTY output before sending every" \
     "\n		next byte to it" \
     "\n	-t	Exit if both stdin and TTY are silent for TIMEOUT ms" \
     "\n	-s	Set serial line to SPEED" \
     "\n	-X	Disable special meaning of NUL and Ctrl-X from stdin" \

#define mkdir_trivial_usage \
       "[OPTION] DIRECTORY..."
#define mkdir_full_usage "\n\n" \
       "Create DIRECTORY\n" \
     "\nOptions:" \
     "\n	-m	Set permission mode (as in chmod), not rwxrwxrwx - umask" \
     "\n	-p	No error if existing, make parent directories as needed" \
	USE_SELINUX( \
     "\n	-Z	Set security context" \
	)

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
#define mke2fs_full_usage "\n\n" \
       "	-b size		Block size in bytes" \
     "\n	-c		Check for bad blocks before creating" \
     "\n	-E opts		Set extended options" \
     "\n	-f size		Fragment size in bytes" \
     "\n	-F		Force (ignore sanity checks)" \
     "\n	-g num		Number of blocks in a block group" \
     "\n	-i ratio	The bytes/inode ratio" \
     "\n	-j		Create a journal (ext3)" \
     "\n	-J opts		Set journal options (size/device)" \
     "\n	-l file		Read bad blocks list from file" \
     "\n	-L lbl		Set the volume label" \
     "\n	-m percent	Percent of fs blocks to reserve for admin" \
     "\n	-M dir		Set last mounted directory" \
     "\n	-n		Do not actually create anything" \
     "\n	-N num		Number of inodes to create" \
     "\n	-o os		Set the 'creator os' field" \
     "\n	-O features	Dir_index/filetype/has_journal/journal_dev/sparse_super" \
     "\n	-q		Quiet" \
     "\n	-r rev		Set filesystem revision" \
     "\n	-S		Write superblock and group descriptors only" \
     "\n	-T fs-type	Set usage type (news/largefile/largefile4)" \
     "\n	-v		Verbose" \

#define mkfifo_trivial_usage \
       "[OPTIONS] name"
#define mkfifo_full_usage "\n\n" \
       "Create named pipe (identical to 'mknod name p')\n" \
     "\nOptions:" \
     "\n	-m MODE	Mode (default a=rw)" \
	USE_SELINUX( \
     "\n	-Z	Set security context" \
	)

#define mkfs_minix_trivial_usage \
       "[-c | -l filename] [-nXX] [-iXX] /dev/name [blocks]"
#define mkfs_minix_full_usage "\n\n" \
       "Make a MINIX filesystem\n" \
     "\nOptions:" \
     "\n	-c		Check device for bad blocks" \
     "\n	-n [14|30]	Maximum length of filenames" \
     "\n	-i INODES	Number of inodes for the filesystem" \
     "\n	-l FILENAME	Read bad blocks list from FILENAME" \
     "\n	-v		Make version 2 filesystem" \

#define mknod_trivial_usage \
       "[OPTIONS] NAME TYPE MAJOR MINOR"
#define mknod_full_usage "\n\n" \
       "Create a special file (block, character, or pipe)\n" \
     "\nOptions:" \
     "\n	-m	Create the special file using the specified mode (default a=rw)" \
     "\nTYPEs include:" \
     "\n	b:	Make a block device" \
     "\n	c or u:	Make a character device" \
     "\n	p:	Make a named pipe (MAJOR and MINOR are ignored)" \
	USE_SELINUX( \
     "\n	-Z	Set security context" \
	)

#define mknod_example_usage \
       "$ mknod /dev/fd0 b 2 0\n" \
       "$ mknod -m 644 /tmp/pipe p\n"

#define mkswap_trivial_usage \
       "DEVICE"
#define mkswap_full_usage "\n\n" \
       "Prepare block device to be used as swap partition"
#if 0
       "[-c] [-v0|-v1] DEVICE [BLOCKS]"
     "\nOptions:"
     "\n	-c	Check for readability"
     "\n	-v0	Make swap version 0 (max 128M)"
     "\n	-v1	Make swap version 1 (default for kernels > 2.1.117)"
     "\n	BLOCKS	Number of blocks to use (default is entire partition)"
#endif

#define mktemp_trivial_usage \
       "[-dt] [-p DIR] [TEMPLATE]"
#define mktemp_full_usage "\n\n" \
       "Create a temporary file with name based on TEMPLATE and print its name.\n" \
       "TEMPLATE must end with XXXXXX (e.g. [/dir/]nameXXXXXX).\n" \
     "\nOptions:" \
     "\n	-d	Make a directory instead of a file" \
/*   "\n	-q	Fail silently if an error occurs" - we ignore it */ \
     "\n	-t	Generate a path rooted in temporary directory" \
     "\n	-p DIR	Use DIR as a temporary directory (implies -t)" \
     "\n" \
     "\nFor -t or -p, directory is chosen as follows:" \
     "\n$TMPDIR if set, else -p DIR, else /tmp" \

#define mktemp_example_usage \
       "$ mktemp /tmp/temp.XXXXXX\n" \
       "/tmp/temp.mWiLjM\n" \
       "$ ls -la /tmp/temp.mWiLjM\n" \
       "-rw-------    1 andersen andersen        0 Apr 25 17:10 /tmp/temp.mWiLjM\n"

#define modprobe_trivial_usage \
       "[-knqrsv] MODULE [symbol=value...]"
#define modprobe_full_usage "\n\n" \
       "Options:" \
     "\n	-k	Make module autoclean-able" \
     "\n	-n	Dry run" \
     "\n	-q	Quiet" \
     "\n	-r	Remove module (stacks) or do autoclean" \
     "\n	-s	Report via syslog instead of stderr" \
     "\n	-v	Verbose" \

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
"    It makes sense if your mod_name is long, or you want a more representative\n" \
"    name for that module (eg. 'scsi' in place of 'aha7xxx').\n" \
"    This makes it also possible to use a different set of options (below) for\n" \
"    the module and the alias.\n" \
"    A module can be aliased more than once.\n" \
"\n" \
"  options <mod_name|alias_name> <symbol=value...>\n" \
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
       "   which are the default for alias 'tulip2' overridden by the option 'irq=8'\n\n" \
       "   from the command line\n\n" \
       "$ modprobe tulip2 irq=2 io=0x210\n" \
       "   will load the module 'tulip' with default options 'irq=4 io=0x308 irq=4 io=0x210',\n" \
       "   which are the default for alias 'tulip2' overridden by the options 'irq=2 io=0x210'\n\n" \
       "   from the command line\n"

#define more_trivial_usage \
       "[FILE...]"
#define more_full_usage "\n\n" \
       "View FILE or standard input one screenful at a time"

#define more_example_usage \
       "$ dmesg | more\n"

#define mount_trivial_usage \
       "[flags] DEVICE NODE [-o options,more-options]"
#define mount_full_usage "\n\n" \
       "Mount a filesystem. Filesystem autodetection requires /proc be mounted.\n" \
     "\nOptions:" \
     "\n	-a		Mount all filesystems in fstab" \
	USE_FEATURE_MOUNT_FAKE( \
     "\n	-f		"USE_FEATURE_MTAB_SUPPORT("Update /etc/mtab, but ")"don't mount" \
	) \
	USE_FEATURE_MTAB_SUPPORT( \
     "\n	-n		Don't update /etc/mtab" \
	) \
     "\n	-r		Read-only mount" \
     "\n	-t fs-type	Filesystem type" \
     "\n	-w		Read-write mount (default)" \
       "\n" \
       "-o option:\n" \
	USE_FEATURE_MOUNT_LOOP( \
       "	loop		Ignored (loop devices are autodetected)\n" \
	) \
	USE_FEATURE_MOUNT_FLAGS( \
       "	[a]sync		Writes are asynchronous / synchronous\n" \
       "	[no]atime	Disable / enable updates to inode access times\n" \
       "	[no]diratime	Disable / enable atime updates to directories\n" \
       "	[no]dev		Allow use of special device files / disallow them\n" \
       "	[no]exec	Allow use of executable files / disallow them\n" \
       "	[no]suid	Allow set-user-id-root programs / disallow them\n" \
       "	[r]shared	Convert [recursively] to a shared subtree\n" \
       "	[r]slave	Convert [recursively] to a slave subtree\n" \
       "	[r]private	Convert [recursively] to a private subtree\n" \
       "	[un]bindable	Make mount point [un]able to be bind mounted\n" \
       "	bind		Bind a directory to an additional location\n" \
       "	move		Relocate an existing mount point\n" \
	) \
       "	remount		Remount a mounted filesystem, changing its flags\n" \
       "	ro/rw		Mount for read-only / read-write\n" \
       "\n" \
       "There are EVEN MORE flags that are specific to each filesystem\n" \
       "You'll have to see the written documentation for those filesystems" \

#define mount_example_usage \
       "$ mount\n" \
       "/dev/hda3 on / type minix (rw)\n" \
       "proc on /proc type proc (rw)\n" \
       "devpts on /dev/pts type devpts (rw)\n" \
       "$ mount /dev/fd0 /mnt -t msdos -o ro\n" \
       "$ mount /tmp/diskimage /opt -t ext2 -o loop\n" \
       "$ mount cd_image.iso mydir\n"
#define mount_notes_usage \
       "Returns 0 for success, number of failed mounts for -a, or errno for one mount."

#define mountpoint_trivial_usage \
       "[-q] <[-d] DIR | -x DEVICE>"
#define mountpoint_full_usage "\n\n" \
       "mountpoint checks if the directory is a mountpoint\n" \
     "\nOptions:" \
     "\n	-q	Quiet" \
     "\n	-d	Print major/minor device number of the filesystem" \
     "\n	-x	Print major/minor device number of the blockdevice" \

#define mountpoint_example_usage \
       "$ mountpoint /proc\n" \
       "/proc is not a mountpoint\n" \
       "$ mountpoint /sys\n" \
       "/sys is a mountpoint\n"

#define mt_trivial_usage \
       "[-f device] opcode value"
#define mt_full_usage "\n\n" \
       "Control magnetic tape drive operation\n" \
       "\n" \
       "Available Opcodes:\n" \
       "\n" \
       "bsf bsfm bsr bss datacompression drvbuffer eof eom erase\n" \
       "fsf fsfm fsr fss load lock mkpart nop offline ras1 ras2\n" \
       "ras3 reset retension rewind rewoffline seek setblk setdensity\n" \
       "setpart tell unload unlock weof wset" \

#define mv_trivial_usage \
       "[OPTION]... SOURCE DEST\n" \
       "or: mv [OPTION]... SOURCE... DIRECTORY"
#define mv_full_usage "\n\n" \
       "Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY\n" \
     "\nOptions:" \
     "\n	-f	Don't prompt before overwriting" \
     "\n	-i	Interactive, prompt before overwrite" \

#define mv_example_usage \
       "$ mv /tmp/foo /bin/bar\n"

#define nameif_trivial_usage \
       "[-s] [-c FILE] [{IFNAME MACADDR}]"
#define nameif_full_usage "\n\n" \
       "Rename network interface while it in the down state\n" \
     "\nOptions:" \
     "\n	-c FILE		Use configuration file (default is /etc/mactab)" \
     "\n	-s		Use syslog (LOCAL0 facility)" \
     "\n	IFNAME MACADDR	new_interface_name interface_mac_address" \

#define nameif_example_usage \
       "$ nameif -s dmz0 00:A0:C9:8C:F6:3F\n" \
       " or\n" \
       "$ nameif -c /etc/my_mactab_file\n" \

#if !ENABLE_DESKTOP

#if ENABLE_NC_SERVER || ENABLE_NC_EXTRA
#define NC_OPTIONS_STR "\n\nOptions:"
#else
#define NC_OPTIONS_STR
#endif

#define nc_trivial_usage \
	USE_NC_EXTRA("[-iN] [-wN] ")USE_NC_SERVER("[-l] [-p PORT] ") \
       "["USE_NC_EXTRA("-f FILENAME|")"IPADDR PORTNUM]"USE_NC_EXTRA(" [-e COMMAND]")
#define nc_full_usage "\n\n" \
       "Open a pipe to IP:port" USE_NC_EXTRA(" or file") \
	NC_OPTIONS_STR \
	USE_NC_EXTRA( \
     "\n	-e	Exec rest of command line after connect" \
     "\n	-i SECS	Delay interval for lines sent" \
     "\n	-w SECS	Timeout for connect" \
     "\n	-f FILE	Use file (ala /dev/ttyS0) instead of network" \
	) \
	USE_NC_SERVER( \
     "\n	-l	Listen mode, for inbound connects" \
	USE_NC_EXTRA( \
     "\n		(use -l twice with -e for persistent server)") \
     "\n	-p PORT	Local port number" \
	)

#define nc_notes_usage "" \
	USE_NC_EXTRA( \
       "To use netcat as a terminal emulator on a serial port:\n\n" \
       "$ stty 115200 -F /dev/ttyS0\n" \
       "$ stty raw -echo -ctlecho && nc -f /dev/ttyS0\n" \
	)

#define nc_example_usage \
       "$ nc foobar.somedomain.com 25\n" \
       "220 foobar ESMTP Exim 3.12 #1 Sat, 15 Apr 2000 00:03:02 -0600\n" \
       "help\n" \
       "214-Commands supported:\n" \
       "214-    HELO EHLO MAIL RCPT DATA AUTH\n" \
       "214     NOOP QUIT RSET HELP\n" \
       "quit\n" \
       "221 foobar closing connection\n"

#else /* DESKTOP nc - much more compatible with nc 1.10 */

#define nc_trivial_usage \
       "[-options] hostname port  - connect" \
	USE_NC_SERVER("\n" \
       "nc [-options] -l -p port [hostname] [port]  - listen")
#define nc_full_usage "\n\n" \
       "Options:" \
     "\n	-e prog [args]	Program to exec after connect (must be last)" \
	USE_NC_SERVER( \
     "\n	-l		Listen mode, for inbound connects" \
	) \
     "\n	-n		Don't do DNS resolution" \
     "\n	-s addr		Local address" \
     "\n	-p port		Local port" \
     "\n	-u		UDP mode" \
     "\n	-v		Verbose (cumulative: -vv)" \
     "\n	-w secs		Timeout for connects and final net reads" \
	USE_NC_EXTRA( \
     "\n	-i sec		Delay interval for lines sent" /* ", ports scanned" */ \
     "\n	-o file		Hex dump of traffic" \
     "\n	-z		Zero-I/O mode (scanning)" \
	) \
/*   "\n	-r		Randomize local and remote ports" */
/*   "\n	-g gateway	Source-routing hop point[s], up to 8" */
/*   "\n	-G num		Source-routing pointer: 4, 8, 12, ..." */
/*   "\nport numbers can be individual or ranges: lo-hi [inclusive]" */

#endif

#define netstat_trivial_usage \
       "[-laentuwxr"USE_FEATURE_NETSTAT_WIDE("W")"]"
#define netstat_full_usage "\n\n" \
       "Display networking information\n" \
     "\nOptions:" \
     "\n	-l	Display listening server sockets" \
     "\n	-a	Display all sockets (default: connected)" \
     "\n	-e	Display other/more information" \
     "\n	-n	Don't resolve names" \
     "\n	-t	Tcp sockets" \
     "\n	-u	Udp sockets" \
     "\n	-w	Raw sockets" \
     "\n	-x	Unix sockets" \
     "\n	-r	Display routing table" \
	USE_FEATURE_NETSTAT_WIDE( \
     "\n	-W	Display with no column truncation" \
	)

#define nice_trivial_usage \
       "[-n ADJUST] [COMMAND [ARG]...]"
#define nice_full_usage "\n\n" \
       "Run a program with modified scheduling priority\n" \
     "\nOptions:" \
     "\n	-n ADJUST	Adjust the scheduling priority by ADJUST" \

#define nmeter_trivial_usage \
       "format_string"
#define nmeter_full_usage "\n\n" \
       "Monitor system in real time\n\n" \
       "Format specifiers:\n" \
       "%Nc or %[cN]	Monitor CPU. N - bar size, default 10\n" \
       "		(displays: S:system U:user N:niced D:iowait I:irq i:softirq)\n" \
       "%[niface]	Monitor network interface 'iface'\n" \
       "%m		Monitor allocated memory\n" \
       "%[mf]		Monitor free memory\n" \
       "%[mt]		Monitor total memory\n" \
       "%s		Monitor allocated swap\n" \
       "%f		Monitor number of used file descriptors\n" \
       "%Ni		Monitor total/specific IRQ rate\n" \
       "%x		Monitor context switch rate\n" \
       "%p		Monitor forks\n" \
       "%[pn]		Monitor # of processes\n" \
       "%b		Monitor block io\n" \
       "%Nt		Show time (with N decimal points)\n" \
       "%Nd		Milliseconds between updates (default=1000)\n" \
       "%r		Print <cr> instead of <lf> at EOL" \

#define nmeter_example_usage \
       "nmeter '%250d%t %20c int %i bio %b mem %m forks%p'"

#define nohup_trivial_usage \
       "COMMAND [ARGS]"
#define nohup_full_usage "\n\n" \
       "Run a command immune to hangups, with output to a non-tty"
#define nohup_example_usage \
       "$ nohup make &"

#define nslookup_trivial_usage \
       "[HOST] [SERVER]"
#define nslookup_full_usage "\n\n" \
       "Query the nameserver for the IP address of the given HOST\n" \
       "optionally using a specified DNS server"
#define nslookup_example_usage \
       "$ nslookup localhost\n" \
       "Server:     default\n" \
       "Address:    default\n" \
       "\n" \
       "Name:       debian\n" \
       "Address:    127.0.0.1\n"

#define od_trivial_usage \
       "[-aBbcDdeFfHhIiLlOovXx] " USE_DESKTOP("[-t TYPE] ") "[FILE]"
#define od_full_usage "\n\n" \
       "Write an unambiguous representation, octal bytes by default, of FILE\n" \
       "to standard output. With no FILE or when FILE is -, read standard input."

#define openvt_trivial_usage \
       "[-c NUM] [-sw] [COMMAND [ARGS]]"
#define openvt_full_usage "\n\n" \
       "Start COMMAND on a new virtual terminal\n" \
     "\nOptions:" \
     "\n	-c	Use specified VT" \
     "\n	-s	Switch to the VT" \
/*   "\n	-l	Run COMMAND as login shell (by prepending '-')" */ \
     "\n	-w	Wait for COMMAND to exit" \

#define openvt_example_usage \
       "openvt 2 /bin/ash\n"

#define passwd_trivial_usage \
       "[OPTION] [name]"
#define passwd_full_usage "\n\n" \
       "Change user's password. If no name is specified,\n" \
       "changes the password for the current user.\n" \
     "\nOptions:" \
     "\n	-a	Algorithm to use for password (choices: des, md5)" /* ", sha1)" */ \
     "\n	-d	Delete password for the account" \
     "\n	-l	Lock (disable) account" \
     "\n	-u	Unlock (re-enable) account" \

#define chpasswd_trivial_usage \
	USE_GETOPT_LONG("[--md5|--encrypt]") SKIP_GETOPT_LONG("[-m|-e]")
#define chpasswd_full_usage "\n\n" \
       "Read user:password information from stdin\n" \
       "and update /etc/passwd accordingly.\n" \
     "\nOptions:" \
	USE_GETOPT_LONG( \
     "\n	-e,--encrypt	Supplied passwords are in encrypted form" \
     "\n	-m,--md5	Use MD5 encryption instead of DES" \
	) \
	SKIP_GETOPT_LONG( \
     "\n	-e	Supplied passwords are in encrypted form" \
     "\n	-m	Use MD5 encryption instead of DES" \
	)

#define patch_trivial_usage \
       "[-p NUM] [-i DIFF] [-R]"
#define patch_full_usage "\n\n" \
       "	-p NUM	Strip NUM leading components from file names" \
     "\n	-i DIFF	Read DIFF instead of stdin" \
     "\n	-R	Reverse patch" \

#define patch_example_usage \
       "$ patch -p1 < example.diff\n" \
       "$ patch -p0 -i example.diff"

#define pgrep_trivial_usage \
       "[-flnovx] pattern"
#define pgrep_full_usage "\n\n" \
       "Display process(es) selected by regex pattern\n" \
     "\nOptions:" \
     "\n	-l	Show command name too" \
     "\n	-f	Match against entire command line" \
     "\n	-n	Show the newest process only" \
     "\n	-o	Show the oldest process only" \
     "\n	-v	Negate the matching" \
     "\n	-x	Match whole name (not substring)" \

#if (ENABLE_FEATURE_PIDOF_SINGLE || ENABLE_FEATURE_PIDOF_OMIT)
#define pidof_trivial_usage \
       "[OPTION] [NAME...]"
#define USAGE_PIDOF "\n\nOptions:"
#else
#define pidof_trivial_usage \
       "[NAME...]"
#define USAGE_PIDOF /* none */
#endif
#define pidof_full_usage "\n\n" \
       "List PIDs of all processes with names that match NAMEs" \
	USAGE_PIDOF \
	USE_FEATURE_PIDOF_SINGLE( \
     "\n	-s	Show only one PID") \
	USE_FEATURE_PIDOF_OMIT( \
     "\n	-o PID	Omit given pid" \
     "\n		Use %PPID to omit pid of pidof's parent") \

#define pidof_example_usage \
       "$ pidof init\n" \
       "1\n" \
	USE_FEATURE_PIDOF_OMIT( \
       "$ pidof /bin/sh\n20351 5973 5950\n") \
	USE_FEATURE_PIDOF_OMIT( \
       "$ pidof /bin/sh -o %PPID\n20351 5950")

#if !ENABLE_FEATURE_FANCY_PING
#define ping_trivial_usage \
       "host"
#define ping_full_usage "\n\n" \
       "Send ICMP ECHO_REQUEST packets to network hosts"
#define ping6_trivial_usage \
       "host"
#define ping6_full_usage "\n\n" \
       "Send ICMP ECHO_REQUEST packets to network hosts"
#else
#define ping_trivial_usage \
       "[OPTION]... host"
#define ping_full_usage "\n\n" \
       "Send ICMP ECHO_REQUEST packets to network hosts\n" \
     "\nOptions:" \
     "\n	-4, -6		Force IPv4 or IPv6 hostname resolution" \
     "\n	-c CNT		Send only CNT pings" \
     "\n	-s SIZE		Send SIZE data bytes in packets (default=56)" \
     "\n	-I iface/IP	Use interface or IP address as source" \
     "\n	-W timeout	Seconds to wait for the first response (default:10)" \
     "\n			(after all -c CNT packets are sent)" \
     "\n	-w deadline	Seconds until ping exits (default:infinite)" \
     "\n			(can exit earlier with -c CNT)" \
     "\n	-q		Quiet, only displays output at start" \
     "\n			and when finished" \

#define ping6_trivial_usage \
       "[OPTION]... host"
#define ping6_full_usage "\n\n" \
       "Send ICMP ECHO_REQUEST packets to network hosts\n" \
     "\nOptions:" \
     "\n	-c CNT		Send only CNT pings" \
     "\n	-s SIZE		Send SIZE data bytes in packets (default=56)" \
     "\n	-I iface/IP	Use interface or IP address as source" \
     "\n	-q		Quiet, only displays output at start" \
     "\n			and when finished" \

#endif
#define ping_example_usage \
       "$ ping localhost\n" \
       "PING slag (127.0.0.1): 56 data bytes\n" \
       "64 bytes from 127.0.0.1: icmp_seq=0 ttl=255 time=20.1 ms\n" \
       "\n" \
       "--- debian ping statistics ---\n" \
       "1 packets transmitted, 1 packets received, 0% packet loss\n" \
       "round-trip min/avg/max = 20.1/20.1/20.1 ms\n"
#define ping6_example_usage \
       "$ ping6 ip6-localhost\n" \
       "PING ip6-localhost (::1): 56 data bytes\n" \
       "64 bytes from ::1: icmp6_seq=0 ttl=64 time=20.1 ms\n" \
       "\n" \
       "--- ip6-localhost ping statistics ---\n" \
       "1 packets transmitted, 1 packets received, 0% packet loss\n" \
       "round-trip min/avg/max = 20.1/20.1/20.1 ms\n"

#define pipe_progress_trivial_usage NOUSAGE_STR
#define pipe_progress_full_usage ""

#define pivot_root_trivial_usage \
       "NEW_ROOT PUT_OLD"
#define pivot_root_full_usage "\n\n" \
       "Move the current root file system to PUT_OLD and make NEW_ROOT\n" \
       "the new root file system"

#define pkill_trivial_usage \
       "[-l] | [-fnovx] [-signal] pattern"
#define pkill_full_usage "\n\n" \
       "Send a signal to process(es) selected by regex pattern\n" \
     "\nOptions:" \
     "\n	-l	List all signals" \
     "\n	-f	Match against entire command line" \
     "\n	-n	Signal the newest process only" \
     "\n	-o	Signal the oldest process only" \
     "\n	-v	Negate the matching" \
     "\n	-x	Match whole name (not substring)" \

#define poweroff_trivial_usage \
       "[-d delay] [-n] [-f]"
#define poweroff_full_usage "\n\n" \
       "Halt and shut off power\n" \
     "\nOptions:" \
     "\n	-d	Delay interval for halting" \
     "\n	-n	No call to sync()" \
     "\n	-f	Force power off (don't go through init)" \

#define printenv_trivial_usage \
       "[VARIABLES...]"
#define printenv_full_usage "\n\n" \
       "Print all or part of environment.\n" \
       "If no environment VARIABLE specified, print them all."

#define printf_trivial_usage \
       "FORMAT [ARGUMENT...]"
#define printf_full_usage "\n\n" \
       "Format and print ARGUMENT(s) according to FORMAT,\n" \
       "where FORMAT controls the output exactly as in C printf"
#define printf_example_usage \
       "$ printf \"Val=%d\\n\" 5\n" \
       "Val=5\n"


#if ENABLE_DESKTOP

#define ps_trivial_usage \
       ""
#define ps_full_usage "\n\n" \
       "Report process status\n" \
     "\nOptions:" \
     "\n	-o col1,col2=header	Select columns for display" \

#else /* !ENABLE_DESKTOP */

#if !ENABLE_SELINUX && !ENABLE_FEATURE_PS_WIDE
#define USAGE_PS "\nThis version of ps accepts no options"
#else
#define USAGE_PS "\nOptions:"
#endif

#define ps_trivial_usage \
       ""
#define ps_full_usage "\n\n" \
       "Report process status\n" \
	USAGE_PS \
	USE_SELINUX( \
     "\n	-Z	Show SE Linux context" \
	) \
	USE_FEATURE_PS_WIDE( \
     "\n	w	Wide output" \
	)

#endif /* ENABLE_DESKTOP */

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

#define pscan_trivial_usage \
       "[-cb] [-p MIN_PORT] [-P MAX_PORT] [-t TIMEOUT] [-T MIN_RTT] HOST"
#define pscan_full_usage "\n\n" \
       "Scan a host, print all open ports\n" \
     "\nOptions:" \
     "\n	-c	Show closed ports too" \
     "\n	-b	Show blocked ports too" \
     "\n	-p	Scan from this port (default 1)" \
     "\n	-P	Scan up to this port (default 1024)" \
     "\n	-t	Timeout (default 5000 ms)" \
     "\n	-T	Minimum rtt (default 5 ms, increase for congested hosts)" \

#define pwd_trivial_usage \
       ""
#define pwd_full_usage "\n\n" \
       "Print the full filename of the current working directory"
#define pwd_example_usage \
       "$ pwd\n" \
       "/root\n"

#define raidautorun_trivial_usage \
       "DEVICE"
#define raidautorun_full_usage "\n\n" \
       "Tell the kernel to automatically search and start RAID arrays"
#define raidautorun_example_usage \
       "$ raidautorun /dev/md0"

#define rdate_trivial_usage \
       "[-sp] HOST"
#define rdate_full_usage "\n\n" \
       "Get and possibly set the system date and time from a remote HOST\n" \
     "\nOptions:" \
     "\n	-s	Set the system date and time (default)" \
     "\n	-p	Print the date and time" \

#define readahead_trivial_usage \
       "[FILE]..."
#define readahead_full_usage "\n\n" \
       "Preload FILE(s) in RAM cache so that subsequent reads for those" \
       "files do not block on disk I/O"

#define readlink_trivial_usage \
	USE_FEATURE_READLINK_FOLLOW("[-f] ") "FILE"
#define readlink_full_usage "\n\n" \
       "Display the value of a symlink" \
	USE_FEATURE_READLINK_FOLLOW( "\n" \
     "\nOptions:" \
     "\n	-f	Canonicalize by following all symlinks") \

#define readprofile_trivial_usage \
       "[OPTIONS]..."
#define readprofile_full_usage "\n\n" \
       "Options:" \
     "\n	-m mapfile	(Default: /boot/System.map)" \
     "\n	-p profile	(Default: /proc/profile)" \
     "\n	-M mult		Set the profiling multiplier to mult" \
     "\n	-i		Print only info about the sampling step" \
     "\n	-v		Verbose" \
     "\n	-a		Print all symbols, even if count is 0" \
     "\n	-b		Print individual histogram-bin counts" \
     "\n	-s		Print individual counters within functions" \
     "\n	-r		Reset all the counters (root only)" \
     "\n	-n		Disable byte order auto-detection" \

#define realpath_trivial_usage \
       "pathname..."
#define realpath_full_usage "\n\n" \
       "Return the absolute pathnames of given argument"

#define reboot_trivial_usage \
       "[-d delay] [-n] [-f]"
#define reboot_full_usage "\n\n" \
       "Reboot the system\n" \
     "\nOptions:" \
     "\n	-d	Delay interval for rebooting" \
     "\n	-n	No call to sync()" \
     "\n	-f	Force reboot (don't go through init)" \

#define renice_trivial_usage \
       "{{-n INCREMENT} | PRIORITY} [[-p | -g | -u] ID...]"
#define renice_full_usage "\n\n" \
       "Change priority of running processes\n" \
     "\nOptions:" \
     "\n	-n	Adjust current nice value (smaller is faster)" \
     "\n	-p	Process id(s) (default)" \
     "\n	-g	Process group id(s)" \
     "\n	-u	Process user name(s) and/or id(s)" \

#define reset_trivial_usage \
       ""
#define reset_full_usage "\n\n" \
       "Reset the screen"

#define resize_trivial_usage \
       ""
#define resize_full_usage "\n\n" \
       "Resize the screen"

#define restorecon_trivial_usage \
       "[-iFnrRv] [-e excludedir]... [-o filename] [-f filename | pathname]"
#define restorecon_full_usage "\n\n" \
       "Reset security contexts of files in pathname\n" \
     "\n	-i		Ignore files that do not exist" \
     "\n	-f file		File with list of files to process. Use - for stdin" \
     "\n	-e directory	Directory to exclude" \
     "\n	-R,-r		Recurse directories" \
     "\n	-n		Don't change any file labels" \
     "\n	-o file		Save list of files with incorrect context" \
     "\n	-v		Verbose" \
     "\n	-vv		Show changed labels" \
     "\n	-F		Force reset of context to match file_context" \
     "\n			for customizable files, or the user section," \
     "\n			if it has changed" \

#define rm_trivial_usage \
       "[OPTION]... FILE..."
#define rm_full_usage "\n\n" \
       "Remove (unlink) the FILE(s). Use '--' to\n" \
       "indicate that all following arguments are non-options.\n" \
     "\nOptions:" \
     "\n	-i	Always prompt before removing" \
     "\n	-f	Never prompt" \
     "\n	-r,-R	Remove directories recursively" \

#define rm_example_usage \
       "$ rm -rf /tmp/foo\n"

#define rmdir_trivial_usage \
       "[OPTION]... DIRECTORY..."
#define rmdir_full_usage "\n\n" \
       "Remove the DIRECTORY, if it is empty"
#define rmdir_example_usage \
       "# rmdir /tmp/foo\n"

#define rmmod_trivial_usage \
       "[OPTION]... [MODULE]..."
#define rmmod_full_usage "\n\n" \
       "Unload the specified kernel modules from the kernel\n" \
     "\nOptions:" \
     "\n	-a	Remove all unused modules (recursively)" \

#define rmmod_example_usage \
       "$ rmmod tulip\n"

#define route_trivial_usage \
       "[{add|del|delete}]"
#define route_full_usage "\n\n" \
       "Edit the kernel's routing tables\n" \
     "\nOptions:" \
     "\n	-n	Don't resolve names" \
     "\n	-e	Display other/more information" \
     "\n	-A inet" USE_FEATURE_IPV6("{6}") "	Select address family" \

#define rpm_trivial_usage \
       "-i -q[ildc]p package.rpm"
#define rpm_full_usage "\n\n" \
       "Manipulate RPM packages\n" \
     "\nOptions:" \
     "\n	-i	Install package" \
     "\n	-q	Query package" \
     "\n	-p	Query uninstalled package" \
     "\n	-i	Show information" \
     "\n	-l	List contents" \
     "\n	-d	List documents" \
     "\n	-c	List config files" \

#define rpm2cpio_trivial_usage \
       "package.rpm"
#define rpm2cpio_full_usage "\n\n" \
       "Output a cpio archive of the rpm file"

#define rtcwake_trivial_usage \
       "[-a | -l | -u] [-d DEV] [-m MODE] [-s SECS | -t TIME]"
#define rtcwake_full_usage "\n\n" \
       "Enter a system sleep state until specified wakeup time\n" \
	USE_GETOPT_LONG( \
     "\n	-a,--auto	 Read clock mode from adjtime" \
     "\n	-l,--local	 Clock is set to local time" \
     "\n	-u,--utc	 Clock is set to UTC time" \
     "\n	-d,--device=DEV	 Specify the RTC device" \
     "\n	-m,--mode=MODE	 Set the sleep state (default: standby)" \
     "\n	-s,--seconds=SEC Set the timeout in SEC seconds from now" \
     "\n	-t,--time=TIME	 Set the timeout to TIME seconds from epoch" \
	) \
	SKIP_GETOPT_LONG( \
     "\n	-a	Read clock mode from adjtime" \
     "\n	-l	Clock is set to local time" \
     "\n	-u	Clock is set to UTC time" \
     "\n	-d DEV	Specify the RTC device" \
     "\n	-m MODE	Set the sleep state (default: standby)" \
     "\n	-s SEC	Set the timeout in SEC seconds from now" \
     "\n	-t TIME	Set the timeout to TIME seconds from epoch" \
	)

#define runcon_trivial_usage \
       "[-c] [-u USER] [-r ROLE] [-t TYPE] [-l RANGE] COMMAND [args]\n" \
       "	runcon CONTEXT COMMAND [args]"
#define runcon_full_usage "\n\n" \
       "Run a program in a different security context\n" \
     "\n	CONTEXT		Complete security context\n" \
	USE_GETOPT_LONG( \
     "\n	-c,--compute	Compute process transition context before modifying" \
     "\n	-t,--type=TYPE	Type (for same role as parent)" \
     "\n	-u,--user=USER	User identity" \
     "\n	-r,--role=ROLE	Role" \
     "\n	-l,--range=RNG	Levelrange" \
	) \
	SKIP_GETOPT_LONG( \
     "\n	-c	Compute process transition context before modifying" \
     "\n	-t TYPE	Type (for same role as parent)" \
     "\n	-u USER	User identity" \
     "\n	-r ROLE	Role" \
     "\n	-l RNG	Levelrange" \
	)

#define run_parts_trivial_usage \
       "[-t] "USE_FEATURE_RUN_PARTS_FANCY("[-l] ")"[-a ARG] [-u MASK] DIRECTORY"
#define run_parts_full_usage "\n\n" \
       "Run a bunch of scripts in a directory\n" \
     "\nOptions:" \
     "\n	-t	Print what would be run, but don't actually run anything" \
     "\n	-a ARG	Pass ARG as argument for every program" \
     "\n	-u MASK	Set the umask to MASK before running every program" \
	USE_FEATURE_RUN_PARTS_FANCY( \
     "\n	-l	Print names of all matching files even if they are not executable" \
	)

#define run_parts_example_usage \
       "$ run-parts -a start /etc/init.d\n" \
       "$ run-parts -a stop=now /etc/init.d\n\n" \
       "Let's assume you have a script foo/dosomething:\n" \
       "#!/bin/sh\n" \
       "for i in $*; do eval $i; done; unset i\n" \
       "case \"$1\" in\n" \
       "start*) echo starting something;;\n" \
       "stop*) set -x; shutdown -h $stop;;\n" \
       "esac\n\n" \
       "Running this yields:\n" \
       "$run-parts -a stop=+4m foo/\n" \
       "+ shutdown -h +4m"

#define runlevel_trivial_usage \
       "[utmp]"
#define runlevel_full_usage "\n\n" \
       "Find the current and previous system runlevel.\n\n" \
       "If no utmp file exists or if no runlevel record can be found,\n" \
       "print \"unknown\""
#define runlevel_example_usage \
       "$ runlevel /var/run/utmp\n" \
       "N 2"

#define runsv_trivial_usage \
       "dir"
#define runsv_full_usage "\n\n" \
       "Start and monitor a service and optionally an appendant log service"

#define runsvdir_trivial_usage \
       "[-P] dir"
#define runsvdir_full_usage "\n\n" \
       "Start a runsv process for each subdirectory"

#define rx_trivial_usage \
       "FILE"
#define rx_full_usage "\n\n" \
       "Receive a file using the xmodem protocol"
#define rx_example_usage \
       "$ rx /tmp/foo\n"

#define script_trivial_usage \
       "[-afq] [-c COMMAND] [OUTFILE]"
#define script_full_usage "\n\n" \
       "Options:" \
     "\n	-a	Append output" \
     "\n	-c	Run COMMAND, not shell" \
     "\n	-f	Flush output after each write" \
     "\n	-q	Quiet" \

#define sed_trivial_usage \
       "[-efinr] pattern [files...]"
#define sed_full_usage "\n\n" \
       "Options:" \
     "\n	-e script	Add the script to the commands to be executed" \
     "\n	-f scriptfile	Add scriptfile contents to the" \
     "\n			commands to be executed" \
     "\n	-i		Edit files in-place" \
     "\n	-n		Suppress automatic printing of pattern space" \
     "\n	-r		Use extended regular expression syntax" \
     "\n" \
     "\nIf no -e or -f is given, the first non-option argument is taken as the sed" \
     "\nscript to interpret. All remaining arguments are names of input files; if no" \
     "\ninput files are specified, then the standard input is read. Source files" \
     "\nwill not be modified unless -i option is given." \

#define sed_example_usage \
       "$ echo \"foo\" | sed -e 's/f[a-zA-Z]o/bar/g'\n" \
       "bar\n"

#define selinuxenabled_trivial_usage NOUSAGE_STR
#define selinuxenabled_full_usage ""

#define sendmail_trivial_usage \
       "[-w timeout] [-U user] [-P password] [-X]\n" \
       "-t to [-t to]... [-n] [-s subject] [-c charset] server[:port] from [body] [attachment ...]"
#define sendmail_full_usage "\n\n" \
       "Send an email.\n" \
     "\nOptions:" \
     "\n	-w timeout	Set timeout on network operations" \
     "\n	-U username	Authenticate with specified username/password" \
     "\n	-P password" \
     "\n	-t address	Recipient(s). May be repeated" \
     "\n	-X		Use openssl connection helper for secured servers" \
     "\n	-n		Request delivery notification to sender" \
     "\n	-s subject	Subject" \
     "\n	-c charset	Assumed charset for body and subject [utf-8]" \

#define seq_trivial_usage \
       "[first [increment]] last"
#define seq_full_usage "\n\n" \
       "Print numbers from FIRST to LAST, in steps of INCREMENT.\n" \
       "FIRST, INCREMENT default to 1" \
       "\n\nArguments:\n" \
       "	LAST\n" \
       "	FIRST LAST\n" \
       "	FIRST INCREMENT LAST"

#define sestatus_trivial_usage \
       "[-vb]"
#define sestatus_full_usage "\n\n" \
       "	-v	Verbose" \
     "\n	-b	Display current state of booleans" \

#define setconsole_trivial_usage \
       "[-r" USE_FEATURE_SETCONSOLE_LONG_OPTIONS("|--reset") "] [DEVICE]"
#define setconsole_full_usage "\n\n" \
       "Redirect system console output to DEVICE (default: /dev/tty)\n" \
     "\nOptions:" \
     "\n	-r	Reset output to /dev/console" \

#define setenforce_trivial_usage \
       "[Enforcing | Permissive | 1 | 0]"
#define setenforce_full_usage ""

#define setfiles_trivial_usage \
       "[-dnpqsvW] [-e dir]... [-o file] [-r alt_root_path]" \
	USE_FEATURE_SETFILES_CHECK_OPTION( \
       " [-c policyfile] spec_file" \
	) \
       " pathname"
#define setfiles_full_usage "\n\n" \
       "Reset file contexts under pathname according to spec_file\n" \
	USE_FEATURE_SETFILES_CHECK_OPTION( \
     "\n	-c file	Check the validity of the contexts against the specified binary policy" \
	) \
     "\n	-d	Show which specification matched each file" \
     "\n	-l	Log changes in file labels to syslog" \
     "\n	-n	Don't change any file labels" \
     "\n	-q	Suppress warnings" \
     "\n	-r dir	Use an altenate root path" \
     "\n	-e dir	Exclude directory" \
     "\n	-F	Force reset of context to match file_context for customizable files" \
     "\n	-o file	Save list of files with incorrect context" \
     "\n	-s	Take a list of files from standard input (instead of command line)" \
     "\n	-v	Show changes in file labels, if type or role are changing" \
     "\n	-vv	Show changes in file labels, if type, role, or user are changing" \
     "\n	-W	Display warnings about entries that had no matching files" \

#define setkeycodes_trivial_usage \
       "SCANCODE KEYCODE..."
#define setkeycodes_full_usage "\n\n" \
       "Set entries into the kernel's scancode-to-keycode map,\n" \
       "allowing unusual keyboards to generate usable keycodes.\n\n" \
       "SCANCODE may be either xx or e0xx (hexadecimal),\n" \
       "and KEYCODE is given in decimal" \

#define setkeycodes_example_usage \
       "$ setkeycodes e030 127\n"

#define setlogcons_trivial_usage \
       "N"
#define setlogcons_full_usage "\n\n" \
       "Redirect the kernel output to console N (0 for current)"

#define setsebool_trivial_usage \
       "boolean value"

#define setsebool_full_usage "\n\n" \
       "Change boolean setting"

#define setsid_trivial_usage \
       "PROG [ARG...]"
#define setsid_full_usage "\n\n" \
       "Run PROG in a new session. PROG will have no controlling terminal\n" \
       "and will not be affected by keyboard signals (Ctrl-C etc).\n" \
       "See setsid(2) for details." \

#define lash_trivial_usage \
       "[FILE]...\n" \
       "or: sh -c command [args]..."
#define lash_full_usage "\n\n" \
       "lash is deprecated, please use hush"

#define last_trivial_usage \
       ""USE_FEATURE_LAST_FANCY("[-HW] [-f file]")
#define last_full_usage "\n\n" \
       "Show listing of the last users that logged into the system" \
	USE_FEATURE_LAST_FANCY( "\n" \
     "\nOptions:" \
/*   "\n	-H	Show header line" */ \
     "\n	-W	Display with no host column truncation" \
     "\n	-f file Read from file instead of /var/log/wtmp" \
	)

#define sha1sum_trivial_usage \
       "[OPTION] [FILEs...]" \
	USE_FEATURE_MD5_SHA1_SUM_CHECK("\n   or: sha1sum [OPTION] -c [FILE]")
#define sha1sum_full_usage "\n\n" \
       "Print" USE_FEATURE_MD5_SHA1_SUM_CHECK(" or check") " SHA1 checksums." \
	USE_FEATURE_MD5_SHA1_SUM_CHECK( "\n" \
     "\nOptions:" \
     "\n	-c	Check SHA1 sums against given list" \
     "\n	-s	Don't output anything, status code shows success" \
     "\n	-w	Warn about improperly formatted SHA1 checksum lines" \
	)

#define slattach_trivial_usage \
       "[-cehmLF] [-s speed] [-p protocol] DEVICEs"
#define slattach_full_usage "\n\n" \
       "Attach network interface(s) to serial line(s)\n" \
     "\nOptions:" \
     "\n	-p	Set protocol (slip, cslip, slip6, clisp6 or adaptive)" \
     "\n	-s	Set line speed" \
     "\n	-e	Exit after initializing device" \
     "\n	-h	Exit when the carrier is lost" \
     "\n	-c	Execute a command when the line is hung up" \
     "\n	-m	Do NOT initialize the line in raw 8 bits mode" \
     "\n	-L	Enable 3-wire operation" \
     "\n	-F	Disable RTS/CTS flow control" \

#define sleep_trivial_usage \
	USE_FEATURE_FANCY_SLEEP("[") "N" USE_FEATURE_FANCY_SLEEP("]...")
#define sleep_full_usage "\n\n" \
	SKIP_FEATURE_FANCY_SLEEP("Pause for N seconds") \
	USE_FEATURE_FANCY_SLEEP( \
       "Pause for a time equal to the total of the args given, where each arg can\n" \
       "have an optional suffix of (s)econds, (m)inutes, (h)ours, or (d)ays")
#define sleep_example_usage \
       "$ sleep 2\n" \
       "[2 second delay results]\n" \
	USE_FEATURE_FANCY_SLEEP( \
       "$ sleep 1d 3h 22m 8s\n" \
       "[98528 second delay results]\n")

#define sort_trivial_usage \
       "[-nru" \
	USE_FEATURE_SORT_BIG("gMcszbdfimSTokt] [-o FILE] [-k start[.offset][opts][,end[.offset][opts]] [-t CHAR") \
       "] [FILE]..."
#define sort_full_usage "\n\n" \
       "Sort lines of text\n" \
     "\nOptions:" \
	USE_FEATURE_SORT_BIG( \
     "\n	-b	Ignore leading blanks" \
     "\n	-c	Check whether input is sorted" \
     "\n	-d	Dictionary order (blank or alphanumeric only)" \
     "\n	-f	Ignore case" \
     "\n	-g	General numerical sort" \
     "\n	-i	Ignore unprintable characters" \
     "\n	-k	Sort key" \
     "\n	-M	Sort month" \
	) \
     "\n	-n	Sort numbers" \
	USE_FEATURE_SORT_BIG( \
     "\n	-o	Output to file" \
     "\n	-k	Sort by key" \
     "\n	-t CHAR	Key separator" \
	) \
     "\n	-r	Reverse sort order" \
	USE_FEATURE_SORT_BIG( \
     "\n	-s	Stable (don't sort ties alphabetically)" \
	) \
     "\n	-u	Suppress duplicate lines" \
	USE_FEATURE_SORT_BIG( \
     "\n	-z	Lines are terminated by NUL, not newline" \
     "\n	-mST	Ignored for GNU compatibility") \

#define sort_example_usage \
       "$ echo -e \"e\\nf\\nb\\nd\\nc\\na\" | sort\n" \
       "a\n" \
       "b\n" \
       "c\n" \
       "d\n" \
       "e\n" \
       "f\n" \
	USE_FEATURE_SORT_BIG( \
		"$ echo -e \"c 3\\nb 2\\nd 2\" | $SORT -k 2,2n -k 1,1r\n" \
		"d 2\n" \
		"b 2\n" \
		"c 3\n" \
	) \
       ""

#define split_trivial_usage \
       "[OPTION] [INPUT [PREFIX]]"
#define split_full_usage "\n\n" \
       "Options:" \
     "\n	-b n[k|m]	Split by bytes" \
     "\n	-l n		Split by lines" \
     "\n	-a n		Use n letters as suffix" \

#define split_example_usage \
       "$ split TODO foo\n" \
       "$ cat TODO | split -a 2 -l 2 TODO_\n"

#define start_stop_daemon_trivial_usage \
       "[OPTIONS] [-S|-K] ... [-- arguments...]"
#define start_stop_daemon_full_usage "\n\n" \
       "Search for matching processes, and then\n" \
       "-S: stop all matching processes.\n" \
       "-K: start a process unless a matching process is found.\n" \
	USE_GETOPT_LONG( \
     "\nProcess matching:" \
     "\n	-u,--user USERNAME|UID	Match only this user's processes" \
     "\n	-n,--name NAME		Match processes with NAME" \
     "\n				in comm field in /proc/PID/stat" \
     "\n	-x,--exec EXECUTABLE	Match processes with this command" \
     "\n				in /proc/PID/cmdline" \
     "\n	-p,--pidfile FILE	Match a process with PID from the file" \
     "\n	All specified conditions must match" \
     "\n-K only:" \
     "\n	-x,--exec EXECUTABLE	Program to run" \
     "\n	-a,--startas NAME	Zeroth argument" \
     "\n	-b,--background		Background" \
	USE_FEATURE_START_STOP_DAEMON_FANCY( \
     "\n	-N,--nicelevel N	Change nice level" \
	) \
     "\n	-c,--chuid USER[:[GRP]]	Change to user/group" \
     "\n	-m,--make-pidfile	Write PID to the pidfile specified by -p" \
     "\n-S only:" \
     "\n	-s,--signal SIG		Signal to send" \
     "\n	-t,--test		Match only, exit with 0 if a process is found" \
     "\nOther:" \
	USE_FEATURE_START_STOP_DAEMON_FANCY( \
     "\n	-o,--oknodo		Exit with status 0 if nothing is done" \
     "\n	-q,--quiet		Quiet" \
	) \
     "\n	-v,--verbose		Verbose" \
	) \
	SKIP_GETOPT_LONG( \
     "\nProcess matching:" \
     "\n	-u USERNAME|UID	Match only this user's processes" \
     "\n	-n NAME		Match processes with NAME" \
     "\n			in comm field in /proc/PID/stat" \
     "\n	-x EXECUTABLE	Match processes with this command" \
     "\n			command in /proc/PID/cmdline" \
     "\n	-p FILE		Match a process with PID from the file" \
     "\n	All specified conditions must match" \
     "\n-K only:" \
     "\n	-x EXECUTABLE	Program to run" \
     "\n	-a NAME		Zeroth argument" \
     "\n	-b		Background" \
	USE_FEATURE_START_STOP_DAEMON_FANCY( \
     "\n	-N N		Change nice level" \
	) \
     "\n	-c USER[:[GRP]]	Change to user/group" \
     "\n	-m		Write PID to the pidfile specified by -p" \
     "\n-S only:" \
     "\n	-s SIG		Signal to send" \
     "\n	-t		Match only, exit with 0 if a process is found" \
     "\nOther:" \
	USE_FEATURE_START_STOP_DAEMON_FANCY( \
     "\n	-o		Exit with status 0 if nothing is done" \
     "\n	-q		Quiet" \
	) \
     "\n	-v		Verbose" \
	) \

#define stat_trivial_usage \
       "[OPTION] FILE..."
#define stat_full_usage "\n\n" \
       "Display file (default) or filesystem status\n" \
     "\nOptions:" \
	USE_FEATURE_STAT_FORMAT( \
     "\n	-c fmt	Use the specified format" \
	) \
     "\n	-f	Display filesystem status" \
     "\n	-L	Dereference links" \
     "\n	-t	Display info in terse form" \
	USE_SELINUX( \
     "\n	-Z	Print security context" \
	) \
	USE_FEATURE_STAT_FORMAT( \
       "\n\nValid format sequences for files:\n" \
       " %a	Access rights in octal\n" \
       " %A	Access rights in human readable form\n" \
       " %b	Number of blocks allocated (see %B)\n" \
       " %B	The size in bytes of each block reported by %b\n" \
       " %d	Device number in decimal\n" \
       " %D	Device number in hex\n" \
       " %f	Raw mode in hex\n" \
       " %F	File type\n" \
       " %g	Group ID of owner\n" \
       " %G	Group name of owner\n" \
       " %h	Number of hard links\n" \
       " %i	Inode number\n" \
       " %n	File name\n" \
       " %N	Quoted file name with dereference if symlink\n" \
       " %o	I/O block size\n" \
       " %s	Total size, in bytes\n" \
       " %t	Major device type in hex\n" \
       " %T	Minor device type in hex\n" \
       " %u	User ID of owner\n" \
       " %U	User name of owner\n" \
       " %x	Time of last access\n" \
       " %X	Time of last access as seconds since Epoch\n" \
       " %y	Time of last modification\n" \
       " %Y	Time of last modification as seconds since Epoch\n" \
       " %z	Time of last change\n" \
       " %Z	Time of last change as seconds since Epoch\n" \
       "\nValid format sequences for file systems:\n" \
       " %a	Free blocks available to non-superuser\n" \
       " %b	Total data blocks in file system\n" \
       " %c	Total file nodes in file system\n" \
       " %d	Free file nodes in file system\n" \
       " %f	Free blocks in file system\n" \
	USE_SELINUX( \
       " %C	Security context in SELinux\n" \
	) \
       " %i	File System ID in hex\n" \
       " %l	Maximum length of filenames\n" \
       " %n	File name\n" \
       " %s	Block size (for faster transfer)\n" \
       " %S	Fundamental block size (for block counts)\n" \
       " %t	Type in hex\n" \
       " %T	Type in human readable form" \
	)

#define strings_trivial_usage \
       "[-afo] [-n length] [file...]"
#define strings_full_usage "\n\n" \
       "Display printable strings in a binary file\n" \
     "\nOptions:" \
     "\n	-a	Scan whole file (default)" \
     "\n	-f	Precede strings with filenames" \
     "\n	-n N	At least N characters form a string (default 4)" \
     "\n	-o	Precede strings with decimal offsets" \

#define stty_trivial_usage \
       "[-a|g] [-F DEVICE] [SETTING]..."
#define stty_full_usage "\n\n" \
       "Without arguments, prints baud rate, line discipline,\n" \
       "and deviations from stty sane\n" \
     "\nOptions:" \
     "\n	-F DEVICE	Open device instead of stdin" \
     "\n	-a		Print all current settings in human-readable form" \
     "\n	-g		Print in stty-readable form" \
     "\n	[SETTING]	See manpage" \

#define su_trivial_usage \
       "[OPTION]... [-] [username]"
#define su_full_usage "\n\n" \
       "Change user id or become root\n" \
     "\nOptions:" \
     "\n	-p, -m	Preserve environment" \
     "\n	-c	Command to pass to 'sh -c'" \
     "\n	-s	Shell to use instead of default shell" \

#define sulogin_trivial_usage \
       "[OPTION]... [tty-device]"
#define sulogin_full_usage "\n\n" \
       "Single user login\n" \
     "\nOptions:" \
     "\n	-t	Timeout" \

#define sum_trivial_usage \
       "[rs] [files...]"
#define sum_full_usage "\n\n" \
       "Checksum and count the blocks in a file\n" \
     "\nOptions:" \
     "\n	-r	Use BSD sum algorithm (1K blocks)" \
     "\n	-s	Use System V sum algorithm (512byte blocks)" \

#define sv_trivial_usage \
       "[-v] [-w sec] command service..."
#define sv_full_usage "\n\n" \
       "Control services monitored by runsv supervisor.\n" \
       "Commands (only first character is enough):\n" \
       "\n" \
       "status: query service status\n" \
       "up: if service isn't running, start it. If service stops, restart it\n" \
       "once: like 'up', but if service stops, don't restart it\n" \
       "down: send TERM and CONT signals. If ./run exits, start ./finish\n" \
       "    if it exists. After it stops, do not restart service\n" \
       "exit: send TERM and CONT signals to service and log service. If they exit,\n" \
       "    runsv exits too\n" \
       "pause, cont, hup, alarm, interrupt, quit, 1, 2, term, kill: send\n" \
       "STOP, CONT, HUP, ALRM, INT, QUIT, USR1, USR2, TERM, KILL signal to service" \

#define svlogd_trivial_usage \
       "[-ttv] [-r c] [-R abc] [-l len] [-b buflen] dir..."
#define svlogd_full_usage "\n\n" \
       "Continuously read log data from standard input, optionally " \
       "filter log messages, and write the data to one or more automatically " \
       "rotated logs" \

#define swapoff_trivial_usage \
       "[-a] [DEVICE]"
#define swapoff_full_usage "\n\n" \
       "Stop swapping on DEVICE\n" \
     "\nOptions:" \
     "\n	-a	Stop swapping on all swap devices" \

#define swapon_trivial_usage \
       "[-a]" USE_FEATURE_SWAPON_PRI(" [-p pri]") " [DEVICE]"
#define swapon_full_usage "\n\n" \
       "Start swapping on DEVICE\n" \
     "\nOptions:" \
     "\n	-a	Start swapping on all swap devices" \
	USE_FEATURE_SWAPON_PRI( \
     "\n	-p pri	Set swap device priority" \
	) \

#define switch_root_trivial_usage \
       "[-c /dev/console] NEW_ROOT NEW_INIT [ARGUMENTS_TO_INIT]"
#define switch_root_full_usage "\n\n" \
       "Use from PID 1 under initramfs to free initramfs, chroot to NEW_ROOT,\n" \
       "and exec NEW_INIT\n" \
     "\nOptions:" \
     "\n	-c	Redirect console to device on new root" \

#define sync_trivial_usage \
       ""
#define sync_full_usage "\n\n" \
       "Write all buffered filesystem blocks to disk"

#define sysctl_trivial_usage \
       "[OPTIONS]... [VALUE]..."
#define sysctl_full_usage "\n\n" \
       "Configure kernel parameters at runtime\n" \
     "\nOptions:" \
     "\n	-n	Disable printing of key names" \
     "\n	-e	Don't warn about unknown keys" \
     "\n	-w	Change sysctl setting" \
     "\n	-p FILE	Load sysctl settings from FILE (default /etc/sysctl.conf)" \
     "\n	-a	Display all values" \
     "\n	-A	Display all values in table form" \

#define sysctl_example_usage \
       "sysctl [-n] [-e] variable...\n" \
       "sysctl [-n] [-e] -w variable=value...\n" \
       "sysctl [-n] [-e] -a\n" \
       "sysctl [-n] [-e] -p file	(default /etc/sysctl.conf)\n" \
       "sysctl [-n] [-e] -A\n"

#define syslogd_trivial_usage \
       "[OPTION]..."
#define syslogd_full_usage "\n\n" \
       "System logging utility.\n" \
       "Note that this version of syslogd ignores /etc/syslog.conf.\n" \
     "\nOptions:" \
     "\n	-n		Run in foreground" \
     "\n	-O FILE		Log to given file (default=/var/log/messages)" \
     "\n	-l n		Set local log level" \
     "\n	-S		Smaller logging output" \
	USE_FEATURE_ROTATE_LOGFILE( \
     "\n	-s SIZE		Max size (KB) before rotate (default=200KB, 0=off)" \
     "\n	-b NUM		Number of rotated logs to keep (default=1, max=99, 0=purge)") \
	USE_FEATURE_REMOTE_LOG( \
     "\n	-R HOST[:PORT]	Log to IP or hostname on PORT (default PORT=514/UDP)" \
     "\n	-L		Log locally and via network (default is network only if -R)") \
	USE_FEATURE_SYSLOGD_DUP( \
     "\n	-D		Drop duplicates") \
	USE_FEATURE_IPC_SYSLOG( \
     "\n	-C[size(KiB)]	Log to shared mem buffer (read it using logread)") \
	/* NB: -Csize shouldn't have space (because size is optional) */
/*   "\n	-m MIN		Minutes between MARK lines (default=20, 0=off)" */

#define syslogd_example_usage \
       "$ syslogd -R masterlog:514\n" \
       "$ syslogd -R 192.168.1.1:601\n"

#define tac_trivial_usage \
	"[FILE]..."
#define tac_full_usage "\n\n" \
	"Concatenate FILE(s) and print them in reverse"

#define tail_trivial_usage \
       "[OPTION]... [FILE]..."
#define tail_full_usage "\n\n" \
       "Print last 10 lines of each FILE to standard output.\n" \
       "With more than one FILE, precede each with a header giving the\n" \
       "file name. With no FILE, or when FILE is -, read standard input.\n" \
     "\nOptions:" \
	USE_FEATURE_FANCY_TAIL( \
     "\n	-c N[kbm]	Output the last N bytes") \
     "\n	-n N[kbm]	Print last N lines instead of last 10" \
     "\n	-f		Output data as the file grows" \
	USE_FEATURE_FANCY_TAIL( \
     "\n	-q		Never output headers giving file names" \
     "\n	-s SEC		Wait SEC seconds between reads with -f" \
     "\n	-v		Always output headers giving file names" \
     "\n\n" \
       "If the first character of N (bytes or lines) is a '+', output begins with\n" \
       "the Nth item from the start of each file, otherwise, print the last N items\n" \
       "in the file. N bytes may be suffixed by k (x1024), b (x512), or m (1024^2)." ) \

#define tail_example_usage \
       "$ tail -n 1 /etc/resolv.conf\n" \
       "nameserver 10.0.0.1\n"

#define tar_trivial_usage \
       "-[" USE_FEATURE_TAR_CREATE("c") USE_FEATURE_TAR_GZIP("z") \
	USE_FEATURE_TAR_BZIP2("j") USE_FEATURE_TAR_LZMA("a") \
	USE_FEATURE_TAR_COMPRESS("Z") "xtvO] " \
	USE_FEATURE_TAR_FROM("[-X FILE] ") \
       "[-f TARFILE] [-C DIR] [FILE(s)]..."
#define tar_full_usage "\n\n" \
       "Create, extract, or list files from a tar file\n" \
     "\nOptions:" \
	USE_FEATURE_TAR_CREATE( \
     "\n	c	Create") \
     "\n	x	Extract" \
     "\n	t	List" \
     "\nArchive format selection:" \
	USE_FEATURE_TAR_GZIP( \
     "\n	z	Filter the archive through gzip" \
	) \
	USE_FEATURE_TAR_BZIP2( \
     "\n	j	Filter the archive through bzip2" \
	) \
	USE_FEATURE_TAR_LZMA( \
     "\n	a	Filter the archive through lzma" \
	) \
	USE_FEATURE_TAR_COMPRESS( \
     "\n	Z	Filter the archive through compress" \
	) \
     "\nFile selection:" \
     "\n	f	Name of TARFILE or \"-\" for stdin" \
     "\n	O	Extract to stdout" \
	USE_FEATURE_TAR_FROM( \
     "\n	exclude	File to exclude" \
     "\n	X	File with names to exclude" \
	) \
     "\n	C	Change to directory DIR before operation" \
     "\n	v	Verbose" \

#define tar_example_usage \
       "$ zcat /tmp/tarball.tar.gz | tar -xf -\n" \
       "$ tar -cf /tmp/tarball.tar /usr/local\n"

#define taskset_trivial_usage \
       "[-p] [mask] [pid | command [arg]...]"
#define taskset_full_usage "\n\n" \
       "Set or get CPU affinity\n" \
     "\nOptions:" \
     "\n	-p	Operate on an existing PID" \

#define taskset_example_usage \
       "$ taskset 0x7 ./dgemm_test&\n" \
       "$ taskset -p 0x1 $!\n" \
       "pid 4790's current affinity mask: 7\n" \
       "pid 4790's new affinity mask: 1\n" \
       "$ taskset 0x7 /bin/sh -c './taskset -p 0x1 $$'\n" \
       "pid 6671's current affinity mask: 1\n" \
       "pid 6671's new affinity mask: 1\n" \
       "$ taskset -p 1\n" \
       "pid 1's current affinity mask: 3\n"

#define tee_trivial_usage \
       "[OPTION]... [FILE]..."
#define tee_full_usage "\n\n" \
       "Copy standard input to each FILE, and also to standard output\n" \
     "\nOptions:" \
     "\n	-a	Append to the given FILEs, do not overwrite" \
     "\n	-i	Ignore interrupt signals (SIGINT)" \

#define tee_example_usage \
       "$ echo \"Hello\" | tee /tmp/foo\n" \
       "$ cat /tmp/foo\n" \
       "Hello\n"

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
#define telnet_trivial_usage \
       "[-a] [-l USER] HOST [PORT]"
#define telnet_full_usage "\n\n" \
       "Connect to telnet server\n" \
     "\nOptions:" \
     "\n	-a	Attempt an automatic login with USER variable" \
     "\n	-l USER	Attempt an automatic login with USER argument" \

#else
#define telnet_trivial_usage \
       "HOST [PORT]"
#define telnet_full_usage "\n\n" \
       "Connect to telnet server"
#endif

#define telnetd_trivial_usage \
       "[OPTION]"
#define telnetd_full_usage "\n\n" \
       "Handle incoming telnet connections" \
	SKIP_FEATURE_TELNETD_STANDALONE(" via inetd") "\n" \
     "\nOptions:" \
     "\n	-l LOGIN	Exec LOGIN on connect" \
     "\n	-f issue_file	Display issue_file instead of /etc/issue" \
     "\n	-K		Close connection as soon as login exits" \
     "\n			(normally wait until all programs close slave pty)" \
	USE_FEATURE_TELNETD_STANDALONE( \
     "\n	-p PORT		Port to listen on" \
     "\n	-b ADDR		Address to bind to" \
     "\n	-F		Run in foreground" \
     "\n	-i		Run as inetd subservice" \
	)

#define test_trivial_usage \
       "EXPRESSION\n" \
       "  or   [ EXPRESSION ]"
#define test_full_usage "\n\n" \
       "Check file types and compares values returning an exit code\n" \
       "determined by the value of EXPRESSION"
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

#define tcpsvd_trivial_usage \
       "[-hEv] [-c n] [-C n:msg] [-b n] [-u user] [-l name] ip port prog..."
/* with not-implemented options: */
/*     "[-hpEvv] [-c n] [-C n:msg] [-b n] [-u user] [-l name] [-i dir|-x cdb] [-t sec] ip port prog..." */
#define tcpsvd_full_usage "\n\n" \
       "Create TCP socket, bind it to ip:port and listen\n" \
       "for incoming connection. Run PROG for each connection.\n" \
     "\nip		IP to listen on. '0' = all" \
     "\nport		Port to listen on" \
     "\nprog [arg]	Program to run" \
     "\n-l name		Local hostname (else looks up local hostname in DNS)" \
     "\n-u user[:group]	Change to user/group after bind" \
     "\n-c n		Handle up to n connections simultaneously" \
     "\n-b n		Allow a backlog of approximately n TCP SYNs" \
     "\n-C n[:msg]	Allow only up to n connections from the same IP" \
     "\n		New connections from this IP address are closed" \
     "\n		immediately. 'msg' is written to the peer before close" \
     "\n-h		Look up peer's hostname" \
     "\n-E		Do not set up environment variables" \
     "\n-v		Verbose" \

#define udpsvd_trivial_usage \
       "[-hEv] [-c n] [-u user] [-l name] ip port prog"
#define udpsvd_full_usage "\n\n" \
       "Create UDP socket, bind it to ip:port and wait\n" \
       "for incoming packets. Run PROG for each packet,\n" \
       "redirecting all further packets with same peer ip:port to it\n" \
     "\nip		IP to listen on. '0' = all" \
     "\nport		Port to listen on" \
     "\nprog [arg]	Program to run" \
     "\n-l name		Local hostname (else looks up local hostname in DNS)" \
     "\n-u user[:group]	Change to user/group after bind" \
     "\n-c n		Handle up to n connections simultaneously" \
     "\n-h		Look up peer's hostname" \
     "\n-E		Do not set up environment variables" \
     "\n-v		Verbose" \

#define tftp_trivial_usage \
       "[OPTION]... HOST [PORT]"
#define tftp_full_usage "\n\n" \
       "Transfer a file from/to tftp server\n" \
     "\nOptions:" \
     "\n	-l FILE	Local FILE" \
     "\n	-r FILE	Remote FILE" \
	USE_FEATURE_TFTP_GET( \
     "\n	-g	Get file" \
	) \
	USE_FEATURE_TFTP_PUT( \
     "\n	-p	Put file" \
	) \
	USE_FEATURE_TFTP_BLOCKSIZE( \
     "\n	-b SIZE	Transfer blocks of SIZE octets" \
	)

#define tftpd_trivial_usage \
       "[-cr] [-u USER] [DIR]"
#define tftpd_full_usage "\n\n" \
       "Transfer a file on tftp client's request.\n" \
     "\nOptions:" \
     "\n	-r	Prohibit upload" \
     "\n	-c	Allow file creation via upload" \
     "\n	-u	Access files as USER" \

#define time_trivial_usage \
       "[OPTION]... COMMAND [ARGS...]"
#define time_full_usage "\n\n" \
       "Run the program COMMAND with arguments ARGS. When COMMAND finishes,\n" \
       "COMMAND's resource usage information is displayed.\n" \
     "\nOptions:" \
     "\n	-v	Verbose" \

#define top_trivial_usage \
       "[-b] [-n COUNT] [-d SECONDS]"
#define top_full_usage "\n\n" \
       "Provide a view of process activity in real time.\n" \
       "Read the status of all processes from /proc each SECONDS\n" \
       "and show the status for however many processes will fit on the screen." \

#define touch_trivial_usage \
       "[-c] FILE [FILE...]"
#define touch_full_usage "\n\n" \
       "Update the last-modified date on the given FILE[s]\n" \
     "\nOptions:" \
     "\n	-c	Do not create any files" \

#define touch_example_usage \
       "$ ls -l /tmp/foo\n" \
       "/bin/ls: /tmp/foo: No such file or directory\n" \
       "$ touch /tmp/foo\n" \
       "$ ls -l /tmp/foo\n" \
       "-rw-rw-r--    1 andersen andersen        0 Apr 15 01:11 /tmp/foo\n"

#define tr_trivial_usage \
       "[-cds] STRING1 [STRING2]"
#define tr_full_usage "\n\n" \
       "Translate, squeeze, and/or delete characters from\n" \
       "standard input, writing to standard output\n" \
     "\nOptions:" \
     "\n	-c	Take complement of STRING1" \
     "\n	-d	Delete input characters coded STRING1" \
     "\n	-s	Squeeze multiple output characters of STRING2 into one character" \

#define tr_example_usage \
       "$ echo \"gdkkn vnqkc\" | tr [a-y] [b-z]\n" \
       "hello world\n"

#define traceroute_trivial_usage \
       "[-FIldnrv] [-f 1st_ttl] [-m max_ttl] [-p port#] [-q nqueries]\n" \
       "	[-s src_addr] [-t tos] [-w wait] [-g gateway] [-i iface]\n" \
       "	[-z pausemsecs] HOST [data size]"
#define traceroute_full_usage "\n\n" \
       "Trace the route to HOST\n" \
     "\nOptions:" \
     "\n	-F	Set the don't fragment bit" \
     "\n	-I	Use ICMP ECHO instead of UDP datagrams" \
     "\n	-l	Display the ttl value of the returned packet" \
     "\n	-d	Set SO_DEBUG options to socket" \
     "\n	-n	Print hop addresses numerically rather than symbolically" \
     "\n	-r	Bypass the normal routing tables and send directly to a host" \
     "\n	-v	Verbose" \
     "\n	-m max_ttl	Max time-to-live (max number of hops)" \
     "\n	-p port#	Base UDP port number used in probes" \
     "\n			(default is 33434)" \
     "\n	-q nqueries	Number of probes per 'ttl' (default 3)" \
     "\n	-s src_addr	IP address to use as the source address" \
     "\n	-t tos		Type-of-service in probe packets (default 0)" \
     "\n	-w wait		Time in seconds to wait for a response" \
     "\n			(default 3 sec)" \
     "\n	-g		Loose source route gateway (8 max)" \

#define true_trivial_usage \
       ""
#define true_full_usage "\n\n" \
       "Return an exit code of TRUE (0)"
#define true_example_usage \
       "$ true\n" \
       "$ echo $?\n" \
       "0\n"

#define tty_trivial_usage \
       ""
#define tty_full_usage "\n\n" \
       "Print file name of standard input's terminal" \
	USE_INCLUDE_SUSv2( "\n" \
     "\nOptions:" \
     "\n	-s	Print nothing, only return exit status" \
	)
#define tty_example_usage \
       "$ tty\n" \
       "/dev/tty2\n"

#define ttysize_trivial_usage \
       "[w] [h]"
#define ttysize_full_usage "\n\n" \
       "Print dimension(s) of standard input's terminal, on error return 80x25"

#define tune2fs_trivial_usage \
       "[-c max-mounts-count] [-e errors-behavior] [-g group] " \
       "[-i interval[d|m|w]] [-j] [-J journal-options] [-l] [-s sparse-flag] " \
       "[-m reserved-blocks-percent] [-o [^]mount-options[,...]] " \
       "[-r reserved-blocks-count] [-u user] [-C mount-count] " \
       "[-L volume-label] [-M last-mounted-dir] [-O [^]feature[,...]] " \
       "[-T last-check-time] [-U UUID] device"
#define tune2fs_full_usage "\n\n" \
       "Adjust filesystem options on ext[23] filesystems"

#define udhcpc_trivial_usage \
       "[-Cfbnqtvo] [-c CID] [-V VCLS] [-H HOSTNAME] [-i INTERFACE]\n" \
       "	[-p pidfile] [-r IP] [-s script] [-O dhcp-option]..." USE_FEATURE_UDHCP_PORT(" [-P N]")
#define udhcpc_full_usage "\n\n" \
	USE_GETOPT_LONG( \
       "	-V,--vendorclass=CLASSID	Vendor class identifier" \
     "\n	-i,--interface=INTERFACE	Interface to use (default eth0)" \
     "\n	-H,-h,--hostname=HOSTNAME	Client hostname" \
     "\n	-c,--clientid=CLIENTID	Client identifier" \
     "\n	-C,--clientid-none	Suppress default client identifier" \
     "\n	-p,--pidfile=file	Create pidfile" \
     "\n	-r,--request=IP		IP address to request" \
     "\n	-s,--script=file	Run file at DHCP events (default "CONFIG_DHCPC_DEFAULT_SCRIPT")" \
     "\n	-t,--retries=N		Send up to N request packets" \
     "\n	-T,--timeout=N		Try to get a lease for N seconds (default 3)" \
     "\n	-A,--tryagain=N		Wait N seconds (default 20) after failure" \
     "\n	-O,--request-option=OPT	Request DHCP option OPT (cumulative)" \
     "\n	-o,--no-default-options	Do not request any options (unless -O is also given)" \
     "\n	-f,--foreground	Run in foreground" \
	USE_FOR_MMU( \
     "\n	-b,--background	Background if lease is not immediately obtained" \
	) \
     "\n	-S,--syslog	Log to syslog too" \
     "\n	-n,--now	Exit with failure if lease is not immediately obtained" \
     "\n	-q,--quit	Quit after obtaining lease" \
     "\n	-R,--release	Release IP on quit" \
	USE_FEATURE_UDHCP_PORT( \
     "\n	-P,--client-port N  Use port N instead of default 68" \
	) \
	USE_FEATURE_UDHCPC_ARPING( \
     "\n	-a,--arping	Use arping to validate offered address" \
	) \
	) \
	SKIP_GETOPT_LONG( \
       "	-V CLASSID	Vendor class identifier" \
     "\n	-i INTERFACE	Interface to use (default: eth0)" \
     "\n	-H,-h HOSTNAME	Client hostname" \
     "\n	-c CLIENTID	Client identifier" \
     "\n	-C		Suppress default client identifier" \
     "\n	-p file		Create pidfile" \
     "\n	-r IP		IP address to request" \
     "\n	-s file		Run file at DHCP events (default "CONFIG_DHCPC_DEFAULT_SCRIPT")" \
     "\n	-t N		Send up to N request packets" \
     "\n	-T N		Try to get a lease for N seconds (default 3)" \
     "\n	-A N		Wait N seconds (default 20) after failure" \
     "\n	-O OPT		Request DHCP option OPT (cumulative)" \
     "\n	-o		Do not request any options (unless -O is also given)" \
     "\n	-f		Run in foreground" \
	USE_FOR_MMU( \
     "\n	-b		Background if lease is not immediately obtained" \
	) \
     "\n	-S		Log to syslog too" \
     "\n	-n		Exit with failure if lease is not immediately obtained" \
     "\n	-q		Quit after obtaining lease" \
     "\n	-R		Release IP on quit" \
	USE_FEATURE_UDHCP_PORT( \
     "\n	-P N		Use port N instead of default 68" \
	) \
	USE_FEATURE_UDHCPC_ARPING( \
     "\n	-a		Use arping to validate offered address" \
	) \
	)

#define udhcpd_trivial_usage \
       "[-fS]" USE_FEATURE_UDHCP_PORT(" [-P N]") " [configfile]" \

#define udhcpd_full_usage "\n\n" \
       "DHCP server\n" \
     "\n	-f	Run in foreground" \
     "\n	-S	Log to syslog too" \
	USE_FEATURE_UDHCP_PORT( \
     "\n	-P N	Use port N instead of default 67" \
	)

#define umount_trivial_usage \
       "[flags] FILESYSTEM|DIRECTORY"
#define umount_full_usage "\n\n" \
       "Unmount file systems\n" \
     "\nOptions:" \
	USE_FEATURE_UMOUNT_ALL( \
     "\n	-a	Unmount all file systems" USE_FEATURE_MTAB_SUPPORT(" in /etc/mtab") \
	) \
	USE_FEATURE_MTAB_SUPPORT( \
     "\n	-n	Don't erase /etc/mtab entries" \
	) \
     "\n	-r	Try to remount devices as read-only if mount is busy" \
     "\n	-l	Lazy umount (detach filesystem)" \
     "\n	-f	Force umount (i.e., unreachable NFS server)" \
	USE_FEATURE_MOUNT_LOOP( \
     "\n	-d	Free loop device if it has been used" \
	)

#define umount_example_usage \
       "$ umount /dev/hdc1\n"

#define uname_trivial_usage \
       "[-amnrspv]"
#define uname_full_usage "\n\n" \
       "Print system information.\n" \
     "\nOptions:" \
     "\n	-a	Print all" \
     "\n	-m	The machine (hardware) type" \
     "\n	-n	Hostname" \
     "\n	-r	OS release" \
     "\n	-s	OS name (default)" \
     "\n	-p	Processor type" \
     "\n	-v	OS version" \

#define uname_example_usage \
       "$ uname -a\n" \
       "Linux debian 2.4.23 #2 Tue Dec 23 17:09:10 MST 2003 i686 GNU/Linux\n"

#define uncompress_trivial_usage \
       "[-c] [-f] [name...]"
#define uncompress_full_usage "\n\n" \
       "Uncompress .Z file[s]\n" \
     "\nOptions:" \
     "\n	-c	Extract to stdout" \
     "\n	-f	Overwrite an existing file" \

#define unexpand_trivial_usage \
       "[-f][-a][-t NUM] [FILE|-]"
#define unexpand_full_usage "\n\n" \
       "Convert spaces to tabs, writing to standard output.\n" \
     "\nOptions:" \
	USE_FEATURE_UNEXPAND_LONG_OPTIONS( \
     "\n	-a,--all	Convert all blanks" \
     "\n	-f,--first-only	Convert only leading blanks" \
     "\n	-t,--tabs=N	Tabstops every N chars" \
	) \
	SKIP_FEATURE_UNEXPAND_LONG_OPTIONS( \
     "\n	-a	Convert all blanks" \
     "\n	-f	Convert only leading blanks" \
     "\n	-t N	Tabstops every N chars" \
	)

#define uniq_trivial_usage \
       "[-fscduw]... [INPUT [OUTPUT]]"
#define uniq_full_usage "\n\n" \
       "Discard duplicate lines\n" \
     "\nOptions:" \
     "\n	-c	Prefix lines by the number of occurrences" \
     "\n	-d	Only print duplicate lines" \
     "\n	-u	Only print unique lines" \
     "\n	-f N	Skip first N fields" \
     "\n	-s N	Skip first N chars (after any skipped fields)" \
     "\n	-w N	Compare N characters in line" \

#define uniq_example_usage \
       "$ echo -e \"a\\na\\nb\\nc\\nc\\na\" | sort | uniq\n" \
       "a\n" \
       "b\n" \
       "c\n"

#define unix2dos_trivial_usage \
       "[option] [FILE]"
#define unix2dos_full_usage "\n\n" \
       "Convert FILE from unix to dos format.\n" \
       "When no file is given, use stdin/stdout.\n" \
     "\nOptions:" \
     "\n	-u	dos2unix" \
     "\n	-d	unix2dos" \

#define unzip_trivial_usage \
       "[-opts[modifiers]] file[.zip] [list] [-x xlist] [-d exdir]"
#define unzip_full_usage "\n\n" \
       "Extract files from ZIP archives\n" \
     "\nOptions:" \
     "\n	-l	List archive contents (with -q for short form)" \
     "\n	-n	Never overwrite existing files (default)" \
     "\n	-o	Overwrite files without prompting" \
     "\n	-p	Send output to stdout" \
     "\n	-q	Quiet" \
     "\n	-x	Exclude these files" \
     "\n	-d	Extract files into this directory" \

#define uptime_trivial_usage \
       ""
#define uptime_full_usage "\n\n" \
       "Display the time since the last boot"

#define uptime_example_usage \
       "$ uptime\n" \
       "  1:55pm  up  2:30, load average: 0.09, 0.04, 0.00\n"

#define usleep_trivial_usage \
       "N"
#define usleep_full_usage "\n\n" \
       "Pause for N microseconds"

#define usleep_example_usage \
       "$ usleep 1000000\n" \
       "[pauses for 1 second]\n"

#define uudecode_trivial_usage \
       "[-o outfile] [infile]"
#define uudecode_full_usage "\n\n" \
       "Uudecode a file\n" \
       "Finds outfile name in uuencoded source unless -o is given"

#define uudecode_example_usage \
       "$ uudecode -o busybox busybox.uu\n" \
       "$ ls -l busybox\n" \
       "-rwxr-xr-x   1 ams      ams        245264 Jun  7 21:35 busybox\n"

#define uuencode_trivial_usage \
       "[-m] [infile] stored_filename"
#define uuencode_full_usage "\n\n" \
       "Uuencode a file to stdout\n" \
     "\nOptions:" \
     "\n	-m	Use base64 encoding per RFC1521" \

#define uuencode_example_usage \
       "$ uuencode busybox busybox\n" \
       "begin 755 busybox\n" \
       "<encoded file snipped>\n" \
       "$ uudecode busybox busybox > busybox.uu\n" \
       "$\n"

#define vconfig_trivial_usage \
       "COMMAND [OPTIONS]..."
#define vconfig_full_usage "\n\n" \
       "Create and remove virtual ethernet devices\n" \
     "\nOptions:" \
     "\n	add		[interface-name] [vlan_id]" \
     "\n	rem		[vlan-name]" \
     "\n	set_flag	[interface-name] [flag-num] [0 | 1]" \
     "\n	set_egress_map	[vlan-name] [skb_priority] [vlan_qos]" \
     "\n	set_ingress_map	[vlan-name] [skb_priority] [vlan_qos]" \
     "\n	set_name_type	[name-type]" \

#define vi_trivial_usage \
       "[OPTION] [FILE]..."
#define vi_full_usage "\n\n" \
       "Edit FILE\n" \
     "\nOptions:" \
	USE_FEATURE_VI_COLON( \
     "\n	-c	Initial command to run ($EXINIT also available)") \
	USE_FEATURE_VI_READONLY( \
     "\n	-R	Read-only - do not write to the file") \
     "\n	-H	Short help regarding available features" \

#define vlock_trivial_usage \
       "[OPTIONS]"
#define vlock_full_usage "\n\n" \
       "Lock a virtual terminal. A password is required to unlock.\n" \
     "\nOptions:" \
     "\n	-a	Lock all VTs" \

#define watch_trivial_usage \
       "[-n seconds] [-t] COMMAND..."
#define watch_full_usage "\n\n" \
       "Execute a program periodically\n" \
     "\nOptions:" \
     "\n	-n	Loop period in seconds (default 2)" \
     "\n	-t	Don't print header" \

#define watch_example_usage \
       "$ watch date\n" \
       "Mon Dec 17 10:31:40 GMT 2000\n" \
       "Mon Dec 17 10:31:42 GMT 2000\n" \
       "Mon Dec 17 10:31:44 GMT 2000"

#define watchdog_trivial_usage \
       "[-t N[ms]] [-F] DEV"
#define watchdog_full_usage "\n\n" \
       "Periodically write to watchdog device DEV\n" \
     "\nOptions:" \
     "\n	-t N	Timer period (default 30)" \
     "\n	-F	Run in foreground" \
     "\n" \
     "\nUse -t 500ms to specify period in milliseconds" \

#define wc_trivial_usage \
       "[OPTION]... [FILE]..."
#define wc_full_usage "\n\n" \
       "Print line, word, and byte counts for each FILE, and a total line if\n" \
       "more than one FILE is specified. With no FILE, read standard input.\n" \
     "\nOptions:" \
     "\n	-c	Print the byte counts" \
     "\n	-l	Print the newline counts" \
     "\n	-L	Print the length of the longest line" \
     "\n	-w	Print the word counts" \

#define wc_example_usage \
       "$ wc /etc/passwd\n" \
       "     31      46    1365 /etc/passwd\n"

#define wget_trivial_usage \
	USE_GETOPT_LONG( \
       "[-c|--continue] [-s|--spider] [-q|--quiet] [-O|--output-document file]\n" \
       "	[--header 'header: value'] [-Y|--proxy on/off] [-P DIR]\n" \
       "	[-U|--user-agent agent] url" \
	) \
	SKIP_GETOPT_LONG( \
       "[-csq] [-O file] [-Y on/off] [-P DIR] [-U agent] url" \
	)
#define wget_full_usage "\n\n" \
       "Retrieve files via HTTP or FTP\n" \
     "\nOptions:" \
     "\n	-s	Spider mode - only check file existence" \
     "\n	-c	Continue retrieval of aborted transfer" \
     "\n	-q	Quiet" \
     "\n	-P	Set directory prefix to DIR" \
     "\n	-O	Save to filename ('-' for stdout)" \
     "\n	-U	Adjust 'User-Agent' field" \
     "\n	-Y	Use proxy ('on' or 'off')" \

#define which_trivial_usage \
       "[COMMAND...]"
#define which_full_usage "\n\n" \
       "Locate a COMMAND"
#define which_example_usage \
       "$ which login\n" \
       "/bin/login\n"

#define who_trivial_usage \
       "[-a]"
#define who_full_usage "\n\n" \
       "Show who is logged on\n" \
     "\nOptions:" \
     "\n	-a	show all" \

#define whoami_trivial_usage \
       ""
#define whoami_full_usage "\n\n" \
       "Print the user name associated with the current effective user id"

#define xargs_trivial_usage \
       "[OPTIONS] [COMMAND] [ARGS...]"
#define xargs_full_usage "\n\n" \
       "Execute COMMAND on every item given by standard input\n" \
     "\nOptions:" \
	USE_FEATURE_XARGS_SUPPORT_CONFIRMATION( \
     "\n	-p	Prompt the user about whether to run each command") \
     "\n	-r	Do not run command for empty read lines" \
	USE_FEATURE_XARGS_SUPPORT_TERMOPT( \
     "\n	-x	Exit if the size is exceeded") \
	USE_FEATURE_XARGS_SUPPORT_ZERO_TERM( \
     "\n	-0	Input filenames are terminated by a null character") \
     "\n	-t	Print the command line on stderr before executing it" \

#define xargs_example_usage \
       "$ ls | xargs gzip\n" \
       "$ find . -name '*.c' -print | xargs rm\n"

#define yes_trivial_usage \
       "[OPTION]... [STRING]..."
#define yes_full_usage "\n\n" \
       "Repeatedly output a line with all specified STRING(s), or 'y'"

#define zcat_trivial_usage \
       "FILE"
#define zcat_full_usage "\n\n" \
       "Uncompress to stdout"

#define zcip_trivial_usage \
       "[OPTIONS] ifname script"
#define zcip_full_usage "\n\n" \
       "Manage a ZeroConf IPv4 link-local address\n" \
     "\nOptions:" \
     "\n	-f		Run in foreground" \
     "\n	-q		Quit after address (no daemon)" \
     "\n	-r 169.254.x.x	Request this address first" \
     "\n	-v		Verbose" \

#endif /* __BB_USAGE_H__ */
