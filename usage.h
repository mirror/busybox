#include "Config.h"

extern const char usage_messages[];

#if defined USAGE_ENUM
enum
#elif defined USAGE_MESSAGES
const char usage_messages[] =
#endif

#if defined USAGE_ENUM || defined USAGE_MESSAGES
{
#endif

#if defined USAGE_ENUM
#define DO_COMMA ,
#elif defined USAGE_MESSAGES
#define DO_COMMA "\0"
#else
#define DO_COMMA 
#endif


#if defined BB_AR
#if defined USAGE_ENUM
ar_usage_index
#elif defined USAGE_MESSAGES
	"-[ovR]{ptx} archive filenames"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nExtract or list files from an ar archive.\n\n"
	"Options:\n"
	"\t-o\t\tpreserve original dates\n"
	"\t-p\t\textract to stdout\n"
	"\t-t\t\tlist\n"
	"\t-x\t\textract\n"
	"\t-v\t\tverbosely list files processed\n"
	"\t-R\t\trecursive action"
#endif
#endif
DO_COMMA
#endif

#if defined BB_BASENAME
#if defined USAGE_ENUM
basename_usage_index
#elif defined USAGE_MESSAGES
	"FILE [SUFFIX]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nStrips directory path and suffixes from FILE.\n"
	"If specified, also removes any trailing SUFFIX."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CAT
#if defined USAGE_ENUM
cat_usage_index
#elif defined USAGE_MESSAGES
	"[FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nConcatenates FILE(s) and prints them to stdout."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CHMOD_CHOWN_CHGRP
#if defined USAGE_ENUM
chgrp_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... GROUP FILE..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nChange the group membership of each FILE to GROUP.\n"
	"\nOptions:\n\t-R\tChanges files and directories recursively."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CHMOD_CHOWN_CHGRP
#if defined USAGE_ENUM
chmod_usage_index
#elif defined USAGE_MESSAGES
	"[-R] MODE[,MODE]... FILE..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nEach MODE is one or more of the letters ugoa, one of the symbols +-= and\n"
	"one or more of the letters rwxst.\n\n"
	"Options:\n\t-R\tChanges files and directories recursively."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CHMOD_CHOWN_CHGRP
#if defined USAGE_ENUM
chown_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]...  OWNER[<.|:>[GROUP] FILE..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nChange the owner and/or group of each FILE to OWNER and/or GROUP.\n"
	"\nOptions:\n\t-R\tChanges files and directories recursively."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CHROOT
#if defined USAGE_ENUM
chroot_usage_index
#elif defined USAGE_MESSAGES
	"NEWROOT [COMMAND...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nRun COMMAND with root directory set to NEWROOT."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CHVT
#if defined USAGE_ENUM
chvt_usage_index
#elif defined USAGE_MESSAGES
	"N"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nChanges the foreground virtual terminal to /dev/ttyN"
#endif
#endif
DO_COMMA
#endif

#if defined BB_CLEAR
#if defined USAGE_ENUM
clear_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nClear screen."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CMP
#if defined USAGE_ENUM
cmp_usage_index
#elif defined USAGE_MESSAGES
	"FILE1 [FILE2]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCompare files."
#endif
#endif
DO_COMMA
#endif

#if defined BB_CP_MV
#if defined USAGE_ENUM
cp_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... SOURCE DEST\n"
	"   or: cp [OPTION]... SOURCE... DIRECTORY"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCopies SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n"
	"\n"
	"\t-a\tSame as -dpR\n"
	"\t-d\tPreserves links\n"
	"\t-p\tPreserves file attributes if possible\n"
	"\t-f\tforce (implied; ignored) - always set\n"
	"\t-R\tCopies directories recursively"
#endif
#endif
DO_COMMA
#endif

#if defined BB_CUT
#if defined USAGE_ENUM
cut_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrints selected fields from each input FILE to standard output.\n\n"
	"Options:\n"
	"\t-b LIST\tOutput only bytes from LIST\n"
	"\t-c LIST\tOutput only characters from LIST\n"
	"\t-d CHAR\tUse CHAR instead of tab as the field delimiter\n"
	"\t-s\tOutput only the lines containing delimiter\n"
	"\t-f N\tPrint only these fields\n"
	"\t-n\tIgnored"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DATE
#if defined USAGE_ENUM
date_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [+FORMAT]\n"
	"  or:  date [OPTION] [MMDDhhmm[[CC]YY][.ss]]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nDisplays the current time in the given FORMAT, or sets the system date.\n"
	"\nOptions:\n\t-R\t\tOutputs RFC-822 compliant date string\n"
	"\t-d STRING\tdisplay time described by STRING, not `now'\n"
	"\t-s\t\tSets time described by STRING\n"
	"\t-u\t\tPrints or sets Coordinated Universal Time"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DC
#if defined USAGE_ENUM
dc_usage_index
#elif defined USAGE_MESSAGES
	"expression ..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nThis is a Tiny RPN calculator that understands the\n"
	"following operations: +, -, /, *, and, or, not, eor.\n"
	"i.e. 'dc 2 2 add' -> 4, and 'dc 8 8 \\* 2 2 + /' -> 16"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DD
#if defined USAGE_ENUM
dd_usage_index
#elif defined USAGE_MESSAGES
	"[if=FILE] [of=FILE] [bs=N] [count=N] [skip=N] [seek=N]\n"
	"\t  [conv=notrunc|sync]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCopy a file, converting and formatting according to options\n\n"
	"\tif=FILE\t\tread from FILE instead of stdin\n"
	"\tof=FILE\t\twrite to FILE instead of stdout\n"
	"\tbs=N\t\tread and write N bytes at a time\n"
	"\tcount=N\t\tcopy only N input blocks\n"
	"\tskip=N\t\tskip N input blocks\n"
	"\tseek=N\t\tskip N output blocks\n"
	"\tconv=notrunc\tdon't truncate output file\n"
	"\tconv=sync\tpad blocks with zeros\n"
	"\n"
	"Numbers may be suffixed by c (x1), w (x2), b (x512), kD (x1000), k (x1024),\n"
	"MD (x1000000), M (x1048576), GD (x1000000000) or G (x1073741824)."
#endif
#endif
DO_COMMA
#endif

#if defined BB_DEALLOCVT
#if defined USAGE_ENUM
deallocvt_usage_index
#elif defined USAGE_MESSAGES
	"N"
#ifndef BB_FEATURE_TRIVIAL_HELP
	 "\n\nDeallocate unused virtual terminal /dev/ttyN"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DF
#if defined USAGE_ENUM
df_usage_index
#elif defined USAGE_MESSAGES
	"[-"
#ifdef BB_FEATURE_HUMAN_READABLE
	"hm"
#endif
	"k] [filesystem ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint the filesystem space used and space available\n\n"
	"Options:\n"
#ifdef BB_FEATURE_HUMAN_READABLE
	"\n\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G)\n"
	"\t-m\tprint sizes in megabytes\n"
	"\t-k\tprint sizes in kilobytes(default)"
#else
	"\n\t-k\tprint sizes in kilobytes(compatability)"
#endif
#endif
#endif
DO_COMMA
#endif

#if defined BB_DIRNAME
#if defined USAGE_ENUM
dirname_usage_index
#elif defined USAGE_MESSAGES
	"[FILENAME ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nStrips non-directory suffix from FILENAME"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DMESG
#if defined USAGE_ENUM
dmesg_usage_index
#elif defined USAGE_MESSAGES
	"[-c] [-n LEVEL] [-s SIZE]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrints or controls the kernel ring buffer\n\n"
	"Options:\n"
	"\t-c\t\tClears the ring buffer's contents after printing\n"
	"\t-n LEVEL\tSets console logging level\n"
	"\t-s SIZE\t\tUse a buffer of size SIZE"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DOS2UNIX
#if defined USAGE_ENUM
dos2unix_usage_index
#elif defined USAGE_MESSAGES
	"< dosfile > unixfile"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nConverts a text file from dos format to unix format."
#endif
#endif
DO_COMMA
#endif

#if defined BB_DPKG
#if defined USAGE_ENUM
dpkg_usage_index
#elif defined USAGE_MESSAGES
	"<-i|-r|--unpack|--configure> my.deb\n"
	"WORK IN PROGRESS, only usefull for debian-installer"
#ifndef BB_FEATURE_TRIVIAL_HELP
#endif
#endif
DO_COMMA
#endif

#if defined BB_DPKG_DEB
#if defined USAGE_ENUM
dpkg_deb_usage_index
#elif defined USAGE_MESSAGES
	"[-cexX] file directory"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPerform actions on debian packages (.debs)\n\n"
	"Options:\n"
	"\t-c\tList contents of filesystem tree (verbose)\n"
	"\t-l\tList contents of filesystem tree (.list format)\n"
	"\t-e\tExtract control files to directory\n"	
	"\t-x\tExctract packages filesystem tree to directory\n"
	"\t-X\tVerbose extract"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DU
#if defined USAGE_ENUM
du_usage_index
#elif defined USAGE_MESSAGES
	"[-ls"
#ifdef BB_FEATURE_HUMAN_READABLE
	"hm"
#endif
	"k] [FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSummarizes disk space used for each FILE and/or directory.\n"
	"Disk space is printed in units of 1024 bytes.\n\n"
	"Options:\n"
	"\t-l\tcount sizes many times if hard linked\n"
	"\t-s\tdisplay only a total for each argument"
#ifdef BB_FEATURE_HUMAN_READABLE
	"\n\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G)\n"
	"\t-m\tprint sizes in megabytes\n"
	"\t-k\tprint sizes in kilobytes(default)"
#else
	"\n\t-k\tprint sizes in kilobytes(compatability)"
#endif
#endif
#endif
DO_COMMA
#endif

#if defined BB_DUMPKMAP
#if defined USAGE_ENUM
dumpkmap_usage_index
#elif defined USAGE_MESSAGES
	"> keymap"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrints out a binary keyboard translation table to standard output"
#endif
#endif
DO_COMMA
#endif

#if defined BB_DUTMP
#if defined USAGE_ENUM
dutmp_usage_index
#elif defined USAGE_MESSAGES
	"[FILE]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nDump utmp file format (pipe delimited) from FILE\n"
	"or stdin to stdout.  (i.e. 'dutmp /var/run/utmp')"
#endif
#endif
DO_COMMA
#endif

#if defined BB_ECHO
#if defined USAGE_ENUM
echo_usage_index
#elif defined USAGE_MESSAGES
	"[-neE] [ARG ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrints the specified ARGs to stdout\n\n"
	"Options:\n"
	"\t-n\tsuppress trailing newline\n"
	"\t-e\tinterpret backslash-escaped characters (i.e. \\t=tab etc)\n"
	"\t-E\tdisable interpretation of backslash-escaped characters"
#endif
#endif
DO_COMMA
#endif

#if defined BB_EXPR
#if defined USAGE_ENUM
expr_usage_index
#elif defined USAGE_MESSAGES
	"EXPRESSION"
#ifndef BB_FEATURE_TRIVIAL_HELP
"\n\nPrints the value of EXPRESSION to standard output.\n\n"
"EXPRESSION may be:\n"
"ARG1 |  ARG2	ARG1 if it is neither null nor 0, otherwise ARG2\n"
"ARG1 &  ARG2	ARG1 if neither argument is null or 0, otherwise 0\n"
"ARG1 <  ARG2	ARG1 is less than ARG2\n"
"ARG1 <= ARG2	ARG1 is less than or equal to ARG2\n"
"ARG1 =  ARG2	ARG1 is equal to ARG2\n"
"ARG1 != ARG2	ARG1 is unequal to ARG2\n"
"ARG1 >= ARG2	ARG1 is greater than or equal to ARG2\n"
"ARG1 >  ARG2	ARG1 is greater than ARG2\n"
"ARG1 +  ARG2	arithmetic sum of ARG1 and ARG2\n"
"ARG1 -  ARG2	arithmetic difference of ARG1 and ARG2\n"
"ARG1 *  ARG2	arithmetic product of ARG1 and ARG2\n"
"ARG1 /  ARG2	arithmetic quotient of ARG1 divided by ARG2\n"
"ARG1 %  ARG2	arithmetic remainder of ARG1 divided by ARG2\n"
"STRING : REGEXP		    anchored pattern match of REGEXP in STRING\n"
"match STRING REGEXP	    same as STRING : REGEXP\n"
"substr STRING POS LENGTH    substring of STRING, POS counted from 1\n"
"index STRING CHARS	    index in STRING where any CHARS is found, or 0\n"
"length STRING		    length of STRING\n"
"quote TOKEN		    interpret TOKEN as a string, even if it is a \n"
"				keyword like `match' or an operator like `/'\n"
"( EXPRESSION )		    value of EXPRESSION\n\n"
"Beware that many operators need to be escaped or quoted for shells.\n"
"Comparisons are arithmetic if both ARGs are numbers, else\n"
"lexicographical.  Pattern matches return the string matched between \n"
"\\( and \\) or null; if \\( and \\) are not used, they return the number \n"
"of characters matched or 0."

#endif
#endif
DO_COMMA
#endif


#if defined BB_TRUE_FALSE
#if defined USAGE_ENUM
false_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nReturn an exit code of FALSE (1)."
#endif
#endif
DO_COMMA
#endif

#if defined BB_FBSET
#if defined USAGE_ENUM
fbset_usage_index
#elif defined USAGE_MESSAGES
	"fbset [options] [mode]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nShows and modifies frame buffer device settings"
#endif
#endif
DO_COMMA
#endif

#if defined BB_FDFLUSH
#if defined USAGE_ENUM
fdflush_usage_index
#elif defined USAGE_MESSAGES
	"DEVICE"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nForces floppy disk drive to detect disk change"
#endif
#endif
DO_COMMA
#endif

#if defined BB_FIND
#if defined USAGE_ENUM
find_usage_index
#elif defined USAGE_MESSAGES
	"[PATH...] [EXPRESSION]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSearch for files in a directory hierarchy.  The default PATH is\n"
	"the current directory; default EXPRESSION is '-print'\n\n"
	"\nEXPRESSION may consist of:\n"
	"\t-follow\t\tDereference symbolic links.\n"
	"\t-name PATTERN\tFile name (leading directories removed) matches PATTERN."
#ifdef BB_FEATURE_FIND_TYPE
	"\n\t-type X\t\tFiletype matches X (where X is one of: f,d,l,b,c,...)"
#endif
#ifdef BB_FEATURE_FIND_PERM
	"\n\t-perm PERMS\tPermissions match any of (+NNN); all of (-NNN);\n\t\t\tor exactly (NNN)"
#endif
#ifdef BB_FEATURE_FIND_MTIME
	"\n\t-mtime TIME\tModified time is greater than (+N); less than (-N);\n\t\t\tor exactly (N) days"
#endif
#endif
#endif
DO_COMMA
#endif

#if defined BB_FREE
#if defined USAGE_ENUM
free_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nDisplays the amount of free and used system memory"
#endif
#endif
DO_COMMA
#endif

#if defined BB_FREERAMDISK
#if defined USAGE_ENUM
freeramdisk_usage_index
#elif defined USAGE_MESSAGES
	"DEVICE"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nFrees all memory used by the specified ramdisk."
#endif
#endif
DO_COMMA
#endif

#if defined BB_FSCK_MINIX
#if defined USAGE_ENUM
fsck_minix_usage_index
#elif defined USAGE_MESSAGES
	"[-larvsmf] /dev/name"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPerforms a consistency check for MINIX filesystems.\n\n"
	"Options:\n"
	"\t-l\tLists all filenames\n"
	"\t-r\tPerform interactive repairs\n"
	"\t-a\tPerform automatic repairs\n"
	"\t-v\tverbose\n"
	"\t-s\tOutputs super-block information\n"
	"\t-m\tActivates MINIX-like \"mode not cleared\" warnings\n"
	"\t-f\tForce file system check."
#endif
#endif
DO_COMMA
#endif

#if defined BB_GETOPT
#if defined USAGE_ENUM
getopt_usage_index
#elif defined USAGE_MESSAGES
	"[OPTIONS]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nParse command options\n"
	"  -a, --alternative            Allow long options starting with single -\n"
	"  -l, --longoptions=longopts   Long options to be recognized\n"
	"  -n, --name=progname          The name under which errors are reported\n"
	"  -o, --options=optstring      Short options to be recognized\n"
	"  -q, --quiet                  Disable error reporting by getopt(3)\n"
	"  -Q, --quiet-output           No normal output\n"
	"  -s, --shell=shell            Set shell quoting conventions\n"
	"  -T, --test                   Test for getopt(1) version\n"
	"  -u, --unqote                 Do not quote the output"
#endif
#endif
DO_COMMA
#endif

#if defined BB_GREP
#if defined USAGE_ENUM
grep_usage_index
#elif defined USAGE_MESSAGES
	"[-ihHnqvs] pattern [files...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSearch for PATTERN in each FILE or standard input.\n\n"
	"Options:\n"
	"\t-H\tprefix output lines with filename where match was found\n"
	"\t-h\tsuppress the prefixing filename on output\n"
	"\t-i\tignore case distinctions\n"
	"\t-n\tprint line number with output lines\n"
	"\t-q\tbe quiet. Returns 0 if result was found, 1 otherwise\n"
	"\t-v\tselect non-matching lines\n"
	"\t-s\tsuppress file open/read error messages"
#endif
#endif
DO_COMMA
#endif

#if defined BB_GUNZIP
#if defined USAGE_ENUM
gunzip_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... FILE"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nUncompress FILE (or standard input if FILE is '-').\n\n"
	"Options:\n"
	"\t-c\tWrite output to standard output\n"
	"\t-t\tTest compressed file integrity"
#endif
#endif
DO_COMMA
#endif

#if defined BB_GZIP
#if defined USAGE_ENUM
gzip_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... FILE"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCompress FILE with maximum compression.\n"
	"When FILE is '-', reads standard input.  Implies -c.\n\n"
	"Options:\n"
	"\t-c\tWrite output to standard output instead of FILE.gz\n"
	"\t-d\tdecompress"
#endif
#endif
DO_COMMA
#endif

#if defined BB_HALT
#if defined USAGE_ENUM
halt_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nHalt the system."
#endif
#endif
DO_COMMA
#endif

#if defined BB_HEAD
#if defined USAGE_ENUM
head_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] [FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint first 10 lines of each FILE to standard output.\n"
	"With more than one FILE, precede each with a header giving the\n"
	"file name. With no FILE, or when FILE is -, read standard input.\n\n"

	"Options:\n" "\t-n NUM\t\tPrint first NUM lines instead of first 10"
#endif
#endif
DO_COMMA
#endif

#if defined BB_HOSTID
#if defined USAGE_ENUM
hostid_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint out a unique 32-bit identifier for the machine."
#endif
#endif
DO_COMMA
#endif

#if defined BB_HOSTNAME
#if defined USAGE_ENUM
hostname_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] {hostname | -F file}"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nGet or set the hostname or DNS domain name. If a hostname is given\n"
	"(or a file with the -F parameter), the host name will be set.\n\n"

	"Options:\n"
	"\t-s\t\tShort\n"
	"\t-i\t\tAddresses for the hostname\n"
	"\t-d\t\tDNS domain name\n"
	"\t-F, --file FILE\tUse the contents of FILE to specify the hostname"
