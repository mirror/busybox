#!/bin/sh

# hush's stderr with leak debug enabled
output=output

freelist=`grep 'free 0x' "$output" | cut -d' ' -f2 | sort | uniq | xargs`

grep -v free "$output" >"$output.leaked"

i=16
list=
for freed in $freelist; do
    list="$list -e $freed"
    test $((--i)) != 0 && continue
    echo Dropping $list
    grep -F -v $list <"$output.leaked" >"$output.temp"
    mv "$output.temp" "$output.leaked"
    i=16
    list=
done
if test "$list"; then
    echo Dropping $list
    grep -F -v $list <"$output.leaked" >"$output.temp"
    mv "$output.temp" "$output.leaked"
fi

# All remaining allocations are on addresses which were never freed.
# * Sort them by line, grouping together allocations which allocated the same address.
#	A leaky allocation will give many different addresses (because it's never freed,
#	the address can not be reused).
# * Remove the address (field #4).
# * Count the allocations per every unique source line and alloc type.
# * Show largest counts on top.
cat output.leaked \
	| sort -u \
	| cut -d' ' -f1-3 \
	| uniq -c \
	| sort -rn \
>output.leaked.counted_uniq_alloc_address
