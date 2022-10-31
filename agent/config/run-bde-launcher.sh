#! /bin/bash

#
# A script to launch Launcher Agent with the right parameters.
#
# This script assumes that:
#
# (1) the necessary keys and certificates are present in ${BDEDIR};
# (2) bdeagent binary is present in ${BDEDIR}/bin; (3) a Launcher
# Agent configuration is present in ${BDEDIR}/etc/launcher.json; and
# (4) a directory named ${BDEDIR}/log/ is available for writing log
# files.
#

set -eu pipefail

BASEDIR=$(dirname $(readlink -f "$0"))
cd ${BASEDIR}

while /bin/true ; do
    ${BASEDIR}/bin/bdeagent -c ${BASEDIR}/etc/launcher.json | \
        tee ${BASEDIR}/log/launcher-$(date +"%Y%m%d-%H%M%S-%Z").log
done
