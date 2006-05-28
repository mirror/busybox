/* libbb_udhcp.h - busybox compatibility wrapper */

/* bit of a hack, do this no matter what the order of the includes.
 * (for busybox) */

#ifndef _LIBBB_UDHCP_H
#define _LIBBB_UDHCP_H

#include "packet.h"
#include "busybox.h"

#ifdef CONFIG_INSTALL_NO_USR
# define DEFAULT_SCRIPT  "/share/udhcpc/default.script"
#else
# define DEFAULT_SCRIPT  "/usr/share/udhcpc/default.script"
#endif



#define COMBINED_BINARY

#define xfopen bb_xfopen

void udhcp_background(const char *pidfile);
void udhcp_start_log_and_pid(const char *client_server, const char *pidfile);
void udhcp_logging(int level, const char *fmt, ...);

void udhcp_run_script(struct dhcpMessage *packet, const char *name);

// Still need to clean these up...

/* from pidfile.h */
#define pidfile_acquire		udhcp_pidfile_acquire
#define pidfile_write_release	udhcp_pidfile_write_release
/* from options.h */
#define get_option		udhcp_get_option
#define end_option		udhcp_end_option
#define add_option_string	udhcp_add_option_string
#define add_simple_option	udhcp_add_simple_option
#define option_lengths		udhcp_option_lengths
/* from socket.h */
#define listen_socket		udhcp_listen_socket
#define read_interface		udhcp_read_interface
/* from dhcpc.h */
#define client_config		udhcp_client_config
/* from dhcpd.h */
#define server_config		udhcp_server_config

#endif /* _LIBBB_UDHCP_H */
