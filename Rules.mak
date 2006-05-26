# Rules.make for busybox
#
# Copyright (C) 1999-2005 by Erik Andersen <andersen@codepoet.org>
#
# Licensed under GPLv2, see the file LICENSE in this tarball for details.
#

# Pull in the user's busybox configuration
ifeq ($(filter $(noconfig_targets),$(MAKECMDGOALS)),)
-include $(top_builddir)/.config
endif

#--------------------------------------------------------
PROG      := busybox
MAJOR_VERSION   :=1
MINOR_VERSION   :=2
SUBLEVEL_VERSION:=0
EXTRAVERSION    :=-pre0
VERSION   :=$(MAJOR_VERSION).$(MINOR_VERSION).$(SUBLEVEL_VERSION)$(EXTRAVERSION)
BUILDTIME := $(shell TZ=UTC date -u "+%Y.%m.%d-%H:%M%z")


#--------------------------------------------------------
# With a modern GNU make(1) (highly recommended, that's what all the
# developers use), all of the following configuration values can be
# overridden at the command line.  For example:
#   make CROSS=powerpc-linux- top_srcdir="$HOME/busybox" PREFIX=/mnt/app
#--------------------------------------------------------

# If you are running a cross compiler, you will want to set 'CROSS'
# to something more interesting...  Target architecture is determined
# by asking the CC compiler what arch it compiles things for, so unless
# your compiler is broken, you should not need to specify TARGET_ARCH
CROSS          =$(strip $(subst ",, $(strip $(CROSS_COMPILER_PREFIX))))
# be gentle to vi coloring.. "))
CC             = $(CROSS)gcc
AR             = $(CROSS)ar
AS             = $(CROSS)as
LD             = $(CROSS)ld
NM             = $(CROSS)nm
STRIP          = $(CROSS)strip
ELF2FLT        = $(CROSS)elf2flt
CPP            = $(CC) -E
SED           ?= sed
BZIP2         ?= bzip2


# What OS are you compiling busybox for?  This allows you to include
# OS specific things, syscall overrides, etc.
TARGET_OS=linux

# Ensure consistent sort order, 'gcc -print-search-dirs' behavior, etc.
LC_ALL:= C

# If you want to add some simple compiler switches (like -march=i686),
# especially from the command line, use this instead of CFLAGS directly.
# For optimization overrides, it's better still to set OPTIMIZATION.
CFLAGS_EXTRA=$(subst ",, $(strip $(EXTRA_CFLAGS_OPTIONS)))

# To compile vs some other alternative libc, you may need to use/adjust
# the following lines to meet your needs...
#
# If you are using Red Hat 6.x with the compatible RPMs (for developing under
# Red Hat 5.x and glibc 2.0) uncomment the following.  Be sure to read about
# using the compatible RPMs (compat-*) at http://www.redhat.com !
#LIBCDIR:=/usr/i386-glibc20-linux
#
# For other libraries, you are on your own.  But these may (or may not) help...
#LDFLAGS+=-nostdlib
#LIBRARIES:=$(LIBCDIR)/lib/libc.a -lgcc
#CROSS_CFLAGS+=-nostdinc -I$(LIBCDIR)/include -I$(GCCINCDIR) -funsigned-char
#GCCINCDIR:=$(shell gcc -print-search-dirs | $(SED) -ne "s/install: \(.*\)/\1include/gp")

# This must bind late because srcdir is reset for every source subdirectory.
INCS:=-I$(top_builddir)/include -I$(top_srcdir)/include
CFLAGS=$(INCS) -I$(srcdir) -D_GNU_SOURCE
CFLAGS+=$(CHECKED_CFLAGS)
ARFLAGS=cru

# gcc centric. Perhaps fiddle with findstring gcc,$(CC) for the rest
# get the CC MAJOR/MINOR version
CC_MAJOR:=$(shell printf "%02d" $(shell echo __GNUC__ | $(CC) -E -xc - | tail -n 1))
CC_MINOR:=$(shell printf "%02d" $(shell echo __GNUC_MINOR__ | $(CC) -E -xc - | tail -n 1))

#--------------------------------------------------------
export VERSION BUILDTIME HOSTCC HOSTCFLAGS CROSS CC AR AS LD NM STRIP CPP
ifeq ($(strip $(TARGET_ARCH)),)
TARGET_ARCH:=$(shell $(CC) -dumpmachine | $(SED) -e s'/-.*//' \
		-e 's/i.86/i386/' \
		-e 's/sparc.*/sparc/' \
		-e 's/arm.*/arm/g' \
		-e 's/m68k.*/m68k/' \
		-e 's/ppc/powerpc/g' \
		-e 's/v850.*/v850/g' \
		-e 's/sh[234]/sh/' \
		-e 's/mips-.*/mips/' \
		-e 's/mipsel-.*/mipsel/' \
		-e 's/cris.*/cris/' \
		)
endif

# A nifty macro to make testing gcc features easier, but note that everything
# that uses this _must_ use := or it will be re-evaluated everytime it is
# referenced.
ifeq ($(strip $(V)),2)
VERBOSE_CHECK_CC=echo CC=\"$(1)\" check_cc $(2) >&2;
endif
check_cc=$(shell \
	$(VERBOSE_CHECK_CC) \
	if [ "x$(1)" != "x" ] && [ "x$(2)" != "x" ]; then \
		echo "int i;" > ./conftest.c; \
		if $(1) $(2) -c -o conftest.o conftest.c > /dev/null 2>&1; \
		then echo "$(2)"; else echo "$(3)"; fi ; \
		rm -f conftest.c conftest.o; \
	fi)

# A not very robust macro to check for available ld flags
ifeq ($(strip $(V)),2)
VERBOSE_CHECK_LD=echo LD=\"$(1)\" check_ld $(2) >&2;
endif
check_ld=$(shell \
	$(VERBOSE_CHECK_LD) \
	if [ "x$(1)" != "x" ] && [ "x$(2)" != "x" ]; then \
		$(1) -o /dev/null -b binary /dev/null > /dev/null 2>&1 && \
		echo "-Wl,$(2)" ; \
	fi)

# A not very robust macro to check for available strip flags
ifeq ($(strip $(V)),2)
VERBOSE_CHECK_STRIP=echo STRIPCMD=\"$(1)\" check_strip $(2) >&2;
endif
check_strip=$(shell \
	$(VERBOSE_CHECK_STRIP) \
	if [ "x$(1)" != "x" ] && [ "x$(2)" != "x" ]; then \
		echo "int i;" > ./conftest.c ; \
		$(CC) -c -o conftest.o conftest.c > /dev/null 2>&1 ; \
		$(1) $(2) conftest.o > /dev/null 2>&1 && \
		echo "$(1) $(2)" || echo "$(3)"; \
		rm -f conftest.c conftest.o > /dev/null 2>&1 ; \
	fi)



# Select the compiler needed to build binaries for your development system
HOSTCC     = gcc
HOSTCFLAGS:=$(call check_cc,$(HOSTCC),-Wall,)
HOSTCFLAGS+=$(call check_cc,$(HOSTCC),-Wstrict-prototypes,)
HOSTCFLAGS+=$(call check_cc,$(HOSTCC),-O2,)
HOSTCFLAGS+=$(call check_cc,$(HOSTCC),-fomit-frame-pointer,)

LD_WHOLE_ARCHIVE:=$(shell echo "int i;" > conftest.c ; \
	$(CC) -c -o conftest.o conftest.c ; \
	echo "int main(void){return 0;}" > conftest_main.c ; \
	$(CC) -c -o conftest_main.o conftest_main.c ; \
	$(AR) $(ARFLAGS) conftest.a conftest.o ; \
	$(CC) -Wl,--whole-archive conftest.a -Wl,--no-whole-archive \
	 conftest_main.o -o conftest > /dev/null 2>&1 \
	 && echo "-Wl,--whole-archive" ; \
	rm conftest_main.o conftest_main.c conftest.o conftest.c \
	 conftest.a conftest > /dev/null 2>&1 ; )
ifneq ($(findstring whole-archive,$(LD_WHOLE_ARCHIVE)),)
LD_NO_WHOLE_ARCHIVE:= -Wl,--no-whole-archive
endif

LD_START_GROUP:=$(shell echo "int bar(void){return 0;}" > conftest.c ; \
	$(CC) -c -o conftest.o conftest.c ; \
	echo "int main(void){return bar();}" > conftest_main.c ; \
	$(CC) -c -o conftest_main.o conftest_main.c ; \
	$(AR) $(ARFLAGS) conftest.a conftest.o ; \
	$(CC) -Wl,--start-group conftest.a conftest_main.o -Wl,--end-group \
	 -o conftest > /dev/null 2>&1 && echo "-Wl,--start-group" ; \
	echo rm conftest_main.o conftest_main.c conftest.o conftest.c \
	 conftest.a conftest > /dev/null 2>&1 ; )
ifneq ($(findstring start-group,$(LD_START_GROUP)),)
LD_END_GROUP:= -Wl,--end-group
endif

CHECKED_LDFLAGS := $(call check_ld,$(LD),--warn-common,)

# Pin CHECKED_CFLAGS with := so it's only evaluated once.
CHECKED_CFLAGS:=$(call check_cc,$(CC),-Wall,)
CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wstrict-prototypes,)
CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wshadow,)
CHECKED_CFLAGS+=$(call check_cc,$(CC),-funsigned-char,)
CHECKED_CFLAGS+=$(call check_cc,$(CC),-mmax-stack-frame=256,)
CHECKED_CFLAGS+=$(call check_cc,$(CC),-fno-builtin-strlen)

# Preemptively pin this too.
PROG_CFLAGS:=


#--------------------------------------------------------
# Arch specific compiler optimization stuff should go here.
# Unless you want to override the defaults, do not set anything
# for OPTIMIZATION...

# use '-Os' optimization if available, else use -O2
OPTIMIZATION:=$(call check_cc,$(CC),-Os,-O2)

ifeq ($(CONFIG_BUILD_AT_ONCE),y)
# gcc 2.95 exits with 0 for "unrecognized option"
ifeq ($(strip $(shell [ $(CC_MAJOR) -ge 3 ] ; echo $$?)),0)
	CFLAGS_COMBINE:=$(call check_cc,$(CC),--combine,)
endif
OPTIMIZATION+=$(call check_cc,$(CC),-funit-at-a-time,)
OPTIMIZATION+=$(call check_cc,$(CC),-fgcse-after-reload,)
ifneq ($(CONFIG_BUILD_LIBBUSYBOX),y)
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=25795
# This prevents us from using -fwhole-program when we build the lib
PROG_CFLAGS+=$(call check_cc,$(CC),-fwhole-program,)
endif # CONFIG_BUILD_LIBBUSYBOX
endif # CONFIG_BUILD_AT_ONCE

LIB_LDFLAGS:=$(call check_ld,$(LD),--enable-new-dtags,)
#LIB_LDFLAGS+=$(call check_ld,$(LD),--reduce-memory-overheads,)
#LIB_LDFLAGS+=$(call check_ld,$(LD),--as-needed,)
#LIB_LDFLAGS+=$(call check_ld,$(LD),--warn-shared-textrel,)


# Some nice architecture specific optimizations
ifeq ($(strip $(TARGET_ARCH)),arm)
	OPTIMIZATION+=-fstrict-aliasing
endif
ifeq ($(strip $(TARGET_ARCH)),i386)
	OPTIMIZATION+=$(call check_cc,$(CC),-march=i386,)
# gcc-4.0 and older seem to benefit from these
#ifneq ($(strip $(shell [ $(CC_MAJOR) -ge 4 -a $(CC_MINOR) -ge 1 ] ; echo $$?)),0)
	OPTIMIZATION+=$(call check_cc,$(CC),-mpreferred-stack-boundary=2,)
	OPTIMIZATION+=$(call check_cc,$(CC),-falign-functions=1 -falign-jumps=1 -falign-loops=1,\
		-malign-functions=0 -malign-jumps=0 -malign-loops=0)
#endif # gcc-4.0 and older

# gcc-4.1 and beyond seem to benefit from these
ifeq ($(strip $(shell [ $(CC_MAJOR) -ge 4 -a $(CC_MINOR) -ge 1 ] ; echo $$?)),0)
	# turn off flags which hurt -Os
	OPTIMIZATION+=$(call check_cc,$(CC),-fno-tree-loop-optimize,)
	OPTIMIZATION+=$(call check_cc,$(CC),-fno-tree-dominator-opts,)
	OPTIMIZATION+=$(call check_cc,$(CC),-fno-strength-reduce,)

	OPTIMIZATION+=$(call check_cc,$(CC),-fno-branch-count-reg,)
endif # gcc-4.1 and beyond
endif
OPTIMIZATION+=$(call check_cc,$(CC),-fomit-frame-pointer,)

#
#--------------------------------------------------------
# If you're going to do a lot of builds with a non-vanilla configuration,
# it makes sense to adjust parameters above, so you can type "make"
# by itself, instead of following it by the same half-dozen overrides
# every time.  The stuff below, on the other hand, is probably less
# prone to casual user adjustment.
#

ifeq ($(strip $(CONFIG_LFS)),y)
    # For large file summit support
    CFLAGS+=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
endif
ifeq ($(strip $(CONFIG_DMALLOC)),y)
    # For testing mem leaks with dmalloc
    CFLAGS+=-DDMALLOC
    LIBRARIES:=-ldmalloc
else
    ifeq ($(strip $(CONFIG_EFENCE)),y)
	LIBRARIES:=-lefence
    endif
endif

# Debugging info

ifeq ($(strip $(CONFIG_DEBUG)),y)
    CFLAGS +=-g
else
    CFLAGS +=-DNDEBUG
    CHECKED_LDFLAGS += $(call check_ld,$(LD),--sort-common,)
endif

ifneq ($(strip $(CONFIG_DEBUG_PESSIMIZE)),y)
    CFLAGS += $(OPTIMIZATION)
endif

# warn a bit more verbosely for non-release versions
ifneq ($(EXTRAVERSION),)
    CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wstrict-prototypes,)
    CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wmissing-prototypes,)
    CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wmissing-declarations,)
    CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wunused,)
    CHECKED_CFLAGS+=$(call check_cc,$(CC),-Winit-self,)
    CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wshadow,)
    CHECKED_CFLAGS+=$(call check_cc,$(CC),-Wcast-align,)