#endif
#endif
DO_COMMA
#endif

#if defined BB_ID
#if defined USAGE_ENUM
id_usage_index
#elif defined USAGE_MESSAGES
	"[OPTIONS]... [USERNAME]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint information for USERNAME or the current user\n\n"
	"Options:\n"
	"\t-g\tprints only the group ID\n"
	"\t-u\tprints only the user ID\n"
	"\t-n\tprint a name instead of a number (with for -ug)\n"
	"\t-r\tprints the real user ID instead of the effective ID (with -ug)"
#endif
#endif
DO_COMMA
#endif

#if defined BB_IFCONFIG
#if defined USAGE_ENUM
ifconfig_usage_index
#elif defined USAGE_MESSAGES
	"[-a] [-i] [-v] <interface> [<address>]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nconfigure a network interface\n\n"
	"Options:\n"
	"  [[-]broadcast [<address>]]  [[-]pointopoint [<address>]]\n"
	"  [netmask <address>]  [dstaddr <address>]  [tunnel <adress>]\n"
#ifdef SIOCSKEEPALIVE
	"  [outfill <NN>] [keepalive <NN>]\n"
#endif
	"  [hw ether <address>]  [metric <NN>]  [mtu <NN>]\n"
	"  [[-]trailers]  [[-]arp]  [[-]allmulti]\n"
	"  [multicast]  [[-]promisc]\n"
	"  [mem_start <NN>]  [io_addr <NN>]  [irq <NN>]  [media <type>]\n"
	"  [up|down] ..."
