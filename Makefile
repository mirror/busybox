# Makefile for busybox
#
# Copyright (C) 1999,2000,2001 Erik Andersen <andersee@debian.org>
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
#

PROG      := busybox
VERSION   := 0.51
BUILDTIME := $(shell TZ=UTC date -u "+%Y.%m.%d-%H:%M%z")
export VERSION

# With a modern GNU make(1) (highly recommended, that's what all the
# developers use), all of the following configuration values can be
# overridden at the command line.  For example:
#   make CROSS=powerpc-linux- BB_SRC_DIR=$HOME/busybox PREFIX=/mnt/app

# If you want to add some simple compiler switches (like -march=i686),
# especially from the command line, use this instead of CFLAGS directly.
# For optimization overrides, it's better still to set OPTIMIZATION.
CFLAGS_EXTRA =
 
# If you want a static binary, turn this on.
DOSTATIC = false

# Set the following to `true' to make a debuggable build.
# Leave this set to `false' for production use.
# eg: `make DODEBUG=true tests'
# Do not enable this for production builds...
DODEBUG = false

# Setting this to `true' will cause busybox to directly use the system's
# password and group functions.  Assuming you use GNU libc, when this is
# `true', you will need to install the /etc/nsswitch.conf configuration file
# and the required libnss_* libraries. This generally makes your embedded
# system quite a bit larger... If you leave this off, busybox will directly
# use the /etc/password, /etc/group files (and your system will be smaller, and
# I will get fewer emails asking about how glibc NSS works).  Enabling this adds
# just 1.4k to the binary size (which is a _lot_ less then glibc NSS costs),
# Most people will want to leave this set to false.
USE_SYSTEM_PWD_GRP = true

# This enables compiling with dmalloc ( http://dmalloc.com/ )
# which is an excellent public domain mem leak and malloc problem
# detector.  To enable dmalloc, before running busybox you will
# want to first set up your environment.
# eg: `export DMALLOC_OPTIONS=debug=0x14f47d83,inter=100,log=logfile`
# Do not enable this for production builds...
DODMALLOC = false

# Electric-fence is another very useful malloc debugging library.
# Do not enable this for production builds...
DOEFENCE  = false

# If you want large file summit support, turn this on.
# This has no effect if you don't have a kernel with lfs
# support, and a system with libc-2.1.3 or later.
# Some of the programs that can benefit from lfs support
# are dd, gzip, mount, tar, and mkfs_minix.
# LFS allows you to use the above programs for files
# larger than 2GB!
DOLFS = false

# If you have a "pristine" source directory, point BB_SRC_DIR to it.
# Experimental and incomplete; tell the mailing list
# <busybox@opensource.lineo.com> if you do or don't like it so far.
BB_SRC_DIR =

# If you are running a cross compiler, you may want to set this
# to something more interesting, like "powerpc-linux-".
CROSS =
CC = $(CROSS)gcc
AR = $(CROSS)ar
STRIPTOOL = $(CROSS)strip

# To compile vs uClibc, just use the compiler wrapper built by uClibc...
# This make things very easy?  Everything should compile and work as
# expected these days...
#CC = ../uClibc/extra/gcc-uClibc/i386-uclibc-gcc

# To compile vs some other alternative libc, you may need to use/adjust
# the following lines to meet your needs...
#
# If you are using Red Hat 6.x with the compatible RPMs (for developing under
# Red Hat 5.x and glibc 2.0) uncomment the following.  Be sure to read about
# using the compatible RPMs (compat-*) at http://www.redhat.com !
#LIBCDIR=/usr/i386-glibc20-linux
#
# The following is used for libc5 (if you install altgcc and libc5-altdev
# on a Debian system).  
#LIBCDIR=/usr/i486-linuxlibc1
#
# For other libraries, you are on your own...
#LDFLAGS+=-nostdlib
#LIBRARIES = $(LIBCDIR)/lib/libc.a -lgcc
#CROSS_CFLAGS+=-nostdinc -I$(LIBCDIR)/include -I$(GCCINCDIR)
#GCCINCDIR = $(shell gcc -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp")

# use '-Os' optimization if available, else use -O2
OPTIMIZATION := $(shell if $(CC) -Os -S -o /dev/null -xc /dev/null >/dev/null 2>&1; \
    then echo "-Os"; else echo "-O2" ; fi)

