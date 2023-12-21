#!/bin/sh

#
# a shell script to clone and compile hamlib
#

if [ ! -d hamlib-local ]; then
    git clone https://github.com/Hamlib/Hamlib.git hamlib-local
fi
cd hamlib-local
git pull
git checkout 4.5.5

./bootstrap
./configure --without-readline --without-libusb --disable-winradio --enable-static --disable-shared

make
