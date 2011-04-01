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
#ifndef BB_USAGE_H
#define BB_USAGE_H 1


#define NOUSAGE_STR "\b"

INSERT

#define acpid_trivial_usage \
       "[-d] [-c CONFDIR] [-l LOGFILE] [-a ACTIONFILE] [-M MAPFILE] [-e PROC_EVENT_FILE] [-p PIDFILE]"
#define acpid_full_usage "\n\n" \
       "Listen to ACPI events and spawn specific helpers on event arrival\n" \
     "\nOptions:" \
     "\n	-c DIR	Config directory [/etc/acpi]" \
     "\n	-d	Don't daemonize, (implies -f)" \
     "\n	-e FILE	/proc event file [/proc/acpi/event]" \
     "\n	-f	Run in foreground" \
     "\n	-l FILE	Log file [/var/log/acpid.log]" \
     "\n	-p FILE	Pid file [/var/run/acpid.pid]" \
     "\n	-a FILE	Action file [/etc/acpid.conf]" \
     "\n	-M FILE Map file [/etc/acpi.map]" \
	IF_FEATURE_ACPID_COMPAT( \
     "\n\nAccept and ignore compatibility options -g -m -s -S -v" \
	)

#define acpid_example_usage \
       "Without -e option, acpid uses all /dev/input/event* files\n" \
       "# acpid\n" \
       "# acpid -l /var/log/my-acpi-log\n" \
       "# acpid -e /proc/acpi/event\n"

#define adjtimex_trivial_usage \
       "[-q] [-o OFF] [-f FREQ] [-p TCONST] [-t TICK]"
#define adjtimex_full_usage "\n\n" \
       "Read and optionally set system timebase parameters. See adjtimex(2)\n" \
     "\nOptions:" \
     "\n	-q	Quiet" \
     "\n	-o OFF	Time offset, microseconds" \
     "\n	-f FREQ	Frequency adjust, integer kernel units (65536 is 1ppm)" \
     "\n		(positive values make clock run faster)" \
     "\n	-t TICK	Microseconds per tick, usually 10000" \
     "\n	-p TCONST" \

#define arp_trivial_usage \
     "\n[-vn]	[-H HWTYPE] [-i IF] -a [HOSTNAME]" \
     "\n[-v]		    [-i IF] -d HOSTNAME [pub]" \
     "\n[-v]	[-H HWTYPE] [-i IF] -s HOSTNAME HWADDR [temp]" \
     "\n[-v]	[-H HWTYPE] [-i IF] -s HOSTNAME HWADDR [netmask MASK] pub" \
     "\n[-v]	[-H HWTYPE] [-i IF] -Ds HOSTNAME IFACE [netmask MASK] pub"
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
       "\n	-A,-p AF	Protocol family" \
       "\n	-H HWTYPE	Hardware address type" \

#define arping_trivial_usage \
       "[-fqbDUA] [-c CNT] [-w TIMEOUT] [-I IFACE] [-s SRC_IP] DST_IP"
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
     "\n	-w TIMEOUT	Time to wait for ARP reply, seconds" \
     "\n	-I IFACE	Interface to use (default eth0)" \
     "\n	-s SRC_IP	Sender IP address" \
     "\n	DST_IP		Target IP address" \

#define beep_trivial_usage \
       "-f FREQ -l LEN -d DELAY -r COUNT -n"
#define beep_full_usage "\n\n" \
       "Options:" \
     "\n	-f	Frequency in Hz" \
     "\n	-l	Length in ms" \
     "\n	-d	Delay in ms" \
     "\n	-r	Repetitions" \
     "\n	-n	Start new tone" \

#define blkid_trivial_usage \
       ""
#define blkid_full_usage "\n\n" \
       "Print UUIDs of all filesystems"

#define brctl_trivial_usage \
       "COMMAND [BRIDGE [INTERFACE]]"
#define brctl_full_usage "\n\n" \
       "Manage ethernet bridges\n" \
     "\nCommands:" \
	IF_FEATURE_BRCTL_SHOW( \
     "\n	show			Show a list of bridges" \
	) \
     "\n	addbr BRIDGE		Create BRIDGE" \
     "\n	delbr BRIDGE		Delete BRIDGE" \
     "\n	addif BRIDGE IFACE	Add IFACE to BRIDGE" \
     "\n	delif BRIDGE IFACE	Delete IFACE from BRIDGE" \
	IF_FEATURE_BRCTL_FANCY( \
     "\n	setageing BRIDGE TIME		Set ageing time" \
     "\n	setfd BRIDGE TIME		Set bridge forward delay" \
     "\n	sethello BRIDGE TIME		Set hello time" \
     "\n	setmaxage BRIDGE TIME		Set max message age" \
     "\n	setpathcost BRIDGE COST		Set path cost" \
     "\n	setportprio BRIDGE PRIO		Set port priority" \
     "\n	setbridgeprio BRIDGE PRIO	Set bridge priority" \
     "\n	stp BRIDGE [1/yes/on|0/no/off]	STP on/off" \
	) \

#define busybox_notes_usage \
       "Hello world!\n"

#define chat_trivial_usage \
       "EXPECT [SEND [EXPECT [SEND...]]]"
#define chat_full_usage "\n\n" \
       "Useful for interacting with a modem connected to stdin/stdout.\n" \
       "A script consists of one or more \"expect-send\" pairs of strings,\n" \
       "each pair is a pair of arguments. Example:\n" \
       "chat '' ATZ OK ATD123456 CONNECT '' ogin: pppuser word: ppppass '~'" \

#define chcon_trivial_usage \
       "[OPTIONS] CONTEXT FILE..." \
       "\n	chcon [OPTIONS] [-u USER] [-r ROLE] [-l RANGE] [-t TYPE] FILE..." \
	IF_FEATURE_CHCON_LONG_OPTIONS( \
       "\n	chcon [OPTIONS] --reference=RFILE FILE..." \
	)
#define chcon_full_usage "\n\n" \
       "Change the security context of each FILE to CONTEXT\n" \
	IF_FEATURE_CHCON_LONG_OPTIONS( \
     "\n	-v,--verbose		Verbose" \
     "\n	-c,--changes		Report changes made" \
     "\n	-h,--no-dereference	Affect symlinks instead of their targets" \
     "\n	-f,--silent,--quiet	Suppress most error messages" \
     "\n	--reference=RFILE	Use RFILE's group instead of using a CONTEXT value" \
     "\n	-u,--user=USER		Set user/role/type/range in the target" \
     "\n	-r,--role=ROLE		security context" \
     "\n	-t,--type=TYPE" \
     "\n	-l,--range=RANGE" \
     "\n	-R,--recursive		Recurse" \
	) \
	IF_NOT_FEATURE_CHCON_LONG_OPTIONS( \
     "\n	-v	Verbose" \
     "\n	-c	Report changes made" \
     "\n	-h	Affect symlinks instead of their targets" \
     "\n	-f	Suppress most error messages" \
     "\n	-u USER	Set user/role/type/range in the target security context" \
     "\n	-r ROLE" \
     "\n	-t TYPE" \
     "\n	-l RNG" \
     "\n	-R	Recurse" \
	)

#define chpst_trivial_usage \
       "[-vP012] [-u USER[:GRP]] [-U USER[:GRP]] [-e DIR]\n" \
       "	[-/ DIR] [-n NICE] [-m BYTES] [-d BYTES] [-o N]\n" \
       "	[-p N] [-f BYTES] [-c BYTES] PROG ARGS"
#define chpst_full_usage "\n\n" \
       "Change the process state, run PROG\n" \
     "\nOptions:" \
     "\n	-u USER[:GRP]	Set uid and gid" \
     "\n	-U USER[:GRP]	Set $UID and $GID in environment" \
     "\n	-e DIR		Set environment variables as specified by files" \
     "\n			in DIR: file=1st_line_of_file" \
     "\n	-/ DIR		Chroot to DIR" \
     "\n	-n NICE		Add NICE to nice value" \
     "\n	-m BYTES	Same as -d BYTES -s BYTES -l BYTES" \
     "\n	-d BYTES	Limit data segment" \
     "\n	-o N		Limit number of open files per process" \
     "\n	-p N		Limit number of processes per uid" \
     "\n	-f BYTES	Limit output file sizes" \
     "\n	-c BYTES	Limit core file size" \
     "\n	-v		Verbose" \
     "\n	-P		Create new process group" \
     "\n	-0		Close stdin" \
     "\n	-1		Close stdout" \
     "\n	-2		Close stderr" \

#define setuidgid_trivial_usage \
       "USER PROG ARGS"
#define setuidgid_full_usage "\n\n" \
       "Set uid and gid to USER's uid and gid, drop supplementary group ids,\n" \
       "run PROG"
#define envuidgid_trivial_usage \
       "USER PROG ARGS"
#define envuidgid_full_usage "\n\n" \
       "Set $UID to USER's uid and $GID to USER's gid, run PROG"
#define envdir_trivial_usage \
       "DIR PROG ARGS"
#define envdir_full_usage "\n\n" \
       "Set various environment variables as specified by files\n" \
       "in the directory DIR, run PROG"
#define softlimit_trivial_usage \
       "[-a BYTES] [-m BYTES] [-d BYTES] [-s BYTES] [-l BYTES]\n" \
       "	[-f BYTES] [-c BYTES] [-r BYTES] [-o N] [-p N] [-t N]\n" \
       "	PROG ARGS"
#define softlimit_full_usage "\n\n" \
       "Set soft resource limits, then run PROG\n" \
     "\nOptions:" \
     "\n	-a BYTES	Limit total size of all segments" \
     "\n	-m BYTES	Same as -d BYTES -s BYTES -l BYTES -a BYTES" \
     "\n	-d BYTES	Limit data segment" \
     "\n	-s BYTES	Limit stack segment" \
     "\n	-l BYTES	Limit locked memory size" \
     "\n	-o N		Limit number of open files per process" \
     "\n	-p N		Limit number of processes per uid" \
     "\nOptions controlling file sizes:" \
     "\n	-f BYTES	Limit output file sizes" \
     "\n	-c BYTES	Limit core file size" \
     "\nEfficiency opts:" \
     "\n	-r BYTES	Limit resident set size" \
     "\n	-t N		Limit CPU time, process receives" \
     "\n			a SIGXCPU after N seconds" \

#define bbconfig_trivial_usage \
       ""
#define bbconfig_full_usage "\n\n" \
       "Print the config file used by busybox build"

#define chrt_trivial_usage \
       "[-prfom] [PRIO] [PID | PROG ARGS]"
#define chrt_full_usage "\n\n" \
       "Change scheduling priority and class for a process\n" \
     "\nOptions:" \
     "\n	-p	Operate on PID" \
     "\n	-r	Set SCHED_RR class" \
     "\n	-f	Set SCHED_FIFO class" \
     "\n	-o	Set SCHED_OTHER class" \
     "\n	-m	Show min/max priorities" \

