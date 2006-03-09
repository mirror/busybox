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

#if 0
/* Attribute __malloc__ on functions was valid as of gcc 2.96. */
#ifndef ATTRIBUTE_MALLOC
# if __GNUC_PREREQ (2,96)
#  define ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# else
#  define ATTRIBUTE_MALLOC
# endif /* GNUC >= 2.96 */
#endif /* ATTRIBUTE_MALLOC */
#endif

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif /* ATTRIBUTE_UNUSED */

#ifndef ATTRIBUTE_NORETURN
#define ATTRIBUTE_NORETURN __attribute__ ((__noreturn__))
#endif /* ATTRIBUTE_NORETURN */

#ifndef ATTRIBUTE_PACKED
#define ATTRIBUTE_PACKED __attribute__ ((__packed__))
#endif /* ATTRIBUTE_NORETURN */

#ifndef ATTRIBUTE_ALIGNED
#define ATTRIBUTE_ALIGNED(m) __attribute__ ((__aligned__(m)))
#endif /* ATTRIBUTE_ALIGNED */

/* -fwhole-program makes all symbols local. The attribute externally_visible
   forces a symbol global.  */
#ifndef ATTRIBUTE_EXTERNALLY_VISIBLE
# if __GNUC_PREREQ (4,1)
# define ATTRIBUTE_EXTERNALLY_VISIBLE __attribute__ ((__externally_visible__))
# else
# define ATTRIBUTE_EXTERNALLY_VISIBLE
# endif /* GNUC >= 4.1 */
#endif /* ATTRIBUTE_EXTERNALLY_VISIBLE */

/* We use __extension__ in some places to suppress -pedantic warnings
   about GCC extensions.  This feature didn't work properly before
   gcc 2.8.  */
#if !__GNUC_PREREQ (2,8)
# ifndef __extension__
#  define __extension__
# endif
#endif

/* ---- Endian Detection ------------------------------------ */
#ifndef __APPLE__
    #include <byteswap.h>
    #include <endian.h>
#endif

#ifdef __BIG_ENDIAN__
    #define BB_BIG_ENDIAN 1
#elif __BYTE_ORDER == __BIG_ENDIAN
    #define BB_BIG_ENDIAN 1
#else
    #define BB_BIG_ENDIAN 0
#endif

/* ---- Networking ------------------------------------------ */
#ifndef __APPLE__
#include <arpa/inet.h>
#else
#include <netinet/in.h>
#endif

/* ---- miscellaneous --------------------------------------- */
/* NLS stuff */
/* THIS SHOULD BE CLEANED OUT OF THE TREE ENTIRELY */
#define _(Text) Text
#define N_(Text) (Text)

#endif	/* platform.h	*/
