#define ar_trivial_usage \
	"-[ovR]{ptx} archive filenames"
#define ar_full_usage \
	"Extract or list files from an ar archive.\n\n" \
	"Options:\n" \
	"\t-o\t\tpreserve original dates\n" \
	"\t-p\t\textract to stdout\n" \
	"\t-t\t\tlist\n" \
	"\t-x\t\textract\n" \
	"\t-v\t\tverbosely list files processed\n" \
	"\t-R\t\trecursive action"

#define basename_trivial_usage \
	"FILE [SUFFIX]"
#define basename_full_usage \
	"Strips directory path and suffixes from FILE.\n" \
	"If specified, also removes any trailing SUFFIX."

#define cat_trivial_usage \
	"[FILE]..."
#define cat_full_usage \
	"Concatenates FILE(s) and prints them to stdout."

#define chgrp_trivial_usage \
	"[OPTION]... GROUP FILE..."
#define chgrp_full_usage \
	"Change the group membership of each FILE to GROUP.\n" \
	"\nOptions:\n" \
	"\t-R\tChanges files and directories recursively."

#define chmod_trivial_usage \
	"[-R] MODE[,MODE]... FILE..."
#define chmod_full_usage \
	"Each MODE is one or more of the letters ugoa, one of the\n" \
	"symbols +-= and one or more of the letters rwxst.\n\n" \
	"Options:\n" \
	"\t-R\tChanges files and directories recursively."

#define chown_trivial_usage \
	"[OPTION]...  OWNER[<.|:>[GROUP] FILE..."
#define chown_full_usage \
	"Change the owner and/or group of each FILE to OWNER and/or GROUP.\n" \
	"\nOptions:\n" \
	"\t-R\tChanges files and directories recursively."

#define chroot_trivial_usage \
	"NEWROOT [COMMAND...]"
#define chroot_full_usage \
	"Run COMMAND with root directory set to NEWROOT."

#define chvt_trivial_usage \
	"N"
#define chvt_full_usage \
	"Changes the foreground virtual terminal to /dev/ttyN"

#define clear_trivial_usage \
	""
#define clear_full_usage \
	"Clear screen."

#define cmp_trivial_usage \
	"FILE1 [FILE2]"
#define cmp_full_usage \
	"Compare files."

#define cp_trivial_usage \
	"[OPTION]... SOURCE DEST"
#define cp_full_usage \
	"Copies SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n" \
	"\n" \
	"\t-a\tSame as -dpR\n" \
	"\t-d\tPreserves links\n" \
	"\t-p\tPreserves file attributes if possible\n" \
	"\t-f\tforce (implied; ignored) - always set\n" \
	"\t-R\tCopies directories recursively"

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

#define date_trivial_usage \
	"[OPTION]... [+FORMAT]"
#define date_full_usage \
	"Displays the current time in the given FORMAT, or sets the system date.\n" \
	"\nOptions:\n" \
	"\t-R\t\tOutputs RFC-822 compliant date string\n" \
	"\t-d STRING\tdisplay time described by STRING, not `now'\n" \
	"\t-s\t\tSets time described by STRING\n" \
	"\t-u\t\tPrints or sets Coordinated Universal Time"

#define dc_trivial_usage \
	"expression ..."
#define dc_full_usage \
	"This is a Tiny RPN calculator that understands the\n" \
	"following operations: +, -, /, *, and, or, not, eor.\n" \
	"i.e. 'dc 2 2 add' -> 4, and 'dc 8 8 \\* 2 2 + /' -> 16"

#define dd_trivial_usage \
	"[if=FILE] [of=FILE] [bs=N] [count=N] [skip=N]\n" \
	"\t  [seek=N] [conv=notrunc|sync]"
#define dd_full_usage \
	"Copy a file, converting and formatting according to options\n\n" \
	"\tif=FILE\t\tread from FILE instead of stdin\n" \
	"\tof=FILE\t\twrite to FILE instead of stdout\n" \
	"\tbs=N\t\tread and write N bytes at a time\n" \
	"\tcount=N\t\tcopy only N input blocks\n" \
	"\tskip=N\t\tskip N input blocks\n" \
	"\tseek=N\t\tskip N output blocks\n" \
	"\tconv=notrunc\tdon't truncate output file\n" \
	"\tconv=sync\tpad blocks with zeros\n" \
	"\n" \
	"Numbers may be suffixed by c (x1), w (x2), b (x512), kD (x1000), k (x1024),\n" \
	"MD (x1000000), M (x1048576), GD (x1000000000) or G (x1073741824)."

#define deallocvt_trivial_usage \
	"N"
#define deallocvt_full_usage \
	 "Deallocate unused virtual terminal /dev/ttyN"


#ifdef BB_FEATURE_HUMAN_READABLE
  #define USAGE_HUMAN_READABLE(a) a
  #define USAGE_NOT_HUMAN_READABLE(a)
#else
  #define USAGE_HUMAN_READABLE(a) 
  #define USAGE_NOT_HUMAN_READABLE(a) a
#endif
#define df_trivial_usage \
	"[-" USAGE_HUMAN_READABLE("hm") USAGE_NOT_HUMAN_READABLE("") "k] [filesystem ...]"
