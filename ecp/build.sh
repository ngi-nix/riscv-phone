#!/bin/sh

if [ -z $1 ]; then
    ARG="all"
else
    ARG=$1
fi

cd src
make clean
if [ $ARG == "all" ]; then
    make || exit
    make install
fi
cd ../util
make clean
if [ $ARG == "all" ]; then
    make || exit
fi
cd ../test
make clean
if [ $ARG == "all" ]; then
    make
fi
