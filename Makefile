# Makefile for busybox
#
# Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
#
# Licensed under GPLv2, see the file LICENSE in this tarball for details.
#

#--------------------------------------------------------------
# You shouldn't need to mess with anything beyond this point...
#--------------------------------------------------------------
noconfig_targets := menuconfig config oldconfig randconfig \
	defconfig allyesconfig allnoconfig allbareconfig \
	release tags

# the toplevel sourcedir
ifndef top_srcdir
top_srcdir:=$(shell cd $(dir $(firstword $(MAKEFILE_LIST))) && pwd)
endif
# toplevel directory of the object-tree
ifndef top_builddir
top_builddir:=$(CURDIR)
endif

export srctree=$(top_srcdir)
vpath %/Config.in $(srctree)

DIRS:=applets archival archival/libunarchive coreutils console-tools \
	debianutils editors findutils init miscutils modutils networking \
	networking/libiproute networking/udhcp procps loginutils shell \
	sysklogd util-linux e2fsprogs libpwdgrp coreutils/libcoreutils libbb

SRC_DIRS:=$(patsubst %,$(top_srcdir)/%,$(DIRS))

# That's our default target when none is given on the command line
.PHONY: _all

_all: all

# see if we are in verbose mode
ifdef VERBOSE
  CHECK_VERBOSE := -v
  PACKAGE_BE_VERBOSE := $(VERBOSE)
endif
ifdef V
  CHECK_VERBOSE := -v
  PACKAGE_BE_VERBOSE := $(V)
endif

ifdef O
  ifeq ("$(origin O)", "command line")
    PACKAGE_OUTPUTDIR := $(shell cd $(O) && pwd)
    top_builddir := $(PACKAGE_OUTPUTDIR)
  endif
else
# If no alternate output-dir was specified, we build in cwd
  PACKAGE_OUTPUTDIR := $(top_builddir)
endif

#######################################################################
# Try to workaround bugs in make

# Workaround for bugs in make-3.80
# eval is broken if it is in a conditional

#$ cat 3.80-eval-in-cond.mak 
#all:: ; @echo it
#define Y
#  all:: ; @echo worked
#endef
#ifdef BAR
#$(eval $(Y))
#endif
#$ make -f 3.80-eval-in-cond.mak
#it
#$ make -f 3.80-eval-in-cond.mak BAR=set
#3.80-eval-in-cond.mak:5: *** missing `endif'.  Stop.

# This was fixed in December 2003.
define check_gcc
$(eval $(1)+=$(if $(2),$(if $(shell $(CC) $(2) -S -o /dev/null -xc /dev/null > /dev/null 2>&1 && echo y),$(2),$(if $(3),$(3))),$(if $(3),$(3))))
endef

define check_ld
$(eval $(1)+=$(if $(2),$(if $(shell $(LD) $(2) -o /dev/null -b binary /dev/null > /dev/null 2>&1 && echo y),$(shell echo \-Wl,$(2)),$(if $(3),$(3))),$(if $(3),$(3))))
endef

#######################################################################


-include $(top_srcdir)/Rules.mak

# Handle building out of tree
ifneq ($(top_builddir),$(top_srcdir))
all_tree := $(patsubst %,$(top_builddir)/%,$(DIRS) scripts scripts/config include include/config)
$(all_tree):
	@mkdir -p "$@"

saved-output := $(PACKAGE_OUTPUTDIR)

$(if $(wildcard $(PACKAGE_OUTPUTDIR)),, \
     $(error output directory "$(saved-output)" does not exist))

.PHONY: $(filter $(noconfig_targets),$(MAKECMDGOALS))

$(PACKAGE_OUTPUTDIR)/Rules.mak:
	@echo > $@
	@echo top_srcdir=$(top_srcdir) >> $@
	@echo top_builddir=$(PACKAGE_OUTPUTDIR) >> $@
	@echo include $$\(top_srcdir\)/Rules.mak >> $@