#define df_full_usage \
	"Print the filesystem space used and space available.\n\n" \
	"Options:\n" \
	USAGE_HUMAN_READABLE( \
	"\n\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G )\n" \
	"\t-m\tprint sizes in megabytes\n" \
	"\t-k\tprint sizes in kilobytes(default)") USAGE_NOT_HUMAN_READABLE( \
	"\n\t-k\tprint sizes in kilobytes(compatability)")

#define dirname_trivial_usage \
	"[FILENAME ...]"
#define dirname_full_usage \
	"Strips non-directory suffix from FILENAME"

#define dmesg_trivial_usage \
	"[-c] [-n LEVEL] [-s SIZE]"
#define dmesg_full_usage \
	"Prints or controls the kernel ring buffer\n\n" \
	"Options:\n" \
	"\t-c\t\tClears the ring buffer's contents after printing\n" \
	"\t-n LEVEL\tSets console logging level\n" \
	"\t-s SIZE\t\tUse a buffer of size SIZE"

#define dos2unix_trivial_usage \
	"[option] [file]"
#define dos2unix_full_usage \
	"Converts a text file to/from dos format to unix format.\n\n" \
	"Options:\n" \
	"\t-u\toutput will be in UNIX format\n" \
	"\t-d\toutput will be in DOS format\n\n" \
	"- when no option is given then input format will be automaticaly detected\n" \
	"  and converted to the oposite format on output\n" \
	"- when no file is given, then stdin is used as input and stdout as output"

#define dpkg_trivial_usage \
	"[-i|-r|--unpack|--configure] my.deb"
#define dpkg_full_usage \
	"WORK IN PROGRESS, only usefull for debian-installer"

#define dpkg_deb_trivial_usage \
	"[-cexX] file directory"
#define dpkg_deb_full_usage \
	"Perform actions on debian packages (.debs)\n\n" \
	"Options:\n" \
	"\t-c\tList contents of filesystem tree (verbose)\n" \
	"\t-l\tList contents of filesystem tree (.list format)\n" \
	"\t-e\tExtract control files to directory\n" \
	"\t-x\tExctract packages filesystem tree to directory\n" \
	"\t-X\tVerbose extract"

#define du_trivial_usage \
	"[-ls" USAGE_HUMAN_READABLE("hm") USAGE_NOT_HUMAN_READABLE("") "k] [FILE]..."
#define du_full_usage \
	"Summarizes disk space used for each FILE and/or directory.\n" \
	"Disk space is printed in units of 1024 bytes.\n\n" \
	"Options:\n" \
	"\t-l\tcount sizes many times if hard linked\n" \
	"\t-s\tdisplay only a total for each argument" \
	USAGE_HUMAN_READABLE( \
	"\n\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G )\n" \
	"\t-m\tprint sizes in megabytes\n" \
	"\t-k\tprint sizes in kilobytes(default)") USAGE_NOT_HUMAN_READABLE( \
	"\n\t-k\tprint sizes in kilobytes(compatability)")

#define dumpkmap_trivial_usage \
	"> keymap"
#define dumpkmap_full_usage \
	"Prints out a binary keyboard translation table to standard output."

#define dutmp_trivial_usage \
	"[FILE]"
#define dutmp_full_usage \
	"Dump utmp file format (pipe delimited) from FILE\n" \
	"or stdin to stdout.  (i.e. 'dutmp /var/run/utmp')"

#define echo_trivial_usage \
	"[-neE] [ARG ...]"
#define echo_full_usage \
	"Prints the specified ARGs to stdout\n\n" \
	"Options:\n" \
	"\t-n\tsuppress trailing newline\n" \
	"\t-e\tinterpret backslash-escaped characters (i.e. \\t=tab etc)\n" \
	"\t-E\tdisable interpretation of backslash-escaped characters"

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

#define false_trivial_usage \
	""
#define false_full_usage \
	"Return an exit code of FALSE (1)."

#define fdflush_trivial_usage \
	"DEVICE"
#define fdflush_full_usage \
	"Forces floppy disk drive to detect disk change"

#ifdef BB_FEATURE_FIND_TYPE
  #define USAGE_FIND_TYPE(a) a
#else
  #define USAGE_FIND_TYPE(a)
#endif
#ifdef BB_FEATURE_FIND_PERM
  #define USAGE_FIND_PERM(a) a
#else
  #define USAGE_FIND_PERM(a)
#endif
#ifdef BB_FEATURE_FIND_MTIME
  #define USAGE_FIND_MTIME(a) a
#else
  #define USAGE_FIND_MTIME(a)
#endif

#define find_trivial_usage \
	"[PATH...] [EXPRESSION]"
#define find_full_usage \
	"Search for files in a directory hierarchy.  The default PATH is\n" \
	"the current directory; default EXPRESSION is '-print'\n" \
	"\nEXPRESSION may consist of:\n" \
	"\t-follow\t\tDereference symbolic links.\n" \
	"\t-name PATTERN\tFile name (leading directories removed) matches PATTERN." \
	USAGE_FIND_TYPE( \
	"\n\t-type X\t\tFiletype matches X (where X is one of: f,d,l,b,c,...)" \
) USAGE_FIND_PERM( \
	"\n\t-perm PERMS\tPermissions match any of (+NNN); all of (-NNN);\n\t\t\tor exactly (NNN)" \
) USAGE_FIND_MTIME( \
	"\n\t-mtime TIME\tModified time is greater than (+N); less than (-N);\n\t\t\tor exactly (N) days")

