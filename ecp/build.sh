#!/bin/sh

if [ -z $1 ]; then
    ARG="all"
else
    ARG=$1
fi


if [ -z $MAKE ]; then
    MAKE=make
fi

PLATFORM=posix

cd src/ecp
if [ "$ARG" != "clean" ]; then
    $MAKE platform=$PLATFORM clean
fi
$MAKE platform=$PLATFORM $ARG || exit
if [ "$ARG" ==  "all" ]; then
    $MAKE platform=$PLATFORM install
fi

cd ../../util
if [ "$ARG" != "clean" ]; then
    $MAKE platform=$PLATFORM clean
fi
$MAKE platform=$PLATFORM $ARG || exit
cd ../test
if [ "$ARG" != "clean" ]; then
    $MAKE platform=$PLATFORM clean
fi
$MAKE platform=$PLATFORM $ARG || exit
