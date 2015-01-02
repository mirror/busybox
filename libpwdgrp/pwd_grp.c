/* vi: set sw=4 ts=4: */
/* Copyright (C) 2014   Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
/* This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY!!
 *
 * Rewrite of some parts. Main differences are:
 *
 * 1) the buffer for getpwuid, getgrgid, getpwnam, getgrnam is dynamically
 *    allocated and reused by later calls. if ERANGE error pops up it is
 *    reallocated to the size of the longest line found so far in the
 *    passwd/group files and reused for later calls.
 *    If ENABLE_FEATURE_CLEAN_UP is set the buffers are freed at program
 *    exit using the atexit function to make valgrind happy.
 * 2) the passwd/group files:
 *      a) must contain the expected number of fields (as per count of field
 *         delimeters ":") or we will complain with a error message.
 *      b) leading or trailing whitespace in fields is allowed and handled.
 *      c) some fields are not allowed to be empty (e.g. username, uid/gid,
 *         homedir, shell) and in this case NULL is returned and errno is
 *         set to EINVAL. This behaviour could be easily changed by
 *         modifying PW_DEF, GR_DEF, SP_DEF strings (uppercase
 *         makes a field mandatory).
 *      d) the string representing uid/gid must be convertible by strtoXX
 *         functions or NULL is returned and errno is set to EINVAL.
 *      e) leading or trailing whitespaces in member names and empty members
 *         are allowed and handled.
 * 3) the internal function for getgrouplist uses a dynamically allocated
 *    buffer and retries with a bigger one in case it is too small;
 * 4) the _r functions use the user supplied buffers that are never reallocated
 *    but use mostly the same common code as the other functions.
 * 5) at the moment only the functions really used by busybox code are
 *    implemented, if you need a particular missing function it should be
 *    easy to write it by using the internal common code.
 */

#include "libbb.h"

/* S = string not empty, s = string maybe empty,  */
/* I = uid,gid, l = long maybe empty, m = members,*/
/* r = reserved */
#define PW_DEF "SsIIsSS"
#define GR_DEF "SsIm"
#define SP_DEF "Ssllllllr"

static const uint8_t pw_off[] ALIGN1 = {
	offsetof(struct passwd, pw_name),       /* 0 S */
	offsetof(struct passwd, pw_passwd),     /* 1 s */
	offsetof(struct passwd, pw_uid),        /* 2 I */
	offsetof(struct passwd, pw_gid),        /* 3 I */
	offsetof(struct passwd, pw_gecos),      /* 4 s */
	offsetof(struct passwd, pw_dir),        /* 5 S */
	offsetof(struct passwd, pw_shell)       /* 6 S */
};
static const uint8_t gr_off[] ALIGN1 = {
	offsetof(struct group, gr_name),        /* 0 S */
	offsetof(struct group, gr_passwd),      /* 1 s */
	offsetof(struct group, gr_gid),         /* 2 I */
	offsetof(struct group, gr_mem)          /* 3 m (char **) */
};
#if ENABLE_USE_BB_SHADOW
static const uint8_t sp_off[] ALIGN1 = {
	offsetof(struct spwd, sp_namp),         /* 0 S Login name */
	offsetof(struct spwd, sp_pwdp),         /* 1 s Encrypted password */
	offsetof(struct spwd, sp_lstchg),       /* 2 l */
	offsetof(struct spwd, sp_min),          /* 3 l */
	offsetof(struct spwd, sp_max),          /* 4 l */
	offsetof(struct spwd, sp_warn),         /* 5 l */
	offsetof(struct spwd, sp_inact),        /* 6 l */
	offsetof(struct spwd, sp_expire),       /* 7 l */
	offsetof(struct spwd, sp_flag)          /* 8 r Reserved */
};
#endif

struct const_passdb {
	const char *filename;
	const uint8_t *off;
	const char def[10];
	uint8_t numfields;
	uint8_t size_of;
};
struct passdb {
	const char *filename;
	const uint8_t *off;
	const char def[10];
	uint8_t numfields;
	uint8_t size_of;
	FILE *fp;
	void *malloced;
};

