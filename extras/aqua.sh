#!/bin/bash

CWD=$(cd)
cd ~

BASE="./software/"
AQUA="$BASE/AqualinkD"

echoerr() { printf "%s\n" "$*" >&2; }

function print_usage {
  echoerr "No arguments supplied, please use use:-"
  echoerr "$0 install    <- Install from scratch, delete any existing install (except configuration files)"
  echoerr "$0 upgrade    <- Upgrade existing install"
  echoerr "$0 clean      <- Remove everything, including configuration files"
  echoerr ""
}

function git_install {
  sudo apt-get install git
}

function remove {
  cd ~

  if [ -d "$AQUA" ]; then
    rm -rf "$AQUA"
  fi
}

function new_install {
  remove

  cd ~

  if [ ! -d "$BASE" ]; then
    mkdir -p $BASE
  fi

  cd $BASE
  git clone https://github.com/sfeakes/AqualinkD.git
}

function upgrade {
  cd ~
  cd $AQUA
  git pull
}

function clean {
  cd ~
  sudo "$AQUA/release/install.sh clean"
  remove
}

function install {
  cd ~
  sudo "$AQUA/release/install.sh"
}

# Check something was passed
if [[ $# -eq 0 ]]; then
    print_usage
    exit
fi

# Check git is installed
command -v git >/dev/null 2>&1 || { echoerr "git is not installed.  Installing"; git_install; }
command -v git >/dev/null 2>&1 || { echoerr "git is not installed.  Aborting"; exit 1; }

# Pass command line

if [ "$1" == "install" ]; then
  new_install
elif [ "$1" == "upgrade" ]; then
  upgrade
elif [ "$1" == "clean" ]; then
  clean
  exit
else
  print_usage
  exit;
fi

install

cd $CWD