$(PACKAGE_OUTPUTDIR)/Makefile:
	@echo > $@
	@echo top_srcdir=$(top_srcdir) >> $@
	@echo top_builddir=$(PACKAGE_OUTPUTDIR) >> $@
	@echo PACKAGE_SOURCEDIR='$$(top_srcdir)' >> $@
	@echo include '$$(PACKAGE_SOURCEDIR)'/Makefile >> $@


buildtree := $(all_tree) $(PACKAGE_OUTPUTDIR)/Rules.mak $(PACKAGE_OUTPUTDIR)/Makefile

# We only need a copy of the Makefile for the config targets and reuse
# the rest from the source directory, i.e. we do not cp ALL_MAKEFILES.
scripts/config/Makefile: $(top_srcdir)/scripts/config/Makefile | $(buildtree)
	@cp $(top_srcdir)/scripts/config/Makefile $@

else
all_tree := include/config
$(all_tree):
	@mkdir -p "$@"
buildtree := $(all_tree)
endif # ifneq ($(PACKAGE_OUTPUTDIR),$(top_srcdir))

help:
	@echo 'Cleaning:'
	@echo '  clean			- delete temporary files created by build'
	@echo '  distclean		- delete all non-source files (including .config)'
	@echo
	@echo 'Build:'
	@echo '  all			- Executable and documentation'
	@echo '  busybox		- the swiss-army executable'
	@echo '  doc			- docs/BusyBox.{txt,html,1}'
	@echo
	@echo 'Configuration:'
	@echo '  allnoconfig		- disable all symbols in .config'
	@echo '  allyesconfig		- enable all symbols in .config (see defconfig)'
	@echo '  allbareconfig		- enable all applets without any sub-features'
	@echo '  config		- text based configurator (of last resort)'
	@echo '  defconfig		- set .config to largest generic configuration'
	@echo '  menuconfig		- interactive curses-based configurator'
	@echo '  oldconfig		- resolve any unresolved symbols in .config'
	@echo
	@echo 'Installation:'
	@echo '  install		- install busybox into $prefix'
	@echo '  uninstall'
	@echo
	@echo 'Development:'
	@echo '  check			- run the test suite for all applets'
	@echo '  randconfig		- generate a random configuration'
	@echo '  release		- create a distribution tarball'
	@echo '  sizes			- show size of all enabled busybox symbols'
	@echo
	@echo 'Make flags:'
	@echo '  V=<number>		- print verbose make output (default: unset)'
	@echo '				  0 print CC invocations'
	@echo '				  1'
	@echo '				  2 also print when make enters a directory'
	@echo '				  3 also verbosely print shell invocations'

ifneq ($(strip $(HAVE_DOT_CONFIG)),y)
# Default target if none was requested explicitly
all: defconfig menuconfig ;

ifneq ($(filter-out $(noconfig_targets),$(MAKECMDGOALS)),)
# warn if no configuration exists and we are asked to build a non-config target
.config:
	@echo ""
	@echo "No $(top_builddir)/$@ found!"
	@echo "Please refer to 'make help', section Configuration."
	@echo ""
	@exit 1
else
# Avoid implicit rule to kick in by using an empty command
.config: $(buildtree) ;
endif
endif # ifneq ($(strip $(HAVE_DOT_CONFIG)),y)


# configuration
# ---------------------------------------------------------------------------

CONFIG_CONFIG_IN = $(top_srcdir)/Config.in

scripts/config/conf: scripts/config/Makefile
	$(Q)$(MAKE) -C scripts/config conf

scripts/config/mconf: scripts/config/Makefile
	$(Q)$(MAKE) -C scripts/config ncurses conf mconf

menuconfig: scripts/config/mconf
	@./scripts/config/mconf $(CONFIG_CONFIG_IN)

config: scripts/config/conf
	@./scripts/config/conf $(CONFIG_CONFIG_IN)