#define chrt_example_usage \
       "$ chrt -r 4 sleep 900; x=$!\n" \
       "$ chrt -f -p 3 $x\n" \
       "You need CAP_SYS_NICE privileges to set scheduling attributes of a process"

#define renice_trivial_usage \
       "{{-n INCREMENT} | PRIORITY} [[-p | -g | -u] ID...]"
#define renice_full_usage "\n\n" \
       "Change scheduling priority for a running process\n" \
     "\nOptions:" \
     "\n	-n	Adjust current nice value (smaller is faster)" \
     "\n	-p	Process id(s) (default)" \
     "\n	-g	Process group id(s)" \
     "\n	-u	Process user name(s) and/or id(s)" \

#define ionice_trivial_usage \
	"[-c 1-3] [-n 0-7] [-p PID] [PROG]"
#define ionice_full_usage "\n\n" \
       "Change I/O priority and class\n" \
     "\nOptions:" \
     "\n	-c	Class. 1:realtime 2:best-effort 3:idle" \
     "\n	-n	Priority" \

#define crond_trivial_usage \
       "-fbS -l N " IF_FEATURE_CROND_D("-d N ") "-L LOGFILE -c DIR"
#define crond_full_usage "\n\n" \
       "	-f	Foreground" \
     "\n	-b	Background (default)" \
     "\n	-S	Log to syslog (default)" \
     "\n	-l	Set log level. 0 is the most verbose, default 8" \
	IF_FEATURE_CROND_D( \
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

#define devmem_trivial_usage \
	"ADDRESS [WIDTH [VALUE]]"

#define devmem_full_usage "\n\n" \
       "Read/write from physical address\n" \
     "\n	ADDRESS	Address to act upon" \
     "\n	WIDTH	Width (8/16/...)" \
     "\n	VALUE	Data to be written" \

#define devfsd_trivial_usage \
       "mntpnt [-v]" IF_DEVFSD_FG_NP("[-fg][-np]")
#define devfsd_full_usage "\n\n" \
       "Manage devfs permissions and old device name symlinks\n" \
     "\nOptions:" \
     "\n	mntpnt	The mount point where devfs is mounted" \
     "\n	-v	Print the protocol version numbers for devfsd" \
     "\n		and the kernel-side protocol version and exit" \
	IF_DEVFSD_FG_NP( \
     "\n	-fg	Run in foreground" \
     "\n	-np	Exit after parsing the configuration file" \
     "\n		and processing synthetic REGISTER events," \
     "\n		don't poll for events" \
	)

#define dhcprelay_trivial_usage \
       "CLIENT_IFACE[,CLIENT_IFACE2]... SERVER_IFACE [SERVER_IP]"
#define dhcprelay_full_usage "\n\n" \
       "Relay DHCP requests between clients and server" \

#define dmesg_trivial_usage \
       "[-c] [-n LEVEL] [-s SIZE]"
#define dmesg_full_usage "\n\n" \
       "Print or control the kernel ring buffer\n" \
     "\nOptions:" \
     "\n	-c		Clear ring buffer after printing" \
     "\n	-n LEVEL	Set console logging level" \
     "\n	-s SIZE		Buffer size" \

#define dnsd_trivial_usage \
       "[-dvs] [-c CONFFILE] [-t TTL_SEC] [-p PORT] [-i ADDR]"
#define dnsd_full_usage "\n\n" \
       "Small static DNS server daemon\n" \
     "\nOptions:" \
     "\n	-c FILE	Config file" \
     "\n	-t SEC	TTL" \
     "\n	-p PORT	Listen on PORT" \
     "\n	-i ADDR	Listen on ADDR" \
     "\n	-d	Daemonize" \
     "\n	-v	Verbose" \
     "\n	-s	Send successful replies only. Use this if you want" \
     "\n		to use /etc/resolv.conf with two nameserver lines:" \
     "\n			nameserver DNSD_SERVER" \
     "\n			nameserver NORNAL_DNS_SERVER" \

#define dumpleases_trivial_usage \
       "[-r|-a] [-f LEASEFILE]"
#define dumpleases_full_usage "\n\n" \
       "Display DHCP leases granted by udhcpd\n" \
     "\nOptions:" \
	IF_LONG_OPTS( \
     "\n	-f,--file=FILE	Lease file" \
     "\n	-r,--remaining	Show remaining time" \
     "\n	-a,--absolute	Show expiration time" \
	) \
	IF_NOT_LONG_OPTS( \
     "\n	-f FILE	Lease file" \
     "\n	-r	Show remaining time" \
     "\n	-a	Show expiration time" \
	)

/*
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
*/

#define eject_trivial_usage \
       "[-t] [-T] [DEVICE]"
#define eject_full_usage "\n\n" \
       "Eject DEVICE or default /dev/cdrom\n" \
     "\nOptions:" \
	IF_FEATURE_EJECT_SCSI( \
     "\n	-s	SCSI device" \
	) \
     "\n	-t	Close tray" \
     "\n	-T	Open/close tray (toggle)" \

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

#define fakeidentd_trivial_usage \
       "[-fiw] [-b ADDR] [STRING]"
#define fakeidentd_full_usage "\n\n" \
       "Provide fake ident (auth) service\n" \
     "\nOptions:" \
     "\n	-f	Run in foreground" \
     "\n	-i	Inetd mode" \
     "\n	-w	Inetd 'wait' mode" \
     "\n	-b ADDR	Bind to specified address" \
     "\n	STRING	Ident answer string (default: nobody)" \

#define fbsplash_trivial_usage \
       "-s IMGFILE [-c] [-d DEV] [-i INIFILE] [-f CMD]"
#define fbsplash_full_usage "\n\n" \
       "Options:" \
     "\n	-s	Image" \
     "\n	-c	Hide cursor" \
     "\n	-d	Framebuffer device (default /dev/fb0)" \
     "\n	-i	Config file (var=value):" \
     "\n			BAR_LEFT,BAR_TOP,BAR_WIDTH,BAR_HEIGHT" \
     "\n			BAR_R,BAR_G,BAR_B" \
     "\n	-f	Control pipe (else exit after drawing image)" \
     "\n			commands: 'NN' (% for progress bar) or 'exit'" \

#define fbset_trivial_usage \
       "[OPTIONS] [MODE]"
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
# define IF_FEATURE_FDISK_BLKSIZE(a)
#endif

#define fdisk_trivial_usage \
       "[-ul" IF_FEATURE_FDISK_BLKSIZE("s") "] " \
       "[-C CYLINDERS] [-H HEADS] [-S SECTORS] [-b SSZ] DISK"
#define fdisk_full_usage "\n\n" \
       "Change partition table\n" \
     "\nOptions:" \
     "\n	-u		Start and End are in sectors (instead of cylinders)" \
     "\n	-l		Show partition table for each DISK, then exit" \
	IF_FEATURE_FDISK_BLKSIZE( \
     "\n	-s		Show partition sizes in kb for each DISK, then exit" \
	) \
     "\n	-b 2048		(for certain MO disks) use 2048-byte sectors" \
     "\n	-C CYLINDERS	Set number of cylinders/heads/sectors" \
     "\n	-H HEADS" \
     "\n	-S SECTORS" \

#define findfs_trivial_usage \
       "LABEL=label or UUID=uuid"
#define findfs_full_usage "\n\n" \
       "Find a filesystem device based on a label or UUID"
#define findfs_example_usage \
       "$ findfs LABEL=MyDevice"

#define flash_lock_trivial_usage \
       "MTD_DEVICE OFFSET SECTORS"
#define flash_lock_full_usage "\n\n" \
       "Lock part or all of an MTD device. If SECTORS is -1, then all sectors\n" \
       "will be locked, regardless of the value of OFFSET"

#define flash_unlock_trivial_usage \
       "MTD_DEVICE"
#define flash_unlock_full_usage "\n\n" \
       "Unlock an MTD device"

#define flash_eraseall_trivial_usage \
       "[-jq] MTD_DEVICE"
#define flash_eraseall_full_usage "\n\n" \
       "Erase an MTD device\n" \
     "\nOptions:" \
     "\n	-j	Format the device for jffs2" \
     "\n	-q	Don't display progress messages" \

#define flashcp_trivial_usage \
       "-v FILE MTD_DEVICE"
#define flashcp_full_usage "\n\n" \
       "Copy an image to MTD device\n" \
     "\nOptions:" \
     "\n	-v	Verbose" \

#define flock_trivial_usage \
       "[-sxun] FD|{FILE [-c] PROG ARGS}"
#define flock_full_usage "\n\n" \
       "[Un]lock file descriptor, or lock FILE, run PROG\n" \
     "\nOptions:" \
     "\n	-s	Shared lock" \
     "\n	-x	Exclusive lock (default)" \
     "\n	-u	Unlock FD" \
     "\n	-n	Fail rather than wait" \

#define free_trivial_usage \
       "" IF_DESKTOP("[-b/k/m/g]")
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

#define fsck_minix_trivial_usage \
       "[-larvsmf] BLOCKDEV"
#define fsck_minix_full_usage "\n\n" \
       "Check MINIX filesystem\n" \
     "\nOptions:" \
     "\n	-l	List all filenames" \
     "\n	-r	Perform interactive repairs" \
     "\n	-a	Perform automatic repairs" \
     "\n	-v	Verbose" \
     "\n	-s	Output superblock information" \
     "\n	-m	Show \"mode not cleared\" warnings" \
     "\n	-f	Force file system check" \

#define ftpget_trivial_usage \
       "[OPTIONS] HOST [LOCAL_FILE] REMOTE_FILE"
#define ftpget_full_usage "\n\n" \
       "Retrieve a remote file via FTP\n" \
     "\nOptions:" \
	IF_FEATURE_FTPGETPUT_LONG_OPTIONS( \
     "\n	-c,--continue	Continue previous transfer" \
     "\n	-v,--verbose	Verbose" \
     "\n	-u,--username	Username" \
     "\n	-p,--password	Password" \
     "\n	-P,--port	Port number" \
	) \
	IF_NOT_FEATURE_FTPGETPUT_LONG_OPTIONS( \
     "\n	-c	Continue previous transfer" \
     "\n	-v	Verbose" \
     "\n	-u	Username" \
     "\n	-p	Password" \
     "\n	-P	Port number" \
	)

#define ftpput_trivial_usage \
       "[OPTIONS] HOST [REMOTE_FILE] LOCAL_FILE"
#define ftpput_full_usage "\n\n" \
       "Store a local file on a remote machine via FTP\n" \
     "\nOptions:" \
	IF_FEATURE_FTPGETPUT_LONG_OPTIONS( \
     "\n	-v,--verbose	Verbose" \
     "\n	-u,--username	Username" \
     "\n	-p,--password	Password" \
     "\n	-P,--port	Port number" \
	) \
	IF_NOT_FEATURE_FTPGETPUT_LONG_OPTIONS( \
     "\n	-v	Verbose" \
     "\n	-u	Username" \
     "\n	-p	Password" \
     "\n	-P	Port number" \
	)

#define fuser_trivial_usage \
       "[OPTIONS] FILE or PORT/PROTO"
#define fuser_full_usage "\n\n" \
       "Find processes which use FILEs or PORTs\n" \
     "\nOptions:" \
     "\n	-m	Find processes which use same fs as FILEs" \
     "\n	-4,-6	Search only IPv4/IPv6 space" \
     "\n	-s	Don't display PIDs" \
     "\n	-k	Kill found processes" \
     "\n	-SIGNAL	Signal to send (default: KILL)" \

#define getenforce_trivial_usage NOUSAGE_STR
#define getenforce_full_usage ""

#define getopt_trivial_usage \
       "[OPTIONS]"
#define getopt_full_usage "\n\n" \
       "Options:" \
	IF_LONG_OPTS( \
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
	IF_NOT_LONG_OPTS( \
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
       "if [ $? != 0 ]; then exit 1; fi\n" \
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
       "	-a	Show all selinux booleans"

#define hdparm_trivial_usage \
       "[OPTIONS] [DEVICE]"
#define hdparm_full_usage "\n\n" \
       "Options:" \
     "\n	-a	Get/set fs readahead" \
     "\n	-A	Set drive read-lookahead flag (0/1)" \
     "\n	-b	Get/set bus state (0 == off, 1 == on, 2 == tristate)" \
     "\n	-B	Set Advanced Power Management setting (1-255)" \
     "\n	-c	Get/set IDE 32-bit IO setting" \
     "\n	-C	Check IDE power mode status" \
	IF_FEATURE_HDPARM_HDIO_GETSET_DMA( \
     "\n	-d	Get/set using_dma flag") \
     "\n	-D	Enable/disable drive defect-mgmt" \
     "\n	-f	Flush buffer cache for device on exit" \
     "\n	-g	Display drive geometry" \
     "\n	-h	Display terse usage information" \
	IF_FEATURE_HDPARM_GET_IDENTITY( \
     "\n	-i	Display drive identification") \
	IF_FEATURE_HDPARM_GET_IDENTITY( \
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
	IF_FEATURE_HDPARM_HDIO_SCAN_HWIF( \
     "\n	-R	Register an IDE interface (DANGEROUS)") \
     "\n	-S	Set standby (spindown) timeout" \
     "\n	-t	Perform device read timings" \
     "\n	-T	Perform cache read timings" \
     "\n	-u	Get/set unmaskirq flag (0/1)" \
	IF_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF( \
     "\n	-U	Unregister an IDE interface (DANGEROUS)") \
     "\n	-v	Defaults; same as -mcudkrag for IDE drives" \
     "\n	-V	Display program version and exit immediately" \
	IF_FEATURE_HDPARM_HDIO_DRIVE_RESET( \
     "\n	-w	Perform device reset (DANGEROUS)") \
     "\n	-W	Set drive write-caching flag (0/1) (DANGEROUS)" \
	IF_FEATURE_HDPARM_HDIO_TRISTATE_HWIF( \
     "\n	-x	Tristate device for hotswap (0/1) (DANGEROUS)") \
     "\n	-X	Set IDE xfer mode (DANGEROUS)" \
     "\n	-y	Put IDE drive in standby mode" \
     "\n	-Y	Put IDE drive to sleep" \
     "\n	-Z	Disable Seagate auto-powersaving mode" \
     "\n	-z	Reread partition table" \

#define hexdump_trivial_usage \
       "[-bcCdefnosvx" IF_FEATURE_HEXDUMP_REVERSE("R") "] [FILE]..."
#define hexdump_full_usage "\n\n" \
       "Display FILEs (or stdin) in a user specified format\n" \
     "\nOptions:" \
     "\n	-b		One-byte octal display" \
     "\n	-c		One-byte character display" \
     "\n	-C		Canonical hex+ASCII, 16 bytes per line" \
     "\n	-d		Two-byte decimal display" \
     "\n	-e FORMAT_STRING" \
     "\n	-f FORMAT_FILE" \
     "\n	-n LENGTH	Interpret only LENGTH bytes of input" \
     "\n	-o		Two-byte octal display" \
     "\n	-s OFFSET	Skip OFFSET bytes" \
     "\n	-v		Display all input data" \
     "\n	-x		Two-byte hexadecimal display" \
	IF_FEATURE_HEXDUMP_REVERSE( \
     "\n	-R		Reverse of 'hexdump -Cv'") \

#define hd_trivial_usage \
       "FILE..."
#define hd_full_usage "\n\n" \
       "hd is an alias for hexdump -C"

#define hostname_trivial_usage \
       "[OPTIONS] [HOSTNAME | -F FILE]"
#define hostname_full_usage "\n\n" \
       "Get or set hostname or DNS domain name\n" \
     "\nOptions:" \
     "\n	-s	Short" \
     "\n	-i	Addresses for the hostname" \
     "\n	-d	DNS domain name" \
     "\n	-f	Fully qualified domain name" \
     "\n	-F FILE	Use FILE's content as hostname" \

#define hostname_example_usage \
       "$ hostname\n" \
       "sage\n"

#define dnsdomainname_trivial_usage NOUSAGE_STR
#define dnsdomainname_full_usage ""

#define httpd_trivial_usage \
       "[-ifv[v]]" \
       " [-c CONFFILE]" \
       " [-p [IP:]PORT]" \
	IF_FEATURE_HTTPD_SETUID(" [-u USER[:GRP]]") \
	IF_FEATURE_HTTPD_BASIC_AUTH(" [-r REALM]") \
       " [-h HOME]\n" \
       "or httpd -d/-e" IF_FEATURE_HTTPD_AUTH_MD5("/-m") " STRING"
#define httpd_full_usage "\n\n" \
       "Listen for incoming HTTP requests\n" \
     "\nOptions:" \
     "\n	-i		Inetd mode" \
     "\n	-f		Don't daemonize" \
     "\n	-v[v]		Verbose" \
     "\n	-p [IP:]PORT	Bind to IP:PORT (default *:80)" \
	IF_FEATURE_HTTPD_SETUID( \
     "\n	-u USER[:GRP]	Set uid/gid after binding to port") \
	IF_FEATURE_HTTPD_BASIC_AUTH( \
     "\n	-r REALM	Authentication Realm for Basic Authentication") \
     "\n	-h HOME		Home directory (default .)" \
     "\n	-c FILE		Configuration file (default {/etc,HOME}/httpd.conf)" \
	IF_FEATURE_HTTPD_AUTH_MD5( \
     "\n	-m STRING	MD5 crypt STRING") \
     "\n	-e STRING	HTML encode STRING" \
     "\n	-d STRING	URL decode STRING" \

#define ifconfig_trivial_usage \
	IF_FEATURE_IFCONFIG_STATUS("[-a]") " interface [address]"
#define ifconfig_full_usage "\n\n" \
       "Configure a network interface\n" \
     "\nOptions:" \
     "\n" \
	IF_FEATURE_IPV6( \
       "	[add ADDRESS[/PREFIXLEN]]\n") \
	IF_FEATURE_IPV6( \
       "	[del ADDRESS[/PREFIXLEN]]\n") \
       "	[[-]broadcast [ADDRESS]] [[-]pointopoint [ADDRESS]]\n" \
       "	[netmask ADDRESS] [dstaddr ADDRESS]\n" \
	IF_FEATURE_IFCONFIG_SLIP( \
       "	[outfill NN] [keepalive NN]\n") \
       "	" IF_FEATURE_IFCONFIG_HW("[hw ether" IF_FEATURE_HWIB("|infiniband")" ADDRESS] ") "[metric NN] [mtu NN]\n" \
       "	[[-]trailers] [[-]arp] [[-]allmulti]\n" \
       "	[multicast] [[-]promisc] [txqueuelen NN] [[-]dynamic]\n" \
	IF_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ( \
       "	[mem_start NN] [io_addr NN] [irq NN]\n") \
       "	[up|down] ..."

#define ifenslave_trivial_usage \
       "[-cdf] MASTER_IFACE SLAVE_IFACE..."
#define ifenslave_full_usage "\n\n" \
       "Configure network interfaces for parallel routing\n" \
     "\nOptions:" \
     "\n	-c,--change-active	Change active slave" \
     "\n	-d,--detach		Remove slave interface from bonding device" \
     "\n	-f,--force		Force, even if interface is not Ethernet" \
/*   "\n	-r,--receive-slave	Create a receive-only slave" */

#define ifenslave_example_usage \
       "To create a bond device, simply follow these three steps:\n" \
       "- ensure that the required drivers are properly loaded:\n" \
       "  # modprobe bonding ; modprobe <3c59x|eepro100|pcnet32|tulip|...>\n" \
       "- assign an IP address to the bond device:\n" \
       "  # ifconfig bond0 <addr> netmask <mask> broadcast <bcast>\n" \
       "- attach all the interfaces you need to the bond device:\n" \
       "  # ifenslave bond0 eth0 eth1 eth2\n" \
       "  If bond0 didn't have a MAC address, it will take eth0's. Then, all\n" \
       "  interfaces attached AFTER this assignment will get the same MAC addr.\n\n" \
       "  To detach a dead interface without setting the bond device down:\n" \
       "  # ifenslave -d bond0 eth1\n\n" \
       "  To set the bond device down and automatically release all the slaves:\n" \
       "  # ifconfig bond0 down\n\n" \
       "  To change active slave:\n" \
       "  # ifenslave -c bond0 eth0\n" \

#define ifplugd_trivial_usage \
       "[OPTIONS]"
#define ifplugd_full_usage "\n\n" \
       "Network interface plug detection daemon\n" \
     "\nOptions:" \
     "\n	-n		Don't daemonize" \
     "\n	-s		Don't log to syslog" \
     "\n	-i IFACE	Interface" \
     "\n	-f/-F		Treat link detection error as link down/link up" \
     "\n			(otherwise exit on error)" \
     "\n	-a		Don't up interface at each link probe" \
     "\n	-M		Monitor creation/destruction of interface" \
     "\n			(otherwise it must exist)" \
     "\n	-r PROG		Script to run" \
     "\n	-x ARG		Extra argument for script" \
     "\n	-I		Don't exit on nonzero exit code from script" \
     "\n	-p		Don't run script on daemon startup" \
     "\n	-q		Don't run script on daemon quit" \
     "\n	-l		Run script on startup even if no cable is detected" \
     "\n	-t SECS		Poll time in seconds" \
     "\n	-u SECS		Delay before running script after link up" \
     "\n	-d SECS		Delay after link down" \
     "\n	-m MODE		API mode (mii, priv, ethtool, wlan, iff, auto)" \
     "\n	-k		Kill running daemon" \

#define ifup_trivial_usage \
       "[-an"IF_FEATURE_IFUPDOWN_MAPPING("m")"vf] [-i FILE] IFACE..."
#define ifup_full_usage "\n\n" \
       "Options:" \
     "\n	-a	De/configure all interfaces automatically" \
     "\n	-i FILE	Use FILE for interface definitions" \
     "\n	-n	Print out what would happen, but don't do it" \
	IF_FEATURE_IFUPDOWN_MAPPING( \
     "\n		(note: doesn't disable mappings)" \
     "\n	-m	Don't run any mappings" \
	) \
     "\n	-v	Print out what would happen before doing it" \
     "\n	-f	Force de/configuration" \

#define ifdown_trivial_usage \
       "[-an"IF_FEATURE_IFUPDOWN_MAPPING("m")"vf] [-i FILE] IFACE..."
#define ifdown_full_usage "\n\n" \
       "Options:" \
     "\n	-a	De/configure all interfaces automatically" \
     "\n	-i FILE	Use FILE for interface definitions" \
     "\n	-n	Print out what would happen, but don't do it" \
	IF_FEATURE_IFUPDOWN_MAPPING( \
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

#define inotifyd_trivial_usage \
	"PROG FILE1[:MASK]..."
#define inotifyd_full_usage "\n\n" \
       "Run PROG on filesystem changes." \
     "\nWhen a filesystem event matching MASK occurs on FILEn," \
     "\nPROG ACTUAL_EVENTS FILEn [SUBFILE] is run." \
     "\nEvents:" \
     "\n	a	File is accessed" \
     "\n	c	File is modified" \
     "\n	e	Metadata changed" \
     "\n	w	Writable file is closed" \
     "\n	0	Unwritable file is closed" \
     "\n	r	File is opened" \
     "\n	D	File is deleted" \
     "\n	M	File is moved" \
     "\n	u	Backing fs is unmounted" \
     "\n	o	Event queue overflowed" \
     "\n	x	File can't be watched anymore" \
     "\nIf watching a directory:" \
     "\n	m	Subfile is moved into dir" \
     "\n	y	Subfile is moved out of dir" \
     "\n	n	Subfile is created" \
     "\n	d	Subfile is deleted" \
     "\n" \
     "\ninotifyd waits for PROG to exit." \
     "\nWhen x event happens for all FILEs, inotifyd exits." \

/* would need to make the " | " optional depending on more than one selected: */
#define ip_trivial_usage \
       "[OPTIONS] {" \
	IF_FEATURE_IP_ADDRESS("address | ") \
	IF_FEATURE_IP_ROUTE("route | ") \
	IF_FEATURE_IP_LINK("link | ") \
	IF_FEATURE_IP_TUNNEL("tunnel | ") \
	IF_FEATURE_IP_RULE("rule") \
       "} {COMMAND}"
#define ip_full_usage "\n\n" \
       "ip [OPTIONS] OBJECT {COMMAND}\n" \
       "where OBJECT := {" \
	IF_FEATURE_IP_ADDRESS("address | ") \
	IF_FEATURE_IP_ROUTE("route | ") \
	IF_FEATURE_IP_LINK("link | ") \
	IF_FEATURE_IP_TUNNEL("tunnel | ") \
	IF_FEATURE_IP_RULE("rule") \
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
       "[OPTIONS] ADDRESS[[/]NETMASK] [NETMASK]"
#define ipcalc_full_usage "\n\n" \
       "Calculate IP network settings from a IP address\n" \
     "\nOptions:" \
	IF_FEATURE_IPCALC_LONG_OPTIONS( \
     "\n	-b,--broadcast	Display calculated broadcast address" \
     "\n	-n,--network	Display calculated network address" \
     "\n	-m,--netmask	Display default netmask for IP" \
	IF_FEATURE_IPCALC_FANCY( \
     "\n	-p,--prefix	Display the prefix for IP/NETMASK" \
     "\n	-h,--hostname	Display first resolved host name" \
     "\n	-s,--silent	Don't ever display error messages" \
	) \
	) \
	IF_NOT_FEATURE_IPCALC_LONG_OPTIONS( \
     "\n	-b	Display calculated broadcast address" \
     "\n	-n	Display calculated network address" \
     "\n	-m	Display default netmask for IP" \
	IF_FEATURE_IPCALC_FANCY( \
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
       "			ROUTE := [TYPE] PREFIX [tos TOS] [proto RTPROTO]\n" \
       "				[metric METRIC]" \

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

#define kill_trivial_usage \
       "[-l] [-SIG] PID..."
#define kill_full_usage "\n\n" \
       "Send a signal (default: TERM) to given PIDs\n" \
     "\nOptions:" \
     "\n	-l	List all signal names and numbers" \
/*   "\n	-s SIG	Yet another way of specifying SIG" */ \

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
       "[-l] [-q] [-SIG] PROCESS_NAME..."
#define killall_full_usage "\n\n" \
       "Send a signal (default: TERM) to given processes\n" \
     "\nOptions:" \
     "\n	-l	List all signal names and numbers" \
/*   "\n	-s SIG	Yet another way of specifying SIG" */ \
     "\n	-q	Don't complain if no processes were killed" \

#define killall_example_usage \
       "$ killall apache\n"

#define killall5_trivial_usage \
       "[-l] [-SIG] [-o PID]..."
#define killall5_full_usage "\n\n" \
       "Send a signal (default: TERM) to all processes outside current session\n" \
     "\nOptions:" \
     "\n	-l	List all signal names and numbers" \
     "\n	-o PID	Don't signal this PID" \
/*   "\n	-s SIG	Yet another way of specifying SIG" */ \

#define klogd_trivial_usage \
       "[-c N] [-n]"
#define klogd_full_usage "\n\n" \
       "Kernel logger\n" \
     "\nOptions:" \
     "\n	-c N	Only messages with level < N are printed to console" \
     "\n	-n	Run in foreground" \

#define less_trivial_usage \
       "[-EMNmh~I?] [FILE]..."
#define less_full_usage "\n\n" \
       "View FILE (or stdin) one screenful at a time\n" \
     "\nOptions:" \
     "\n	-E	Quit once the end of a file is reached" \
     "\n	-M,-m	Display status line with line numbers" \
     "\n		and percentage through the file" \
     "\n	-N	Prefix line number to each line" \
     "\n	-I	Ignore case in all searches" \
     "\n	-~	Suppress ~s displayed past the end of the file" \

#define linux32_trivial_usage NOUSAGE_STR
#define linux32_full_usage ""
#define linux64_trivial_usage NOUSAGE_STR
#define linux64_full_usage ""

#define setarch_trivial_usage \
       "personality PROG ARGS"
#define setarch_full_usage "\n\n" \
       "Personality may be:\n" \
       "	linux32		Set 32bit uname emulation\n" \
       "	linux64		Set 64bit uname emulation" \

#define load_policy_trivial_usage NOUSAGE_STR
#define load_policy_full_usage ""

#define logger_trivial_usage \
       "[OPTIONS] [MESSAGE]"
#define logger_full_usage "\n\n" \
       "Write MESSAGE (or stdin) to syslog\n" \
     "\nOptions:" \
     "\n	-s	Log to stderr as well as the system log" \
     "\n	-t TAG	Log using the specified tag (defaults to user name)" \
     "\n	-p PRIO	Priority (numeric or facility.level pair)" \

#define logger_example_usage \
       "$ logger \"hello\"\n"

#define logread_trivial_usage \
       "[-f]"
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
       "SPOOLDIR [HELPER [ARGS]]"
#define lpd_full_usage "\n\n" \
       "SPOOLDIR must contain (symlinks to) device nodes or directories" \
     "\nwith names matching print queue names. In the first case, jobs are" \
     "\nsent directly to the device. Otherwise each job is stored in queue" \
     "\ndirectory and HELPER program is called. Name of file to print" \
     "\nis passed in $DATAFILE variable." \
     "\nExample:" \
     "\n	tcpsvd -E 0 515 softlimit -m 999999 lpd /var/spool ./print" \

#define lpq_trivial_usage \
       "[-P queue[@host[:port]]] [-U USERNAME] [-d JOBID]... [-fs]"
#define lpq_full_usage "\n\n" \
       "Options:" \
     "\n	-P	lp service to connect to (else uses $PRINTER)" \
     "\n	-d	Delete jobs" \
     "\n	-f	Force any waiting job to be printed" \
     "\n	-s	Short display" \

#define lpr_trivial_usage \
       "-P queue[@host[:port]] -U USERNAME -J TITLE -Vmh [FILE]..."
/* -C CLASS exists too, not shown.
 * CLASS is supposed to be printed on banner page, if one is requested */
#define lpr_full_usage "\n\n" \
       "Options:" \
     "\n	-P	lp service to connect to (else uses $PRINTER)"\
     "\n	-m	Send mail on completion" \
     "\n	-h	Print banner page too" \
     "\n	-V	Verbose" \

#define lspci_trivial_usage \
       "[-mk]"
#define lspci_full_usage "\n\n" \
       "List all PCI devices" \
     "\n" \
     "\n	-m	Parseable output" \
     "\n	-k	Show driver" \

#define lsusb_trivial_usage NOUSAGE_STR
#define lsusb_full_usage ""

#if ENABLE_FEATURE_MAKEDEVS_LEAF
#define makedevs_trivial_usage \
       "NAME TYPE MAJOR MINOR FIRST LAST [s]"
#define makedevs_full_usage "\n\n" \
       "Create a range of block or character special files" \
     "\n" \
     "\nTYPE is:" \
     "\n	b	Block device" \
     "\n	c	Character device" \
     "\n	f	FIFO, MAJOR and MINOR are ignored" \
     "\n" \
     "\nFIRST..LAST specify numbers appended to NAME." \
     "\nIf 's' is the last argument, the base device is created as well." \
     "\n" \
     "\nExamples:" \
     "\n	makedevs /dev/ttyS c 4 66 2 63   ->  ttyS2-ttyS63" \
     "\n	makedevs /dev/hda b 3 0 0 8 s    ->  hda,hda1-hda8"
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
       "	f	Regular file\n" \
       "	d	Directory\n" \
       "	c	Character device\n" \
       "	b	Block device\n" \
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
       "[-aw] [MANPAGE]..."
#define man_full_usage "\n\n" \
       "Format and display manual page\n" \
     "\nOptions:" \
     "\n	-a      Display all pages" \
     "\n	-w	Show page locations" \

#define matchpathcon_trivial_usage \
       "[-n] [-N] [-f file_contexts_file] [-p prefix] [-V]"
#define matchpathcon_full_usage "\n\n" \
       "	-n	Don't display path" \
     "\n	-N	Don't use translations" \
     "\n	-f	Use alternate file_context file" \
     "\n	-p	Use prefix to speed translations" \
     "\n	-V	Verify file context on disk matches defaults" \

#define mdev_trivial_usage \
       "[-s]"
#define mdev_full_usage "\n\n" \
       "	-s	Scan /sys and populate /dev during system boot\n" \
       "\n" \
       "It can be run by kernel as a hotplug helper. To activate it:\n" \
       " echo /sbin/mdev > /proc/sys/kernel/hotplug\n" \
	IF_FEATURE_MDEV_CONF( \
       "It uses /etc/mdev.conf with lines\n" \
       "[-]DEVNAME UID:GID PERM" \
			IF_FEATURE_MDEV_RENAME(" [>|=PATH]") \
			IF_FEATURE_MDEV_EXEC(" [@|$|*PROG]") \
	) \

#define mdev_notes_usage "" \
	IF_FEATURE_MDEV_CONFIG( \
       "The mdev config file contains lines that look like:\n" \
       "  hd[a-z][0-9]* 0:3 660\n\n" \
       "That's device name (with regex match), uid:gid, and permissions.\n\n" \
	IF_FEATURE_MDEV_EXEC( \
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

#define mkfs_ext2_trivial_usage \
       "[-Fn] " \
       /* "[-c|-l filename] " */ \
       "[-b BLK_SIZE] " \
       /* "[-f fragment-size] [-g blocks-per-group] " */ \
       "[-i INODE_RATIO] [-I INODE_SIZE] " \
       /* "[-j] [-J journal-options] [-N number-of-inodes] " */ \
       "[-m RESERVED_PERCENT] " \
       /* "[-o creator-os] [-O feature[,...]] [-q] " */ \
       /* "[r fs-revision-level] [-E extended-options] [-v] [-F] " */ \
       "[-L LABEL] " \
       /* "[-M last-mounted-directory] [-S] [-T filesystem-type] " */ \
       "BLOCKDEV [KBYTES]"
#define mkfs_ext2_full_usage "\n\n" \
       "	-b BLK_SIZE	Block size, bytes" \
/*   "\n	-c		Check device for bad blocks" */ \
/*   "\n	-E opts		Set extended options" */ \
/*   "\n	-f size		Fragment size in bytes" */ \
     "\n	-F		Force" \
/*   "\n	-g N		Number of blocks in a block group" */ \
     "\n	-i RATIO	Max number of files is filesystem_size / RATIO" \
     "\n	-I BYTES	Inode size (min 128)" \
/*   "\n	-j		Create a journal (ext3)" */ \
/*   "\n	-J opts		Set journal options (size/device)" */ \
/*   "\n	-l file		Read bad blocks list from file" */ \
     "\n	-L LBL		Volume label" \
     "\n	-m PERCENT	Percent of blocks to reserve for admin" \
/*   "\n	-M dir		Set last mounted directory" */ \
     "\n	-n		Dry run" \
/*   "\n	-N N		Number of inodes to create" */ \
/*   "\n	-o os		Set the 'creator os' field" */ \
/*   "\n	-O features	Dir_index/filetype/has_journal/journal_dev/sparse_super" */ \
/*   "\n	-q		Quiet" */ \
/*   "\n	-r rev		Set filesystem revision" */ \
/*   "\n	-S		Write superblock and group descriptors only" */ \
/*   "\n	-T fs-type	Set usage type (news/largefile/largefile4)" */ \
/*   "\n	-v		Verbose" */ \

#define mkfs_minix_trivial_usage \
       "[-c | -l FILE] [-nXX] [-iXX] BLOCKDEV [KBYTES]"
#define mkfs_minix_full_usage "\n\n" \
       "Make a MINIX filesystem\n" \
     "\nOptions:" \
     "\n	-c		Check device for bad blocks" \
     "\n	-n [14|30]	Maximum length of filenames" \
     "\n	-i INODES	Number of inodes for the filesystem" \
     "\n	-l FILE		Read bad blocks list from FILE" \
     "\n	-v		Make version 2 filesystem" \

#define mkfs_reiser_trivial_usage \
       "[-f] [-l LABEL] BLOCKDEV [4K-BLOCKS]"

#define mkfs_reiser_full_usage "\n\n" \
       "Make a ReiserFS V3 filesystem\n" \
     "\nOptions:" \
     "\n	-f	Force" \
     "\n	-l LBL	Volume label" \

#define mkfs_vfat_trivial_usage \
       "[-v] [-n LABEL] BLOCKDEV [KBYTES]"
/* Accepted but ignored:
       "[-c] [-C] [-I] [-l bad-block-file] [-b backup-boot-sector] "
       "[-m boot-msg-file] [-i volume-id] "
       "[-s sectors-per-cluster] [-S logical-sector-size] [-f number-of-FATs] "
       "[-h hidden-sectors] [-F fat-size] [-r root-dir-entries] [-R reserved-sectors] "
*/
#define mkfs_vfat_full_usage "\n\n" \
       "Make a FAT32 filesystem\n" \
     "\nOptions:" \
/*   "\n	-c	Check device for bad blocks" */ \
     "\n	-v	Verbose" \
/*   "\n	-I	Allow to use entire disk device (e.g. /dev/hda)" */ \
     "\n	-n LBL	Volume label" \

#define mkswap_trivial_usage \
       "[-L LBL] BLOCKDEV [KBYTES]"
#define mkswap_full_usage "\n\n" \
       "Prepare BLOCKDEV to be used as swap partition\n" \
     "\nOptions:" \
     "\n	-L LBL	Label" \

#define more_trivial_usage \
       "[FILE]..."
#define more_full_usage "\n\n" \
       "View FILE (or stdin) one screenful at a time"

#define more_example_usage \
       "$ dmesg | more\n"

#define mount_trivial_usage \
       "[OPTIONS] [-o OPTS] DEVICE NODE"
#define mount_full_usage "\n\n" \
       "Mount a filesystem. Filesystem autodetection requires /proc.\n" \
     "\nOptions:" \
     "\n	-a		Mount all filesystems in fstab" \
	IF_FEATURE_MOUNT_FAKE( \
	IF_FEATURE_MTAB_SUPPORT( \
     "\n	-f		Update /etc/mtab, but don't mount" \
	) \
	IF_NOT_FEATURE_MTAB_SUPPORT( \
     "\n	-f		Dry run" \
	) \
	) \
	IF_FEATURE_MOUNT_HELPERS( \
     "\n	-i		Don't run mount helper" \
	) \
	IF_FEATURE_MTAB_SUPPORT( \
     "\n	-n		Don't update /etc/mtab" \
	) \
     "\n	-r		Read-only mount" \
     "\n	-w		Read-write mount (default)" \
     "\n	-t FSTYPE	Filesystem type" \
     "\n	-O OPT		Mount only filesystems with option OPT (-a only)" \
     "\n-o OPT:" \
	IF_FEATURE_MOUNT_LOOP( \
     "\n	loop		Ignored (loop devices are autodetected)" \
	) \
	IF_FEATURE_MOUNT_FLAGS( \
     "\n	[a]sync		Writes are [a]synchronous" \
     "\n	[no]atime	Disable/enable updates to inode access times" \
     "\n	[no]diratime	Disable/enable atime updates to directories" \
     "\n	[no]relatime	Disable/enable atime updates relative to modification time" \
     "\n	[no]dev		(Dis)allow use of special device files" \
     "\n	[no]exec	(Dis)allow use of executable files" \
     "\n	[no]suid	(Dis)allow set-user-id-root programs" \
     "\n	[r]shared	Convert [recursively] to a shared subtree" \
     "\n	[r]slave	Convert [recursively] to a slave subtree" \
     "\n	[r]private	Convert [recursively] to a private subtree" \
     "\n	[un]bindable	Make mount point [un]able to be bind mounted" \
     "\n	[r]bind		Bind a file or directory [recursively] to another location" \
     "\n	move		Relocate an existing mount point" \
	) \
     "\n	remount		Remount a mounted filesystem, changing flags" \
     "\n	ro/rw		Same as -r/-w" \
     "\n" \
     "\nThere are filesystem-specific -o flags." \

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
       "[-q] <[-dn] DIR | -x DEVICE>"
#define mountpoint_full_usage "\n\n" \
       "Check if the directory is a mountpoint\n" \
     "\nOptions:" \
     "\n	-q	Quiet" \
     "\n	-d	Print major/minor device number of the filesystem" \
     "\n	-n	Print device name of the filesystem" \
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

#define ntpd_trivial_usage \
	"[-dnqNw"IF_FEATURE_NTPD_SERVER("l")"] [-S PROG] [-p PEER]..."
#define ntpd_full_usage "\n\n" \
       "NTP client/server\n" \
     "\nOptions:" \
     "\n	-d	Verbose" \
     "\n	-n	Do not daemonize" \
     "\n	-q	Quit after clock is set" \
     "\n	-N	Run at high priority" \
     "\n	-w	Do not set time (only query peers), implies -n" \
	IF_FEATURE_NTPD_SERVER( \
     "\n	-l	Run as server on port 123" \
	) \
     "\n	-S PROG	Run PROG after stepping time, stratum change, and every 11 mins" \
     "\n	-p PEER	Obtain time from PEER (may be repeated)" \

/*
#define parse_trivial_usage \
       "[-n MAXTOKENS] [-m MINTOKENS] [-d DELIMS] [-f FLAGS] FILE..."
#define parse_full_usage ""
*/

#define pgrep_trivial_usage \
       "[-flnovx] [-s SID|-P PPID|PATTERN]"
#define pgrep_full_usage "\n\n" \
       "Display process(es) selected by regex PATTERN\n" \
     "\nOptions:" \
     "\n	-l	Show command name too" \
     "\n	-f	Match against entire command line" \
     "\n	-n	Show the newest process only" \
     "\n	-o	Show the oldest process only" \
     "\n	-v	Negate the match" \
     "\n	-x	Match whole name (not substring)" \
     "\n	-s	Match session ID (0 for current)" \
     "\n	-P	Match parent process ID" \

#if (ENABLE_FEATURE_PIDOF_SINGLE || ENABLE_FEATURE_PIDOF_OMIT)
#define pidof_trivial_usage \
       "[OPTIONS] [NAME]..."
#define USAGE_PIDOF "\n\nOptions:"
#else
#define pidof_trivial_usage \
       "[NAME]..."
#define USAGE_PIDOF /* none */
#endif
#define pidof_full_usage "\n\n" \
       "List PIDs of all processes with names that match NAMEs" \
	USAGE_PIDOF \
	IF_FEATURE_PIDOF_SINGLE( \
     "\n	-s	Show only one PID" \
	) \
	IF_FEATURE_PIDOF_OMIT( \
     "\n	-o PID	Omit given pid" \
     "\n		Use %PPID to omit pid of pidof's parent" \
	) \

#define pidof_example_usage \
       "$ pidof init\n" \
       "1\n" \
	IF_FEATURE_PIDOF_OMIT( \
       "$ pidof /bin/sh\n20351 5973 5950\n") \
	IF_FEATURE_PIDOF_OMIT( \
       "$ pidof /bin/sh -o %PPID\n20351 5950")

#define pivot_root_trivial_usage \
       "NEW_ROOT PUT_OLD"
#define pivot_root_full_usage "\n\n" \
       "Move the current root file system to PUT_OLD and make NEW_ROOT\n" \
       "the new root file system"

#define pkill_trivial_usage \
       "[-l|-SIGNAL] [-fnovx] [-s SID|-P PPID|PATTERN]"
#define pkill_full_usage "\n\n" \
       "Send a signal to process(es) selected by regex PATTERN\n" \
     "\nOptions:" \
     "\n	-l	List all signals" \
     "\n	-f	Match against entire command line" \
     "\n	-n	Signal the newest process only" \
     "\n	-o	Signal the oldest process only" \
     "\n	-v	Negate the match" \
     "\n	-x	Match whole name (not substring)" \
     "\n	-s	Match session ID (0 for current)" \
     "\n	-P	Match parent process ID" \


#if ENABLE_DESKTOP

#define ps_trivial_usage \
       "[-o COL1,COL2=HEADER]" IF_FEATURE_SHOW_THREADS(" [-T]")
#define ps_full_usage "\n\n" \
       "Show list of processes\n" \
     "\nOptions:" \
     "\n	-o COL1,COL2=HEADER	Select columns for display" \
	IF_FEATURE_SHOW_THREADS( \
     "\n	-T			Show threads" \
	)

#else /* !ENABLE_DESKTOP */

#if !ENABLE_SELINUX && !ENABLE_FEATURE_PS_WIDE
#define USAGE_PS "\nThis version of ps accepts no options"
#else
#define USAGE_PS "\nOptions:"
#endif

#define ps_trivial_usage \
       ""
#define ps_full_usage "\n\n" \
       "Show list of processes\n" \
	USAGE_PS \
	IF_SELINUX( \
     "\n	-Z	Show selinux context" \
	) \
	IF_FEATURE_PS_WIDE( \
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
       " 2990 andersen andersen R ps\n" \

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

#define rdev_trivial_usage \
       ""
#define rdev_full_usage "\n\n" \
       "Print the device node associated with the filesystem mounted at '/'"
#define rdev_example_usage \
       "$ rdev\n" \
       "/dev/mtdblock9 /\n"

#define readahead_trivial_usage \
       "[FILE]..."
#define readahead_full_usage "\n\n" \
       "Preload FILEs to RAM"

#define readprofile_trivial_usage \
       "[OPTIONS]"
#define readprofile_full_usage "\n\n" \
       "Options:" \
     "\n	-m mapfile	(Default: /boot/System.map)" \
     "\n	-p profile	(Default: /proc/profile)" \
     "\n	-M NUM		Set the profiling multiplier to NUM" \
     "\n	-i		Print only info about the sampling step" \
     "\n	-v		Verbose" \
     "\n	-a		Print all symbols, even if count is 0" \
     "\n	-b		Print individual histogram-bin counts" \
     "\n	-s		Print individual counters within functions" \
     "\n	-r		Reset all the counters (root only)" \
     "\n	-n		Disable byte order auto-detection" \

#define scriptreplay_trivial_usage \
       "timingfile [typescript [divisor]]"
#define scriptreplay_full_usage "\n\n" \
       "Play back typescripts, using timing information"

#define restorecon_trivial_usage \
       "[-iFnRv] [-e EXCLUDEDIR]... [-o FILE] [-f FILE]"
#define restorecon_full_usage "\n\n" \
       "Reset security contexts of files in pathname\n" \
     "\n	-i	Ignore files that don't exist" \
     "\n	-f FILE	File with list of files to process" \
     "\n	-e DIR	Directory to exclude" \
     "\n	-R,-r	Recurse" \
     "\n	-n	Don't change any file labels" \
     "\n	-o FILE	Save list of files with incorrect context" \
     "\n	-v	Verbose" \
     "\n	-vv	Show changed labels" \
     "\n	-F	Force reset of context to match file_context" \
     "\n		for customizable files, or the user section," \
     "\n		if it has changed" \

#define rfkill_trivial_usage \
       "COMMAND [INDEX|TYPE]"
#define rfkill_full_usage "\n\n" \
       "Enable/disable wireless devices\n" \
       "\nCommands:" \
     "\n	list [INDEX|TYPE]	List current state" \
     "\n	block INDEX|TYPE	Disable device" \
     "\n	unblock INDEX|TYPE	Enable device" \
     "\n" \
     "\n	TYPE: all, wlan(wifi), bluetooth, uwb(ultrawideband)," \
     "\n		wimax, wwan, gps, fm" \

#define route_trivial_usage \
       "[{add|del|delete}]"
#define route_full_usage "\n\n" \
       "Edit kernel routing tables\n" \
     "\nOptions:" \
     "\n	-n	Don't resolve names" \
     "\n	-e	Display other/more information" \
     "\n	-A inet" IF_FEATURE_IPV6("{6}") "	Select address family" \

#define rtcwake_trivial_usage \
       "[-a | -l | -u] [-d DEV] [-m MODE] [-s SEC | -t TIME]"
#define rtcwake_full_usage "\n\n" \
       "Enter a system sleep state until specified wakeup time\n" \
	IF_LONG_OPTS( \
     "\n	-a,--auto	Read clock mode from adjtime" \
     "\n	-l,--local	Clock is set to local time" \
     "\n	-u,--utc	Clock is set to UTC time" \
     "\n	-d,--device=DEV	Specify the RTC device" \
     "\n	-m,--mode=MODE	Set the sleep state (default: standby)" \
     "\n	-s,--seconds=SEC Set the timeout in SEC seconds from now" \
     "\n	-t,--time=TIME	Set the timeout to TIME seconds from epoch" \
	) \
	IF_NOT_LONG_OPTS( \
     "\n	-a	Read clock mode from adjtime" \
     "\n	-l	Clock is set to local time" \
     "\n	-u	Clock is set to UTC time" \
     "\n	-d DEV	Specify the RTC device" \
     "\n	-m MODE	Set the sleep state (default: standby)" \
     "\n	-s SEC	Set the timeout in SEC seconds from now" \
     "\n	-t TIME	Set the timeout to TIME seconds from epoch" \
	)

#define runcon_trivial_usage \
       "[-c] [-u USER] [-r ROLE] [-t TYPE] [-l RANGE] PROG ARGS\n" \
       "runcon CONTEXT PROG ARGS"
#define runcon_full_usage "\n\n" \
       "Run PROG in a different security context\n" \
     "\n	CONTEXT		Complete security context\n" \
	IF_FEATURE_RUNCON_LONG_OPTIONS( \
     "\n	-c,--compute	Compute process transition context before modifying" \
     "\n	-t,--type=TYPE	Type (for same role as parent)" \
     "\n	-u,--user=USER	User identity" \
     "\n	-r,--role=ROLE	Role" \
     "\n	-l,--range=RNG	Levelrange" \
	) \
	IF_NOT_FEATURE_RUNCON_LONG_OPTIONS( \
     "\n	-c	Compute process transition context before modifying" \
     "\n	-t TYPE	Type (for same role as parent)" \
     "\n	-u USER	User identity" \
     "\n	-r ROLE	Role" \
     "\n	-l RNG	Levelrange" \
	)

#define runlevel_trivial_usage \
       "[FILE]"
#define runlevel_full_usage "\n\n" \
       "Find the current and previous system runlevel\n" \
       "\n" \
       "If no utmp FILE exists or if no runlevel record can be found,\n" \
       "print \"unknown\""
#define runlevel_example_usage \
       "$ runlevel /var/run/utmp\n" \
       "N 2"

#define runsv_trivial_usage \
       "DIR"
#define runsv_full_usage "\n\n" \
       "Start and monitor a service and optionally an appendant log service"

#define runsvdir_trivial_usage \
       "[-P] [-s SCRIPT] DIR"
#define runsvdir_full_usage "\n\n" \
       "Start a runsv process for each subdirectory. If it exits, restart it.\n" \
     "\n	-P		Put each runsv in a new session" \
     "\n	-s SCRIPT	Run SCRIPT <signo> after signal is processed" \

#define rx_trivial_usage \
       "FILE"
#define rx_full_usage "\n\n" \
       "Receive a file using the xmodem protocol"
#define rx_example_usage \
       "$ rx /tmp/foo\n"

#define script_trivial_usage \
       "[-afq" IF_SCRIPTREPLAY("t") "] [-c PROG] [OUTFILE]"
#define script_full_usage "\n\n" \
       "Options:" \
     "\n	-a	Append output" \
     "\n	-c PROG	Run PROG, not shell" \
     "\n	-f	Flush output after each write" \
     "\n	-q	Quiet" \
	IF_SCRIPTREPLAY( \
     "\n	-t	Send timing to stderr" \
	)

#define selinuxenabled_trivial_usage NOUSAGE_STR
#define selinuxenabled_full_usage ""

#define sestatus_trivial_usage \
       "[-vb]"
#define sestatus_full_usage "\n\n" \
       "	-v	Verbose" \
     "\n	-b	Display current state of booleans" \

#define setenforce_trivial_usage \
       "[Enforcing | Permissive | 1 | 0]"
#define setenforce_full_usage ""

#define setfiles_trivial_usage \
       "[-dnpqsvW] [-e DIR]... [-o FILE] [-r alt_root_path]" \
	IF_FEATURE_SETFILES_CHECK_OPTION( \
       " [-c policyfile] spec_file" \
	) \
       " pathname"
#define setfiles_full_usage "\n\n" \
       "Reset file contexts under pathname according to spec_file\n" \
	IF_FEATURE_SETFILES_CHECK_OPTION( \
     "\n	-c FILE	Check the validity of the contexts against the specified binary policy" \
	) \
     "\n	-d	Show which specification matched each file" \
     "\n	-l	Log changes in file labels to syslog" \
     "\n	-n	Don't change any file labels" \
     "\n	-q	Suppress warnings" \
     "\n	-r DIR	Use an alternate root path" \
     "\n	-e DIR	Exclude DIR" \
     "\n	-F	Force reset of context to match file_context for customizable files" \
     "\n	-o FILE	Save list of files with incorrect context" \
     "\n	-s	Take a list of files from stdin (instead of command line)" \
     "\n	-v	Show changes in file labels, if type or role are changing" \
     "\n	-vv	Show changes in file labels, if type, role, or user are changing" \
     "\n	-W	Display warnings about entries that had no matching files" \

#define setfont_trivial_usage \
       "FONT [-m MAPFILE] [-C TTY]"
#define setfont_full_usage "\n\n" \
       "Load a console font\n" \
     "\nOptions:" \
     "\n	-m MAPFILE	Load console screen map" \
     "\n	-C TTY		Affect TTY instead of /dev/tty" \

#define setfont_example_usage \
       "$ setfont -m koi8-r /etc/i18n/fontname\n"

#define setsebool_trivial_usage \
       "boolean value"

#define setsebool_full_usage "\n\n" \
       "Change boolean setting"

#define setsid_trivial_usage \
       "PROG ARGS"
#define setsid_full_usage "\n\n" \
       "Run PROG in a new session. PROG will have no controlling terminal\n" \
       "and will not be affected by keyboard signals (Ctrl-C etc).\n" \
       "See setsid(2) for details." \

#define last_trivial_usage \
       ""IF_FEATURE_LAST_FANCY("[-HW] [-f FILE]")
#define last_full_usage "\n\n" \
       "Show listing of the last users that logged into the system" \
	IF_FEATURE_LAST_FANCY( "\n" \
     "\nOptions:" \
/*   "\n	-H	Show header line" */ \
     "\n	-W	Display with no host column truncation" \
     "\n	-f FILE Read from FILE instead of /var/log/wtmp" \
	)

#define slattach_trivial_usage \
       "[-cehmLF] [-s SPEED] [-p PROTOCOL] DEVICE"
#define slattach_full_usage "\n\n" \
       "Attach network interface(s) to serial line(s)\n" \
     "\nOptions:" \
     "\n	-p PROT	Set protocol (slip, cslip, slip6, clisp6 or adaptive)" \
     "\n	-s SPD	Set line speed" \
     "\n	-e	Exit after initializing device" \
     "\n	-h	Exit when the carrier is lost" \
     "\n	-c PROG	Run PROG when the line is hung up" \
     "\n	-m	Do NOT initialize the line in raw 8 bits mode" \
     "\n	-L	Enable 3-wire operation" \
     "\n	-F	Disable RTS/CTS flow control" \

#define strings_trivial_usage \
       "[-afo] [-n LEN] [FILE]..."
#define strings_full_usage "\n\n" \
       "Display printable strings in a binary file\n" \
     "\nOptions:" \
     "\n	-a	Scan whole file (default)" \
     "\n	-f	Precede strings with filenames" \
     "\n	-n LEN	At least LEN characters form a string (default 4)" \
     "\n	-o	Precede strings with decimal offsets" \

#define sv_trivial_usage \
       "[-v] [-w SEC] CMD SERVICE_DIR..."
#define sv_full_usage "\n\n" \
       "Control services monitored by runsv supervisor.\n" \
       "Commands (only first character is enough):\n" \
       "\n" \
       "status: query service status\n" \
       "up: if service isn't running, start it. If service stops, restart it\n" \
       "once: like 'up', but if service stops, don't restart it\n" \
       "down: send TERM and CONT signals. If ./run exits, start ./finish\n" \
       "	if it exists. After it stops, don't restart service\n" \
       "exit: send TERM and CONT signals to service and log service. If they exit,\n" \
       "	runsv exits too\n" \
       "pause, cont, hup, alarm, interrupt, quit, 1, 2, term, kill: send\n" \
       "STOP, CONT, HUP, ALRM, INT, QUIT, USR1, USR2, TERM, KILL signal to service" \

#define swapoff_trivial_usage \
       "[-a] [DEVICE]"
#define swapoff_full_usage "\n\n" \
       "Stop swapping on DEVICE\n" \
     "\nOptions:" \
     "\n	-a	Stop swapping on all swap devices" \

#define swapon_trivial_usage \
       "[-a]" IF_FEATURE_SWAPON_PRI(" [-p PRI]") " [DEVICE]"
#define swapon_full_usage "\n\n" \
       "Start swapping on DEVICE\n" \
     "\nOptions:" \
     "\n	-a	Start swapping on all swap devices" \
	IF_FEATURE_SWAPON_PRI( \
     "\n	-p PRI	Set swap device priority" \
	) \

#define switch_root_trivial_usage \
       "[-c /dev/console] NEW_ROOT NEW_INIT [ARGS]"
#define switch_root_full_usage "\n\n" \
       "Free initramfs and switch to another root fs:\n" \
       "chroot to NEW_ROOT, delete all in /, move NEW_ROOT to /,\n" \
       "execute NEW_INIT. PID must be 1. NEW_ROOT must be a mountpoint.\n" \
     "\nOptions:" \
     "\n	-c DEV	Reopen stdio to DEV after switch" \

#define sysctl_trivial_usage \
       "[OPTIONS] [VALUE]..."
#define sysctl_full_usage "\n\n" \
       "Configure kernel parameters at runtime\n" \
     "\nOptions:" \
     "\n	-n	Don't print key names" \
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
       "[OPTIONS]"
#define syslogd_full_usage "\n\n" \
       "System logging utility.\n" \
       "This version of syslogd ignores /etc/syslog.conf\n" \
     "\nOptions:" \
     "\n	-n		Run in foreground" \
     "\n	-O FILE		Log to given file (default:/var/log/messages)" \
     "\n	-l N		Set local log level" \
     "\n	-S		Smaller logging output" \
	IF_FEATURE_ROTATE_LOGFILE( \
     "\n	-s SIZE		Max size (KB) before rotate (default:200KB, 0=off)" \
     "\n	-b N		N rotated logs to keep (default:1, max=99, 0=purge)") \
	IF_FEATURE_REMOTE_LOG( \
     "\n	-R HOST[:PORT]	Log to IP or hostname on PORT (default PORT=514/UDP)" \
     "\n	-L		Log locally and via network (default is network only if -R)") \
	IF_FEATURE_SYSLOGD_DUP( \
     "\n	-D		Drop duplicates") \
	IF_FEATURE_IPC_SYSLOG( \
     "\n	-C[size(KiB)]	Log to shared mem buffer (read it using logread)") \
	/* NB: -Csize shouldn't have space (because size is optional) */
/*   "\n	-m MIN		Minutes between MARK lines (default:20, 0=off)" */

#define syslogd_example_usage \
       "$ syslogd -R masterlog:514\n" \
       "$ syslogd -R 192.168.1.1:601\n"

#define taskset_trivial_usage \
       "[-p] [MASK] [PID | PROG ARGS]"
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

#if ENABLE_FEATURE_TELNET_AUTOLOGIN
#define telnet_trivial_usage \
       "[-a] [-l USER] HOST [PORT]"
#define telnet_full_usage "\n\n" \
       "Connect to telnet server\n" \
     "\nOptions:" \
     "\n	-a	Automatic login with $USER variable" \
     "\n	-l USER	Automatic login as USER" \

#else
#define telnet_trivial_usage \
       "HOST [PORT]"
#define telnet_full_usage "\n\n" \
       "Connect to telnet server"
#endif

#define telnetd_trivial_usage \
       "[OPTIONS]"
#define telnetd_full_usage "\n\n" \
       "Handle incoming telnet connections" \
	IF_NOT_FEATURE_TELNETD_STANDALONE(" via inetd") "\n" \
     "\nOptions:" \
     "\n	-l LOGIN	Exec LOGIN on connect" \
     "\n	-f ISSUE_FILE	Display ISSUE_FILE instead of /etc/issue" \
     "\n	-K		Close connection as soon as login exits" \
     "\n			(normally wait until all programs close slave pty)" \
	IF_FEATURE_TELNETD_STANDALONE( \
     "\n	-p PORT		Port to listen on" \
     "\n	-b ADDR[:PORT]	Address to bind to" \
     "\n	-F		Run in foreground" \
     "\n	-i		Inetd mode" \
	IF_FEATURE_TELNETD_INETD_WAIT( \
     "\n	-w SEC		Inetd 'wait' mode, linger time SEC" \
     "\n	-S		Log to syslog (implied by -i or without -F and -w)" \
	) \
	)

#define tc_trivial_usage \
	/*"[OPTIONS] "*/"OBJECT CMD [dev STRING]"
#define tc_full_usage "\n\n" \
	"OBJECT: {qdisc|class|filter}\n" \
	"CMD: {add|del|change|replace|show}\n" \
	"\n" \
	"qdisc [ handle QHANDLE ] [ root |"IF_FEATURE_TC_INGRESS(" ingress |")" parent CLASSID ]\n" \
	/* "[ estimator INTERVAL TIME_CONSTANT ]\n" */ \
	"	[ [ QDISC_KIND ] [ help | OPTIONS ] ]\n" \
	"	QDISC_KIND := { [p|b]fifo | tbf | prio | cbq | red | etc. }\n" \
	"qdisc show [ dev STRING ]"IF_FEATURE_TC_INGRESS(" [ingress]")"\n" \
	"class [ classid CLASSID ] [ root | parent CLASSID ]\n" \
	"	[ [ QDISC_KIND ] [ help | OPTIONS ] ]\n" \
	"class show [ dev STRING ] [ root | parent CLASSID ]\n" \
	"filter [ pref PRIO ] [ protocol PROTO ]\n" \
	/* "\t[ estimator INTERVAL TIME_CONSTANT ]\n" */ \
	"	[ root | classid CLASSID ] [ handle FILTERID ]\n" \
	"	[ [ FILTER_TYPE ] [ help | OPTIONS ] ]\n" \
	"filter show [ dev STRING ] [ root | parent CLASSID ]"

#define tcpsvd_trivial_usage \
       "[-hEv] [-c N] [-C N[:MSG]] [-b N] [-u USER] [-l NAME] IP PORT PROG"
/* with not-implemented options: */
/*     "[-hpEvv] [-c N] [-C N[:MSG]] [-b N] [-u USER] [-l NAME] [-i DIR|-x CDB] [-t SEC] IP PORT PROG" */
#define tcpsvd_full_usage "\n\n" \
       "Create TCP socket, bind to IP:PORT and listen\n" \
       "for incoming connection. Run PROG for each connection.\n" \
     "\n	IP		IP to listen on. '0' = all" \
     "\n	PORT		Port to listen on" \
     "\n	PROG ARGS	Program to run" \
     "\n	-l NAME		Local hostname (else looks up local hostname in DNS)" \
     "\n	-u USER[:GRP]	Change to user/group after bind" \
     "\n	-c N		Handle up to N connections simultaneously" \
     "\n	-b N		Allow a backlog of approximately N TCP SYNs" \
     "\n	-C N[:MSG]	Allow only up to N connections from the same IP." \
     "\n			New connections from this IP address are closed" \
     "\n			immediately. MSG is written to the peer before close" \
     "\n	-h		Look up peer's hostname" \
     "\n	-E		Don't set up environment variables" \
     "\n	-v		Verbose" \

#define udpsvd_trivial_usage \
       "[-hEv] [-c N] [-u USER] [-l NAME] IP PORT PROG"
#define udpsvd_full_usage "\n\n" \
       "Create UDP socket, bind to IP:PORT and wait\n" \
       "for incoming packets. Run PROG for each packet,\n" \
       "redirecting all further packets with same peer ip:port to it.\n" \
     "\n	IP		IP to listen on. '0' = all" \
     "\n	PORT		Port to listen on" \
     "\n	PROG ARGS	Program to run" \
     "\n	-l NAME		Local hostname (else looks up local hostname in DNS)" \
     "\n	-u USER[:GRP]	Change to user/group after bind" \
     "\n	-c N		Handle up to N connections simultaneously" \
     "\n	-h		Look up peer's hostname" \
     "\n	-E		Don't set up environment variables" \
     "\n	-v		Verbose" \

#define tftp_trivial_usage \
       "[OPTIONS] HOST [PORT]"
#define tftp_full_usage "\n\n" \
       "Transfer a file from/to tftp server\n" \
     "\nOptions:" \
     "\n	-l FILE	Local FILE" \
     "\n	-r FILE	Remote FILE" \
	IF_FEATURE_TFTP_GET( \
     "\n	-g	Get file" \
	) \
	IF_FEATURE_TFTP_PUT( \
     "\n	-p	Put file" \
	) \
	IF_FEATURE_TFTP_BLOCKSIZE( \
     "\n	-b SIZE	Transfer blocks of SIZE octets" \
	)

#define tftpd_trivial_usage \
       "[-cr] [-u USER] [DIR]"
#define tftpd_full_usage "\n\n" \
       "Transfer a file on tftp client's request\n" \
       "\n" \
       "tftpd should be used as an inetd service.\n" \
       "tftpd's line for inetd.conf:\n" \
       "	69 dgram udp nowait root tftpd tftpd /files/to/serve\n" \
       "It also can be ran from udpsvd:\n" \
       "	udpsvd -vE 0.0.0.0 69 tftpd /files/to/serve\n" \
     "\nOptions:" \
     "\n	-r	Prohibit upload" \
     "\n	-c	Allow file creation via upload" \
     "\n	-u	Access files as USER" \

#define time_trivial_usage \
       "[-v] PROG ARGS"
#define time_full_usage "\n\n" \
       "Run PROG, display resource usage when it exits\n" \
     "\nOptions:" \
     "\n	-v	Verbose" \

#define timeout_trivial_usage \
       "[-t SECS] [-s SIG] PROG ARGS"
#define timeout_full_usage "\n\n" \
       "Runs PROG. Sends SIG to it if it is not gone in SECS seconds.\n" \
       "Defaults: SECS: 10, SIG: TERM." \

#define traceroute_trivial_usage \
       "[-"IF_TRACEROUTE6("46")"FIldnrv] [-f 1ST_TTL] [-m MAXTTL] [-p PORT] [-q PROBES]\n" \
       "	[-s SRC_IP] [-t TOS] [-w WAIT_SEC] [-g GATEWAY] [-i IFACE]\n" \
       "	[-z PAUSE_MSEC] HOST [BYTES]"
#define traceroute_full_usage "\n\n" \
       "Trace the route to HOST\n" \
     "\nOptions:" \
	IF_TRACEROUTE6( \
     "\n	-4,-6	Force IP or IPv6 name resolution" \
	) \
     "\n	-F	Set the don't fragment bit" \
     "\n	-I	Use ICMP ECHO instead of UDP datagrams" \
     "\n	-l	Display the TTL value of the returned packet" \
     "\n	-d	Set SO_DEBUG options to socket" \
     "\n	-n	Print numeric addresses" \
     "\n	-r	Bypass routing tables, send directly to HOST" \
     "\n	-v	Verbose" \
     "\n	-m	Max time-to-live (max number of hops)" \
     "\n	-p	Base UDP port number used in probes" \
     "\n		(default 33434)" \
     "\n	-q	Number of probes per TTL (default 3)" \
     "\n	-s	IP address to use as the source address" \
     "\n	-t	Type-of-service in probe packets (default 0)" \
     "\n	-w	Time in seconds to wait for a response (default 3)" \
     "\n	-g	Loose source route gateway (8 max)" \

#define traceroute6_trivial_usage \
       "[-dnrv] [-m MAXTTL] [-p PORT] [-q PROBES]\n" \
       "	[-s SRC_IP] [-t TOS] [-w WAIT_SEC] [-i IFACE]\n" \
       "	HOST [BYTES]"
#define traceroute6_full_usage "\n\n" \
       "Trace the route to HOST\n" \
     "\nOptions:" \
     "\n	-d	Set SO_DEBUG options to socket" \
     "\n	-n	Print numeric addresses" \
     "\n	-r	Bypass routing tables, send directly to HOST" \
     "\n	-v	Verbose" \
     "\n	-m	Max time-to-live (max number of hops)" \
     "\n	-p	Base UDP port number used in probes" \
     "\n		(default is 33434)" \
     "\n	-q	Number of probes per TTL (default 3)" \
     "\n	-s	IP address to use as the source address" \
     "\n	-t	Type-of-service in probe packets (default 0)" \
     "\n	-w	Time in seconds to wait for a response (default 3)" \

#define ttysize_trivial_usage \
       "[w] [h]"
#define ttysize_full_usage "\n\n" \
       "Print dimension(s) of stdin's terminal, on error return 80x25"

#define tunctl_trivial_usage \
       "[-f device] ([-t name] | -d name)" IF_FEATURE_TUNCTL_UG(" [-u owner] [-g group] [-b]")
#define tunctl_full_usage "\n\n" \
       "Create or delete tun interfaces\n" \
     "\nOptions:" \
     "\n	-f name		tun device (/dev/net/tun)" \
     "\n	-t name		Create iface 'name'" \
     "\n	-d name		Delete iface 'name'" \
	IF_FEATURE_TUNCTL_UG( \
     "\n	-u owner	Set iface owner" \
     "\n	-g group	Set iface group" \
     "\n	-b		Brief output" \
	)
#define tunctl_example_usage \
       "# tunctl\n" \
       "# tunctl -d tun0\n"

#define udhcpd_trivial_usage \
       "[-fS]" IF_FEATURE_UDHCP_PORT(" [-P N]") " [CONFFILE]" \

#define udhcpd_full_usage "\n\n" \
       "DHCP server\n" \
     "\n	-f	Run in foreground" \
     "\n	-S	Log to syslog too" \
	IF_FEATURE_UDHCP_PORT( \
     "\n	-P N	Use port N (default 67)" \
	)

#define umount_trivial_usage \
       "[OPTIONS] FILESYSTEM|DIRECTORY"
#define umount_full_usage "\n\n" \
       "Unmount file systems\n" \
     "\nOptions:" \
	IF_FEATURE_UMOUNT_ALL( \
     "\n	-a	Unmount all file systems" IF_FEATURE_MTAB_SUPPORT(" in /etc/mtab") \
	) \
	IF_FEATURE_MTAB_SUPPORT( \
     "\n	-n	Don't erase /etc/mtab entries" \
	) \
     "\n	-r	Try to remount devices as read-only if mount is busy" \
     "\n	-l	Lazy umount (detach filesystem)" \
     "\n	-f	Force umount (i.e., unreachable NFS server)" \
	IF_FEATURE_MOUNT_LOOP( \
     "\n	-d	Free loop device if it has been used" \
	)

#define umount_example_usage \
       "$ umount /dev/hdc1\n"

#define uptime_trivial_usage \
       ""
#define uptime_full_usage "\n\n" \
       "Display the time since the last boot"

#define uptime_example_usage \
       "$ uptime\n" \
       "  1:55pm  up  2:30, load average: 0.09, 0.04, 0.00\n"

#define vconfig_trivial_usage \
       "COMMAND [OPTIONS]"
#define vconfig_full_usage "\n\n" \
       "Create and remove virtual ethernet devices\n" \
     "\nOptions:" \
     "\n	add		[interface-name] [vlan_id]" \
     "\n	rem		[vlan-name]" \
     "\n	set_flag	[interface-name] [flag-num] [0 | 1]" \
     "\n	set_egress_map	[vlan-name] [skb_priority] [vlan_qos]" \
     "\n	set_ingress_map	[vlan-name] [skb_priority] [vlan_qos]" \
     "\n	set_name_type	[name-type]" \

#define volname_trivial_usage \
       "[DEVICE]"
#define volname_full_usage "\n\n" \
       "Show CD volume name of the DEVICE (default /dev/cdrom)"

#define wall_trivial_usage \
	"[FILE]"
#define wall_full_usage "\n\n" \
	"Write content of FILE or stdin to all logged-in users"
#define wall_sample_usage \
	"echo foo | wall\n" \
	"wall ./mymessage"

#define watch_trivial_usage \
       "[-n SEC] [-t] PROG ARGS"
#define watch_full_usage "\n\n" \
       "Run PROG periodically\n" \
     "\nOptions:" \
     "\n	-n	Loop period in seconds (default 2)" \
     "\n	-t	Don't print header" \

#define watch_example_usage \
       "$ watch date\n" \
       "Mon Dec 17 10:31:40 GMT 2000\n" \
       "Mon Dec 17 10:31:42 GMT 2000\n" \
       "Mon Dec 17 10:31:44 GMT 2000"

#define watchdog_trivial_usage \
       "[-t N[ms]] [-T N[ms]] [-F] DEV"
#define watchdog_full_usage "\n\n" \
       "Periodically write to watchdog device DEV\n" \
     "\nOptions:" \
     "\n	-T N	Reboot after N seconds if not reset (default 60)" \
     "\n	-t N	Reset every N seconds (default 30)" \
     "\n	-F	Run in foreground" \
     "\n" \
     "\nUse 500ms to specify period in milliseconds" \

#define zcip_trivial_usage \
       "[OPTIONS] IFACE SCRIPT"
#define zcip_full_usage "\n\n" \
       "Manage a ZeroConf IPv4 link-local address\n" \
     "\nOptions:" \
     "\n	-f		Run in foreground" \
     "\n	-q		Quit after obtaining address" \
     "\n	-r 169.254.x.x	Request this address first" \
     "\n	-v		Verbose" \
     "\n" \
     "\nWith no -q, runs continuously monitoring for ARP conflicts," \
     "\nexits only on I/O errors (link down etc)" \


#endif
