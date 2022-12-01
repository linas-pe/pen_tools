#!/bin/bash

source .github/bin/common.sh

mkdir -p m4 build
cd m4; libtoolize; cd ..
aclocal
autoheader
automake --add-missing
autoconf

build_type=`basename $0`

cd build
if [ $build_type == 'build_dev.sh' ]; then
    ../configure  --enable-debug
else
    ../configure --prefix=/usr
fi
cd ..

make

if [ $? -ne 0 ]; then
    exit 1
fi

if [ $build_type == 'build.sh' ]; then
    make install prefix=${workdir}
fi