#define free_trivial_usage \
	""
#define free_full_usage \
	"Displays the amount of free and used system memory"

#define freeramdisk_trivial_usage \
	"DEVICE"
#define freeramdisk_full_usage \
	"Frees all memory used by the specified ramdisk."

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
	"\t-f\tForce file system check."

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
	"\t-u, --unqote			Do not quote the output"

#define grep_trivial_usage \
	"[-ihHnqvs] pattern [files...]"
#define grep_full_usage \
	"Search for PATTERN in each FILE or standard input.\n\n" \
	"Options:\n" \
	"\t-H\tprefix output lines with filename where match was found\n" \
	"\t-h\tsuppress the prefixing filename on output\n" \
	"\t-i\tignore case distinctions\n" \
	"\t-n\tprint line number with output lines\n" \
	"\t-q\tbe quiet. Returns 0 if result was found, 1 otherwise\n" \
	"\t-v\tselect non-matching lines\n" \
	"\t-s\tsuppress file open/read error messages"

#define gunzip_trivial_usage \
	"[OPTION]... FILE"
#define gunzip_full_usage \
	"Uncompress FILE (or standard input if FILE is '-').\n\n" \
	"Options:\n" \
	"\t-c\tWrite output to standard output\n" \
	"\t-t\tTest compressed file integrity"

#define gzip_trivial_usage \
	"[OPTION]... FILE"
#define gzip_full_usage \
	"Compress FILE with maximum compression.\n" \
	"When FILE is '-', reads standard input.  Implies -c.\n\n" \
	"Options:\n" \
	"\t-c\tWrite output to standard output instead of FILE.gz\n" \
	"\t-d\tdecompress"

#define halt_trivial_usage \
	""
#define halt_full_usage \
	"Halt the system."

#define head_trivial_usage \
	"[OPTION] [FILE]..."
#define head_full_usage \
	"Print first 10 lines of each FILE to standard output.\n" \
	"With more than one FILE, precede each with a header giving the\n" \
	"file name. With no FILE, or when FILE is -, read standard input.\n\n" \
	"Options:\n" \
	"\t-n NUM\t\tPrint first NUM lines instead of first 10"

#define hostid_trivial_usage \
	""
#define hostid_full_usage \
	"Print out a unique 32-bit identifier for the machine."

#define hostname_trivial_usage \
	"[OPTION] {hostname | -F file}"
#define hostname_full_usage \
	"Get or set the hostname or DNS domain name. If a hostname is given\n" \
	"(or a file with the -F parameter), the host name will be set.\n\n" \
	"Options:\n" \
	"\t-s\t\tShort\n" \
	"\t-i\t\tAddresses for the hostname\n" \
	"\t-d\t\tDNS domain name\n" \
	"\t-F, --file FILE\tUse the contents of FILE to specify the hostname"

#define id_trivial_usage \
	"[OPTIONS]... [USERNAME]"
#define id_full_usage \
	"Print information for USERNAME or the current user\n\n" \
	"Options:\n" \
	"\t-g\tprints only the group ID\n" \
	"\t-u\tprints only the user ID\n" \
	"\t-n\tprint a name instead of a number (with for -ug)\n" \
	"\t-r\tprints the real user ID instead of the effective ID (with -ug)"

#ifdef SIOCSKEEPALIVE
  #define USAGE_SIOCSKEEPALIVE(a) a
#else
  #define USAGE_SIOCSKEEPALIVE(a)
#endif
#define ifconfig_trivial_usage \
	"[-a] [-i] [-v] <interface> [<address>]"
#define ifconfig_full_usage \
	"configure a network interface\n\n" \
	"Options:\n" \
	"\t[[-]broadcast [<address>]]  [[-]pointopoint [<address>]]\n" \
	"\t[netmask <address>]  [dstaddr <address>]  [tunnel <adress>]\n" \
	USAGE_SIOCSKEEPALIVE("\t[outfill <NN>] [keepalive <NN>]\n") \
	"\t[hw ether <address>]  [metric <NN>]  [mtu <NN>]\n" \
	"\t[[-]trailers]  [[-]arp]  [[-]allmulti]\n" \
	"\t[multicast]  [[-]promisc]\n" \
	"\t[mem_start <NN>]  [io_addr <NN>]  [irq <NN>]  [media <type>]\n" \
	"\t[up|down] ..."

#define init_trivial_usage \
	""
#define init_full_usage \
	"Init is the parent of all processes.\n\n" \
	"This version of init is designed to be run only by the kernel."

#define insmod_trivial_usage \
	"[OPTION]... MODULE [symbol=value]..."
