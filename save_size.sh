#!/bin/bash

set -e

if ! [ -n "$size_info_file" ]; then
    exit 0
fi

# separate calling commands and filling DB
SIZES=`$1 $2 | grep $2`
GIT_HASH=`git log --format=format:%H -1`

echo -n "$GIT_HASH " >> "$size_info_file"
echo -n "$DEFCONFIG_NAME ">> "$size_info_file"
echo $SIZES >> "$size_info_file"
