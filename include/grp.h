#ifndef	__BB_GRP_H
#define	__BB_GRP_H

#if defined USE_SYSTEM_PWD_GRP
#include <grp.h>
#else

#define bb_setgrent setgrent
#define bb_endgrent endgrent
#define bb_getgrent getgrent
#define bb_getgrgid getgrgid
#define bb_getgrnam getgrnam
#define bb_fgetgrent fgetgrent
#define bb_setgroups setgroups
#define bb_initgroups initgroups
#define __bb_getgrent __getgrent

#include <sys/types.h>
#include <features.h>
#include <stdio.h>

/* The group structure */
struct group
{
  char *gr_name;		/* Group name.	*/
  char *gr_passwd;		/* Password.	*/
  gid_t gr_gid;			/* Group ID.	*/
  char **gr_mem;		/* Member list.	*/
};

extern void bb_setgrent __P ((void));
extern void bb_endgrent __P ((void));
extern struct group * bb_getgrent __P ((void));

extern struct group * bb_getgrgid __P ((__const gid_t gid));
extern struct group * bb_getgrnam __P ((__const char * name));

extern struct group * bb_fgetgrent __P ((FILE * file));

extern int bb_setgroups __P ((size_t n, __const gid_t * groups));
extern int bb_initgroups __P ((__const char * user, gid_t gid));

extern struct group * __bb_getgrent __P ((int grp_fd));

#endif /* USE_SYSTEM_PWD_GRP */
#endif /* __BB_GRP_H */