WARNINGS = -Wall -Wshadow

ARFLAGS = -r

#
#--------------------------------------------------------
# If you're going to do a lot of builds with a non-vanilla configuration,
# it makes sense to adjust parameters above, so you can type "make"
# by itself, instead of following it by the same half-dozen overrides
# every time.  The stuff below, on the other hand, is probably less
# prone to casual user adjustment.
# 

ifeq ($(strip $(DOLFS)),true)
    # For large file summit support
    CFLAGS+=-D_FILE_OFFSET_BITS=64
endif
ifeq ($(strip $(DODMALLOC)),true)
    # For testing mem leaks with dmalloc
    CFLAGS+=-DDMALLOC
    LIBRARIES = -ldmalloc
    # Force debug=true, since this is useless when not debugging...
    DODEBUG = true
else
    ifeq ($(strip $(DOEFENCE)),true)
	LIBRARIES = -lefence
	# Force debug=true, since this is useless when not debugging...
	DODEBUG = true
    endif
endif
ifeq ($(strip $(DODEBUG)),true)
    CFLAGS  += $(WARNINGS) -g -D_GNU_SOURCE
    LDFLAGS += -Wl,-warn-common
    STRIP    =
else
    CFLAGS  += $(WARNINGS) $(OPTIMIZATION) -fomit-frame-pointer -D_GNU_SOURCE
    LDFLAGS += -s -Wl,-warn-common
    STRIP    = $(STRIPTOOL) --remove-section=.note --remove-section=.comment $(PROG)
endif
ifeq ($(strip $(DOSTATIC)),true)
    LDFLAGS += --static
    #
    #use '-ffunction-sections -fdata-sections' and '--gc-sections' (if they 
    # work) to try and strip out any unused junk.  Doesn't do much for me, 
    # but you may want to give it a shot...
    #
    #ifeq ($(shell $(CC) -ffunction-sections -fdata-sections -S \
    #	-o /dev/null -xc /dev/null 2>/dev/null && $(LD) \
    #			--gc-sections -v >/dev/null && echo 1),1)
    #	CFLAGS += -ffunction-sections -fdata-sections
    #	LDFLAGS += --gc-sections
    #endif
endif

ifndef $(PREFIX)
    PREFIX = `pwd`/_install
endif

# Additional complications due to support for pristine source dir.
# Include files in the build directory should take precedence over
# the copy in BB_SRC_DIR, both during the compilation phase and the
# shell script that finds the list of object files.
# Work in progress by <ldoolitt@recycle.lbl.gov>.
#
ifneq ($(strip $(BB_SRC_DIR)),)
    VPATH = $(BB_SRC_DIR)
endif
#ifneq ($(strip $(VPATH)),)
#    CFLAGS += -I- -I. $(patsubst %,-I%,$(subst :, ,$(VPATH)))
#endif

# We need to set APPLET_SOURCES to something like
#   $(shell busybox.sh Config.h)
# but in a manner that works with VPATH and BB_SRC_DIR.
# Possible ways to approach this:
#
#   1. Explicitly search through .:$(VPATH) for busybox.sh and config.h,
#      then $(shell $(BUSYBOX_SH) $(CONFIG_H) $(BB_SRC_DIR))
#
#   2. Explicity search through .:$(VPATH) for slist.mk,
#      then $(shell $(MAKE) -f $(SLIST_MK) VPATH=$(VPATH) BB_SRC_DIR=$(BB_SRC_DIR))
#
#   3. Create slist.mk in this directory, with commands embedded in
#      a $(shell ...) command, and $(MAKE) it immediately.
#
#   4. Use a real rule within this makefile to create a file that sets 
#      APPLET_SOURCE_LIST, then include that file.  Has complications
#      with the first trip through the makefile (before processing the
#      include) trying to do too much, and a spurious warning the first
#      time make is run.
#
# This is option 3:
#
#APPLET_SOURCES = $(shell \
#   echo -e 'all: busybox.sh Config.h\n\t@ $$(SHELL) $$^ $$(BB_SRC_DIR)' >slist.mk; \
#   make -f slist.mk VPATH=$(VPATH) BB_SRC_DIR=$(BB_SRC_DIR) \
#)
# And option 4:
-include applet_source_list

