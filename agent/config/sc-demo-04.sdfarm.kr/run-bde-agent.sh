#! /bin/bash

set -eux

BASEDIR=$(dirname $(readlink -f "$0"))
cd ${BASEDIR}

sudo -E ${BASEDIR}/bin/bdeagent -c ${BASEDIR}/etc/agent.json 2>&1 | tee ${BASEDIR}/log/bdeagent-$(date +"%Y%m%d-%H%M%S-%Z").log

