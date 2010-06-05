#!/bin/sh

test $# -ge 2 || { echo "Syntax: $0 SRCTREE OBJTREE"; exit 1; }

# cd to objtree
cd -- "$2" || { echo "Syntax: $0 SRCTREE OBJTREE"; exit 1; }

srctree="$1"

# (Re)generate include/applets.h
src="$srctree/include/applets.src.h"
dst="include/applets.h"
s=`sed -n 's@^//applet:@@p' -- "$srctree"/*/*.c "$srctree"/*/*/*.c`
echo "/* DO NOT EDIT. This file is generated from applets.src.h */" >"$dst.$$.tmp"
# Why "IFS='' read -r REPLY"??
# This atrocity is needed to read lines without mangling.
# IFS='' prevents whitespace trimming,
# -r suppresses backslash handling.
while IFS='' read -r REPLY; do
	test x"$REPLY" = x"INSERT" && REPLY="$s"
	printf "%s\n" "$REPLY"
done <"$src" >>"$dst.$$.tmp"
if test -f "$dst" && cmp -s "$dst.$$.tmp" "$dst"; then
	rm -- "$dst.$$.tmp"
else
	echo "  GEN     $dst"
	mv -- "$dst.$$.tmp" "$dst"
fi

# (Re)generate include/usage.h
src="$srctree/include/usage.src.h"
dst="include/usage.h"
# We add line continuation backslash after each line,
# and insert empty line before each line which doesn't start
# with space or tab
# (note: we need to use \\\\ because of ``)
s=`sed -n -e 's@^//usage:\([ \t].*\)$@\1 \\\\@p' -e 's@^//usage:\([^ \t].*\)$@\n\1 \\\\@p' -- "$srctree"/*/*.c "$srctree"/*/*/*.c`
echo "/* DO NOT EDIT. This file is generated from usage.src.h */" >"$dst.$$.tmp"
# Why "IFS='' read -r REPLY"??
# This atrocity is needed to read lines without mangling.
# IFS='' prevents whitespace trimming,
# -r suppresses backslash handling.
while IFS='' read -r REPLY; do
	test x"$REPLY" = x"INSERT" && REPLY="$s"
	printf "%s\n" "$REPLY"
done <"$src" >>"$dst.$$.tmp"
if test -f "$dst" && cmp -s "$dst.$$.tmp" "$dst"; then
	rm -- "$dst.$$.tmp"
else
	echo "  GEN     $dst"
	mv -- "$dst.$$.tmp" "$dst"
fi

# (Re)generate */Kbuild and */Config.in
find -type d | while read -r d; do
	d="${d#./}"
	src="$srctree/$d/Kbuild.src"
	dst="$d/Kbuild"
	if test -f "$src"; then
		#echo "  CHK     $dst"

		s=`sed -n 's@^//kbuild:@@p' -- "$srctree/$d"/*.c`

		echo "# DO NOT EDIT. This file is generated from Kbuild.src" >"$dst.$$.tmp"
		while IFS='' read -r REPLY; do
			test x"$REPLY" = x"INSERT" && REPLY="$s"
			printf "%s\n" "$REPLY"
		done <"$src" >>"$dst.$$.tmp"
		if test -f "$dst" && cmp -s "$dst.$$.tmp" "$dst"; then
			rm -- "$dst.$$.tmp"
		else
			echo "  GEN     $dst"
			mv -- "$dst.$$.tmp" "$dst"
		fi
	fi

	src="$srctree/$d/Config.src"
	dst="$d/Config.in"
	if test -f "$src"; then
		#echo "  CHK     $dst"

		s=`sed -n 's@^//config:@@p' -- "$srctree/$d"/*.c`

		echo "# DO NOT EDIT. This file is generated from Config.src" >"$dst.$$.tmp"
		while IFS='' read -r REPLY; do
			test x"$REPLY" = x"INSERT" && REPLY="$s"
			printf "%s\n" "$REPLY"
		done <"$src" >>"$dst.$$.tmp"
		if test -f "$dst" && cmp -s "$dst.$$.tmp" "$dst"; then
			rm -- "$dst.$$.tmp"
		else
			echo "  GEN     $dst"
			mv -- "$dst.$$.tmp" "$dst"
		fi
	fi
done

# Last read failed. This is normal. Don't exit with its error code:
exit 0
