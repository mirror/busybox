/* vi: set sw=4 ts=4: */
/*
   Copyright 2006, Bernhard Fischer

   Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
*/
#ifndef	__PLATFORM_H
#define __PLATFORM_H	1

/* Convenience macros to test the version of gcc. */
#undef __GNUC_PREREQ
#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_PREREQ(maj, min) \
	        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define __GNUC_PREREQ(maj, min) 0
#endif

/* __restrict is known in EGCS 1.2 and above. */
#if !__GNUC_PREREQ (2,92)
# ifndef __restrict
#  define __restrict     /* Ignore */
# endif
#endif

/* Define macros for some gcc attributes.  This permits us to use the
   macros freely, and know that they will come into play for the
   version of gcc in which they are supported.  */

#if !__GNUC_PREREQ (2,7)
# ifndef __attribute__
#  define __attribute__(x)
# endif
#endif

#undef inline
#if __STDC_VERSION__ > 199901L
/* it's a keyword */
#else
# if __GNUC_PREREQ (2,7)
#  define inline __inline__
# else
#  define inline
# endif
#endif

#ifndef __const
# define __const const
#endif

# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# define ATTRIBUTE_NORETURN __attribute__ ((__noreturn__))
# define ATTRIBUTE_PACKED __attribute__ ((__packed__))
# define ATTRIBUTE_ALIGNED(m) __attribute__ ((__aligned__(m)))
# if __GNUC_PREREQ (3,0)
#  define ATTRIBUTE_ALWAYS_INLINE __attribute__ ((always_inline)) inline
# else
#  define ATTRIBUTE_ALWAYS_INLINE inline
# endif

/* -fwhole-program makes all symbols local. The attribute externally_visible
   forces a symbol global.  */
# if __GNUC_PREREQ (4,1)
#  define ATTRIBUTE_EXTERNALLY_VISIBLE __attribute__ ((__externally_visible__))
# else
#  define ATTRIBUTE_EXTERNALLY_VISIBLE
# endif /* GNUC >= 4.1 */

/* We use __extension__ in some places to suppress -pedantic warnings
   about GCC extensions.  This feature didn't work properly before
   gcc 2.8.  */
#if !__GNUC_PREREQ (2,8)
# ifndef __extension__
#  define __extension__
# endif
#endif

/* ---- Endian Detection ------------------------------------ */

#if (defined __digital__ && defined __unix__)
# include <sex.h>
# define __BIG_ENDIAN__ (BYTE_ORDER == BIG_ENDIAN)
# define __BYTE_ORDER BYTE_ORDER
#elif !defined __APPLE__
# include <byteswap.h>
# include <endian.h>
#endif

#ifdef __BIG_ENDIAN__
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#elif __BYTE_ORDER == __BIG_ENDIAN
# define BB_BIG_ENDIAN 1
# define BB_LITTLE_ENDIAN 0
#else
# define BB_BIG_ENDIAN 0
# define BB_LITTLE_ENDIAN 1
#endif

#if BB_BIG_ENDIAN
#define SWAP_BE16(x) (x)
#define SWAP_BE32(x) (x)
#define SWAP_BE64(x) (x)
#define SWAP_LE16(x) bswap_16(x)
#define SWAP_LE32(x) bswap_32(x)
#define SWAP_LE64(x) bswap_64(x)
#else
#define SWAP_BE16(x) bswap_16(x)
#define SWAP_BE32(x) bswap_32(x)
#define SWAP_BE64(x) bswap_64(x)
#define SWAP_LE16(x) (x)
#define SWAP_LE32(x) (x)
#define SWAP_LE64(x) (x)
#endif

/* ---- Networking ------------------------------------------ */
#ifndef __APPLE__
# include <arpa/inet.h>
#else
# include <netinet/in.h>
#endif

#ifndef __socklen_t_defined
typedef int socklen_t;
#endif

