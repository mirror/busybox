/*
 * libbb/selinux_common.c
 *   -- common SELinux utility functions
 * 
 * Copyright 2007 KaiGai Kohei <kaigai@kaigai.gr.jp>
 */
#include "busybox.h"
#include <selinux/context.h>

context_t set_security_context_component(security_context_t cur_context,
					 char *user, char *role, char *type, char *range)
{
	context_t con = context_new(cur_context);
	if (!con)
		return NULL;

	if (user && context_user_set(con, user))
		goto error;
	if (type && context_type_set(con, type))
		goto error;
	if (range && context_range_set(con, range))
		goto error;
	if (role && context_role_set(con, role))
		goto error;
	return con;

error:
	context_free(con);
	return NULL;
}