oldconfig: scripts/config/conf
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

randconfig: scripts/config/conf
	@./scripts/config/conf -r $(CONFIG_CONFIG_IN)

allyesconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN)
	@$(SED) -i -r -e "s/^(USING_CROSS_COMPILER)=.*/# \1 is not set/" .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

allnoconfig: scripts/config/conf
	@./scripts/config/conf -n $(CONFIG_CONFIG_IN)

# defconfig is allyesconfig minus any features that are specialized enough
# or cause enough behavior change that the user really should switch them on
# manually if that's what they want.  Sort of "maximum sane config".

defconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN)
	@$(SED) -i -r -e "s/^(USING_CROSS_COMPILER|CONFIG_(DEBUG.*|STATIC|SELINUX|BUILD_(AT_ONCE|LIBBUSYBOX)|FEATURE_(DEVFS|FULL_LIBBUSYBOX|SHARED_BUSYBOX|MTAB_SUPPORT|CLEAN_UP|UDHCP_DEBUG)|INSTALL_NO_USR))=.*/# \1 is not set/" .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

allbareconfig: scripts/config/conf
	@./scripts/config/conf -y $(CONFIG_CONFIG_IN)
	@$(SED) -i -r -e "s/^(USING_CROSS_COMPILER|CONFIG_(DEBUG|STATIC|SELINUX|DEVFSD|NC_GAPING_SECURITY_HOLE|BUILD_AT_ONCE)).*/# \1 is not set/" .config
	@$(SED) -i -e "/FEATURE/s/=.*//;/^[^#]/s/.*FEATURE.*/# \0 is not set/;" .config
	@echo "CONFIG_FEATURE_BUFFERS_GO_ON_STACK=y" >> .config
	@./scripts/config/conf -o $(CONFIG_CONFIG_IN)

ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

# Load all Config.in
-include $(top_builddir)/.config.cmd

endif # ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

# convert $(DIRS) to upper case. Use sed instead of tr since we're already
# depending on it.
DIRS_UPPER:=$(shell echo $(DIRS) | $(SED) 'h;y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/')

# First populate the variables ..._OBJ-y et al
$(foreach d,$(DIRS_UPPER),$(eval $(notdir $(d))-y:=))

include $(patsubst %,%/Makefile.in,$(SRC_DIRS))

# Then we need the dependencies for ..._OBJ
define dir_pattern.o
ifeq ($(os),.os)
# write patterns for both .os and .o
$(if $($(1)_OBJ.os),$($(1)_OBJ.os:.os=.o): $(top_builddir)/$(2)/%.o: $(top_srcdir)/$(2)/%.c)
endif
$(if $($(1)_OBJ$(os)),$($(1)_OBJ$(os)): $(top_builddir)/$(2)/%$(os): $(top_srcdir)/$(2)/%.c)
$(if $($(1)_OBJ),$($(1)_OBJ): $(top_builddir)/$(2)/%.o: $(top_srcdir)/$(2)/%.c)

lib-obj-y+=$($(1)_OBJ) $($(1)_OBJ.o) $($(1)_OBJ.os)
lib-mobj-y+=$($(1)_MOBJ.o) $($(1)_MOBJ.os)
bin-obj-y+=$($(1)_OBJ:.os=.o) $($(1)_OBJ.o:.os=.o) $($(1)_OBJ.os:.os=.o)
bin-mobj-y+=$($(1)_MOBJ.o:.osm=.om) $($(1)_MOBJ.os:.osm=.om)
endef
# The actual directory patterns for .o*
$(foreach d,$(DIRS),$(eval $(call dir_pattern.o,$(subst /,_,$(d)),$(d))))

ifeq ($(strip $(HAVE_DOT_CONFIG)),y)
# Finally pull in the dependencies (headers and other includes) of the
# individual object files
-include $(top_builddir)/.depend

$(top_builddir)/applets/applets.o: $(top_builddir)/.config
# Everything is set.

