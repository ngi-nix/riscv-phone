#!/bin/sh

if [ -z $1 ]; then
    ARG="all"
else
    ARG=$1
fi
PLATFORM=posix

cd src/ecp
if [ "$ARG" != "clean" ]; then
    make platform=$PLATFORM clean
fi
make platform=$PLATFORM $ARG || exit
if [ "$ARG" ==  "all" ]; then
    make platform=$PLATFORM install
fi

cd ../../util
if [ "$ARG" != "clean" ]; then
    make platform=$PLATFORM clean
fi
make platform=$PLATFORM $ARG || exit
cd ../test
if [ "$ARG" != "clean" ]; then
    make platform=$PLATFORM clean
fi
make platform=$PLATFORM $ARG || exit
