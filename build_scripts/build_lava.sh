#!/bin/bash

CPU_CORES=$(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')
LAVA_BUILD_DIR=$PWD/build
LAVA_BUILD_DEPS_DIR=$PWD/deps
LAVA_THIRD_PARTY_DIR=$PWD/third_party

LAVA_INSTALL_DIR=/opt/lava
LAVA_INSTALL_INC_DIR=$LAVA_INSTALL_DIR/include
LAVA_INSTALL_LIBS_DIR=$LAVA_INSTALL_DIR/lib

echo 'Building LAVA deps ...'

mkdir -p $LAVA_INSTALL_INC_DIR
mkdir -p $LAVA_INSTALL_LIBS_DIR

cd $LAVA_THIRD_PARTY_DIR/zlib
./configure --includedir=$LAVA_INSTALL_INC_DIR --libdir=$LAVA_INSTALL_LIBS_DIR && make -j$CPU_CORES install

cd $LAVA_THIRD_PARTY_DIR/FreeImage
export INCDIR=$LAVA_INSTALL_INC_DIR
export INSTALLDIR=$LAVA_INSTALL_LIBS_DIR
make -j$CPU_CORES install

echo 'Building LAVA ...'
mkdir -p $LAVA_BUILD_DIR
cd $LAVA_BUILD_DIR

#cmake .. -DCMAKE_BUILD_TYPE=DEBUG
#make -j$CPU_CORES install
