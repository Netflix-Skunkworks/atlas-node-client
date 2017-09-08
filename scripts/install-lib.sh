#!/bin/bash

if [ -d nc/root/usr/local/include/atlas/atlas_client.h ]; then
  echo Already installed
  exit 0
fi

#NATIVE_CLIENT_VERSION=${1:-v2.2.0}
#rm -rf nc
#mkdir nc
cd nc
#git init 
#git remote add origin https://github.com/Netflix-Skunkworks/atlas-native-client.git
#git fetch origin $NATIVE_CLIENT_VERSION
#git reset --hard FETCH_HEAD
#mkdir build root
cd build
cmake -DCMAKE_INSTALL_PREFIX=/ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
make install DESTDIR=../root
cp ../root/lib/libatlas* ../../build/Release
