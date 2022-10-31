#! /bin/bash

set -eu

# ------------------------------------------------------------------------
#
# This script will install mdtm middleware and mdtmftp+ under
# INSTALL_PREFIX (see below).  Of course, depending on what you want
# to do, you may want to change some variables below.
#
# This script makes assumptions about the presence of necessary tools:
# C and C++ compilers, Git, autoconf, automake, libtool, and friends.
#
# Before installing mdtm4fnal and mdtmftp+, install build tools and
# dependencies:
#
#  - autoconf, automake, libtool, m4, gcc, gcc-c++
#
#  - numactl-devel, hwloc-devel, lvm2-devel, mosquitto-devel,
#    openssl-devel, json-c-devel, libuuid-devel, rrdtool-devel,
#    krb5-devel, libtool-ltdl-devel, ncurses-devel
#
# OpenSSL has to be some 1.0.x version.  Newer versions (OpenSSL
# 1.1.x) will not work yet, because of the API changes.
#
# On Scientific Linux 7 (and CentOS 7, and other RHEL 7 spins) the
# working OpenSSL package be simply "openssl-devel"; on newer RedHat
# systems this would be "compat-openssl10-devel".
#
# On Debian 8/9 and spins, install libssl-dev; on newer Debian
# releases (Ubuntu 18.04, for example), install libssl1.0-dev.
#
# ------------------------------------------------------------------------

# MDTM and MDTMFTP+ branches/commits that we're going to build.

MDTM_BRANCH=master
MDTM_COMMIT=HEAD

MDTMFTP_BRANCH=mdtmftp+
MDTMFTP_COMMIT=HEAD

# ------------------------------------------------------------------------

# Bash command line parsing is not... very good; we gotta live with
# what we got.

OPTION_GET_FROM_GIT="yes"
OPTION_REMOVE_GIT_WORKTREE="no"

while getopts t:nr option
do
    case "${option}"
    in
        t) MDTMFTP_COMMIT=${OPTARG};;
        n) OPTION_GET_FROM_GIT="no";;
        r) OPTION_REMOVE_GIT_WORKTREE="yes";;
    esac
done

# ------------------------------------------------------------------------

# Give names to build and build branches.

# TODO: append "git rev-parse --short ${MDTMFTP_COMMIT}" to build name?
BUILD_NAME=build-${MDTMFTP_BRANCH}-${MDTMFTP_COMMIT}-$(date "+%Y%m%d")
INSTALL_PREFIX=/usr/local/mdtmftp+/builds/${BUILD_NAME}

MDTM_BUILD_BRANCH=${BUILD_NAME}
MDTMFTP_BUILD_BRANCH=${BUILD_NAME}

# ------------------------------------------------------------------------

# Details about Git repositories.

GIT_REPO_ROOT=ssh://p-mdtm4fnal@cdcvs.fnal.gov/cvs/projects
MDTM_GIT_SRC_REPO=${GIT_REPO_ROOT}/mdtm4fnal
MDTMFTP_GIT_SRC_REPO=${GIT_REPO_ROOT}/mdtm-app-gt

# ------------------------------------------------------------------------

# Details about where we clone the Git repositories.

BUILD_DIR=/usr/src/mdtm
MDTM_GIT_SRC_PATH=${BUILD_DIR}/mdtm4fnal
MDTMFTP_GIT_SRC_PATH=${BUILD_DIR}/mdtm-app-gt

# ------------------------------------------------------------------------

ColorRed='\033[0;31m'
ColorPurple='\033[0;35m'
ColorNone='\033[0m'

# ------------------------------------------------------------------------

printf "Building mdtmftp+ (branch: ${ColorPurple}${MDTMFTP_BRANCH}${ColorNone}, \
commit: ${ColorPurple}${MDTMFTP_COMMIT}${ColorNone}) \
with mdtm (branch: ${ColorPurple}${MDTM_BRANCH}${ColorNone}, \
commit: ${ColorPurple}${MDTM_COMMIT}${ColorNone}).
Install prefix will be ${ColorRed}${INSTALL_PREFIX}${ColorNone}.
Proceed?\n"

read -p "y/N > " yes_or_no

