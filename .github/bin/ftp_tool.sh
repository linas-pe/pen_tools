#!/bin/bash

source .github/bin/common.sh

URL="https://${PEN_FILE_SERVER}/linas"


function try_curl() {
  code="-1"
  result=0
  for i in $(seq 1 5); do
    code=$(curl $@)
    result=$?
    if [ "$result" -eq 0 ]; then
      break
    fi
  done
  if [ "$result" -ne 0 ]; then
    exit 1
  fi
  echo "$code"
}

function upload() {
  echo "upload ${target_pkg_name}"
  args="branch=${branch_name}&secret=${UPLOAD_SECRET}"
  curl_args="--retry 5 -o /dev/null -F libpen=@${target_pkg_name}"
  code=$(try_curl -sw "%{http_code}" ${curl_args} "${URL}/upload/libpen?${args}")
  if [ "$code" != "200" ]; then
    exit 1
  fi
  exit 0
}

function download() {
  if [ ! -f "$depfile" ]; then
    exit 0
  fi
  mkdir -p libpen
  while read -r item
  do
    item="$(echo -e "$item" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
    if [ -n "$item" ]; then
      target="${item}-${branch_name}-${os}.${pen_ext}"
      echo "Download ${target} ..."
      try_curl --retry 5 "${URL}/app/${branch_name}/${target}" -o "${target}"
      if [ "$os" == "windows" ]; then
        7z x "${target}" -olibpen -aoa
      else
        tar xf "${target}" -C libpen || exit 1
      fi
      if [ "${item}" == "pen_utils" ]; then
        mkdir -p include
        ln -s "${GITHUB_WORKSPACE}/libpen/include/pen_utils/pen.cmake" include
      fi
    fi
  done < $depfile
  exit 0
}

function download_googletest() {
    target="googletest-${os}.${pen_ext}"
    echo "Download ${target} ..."
    mkdir -p googletest
    try_curl --retry 5 "${URL}/app/${target}" -o "${target}"
    if [ "$os" == "windows" ]; then
        7z x "${target}" -ogoogletest -aoa
    else
        tar xf "${target}" -C googletest || exit 1
    fi
    exit 0
}

function usage() {
  echo "Usage: ftp_tool.sh [-u | -d]"
  exit 1
}

if [ $# -ne 1 ]; then
  usage
fi

if [ "$1" == "-u" ]; then
  upload
fi

if [ "$1" == "-g" ]; then
  download_googletest
fi

if [ "$1" != "-d" ]; then
  usage
fi

download

