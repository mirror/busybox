#!/bin/sh
# Called from top-level directory a-la
#
# scripts/generate_BUFSIZ.sh include/common_bufsiz.h

. ./.config || exit 1

debug=false

common_bufsiz_h=$1

test x"$NM" = x"" && NM="${CONFIG_CROSS_COMPILER_PREFIX}nm"
test x"$CC" = x"" && CC="${CONFIG_CROSS_COMPILER_PREFIX}gcc"

regenerate() {
	cat >"$1.$$"
	test -f "$1" && diff "$1.$$" "$1" >/dev/null && rm "$1.$$" && return
	mv "$1.$$" "$1"
}

generate_std_and_exit() {
	$debug && echo "Default: bb_common_bufsiz1[] in bss"
	{
	echo "enum { COMMON_BUFSIZE = 1024 };"
	echo "extern char bb_common_bufsiz1[];"
	echo "#define setup_common_bufsiz() ((void)0)"
	} | regenerate "$common_bufsiz_h"
	exit 0
}

# User does not want any funky stuff?
test x"$CONFIG_FEATURE_USE_BSS_TAIL" = x"y" || generate_std_and_exit

test -f busybox_unstripped || {
	# We did not try anything yet
	$debug && echo "Will try to fit bb_common_bufsiz1[] into _end[]"
	{
	echo "enum { COMMON_BUFSIZE = 1024 };"
	echo "extern char _end[]; /* linker-provided label */"
	echo "#define bb_common_bufsiz1 _end"
	echo "#define setup_common_bufsiz() ((void)0)"
	} | regenerate "$common_bufsiz_h"
	echo 1024 >"$common_bufsiz_h.BUFSIZE"
	exit 0
}

# Get _end address
END=`$NM busybox_unstripped | grep ' . _end$'| cut -d' ' -f1`
test x"$END" = x"" && generate_std_and_exit
$debug && echo "END:0x$END $((0x$END))"
END=$((0x$END))

# Get PAGE_SIZE
echo "\
#include <sys/user.h>
#if defined(PAGE_SIZE) && PAGE_SIZE > 0
char page_size[PAGE_SIZE];
#else
char page_size[1];
#endif
" >page_size_$$.c
$CC -c "page_size_$$.c" || generate_std_and_exit
PAGE_SIZE=`$NM --size-sort "page_size_$$.o" | cut -d' ' -f1`
rm "page_size_$$.c" "page_size_$$.o"
test x"$PAGE_SIZE" = x"" && generate_std_and_exit
$debug && echo "PAGE_SIZE:0x$PAGE_SIZE $((0x$PAGE_SIZE))"
PAGE_SIZE=$((0x$PAGE_SIZE))
test $PAGE_SIZE -lt 1024 && generate_std_and_exit

# How much space between _end[] and next page?
PAGE_MASK=$((PAGE_SIZE-1))
REM=$(( (-END) & PAGE_MASK ))
$debug && echo "REM:$REM"

if test $REM -lt 1024; then
	# _end[] has no enough space for bb_common_bufsiz1[],
	# users will need to malloc it.
	{
	echo "enum { COMMON_BUFSIZE = 1024 };"
	echo "extern char *bb_common_bufsiz1;"
	echo "void setup_common_bufsiz(void);"
	} | regenerate "$common_bufsiz_h"
	# Check that we aren't left with a buggy binary:
	if test -f "$common_bufsiz_h.BUFSIZE"; then
		rm "$common_bufsiz_h.BUFSIZE"
		echo "Warning! Space in _end[] is too small ($REM bytes)!"
		echo "Rerun make to build a binary which doesn't use it!"
		exit 1
	fi
        exit 0
fi

# _end[] has REM bytes for bb_common_bufsiz1[]
OLD=1024
test -f "$common_bufsiz_h.BUFSIZE" && OLD=`cat "$common_bufsiz_h.BUFSIZE"`
$debug && echo "OLD:$OLD"
{
echo "enum { COMMON_BUFSIZE = $REM };"
echo "extern char _end[]; /* linker-provided label */"
echo "#define bb_common_bufsiz1 _end"
echo "#define setup_common_bufsiz() ((void)0)"
} | regenerate "$common_bufsiz_h"
echo $REM >"$common_bufsiz_h.BUFSIZE"

# Check that code did not grow too much and thus _end[] did not shink:
if test $OLD -gt $REM; then
	echo "Warning! Space in _end[] has decreased from $OLD to $REM bytes!"
	echo "Rerun make!"
	exit 1
fi

if test $OLD != $REM; then
	echo "Space in _end[] is $REM bytes. Rerun make to use larger COMMON_BUFSIZE."
fi
