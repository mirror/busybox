#ifndef	__BB_PWD_H
#define	__BB_PWD_H

#if defined USE_SYSTEM_PWD_GRP
#include <pwd.h>
#else

#define bb_setpwent setpwent
#define bb_endpwent endpwent
#define bb_getpwent getpwent
#define bb_putpwent putpwent
#define bb_getpw getpw
#define bb_fgetpwent fgetpwent
#define bb_getpwuid getpwuid
#define bb_getpwnam getpwnam
#define __bb_getpwent __bb_getpwent


#include <sys/types.h>
#include <features.h>
#include <stdio.h>

/* The passwd structure.  */
struct passwd
{
  char *pw_name;		/* Username.  */
  char *pw_passwd;		/* Password.  */
  uid_t pw_uid;			/* User ID.  */
  gid_t pw_gid;			/* Group ID.  */
  char *pw_gecos;		/* Real name.  */
  char *pw_dir;			/* Home directory.  */
  char *pw_shell;		/* Shell program.  */
};

extern void bb_setpwent __P ((void));
extern void bb_endpwent __P ((void));
extern struct passwd * bb_getpwent __P ((void));

extern int bb_putpwent __P ((__const struct passwd * __p, FILE * __f));
extern int bb_getpw __P ((uid_t uid, char *buf));

extern struct passwd * bb_fgetpwent __P ((FILE * file));

extern struct passwd * bb_getpwuid __P ((__const uid_t));
extern struct passwd * bb_getpwnam __P ((__const char *));

extern struct passwd * __bb_getpwent __P ((__const int passwd_fd));

#endif /* USE_SYSTEM_PWD_GRP */
#endif /* __BB_PWD_H  */

