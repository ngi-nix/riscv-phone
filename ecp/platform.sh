#!/bin/sh
#

BASEDIR=$(dirname $0)

if [ -z $1 ]; then
  echo usage $0 "<platform>"
  exit 1
fi

PLATFORM=$1

rm -f ${BASEDIR}/src/platform
rm -f ${BASEDIR}/src/platform.mk
ln -sf ../platform/${PLATFORM} ${BASEDIR}/src/platform
ln -sf ../platform/${PLATFORM}.mk ${BASEDIR}/src/platform.mk
