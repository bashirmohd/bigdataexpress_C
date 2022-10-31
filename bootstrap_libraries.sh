#! /bin/bash

# exit on error
set -e

# root path
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

mkdir -p downloads
mkdir -p install

# clear the install path
cd $DIR/install
rm -rf *

# clear the downloads path
cd $DIR/downloads
rm -rf *

# rabbitmq-c
cd $DIR/downloads

# mongo c driver
cd $DIR/downloads

#git clone https://github.com/mongodb/mongo-c-driver.git
#cd mongo-c-driver
#git checkout 1.3.0-dev

#wget https://github.com/mongodb/mongo-c-driver/releases/download/1.3.5/mongo-c-driver-1.3.5.tar.gz
#tar xf mongo-c-driver-1.3.5.tar.gz
#cd mongo-c-driver-1.3.5

#wget https://github.com/mongodb/mongo-c-driver/releases/download/1.7.0/mongo-c-driver-1.7.0.tar.gz
#tar xf mongo-c-driver-1.7.0.tar.gz
#cd mongo-c-driver-1.7.0

wget https://github.com/mongodb/mongo-c-driver/releases/download/1.8.1/mongo-c-driver-1.8.1.tar.gz
tar xf mongo-c-driver-1.8.1.tar.gz
cd mongo-c-driver-1.8.1

./configure --prefix=$DIR/install --enable-static --disable-sasl
make -j 4
make install


# mongo c++ driver
cd $DIR/downloads

rm -rf mongo-cxx-driver
git clone -b master https://github.com/mongodb/mongo-cxx-driver

cd mongo-cxx-driver
#git checkout r3.0.0
git checkout r3.1.3

cd build
PKG_CONFIG_PATH=$DIR/install/lib/pkgconfig cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$DIR/install ..
make -j 4
make install

# libcurl
cd $DIR/downloads
wget https://curl.haxx.se/download/curl-7.49.1.tar.bz2
tar xf curl-7.49.1.tar.bz2
cd curl-7.49.1
./configure --prefix=$DIR/install --without-libssh2 --disable-ldap --disable-ldaps
make -j 4
make install

# curlcpp
cd $DIR/downloads
rm -rf curlcpp
git clone -b 1.1 https://github.com/JosephP91/curlcpp.git
mkdir curlcpp/build
cd curlcpp/build
cmake -DCURL_INCLUDE_DIR=$DIR/install/include -DCMAKE_INSTALL_PREFIX=$DIR/install ..
make -j 4
make install

# mqtt mosquitto
echo "Install mosquitto"

cd ${DIR}/downloads
git clone https://github.com/eclipse/mosquitto.git
cd mosquitto
git checkout tags/v1.5.5 -b v1.5.5
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=${DIR}/install -DWITH_TLS=ON -DWITH_SRV=no -DDOCUMENTATION=no -DWITH_STATIC_LIBRARIES=ON ..
make -j 4
make install

# asio
echo "Installing asio"

cd ${DIR}/downloads
wget -O asio-1.11.0.tar.bz2 https://sourceforge.net/projects/asio/files/asio/1.11.0%20%28Development%29/asio-1.11.0.tar.bz2/download

tar xf asio-1.11.0.tar.bz2
cp ${DIR}/downloads/asio-1.11.0/include/asio.hpp ${DIR}/install/include/asio.hpp
cp -r ${DIR}/downloads/asio-1.11.0/include/asio ${DIR}/install/include/

# Install util-linux, which provides libmount.
cd $DIR/downloads && \
    wget "https://www.kernel.org/pub/linux/utils/util-linux/v2.29/util-linux-2.29.tar.xz" && \
    tar xvf util-linux-2.29.tar.xz && \
    cd util-linux-2.29 && \
    ./configure --prefix=$DIR/install \
                --disable-all-programs \
                --disable-shared \
                --enable-static \
                --enable-libblkid \
                --enable-libmount \
                --disable-libuuid \
                --disable-libsmartcols \
                --disable-libfdisk \
                --without-systemd \
                --without-python && \
    make install

# return to root dir
cd $DIR