endif
STRIPCMD:=$(call check_strip,$(STRIP),-s --remove-section=.note --remove-section=.comment,$(STRIP))
ifeq ($(strip $(CONFIG_STATIC)),y)
    PROG_CFLAGS += $(call check_cc,$(CC),-static,)
endif
CFLAGS_SHARED := $(call check_cc,$(CC),-shared,)
LIB_CFLAGS+=$(CFLAGS_SHARED)

ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)
    CFLAGS_PIC:= $(call check_cc,$(CC),-fPIC,)
    LIB_CFLAGS+=$(CFLAGS_PIC)
endif

ifeq ($(strip $(CONFIG_SELINUX)),y)
    LIBRARIES += -lselinux
endif

ifeq ($(strip $(PREFIX)),)
    PREFIX:=`pwd`/_install
endif

ifneq ($(strip $(CONFIG_GETOPT_LONG)),y)
    CFLAGS += -D__need_getopt
endif

# Additional complications due to support for pristine source dir.
# Include files in the build directory should take precedence over
# the copy in top_srcdir, both during the compilation phase and the
# shell script that finds the list of object files.
# Work in progress by <ldoolitt@recycle.lbl.gov>.


OBJECTS:=$(APPLET_SOURCES:.c=.o) busybox.o usage.o applets.o
CFLAGS  += $(CHECKED_CFLAGS) $(CROSS_CFLAGS)
LDFLAGS += $(CHECKED_LDFLAGS)

