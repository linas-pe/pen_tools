#!/bin/bash

source .github/bin/common.sh

URL=https://${PEN_FILE_SERVER}/linas

function upload() {
  args="branch=${branch_name}&secret=${UPLOAD_SECRET}"
  curl_args="-o /dev/null -F libpen=@${target_pkg_name}"
  code=`curl -sw '%{http_code}' ${curl_args} ${URL}/upload/libpen\?${args}`
  if [ $? -ne 0 ]; then
    exit $?
  fi
  if [ "$code" != "200" ]; then
    exit 1
  fi
  exit 0
}

function download() {
  while read item
  do
    if [ -n "$item" ]; then
      echo "Download ${item} ..."
      target=${item}-${branch_name}-${os}.tar.gz
      curl ${URL}/app/${branch_name}/${target} -o $target
      if [ $? -ne 0 ]; then
        exit $?
      fi
    fi
  done < $depfile
  exit 0
}

function usage() {
  echo "Usage: ftp_tool.sh [-u | -d]"
  exit 1
}

if [ $# -ne 1 ]; then
  usage
fi

if [ $1 == "-u" ]; then
  upload
fi

if [ $1 != "-d" ]; then
  usage
fi

download