#endif
#endif
DO_COMMA
#endif

#if defined BB_INIT
#if defined USAGE_ENUM
init_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nInit is the parent of all processes.\n\n"
	"This version of init is designed to be run only by the kernel."
#endif
#endif
DO_COMMA
#endif

#if defined BB_INSMOD
#if defined USAGE_ENUM
insmod_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... MODULE [symbol=value]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nLoads the specified kernel modules into the kernel.\n\n"
	"Options:\n"
	"\t-f\tForce module to load into the wrong kernel version.\n"
	"\t-k\tMake module autoclean-able.\n"
	"\t-v\tverbose output\n" 
	"\t-L\tLock to prevent simultaneous loads of a module\n"
	"\t-x\tdo not export externs"
#endif
#endif
DO_COMMA
#endif

#if defined BB_KILL
#if defined USAGE_ENUM
kill_usage_index
#elif defined USAGE_MESSAGES
	"[-signal] process-id [process-id ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSend a signal (default is SIGTERM) to the specified process(es).\n\n"
	"Options:\n" "\t-l\tList all signal names and numbers."
#endif
#endif
DO_COMMA
#endif

#if defined BB_KILLALL
#if defined USAGE_ENUM
killall_usage_index
#elif defined USAGE_MESSAGES
	"[-signal] process-name [process-name ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSend a signal (default is SIGTERM) to the specified process(es).\n\n"
	"Options:\n" "\t-l\tList all signal names and numbers."
