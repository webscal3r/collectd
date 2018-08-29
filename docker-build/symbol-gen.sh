#!/bin/bash

scriptdir=`dirname ${0}`
scriptdir=`(cd ${scriptdir}; pwd)`
scriptname=`basename ${0}`

set -e

function errorexit()
{
  errorcode=${1}
  shift
  echo $@
  exit ${errorcode}
}

function usage()
{
  echo "USAGE ${scriptname} <tostripdir> <debugdir>"
}

tostripdir=$1
debugdir=$2

if [ -z ${tostripdir} ] ; then
  usage
  errorexit 0 "tostripdir must be specified"
fi

if [ -z ${debugdir} ] ; then
  usage
  errorexit 0 "debugdir must be specified"
fi
cd $debugdir
for tostripfile in $(find $tostripdir -type f -and -executable -or -name "*.so*" | grep -v ".la")
do
  filename=`basename $tostripfile`
  debugfile="${filename}.debug"
  echo "stripping ${tostripfile}, putting debug info into ${debugdir}/${debugfile}"
  objcopy --only-keep-debug "${tostripfile}" "${debugdir}/${debugfile}"
  strip --strip-debug --strip-unneeded "${tostripfile}"
  objcopy --add-gnu-debuglink="${debugfile}" "${tostripfile}"
  chmod -x "${debugdir}/${debugfile}"
done
