#
# This file contains rules which are shared between multiple Makefiles.
#

#
# False targets.
#
.PHONY: dummy

#
# Special variables which should not be exported
#
unexport EXTRA_AFLAGS
unexport EXTRA_CFLAGS
unexport EXTRA_LDFLAGS
unexport EXTRA_ARFLAGS
unexport SUBDIRS
unexport SUB_DIRS
unexport ALL_SUB_DIRS
unexport O_TARGET

unexport obj-y
unexport obj-n
unexport obj-
unexport export-objs
unexport subdir-y
unexport subdir-n
unexport subdir-

#
# Get things started.
#
first_rule: sub_dirs
	$(MAKE) all_targets

SUB_DIRS	:= $(subdir-y)
ALL_SUB_DIRS	:= $(sort $(subdir-y) $(subdir-n) $(subdir-))


#
# Common rules
#

%.s: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -S $< -o $@

%.i: %.c
	$(CPP) $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) $< > $@

%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -c -o $@ $<
	@ ( \
	    echo 'ifeq ($(strip $(subst $(comma),:,$(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@))),$$(strip $$(subst $$(comma),:,$$(CFLAGS) $$(EXTRA_CFLAGS) $$(CFLAGS_$@))))' ; \
	    echo 'FILES_FLAGS_UP_TO_DATE += $@' ; \
	    echo 'endif' \
	) > $(dir $@)/.$(notdir $@).flags

%.o: %.s
	$(AS) $(AFLAGS) $(EXTRA_CFLAGS) -o $@ $<

# Old makefiles define their own rules for compiling .S files,
# but these standard rules are available for any Makefile that
# wants to use them.  Our plan is to incrementally convert all
# the Makefiles to these standard rules.  -- rmk, mec
ifdef USE_STANDARD_AS_RULE

%.s: %.S
	$(CPP) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) $< > $@

%.o: %.S
	$(CC) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -c -o $@ $<

endif

%.lst: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -g -c -o $*.o $<
	$(TOPDIR)/scripts/makelst $* $(TOPDIR) $(OBJDUMP)
#
#
#
all_targets: $(O_TARGET) $(L_TARGET)

#
# Rule to compile a set of .o files into one .o file
#
ifdef O_TARGET
$(O_TARGET): $(obj-y)
	rm -f $@
    ifneq "$(strip $(obj-y))" ""
	$(LD) $(EXTRA_LDFLAGS) -r -o $@ $(filter $(obj-y), $^)
    else
	$(AR) rcs $@
    endif
	@ ( \
	    echo 'ifeq ($(strip $(subst $(comma),:,$(EXTRA_LDFLAGS) $(obj-y))),$$(strip $$(subst $$(comma),:,$$(EXTRA_LDFLAGS) $$(obj-y))))' ; \
	    echo 'FILES_FLAGS_UP_TO_DATE += $@' ; \
	    echo 'endif' \
	) > $(dir $@)/.$(notdir $@).flags
endif # O_TARGET

#
# Rule to compile a set of .o files into one .a file
#
ifdef L_TARGET
$(L_TARGET): $(obj-y)
	rm -f $@
	$(AR) $(EXTRA_ARFLAGS) rcs $@ $(obj-y)
	@ ( \
	    echo 'ifeq ($(strip $(subst $(comma),:,$(EXTRA_ARFLAGS) $(obj-y))),$$(strip $$(subst $$(comma),:,$$(EXTRA_ARFLAGS) $$(obj-y))))' ; \
	    echo 'FILES_FLAGS_UP_TO_DATE += $@' ; \
	    echo 'endif' \
	) > $(dir $@)/.$(notdir $@).flags
endif


#
# This make dependencies quickly
#
fastdep: dummy
	$(TOPDIR)/scripts/mkdep $(CFLAGS) $(EXTRA_CFLAGS) -- $(wildcard *.[chS]) > .depend
ifdef ALL_SUB_DIRS
	$(MAKE) $(patsubst %,_sfdep_%,$(ALL_SUB_DIRS)) _FASTDEP_ALL_SUB_DIRS="$(ALL_SUB_DIRS)"
endif

ifdef _FASTDEP_ALL_SUB_DIRS
$(patsubst %,_sfdep_%,$(_FASTDEP_ALL_SUB_DIRS)):
	$(MAKE) -C $(patsubst _sfdep_%,%,$@) fastdep
endif

	
#
# A rule to make subdirectories
#
subdir-list = $(sort $(patsubst %,_subdir_%,$(SUB_DIRS)))
sub_dirs: dummy $(subdir-list)

ifdef SUB_DIRS
$(subdir-list) : dummy
	$(MAKE) -C $(patsubst _subdir_%,%,$@)
endif

#
# A rule to do nothing
#
dummy:

#
# This is useful for testing
#
script:
	$(SCRIPT)

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

ifneq ($(wildcard $(TOPDIR)/.hdepend),)
include $(TOPDIR)/.hdepend
endif

#
# Find files whose flags have changed and force recompilation.
# For safety, this works in the converse direction:
#   every file is forced, except those whose flags are positively up-to-date.
#
FILES_FLAGS_UP_TO_DATE :=

# For use in expunging commas from flags, which mung our checking.
comma = ,

FILES_FLAGS_EXIST := $(wildcard .*.flags)
ifneq ($(FILES_FLAGS_EXIST),)
include $(FILES_FLAGS_EXIST)
endif

FILES_FLAGS_CHANGED := $(strip \
    $(filter-out $(FILES_FLAGS_UP_TO_DATE), \
	$(O_TARGET) $(L_TARGET) $(active-objs) \
	))

# A kludge: .S files don't get flag dependencies (yet),
#   because that will involve changing a lot of Makefiles.  Also
#   suppress object files explicitly listed in $(IGNORE_FLAGS_OBJS).
#   This allows handling of assembly files that get translated into
#   multiple object files (see arch/ia64/lib/idiv.S, for example).
FILES_FLAGS_CHANGED := $(strip \
    $(filter-out $(patsubst %.S, %.o, $(wildcard *.S) $(IGNORE_FLAGS_OBJS)), \
    $(FILES_FLAGS_CHANGED)))

ifneq ($(FILES_FLAGS_CHANGED),)
$(FILES_FLAGS_CHANGED): dummy
endif
