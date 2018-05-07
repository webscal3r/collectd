#!/bin/bash

set -ex

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VER=`cat VERSION`
output="insights-collectd-${VER}-${BUILD_NUMBER}.tar.gz"
output_tar=$(basename $output .gz)

image=collectd-dse-bundle

#docker pull $image || true
cd ../; docker build -t $image . 

cid=$(docker create $image true)
trap "docker rm -f $cid; rm -f $output_tar" EXIT

docker export $cid | tar --delete 'dev' 'proc' 'etc' 'sys' | gzip -f - > $output
