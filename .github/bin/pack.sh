#!/bin/bash

source .github/bin/common.sh

if [ "$os" == "windows" ]; then
	7z a "${target_pkg_name}" ${workdir}/* -r -mx=9
else
	cd "$workdir" || exit 1
	tar -czf "../${target_pkg_name}" -- *
fi
