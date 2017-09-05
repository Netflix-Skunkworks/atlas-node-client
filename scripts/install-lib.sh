#!/bin/bash

OS=$(uname -s)
if [ "$OS" = "Linux" ]; then
  # see if package already installed
  V=$(dpkg -l libatlasclient | awk '/^ii/{print $3}')
  case $V in
    2.2.* )
      # assume the rest of the dependencies have been installed
      echo libatlasclient already installed: $V
      ;;
    * )
      # same package name, but the xenial ami doesn't ship g++
      ubuntu=$(lsb_release -c -s)
      if [ ! -r /etc/apt/sources.list.d/nflx-$ubuntu.list ]; then
        echo "deb     [arch=amd64] http://repo.test.netflix.net:7001/artifactory/debian-local $ubuntu main" | sudo tee /etc/apt/sources.list.d/nflx-$ubuntu.list
      fi
      sudo apt-get update
      sudo apt-get install -y --allow-unauthenticated libatlasclient g++ 
      ;;
  esac
  mkdir -p build/Release
  rm -f build/Release/libatlasclient.dylib
  cp /usr/local/lib/libatlasclient.so build/Release/
elif [ "$OS" = "Darwin" ]; then
  brew tap | grep -q homebrew/nflx || brew tap homebrew/nflx https://stash.corp.netflix.com/scm/brew/nflx.git
  if brew ls --versions nflx-atlas-client | grep -q "2\.2" ; then
    echo nflx-atlas-client already installed: $(brew ls --versions nflx-atlas-client)
  else
    brew update
    if brew ls --versions nflx-atlas-client >/dev/null 2>/dev/null; then
      brew upgrade nflx-atlas-client || true
    else
      brew install nflx-atlas-client
    fi
  fi
  mkdir -p build/Release
  rm -f build/Release/libatlasclient.dylib
  cp /usr/local/lib/libatlasclient.dylib build/Release/
fi