#endif
#endif
DO_COMMA
#endif

#if defined BB_LENGTH
#if defined USAGE_ENUM
length_usage_index
#elif defined USAGE_MESSAGES
	"STRING"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrints out the length of the specified STRING."
#endif
#endif
DO_COMMA
#endif

#if defined BB_LN
#if defined USAGE_ENUM
ln_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] TARGET... LINK_NAME|DIRECTORY"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCreate a link named LINK_NAME or DIRECTORY to the specified TARGET\n"
	"\nYou may use '--' to indicate that all following arguments are non-options.\n\n"
	"Options:\n"
	"\t-s\tmake symbolic links instead of hard links\n"
	"\t-f\tremove existing destination files\n"
	"\t-n\tno dereference symlinks - treat like normal file"
#endif
#endif
DO_COMMA
#endif

#if defined BB_LOADACM
#if defined USAGE_ENUM
loadacm_usage_index
#elif defined USAGE_MESSAGES
	"< mapfile"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nLoads an acm from standard input."
#endif
#endif
DO_COMMA
#endif

#if defined BB_LOADFONT
#if defined USAGE_ENUM
loadfont_usage_index
#elif defined USAGE_MESSAGES
	"< font"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nLoads a console font from standard input."
#endif
#endif
DO_COMMA
#endif

#if defined BB_LOADKMAP
#if defined USAGE_ENUM
loadkmap_usage_index
#elif defined USAGE_MESSAGES
	"< keymap"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nLoads a binary keyboard translation table from standard input."
#endif
#endif
DO_COMMA
#endif

#if defined BB_LOGGER
#if defined USAGE_ENUM
logger_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [MESSAGE]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nWrite MESSAGE to the system log.  If MESSAGE is omitted, log stdin.\n\n"
	"Options:\n"
	"\t-s\tLog to stderr as well as the system log.\n"
	"\t-t\tLog using the specified tag (defaults to user name).\n"
	"\t-p\tEnter the message with the specified priority.\n"
	"\t\tThis may be numerical or a ``facility.level'' pair."
#endif
#endif
DO_COMMA
#endif

#if defined BB_LOGNAME
#if defined USAGE_ENUM
logname_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint the name of the current user."
#endif
#endif
DO_COMMA
#endif

#if defined BB_LS
#if defined USAGE_ENUM
ls_usage_index
#elif defined USAGE_MESSAGES
	"[-1Aa"
#ifdef BB_FEATURE_LS_TIMESTAMPS
	"c"
#endif
	"Cd"
#ifdef BB_FEATURE_LS_TIMESTAMPS
	"e"
#endif
#ifdef BB_FEATURE_LS_FILETYPES
	"F"
#endif
	"iln"
#ifdef BB_FEATURE_LS_FILETYPES
	"p"
#endif
#ifdef BB_FEATURE_LS_FOLLOWLINKS
    "L"
#endif
#ifdef BB_FEATURE_LS_RECURSIVE
	"R"
#endif
#ifdef BB_FEATURE_LS_SORTFILES
	"rS"
#endif
	"s"
#ifdef BB_FEATURE_AUTOWIDTH
	"T"
#endif
#ifdef BB_FEATURE_LS_TIMESTAMPS
	"tu"
#endif
#ifdef BB_FEATURE_LS_SORTFILES
	"v"
#endif
#ifdef BB_FEATURE_AUTOWIDTH
	"w"
#endif
	"x"
#ifdef BB_FEATURE_LS_SORTFILES
	"X"
#endif
#ifdef BB_FEATURE_HUMAN_READABLE
	"h"
#endif
	"k] [filenames...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nList directory contents\n\n"
	"Options:\n"
	"\t-1\tlist files in a single column\n"
	"\t-A\tdo not list implied . and ..\n"
	"\t-a\tdo not hide entries starting with .\n"
	"\t-C\tlist entries by columns\n"
#ifdef BB_FEATURE_LS_TIMESTAMPS
	"\t-c\twith -l: show ctime (the time of last\n"
	"\t\tmodification of file status information)\n"
#endif
	"\t-d\tlist directory entries instead of contents\n"
#ifdef BB_FEATURE_LS_TIMESTAMPS
	"\t-e\tlist both full date and full time\n"
#endif
#ifdef BB_FEATURE_LS_FILETYPES
	"\t-F\tappend indicator (one of */=@|) to entries\n"
#endif
	"\t-i\tlist the i-node for each file\n"
	"\t-l\tuse a long listing format\n"
	"\t-n\tlist numeric UIDs and GIDs instead of names\n"
#ifdef BB_FEATURE_LS_FILETYPES
	"\t-p\tappend indicator (one of /=@|) to entries\n"
#endif
#ifdef BB_FEATURE_LS_FOLLOWLINKS
	"\t-L\tlist entries pointed to by symbolic links\n"
#endif
#ifdef BB_FEATURE_LS_RECURSIVE
	"\t-R\tlist subdirectories recursively\n"
#endif
#ifdef BB_FEATURE_LS_SORTFILES
	"\t-r\tsort the listing in reverse order\n"
	"\t-S\tsort the listing by file size\n"
#endif
	"\t-s\tlist the size of each file, in blocks\n"
#ifdef BB_FEATURE_AUTOWIDTH
	"\t-T NUM\tassume Tabstop every NUM columns\n"
#endif
#ifdef BB_FEATURE_LS_TIMESTAMPS
	"\t-t\twith -l: show modification time (the time of last\n"
	"\t\tchange of the file)\n"
	"\t-u\twith -l: show access time (the time of last\n"
	"\t\taccess of the file)\n"
#endif
#ifdef BB_FEATURE_LS_SORTFILES
	"\t-v\tsort the listing by version\n"
#endif
#ifdef BB_FEATURE_AUTOWIDTH
	"\t-w NUM\tassume the terminal is NUM columns wide\n"
#endif
	"\t-x\tlist entries by lines instead of by columns\n"