#define insmod_full_usage \
	"Loads the specified kernel modules into the kernel.\n\n" \
	"Options:\n" \
	"\t-f\tForce module to load into the wrong kernel version.\n" \
	"\t-k\tMake module autoclean-able.\n" \
	"\t-v\tverbose output\n"  \
	"\t-L\tLock to prevent simultaneous loads of a module\n" \
	"\t-x\tdo not export externs"

#define kill_trivial_usage \
	"[-signal] process-id [process-id ...]"
#define kill_full_usage \
	"Send a signal (default is SIGTERM) to the specified process(es).\n\n"\
	"Options:\n" \
	"\t-l\tList all signal names and numbers."

#define killall_trivial_usage \
	"[-signal] process-name [process-name ...]"
#define killall_full_usage \
	"Send a signal (default is SIGTERM) to the specified process(es).\n\n"\
	"Options:\n" \
	"\t-l\tList all signal names and numbers."

#define length_trivial_usage \
	"STRING"
#define length_full_usage \
	"Prints out the length of the specified STRING."

#define ln_trivial_usage \
	"[OPTION] TARGET... LINK_NAME|DIRECTORY"
#define ln_full_usage \
	"Create a link named LINK_NAME or DIRECTORY to the specified TARGET\n"\
	"\nYou may use '--' to indicate that all following arguments are non-options.\n\n" \
	"Options:\n" \
	"\t-s\tmake symbolic links instead of hard links\n" \
	"\t-f\tremove existing destination files\n" \
	"\t-n\tno dereference symlinks - treat like normal file"

#define loadacm_trivial_usage \
	"< mapfile"
#define loadacm_full_usage \
	"Loads an acm from standard input."

#define loadfont_trivial_usage \
	"< font"
#define loadfont_full_usage \
	"Loads a console font from standard input."

#define loadkmap_trivial_usage \
	"< keymap"
#define loadkmap_full_usage \
	"Loads a binary keyboard translation table from standard input."

#define logger_trivial_usage \
	"[OPTION]... [MESSAGE]"
#define logger_full_usage \
	"Write MESSAGE to the system log.  If MESSAGE is omitted, log stdin.\n\n" \
	"Options:\n" \
	"\t-s\tLog to stderr as well as the system log.\n" \
	"\t-t\tLog using the specified tag (defaults to user name).\n" \
	"\t-p\tEnter the message with the specified priority.\n" \
	"\t\tThis may be numerical or a ``facility.level'' pair."

#define logname_trivial_usage \
	""
#define logname_full_usage \
	"Print the name of the current user."

#ifdef BB_FEATURE_LS_TIMESTAMPS
  #define USAGE_LS_TIMESTAMPS(a) a
#else
  #define USAGE_LS_TIMESTAMPS(a)
#endif
#ifdef BB_FEATURE_LS_FILETYPES
  #define USAGE_LS_FILETYPES(a) a
#else
  #define USAGE_LS_FILETYPES(a)
#endif
#ifdef BB_FEATURE_LS_FOLLOWLINKS
  #define USAGE_LS_FOLLOWLINKS(a) a
#else
  #define USAGE_LS_FOLLOWLINKS(a)
#endif
#ifdef BB_FEATURE_LS_RECURSIVE
  #define USAGE_LS_RECURSIVE(a) a
#else
  #define USAGE_LS_RECURSIVE(a)
#endif
#ifdef BB_FEATURE_LS_SORTFILES
  #define USAGE_LS_SORTFILES(a) a
#else
  #define USAGE_LS_SORTFILES(a)
#endif
#ifdef BB_FEATURE_AUTOWIDTH
  #define USAGE_AUTOWIDTH(a) a
#else
  #define USAGE_AUTOWIDTH(a)
#endif
#define ls_trivial_usage \
	"[-1Aa" USAGE_LS_TIMESTAMPS("c") "Cd" USAGE_LS_TIMESTAMPS("e") USAGE_LS_FILETYPES("F") "iln" USAGE_LS_FILETYPES("p") USAGE_LS_FOLLOWLINKS("L") USAGE_LS_RECURSIVE("R") USAGE_LS_SORTFILES("rS") "s" USAGE_AUTOWIDTH("T") USAGE_LS_TIMESTAMPS("tu") USAGE_LS_SORTFILES("v") USAGE_AUTOWIDTH("w") "x" USAGE_LS_SORTFILES("X") USAGE_HUMAN_READABLE("h") USAGE_NOT_HUMAN_READABLE("") "k] [filenames...]"
#define ls_full_usage \
	"List directory contents\n\n" \
	"Options:\n" \
	"\t-1\tlist files in a single column\n" \
	"\t-A\tdo not list implied . and ..\n" \
	"\t-a\tdo not hide entries starting with .\n" \
	"\t-C\tlist entries by columns\n" \
	USAGE_LS_TIMESTAMPS("\t-c\twith -l: show ctime\n") \
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
	"\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G )\n" \
	"\t-k\tprint sizes in kilobytes(default)") USAGE_NOT_HUMAN_READABLE( \
	"\t-k\tprint sizes in kilobytes(compatability)") 

#define lsmod_trivial_usage \
	""
#define lsmod_full_usage \
	"List the currently loaded kernel modules."

