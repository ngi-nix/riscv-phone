#!/bin/sh

subdirs="src util test"

for i in $subdirs; do
	(cd $i && make $1 && cd ..) || exit;
done