#ifdef BB_FEATURE_LS_SORTFILES
	"\t-X\tsort the listing by extension\n"
#endif

#ifdef BB_FEATURE_HUMAN_READABLE
	"\t-h\tprint sizes in human readable format (e.g., 1K 243M 2G)\n"
	"\t-k\tprint sizes in kilobytes(default)"
#else
	"\t-k\tprint sizes in kilobytes(compatability)"
#endif

#endif /*  BB_FEATURE_TRIVIAL_HELP */
#endif
DO_COMMA
#endif /* BB_LS */

#if defined BB_LSMOD
#if defined USAGE_ENUM
lsmod_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nList the currently loaded kernel modules."
#endif
#endif
DO_COMMA
#endif

#if defined BB_MAKEDEVS
#if defined USAGE_ENUM
makedevs_usage_index
#elif defined USAGE_MESSAGES
	"NAME TYPE MAJOR MINOR FIRST LAST [s]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCreates a range of block or character special files\n\n"
	"TYPEs include:\n"
	"\tb:\tMake a block (buffered) device.\n"
	"\tc or u:\tMake a character (un-buffered) device.\n"
	"\tp:\tMake a named pipe. MAJOR and MINOR are ignored for named pipes.\n\n"
	"FIRST specifies the number appended to NAME to create the first device.\n"
	"LAST specifies the number of the last item that should be created.\n"
	"If 's' is the last argument, the base device is created as well.\n\n"
	"For example:\n"
	"\tmakedevs /dev/ttyS c 4 66 2 63   ->  ttyS2-ttyS63\n"
	"\tmakedevs /dev/hda b 3 0 0 8 s    ->  hda,hda1-hda8"
#endif
#endif
DO_COMMA
#endif

#if defined BB_MD5SUM
#if defined USAGE_ENUM
md5sum_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] [FILE]...\n"
	"or:    md5sum [OPTION] -c [FILE]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint or check MD5 checksums.\n\n"
	"Options:\n"
	"With no FILE, or when FILE is -, read standard input.\n\n"
	"\t-b\tread files in binary mode\n"
	"\t-c\tcheck MD5 sums against given list\n"
	"\t-t\tread files in text mode (default)\n"
	"\t-g\tread a string\n"
	"\nThe following two options are useful only when verifying checksums:\n"
	"\t-s,\tdon't output anything, status code shows success\n"
	"\t-w,\twarn about improperly formated MD5 checksum lines"
#endif
#endif
DO_COMMA
#endif

#if defined BB_MKDIR
#if defined USAGE_ENUM
mkdir_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] DIRECTORY..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCreate the DIRECTORY(ies), if they do not already exist\n\n"
	
	"Options:\n"
	"\t-m\tset permission mode (as in chmod), not rwxrwxrwx - umask\n"
	"\t-p\tno error if existing, make parent directories as needed"
#endif
#endif
DO_COMMA
#endif

#if defined BB_MKFIFO
#if defined USAGE_ENUM
mkfifo_usage_index
#elif defined USAGE_MESSAGES
	"[OPTIONS] name"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCreates a named pipe (identical to 'mknod name p')\n\n"
	"Options:\n"
	"\t-m\tcreate the pipe using the specified mode (default a=rw)"
#endif
#endif
DO_COMMA
#endif

#if defined BB_MKFS_MINIX
#if defined USAGE_ENUM
mkfs_minix_usage_index
#elif defined USAGE_MESSAGES
	"[-c | -l filename] [-nXX] [-iXX] /dev/name [blocks]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nMake a MINIX filesystem.\n\n"
	"Options:\n"
	"\t-c\t\tCheck the device for bad blocks\n"
	"\t-n [14|30]\tSpecify the maximum length of filenames\n"
	"\t-i INODES\tSpecify the number of inodes for the filesystem\n"
	"\t-l FILENAME\tRead the bad blocks list from FILENAME\n"
	"\t-v\t\tMake a Minix version 2 filesystem"
#endif
#endif
DO_COMMA
#endif

#if defined BB_MKNOD
#if defined USAGE_ENUM
mknod_usage_index
#elif defined USAGE_MESSAGES
	"[OPTIONS] NAME TYPE MAJOR MINOR"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCreate a special file (block, character, or pipe).\n\n"
	"Options:\n"
	"\t-m\tcreate the special file using the specified mode (default a=rw)\n\n"
	"TYPEs include:\n"
	"\tb:\tMake a block (buffered) device.\n"
	"\tc or u:\tMake a character (un-buffered) device.\n"
	"\tp:\tMake a named pipe. MAJOR and MINOR are ignored for named pipes."
#endif
#endif
DO_COMMA
#endif

#if defined BB_MKSWAP
#if defined USAGE_ENUM
mkswap_usage_index
#elif defined USAGE_MESSAGES
	"[-c] [-v0|-v1] device [block-count]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrepare a disk partition to be used as a swap partition.\n\n"
	"Options:\n" "\t-c\t\tCheck for read-ability.\n"
	"\t-v0\t\tMake version 0 swap [max 128 Megs].\n"
	"\t-v1\t\tMake version 1 swap [big!] (default for kernels >\n"
	"\t\t\t2.1.117).\n"
	"\tblock-count\tNumber of block to use (default is entire partition)."
#endif
#endif
DO_COMMA
#endif

#if defined BB_MKTEMP
#if defined USAGE_ENUM
mktemp_usage_index
#elif defined USAGE_MESSAGES
	"[-q] TEMPLATE"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCreates a temporary file with its name based on TEMPLATE.\n"
	"TEMPLATE is any name with six `Xs' (i.e. /tmp/temp.XXXXXX)."
#endif
#endif
DO_COMMA
#endif

#if defined BB_MORE
#if defined USAGE_ENUM
more_usage_index
#elif defined USAGE_MESSAGES
	"[FILE ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nMore is a filter for viewing FILE one screenful at a time."
#endif
#endif
DO_COMMA
#endif

#if defined BB_MOUNT
#if defined USAGE_ENUM
mount_usage_index
#elif defined USAGE_MESSAGES
	"[flags] device directory [-o options,more-options]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nMount a filesystem\n\n"
	"Flags:\n" 
	"\t-a:\t\tMount all filesystems in fstab.\n"
#ifdef BB_MTAB
	"\t-f:\t\t\"Fake\" Add entry to mount table but don't mount it.\n"
	"\t-n:\t\tDon't write a mount table entry.\n"
#endif
	"\t-o option:\tOne of many filesystem options, listed below.\n"
	"\t-r:\t\tMount the filesystem read-only.\n"
	"\t-t fs-type:\tSpecify the filesystem type.\n"
	"\t-w:\t\tMount for reading and writing (default).\n"
	"\n"
	"Options for use with the \"-o\" flag:\n"
	"\tasync/sync:\tWrites are asynchronous / synchronous.\n"
	"\tatime/noatime:\tEnable / disable updates to inode access times.\n"
	"\tdev/nodev:\tAllow use of special device files / disallow them.\n"
	"\texec/noexec:\tAllow use of executable files / disallow them.\n"