#define makedevs_trivial_usage \
	"NAME TYPE MAJOR MINOR FIRST LAST [s]"
#define makedevs_full_usage \
	"Creates a range of block or character special files\n\n" \
	"TYPEs include:\n" \
	"\tb:\tMake a block (buffered) device.\n" \
	"\tc or u:\tMake a character (un-buffered) device.\n" \
	"\tp:\tMake a named pipe. MAJOR and MINOR are ignored for named pipes.\n\n" \
	"FIRST specifies the number appended to NAME to create the first device.\n" \
	"LAST specifies the number of the last item that should be created.\n" \
	"If 's' is the last argument, the base device is created as well.\n\n" \
	"For example:\n" \
	"\tmakedevs /dev/ttyS c 4 66 2 63   ->  ttyS2-ttyS63\n" \
	"\tmakedevs /dev/hda b 3 0 0 8 s    ->  hda,hda1-hda8"

#define md5sum_trivial_usage \
	"[OPTION] [FILE]...\n" \
	"or: md5sum [OPTION] -c [FILE]"
#define md5sum_full_usage \
	"Print or check MD5 checksums.\n\n" \
	"Options:\n" \
	"With no FILE, or when FILE is -, read standard input.\n\n" \
	"\t-b\tread files in binary mode\n" \
	"\t-c\tcheck MD5 sums against given list\n" \
	"\t-t\tread files in text mode (default)\n" \
	"\t-g\tread a string\n" \
	"\nThe following two options are useful only when verifying checksums:\n" \
	"\t-s\tdon't output anything, status code shows success\n" \
	"\t-w\twarn about improperly formated MD5 checksum lines"

#define mkdir_trivial_usage \
	"[OPTION] DIRECTORY..."
#define mkdir_full_usage \
	"Create the DIRECTORY(ies), if they do not already exist\n\n" \
	"Options:\n" \
	"\t-m\tset permission mode (as in chmod), not rwxrwxrwx - umask\n" \
	"\t-p\tno error if existing, make parent directories as needed"

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
	"\tb:\tMake a block (buffered) device.\n" \
	"\tc or u:\tMake a character (un-buffered) device.\n" \
	"\tp:\tMake a named pipe. MAJOR and MINOR are ignored for named pipes."

#define mkswap_trivial_usage \
	"[-c] [-v0|-v1] device [block-count]"
#define mkswap_full_usage \
	"Prepare a disk partition to be used as a swap partition.\n\n" \
	"Options:\n" \
	"\t-c\t\tCheck for read-ability.\n" \
	"\t-v0\t\tMake version 0 swap [max 128 Megs].\n" \
	"\t-v1\t\tMake version 1 swap [big!] (default for kernels >\n\t\t\t2.1.117).\n" \
	"\tblock-count\tNumber of block to use (default is entire partition)."

#define mktemp_trivial_usage \
	"[-q] TEMPLATE"
#define mktemp_full_usage \
	"Creates a temporary file with its name based on TEMPLATE.\n" \
	"TEMPLATE is any name with six `Xs' (i.e. /tmp/temp.XXXXXX)."

#define more_trivial_usage \
	"[FILE ...]"
#define more_full_usage \
	"More is a filter for viewing FILE one screenful at a time."

#ifdef BB_FEATURE_MOUNT_LOOP
  #define USAGE_MOUNT_LOOP(a) a
#else
  #define USAGE_MOUNT_LOOP(a)
#endif
#ifdef BB_MTAB
  #define USAGE_MTAB(a) a
#else
  #define USAGE_MTAB(a)
#endif
#define mount_trivial_usage \
	"[flags] device directory [-o options,more-options]"
#define mount_full_usage \
	"Mount a filesystem\n\n" \
	"Flags:\n"  \
	"\t-a:\t\tMount all filesystems in fstab.\n" \
	USAGE_MTAB( \
	"\t-f:\t\t\"Fake\" Add entry to mount table but don't mount it.\n" \
	"\t-n:\t\tDon't write a mount table entry.\n" \
	) \
	"\t-o option:\tOne of many filesystem options, listed below.\n" \
	"\t-r:\t\tMount the filesystem read-only.\n" \
	"\t-t fs-type:\tSpecify the filesystem type.\n" \
	"\t-w:\t\tMount for reading and writing (default).\n" \
	"\n" \
	"Options for use with the \"-o\" flag:\n" \
	"\tasync/sync:\tWrites are asynchronous / synchronous.\n" \
	"\tatime/noatime:\tEnable / disable updates to inode access times.\n" \
	"\tdev/nodev:\tAllow use of special device files / disallow them.\n" \
	"\texec/noexec:\tAllow use of executable files / disallow them.\n" \
	USAGE_MOUNT_LOOP( \
	"\tloop:\t\tMounts a file via loop device.\n" \
	) \
	"\tsuid/nosuid:\tAllow set-user-id-root programs / disallow them.\n" \
	"\tremount:\tRe-mount a mounted filesystem, changing its flags.\n" \
	"\tro/rw:\t\tMount for read-only / read-write.\n" \
	"\nThere are EVEN MORE flags that are specific to each filesystem.\n" \
	"You'll have to see the written documentation for those."

