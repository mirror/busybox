# Makefile for busybox
#
# Copyright (C) 2001 Erik Andersen <andersee@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA



TOPDIR   :=..
L_TARGET := libbb.a

LIBBB_MSRC=messages.c
LIBBB_OBJ= full_version name_too_long omitting_directory not_a_directory \
	memory_exhausted invalid_date invalid_option io_error dash_dash_help \
	write_error too_few_args name_longer_than_foo unknown can_not_create_raw_socket
LIBBB_MOBJS=$(patsubst %,%.o, $(LIBBB_OBJ))

obj-y           :=
obj-n           :=
obj-            :=

obj-y += ask_confirmation.o chomp.o concat_path_file.o copy_file.o \
	copy_file_chunk.o libc5.o device_open.o error_msg.o \
	error_msg_and_die.o fgets_str.o find_mount_point.o find_pid_by_name.o \
	find_root_device.o full_read.o full_write.o get_console.o \
	get_last_path_component.o get_line_from_file.o gz_open.o human_readable.o \
	isdirectory.o kernel_version.o loop.o mode_string.o module_syscalls.o mtab.o \
	mtab_file.o my_getgrnam.o my_getgrgid.o my_getpwnam.o my_getpwnamegid.o \
	my_getpwuid.o parse_mode.o parse_number.o perror_msg.o perror_msg_and_die.o \
	print_file.o process_escape_sequence.o read_package_field.o recursive_action.o \
	safe_read.o safe_strncpy.o syscalls.o syslog_msg_with_name.o time_string.o \
	trim.o unzip.o vdprintf.o verror_msg.o vperror_msg.o wfopen.o xfuncs.o \
	xgetcwd.o xreadlink.o xregcomp.o interface.o remove_file.o last_char_is.o \
	copyfd.o vherror_msg.o herror_msg.o herror_msg_and_die.o xgethostbyname.o \
	dirname.o make_directory.o create_icmp_socket.o u_signal_names.o arith.o \
	simplify_path.o $(LIBBB_MOBJS) $(LIBBB_AROBJS)


# Hand off to toplevel Rules.mak
include $(TOPDIR)/Rules.mak

$(LIBBB_MOBJS): $(LIBBB_MSRC)
	$(CC) $(CFLAGS) -DBB_VER='"$(VERSION)"' -DBB_BT='"$(BUILDTIME)"' \
		$(LIBBB_CFLAGS) -DL_$(patsubst %,%,$*) -c $< -o $*.o

$(LIBBB_AROBJS): $(LIBBB_ARCSRC)
	$(CC) $(CFLAGS) $(LIBBB_CFLAGS) -DL_$(patsubst %,%,$*) -c $< -o $*.o

loop.o: loop.h

loop.h: mk_loop_h.sh
	@ $(SHELL) $< > $@

clean:
	rm -f $(L_TARGET) *.o core


