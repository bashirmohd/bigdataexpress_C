#! /bin/bash

set -eu

# This script will install "version-1" branches of mdtm middleware and
# mdtmftp+ under INSTALL_PATH.

# TODO: merge this script with install-mdtm.sh.

MDTMFTP_START_POINT=HEAD
MDTM_START_POINT=HEAD

OPTION_REMOVE_GIT_WORKTREE="no"

while getopts t:r option
do
    case "${option}"
    in
        t) MDTMFTP_START_POINT=${OPTARG};;
        r) OPTION_REMOVE_GIT_WORKTREE="yes"
    esac
done

echo "Building mdtmftp version-1 (${MDTMFTP_START_POINT}) with mdtm (${MDTM_START_POINT}). Proceed?"
read -p "y/N > " yes_or_no

if [ ! $yes_or_no == 'y' ]; then
    echo "Aborting."
    exit
fi

if [ "x${OPTION_REMOVE_GIT_WORKTREE}" = "xyes" ]; then
    echo "This will delete mdtmftp version-1 git work trees in the end. OK?"
    read -p "y/N > " yes_or_no2
    if [ ! $yes_or_no2 = 'y' ]; then
        echo "Aborting."
        exit
    fi
fi

# Get kerberos tokens if we don't have them already.
kinit -R 2> /dev/null || kinit

INSTALL_PATH=/usr/local/mdtmftp+/version-1/builds/${MDTMFTP_START_POINT}
MDTM_SRC_PATH=${HOME}/src/mdtm/mdtm-2.0.0

MDTMFTP_GIT_SRC_REPO=ssh://p-mdtm-app-gt@cdcvs.fnal.gov/cvs/projects/mdtm-app-gt
MDTMFTP_GIT_SRC_DIR=${HOME}/src/mdtm/mdtm-app-gt

MDTM_GIT_SRC_REPO=ssh://p-mdtm4fnal@cdcvs.fnal.gov/cvs/projects/mdtm4fnal
MDTM_GIT_SRC_PATH=${HOME}/src/mdtm/mdtm

RABBITMQ_C_PATH=/usr/local/rabbitmq-c/0.8.0
JSON_C_PATH=/usr/local/json-c/0.12.1

# if [ ! -d ${MDTM_SRC_PATH} ]; then
#     echo "${MDTM_SRC_PATH} not found"
#     exit
# fi

# if [ ! -d ${MDTMFTP_SRC_PATH} ]; then
#     echo "${MDTMFTP_SRC_PATH} not found"
#     exit
# fi

if [ ! -d ${RABBITMQ_C_PATH} ]; then
    echo "${RABBITMQ_C_PATH} not found"
    exit
fi

if [ ! -d ${JSON_C_PATH} ]; then
    echo "${JSON_C_PATH} not found"
    exit
fi

export C_INCLUDE_PATH=${INSTALL_PATH}/include:${RABBITMQ_C_PATH}/include:${JSON_C_PATH}/include
export LD_LIBRARY_PATH=${INSTALL_PATH}/lib:${RABBITMQ_C_PATH}/lib:${JSON_C_PATH}/lib

if [ -d /tmp/mdtm ]; then
    sudo rm -rf /tmp/mdtm
fi

if [ ! -z ${MDTM_START_POINT} ] ; then
    if [ ! -d ${MDTM_GIT_SRC_PATH} ] ; then
        git clone ${MDTM_GIT_SRC_REPO} ${MDTM_GIT_SRC_PATH}
    fi
    cd ${MDTM_GIT_SRC_PATH}
    git checkout version-1
    git clean -f -d
    git reset --hard
    git fetch --all --tags
    git pull --rebase

    # We're creating separate "build" branches (because reasons) for
    # mdtm and mdtmftp+.  We delete any build branch exists.

    # Normally we should be able to use 'git rev-parse' or 'git
    # show-ref' to check the presence of a local branch, but 'set -e'
    # in this shell script has made things complicated.

    if [ -f .git/refs/heads/${MDTM_START_POINT}-build-branch ]; then
        git branch -D ${MDTM_START_POINT}-build-branch
    else
        echo "Branch ${MDTM_START_POINT}-build-branch does not exist"
    fi

    git checkout -B ${MDTM_START_POINT}-build-branch ${MDTM_START_POINT} --track

    mdtm_last_log=$(git log -n1)
    ./configure --prefix=${INSTALL_PATH}
    make
    make install
else
    cd ${MDTM_SRC_PATH}
    make distclean
    ./configure --prefix=${INSTALL_PATH}
    make
    make install
fi

if [ ! -z ${MDTMFTP_START_POINT} ]; then
    if [ ! -d ${MDTMFTP_GIT_SRC_DIR} ]; then
        git clone ${MDTMFTP_GIT_SRC_REPO} ${MDTMFTP_GIT_SRC_DIR}
    fi
    cd ${MDTMFTP_GIT_SRC_DIR}
    # Bring things to a clean state.  The build artifacts are left
    # around, and it seems to be problematic for git when updating.
    git checkout version-1  # switch to version-1 branch
    git clean -f -d
    git reset --hard
    git fetch --all --prune
    git pull --rebase

    # When in doubt, do: "git show-ref --tags -d"

    if [ -f .git/refs/heads/${MDTMFTP_START_POINT}-build-branch ]; then
        echo "Deleting ${MDTMFTP_START_POINT}-build-branch"
        git branch -D ${MDTMFTP_START_POINT}-build-branch
    else
        echo "Branch ${MDTMFTP_START_POINT}-build-branch does not exist"
    fi

    git checkout -B ${MDTMFTP_START_POINT}-build-branch ${MDTMFTP_START_POINT}

    mdtmftp_last_log=$(git log -n1)
else
    cd ${MDTMFTP_SRC_PATH}
fi
# make distclean
./configure --prefix=${INSTALL_PATH}
make mdtm_ftp_server mdtm_ftp_client
# sudo ln -sfn ${INSTALL_PATH} /usr/local/mdtmftp+/version-1/current

if [ "x${OPTION_REMOVE_GIT_WORKTREE}" = "xyes" ]; then
    rm -rf ${MDTM_GIT_SRC_PATH}
    rm -rf ${MDTMFTP_GIT_SRC_DIR}
fi

echo "------------------------------------------------------------"
echo "Built mdtm ${MDTM_START_POINT}"
echo "Last log: ${mdtm_last_log}"
echo "------------------------------------------------------------"
echo "Built mdtmftp+ ${MDTMFTP_START_POINT}"
echo "Last log: ${mdtmftp_last_log}"
echo "------------------------------------------------------------"
