#!/bin/bash
# The list of all applet config symbols
test -f include/applets.h || { echo "No include/applets.h file"; exit 1; }
apps="`
grep ^IF_ include/applets.h \
| grep -v ^IF_FEATURE_ \
| sed 's/IF_\([A-Z0-9._-]*\)(.*/\1/' \
| sort | uniq
`"

# Take existing config
test -f .config || { echo "No .config file"; exit 1; }
cfg="`cat .config`"

# Make a config with all applet symbols off
allno="$cfg"
for app in $apps; do
	allno="`echo "$allno" | sed "s/^CONFIG_${app}=y\$/# CONFIG_${app} is not set/"`"
done
#echo "$allno" >.config_allno

test $# = 0 && set -- $apps

mintext=999999999
for app; do
	b="busybox_${app}"
	test -f "$b" || continue
	text=`size "$b" | tail -1 | sed -e's/\t/ /g' -e's/^ *//' -e's/ .*//'`
	#echo "text from $app: $text"
	test x"${text//[0123456789]/}" = x"" || {
		echo "Can't get: size $b"
		exit 1
	}
	test $mintext -gt $text && {
		mintext=$text
		echo "New mintext from $app: $mintext"
	}
	eval "text_${app}=$text"
done

for app; do
	b="busybox_${app}"
	test -f "$b" || continue
	eval "text=\$text_${app}"
	echo "$app adds $((text-mintext))"
done
