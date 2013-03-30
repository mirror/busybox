#!/bin/sh

# Seconds to try to reread partition table
cnt=60

exec </dev/null
exec >"/tmp/${0##*/}.$$.out"
exec 2>&1

(
echo "Running: $0"
echo "Env:"
env | sort

while sleep 1; test $cnt != 0; do
	echo "Trying to rereat partition table on $DEVNAME ($cnt)"
	: $((cnt--))
	test -e "$DEVNAME" || { echo "$DEVNAME doesn't exist, aborting"; exit 1; }
	#echo "$DEVNAME exists"
	blockdev --rereadpt "$DEVNAME" && break
	echo "blockdev --rereadpt failed, exit code: $?"
done
echo "blockdev --rereadpt succeeded"
) &