all: busybox busybox.links doc ;

# Two modes of operation: legacy and IMA
# Legacy mode builds each object through an individual invocation of CC
# IMA compiles all sources at once (aka IPO aka IPA etc.)

ifeq ($(strip $(CONFIG_BUILD_AT_ONCE)),y)
# We are not building .o
bin-obj-y:=
bin-mobj-y:=
# Which parts of the internal libs are requested?
# Per default we only want what was actually selected.
# -a denotes all while -y denotes the selected ones.
ifeq ($(strip $(CONFIG_FEATURE_FULL_LIBBUSYBOX)),y)
LIBRARY_DEFINE:=$(LIBRARY_DEFINE-a)
LIBRARY_SRC   :=$(LIBRARY_SRC-a)
else # CONFIG_FEATURE_FULL_LIBBUSYBOX
LIBRARY_DEFINE:=$(LIBRARY_DEFINE-y)
LIBRARY_SRC   :=$(LIBRARY_SRC-y)
endif # CONFIG_FEATURE_FULL_LIBBUSYBOX
APPLET_SRC:=$(APPLET_SRC-y)
APPLETS_DEFINE:=$(APPLETS_DEFINE-y)
else  # CONFIG_BUILD_AT_ONCE
# no --combine, build archives out of the individual .o
# This was the old way the binary was built.
libbusybox-obj:=$(archival_libunarchive_OBJ$(os)) \
	$(networking_libiproute_OBJ$(os)) \
	$(libpwdgrp_MOBJ$(os)) \
	$(coreutils_libcoreutils_OBJ$(os)) \
	$(libbb_OBJ$(os)) $(libbb_MOBJ$(os))

ifeq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)
# linking against libbusybox, so don't build the .o already contained in the .so
bin-obj-y:=$(filter-out $(libbusybox-obj) $(libbusybox-obj:.os=.o),$(bin-obj-y))
bin-mobj-y:=$(filter-out $(libbusybox-obj) $(libbusybox-obj:.osm=.om),$(bin-mobj-y))
endif # CONFIG_FEATURE_SHARED_BUSYBOX
endif # CONFIG_BUILD_AT_ONCE

# build an .a to keep .hash et al small
ifneq ($(bin-obj-y)$(bin-mobj-y),)
  applets.a:=$(bin-obj-y) $(bin-mobj-y)
endif
ifdef applets.a
applets.a: $(applets.a)
	$(Q)-rm -f $(@)
	$(do_ar)

bin-obj.a=applets.a
endif

ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)
LD_LIBBUSYBOX:=libbusybox.so
LIBBUSYBOX_SONAME:=$(LD_LIBBUSYBOX).$(MAJOR_VERSION).$(MINOR_VERSION).$(SUBLEVEL_VERSION)
DO_INSTALL_LIBS:=$(LD_LIBBUSYBOX) \
	$(LD_LIBBUSYBOX).$(MAJOR_VERSION) \
	$(LD_LIBBUSYBOX).$(MAJOR_VERSION).$(MINOR_VERSION)

endif # CONFIG_BUILD_LIBBUSYBOX

ifeq ($(strip $(CONFIG_BUILD_AT_ONCE)),y)
ifneq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)
# --combine but not linking against libbusybox, so compile lib*.c
BUSYBOX_SRC   := $(LIBRARY_SRC)
BUSYBOX_DEFINE:= $(LIBRARY_DEFINE)
endif # !CONFIG_FEATURE_SHARED_BUSYBOX
$(LIBBUSYBOX_SONAME): $(LIBRARY_SRC)
else # CONFIG_BUILD_AT_ONCE
$(LIBBUSYBOX_SONAME): $(libbusybox-obj)
endif # CONFIG_BUILD_AT_ONCE


