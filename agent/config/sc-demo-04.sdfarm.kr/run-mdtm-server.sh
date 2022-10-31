#! /bin/bash

set -e

# BASEDIR=$(dirname "$0")

BASEDIR=$(dirname $(readlink -f "$0"))

cd ${BASEDIR}

export LD_LIBRARY_PATH=${BASEDIR}/lib:${BASEDIR}/lib64:${LD_LIBRARY_PATH}

${BASEDIR}/bin/mdtm-ftp-server

