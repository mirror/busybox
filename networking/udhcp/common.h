/* common.h
 *
 * Russ Dill <Russ.Dill@asu.edu> Soptember 2001
 * Rewrited by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "version.h"
#include "busybox.h"

#ifndef CONFIG_FEATURE_UDHCP_SYSLOG
enum syslog_levels {
	LOG_EMERG = 0,
	LOG_ALERT,
	LOG_CRIT,
	LOG_WARNING,
	LOG_ERR,
	LOG_INFO,
	LOG_DEBUG
};
#else
#include <syslog.h>
#endif

void start_log(const char *client_server);
void background(const char *pidfile);
void udhcp_logging(int level, const char *fmt, ...);

extern int udhcp_signal_pipe[2];
void udhcp_set_signal_pipe(int sig_add);


#define LOG(level, str, args...) udhcp_logging(level, str, ## args)

#ifdef CONFIG_FEATURE_UDHCP_DEBUG
# define DEBUG(level, str, args...) udhcp_logging(level, str, ## args)
#else
# define DEBUG(level, str, args...) do {;} while(0)
#endif
