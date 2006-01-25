/* libbb_udhcp.h - busybox compatability wrapper */

/* bit of a hack, do this no matter what the order of the includes.
 * (for busybox) */

#ifdef CONFIG_INSTALL_NO_USR
#undef DEFAULT_SCRIPT
#define DEFAULT_SCRIPT  "/share/udhcpc/default.script"
#endif

#ifndef _LIBBB_UDHCP_H
#define _LIBBB_UDHCP_H

#ifdef IN_BUSYBOX
#include "busybox.h"

#ifdef CONFIG_FEATURE_UDHCP_SYSLOG
#define UDHCP_SYSLOG
#endif

#ifdef CONFIG_FEATURE_UDHCP_DEBUG
#define UDHCP_DEBUG
#endif

#define COMBINED_BINARY
#include "version.h"

#define xfopen bb_xfopen

/* make safe the exported namespace */
/* from common.h */
#define background		udhcp_background
#define start_log_and_pid	udhcp_start_log_and_pid
/* from script.h */
#define run_script		udhcp_run_script
/* from packet.h */
#define init_header		udhcp_init_header
#define get_packet		udhcp_get_packet
#define checksum		udhcp_checksum
#define raw_packet		udhcp_raw_packet
#define kernel_packet		udhcp_kernel_packet
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

#else /* ! BB_VER */

#include <stdlib.h>
#include <stdio.h>
#include <sys/sysinfo.h>

#define TRUE			1
#define FALSE			0

#define xmalloc malloc
#define xcalloc calloc

static inline FILE *xfopen(const char *file, const char *mode)
{
	FILE *fp;
	if (!(fp = fopen(file, mode))) {
		perror("could not open input file");
		exit(0);
	}
	return fp;
}

#endif /* BB_VER */

#endif /* _LIBBB_UDHCP_H */