#define mt_trivial_usage \
	"[-f device] opcode value"
#define mt_full_usage \
	"Control magnetic tape drive operation\n" \
	"\nAvailable Opcodes:\n\n" \
	"bsf bsfm bsr bss datacompression drvbuffer eof eom erase\n" \
	"fsf fsfm fsr fss load lock mkpart nop offline ras1 ras2\n" \
	"ras3 reset retension rew rewoffline seek setblk setdensity\n" \
	"setpart tell unload unlock weof wset"

#define mv_trivial_usage \
	"SOURCE DEST\n" \
	"or: mv SOURCE... DIRECTORY"
#define mv_full_usage \
	"Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY."

#define nc_trivial_usage \
	"[IP] [port]" 
#define nc_full_usage \
	"Netcat opens a pipe to IP:port"

#define nslookup_trivial_usage \
	"[HOST]"
#define nslookup_full_usage \
	"Queries the nameserver for the IP address of the given HOST"

#ifdef BB_FEATURE_SIMPLE_PING
#define ping_trivial_usage "host"
#define ping_full_usage    "Send ICMP ECHO_REQUEST packets to network hosts"
#else
#define ping_trivial_usage \
	"[OPTION]... host"
#define ping_full_usage \
	"Send ICMP ECHO_REQUEST packets to network hosts.\n\n" \
	"Options:\n" \
	"\t-c COUNT\tSend only COUNT pings.\n" \
	"\t-s SIZE\t\tSend SIZE data bytes in packets (default=56).\n" \
	"\t-q\t\tQuiet mode, only displays output at start\n" \
	"\t\t\tand when finished."
#endif

#define pivot_root_trivial_usage \
	"new_root put_old"
#define pivot_root_full_usage \
	"Move the current root file system to put_old and make new_root\n" \
	"the new root file system."

#define poweroff_trivial_usage \
	""
#define poweroff_full_usage \
	"Halt the system and request that the kernel shut off the power."

#define printf_trivial_usage \
	"FORMAT [ARGUMENT...]"
#define printf_full_usage \
	"Formats and prints ARGUMENT(s) according to FORMAT,\n" \
	"Where FORMAT controls the output exactly as in C printf."

#define ps_trivial_usage \
	""
#define ps_full_usage \
	"Report process status\n" \
	"\nThis version of ps accepts no options."

#define pwd_trivial_usage \
	""
#define pwd_full_usage \
	"Print the full filename of the current working directory."

#define rdate_trivial_usage \
	"[OPTION] HOST"
#define rdate_full_usage \
	"Get and possibly set the system date and time from a remote HOST.\n\n" \
	"Options:\n" \
	"\t-s\tSet the system date and time (default).\n" \
	"\t-p\tPrint the date and time."

#define readlink_trivial_usage \
	""
#define readlink_full_usage \
	"Read a symbolic link."

#define reboot_trivial_usage \
	""
#define reboot_full_usage \
	"Reboot the system."

#define renice_trivial_usage \
	"priority pid [pid ...]"
#define renice_full_usage \
	"Changes priority of running processes. Allowed priorities range\n" \
	"from 20 (the process runs only when nothing else is running) to 0\n" \
	"(default priority) to -20 (almost nothing else ever gets to run)."

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
	"\t-f\t\tremove existing destinations, never prompt\n" \
	"\t-r or -R\tremove the contents of directories recursively"

#define rmdir_trivial_usage \
	"[OPTION]... DIRECTORY..."
#define rmdir_full_usage \
	"Remove the DIRECTORY(ies), if they are empty."

#define rmmod_trivial_usage \
	"[OPTION]... [MODULE]..."
#define rmmod_full_usage \
	"Unloads the specified kernel modules from the kernel.\n\n" \
	"Options:\n" \
	"\t-a\tTry to remove all unused kernel modules."

#define route_trivial_usage \
	"[{add|del|flush}]"
#define route_full_usage \
	"Edit the kernel's routing tables"

#define rpmunpack_trivial_usage \
	"< package.rpm | gunzip | cpio -idmuv"
#define rpmunpack_full_usage \
	"Extracts an rpm archive."

#define sed_trivial_usage \
	"[-Vhnef] pattern [files...]"
#define sed_full_usage \
	"Options:\n" \
	"\t-n\t\tsuppress automatic printing of pattern space\n" \
	"\t-e script\tadd the script to the commands to be executed\n" \
	"\t-f scriptfile\tadd the contents of script-file to the commands to be executed\n" \
	"\t-h\t\tdisplay this help message\n" \
	"\n" \
	"If no -e or -f is given, the first non-option argument is taken as the\n" \
	"sed script to interpret. All remaining arguments are names of input\n" \
	"files; if no input files are specified, then the standard input is read."

#define setkeycodes_trivial_usage \
	"SCANCODE KEYCODE ..."
#define setkeycodes_full_usage \
	"Set entries into the kernel's scancode-to-keycode map,\n" \
	"allowing unusual keyboards to generate usable keycodes.\n\n" \
	"SCANCODE may be either xx or e0xx (hexadecimal),\n" \
	"and KEYCODE is given in decimal"

