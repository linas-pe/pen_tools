export os=`awk '{print tolower($0)}' <<< ${RUNNER_OS}`
export proj_name=`basename ${GITHUB_REPOSITORY}`

export branch_name=`basename ${GITHUB_REF_NAME}`
if [ $branch_name == "merge" ]; then
    branch_name=`basename ${GITHUB_BASE_REF}`
fi

export target_name=${proj_name}-${branch_name}-${os}
export target_pkg_name=${target_name}.tar.gz
export workdir=${GITHUB_WORKSPACE}/${target_name}
export depfile=.Pen.deps

