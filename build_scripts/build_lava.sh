#!/bin/bash

set -e # exit on first error

CPU_CORES=$(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')

LAVA_HOME_DIR=/opt/lava

if [[ -z "${LAVA_BUILD_TYPE}" ]]; then
	LAVA_BUILD_TYPE=DEBUG
fi

LAVA_BUILD_DIR=$PWD/build
LAVA_BUILD_DEPS_DIR=$PWD/deps
LAVA_3RDPARTY_SOURCE_DIR=$PWD/third_party

LAVA_INSTALL_DIR=/opt/lava
LAVA_3RDPARTY_INSTALL_DIR=$LAVA_INSTALL_DIR/3rdparty
LAVA_3RDPARTY_INC_INSTALL_DIR=$LAVA_3RDPARTY_INSTALL_DIR/include
LAVA_3RDPARTY_LIB_INSTALL_DIR=$LAVA_3RDPARTY_INSTALL_DIR/lib

#------------------------------------
# Required ubuntu packages
#------------------------------------

#sudo snap install cmake --classic
#sudo apt install -y libboost-all-dev libavcodec57 libavformat57 libswscale4 libdc1394-22-dev libgtk-3-dev libglfw3-dev libsdl2-dev libglew-dev libavformat-dev libswscale-dev vulkan-validationlayers vulkan-sdk clang \
#    libminizip-dev g++-multilib libzzip-dev libtiff-dev libgif-dev libopencolorio-dev libraw-dev libwebp-dev python3.7-dev libpython3.7-dev rapidjson-dev

export PYTHONPATH=${PYTHONPATH}:/usr/local/lib/python3.7/


#------------------------------------
# Lava deps
#------------------------------------
echo 'Building LAVA deps ...'

mkdir -p $LAVA_3RDPARTY_INSTALL_DIR
mkdir -p $LAVA_3RDPARTY_INC_INSTALL_DIR
mkdir -p $LAVA_3RDPARTY_LIB_INSTALL_DIR

#cd $LAVA_3RDPARTY_SOURCE_DIR/boost_1_75_0
#./bootstrap.sh --prefix=$LAVA_3RDPARTY_INSTALL_DIR
#./b2 install


cd $LAVA_3RDPARTY_SOURCE_DIR/zlib
export INCDIR=$LAVA_3RDPARTY_INC_INSTALL_DIR
./configure --prefix=$LAVA_3RDPARTY_INSTALL_DIR --includedir=$LAVA_3RDPARTY_INC_INSTALL_DIR --libdir=$LAVA_3RDPARTY_LIB_INSTALL_DIR && make -j$CPU_CORES all && make install

cd $LAVA_3RDPARTY_SOURCE_DIR/FreeImage
export INCDIR=$LAVA_3RDPARTY_INC_INSTALL_DIR
export INSTALLDIR=$LAVA_3RDPARTY_LIB_INSTALL_DIR
make -j$CPU_CORES install

cd $LAVA_3RDPARTY_SOURCE_DIR/Imath
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=${LAVA_3RDPARTY_INSTALL_DIR} && make -j$CPU_CORES install

cd $LAVA_3RDPARTY_SOURCE_DIR/OpenEXR
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=${LAVA_3RDPARTY_INSTALL_DIR} && make -j$CPU_CORES install

cd $LAVA_3RDPARTY_SOURCE_DIR/pybind11
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=${LAVA_3RDPARTY_INSTALL_DIR} && make -j$CPU_CORES install

cd $LAVA_3RDPARTY_SOURCE_DIR/OpenCV
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=${LAVA_3RDPARTY_INSTALL_DIR} -DCMAKE_PREFIX_PATH=${LAVA_3RDPARTY_INSTALL_DIR} && make -j$CPU_CORES install

cd $LAVA_3RDPARTY_SOURCE_DIR/OpenImageIO
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=${LAVA_3RDPARTY_INSTALL_DIR} && make -j$CPU_CORES install

cd $LAVA_3RDPARTY_SOURCE_DIR/assimp
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=${LAVA_3RDPARTY_INSTALL_DIR} && make -j$CPU_CORES install

cd $LAVA_3RDPARTY_SOURCE_DIR/slang
git submodule update --init
mkdir -p bin
$LAVA_3RDPARTY_SOURCE_DIR/premake5 gmake --cc=clang --os=linux
if [ "$LAVA_BUILD_TYPE" == "RELEASE" ]
then
	make config=release_x64 && make -j$CPU_CORES
	rsync $LAVA_3RDPARTY_SOURCE_DIR/slang/bin/linux-x64/release/*.so ${LAVA_3RDPARTY_LIB_INSTALL_DIR}/
else 
	make config=debug_x64 && make -j$CPU_CORES
	rsync $LAVA_3RDPARTY_SOURCE_DIR/slang/bin/linux-x64/debug/*.so ${LAVA_3RDPARTY_LIB_INSTALL_DIR}/
fi

#------------------------------------
# Lava
#------------------------------------

echo 'Building LAVA ...'
mkdir -p $LAVA_HOME_DIR
mkdir -p $LAVA_BUILD_DIR
cd $LAVA_BUILD_DIR

cmake .. -DCMAKE_BUILD_TYPE=${LAVA_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${LAVA_HOME_DIR} -DCMAKE_PREFIX_PATH=${LAVA_3RDPARTY_INSTALL_DIR} -DCMAKE_IGNORE_PATH=/opt/houdini*
#	-DOpenEXR_ROOT=${LAVA_3RDPARTY_INSTALL_DIR} \
#	-DZLIB_ROOT=${LAVA_3RDPARTY_INSTALL_DIR} \
#	-DFREEIMAGE_ROOT_DIR=${LAVA_3RDPARTY_INSTALL_DIR}

make -j$CPU_CORES install
