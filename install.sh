#!/bin/sh

set -e
set -x
if [ "$1" = "" ]; then
    echo "No installation directory, aborting."
    exit 1;
fi
if [ "$2" = "--hardlinks" ]; then
    linkopts="-f"
else
    linkopts="-fs"
fi
prefix=$1
h=`sort busybox.links | uniq`


rm -f $1/bin/busybox
mkdir -p $1/bin
install -m 755 busybox $1/bin/busybox

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
