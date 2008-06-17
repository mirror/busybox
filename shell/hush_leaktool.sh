#!/bin/sh

# hush's stderr with leak debug enabled
output=output

freelist=`grep 'free 0x' "$output" | cut -d' ' -f2 | sort | uniq | xargs`

grep -v free "$output" >temp1
for freed in $freelist; do
    echo Dropping $freed
    grep -v $freed <temp1 >temp2
    mv temp2 temp1
done
