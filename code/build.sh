#!/bin/sh

subdirs="core vconn util test"

for i in $subdirs; do
	(cd $i && make $1 && cd ..) || exit;
done

