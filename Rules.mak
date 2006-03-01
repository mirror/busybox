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

ifeq ($(MAKELEVEL),0)
ifeq ($(HAVE_DOT_CONFIG),y)
rules-mak-rules:=0
endif
endif

#--------------------------------------------------------
PROG      := busybox
MAJOR_VERSION   :=1
MINOR_VERSION   :=1
SUBLEVEL_VERSION:=1
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
# your compiler is broken, you should not need to specify __TARGET_ARCH
CROSS           =$(subst ",, $(strip $(CROSS_COMPILER_PREFIX)))
#")
CC             = $(CROSS)gcc
AR             = $(CROSS)ar
AS             = $(CROSS)as
LD             = $(CROSS)ld
NM             = $(CROSS)nm
STRIP          = $(CROSS)strip
CPP            = $(CC) -E
SED           ?= sed
AWK           ?= awk


ifdef PACKAGE_BE_VERBOSE
PACKAGE_BE_VERBOSE := $(shell echo $(PACKAGE_BE_VERBOSE) | $(SED) "s/[[:alpha:]]*//g")
endif

# for make V=3 and above make $(shell) invocations verbose
ifeq ($(if $(strip $(PACKAGE_BE_VERBOSE)),$(shell test $(PACKAGE_BE_VERBOSE) -gt 2 ; echo $$?),1),0)
 SHELL+=-x
 MKDEP_ARGS:=-w
endif

# What OS are you compiling busybox for?  This allows you to include
# OS specific things, syscall overrides, etc.
TARGET_OS=linux

# Select the compiler needed to build binaries for your development system
HOSTCC    = gcc
HOSTCFLAGS= -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer

# Ensure consistent sort order, 'gcc -print-search-dirs' behavior, etc.
LC_ALL:= C

# If you want to add some simple compiler switches (like -march=i686),
# especially from the command line, use this instead of CFLAGS directly.
# For optimization overrides, it's better still to set OPTIMIZATIONS.
CFLAGS_EXTRA=$(subst ",, $(strip $(EXTRA_CFLAGS_OPTIONS)))
#")

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
#GCCINCDIR:=$(shell gcc -print-search-dirs | sed -ne "s/install: \(.*\)/\1include/gp")

WARNINGS=-Wall -Wstrict-prototypes -Wshadow
CFLAGS+=-I$(top_builddir)/include -I$(top_srcdir)/include

ARFLAGS=cru



# Get the CC MAJOR/MINOR version
# gcc centric. Perhaps fiddle with findstring gcc,$(CC) for the rest
CC_MAJOR:=$(shell printf "%02d" $(shell echo __GNUC__ | $(CC) -E -xc - | tail -n 1))
CC_MINOR:=$(shell printf "%02d" $(shell echo __GNUC_MINOR__ | $(CC) -E -xc - | tail -n 1))

# Note: spaces are significant here!
# Check if CC version is equal to given MAJOR,MINOR. Returns empty if false.
define cc_eq
$(shell [ $(CC_MAJOR) -eq $(1) -a $(CC_MINOR) -eq $(2) ] && echo y)
endef
# Check if CC version is greater or equal than given MAJOR,MINOR
define cc_ge
$(shell [ $(CC_MAJOR) -ge $(1) -a $(CC_MINOR) -ge $(2) ] && echo y)
endef
# Check if CC version is less or equal than given MAJOR,MINOR
define cc_le
$(shell [ $(CC_MAJOR) -le $(1) -a $(CC_MINOR) -le $(2) ] && echo y)
endef

# Workaround bugs in make-3.80 for eval in conditionals
define is_eq
$(shell [ $(1) = $(2) ] 2> /dev/null && echo y)
endef
define is_neq
$(shell [ $(1) != $(2) ] 2> /dev/null && echo y)
endef

#--------------------------------------------------------
export VERSION BUILDTIME HOSTCC HOSTCFLAGS CROSS CC AR AS LD NM STRIP CPP

# TARGET_ARCH and TARGET_MACH will be passed verbatim to CC with recent
# versions of make, so we use __TARGET_ARCH here.
# Current builtin rules looks like that:
# COMPILE.s = $(AS) $(ASFLAGS) $(TARGET_MACH)
# COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

ifeq ($(strip $(__TARGET_ARCH)),)
__TARGET_ARCH:=$(shell $(CC) -dumpmachine | sed -e s'/-.*//' \
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

$(call check_gcc,CFLAGS,-funsigned-char,)
$(call check_gcc,CFLAGS,-mmax-stack-frame=256,)

#--------------------------------------------------------
# Arch specific compiler optimization stuff should go here.
# Unless you want to override the defaults, do not set anything
# for OPTIMIZATIONS...

# use '-Os' optimization if available, else use -O2
$(call check_gcc,OPTIMIZATIONS,-Os,-O2)

# gcc 2.95 exits with 0 for "unrecognized option"
$(if $(call is_eq,$(CONFIG_BUILD_AT_ONCE),y),\
  $(if $(call cc_ge,3,0),\
    $(call check_gcc,CFLAGS_COMBINE,--combine,)))

$(if $(call is_eq,$(CONFIG_BUILD_AT_ONCE),y),\
  $(call check_gcc,OPTIMIZATIONS,-funit-at-a-time,))

# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=25795
#$(if $(call is_eq,$(CONFIG_BUILD_AT_ONCE),y),\
#  $(call check_gcc,PROG_CFLAGS,-fwhole-program,))

$(call check_ld,LIB_LDFLAGS,--enable-new-dtags,)
#$(call check_ld,LIB_LDFLAGS,--reduce-memory-overheads,)
#$(call check_ld,LIB_LDFLAGS,--as-needed,)
#$(call check_ld,LIB_LDFLAGS,--warn-shared-textrel,)

$(call check_ld,PROG_LDFLAGS,--gc-sections,)

# Some nice architecture specific optimizations
ifeq ($(__TARGET_ARCH),arm)
    OPTIMIZATIONS+=-fstrict-aliasing
endif # arm

$(if $(call is_eq,$(__TARGET_ARCH),i386),\
  $(call check_gcc,OPTIMIZATIONS,-march=i386,))

# gcc-4.0 and older seem to suffer from these
$(if $(call cc_le,4,0),\
  $(call check_gcc,OPTIMIZATIONS,-mpreferred-stack-boundary=2,)\
  $(call check_gcc,OPTIMIZATIONS,-falign-functions=0 -falign-jumps=0 -falign-loops=0,\
		-malign-functions=0 -malign-jumps=0 -malign-loops=0))

# gcc-4.1 and beyond seem to benefit from these
# turn off flags which hurt -Os
$(if $(call cc_ge,4,1),\
  $(call check_gcc,OPTIMIZATIONS,-fno-tree-loop-optimize,)\
  $(call check_gcc,OPTIMIZATIONS,-fno-tree-dominator-opts,)\
  $(call check_gcc,OPTIMIZATIONS,-fno-strength-reduce,)\
\
  $(call check_gcc,OPTIMIZATIONS,-fno-branch-count-reg,))

$(call check_gcc,OPTIMIZATIONS,-fomit-frame-pointer,)

#
#--------------------------------------------------------
# If you're going to do a lot of builds with a non-vanilla configuration,
# it makes sense to adjust parameters above, so you can type "make"
# by itself, instead of following it by the same half-dozen overrides
# every time.  The stuff below, on the other hand, is probably less
# prone to casual user adjustment.
#

ifeq ($(CONFIG_LFS),y)
    # For large file summit support
    CFLAGS+=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
endif
ifeq ($(CONFIG_DMALLOC),y)
    # For testing mem leaks with dmalloc
    CFLAGS+=-DDMALLOC
    LIBRARIES:=-ldmalloc
else
    ifeq ($(CONFIG_EFENCE),y)
	LIBRARIES:=-lefence
    endif
endif

$(if $(call is_eq,$(CONFIG_DEBUG),y),\
 $(call check_ld,LDFLAGS,--warn-common,),\
 $(call check_ld,LDFLAGS,--warn-common,)$(call check_ld,LDFLAGS,--sort-common,))
ifeq ($(CONFIG_DEBUG),y)
    CFLAGS  +=$(WARNINGS) -g -D_GNU_SOURCE
    STRIPCMD:=/bin/true -Not_stripping_since_we_are_debugging
else
    CFLAGS+=$(WARNINGS) $(OPTIMIZATIONS) -D_GNU_SOURCE -DNDEBUG
    STRIPCMD:=$(STRIP) -s --remove-section=.note --remove-section=.comment
endif
$(if $(call is_eq,$(CONFIG_STATIC),y),\
    $(call check_gcc,PROG_CFLAGS,-static,))

$(call check_gcc,CFLAGS_SHARED,-shared,)
LIB_CFLAGS+=$(CFLAGS_SHARED)

$(if $(call is_eq,$(CONFIG_BUILD_LIBBUSYBOX),y),\
    $(call check_gcc,CFLAGS_PIC,-fPIC,))

ifeq ($(CONFIG_SELINUX),y)
    LIBRARIES += -lselinux
endif

ifeq ($(strip $(PREFIX)),)
    PREFIX:=`pwd`/_install
endif

CFLAGS    += $(CROSS_CFLAGS)
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
# object extensions

# object potentially used in shared object
ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)
# single-object extension
os:=.os
# multi-object extension
om:=.osm
else
os:=.o
om:=.om
endif

#------------------------------------------------------------
# Make the output nice and tight

# for make V=2 and above, do print directory
ifneq ($(shell test -n "$(strip $(PACKAGE_BE_VERBOSE))" && test $(PACKAGE_BE_VERBOSE) -gt 1 ; echo $$?),0)
  MAKEFLAGS += --no-print-directory
endif

export MAKEOVERRIDES
export MAKE_IS_SILENT=n
ifneq ($(findstring s,$(MAKEFLAGS)),)
export MAKE_IS_SILENT=y
DISP  := sil
Q     := @
else
ifneq ($(V)$(VERBOSE),)
DISP  := ver
Q     := 
else
DISP  := pur
Q     := @
endif
endif

define show_objs
  $(subst $(top_builddir)/,,$(subst ../,,$@))
endef
pur_disp_compile.c = @echo "  "CC $(show_objs) ;
pur_disp_compile.h = @echo "  "HOSTCC $(show_objs) ;
pur_disp_strip     = @echo "  "STRIP $(show_objs) ;
pur_disp_link      = @echo "  "LINK $(show_objs) ;
pur_disp_link.h    = @echo "  "HOSTLINK $(show_objs) ;
pur_disp_ar        = @echo "  "AR $(ARFLAGS) $(show_objs) ;
pur_disp_gen       = @echo "  "GEN $@ ;
pur_disp_doc       = @echo "  "DOC $(subst docs/,,$@) ;
pur_disp_bin       = @echo "  "BIN $(show_objs) ;
sil_disp_compile.c = @
sil_disp_compile.h = @
sil_disp_strip     = @
sil_disp_link      = @
sil_disp_link.h    = @
sil_disp_ar        = @
sil_disp_gen       = @
sil_disp_doc       = @
sil_disp_bin       = @
ver_disp_compile.c =
ver_disp_compile.h =
ver_disp_strip     =
ver_disp_link      =
ver_disp_link.h    =
ver_disp_ar        =
ver_disp_gen       =
ver_disp_doc       =
ver_disp_bin       =
disp_compile.c     = $($(DISP)_disp_compile.c)
disp_compile.h     = $($(DISP)_disp_compile.h)
disp_strip         = $($(DISP)_disp_strip)
disp_link          = $($(DISP)_disp_link)
disp_link.h        = $($(DISP)_disp_link.h)
disp_ar            = $($(DISP)_disp_ar)
disp_gen           = $($(DISP)_disp_gen)
disp_doc           = $($(DISP)_disp_doc)
disp_bin           = $($(DISP)_disp_bin)
# CFLAGS-dir        == $(CFLAGS-$(notdir $(@D)))
# CFLAGS-dir-file.o == $(CFLAGS-$(notdir $(@D))-$(notdir $(@F)))
# CFLAGS-dir-file.c == $(CFLAGS-$(notdir $(<D))-$(notdir $(<F)))
# all prerequesites == $(foreach fil,$^,$(CFLAGS-$(notdir $(patsubst %/$,%,$(dir $(fil))))-$(notdir $(fil))))
cmd_compile.c      = $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -I$(srcdir) -c -o $@ $< \
 $(foreach f,$^,$(CFLAGS-$(notdir $(patsubst %/$,%,$(dir $(f))))-$(notdir $(f)))) \
 $(CFLAGS-$(notdir $(@D))-$(notdir $(@F))) \
 $(CFLAGS-$(notdir $(@D)))
cmd_compile.m      = $(cmd_compile.c) -DL_$(patsubst %$(suffix $(notdir $@)),%,$(notdir $@))
cmd_compile.h      = $(HOSTCC) $(HOSTCFLAGS) -c -o $@ $<
cmd_strip          = $(STRIPCMD) $@
cmd_link           = $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -I$(srcdir) $(LDFLAGS)
cmd_link.h         = $(HOSTCC) $(HOSTCFLAGS) $(HOST_LDFLAGS) $^ -o $@
cmd_ar             = $(AR) $(ARFLAGS) $@ $^
compile.c          = $(disp_compile.c) $(cmd_compile.c)
compile.m          = $(disp_compile.c) $(cmd_compile.m)
compile.h          = $(disp_compile.h) $(cmd_compile.h)
do_strip           = $(disp_strip)     $(cmd_strip)
do_link            = $(disp_link)      $(cmd_link)
do_link.h          = $(disp_link.h)    $(cmd_link.h)
do_ar              = $(disp_ar)        $(cmd_ar)

ifdef rules-mak-rules
.SUFFIXES: .c .S .o .os .om .osm .oS .so .a .s .i .E

# generic rules
%.o:  %.c ; $(compile.c)
%.os: %.c ; $(compile.c) $(CFLAGS_PIC)
%.o:      ; $(compile.c)
%.os:     ; $(compile.c) $(CFLAGS_PIC)
%.om:     ; $(compile.m)
%.osm:    ; $(compile.m) $(CFLAGS_PIC)

endif # rules-mak-rules

.PHONY: dummy