/* ---- Compiler dependent settings ------------------------- */
#ifndef __GNUC__
#if defined __INTEL_COMPILER
__extension__ typedef __signed__ long long __s64;
__extension__ typedef unsigned long long __u64;
#endif /* __INTEL_COMPILER */
#endif /* ifndef __GNUC__ */

#if (defined __digital__ && defined __unix__)
# undef HAVE_MNTENT_H
#else
# define HAVE_MNTENT_H 1
#endif /* ___digital__ && __unix__ */

/*----- Kernel versioning ------------------------------------*/
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

/* ---- miscellaneous --------------------------------------- */

#if defined(__GNU_LIBRARY__) && __GNU_LIBRARY__ < 5 && \
	!defined(__dietlibc__) && \
	!defined(_NEWLIB_VERSION) && \
	!(defined __digital__ && defined __unix__)
# error "Sorry, this libc version is not supported :("
#endif

// Don't perpetuate e2fsck crap into the headers.  Clean up e2fsck instead.

#if defined __GLIBC__ || defined __UCLIBC__ \
	|| defined __dietlibc__ || defined _NEWLIB_VERSION
#include <features.h>
#define HAVE_FEATURES_H
#include <stdint.h>
#define HAVE_STDINT_H
#else
/* Largest integral types.  */
#if __BIG_ENDIAN__
typedef long int                intmax_t;
typedef unsigned long int       uintmax_t;
#else
__extension__
typedef long long int           intmax_t;
__extension__
typedef unsigned long long int  uintmax_t;
#endif
#endif

/* uclibc does not implement daemon for no-mmu systems.
 * For 0.9.29 and svn, __ARCH_USE_MMU__ indicates no-mmu reliably.
 * For earlier versions there is no reliable way to check if we are building
 * for a mmu-less system; the user should pass EXTRA_CFLAGS="-DBB_NOMMU"
 * on his own.
 */
#if defined __UCLIBC__ && __UCLIBC_MAJOR__ >= 0 && __UCLIBC_MINOR__ >= 9 && \
    __UCLIBC_SUBLEVEL__ > 28 && !defined __ARCH_USE_MMU__
#define BB_NOMMU
#endif

/* Platforms that haven't got dprintf need to implement fdprintf() in
 * libbb.  This would require a platform.c.  It's not going to be cleaned
 * out of the tree, so stop saying it should be. */
#define fdprintf dprintf

/* THIS SHOULD BE CLEANED OUT OF THE TREE ENTIRELY */
/* FIXME: fix tar.c! */
#ifndef FNM_LEADING_DIR
#define FNM_LEADING_DIR 0
#endif

#if (defined __digital__ && defined __unix__)
#include <standards.h>
#define HAVE_STANDARDS_H
#include <inttypes.h>
#define HAVE_INTTYPES_H
#define PRIu32 "u"

/* use legacy setpgrp(pidt_,pid_t) for now.  move to platform.c */
#define bb_setpgrp do{pid_t __me = getpid();setpgrp(__me,__me);}while(0)

#if !defined ADJ_OFFSET_SINGLESHOT && defined MOD_CLKA && defined MOD_OFFSET
#define ADJ_OFFSET_SINGLESHOT (MOD_CLKA | MOD_OFFSET)
#endif
#if !defined ADJ_FREQUENCY && defined MOD_FREQUENCY
#define ADJ_FREQUENCY MOD_FREQUENCY
#endif
#if !defined ADJ_TIMECONST && defined MOD_TIMECONST
#define ADJ_TIMECONST MOD_TIMECONST
#endif
#if !defined ADJ_TICK && defined MOD_CLKB
#define ADJ_TICK MOD_CLKB
#endif

#else
#define bb_setpgrp setpgrp()
#endif

#if defined(__linux__)
#include <sys/mount.h>
#if !defined(BLKSSZGET)
#define BLKSSZGET _IO(0x12, 104)
#endif
#if !defined(BLKGETSIZE64)
#define BLKGETSIZE64 _IOR(0x12,114,size_t)
#endif
#endif

#endif	/* platform.h	*/