ifdef BB_INIT_SCRIPT
    CFLAGS += -DINIT_SCRIPT='"$(BB_INIT_SCRIPT)"'
endif

# Put user-supplied flags at the end, where they
# have a chance of winning.
CFLAGS += $(CFLAGS_EXTRA)

#------------------------------------------------------------
# Installation options
ifeq ($(strip $(CONFIG_INSTALL_APPLET_HARDLINKS)),y)
INSTALL_OPTS=--hardlinks
endif
ifeq ($(strip $(CONFIG_INSTALL_APPLET_SYMLINKS)),y)
INSTALL_OPTS=--symlinks
endif
ifeq ($(strip $(CONFIG_INSTALL_APPLET_DONT)),y)
INSTALL_OPTS=
endif

#------------------------------------------------------------
# Make the output nice and tight
MAKEFLAGS += --no-print-directory
export MAKE_IS_SILENT=n
ifneq ($(findstring s,$(MAKEFLAGS)),)
export MAKE_IS_SILENT=y
SECHO := @-false
DISP  := sil
Q     := @
else
ifneq ($(V)$(VERBOSE),)
SECHO := @-false
DISP  := ver
Q     := 
else
SECHO := @echo
DISP  := pur
Q     := @
endif
endif

show_objs = $(subst $(top_builddir)/,,$(subst ../,,$@))
pur_disp_compile.c = echo "  "CC $(show_objs)
pur_disp_compile.h = echo "  "HOSTCC $(show_objs)
pur_disp_strip     = echo "  "STRIP $(show_objs)
pur_disp_link      = echo "  "LINK $(show_objs)
pur_disp_link.h    = echo "  "HOSTLINK $(show_objs)
pur_disp_ar        = echo "  "AR $(ARFLAGS) $(show_objs)
pur_disp_elf2flt   = echo "  "ELF2FLT $(ELF2FLTFLAGS) $(show_objs)
sil_disp_compile.c = true
sil_disp_compile.h = true
sil_disp_strip     = true
sil_disp_link      = true
sil_disp_link.h    = true
sil_disp_ar        = true
sil_disp_elf2flt   = true
ver_disp_compile.c = echo $(cmd_compile.c)
ver_disp_compile.h = echo $(cmd_compile.h)
ver_disp_strip     = echo $(cmd_strip)
ver_disp_link      = echo $(cmd_link)
ver_disp_link.h    = echo $(cmd_link.h)
ver_disp_ar        = echo $(cmd_ar)
ver_disp_elf2flt   = echo $(cmd_elf2flt)
disp_compile.c     = $($(DISP)_disp_compile.c)
disp_compile.h     = $($(DISP)_disp_compile.h)
disp_strip         = $($(DISP)_disp_strip)
disp_link          = $($(DISP)_disp_link)
disp_link.h        = $($(DISP)_disp_link.h)
disp_ar            = $($(DISP)_disp_ar)
disp_gen           = $(SECHO) "  "GEN $@ ; true
disp_doc           = $(SECHO) "  "DOC $(subst docs/,,$@) ; true
disp_elf2flt       = $($(DISP)_disp_elf2flt)
cmd_compile.c      = $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
cmd_compile.h      = $(HOSTCC) $(HOSTCFLAGS) $(INCS) -c -o $@ $<
cmd_strip          = $(STRIPCMD) $@
cmd_link           = $(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS)
cmd_link.h         = $(HOSTCC) $(HOSTCFLAGS) $(INCS) $< -o $@
cmd_ar             = $(AR) $(ARFLAGS) $@ $^
cmd_elf2flt        = $(ELF2FLT) $(ELF2FLTFLAGS) $< -o $@
compile.c          = @$(disp_compile.c) ; $(cmd_compile.c)
compile.h          = @$(disp_compile.h) ; $(cmd_compile.h)
do_strip           = @$(disp_strip)     ; $(cmd_strip)
do_link            = @$(disp_link)      ; $(cmd_link)
do_link.h          = @$(disp_link.h)    ; $(cmd_link.h)
do_ar              = @$(disp_ar)        ; $(cmd_ar)
do_elf2flt         = @$(disp_elf2flt)   ; $(cmd_elf2flt)

uppercase = $(shell echo $1 | tr '[:lower:]' '[:upper:]')
%.a:
	@if test -z "$($(call uppercase,$*)_DIR)" ; then \
		echo "Invalid target $@" ; \
		exit 1 ; \
	fi
	$(Q)$(MAKE) $($(call uppercase,$*)_DIR)$@

.PHONY: dummy
