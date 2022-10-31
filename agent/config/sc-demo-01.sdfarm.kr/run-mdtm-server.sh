#! /bin/bash

set -eux

BASEDIR=$(dirname $(readlink -f "$0"))
cd ${BASEDIR}

/usr/local/mdtmftp+/builds/HEAD/sbin/mdtm-ftp-server

