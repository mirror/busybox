/* Hack. kbuild will not define needed macros for config symbols
 * which depend on other symbols, which themself are off.
 * Provide them here by hand. Need a better idea. */

#ifndef ENABLE_KILLALL5
#define ENABLE_KILLALL5 0
#define    USE_KILLALL5(...)
#define   SKIP_KILLALL5(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_QUERY_MODULE_INTERFACE
#define ENABLE_FEATURE_QUERY_MODULE_INTERFACE 0
#define    USE_FEATURE_QUERY_MODULE_INTERFACE(...)
#define   SKIP_FEATURE_QUERY_MODULE_INTERFACE(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_CLEAN_UP
#define ENABLE_FEATURE_CLEAN_UP 0
#define    USE_FEATURE_CLEAN_UP(...)
#define   SKIP_FEATURE_CLEAN_UP(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_SH_STANDALONE_SHELL
#define ENABLE_FEATURE_SH_STANDALONE_SHELL 0
#define    USE_FEATURE_SH_STANDALONE_SHELL(...)
#define   SKIP_FEATURE_SH_STANDALONE_SHELL(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_MTAB_SUPPORT
#define ENABLE_FEATURE_MTAB_SUPPORT 0
#define    USE_FEATURE_MTAB_SUPPORT(...)
#define   SKIP_FEATURE_MTAB_SUPPORT(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_PRESERVE_HARDLINKS
#define ENABLE_FEATURE_PRESERVE_HARDLINKS 0
#define    USE_FEATURE_PRESERVE_HARDLINKS(...)
#define   SKIP_FEATURE_PRESERVE_HARDLINKS(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_AUTOWIDTH
#define ENABLE_FEATURE_AUTOWIDTH 0
#define    USE_FEATURE_AUTOWIDTH(...)
#define   SKIP_FEATURE_AUTOWIDTH(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_SUID_CONFIG
#define ENABLE_FEATURE_SUID_CONFIG 0
#define    USE_FEATURE_SUID_CONFIG(...)
#define   SKIP_FEATURE_SUID_CONFIG(...) __VA_ARGS__
#endif

#ifndef ENABLE_APP_DUMPLEASES
#define ENABLE_APP_DUMPLEASES 0
#define    USE_APP_DUMPLEASES(...)
#define   SKIP_APP_DUMPLEASES(...) __VA_ARGS__
#endif

#ifndef ENABLE_E2LABEL
#define ENABLE_E2LABEL 0
#define    USE_E2LABEL(...)
#define   SKIP_E2LABEL(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_GREP_EGREP_ALIAS
#define ENABLE_FEATURE_GREP_EGREP_ALIAS 0
#define    USE_FEATURE_GREP_EGREP_ALIAS(...)
#define   SKIP_FEATURE_GREP_EGREP_ALIAS(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_GREP_FGREP_ALIAS
#define ENABLE_FEATURE_GREP_FGREP_ALIAS 0
#define    USE_FEATURE_GREP_FGREP_ALIAS(...)
#define   SKIP_FEATURE_GREP_FGREP_ALIAS(...) __VA_ARGS__
#endif

#ifndef ENABLE_FINDFS
#define ENABLE_FINDFS 0
#define    USE_FINDFS(...)
#define   SKIP_FINDFS(...) __VA_ARGS__
#endif

#ifndef ENABLE_IPADDR
#define ENABLE_IPADDR 0
#define    USE_IPADDR(...)
#define   SKIP_IPADDR(...) __VA_ARGS__
#endif

#ifndef ENABLE_IPLINK
#define ENABLE_IPLINK 0
#define    USE_IPLINK(...)
#define   SKIP_IPLINK(...) __VA_ARGS__
#endif

#ifndef ENABLE_IPROUTE
       
#define ENABLE_IPROUTE 0
#define    USE_IPROUTE(...)
#define   SKIP_IPROUTE(...) __VA_ARGS__
#endif

#ifndef ENABLE_IPTUNNEL
#define ENABLE_IPTUNNEL 0
#define    USE_IPTUNNEL(...)
#define   SKIP_IPTUNNEL(...) __VA_ARGS__
#endif

#ifndef ENABLE_KILLALL
#define ENABLE_KILLALL 0
#define    USE_KILLALL(...)
#define   SKIP_KILLALL(...) __VA_ARGS__
#endif

#ifndef ENABLE_KLOGD
#define ENABLE_KLOGD 0
#define    USE_KLOGD(...)
#define   SKIP_KLOGD(...) __VA_ARGS__
#endif

#ifndef ENABLE_FEATURE_INITRD
#define ENABLE_FEATURE_INITRD 0
#define    USE_FEATURE_INITRD(...)
#define   SKIP_FEATURE_INITRD(...) __VA_ARGS__
#endif

#ifndef ENABLE_LOGREAD
#define ENABLE_LOGREAD 0
#define    USE_LOGREAD(...)
#define   SKIP_LOGREAD(...) __VA_ARGS__
#endif

#ifndef ENABLE_PING6
#define ENABLE_PING6 0
#define    USE_PING6(...)
#define   SKIP_PING6(...) __VA_ARGS__
#endif

#ifndef ENABLE_UNIX2DOS
#define ENABLE_UNIX2DOS 0
#define    USE_UNIX2DOS(...)
#define   SKIP_UNIX2DOS(...) __VA_ARGS__
#endif
