/* vi: set sw=4 ts=4: */
/*
 * password utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#define assert(x) ((void)0)

/* internal function for bb_getpwuid and bb_getgrgid */
/* Hacked by Tito Ragusa (c) 2004 <farmatito@tiscali.it> to make it more
 * flexible:
 *
 * bufsize > 0:      If idname is not NULL it is copied to buffer,
 *                   and buffer is returned. Else id as string is written
 *                   to buffer, and NULL is returned.
 *
 * bufsize == 0:     idname is returned.
 *
 * bufsize < 0:      If idname is not NULL it is returned.
 *                   Else an error message is printed and the program exits.
 */
static char* bb_getug(char *buffer, int bufsize, char *idname, long id, char prefix)
{
	if (bufsize > 0) {
		assert(buffer != NULL);
		if (idname) {
			return safe_strncpy(buffer, idname, bufsize);
		}
		snprintf(buffer, bufsize, "%ld", id);
	} else if (bufsize < 0 && !idname) {
		bb_error_msg_and_die("unknown %cid %ld", prefix, id);
	}
	return idname;
}

/* bb_getpwuid, bb_getgrgid:
 * bb_getXXXid(buf, bufsz, id) - copy user/group name or id
 *               as a string to buf, return user/group name or NULL
 * bb_getXXXid(NULL, 0, id) - return user/group name or NULL
 * bb_getXXXid(NULL, -1, id) - return user/group name or exit
 */
/* gets a username given a uid */
char* bb_getpwuid(char *name, int bufsize, long uid)
{
	struct passwd *myuser = getpwuid(uid);

	return bb_getug(name, bufsize,
			(myuser ? myuser->pw_name : (char*)myuser),
			uid, 'u');
}
/* gets a groupname given a gid */
char* bb_getgrgid(char *group, int bufsize, long gid)
{
	struct group *mygroup = getgrgid(gid);

	return bb_getug(group, bufsize,
			(mygroup ? mygroup->gr_name : (char*)mygroup),
			gid, 'g');
}

/* returns a gid given a group name */
long xgroup2gid(const char *name)
{
	struct group *mygroup;

	mygroup = getgrnam(name);
	if (mygroup == NULL)
		bb_error_msg_and_die("unknown group name: %s", name);

	return mygroup->gr_gid;
}

/* returns a uid given a username */
long xuname2uid(const char *name)
{
	struct passwd *myuser;

	myuser = getpwnam(name);
	if (myuser == NULL)
		bb_error_msg_and_die("unknown user name: %s", name);

	return myuser->pw_uid;
}

unsigned long get_ug_id(const char *s,
		long (*xname2id)(const char *))
{
	unsigned long r;

	r = bb_strtoul(s, NULL, 10);
	if (errno)
		return xname2id(s);
	return r;
}
