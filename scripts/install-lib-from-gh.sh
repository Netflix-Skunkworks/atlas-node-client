#!/bin/bash

set -e
if [ -r nc/root/include/atlas/atlas_client.h ]; then
  echo Already installed
  cp nc/root/lib/libatlas* build/Release
  exit 0
fi

if [ $# = 0 ] ; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi
NATIVE_CLIENT_VERSION=$1

rm -rf nc
mkdir -p nc
cd nc
git init 
git remote add origin https://github.com/Netflix-Skunkworks/atlas-native-client.git
git fetch origin $NATIVE_CLIENT_VERSION
git reset --hard FETCH_HEAD
mkdir -p build root
cd build
cmake -DCMAKE_INSTALL_PREFIX=/ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j8 
make install DESTDIR=../root
rm -f ../root/lib/libatlas*a
cp ../root/lib/libatlas* ../../build/Release
