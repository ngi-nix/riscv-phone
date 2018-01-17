#!/bin/sh
#

BASEDIR=$(dirname $0)

if [ -z $1 ]; then
  echo usage $0 "<platform>"
  exit 1
fi

PLATFORM=$1

rm -f $BASEDIR/platform
ln -sf ./$PLATFORM $BASEDIR/platform
ln -sf ./config_$PLATFORM.h $BASEDIR/config.h
ln -sf ./Makefile.$PLATFORM $BASEDIR/Makefile.platform