ifeq ($(strip $(CONFIG_FEATURE_SHARED_BUSYBOX)),y)
LDBUSYBOX:=-L$(top_builddir) -lbusybox
endif

ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)
$(LIBBUSYBOX_SONAME):
ifndef MAJOR_VERSION
	$(error MAJOR_VERSION needed for $@ is not defined)
endif
	$(do_link) $(LIB_CFLAGS) $(LIB_LDFLAGS) $(CFLAGS_COMBINE) \
	-Wl,-soname=$(LD_LIBBUSYBOX).$(MAJOR_VERSION) \
	-Wl,-z,combreloc $(LIB_LDFLAGS) \
	-o $(@) \
	-Wl,--start-group \
	$(LIBRARY_DEFINE) $(^) \
	-Wl,--end-group
	@rm -f $(DO_INSTALL_LIBS)
	@for i in $(DO_INSTALL_LIBS); do ln -s $(@) $$i ; done
	$(do_strip)

endif # ifeq ($(strip $(CONFIG_BUILD_LIBBUSYBOX)),y)

busybox_unstripped: $(top_builddir)/.depend $(LIBBUSYBOX_SONAME) $(BUSYBOX_SRC) $(APPLET_SRC) $(bin-obj.a)
	$(do_link) $(PROG_CFLAGS) $(PROG_LDFLAGS) $(CFLAGS_COMBINE) \
	$(foreach f,$(^:.o=.c),$(CFLAGS-$(notdir $(patsubst %/$,%,$(dir $(f))))-$(notdir $(f)))) \
	$(CFLAGS-$(@)) \
	-o $@ -Wl,--start-group \
	$(APPLETS_DEFINE) $(APPLET_SRC) \
	$(BUSYBOX_DEFINE) $(BUSYBOX_SRC) \
	$(bin-obj.a) \
	$(LDBUSYBOX) $(LIBRARIES) \
	-Wl,--end-group

busybox: busybox_unstripped
	$(Q)cp busybox_unstripped busybox
	$(do_strip)

busybox.links: $(top_srcdir)/applets/busybox.mkll include/bb_config.h $(top_srcdir)/include/applets.h
	$(Q)-$(SHELL) $^ >$@

install: $(top_srcdir)/applets/install.sh busybox busybox.links
	$(Q)DO_INSTALL_LIBS="$(strip $(LIBBUSYBOX_SONAME) $(DO_INSTALL_LIBS))" \
		$(SHELL) $< $(PREFIX) $(INSTALL_OPTS)
ifeq ($(strip $(CONFIG_FEATURE_SUID)),y)
	@echo
	@echo
	@echo --------------------------------------------------
	@echo You will probably need to make your busybox binary
	@echo setuid root to ensure all configured applets will
	@echo work properly.
	@echo --------------------------------------------------
	@echo
endif

uninstall: busybox.links
	rm -f $(PREFIX)/bin/busybox
	for i in `cat busybox.links` ; do rm -f $(PREFIX)$$i; done
ifneq ($(strip $(DO_INSTALL_LIBS)),n)
	for i in $(LIBBUSYBOX_SONAME) $(DO_INSTALL_LIBS); do \
		rm -f $(PREFIX)$$i; \
	done
endif

check test: busybox
	bindir=$(top_builddir) srcdir=$(top_srcdir)/testsuite \
	$(top_srcdir)/testsuite/runtest $(CHECK_VERBOSE)

sizes: busybox_unstripped
	$(NM) --size-sort $(<)

# Documentation Targets
doc: docs/busybox.pod docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html ;

docs/busybox.pod : $(top_srcdir)/docs/busybox_header.pod $(top_srcdir)/include/usage.h $(top_srcdir)/docs/busybox_footer.pod $(top_srcdir)/docs/autodocifier.pl
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-( cat $(top_srcdir)/docs/busybox_header.pod ; \
	    $(top_srcdir)/docs/autodocifier.pl $(top_srcdir)/include/usage.h ; \
	    cat $(top_srcdir)/docs/busybox_footer.pod ; ) > docs/busybox.pod

