#!/usr/bin/env bash

rm -fr nanolibs/*.a
mkdir -p nanolibs
for file in $NANOLIBS_PATH; do
   ln -s "$file" nanolibs
done
for file in nanolibs/*.a; do
   mv "$file" "${file%%.a}_nano.a"
done
