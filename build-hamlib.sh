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

if [ ! -f config.log  -o ! -f Makefile ]; then
./bootstrap
autoreconf -i
./configure --without-readline --without-libusb --disable-winradio --enable-static --disable-shared
fi

make

#
# Test presence of certain  files
#
if [ ! -f include/hamlib/rig.h ]; then
  echo /rig.h NOT FOUND!
  exit 8
fi
if [ ! -f include/hamlib/config.h ]; then
  echo config.h NOT FOUND
  exit 8
fi
if [ ! -f src/.libs/libhamlib.a ]; then
  echo libhamlib.a NOT FOUND
  exit 8
fi