docs/BusyBox.txt: docs/busybox.pod
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-pod2text $< > $@

docs/BusyBox.1: docs/busybox.pod
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-pod2man --center=BusyBox --release="version $(VERSION)" \
		$< > $@

docs/BusyBox.html: docs/busybox.net/BusyBox.html
	$(disp_doc)
	$(Q)-mkdir -p docs
	$(Q)-rm -f docs/BusyBox.html
	$(Q)-cp docs/busybox.net/BusyBox.html docs/BusyBox.html

docs/busybox.net/BusyBox.html: docs/busybox.pod
	$(Q)-mkdir -p docs/busybox.net
	$(Q)-pod2html --noindex $< > \
	    docs/busybox.net/BusyBox.html
	$(Q)-rm -f pod2htm*

# The nifty new dependency stuff
scripts/bb_mkdep: $(top_srcdir)/scripts/bb_mkdep.c
	$(do_link.h)

DEP_INCLUDES := include/bb_config.h

ifeq ($(strip $(CONFIG_BBCONFIG)),y)
DEP_INCLUDES += include/bbconfigopts.h

include/bbconfigopts.h: .config
	$(disp_gen)
	$(Q)$(top_srcdir)/scripts/config/mkconfigs > $@
endif

depend dep: $(top_builddir)/.depend ;
$(top_builddir)/.depend: $(buildtree) scripts/bb_mkdep $(DEP_INCLUDES)
	$(disp_gen)
	$(Q)rm -f .depend
	$(Q)scripts/bb_mkdep $(MKDEP_ARGS) \
		-I $(top_srcdir)/include $(top_srcdir) > $@.tmp
	$(Q)mv $@.tmp $@

include/bb_config.h: .config
	$(disp_gen)
	@$(top_builddir)/scripts/config/conf -o $(CONFIG_CONFIG_IN)

endif # ifeq ($(strip $(HAVE_DOT_CONFIG)),y)

clean:
	- rm -f docs/busybox.dvi docs/busybox.ps \
	    docs/busybox.pod docs/busybox.net/busybox.html \
	    docs/busybox pod2htm* *.gdb *.elf *~ core .*config.log \
	    docs/BusyBox.txt docs/BusyBox.1 docs/BusyBox.html \
	    docs/busybox.net/BusyBox.html busybox.links \
	    $(DO_INSTALL_LIBS) $(LIBBUSYBOX_SONAME) \
	    .config.old busybox
	- rm -r -f _install testsuite/links
	- find . -name .\*.flags -exec rm -f {} \;
	- find . -name \*.o -exec rm -f {} \;
	- find . -name \*.om -exec rm -f {} \;
	- find . -name \*.os -exec rm -f {} \;
	- find . -name \*.a -exec rm -f {} \;

distclean: clean
	- $(MAKE) -C scripts/config clean
	- rm -f scripts/bb_mkdep
	- rm -r -f include/config $(DEP_INCLUDES)
	- find . -name .depend'*' -exec rm -f {} \;
	rm -f .config .config.old .config.cmd

release: distclean #doc
	cd ..; \
	rm -r -f $(PROG)-$(VERSION); \
	cp -a busybox $(PROG)-$(VERSION); \
	\
	find $(PROG)-$(VERSION)/ -type d \
		-name .svn \
		-print \
		-exec rm -r -f {} \; ; \
	\
	find $(PROG)-$(VERSION)/ -type f \
		-name .\#* \
		-print \
		-exec rm -f {} \; ; \
	\
	tar -cvzf $(PROG)-$(VERSION).tar.gz $(PROG)-$(VERSION)/;

tags:
	ctags -R .

# keep these in sync with noconfig_targets above!
.PHONY: dummy subdirs check test depend dep buildtree \
        menuconfig config oldconfig randconfig \
	defconfig allyesconfig allnoconfig allbareconfig \
	clean distclean \
	release tags
