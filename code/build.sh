#!/bin/sh

subdirs="core util test"

for i in $subdirs; do
	(cd $i && make $1 && cd ..) || exit;
done

