#!/bin/sh

subdirs="ecp util test"

for i in $subdirs; do
	(cd $i && make $1 && cd ..) || exit;
done

