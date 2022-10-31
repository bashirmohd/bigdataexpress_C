#! /bin/bash

set -eu

BASEDIR=$(dirname $(readlink -f "$0"))
cd ${BASEDIR}

BDECONFIG=${BASEDIR}/etc/agent.$(hostname).json

if [ ! -f ${BDECONFIG} ]; then
    echo "Config file ${BDECONFIG} not found."
    exit 1
fi

sudo -E ${BASEDIR}/bin/bdeagent \
     -c ${BDECONFIG} 2>&1 | \
    tee ${BASEDIR}/log/bdeagent-$(date +"%Y%m%d-%H%M%S-%Z").log
