#!/bin/bash

TRUE=0
FALSE=1

#REPO="https://api.github.com/repos/AqualinkD/AqualinkD"
REPO="https://api.github.com/repos/sfeakes/AqualinkD"

INSTALLED_BINARY="/usr/local/bin/aqualinkd"

REL_VERSION=""
DEV_VERSION=""
INSTALLED_VERSION=""

TEMP_INSTALL="/tmp/aqualinkd"
OUTPUT="/tmp/aqualinkd_upgrade.log"

# Remember not to use (check for terminal, as it may not exist when pipe to bash)
# ie.  if [ -t 0 ]; then  

log()
{ 
  echo $*
  logger -p local0.info -t aqualinkd_upgrade "$*"
  # Below is same as above but will only wotk on journald (leaving it here if we use that rater then file)
  #echo $* | systemd-cat -t aqualinkd_upgrade -p info 
  echo $* >> $OUTPUT
}

logerr()
{ 
  echo $* >&2
  logger -p local0.ERROR -t aqualinkd_upgrade "$*"
  # Below is same as above but will only wotk on journald (leaving it here if we use that rater then file)
  #echo $* | systemd-cat -t aqualinkd_upgrade -p emerg
  echo "ERROR: $*" >> $OUTPUT
}


function check_tool() {
  cmd=$1
  if ! command -v $cmd &>/dev/null
  then
    log "Command '$cmd' could not be found!"
    return $FALSE
  fi

  return $TRUE
}

function latest_release_version {
  REL_VERSION=$(curl --silent -L "$REPO/releases/latest" | grep -Po '"tag_name": "\K.*?(?=")')
  if [[ "$REL_VERSION" != "" ]]; then
    return $TRUE
  else
    return $FALSE
  fi
}

function latest_development_version {
  #DEV_VERSION=$(curl --silent -L "https://raw.githubusercontent.com/sfeakes/AqualinkD/master/version.h" | grep AQUALINKD_VERSION | cut -d '"' -f 2)
  DEV_VERSION=$(curl --silent -L -H "Accept: application/vnd.github.raw+json" "$REPO/contents/source/version.h" | grep AQUALINKD_VERSION | cut -d '"' -f 2)
  if [[ "$DEV_VERSION" != "" ]]; then
    return $TRUE
  else
    return $FALSE
  fi
}

function installed_version {
  if check_tool strings &&
     check_tool grep &&
     check_tool awk &&
     check_tool tr; then
    INSTALLED_VERSION=$(strings $INSTALLED_BINARY | grep sw_version | awk -v RS="," -v FS=":" '/sw_version/{print $2;exit;}' | tr -d ' "' )
    echo "Current installed version $INSTALLED_VERSION"
  fi

  if [[ "$INSTALLED_VERSION" != "" ]]; then
    return $TRUE
  else
    return $FALSE
  fi
}

function check_system_arch {
  ARCH=$(dpkg --print-architecture)
  BINEXT=""

  case $ARCH in 
    arm64 |\
    armhf)
      return $TRUE
    ;;
    *)
      logerr "System arch is $ARCH, this is not supported by AqualinkD"
      return $FALSE;
    ;;
  esac
}


function check_can_upgrade {
  version=$1
  output=""
  # Check we have needed commands.
  # curl, dpkg, systemctl
  if ! command -v curl &>/dev/null; then output+="Command 'curl' not found, please check it's installed and in path\n"; fi
  if ! command -v dpkg &>/dev/null; then output+="Command 'dpkg' not found, please check it's installed and in path\n"; fi
  if ! command -v systemctl &>/dev/null; then output+="Command 'systemctl' not found, please check it's installed and in path\n"; fi
  
  # Check root is rw
  if [[ $(mount | grep " / " | grep "(ro,") ]]; then output+="Root filesystem is readonly, can't upgrade"; fi

  # Check we can get the latest version
  if ! latest_release_version; then output+="Couldn't find latest version on github"; fi

   # Check we can get the latest version
  if command -v dpkg &>/dev/null; then
    if ! check_system_arch; then output+="System Architecture not supported!"; fi
  fi

  if [[ "$output" == "" ]] && [[ "$REL_VERSION" != "" ]]; then 
    return $TRUE
  else [[ "$output" != "" ]]
    logerr $output;
    return $FALSE
  fi

  return $TRUE
}


