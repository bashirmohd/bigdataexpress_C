#! /bin/bash

set -eu

BASEDIR=$(dirname $(readlink -f "$0"))
cd ${BASEDIR}

sudo /usr/local/mdtmftp+/current/sbin/mdtm-ftp-server \
     -control-interface 134.75.125.79 \
     -data-interface 192.2.2.9 \
     -p 5001 \
     -password-file /home/sctest/bde/etc/mdtmftp+/password \
     -c /home/sctest/bde/etc/mdtmftp+/server.conf \
     -l /tmp/mdtmftp.log \
     -log-level all