OBJECTS   = $(APPLET_SOURCES:.c=.o) busybox.o messages.o usage.o applets.o
CFLAGS    += $(CROSS_CFLAGS)
CFLAGS    += -DBB_VER='"$(VERSION)"'
CFLAGS    += -DBB_BT='"$(BUILDTIME)"'
ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif

ifneq ($(strip $(USE_SYSTEM_PWD_GRP)),true)
    PWD_GRP	= pwd_grp
    PWD_GRP_DIR = $(BB_SRC_DIR:=/)$(PWD_GRP)
    PWD_LIB     = libpwd.a
    PWD_CSRC=__getpwent.c pwent.c getpwnam.c getpwuid.c putpwent.c getpw.c \
	    fgetpwent.c __getgrent.c grent.c getgrnam.c getgrgid.c fgetgrent.c \
	    initgroups.c setgroups.c
    PWD_OBJS=$(patsubst %.c,$(PWD_GRP)/%.o, $(PWD_CSRC))
    PWD_CFLAGS = -I$(PWD_GRP_DIR)
else
    CFLAGS    += -DUSE_SYSTEM_PWD_GRP
endif
    
LIBBB	  = libbb
LIBBB_LIB = libbb.a
LIBBB_CSRC= ask_confirmation.c check_wildcard_match.c chomp.c \
concat_path_file.c copy_file.c copy_file_chunk.c create_path.c \
daemon.c deb_extract.c device_open.c error_msg.c error_msg_and_die.c \
find_mount_point.c find_pid_by_name.c find_root_device.c full_read.c \
full_write.c get_ar_headers.c get_console.c get_last_path_component.c \
get_line_from_file.c gz_open.c human_readable.c inode_hash.c isdirectory.c \
kernel_version.c loop.c mode_string.c module_syscalls.c mtab.c mtab_file.c \
my_getgrnam.c my_getgrgid.c my_getpwnam.c my_getpwnamegid.c my_getpwuid.c \
parse_mode.c parse_number.c perror_msg.c perror_msg_and_die.c print_file.c \
process_escape_sequence.c recursive_action.c safe_read.c safe_strncpy.c \
seek_ared_file.c syscalls.c syslog_msg_with_name.c time_string.c trim.c \
untar.c unzip.c vdprintf.c verror_msg.c vperror_msg.c wfopen.c xfuncs.c \
xgetcwd.c xregcomp.c

LIBBB_OBJS=$(patsubst %.c,$(LIBBB)/%.o, $(LIBBB_CSRC))
LIBBB_CFLAGS = -I$(LIBBB)
ifneq ($(strip $(BB_SRC_DIR)),)
    LIBBB_CFLAGS += -I$(BB_SRC_DIR)/$(LIBBB)
endif


# Put user-supplied flags at the end, where they
# have a chance of winning.
CFLAGS += $(CFLAGS_EXTRA)

.EXPORT_ALL_VARIABLES:

all: applet_source_list busybox busybox.links doc

applet_source_list: busybox.sh Config.h
	(echo -n "APPLET_SOURCES := "; $(SHELL) $^ $(BB_SRC_DIR)) > $@

doc: olddoc

# Old Docs...
olddoc: docs/busybox.pod docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html

docs/busybox.pod : docs/busybox_header.pod usage.h docs/busybox_footer.pod
	- ( cat docs/busybox_header.pod; \
	    docs/autodocifier.pl usage.h; \
	    cat docs/busybox_footer.pod ) > docs/busybox.pod

docs/BusyBox.txt: docs/busybox.pod
	@echo
	@echo BusyBox Documentation
	@echo
	-mkdir -p docs
	-pod2text $< > $@

docs/BusyBox.1: docs/busybox.pod
	- mkdir -p docs
	- pod2man --center=BusyBox --release="version $(VERSION)" \
		$< > $@

docs/BusyBox.html: docs/busybox.lineo.com/BusyBox.html
	- mkdir -p docs
	-@ rm -f docs/BusyBox.html
	-@ ln -s busybox.lineo.com/BusyBox.html docs/BusyBox.html

docs/busybox.lineo.com/BusyBox.html: docs/busybox.pod
	-@ mkdir -p docs/busybox.lineo.com
	-  pod2html --noindex $< > \
	    docs/busybox.lineo.com/BusyBox.html
	-@ rm -f pod2html*