if [ ! $yes_or_no == 'y' ]; then
    echo "Aborting."
    exit
fi

if [ "x${OPTION_REMOVE_GIT_WORKTREE}" = "xyes" ]; then
    echo "This will delete mdtmftp+ git work trees in the end. OK?"
    read -p "y/N > " yes_or_no2
    if [ ! $yes_or_no2 = 'y' ]; then
        echo "Aborting."
        exit
    fi
fi

# ------------------------------------------------------------------------

BUILD_START_TIME=$(date "+%s")

# ------------------------------------------------------------------------

if [ ! -d ${BUILD_DIR} ]; then
    echo "Creating build directory at ${BUILD_DIR}."
    sudo mkdir -p ${BUILD_DIR}
    sudo chown $USER:$GROUPS ${BUILD_DIR}
fi

# ------------------------------------------------------------------------

if [ -z "${C_INCLUDE_PATH+x}" ]; then
    export C_INCLUDE_PATH=${INSTALL_PREFIX}/include
else
    export C_INCLUDE_PATH=${C_INCLUDE_PATH}:${INSTALL_PREFIX}/include
fi

if [ -z "${LD_LIBRARY_PATH+x}"]; then
    export LD_LIBRARY_PATH=${INSTALL_PREFIX}/lib
else
    export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${INSTALL_PREFIX}/lib
fi

if [ -z "${PKG_CONFIG_PATH+x}" ]; then
    export PKG_CONFIG_PATH=${INSTALL_PREFIX}/lib/pkgconfig
else
    export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:${INSTALL_PREFIX}/lib/pkgconfig
fi

if [ -d /usr/lib64/pkgconfig ]; then
    export PKG_CONFIG_PATH=${PKG_CONFIG_PATH:+$PKG_CONFIG_PATH:}/usr/lib64/pkgconfig
fi

# ------------------------------------------------------------------------

# The build cannot proceed if someone else has built mdtmftp+ in this
# system already.  I do not know what this is about.

if [ -d /tmp/mdtm ]; then
    sudo rm -rf /tmp/mdtm
fi

# ------------------------------------------------------------------------

# Check out mdtm4fnal sources and do... a bunch of stuff.

function get_mdtm_git_sources
{
    if [ ! -z ${MDTM_COMMIT} ] ; then

        if [ ! -d ${MDTM_GIT_SRC_PATH} ] ; then
            git clone ${MDTM_GIT_SRC_REPO} ${MDTM_GIT_SRC_PATH}
        fi

        cd ${MDTM_GIT_SRC_PATH}

        git checkout ${MDTM_BRANCH}
        git clean -f -d
        git reset --hard
        git fetch --all --tags
        git pull --rebase

        # We'll create separate "build" branches (because reasons) for
        # mdtm and mdtmftp+.  If there's an existing branch by the
        # same exists, we'll delete it.

        # Normally we should be able to use 'git rev-parse' or 'git
        # show-ref' to check the presence of a local branch, but 'set
        # -e' in this shell script has made things complicated.

        if [ -f .git/refs/heads/${MDTM_BUILD_BRANCH} ]; then
            echo "Branch ${MDTM_BUILD_BRANCH} exists; deleting."
            git branch -D ${MDTM_BUILD_BRANCH}
        else
            echo "Branch ${MDTM_BUILD_BRANCH} does not exist"
        fi

        git checkout -B ${MDTM_BUILD_BRANCH} ${MDTM_COMMIT}

    else
        cd ${MDTM_GIT_SRC_PATH}
        make distclean
    fi
}

# ------------------------------------------------------------------------

# Check out mdtmftp+ sources and do... a bunch of stuff.