static const struct const_passdb const_pw_db = { _PATH_PASSWD, pw_off, PW_DEF, sizeof(PW_DEF)-1, sizeof(struct passwd) };
static const struct const_passdb const_gr_db = { _PATH_GROUP , gr_off, GR_DEF, sizeof(GR_DEF)-1, sizeof(struct group) };
#if ENABLE_USE_BB_SHADOW
static const struct const_passdb const_sp_db = { _PATH_SHADOW, sp_off, SP_DEF, sizeof(SP_DEF)-1, sizeof(struct spwd) };
#endif

/* We avoid having big global data. */
struct statics {
	/* It's ok to use one buffer for getpwuid and getpwnam. Manpage says:
	 * "The return value may point to a static area, and may be overwritten
	 * by subsequent calls to getpwent(), getpwnam(), or getpwuid()."
	 */
	struct passdb db[2 + ENABLE_USE_BB_SHADOW];
	char *tokenize_end;
};

static struct statics *ptr_to_statics;
#define S     (*ptr_to_statics)
#define has_S (ptr_to_statics)

static struct statics *get_S(void)
{
	if (!ptr_to_statics) {
		ptr_to_statics = xzalloc(sizeof(S));
		memcpy(&S.db[0], &const_pw_db, sizeof(const_pw_db));
		memcpy(&S.db[1], &const_gr_db, sizeof(const_gr_db));
#if ENABLE_USE_BB_SHADOW
		memcpy(&S.db[2], &const_sp_db, sizeof(const_sp_db));
#endif
	}
	return ptr_to_statics;
}

/**********************************************************************/
/* Internal functions                                                 */
/**********************************************************************/

/* Divide the passwd/group/shadow record in fields
 * by substituting the given delimeter
 * e.g. ':' or ',' with '\0'.
 * Returns the  number of fields found.
 * Strips leading and trailing whitespace in fields.
 */
static int tokenize(char *buffer, int ch)
{
	char *p = buffer;
	char *s = p;
	int num_fields = 0;

	for (;;) {
		if (isblank(*s)) {
			overlapping_strcpy(s, skip_whitespace(s));
		}
		if (*p == ch || *p == '\0') {
			char *end = p;
			while (p != s && isblank(p[-1]))
				p--;
			if (p != end)
				overlapping_strcpy(p, end);
			num_fields++;
			if (*end == '\0') {
				S.tokenize_end = p + 1;
				return num_fields;
			}
			*p = '\0';
			s = p + 1;
		}
		p++;
	}
}

/* Returns !NULL on success and matching line broken up in fields by '\0' in buf.
 * We require the expected number of fields to be found.
 */
static char *parse_common(FILE *fp, const char *filename,
		int n_fields,
		const char *key, int field_pos)
{
	int count = 0;
	char *buf;

	while ((buf = xmalloc_fgetline(fp)) != NULL) {
		count++;
		/* Skip empty lines, comment lines */
		if (buf[0] == '\0' || buf[0] == '#')
			goto free_and_next;
		if (tokenize(buf, ':') != n_fields) {
			/* number of fields is wrong */
			bb_error_msg("bad record at %s:%u", filename, count);
			goto free_and_next;
		}

/* Ugly hack: group db requires aqdditional buffer space
 * for members[] array. If there is only one group, we need space
 * for 3 pointers: alignment padding, group name, NULL.
 * +1 for every additional group.
 */
		if (n_fields == sizeof(GR_DEF)-1) { /* if we read group file */
			int resize = 3;
			char *p = buf;
			while (*p)
				if (*p++ == ',')
					resize++;
			resize *= sizeof(char**);
			resize += S.tokenize_end - buf;
			buf = xrealloc(buf, resize);
		}

		if (!key) {
			/* no key specified: sequential read, return a record */
			break;
		}
		if (strcmp(key, nth_string(buf, field_pos)) == 0) {
			/* record found */
			break;
		}
 free_and_next:
		free(buf);
	}

	return buf;
}

static char *parse_file(const char *filename,
		int n_fields,
		const char *key, int field_pos)
{
	char *buf = NULL;
	FILE *fp = fopen_for_read(filename);

	if (fp) {
		buf = parse_common(fp, filename, n_fields, key, field_pos);
		fclose(fp);
	}
	return buf;
}