# New docs based on DOCBOOK SGML
newdoc: docs/busybox.txt docs/busybox.pdf docs/busybox/busyboxdocumentation.html

docs/busybox.txt: docs/busybox.sgml
	@echo
	@echo BusyBox Documentation
	@echo
	- mkdir -p docs
	(cd docs; sgmltools -b txt busybox.sgml)

docs/busybox.dvi: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b dvi busybox.sgml)

docs/busybox.ps: docs/busybox.sgml
	- mkdir -p docs
	(cd docs; sgmltools -b ps busybox.sgml)

docs/busybox.pdf: docs/busybox.ps
	- mkdir -p docs
	(cd docs; ps2pdf busybox.ps)

docs/busybox/busyboxdocumentation.html: docs/busybox.sgml
	- mkdir -p docs
	(cd docs/busybox.lineo.com; sgmltools -b html ../busybox.sgml)


busybox: $(PWD_LIB) $(LIBBB_LIB) $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(PWD_LIB) $(LIBBB_LIB) $(LIBRARIES)
	$(STRIP)

# Without VPATH, rule expands to "/bin/sh busybox.mkll Config.h applets.h"
# but with VPATH, some or all of those file names are resolved to the
# directories in which they live.
busybox.links: busybox.mkll Config.h applets.h
	- $(SHELL) $^ >$@

nfsmount.o cmdedit.o: %.o: %.h
sh.o: cmdedit.h
$(OBJECTS): %.o: %.c Config.h busybox.h applets.h Makefile
	$(CC) -I- $(CFLAGS) -I. $(patsubst %,-I%,$(subst :, ,$(BB_SRC_DIR))) -c $< -o $*.o

$(PWD_OBJS): %.o: %.c Config.h busybox.h applets.h Makefile
	- mkdir -p $(PWD_GRP)
	$(CC) $(CFLAGS) $(PWD_CFLAGS) -c $< -o $*.o

$(LIBBB_OBJS): %.o: %.c Config.h busybox.h applets.h Makefile libbb/libbb.h
	- mkdir -p $(LIBBB)
	$(CC) $(CFLAGS) $(LIBBB_CFLAGS) -c $< -o $*.o

libpwd.a: $(PWD_OBJS)
	$(AR) $(ARFLAGS) $@ $^

libbb.a: $(LIBBB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

usage.o: usage.h

libbb/loop.o: libbb/loop.h

libbb/loop.h: mk_loop_h.sh
	@ $(SHELL) $< > $@

test tests:
	# old way of doing it
	#cd tests && $(MAKE) all
	# new way of doing it
	cd tests && ./tester.sh

clean:
	- cd tests && $(MAKE) clean
	- rm -f docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.lineo.com/BusyBox.html
	- rm -f docs/busybox.txt docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pdf docs/busybox.lineo.com/busybox.html
	- rm -f multibuild.log Config.h.orig
	- rm -rf docs/busybox _install libpwd.a libbb.a
	- rm -f busybox.links libbb/loop.h *~ slist.mk core applet_source_list
	- find -name \*.o -exec rm -f {} \;

distclean: clean
	- rm -f busybox
	- cd tests && $(MAKE) distclean

install: install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX)

install-hardlinks: install.sh busybox busybox.links
	$(SHELL) $< $(PREFIX) --hardlinks

debug_pristine:
	@ echo VPATH=\"$(VPATH)\"
	@ echo OBJECTS=\"$(OBJECTS)\"

dist release: distclean doc
	cd ..;					\
	rm -rf busybox-$(VERSION);		\
	cp -a busybox busybox-$(VERSION);	\
						\
	find busybox-$(VERSION)/ -type d	\
				 -name CVS	\
				 -print		\
		-exec rm -rf {} \; ;            \
						\
	find busybox-$(VERSION)/ -type f	\
				 -name .cvsignore \
				 -print		\
		-exec rm -f {}  \; ;            \
						\
	find busybox-$(VERSION)/ -type f	\
				 -name .\#*	\
				 -print		\
		-exec rm -f {} \;  ;            \
						\
	tar -cvzf busybox-$(VERSION).tar.gz busybox-$(VERSION)/;

.PHONY: tags
tags:
	ctags -R .
