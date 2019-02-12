#!/bin/bash

set -ex

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VER=`cat VERSION`
INSIGHT_VERSION="${VER}-${BUILD_NUMBER}"
debugoutput="insights-collectd-debug-${INSIGHT_VERSION}.tar.gz"
output="insights-collectd-${INSIGHT_VERSION}.tar.gz"
output_tar=$(basename $output .gz)

image=collectd-dse-bundle

cd ../; docker build --build-arg insight_version=${INSIGHT_VERSION} -t $image . 

cid=$(docker create $image true)
trap "docker rm -f $cid; rm -f $output_tar" EXIT

docker export $cid | tar --delete 'dev' 'proc' 'etc' 'sys' 'collectd-symbols' | gzip -f - > $output
docker export $cid | tar --delete 'dev' 'proc' 'etc' 'sys' 'collectd' | gzip -f - > $debugoutput
