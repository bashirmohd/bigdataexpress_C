#! /bin/bash

#
# A script to launch BDE Agent with the right parameters.
#
# This script assumes that:
#
# (1) the necessary keys and certificates are present in ${BDEDIR};
# (2) bdeagent binary is present in ${BDEDIR}/bin; (3) a BDE Agent
# configuration is present in ${BDEDIR}/etc/agent.json; and (3) a
# directory named ${BDEDIR}/log/ is available for writing log files.
#

set -eu pipefail

BDEDIR=$(dirname $(readlink -f "$0"))
cd ${BDEDIR}

# The below LD_LIBRARY_PATH may be helpful sometimes.

# export LD_LIBRARY_PATH=/usr/local/mdtmftp+/current/lib64:\
#        /usr/local/mdtmftp+/current/lib

sudo -E ${BDEDIR}/bin/bdeagent -c ${BDEDIR}/etc/agent.json 2>&1 | \
    tee ${BDEDIR}/log/bdeagent-$(date +"%Y%m%d-%H%M%S-%Z").log