function download_latest_release {
  mkdir -p $TEMP_INSTALL
  tar_url=$(curl --silent -L "$REPO/releases/latest" | grep -Po '"tarball_url": "\K.*?(?=")')
  if [[ "$tar_url" == "" ]]; then return $FALSE; fi

  curl --silent -L "$tar_url" | tar xz --strip-components=1 --directory=$TEMP_INSTALL
  if [ $? -ne 0 ]; then return $FALSE; fi

  return $TRUE;
}

function download_latest_development {
  mkdir -p $TEMP_INSTALL
  tar_url="$REPO/tarball/master"
  curl --silent -L "$tar_url" | tar xz --strip-components=1 --directory=$TEMP_INSTALL
  if [ $? -ne 0 ]; then return $FALSE; fi

  return $TRUE;
}

function run_install_script {
  if [ ! -f "$TEMP_INSTALL/release/install.sh" ]; then
    logerr "Can not find install script $TEMP_INSTALL/release/install.sh"
    return $FALSE
  fi
  log "Installing AqualinkD $1"

  # If in terminal then log
  if [ -t 0 ]; then 
   "$TEMP_INSTALL/release/install.sh" &> $OUTPUT
  else
   "$TEMP_INSTALL/release/install.sh" &> $OUTPUT
  fi
}

function remove_install {
  curl -fsSL -H "Accept: application/vnd.github.raw+json" "$REPO/contents/install/install.sh" | sudo bash clean
}

function cleanup {
  rm -rf $TEMP_INSTALL
}


####################################################
#
#  Main
#  


if [[ $EUID -ne 0 ]]; then
   logerr "This script must be run as root" 
   exit 1
fi

#if [ ! -t 0 ]; then
  #Don't use log function here as we will cleanout the file if it exists.
  # Can't use $0 below as this script might be piped into bash from curl
  echo "upgrade.sh - `date` " > $OUTPUT
#fi

if check_can_upgrade; then
  installed_version
  if [[ "$INSTALLED_VERSION" != "" ]]; then
    log "Current AqualinkD installation $INSTALED_VERSION"
    log "System OK to upgrade AqualinkD to $REL_VERSION"
  else
    log "System OK to install AqualinkD $REL_VERSION"
  fi
  
  #exit $TRUE;
else
  logerr "Can not upgrade, Please fix error(s)!"
  exit $FALSE;
fi

echo "DEBUG, Exiting"
exit

case $1 in
  latest)
    if ! download_latest_release; then logerr "downloading latest"; exit $FALSE; fi
    run_install_script $REL_VERSION
    cleanup
  ;;
  development)
    if ! latest_development_version; then logerr "getting development version";exit $FALSE; fi
    if ! download_latest_development; then logerr "downloading latest development";exit $FALSE; fi
    run_install_script $REL_VERSION
    cleanup
  ;;
  # Add Delete / remove / clean.
  clean|delete|remove)
    if ! remove_install; then logerr "Removing install";exit $FALSE; fi
  ;;
  *)
    echo "ERROR Unknown"
  ;;
esac


exit



#################
# Junk

if my_function; then echo "Success"; else echo "Failure"; fi

case expression in
  check)
    
  ;;
  latest)
  ;;
esac

if [ "$1" == "check" ]; then
  check_can_upgrade $2
  if [[ $? -eq 0 ]]; then
    echo "OK to upgrade to $REL_VERSION"
    exit $TRUE;
  else
     echo "Can not upgrade, Please fix error(s)!"
    exit $FALSE;
  fi
fi


exit


check_can_upgrade

mkdir -p $TEMP_INSTALL
tar_url=$(curl --silent -L "$REPO/releases/latest" | grep -Po '"tarball_url": "\K.*?(?=")')
curl --silent -L "$tar_url" | tar xz --strip-components=1 --directory=$TEMP_INSTALL

if [ $? -ne 0 ]; then
  echo "ERROR: extracting files to $TEMP_INSTALL from $tar_url"
  exit $FALSE
fi

echo "Installing AqualinkD $REL_VERSION"
# Should be good to run install script.

rm -rf $TEMP_INSTALL

exit $TRUE
######

#Below will extract latest release into current directoy. the tar release from github has a crap root path, so strip it.
tar_url=$(curl --silent -L "$REPO/releases/latest" | grep -Po '"tarball_url": "\K.*?(?=")')
curl --silent -L "$tar_url" | tar xz --strip-components=1 --directory=.


# Latest Development
tar_url="https://api.github.com/repos/sfeakes/aqualinkd/tarball/master"
curl --silent -L "$tar_url" | tar xz --strip-components=1 --directory=.