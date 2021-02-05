#!/bin/bash
#
# Copyright (C) 2020 Waldemar Kozaczuk
#
# This work is open source software, licensed under the terms of the
# BSD license as described in the LICENSE file in the top-level directory.
#

# Usage:
#  ./scripts/download_and_build_boost.sh <BOOST_VERSION> <ARCH> <CROSS_AARCH64_TOOLSET>
#

OSV_ROOT=$(dirname $(readlink -e $0))/..
echo $OSV_ROOT

BOOST_VERSION=$1
if [[ "${BOOST_VERSION}" == "" ]]; then
  BOOST_VERSION="1.70.0"
fi
BOOST_VERSION2=${BOOST_VERSION//[.]/_}

ARCH=$2
if [[ "${ARCH}" == "" ]]; then
  ARCH=$(uname -m)
fi

#If CROSS_AARCH64_TOOLSET is set or passed we assume we are crosscompiling aarch64 on Intel
CROSS_AARCH64_TOOLSET=$3

rm -rf %s/boost/install

TAR_DOWNLOAD_DIR=${OSV_ROOT}/build/downloaded_packages/${ARCH}/boost/upstream/
mkdir -p ${TAR_DOWNLOAD_DIR}

BOOST_URL=https://dl.bintray.com/boostorg/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION2}.tar.gz
wget -c -O ${TAR_DOWNLOAD_DIR}/boost_${BOOST_VERSION2}.tar.gz ${BOOST_URL}

pushd ${TAR_DOWNLOAD_DIR} 
rm -rf ./boost_${BOOST_VERSION2} && tar -xf ./boost_${BOOST_VERSION2}.tar.gz
cd ./boost_${BOOST_VERSION2}
./bootstrap.sh --with-libraries=system,thread,test,chrono,regex,date_time,filesystem,locale,random,atomic,log,program_options

BOOST_DIR=$(pwd)
case $CROSS_AARCH64_TOOLSET in
  gcc)
    echo "using gcc : arm : aarch64-linux-gnu-g++ ;" > user-config.jam ;;
  gcc-arm)
    echo "using gcc : arm : aarch64-none-linux-gnu-g++ ;" > user-config.jam ;;
esac

B2_OPTIONS="threading=multi"
if [[ "${CROSS_AARCH64_TOOLSET}" != "" ]]; then
  B2_OPTIONS="${B2_OPTIONS} --user-config=${BOOST_DIR}/user-config.jam toolset=${CROSS_AARCH64_TOOLSET} architecture=arm address-model=64"
fi
./b2 ${B2_OPTIONS} -j$(nproc)

#
# Create symlinks to install boost and make it visible to OSv makefile
rm -f ../../install
ln -s upstream/boost_${BOOST_VERSION2}/stage ../../install
mkdir -p stage/usr/include
ln -s ../../../boost stage/usr/include/boost
popd