#if defined BB_FEATURE_MOUNT_LOOP
	"\tloop:\t\tMounts a file via loop device.\n"
#endif
	"\tsuid/nosuid:\tAllow set-user-id-root programs / disallow them.\n"
	"\tremount:\tRe-mount a mounted filesystem, changing its flags.\n"
	"\tro/rw:\t\tMount for read-only / read-write.\n"
	"\nThere are EVEN MORE flags that are specific to each filesystem.\n"
	"You'll have to see the written documentation for those."
#endif
#endif
DO_COMMA
#endif

#if defined BB_MT
#if defined USAGE_ENUM
mt_usage_index
#elif defined USAGE_MESSAGES
	"[-f device] opcode value"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nControl magnetic tape drive operation\n"
	"\nAvailable Opcodes:\n\n"
	"bsf bsfm bsr bss datacompression drvbuffer eof eom erase\n"
	"fsf fsfm fsr fss load lock mkpart nop offline ras1 ras2\n"
	"ras3 reset retension rew rewoffline seek setblk setdensity\n"
	"setpart tell unload unlock weof wset"
#endif
#endif
DO_COMMA
#endif

#if defined BB_CP_MV
#if defined USAGE_ENUM
mv_usage_index
#elif defined USAGE_MESSAGES
	"SOURCE DEST\n"
	"   or: mv SOURCE... DIRECTORY"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nRename SOURCE to DEST, or move SOURCE(s) to DIRECTORY."
#endif
#endif
DO_COMMA
#endif

#if defined BB_NC
#if defined USAGE_ENUM
nc_usage_index
#elif defined USAGE_MESSAGES
	"[IP] [port]" 
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nNetcat opens a pipe to IP:port"
#endif
#endif
DO_COMMA
#endif

#if defined BB_NSLOOKUP
#if defined USAGE_ENUM
nslookup_usage_index
#elif defined USAGE_MESSAGES
	"[HOST]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nQueries the nameserver for the IP address of the given HOST"
#endif
#endif
DO_COMMA
#endif

#if defined BB_PING
#if defined BB_FEATURE_SIMPLE_PING
#if defined USAGE_ENUM
ping_usage_index
#elif defined USAGE_MESSAGES
	"host"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSend ICMP ECHO_REQUEST packets to network hosts"
#endif
#endif
DO_COMMA
#else /* ! defined BB_FEATURE_SIMPLE_PING */
#if defined USAGE_ENUM
ping_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... host"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSend ICMP ECHO_REQUEST packets to network hosts.\n\n"
	"Options:\n"
	"\t-c COUNT\tSend only COUNT pings.\n"
	"\t-s SIZE\t\tSend SIZE data bytes in packets (default=56).\n"
	"\t-q\t\tQuiet mode, only displays output at start\n"
	"\t\t\tand when finished."
#endif
#endif
DO_COMMA
#endif
#endif

#if defined BB_PIVOT_ROOT
#if defined USAGE_ENUM
pivot_root_usage_index
#elif defined USAGE_MESSAGES
	"new_root put_old"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nMove the current root file system to put_old and make new_root\n"
	"the new root file system."
#endif
#endif
DO_COMMA
#endif

#if defined BB_POWEROFF
#if defined USAGE_ENUM
poweroff_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nHalt the system and request that the kernel shut off the power."
#endif
#endif
DO_COMMA
#endif

#if defined BB_PRINTF
#if defined USAGE_ENUM
printf_usage_index
#elif defined USAGE_MESSAGES
	"FORMAT [ARGUMENT...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nFormats and prints ARGUMENT(s) according to FORMAT,\n"
	"Where FORMAT controls the output exactly as in C printf."
#endif
#endif
DO_COMMA
#endif

#if defined BB_PS
#if defined USAGE_ENUM
ps_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nReport process status\n"
	"\nThis version of ps accepts no options."
#endif
#endif
DO_COMMA
#endif

#if defined BB_PWD
#if defined USAGE_ENUM
pwd_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint the full filename of the current working directory."
#endif
#endif
DO_COMMA
#endif

#if defined BB_RDATE
#if defined USAGE_ENUM
rdate_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] HOST"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nGet and possibly set the system date and time from a remote HOST.\n"
	"Options:\n"
	"\t-s\tSet the system date and time (default).\n"
	"\t-p\tPrint the date and time."
#endif
#endif
DO_COMMA
#endif

#if defined BB_READLINK
#if defined USAGE_ENUM
readlink_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nRead a symbolic link."
#endif
#endif
DO_COMMA
#endif

#if defined BB_REBOOT
#if defined USAGE_ENUM
reboot_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nReboot the system."
#endif
#endif
DO_COMMA
#endif
	
#if defined BB_RENICE
#if defined USAGE_ENUM
renice_usage_index
#elif defined USAGE_MESSAGES
	"priority pid [pid ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nChanges priority of running processes. Allowed priorities range\n"
	"from 20 (the process runs only when nothing else is running) to 0\n"
	"(default priority) to -20 (almost nothing else ever gets to run)."
#endif
#endif
DO_COMMA
#endif


#if defined BB_RESET
#if defined USAGE_ENUM
reset_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nResets the screen."
#endif
#endif
DO_COMMA
#endif

#if defined BB_RM
#if defined USAGE_ENUM
rm_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... FILE..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nRemove (unlink) the FILE(s).  You may use '--' to\n"
	"indicate that all following arguments are non-options.\n\n"
	"Options:\n"
	"\t-f\t\tremove existing destinations, never prompt\n"
	"\t-r or -R\tremove the contents of directories recursively"
#endif
#endif
DO_COMMA
#endif

#if defined BB_RMDIR
#if defined USAGE_ENUM
rmdir_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... DIRECTORY..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nRemove the DIRECTORY(ies), if they are empty."
#endif
#endif
DO_COMMA
#endif

