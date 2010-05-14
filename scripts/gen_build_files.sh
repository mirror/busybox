#!/bin/sh

test $# -ge 2 || { echo "Syntax: $0 SRCTREE OBJTREE"; exit 1; }

# cd to objtree
cd -- "$2" || { echo "Syntax: $0 SRCTREE OBJTREE"; exit 1; }

srctree="$1"

find -type d | while read -r; do
	d="$REPLY"

	src="$srctree/$d/Kbuild.src"
	dst="$d/Kbuild"
	if test -f "$src"; then
		echo "  CHK     $dst"

		s=`sed -n 's@^//kbuild:@@p' -- "$srctree/$d"/*.c`
		echo "# DO NOT EDIT. This file is generated from Kbuild.src" >"$dst.$$.tmp"
		while read -r; do
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
		echo "  CHK     $dst"

		s=`sed -n 's@^//config:@@p' -- "$srctree/$d"/*.c`
		echo "# DO NOT EDIT. This file is generated from Config.src" >"$dst.$$.tmp"
		while read -r; do
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
