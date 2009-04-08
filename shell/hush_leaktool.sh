#!/bin/sh

# hush's stderr with leak debug enabled
output=output

freelist=`grep 'free 0x' "$output" | cut -d' ' -f2 | sort | uniq | xargs`

grep -v free "$output" >"$output.leaked"
for freed in $freelist; do
    echo Dropping $freed
    grep -v $freed <"$output.leaked" >"$output.temp"
    mv "$output.temp" "$output.leaked"
done
