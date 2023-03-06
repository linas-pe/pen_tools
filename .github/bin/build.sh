#!/bin/bash

source .github/bin/common.sh

build_type=$(basename "$0")
build_args="-B build -S . -DPEN_LIBRARY_PATH=${GITHUB_WORKSPACE}/libpen"
if [ "$build_type" == "build_dev.sh" ]; then
    build_args="${build_args} -DCMAKE_BUILD_TYPE=Debug -DPEN_LOCAL_GTEST=ON"
else
    build_args="${build_args} -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${workdir}"
fi

cmake ${build_args} || exit 1

if [ "$os" == "windows" ]; then
    echo not build for windows
else
    make -C build VERBOSE=1 || exit 1
fi

if [ "$build_type" == "build.sh" ]; then
    if [ "$os" == "windows" ]; then
        echo no install for windows
    else
        make -C build install || exit 1
    fi
fi

