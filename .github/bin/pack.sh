#!/bin/bash

source .github/bin/common.sh

cd $workdir
tar -czf ../${target_pkg_name} *