#define sh_trivial_usage \
	"[FILE]...\n" \
	"or: sh -c command [args]..."
#define sh_full_usage \
	"lash: The BusyBox command interpreter (shell)."

#define sleep_trivial_usage \
	"N"
#define sleep_full_usage \
	"Pause for N seconds."


#ifdef BB_FEATURE_SORT_REVERSE
  #define USAGE_SORT_REVERSE(a) a
#else
  #define USAGE_SORT_REVERSE(a)
#endif
#define sort_trivial_usage \
	"[-n]" USAGE_SORT_REVERSE(" [-r]") " [FILE]..."
#define sort_full_usage \
	"Sorts lines of text in the specified files"

#define stty_trivial_usage \
	"[-a|g] [-F device] [SETTING]..."
#define stty_full_usage \
	"Without arguments, prints baud rate, line discipline," \
	"\nand deviations from stty sane." \
	"\n\nOptions:" \
	"\n\t-F device\topen device instead of stdin" \
	"\n\t-a\t\tprint all current settings in human-readable form" \
	"\n\t-g\t\tprint in stty-readable form" \
	"\n\t[SETTING]\tsee documentation"

#define swapoff_trivial_usage \
	"[OPTION] [device]"
#define swapoff_full_usage \
	"Stop swapping virtual memory pages on the given device.\n\n" \
	"Options:\n" \
	"\t-a\tStop swapping on all swap devices"

#define swapon_trivial_usage \
	"[OPTION] [device]"
#define swapon_full_usage \
	"Start swapping virtual memory pages on the given device.\n\n" \
	"Options:\n" \
	"\t-a\tStart swapping on all swap devices"

#define sync_trivial_usage \
	""
#define sync_full_usage \
	"Write all buffered filesystem blocks to disk."


#ifdef BB_FEATURE_KLOGD
  #define USAGE_KLOGD(a) a
#else
  #define USAGE_KLOGD(a)
#endif
#ifdef BB_FEATURE_REMOTE_LOG
  #define USAGE_REMOTE_LOG(a) a
#else
  #define USAGE_REMOTE_LOG(a)
#endif
#define syslogd_trivial_usage \
	"[OPTION]..."
#define syslogd_full_usage \
	"Linux system and kernel (provides klogd) logging utility.\n" \
	"Note that this version of syslogd/klogd ignores /etc/syslog.conf.\n\n" \
	"Options:\n" \
	"\t-m NUM\t\tInterval between MARK lines (default=20min, 0=off)\n" \
	"\t-n\t\tRun as a foreground process\n" \
	USAGE_KLOGD("\t-K\t\tDo not start up the klogd process\n") \
	"\t-O FILE\t\tUse an alternate log file (default=/var/log/messages)" \
	USAGE_REMOTE_LOG( \
	"\n\t-R HOST[:PORT]\tLog remotely to IP or hostname on PORT (default PORT=514/UDP)\n" \
	"\t-L\t\tLog locally as well as network logging (default is network only)")


#ifdef BB_FEATURE_SIMPLE_TAIL
  #define USAGE_UNSIMPLE_TAIL(a)
#else
  #define USAGE_UNSIMPLE_TAIL(a) a
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
	USAGE_UNSIMPLE_TAIL( \
	"\n\t-q\t\tnever output headers giving file names\n" \
	"\t-s SEC\t\twait SEC seconds between reads with -f\n" \
	"\t-v\t\talways output headers giving file names\n\n" \
	"If the first character of N (bytes or lines) is a `+', output begins with \n" \
	"the Nth item from the start of each file, otherwise, print the last N items\n" \
	"in the file. N bytes may be suffixed by k (x1024), b (x512), or m (1024^2)." \
	)

#ifdef BB_FEATURE_TAR_CREATE
  #define USAGE_TAR_CREATE(a) a
#else
  #define USAGE_TAR_CREATE(a)
#endif
#ifdef BB_FEATURE_TAR_EXCLUDE
  #define USAGE_TAR_EXCLUDE(a) a
#else
  #define USAGE_TAR_EXCLUDE(a)
#endif
#define tar_trivial_usage \
	"-[" USAGE_TAR_CREATE("c") "xtvO] " \
	USAGE_TAR_EXCLUDE("[--exclude File] [-X File]") \
	"[-f tarFile] [FILE(s)] ..."
#define tar_full_usage \
	"Create, extract, or list files from a tar file.\n\n" \
	"Main operation mode:\n" \
	USAGE_TAR_CREATE("\tc\t\tcreate\n") \
	"\tx\t\textract\n" \
	"\tt\t\tlist\n" \
	"\nFile selection:\n" \
	"\tf\t\tname of tarfile or \"-\" for stdin\n" \
	"\tO\t\textract to stdout\n" \
	USAGE_TAR_EXCLUDE( \
	"\texclude\t\tfile to exclude\n" \
	 "\tX\t\tfile with names to exclude\n" \
	) \
	"\nInformative output:\n" \
	"\tv\t\tverbosely list files processed"

#define tee_trivial_usage \
	"[OPTION]... [FILE]..."