/* Convert passwd/group/shadow file record in buffer to a struct */
static void *convert_to_struct(const char *def,	const unsigned char *off,
		char *buffer, void *result)
{
	for (;;) {
		void *member = (char*)result + (*off++);

		if ((*def | 0x20) == 's') { /* s or S */
			*(char **)member = (char*)buffer;
			if (!buffer[0] && (*def == 'S')) {
				errno = EINVAL;
			}
		}
		if (*def == 'I') {
			*(int *)member = bb_strtou(buffer, NULL, 10);
		}
#if ENABLE_USE_BB_SHADOW
		if (*def == 'l') {
			long n = -1;
			if (buffer[0])
				n = bb_strtol(buffer, NULL, 10);
			*(long *)member = n;
		}
#endif
		if (*def == 'm') {
			char **members;
			int i = tokenize(buffer, ',');

			/* Store members[] after buffer's end.
			 * This is safe ONLY because there is a hack
			 * in parse_common() which allocates additional space
			 * at the end of malloced buffer!
			 */
			members = (char **)
				( ((intptr_t)S.tokenize_end + sizeof(char**))
				& -(intptr_t)sizeof(char**)
				);

			((struct group *)result)->gr_mem = members;
			while (--i >= 0) {
				*members++ = buffer;
				buffer += strlen(buffer) + 1;
			}
			*members = NULL;
		}
		/* def "r" does nothing */

		def++;
		if (*def == '\0')
			break;
		buffer += strlen(buffer) + 1;
	}

	if (errno)
		result = NULL;
	return result;
}

/****** getXXnam/id_r */

static int getXXnam_r(const char *name, uintptr_t db_and_field_pos, char *buffer, size_t buflen,
		void *result)
{
	void *struct_buf = *(void**)result;
	char *buf;
	struct passdb *db;
	get_S();
	db = &S.db[db_and_field_pos >> 2];

	*(void**)result = NULL;
	buf = parse_file(db->filename, db->numfields, name, db_and_field_pos & 3);
	if (buf) {
		size_t size = S.tokenize_end - buf;
		if (size > buflen) {
			errno = ERANGE;
		} else {
			memcpy(buffer, buf, size);
			*(void**)result = convert_to_struct(db->def, db->off, buffer, struct_buf);
		}
		free(buf);
	}
	/* "The reentrant functions return zero on success.
	 * In case of error, an error number is returned."
	 * NB: not finding the record is also a "success" here:
	 */
	return errno;
}

int getpwnam_r(const char *name, struct passwd *struct_buf, char *buffer, size_t buflen,
				struct passwd **result)
{
	/* Why the "store buffer address in result" trick?
	 * This way, getXXnam_r has the same ABI signature as getpwnam_r,
	 * hopefully compiler can optimize tall call better in this case.
	 */
	*result = struct_buf;
	return getXXnam_r(name, (0 << 2) + 0, buffer, buflen, result);
}
#if ENABLE_USE_BB_SHADOW
int getspnam_r(const char *name, struct spwd *struct_buf, char *buffer, size_t buflen,
			   struct spwd **result)
{
	*result = struct_buf;
	return getXXnam_r(name, (2 << 2) + 0, buffer, buflen, result);
}
#endif

/****** getXXent_r */

static int getXXent_r(void *struct_buf, char *buffer, size_t buflen,
		void *result,
		unsigned db_idx)
{
	char *buf;
	struct passdb *db;
	get_S();
	db = &S.db[db_idx];

	*(void**)result = NULL;

	if (!db->fp) {
		db->fp = fopen_for_read(db->filename);
		if (!db->fp) {
			return errno;
		}
		close_on_exec_on(fileno(db->fp));
	}

	buf = parse_common(db->fp, db->filename, db->numfields, /*no search key:*/ NULL, 0);
	if (buf) {
		size_t size = S.tokenize_end - buf;
		if (size > buflen) {
			errno = ERANGE;
		} else {
			memcpy(buffer, buf, size);
			*(void**)result = convert_to_struct(db->def, db->off, buffer, struct_buf);
		}
		free(buf);
	}
	/* "The reentrant functions return zero on success.
	 * In case of error, an error number is returned."
	 * NB: not finding the record is also a "success" here:
	 */
	return errno;
}

