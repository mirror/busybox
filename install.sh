#!/bin/sh

set -e
set -x
prefix=$1
if [ "$prefix" = "" ]; then
    echo "No installation directory, aborting."
    exit 1;
fi
if [ "$2" = "--hardlinks" ]; then
    linkopts="-f"
else
    linkopts="-fs"
fi
h=`sort busybox.links | uniq`


rm -f $prefix/bin/busybox
mkdir -p $prefix/bin
install -m 755 busybox $prefix/bin/busybox

for i in $h ; do
	appdir=`dirname $i`
	mkdir -p $prefix/$appdir
	if [ "$2" = "--hardlinks" ]; then
	    bb_path="$prefix/bin/busybox"
	else
	    case "$appdir" in
		/)
		    bb_path="bin/busybox"
		;;
		/bin)
		    bb_path="busybox"
		;;
		/sbin)
		    bb_path="../bin/busybox"
		;;
		/usr/bin|/usr/sbin)
		    bb_path="../../bin/busybox"
		;;
		*)
		echo "Unknown installation directory: $appdir"
		exit 1
		;;
	    esac
	fi
	echo "  $prefix$i -> /bin/busybox"
	ln $linkopts $bb_path $prefix$i
done

exit 0