#if defined BB_RMMOD
#if defined USAGE_ENUM
rmmod_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [MODULE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nUnloads the specified kernel modules from the kernel.\n\n"
	"Options:\n" 
	"\t-a\tTry to remove all unused kernel modules."
#endif
#endif
DO_COMMA
#endif

#if defined BB_ROUTE
#if defined USAGE_ENUM
route_usage_index
#elif defined USAGE_MESSAGES
	"[{add|del|flush}]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nEdit the kernel's routing tables"
#endif
#endif
DO_COMMA
#endif

#if defined BB_RPMUNPACK
#if defined USAGE_ENUM
rpmunpack_usage_index
#elif defined USAGE_MESSAGES
	"< package.rpm | gunzip | cpio -idmuv"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nExtracts an rpm archive."
#endif
#endif
DO_COMMA
#endif

#if defined BB_SED
#if defined USAGE_ENUM
sed_usage_index
#elif defined USAGE_MESSAGES
	"[-Vhnef] pattern [files...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\n"
	"-n\t\tsuppress automatic printing of pattern space\n"
	"-e script\tadd the script to the commands to be executed\n"
	"-f scriptfile\tadd the contents of script-file to the commands to be executed\n"
	"\n"
	"If no -e or -f is given, the first non-option argument is taken as the\n"
	"sed script to interpret. All remaining arguments are names of input\n"
	"files; if no input files are specified, then the standard input is read."
#endif
#endif
DO_COMMA
#endif

#if defined BB_SETKEYCODES
#if defined USAGE_ENUM
setkeycodes_usage_index
#elif defined USAGE_MESSAGES
	"SCANCODE KEYCODE ..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSet entries into the kernel's scancode-to-keycode map,\n"
	"allowing unusual keyboards to generate usable keycodes.\n\n" 
	"SCANCODE may be either xx or e0xx (hexadecimal),\n"
	"and KEYCODE is given in decimal"
#endif
#endif
DO_COMMA
#endif

#if defined BB_SH
#if defined USAGE_ENUM
shell_usage_index
#elif defined USAGE_MESSAGES
	"[FILE]...\n"
	"   or: sh -c command [args]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nlash: The BusyBox command interpreter (shell)."
#endif
#endif
DO_COMMA
#endif

#if defined BB_SLEEP
#if defined USAGE_ENUM
sleep_usage_index
#elif defined USAGE_MESSAGES
	"N" 
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPause for N seconds."
#endif
#endif
DO_COMMA
#endif

#if defined BB_SORT
#if defined USAGE_ENUM
sort_usage_index
#elif defined USAGE_MESSAGES
	"[-n]"
#ifdef BB_FEATURE_SORT_REVERSE
	" [-r]"
#endif
	" [FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nSorts lines of text in the specified files"
#endif
#endif
DO_COMMA
#endif

#if defined BB_STTY
#if defined USAGE_ENUM
stty_usage_index
#elif defined USAGE_MESSAGES
	"stty [-a|g] [-F device] [SETTING]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nWithout arguments, prints baud rate, line discipline,"
	"\nand deviations from stty sane."
	"\n -F device  open and use the specified device instead of stdin"
	"\n -a         print all current settings in human-readable form. Or"
	"\n -g         print in a stty-readable form"
	"\n [SETTING]  see in documentation"
#endif
#endif
DO_COMMA
#endif

#if defined BB_SWAPONOFF
#if defined USAGE_ENUM
swapoff_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] [device]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nStop swapping virtual memory pages on the given device.\n\n"
	"Options:\n"
	"\t-a\tStop swapping on all swap devices"
#endif
#endif
DO_COMMA
#endif

#if defined BB_SWAPONOFF
#if defined USAGE_ENUM
swapon_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] [device]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nStart swapping virtual memory pages on the given device.\n\n"
	"Options:\n"
	"\t-a\tStart swapping on all swap devices"
#endif
#endif
DO_COMMA
#endif

#if defined BB_SYNC
#if defined USAGE_ENUM
sync_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nWrite all buffered filesystem blocks to disk."
#endif
#endif
DO_COMMA
#endif

#if defined BB_SYSLOGD
#if defined USAGE_ENUM
syslogd_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nLinux system and kernel (provides klogd) logging utility.\n"
	"Note that this version of syslogd/klogd ignores /etc/syslog.conf.\n\n"
	"Options:\n"
	"\t-m NUM\t\tInterval between MARK lines (default=20min, 0=off)\n"
	"\t-n\t\tRun as a foreground process\n"
#ifdef BB_FEATURE_KLOGD
	"\t-K\t\tDo not start up the klogd process\n"
#endif
	"\t-O FILE\t\tUse an alternate log file (default=/var/log/messages)"
#ifdef BB_FEATURE_REMOTE_LOG
	"\n\t-R HOST[:PORT]\t\tLog remotely to IP or hostname on PORT (default PORT=514/UDP)\n"
	"\t-L\t\tLog locally as well as network logging (default is network only)"
#endif
#endif
#endif
DO_COMMA
#endif

#if defined BB_TAIL
#if defined USAGE_ENUM
tail_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint last 10 lines of each FILE to standard output.\n"
	"With more than one FILE, precede each with a header giving the\n"
	"file name. With no FILE, or when FILE is -, read standard input.\n\n"
	"Options:\n"
#ifndef BB_FEATURE_SIMPLE_TAIL
	"\t-c N[kbm]\toutput the last N bytes\n"
#endif
	"\t-n N[kbm]\tprint last N lines instead of last 10\n"
	"\t-f\t\toutput data as the file grows"
#ifndef BB_FEATURE_SIMPLE_TAIL
	"\n\t-q\t\tnever output headers giving file names\n"
	"\t-s SEC\t\twait SEC seconds between reads with -f\n"
	"\t-v\t\talways output headers giving file names\n\n"
	"If the first character of N (bytes or lines) is a `+', output begins with \n"
	"the Nth item from the start of each file, otherwise, print the last N items\n"
	"in the file. N bytes may be suffixed by k (x1024), b (x512), or m (1024^2)."
//#else
//	"\nIf the first character of N (bytes or lines) is a `+', output begins with \n"
//	"the Nth item from the start of each file."
#endif
#endif
#endif
DO_COMMA
#endif

#if defined BB_TAR
#if defined USAGE_ENUM
tar_usage_index
#elif defined USAGE_MESSAGES
#ifdef BB_FEATURE_TAR_CREATE
	"-[cxtvO] "
#else
	"-[xtvO] "
#endif
#if defined BB_FEATURE_TAR_EXCLUDE
	"[--exclude File] "
        "[-X File]"
#endif
	"[-f tarFile] [FILE(s)] ..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCreate, extract, or list files from a tar file.\n\n"
	"Main operation mode:\n"
#ifdef BB_FEATURE_TAR_CREATE
	"\tc\t\tcreate\n"
#endif
	"\tx\t\textract\n"
	"\tt\t\tlist\n"
	"\nFile selection:\n"
	"\tf\t\tname of tarfile or \"-\" for stdin\n"
	"\tO\t\textract to stdout\n"
#if defined BB_FEATURE_TAR_EXCLUDE
	"\texclude\t\tfile to exclude\n"
        "\tX\t\tfile with names to exclude\n"
#endif
	"\nInformative output:\n"
	"\tv\t\tverbosely list files processed"
#endif
#endif
DO_COMMA
#endif

#if defined BB_TEE
#if defined USAGE_ENUM
tee_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nCopy standard input to each FILE, and also to standard output.\n\n"
	"Options:\n" "\t-a\tappend to the given FILEs, do not overwrite"
#endif
#endif
DO_COMMA
#endif

#if defined BB_TELNET
#if defined USAGE_ENUM
telnet_usage_index
#elif defined USAGE_MESSAGES
	"host [port]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nTelnet is used to establish interactive communication with another\n"
	"computer over a network using the TELNET protocol."
#endif
#endif
DO_COMMA
#endif

#if defined BB_TEST
#if defined USAGE_ENUM
test_usage_index
#elif defined USAGE_MESSAGES
	"\ntest EXPRESSION\n"
	"or   [ EXPRESSION ]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nChecks file types and compares values returning an exit\n"
	"code determined by the value of EXPRESSION."
#endif
#endif
DO_COMMA
#endif

#if defined BB_TOUCH
#if defined USAGE_ENUM
touch_usage_index
#elif defined USAGE_MESSAGES
	"[-c] file [file ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nUpdate the last-modified date on the given file[s].\n\n"
	"Options:\n"
	"\t-c\tDo not create any files"