function get_mdtmftp_git_sources
{
    if [ ! -z ${MDTMFTP_COMMIT} ]; then

        if [ ! -d ${MDTMFTP_GIT_SRC_PATH} ]; then
            git clone ${MDTMFTP_GIT_SRC_REPO} ${MDTMFTP_GIT_SRC_PATH}
        fi

        cd ${MDTMFTP_GIT_SRC_PATH}

        # Bring things to a clean state.  The build artifacts are left
        # around, and it seems to be problematic for git when updating.
        git checkout ${MDTMFTP_BRANCH}
        git clean -f -d
        git reset --hard
        git fetch --all --prune
        git pull --rebase

        # When in doubt, do: "git show-ref --tags -d"

        if [ -f .git/refs/heads/${MDTMFTP_BUILD_BRANCH} ]; then
            echo "Deleting ${MDTMFTP_BUILD_BRANCH}"
            git branch -D ${MDTMFTP_BUILD_BRANCH}
        else
            echo "Branch ${MDTMFTP_BUILD_BRANCH} does not exist"
        fi

        git checkout -B ${MDTMFTP_BUILD_BRANCH} ${MDTMFTP_COMMIT}

    else
        cd ${MDTMFTP_GIT_SRC_PATH}
    fi
}

# ------------------------------------------------------------------------

if [ "x${OPTION_GET_FROM_GIT}" = "xyes" ]; then
    # Get kerberos tokens if we don't have them already.
    echo "May need Kerberos credentials for Git repository access."
    klist -s || kinit

    get_mdtm_git_sources
    get_mdtmftp_git_sources
fi

if [ ! -d ${MDTM_GIT_SRC_PATH} ] ; then
    printf "${ColorRed}Error: ${MDTM_GIT_SRC_PATH} does not exist.${ColorNone}\n"
    exit 1
fi

if [ ! -d ${MDTMFTP_GIT_SRC_PATH} ] ; then
    printf "${ColorRed}Error: ${MDTMFTP_GIT_SRC_PATH} does not exist.${ColorNone}\n"
    exit 1
fi


# ------------------------------------------------------------------------

# Make install path, if it doesn't exist.
if [ ! -d ${INSTALL_PREFIX} ]; then
    sudo mkdir -p ${INSTALL_PREFIX}
    sudo chown ${USER}: ${INSTALL_PREFIX}
fi

# ------------------------------------------------------------------------

cd ${MDTM_GIT_SRC_PATH}

MDTM_LAST_LOG=$(git log -n1)

if [ -x ./autogen.sh ] ; then
    ./autogen.sh
fi

./configure --prefix=${INSTALL_PREFIX}
make clean
make install

# ------------------------------------------------------------------------

cd ${MDTMFTP_GIT_SRC_PATH}

MDTMFTP_LAST_LOG=$(git log -n1)

if [ -x ./autogen.sh ] ; then
    ./autogen.sh
    ./configure --prefix=${INSTALL_PREFIX}
    make clean
    make install
else
    ./configure --prefix=${INSTALL_PREFIX}
    make mdtm_ftp_server mdtm_ftp_client
fi

# ------------------------------------------------------------------------

BUILD_END_TIME=$(date "+%s")

# ------------------------------------------------------------------------

# Make a symbolic link to install path.
ln -sfn ${INSTALL_PREFIX} /usr/local/mdtmftp+/current

# ------------------------------------------------------------------------

if [ "x${OPTION_REMOVE_GIT_WORKTREE}" = "xyes" ]; then
    rm -rf ${MDTM_GIT_SRC_PATH}
    rm -rf ${MDTMFTP_GIT_SRC_PATH}
fi

# ------------------------------------------------------------------------

echo "------------------------------------------------------------"
echo "Built mdtm."
echo
echo "-- Sources at:   ${MDTM_GIT_SRC_PATH}"
echo "-- Branch:       ${MDTM_BRANCH}"
echo "-- Commit:       ${MDTM_COMMIT}"
echo "-- Build branch: ${MDTM_BUILD_BRANCH}"
echo
echo "Last log: ${MDTM_LAST_LOG}"
echo "------------------------------------------------------------"
echo "Built mdtmftp+."
echo
echo "-- Sources at:   ${MDTMFTP_GIT_SRC_PATH}"
echo "-- Branch:       ${MDTMFTP_BRANCH}"
echo "-- Commit:       ${MDTMFTP_COMMIT}"
echo "-- Build branch: ${MDTMFTP_BUILD_BRANCH}"
echo
echo "Last log: ${MDTMFTP_LAST_LOG}"
echo "------------------------------------------------------------"
echo "Build took $((${BUILD_END_TIME} - ${BUILD_START_TIME})) seconds."
echo "------------------------------------------------------------"

# ------------------------------------------------------------------------