#define tee_full_usage \
	"Copy standard input to each FILE, and also to standard output.\n\n" \
	"Options:\n" \
	"\t-a\tappend to the given FILEs, do not overwrite"

#define telnet_trivial_usage \
	"host [port]"
#define telnet_full_usage \
	"Telnet is used to establish interactive communication with another\n"\
	"computer over a network using the TELNET protocol."

#define test_trivial_usage \
	"EXPRESSION\n  or   [ EXPRESSION ]"
#define test_full_usage \
	"Checks file types and compares values returning an exit\n" \
	"code determined by the value of EXPRESSION."

#define touch_trivial_usage \
	"[-c] file [file ...]"
#define touch_full_usage \
	"Update the last-modified date on the given file[s].\n\n" \
	"Options:\n" \
	"\t-c\tDo not create any files"

#define tr_trivial_usage \
	"[-cds] STRING1 [STRING2]"
#define tr_full_usage \
	"Translate, squeeze, and/or delete characters from\n" \
	"standard input, writing to standard output.\n\n" \
	"Options:\n" \
	"\t-c\ttake complement of STRING1\n" \
	"\t-d\tdelete input characters coded STRING1\n" \
	"\t-s\tsqueeze multiple output characters of STRING2 into one character"

#define true_trivial_usage \
	""
#define true_full_usage \
	"Return an exit code of TRUE (0)."

#define tty_trivial_usage \
	""
#define tty_full_usage \
	"Print the file name of the terminal connected to standard input.\n\n"\
	"Options:\n" \
	"\t-s\tprint nothing, only return an exit status"

#ifdef BB_FEATURE_MOUNT_FORCE
  #define USAGE_MOUNT_FORCE(a) a
#else
  #define USAGE_MOUNT_FORCE(a)
#endif
#define umount_trivial_usage \
	"[flags] filesystem|directory"
#define umount_full_usage \
	"Unmount file systems\n" \
	"\nFlags:\n" "\t-a:\tUnmount all file systems" \
	USAGE_MTAB(" in /etc/mtab\n\t-n:\tDon't erase /etc/mtab entries") \
	"\n\t-r:\tTry to remount devices as read-only if mount is busy" \
	USAGE_MOUNT_FORCE("\n\t-f:\tForce filesystem umount (i.e. unreachable NFS server)") \
	USAGE_MOUNT_LOOP("\n\t-l:\tDo not free loop device (if a loop device has been used)")

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

#define uniq_trivial_usage \
	"[OPTION]... [INPUT [OUTPUT]]"
#define uniq_full_usage \
	"Discard all but one of successive identical lines from INPUT\n" \
	"(or standard input), writing to OUTPUT (or standard output).\n\n" \
	"Options:\n" \
	"\t-c\tprefix lines by the number of occurrences\n" \
	"\t-d\tonly print duplicate lines\n" \
	"\t-u\tonly print unique lines"

#define unix2dos_trivial_usage \
	"[option] [file]"
#define unix2dos_full_usage \
	"See 'dos2unix --help' for help!"

#define update_trivial_usage \
	"[options]"
#define update_full_usage \
	"Periodically flushes filesystem buffers.\n\n" \
	"Options:\n" \
	"\t-S\tforce use of sync(2) instead of flushing\n" \
	"\t-s SECS\tcall sync this often (default 30)\n" \
	"\t-f SECS\tflush some buffers this often (default 5)"

#define uptime_trivial_usage \
	""
#define uptime_full_usage \
	"Display the time since the last boot."

#define usleep_trivial_usage \
	"N" 
#define usleep_full_usage \
	"Pause for N microseconds."

#define uudecode_trivial_usage \
	"[FILE]..."
#define uudecode_full_usage \
	"Uudecode a file that is uuencoded.\n\n" \
	"Options:\n" \
	"\t-o FILE\tdirect output to FILE" \

#define uuencode_trivial_usage \
	"[OPTION] [INFILE] REMOTEFILE"
#define uuencode_full_usage \
	"Uuencode a file.\n\n" \
	"Options:\n" \
	"\t-m\tuse base64 encoding as of RFC1521"

#define watchdog_trivial_usage \
	"DEV"
#define watchdog_full_usage \
	"Periodically write to watchdog device DEV"

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

#define wget_trivial_usage \
	"[-c] [-O file] url"
#define wget_full_usage \
	"wget retrieves files via HTTP\n\n" \
	"Options:\n" \
	"\t-c\tcontinue retrieval of aborted transfers\n" \
	"\t-O\tsave to filename ('-' for stdout)"

#define which_trivial_usage \
	"[COMMAND ...]"
#define which_full_usage \
	"Locates a COMMAND."

#define whoami_trivial_usage \
	""
#define whoami_full_usage \
	"Prints the user name associated with the current effective user id."

#define xargs_trivial_usage \
	"[COMMAND] [ARGS...]"
#define xargs_full_usage \
	"Executes COMMAND on every item given by standard input."

#define yes_trivial_usage \
	"[OPTION]... [STRING]..."
#define yes_full_usage \
	"Repeatedly outputs a line with all specified STRING(s), or `y'."

#define zcat_trivial_usage \
	"FILE"
#define zcat_full_usage \
	"Uncompress to stdout."
