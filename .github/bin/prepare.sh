#!/bin/bash

source .github/bin/common.sh

if [ ! -f "$depfile" ]; then
    exit 0
fi

.github/bin/ftp_tool.sh -d

while read line
do
    filename="$line-$branch_name-$os.tar.gz"
    sudo tar xf $filename -C /usr
    if [ $? -ne 0 ]; then
        exit 1
    fi
    if [ $line == 'pen_utils' ]; then
        ln -s /usr/include/pen_utils/pen.m4 .
    fi
done < $depfile