int getpwent_r(struct passwd *struct_buf, char *buffer, size_t buflen, struct passwd **result)
{
	return getXXent_r(struct_buf, buffer, buflen, result, 0);
}

/****** getXXnam/id */

static void *getXXnam(const char *name, unsigned db_and_field_pos)
{
	char *buf;
	void *result;
	struct passdb *db;
	get_S();
	db = &S.db[db_and_field_pos >> 2];

	result = NULL;

	if (!db->fp) {
		db->fp = fopen_for_read(db->filename);
		if (!db->fp) {
			return NULL;
		}
		close_on_exec_on(fileno(db->fp));
	}

	free(db->malloced);
	db->malloced = NULL;
	buf = parse_common(db->fp, db->filename, db->numfields, name, db_and_field_pos & 3);
	if (buf) {
		db->malloced = xzalloc(db->size_of);
		result = convert_to_struct(db->def, db->off, buf, db->malloced);
	}
	return result;
}

struct passwd *getpwnam(const char *name)
{
	return getXXnam(name, (0 << 2) + 0);
}
struct group *getgrnam(const char *name)
{
	return getXXnam(name, (1 << 2) + 0);
}
struct passwd *getpwuid(uid_t id)
{
	return getXXnam(utoa(id), (0 << 2) + 2);
}
struct group *getgrgid(gid_t id)
{
	return getXXnam(utoa(id), (1 << 2) + 2);
}

/****** end/setXXend */

void endpwent(void)
{
	if (has_S && S.db[0].fp) {
		fclose(S.db[0].fp);
		S.db[0].fp = NULL;
	}
}
void setpwent(void)
{
	if (has_S && S.db[0].fp) {
		rewind(S.db[0].fp);
	}
}
void endgrent(void)
{
	if (has_S && S.db[1].fp) {
		fclose(S.db[1].fp);
		S.db[1].fp = NULL;
	}
}

/****** initgroups and getgrouplist */

static gid_t* FAST_FUNC getgrouplist_internal(int *ngroups_ptr,
		const char *user, gid_t gid)
{
	FILE *fp;
	gid_t *group_list;
	int ngroups;

	get_S();

	/* We alloc space for 8 gids at a time. */
	group_list = xmalloc(8 * sizeof(group_list[0]));
	group_list[0] = gid;
	ngroups = 1;

	fp = fopen_for_read(_PATH_GROUP);
	if (fp) {
		char *buf;
		while ((buf = parse_common(fp, _PATH_GROUP, sizeof(GR_DEF)-1, NULL, 0)) != NULL) {
			char **m;
			struct group group;
			if (!convert_to_struct(GR_DEF, gr_off, buf, &group))
				goto next;
			if (group.gr_gid == gid)
				goto next;
			for (m = group.gr_mem; *m; m++) {
				if (strcmp(*m, user) != 0)
					continue;
				group_list = xrealloc_vector(group_list, /*8=2^3:*/ 3, ngroups);
				group_list[ngroups++] = group.gr_gid;
				break;
			}
 next:
			free(buf);
		}
		fclose(fp);
	}
	*ngroups_ptr = ngroups;
	return group_list;
}

int initgroups(const char *user, gid_t gid)
{
	int ngroups;
	gid_t *group_list = getgrouplist_internal(&ngroups, user, gid);

	ngroups = setgroups(ngroups, group_list);
	free(group_list);
	return ngroups;
}

int getgrouplist(const char *user, gid_t gid, gid_t *groups, int *ngroups)
{
	int ngroups_old = *ngroups;
	gid_t *group_list = getgrouplist_internal(ngroups, user, gid);

	if (*ngroups <= ngroups_old) {
		ngroups_old = *ngroups;
		memcpy(groups, group_list, ngroups_old * sizeof(groups[0]));
	} else {
		ngroups_old = -1;
	}
	free(group_list);
	return ngroups_old;
}
