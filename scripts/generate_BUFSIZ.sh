#!/bin/sh
# Called from top-level directory a-la
#
# scripts/generate_BUFSIZ.sh include/common_bufsiz.h

. ./.config || exit 1

debug=false

postcompile=false
test x"$1" = x"--post" && { postcompile=true; shift; }

common_bufsiz_h=$1

test x"$NM" = x"" && NM="${CONFIG_CROSS_COMPILER_PREFIX}nm"
test x"$CC" = x"" && CC="${CONFIG_CROSS_COMPILER_PREFIX}gcc"

exitcmd="exit 0"

regenerate() {
	cat >"$1.$$"
	test -f "$1" && diff "$1.$$" "$1" >/dev/null && rm "$1.$$" && return
	mv "$1.$$" "$1"
}

generate_std_and_exit() {
	$debug && echo "Configuring: bb_common_bufsiz1[] in bss"
	{
	echo "enum { COMMON_BUFSIZE = 1024 };"
	echo "extern char bb_common_bufsiz1[];"
	echo "#define setup_common_bufsiz() ((void)0)"
	} | regenerate "$common_bufsiz_h"
	echo "std" >"$common_bufsiz_h.method"
	$exitcmd
}

generate_big_and_exit() {
	$debug && echo "Configuring: bb_common_bufsiz1[] in _end[], COMMON_BUFSIZE = $1"
	{
	echo "enum { COMMON_BUFSIZE = $1 };"
	echo "extern char _end[]; /* linker-provided label */"
	echo "#define bb_common_bufsiz1 _end"
	echo "#define setup_common_bufsiz() ((void)0)"
	} | regenerate "$common_bufsiz_h"
	echo "$2" >"$common_bufsiz_h.method"
	$exitcmd
}

generate_1k_and_exit() {
	generate_big_and_exit 1024 "1k"
}


generate_malloc_and_exit() {
	$debug && echo "Configuring: bb_common_bufsiz1[] is malloced"
	{
	echo "enum { COMMON_BUFSIZE = 1024 };"
	echo "extern char *const bb_common_bufsiz1;"
	echo "void setup_common_bufsiz(void);"
	} | regenerate "$common_bufsiz_h"
	echo "malloc" >"$common_bufsiz_h.method"
	$exitcmd
}

# User does not want any funky stuff?
test x"$CONFIG_FEATURE_USE_BSS_TAIL" = x"y" || generate_std_and_exit

# The script is run two times: before compilation, when it needs to
# (re)generate $common_bufsiz_h, and directly after successful build,
# when it needs to assess whether the build is ok to use at all (not buggy),
# and (re)generate $common_bufsiz_h for a future build.

if $postcompile; then
	# Postcompile needs to create/delete OK/FAIL files

	test -f busybox_unstripped || exit 1
	test -f "$common_bufsiz_h.method" || exit 1

	# How the build was done?
	method=`cat -- "$common_bufsiz_h.method"`

	# Get _end address
	END=`$NM busybox_unstripped | grep ' . _end$'| cut -d' ' -f1`
	test x"$END" = x"" && generate_std_and_exit
	$debug && echo "END:0x$END $((0x$END))"
	END=$((0x$END))

	# Get PAGE_SIZE
	{
	echo "#include <sys/user.h>"
	echo "#if defined(PAGE_SIZE) && PAGE_SIZE > 0"
	echo "char page_size[PAGE_SIZE];"
	echo "#endif"
	} >page_size_$$.c
	$CC -c "page_size_$$.c" || exit 1
	PAGE_SIZE=`$NM --size-sort "page_size_$$.o" | cut -d' ' -f1`
	rm "page_size_$$.c" "page_size_$$.o"
	test x"$PAGE_SIZE" = x"" && exit 1
	$debug && echo "PAGE_SIZE:0x$PAGE_SIZE $((0x$PAGE_SIZE))"
	PAGE_SIZE=$((0x$PAGE_SIZE))
	test $PAGE_SIZE -lt 512 && exit 1

	# How much space between _end[] and next page?
	PAGE_MASK=$((PAGE_SIZE-1))
	COMMON_BUFSIZE=$(( (-END) & PAGE_MASK ))
	echo "COMMON_BUFSIZE = $COMMON_BUFSIZE bytes"

	if test x"$method" = x"1k"; then
		if test $COMMON_BUFSIZE -lt 1024; then
			# _end[] has no enough space for bb_common_bufsiz1[]
			rm -- "$common_bufsiz_h.1k.OK" 2>/dev/null
			{ md5sum <.config | cut -d' ' -f1; stat -c "%Y" .config; } >"$common_bufsiz_h.1k.FAIL"
			echo "Warning! Space in _end[] is too small ($COMMON_BUFSIZE bytes)!"
			echo "Rerun make to build a binary which doesn't use it!"
			rm busybox_unstripped
			exitcmd="exit 1"
		else
			rm -- "$common_bufsiz_h.1k.FAIL" 2>/dev/null
			echo $COMMON_BUFSIZE >"$common_bufsiz_h.1k.OK"
			test $COMMON_BUFSIZE -gt $((1024+32)) && echo "Rerun make to use larger COMMON_BUFSIZE"
		fi
	fi
fi

# Based on past success/fail of 1k build, decide next build type

if test -f "$common_bufsiz_h.1k.OK"; then
	# Previous build succeeded fitting 1k into _end[].
	# Try bigger COMMON_BUFSIZE if possible.
	COMMON_BUFSIZE=`cat -- "$common_bufsiz_h.1k.OK"`
	# Round down a bit
	COMMON_BUFSIZE=$(( (COMMON_BUFSIZE-32) & 0xfffffe0 ))
	COMMON_BUFSIZE=$(( COMMON_BUFSIZE < 1024 ? 1024 : COMMON_BUFSIZE ))
	test $COMMON_BUFSIZE = 1024 && generate_1k_and_exit
	generate_big_and_exit $COMMON_BUFSIZE "big"
fi
if test -f "$common_bufsiz_h.1k.FAIL"; then
	# Previous build FAILED to fit 1k into _end[].
	# Was it with same .config?
	oldcfg=`cat -- "$common_bufsiz_h.1k.FAIL"`
	curcfg=`md5sum <.config | cut -d' ' -f1; stat -c "%Y" .config`
	# If yes, then build a "malloced" version
	if test x"$oldcfg" = x"$curcfg"; then
		echo "Will not try 1k build, it failed before. Touch .config to override"
		generate_malloc_and_exit
	fi
	# else: try 1k version
	echo "New .config, will try 1k build"
	rm -- "$common_bufsiz_h.1k.FAIL"
	generate_1k_and_exit
fi
# There was no 1k build yet. Try it.
generate_1k_and_exit