#endif
#endif
DO_COMMA
#endif

#if defined BB_TR
#if defined USAGE_ENUM
tr_usage_index
#elif defined USAGE_MESSAGES
	"[-cds] STRING1 [STRING2]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nTranslate, squeeze, and/or delete characters from\n"
	"standard input, writing to standard output.\n\n"
	"Options:\n"
	"\t-c\ttake complement of STRING1\n"
	"\t-d\tdelete input characters coded STRING1\n"
	"\t-s\tsqueeze multiple output characters of STRING2 into one character"
#endif
#endif
DO_COMMA
#endif

#if defined BB_TRUE_FALSE
#if defined USAGE_ENUM
true_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nReturn an exit code of TRUE (0)."
#endif
#endif
DO_COMMA
#endif

#if defined BB_TTY
#if defined USAGE_ENUM
tty_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint the file name of the terminal connected to standard input.\n\n"
	"Options:\n"
	"\t-s\tprint nothing, only return an exit status"
#endif
#endif
DO_COMMA
#endif

#if defined BB_UMOUNT
#if defined USAGE_ENUM
umount_usage_index
#elif defined USAGE_MESSAGES
	"[flags] filesystem|directory"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nUnmount file systems\n"
	"\nFlags:\n" "\t-a:\tUnmount all file systems"
#ifdef BB_MTAB
	" in /etc/mtab\n\t-n:\tDon't erase /etc/mtab entries\n"
#else
	"\n"
#endif
	"\t-r:\tTry to remount devices as read-only if mount is busy"
#if defined BB_FEATURE_MOUNT_FORCE
	"\n\t-f:\tForce filesystem umount (i.e. unreachable NFS server)"
#endif
#if defined BB_FEATURE_MOUNT_LOOP
	"\n\t-l:\tDo not free loop device (if a loop device has been used)"
#endif
#endif
#endif
DO_COMMA
#endif

#if defined BB_UNAME
#if defined USAGE_ENUM
uname_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint certain system information.  With no OPTION, same as -s.\n\n"
	"Options:\n"
	"\t-a\tprint all information\n"
	"\t-m\tthe machine (hardware) type\n"
	"\t-n\tprint the machine's network node hostname\n"
	"\t-r\tprint the operating system release\n"
	"\t-s\tprint the operating system name\n"

	"\t-p\tprint the host processor type\n"
	"\t-v\tprint the operating system version"
#endif
#endif
DO_COMMA
#endif

#if defined BB_UNIQ
#if defined USAGE_ENUM
uniq_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [INPUT [OUTPUT]]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nDiscard all but one of successive identical lines from INPUT\n"
	"(or standard input), writing to OUTPUT (or standard output).\n"
	"Options:\n"
	"\t-c\tprefix lines by the number of occurrences\n"
	"\t-d\tonly print duplicate lines\n"
	"\t-u\tonly print unique lines"
#endif
#endif
DO_COMMA
#endif

#if defined BB_UNIX2DOS
#if defined USAGE_ENUM
unix2dos_usage_index
#elif defined USAGE_MESSAGES
	"< unixfile > dosfile"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nConverts a text file from unix format to dos format."
#endif
#endif
DO_COMMA
#endif

#if defined BB_UPDATE
#if defined USAGE_ENUM
update_usage_index
#elif defined USAGE_MESSAGES
	"[options]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPeriodically flushes filesystem buffers.\n\n"
	"Options:\n"
	"\t-S\tforce use of sync(2) instead of flushing\n"
	"\t-s SECS\tcall sync this often (default 30)\n"
	"\t-f SECS\tflush some buffers this often (default 5)"
#endif
#endif
DO_COMMA
#endif

#if defined BB_UPTIME
#if defined USAGE_ENUM
uptime_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nDisplay the time since the last boot."
#endif
#endif
DO_COMMA
#endif

#if defined BB_USLEEP
#if defined USAGE_ENUM
usleep_usage_index
#elif defined USAGE_MESSAGES
	"N" 
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPause for N microseconds."
#endif
#endif
DO_COMMA
#endif

#if defined BB_UUDECODE
#if defined USAGE_ENUM
uudecode_usage_index
#elif defined USAGE_MESSAGES
	"[FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nUudecode a file that is uuencoded.\n\n"
	"Options:\n"
	"\t-o FILE\tdirect output to FILE"
#endif
#endif
DO_COMMA
#endif

#if defined BB_UUENCODE
#if defined USAGE_ENUM
uuencode_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION] [INFILE] REMOTEFILE"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nUuencode a file.\n\n"
	"Options:\n"
	"\t-m\tuse base64 encoding as of RFC1521"
#endif
#endif
DO_COMMA
#endif

#if defined BB_WATCHDOG
#if defined USAGE_ENUM
watchdog_usage_index
#elif defined USAGE_MESSAGES
	"dev"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPeriodically write to watchdog device \"dev\""
#endif
#endif
DO_COMMA
#endif

#if defined BB_WC
#if defined USAGE_ENUM
wc_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [FILE]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrint line, word, and byte counts for each FILE, and a total line if\n"
	"more than one FILE is specified.  With no FILE, read standard input.\n\n"
	"Options:\n"
	"\t-c\tprint the byte counts\n"
	"\t-l\tprint the newline counts\n"

	"\t-L\tprint the length of the longest line\n"
	"\t-w\tprint the word counts"
#endif
#endif
DO_COMMA
#endif

#if defined BB_WGET
#if defined USAGE_ENUM
wget_usage_index
#elif defined USAGE_MESSAGES
 "[-c] [-O file] url"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nwget retrieves files via HTTP\n\n"
	"Options:\n"
	"\t-c\tcontinue retrieval of aborted transfers\n"
	"\t-O\tsave to filename ('-' for stdout)"
#endif
#endif
DO_COMMA
#endif

#if defined BB_WHICH
#if defined USAGE_ENUM
which_usage_index
#elif defined USAGE_MESSAGES
	"[COMMAND ...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nLocates a COMMAND."
#endif
#endif
DO_COMMA
#endif

#if defined BB_WHOAMI
#if defined USAGE_ENUM
whoami_usage_index
#elif defined USAGE_MESSAGES
	""
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nPrints the user name associated with the current effective user id."
#endif
#endif
DO_COMMA
#endif

#if defined BB_XARGS
#if defined USAGE_ENUM
xargs_usage_index
#elif defined USAGE_MESSAGES
 "[COMMAND] [ARGS...]"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nExecutes COMMAND on every item given by standard input." 
#endif
#endif
DO_COMMA
#endif

#if defined BB_YES
#if defined USAGE_ENUM
yes_usage_index
#elif defined USAGE_MESSAGES
	"[OPTION]... [STRING]..."
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\n\nRepeatedly outputs a line with all specified STRING(s), or `y'."
#endif
#endif
DO_COMMA
#endif

#if defined USAGE_ENUM || defined USAGE_MESSAGES
};
#endif

#undef DOCOMMA
#undef USAGE_ENUM
#undef USAGE_MESSAGES

