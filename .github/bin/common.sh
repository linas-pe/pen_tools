#!/bin/bash

os=$(echo "${RUNNER_OS}" | awk '{print tolower($0)}')
proj_name=$(basename "${GITHUB_REPOSITORY}")

branch_name=$(basename "${GITHUB_REF_NAME}")
if [ "$branch_name" == "merge" ]; then
    branch_name=$(basename "${GITHUB_BASE_REF}")
fi

workdir="${GITHUB_WORKSPACE}/${target_name}"

export os proj_name branch_name workdir

