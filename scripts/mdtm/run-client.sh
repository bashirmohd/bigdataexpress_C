#! /bin/sh

set -e

RANDSTR=$(head -c 2 /dev/urandom | xxd -p)

COMMAND_QUEUE=mdtm-listen-${RANDSTR}
REPORT_QUEUE=mdtm-report-${RANDSTR}

echo "-----------------------------------------------------------"
echo " Client command queue : ${COMMAND_QUEUE}"
echo " Report queue         : ${REPORT_QUEUE}"
echo "-----------------------------------------------------------"

/usr/local/mdtmftp+/current/bin/mdtm-ftp-client -vb -p 4 -msgq yosemite.fnal.gov:8883:${COMMAND_QUEUE}:${REPORT_QUEUE}
