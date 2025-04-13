#!/bin/bash

#
# run from curl or local will give different results.
#  curl -fsSL https://raw.githubusercontent.com/aqualinkd/AqualinkD/master/release/remote_install.sh | sudo bash -s -- latest
#  ./upgrade.sh latest
#
# To get good / bad exit code from both curl and bash, use below. It will exit current term so be careful.
# curl -fsSL "https://raw.githubusercontent.com/aqualinkd/AqualinkD/master/release/remote_install.sh" | ( sudo bash && exit 0 ) || exit $?


TRUE=0
FALSE=1

REPO="https://api.github.com/repos/AqualinkD/AqualinkD"
#REPO="https://api.github.com/repos/sfeakes/AqualinkD"

INSTALLED_BINARY="/usr/local/bin/aqualinkd"

# Can't use $0 since this script is usually piped into bash
SELF="remote_install.sh"

REL_VERSION=""
DEV_VERSION=""
INSTALLED_VERSION=""

TEMP_INSTALL="/tmp/aqualinkd"
OUTPUT="/tmp/aqualinkd_upgrade.log"

FROM_CURL=$FASE

# Remember not to use (check for terminal, as it may not exist when pipe to bash)
# ie.  if [ -t 0 ]; then  

if command -v "systemd-cat" &>/dev/null; then SYSTEMD_LOG=$TRUE;fi

log()
{ 
  echo "$*"
  if [[ $SYSTEMD_LOG -eq $TRUE ]]; then
    echo "Upgrade:   $*" | systemd-cat -t aqualinkd -p info
  else
    logger -p local0.notice -t aqualinkd "Upgrade:   $*"
  fi
  echo "$*" 2>/dev/null >> "$OUTPUT"
}

logerr()
{ 
  echo "Error: $*" >&2
 
  if [[ $SYSTEMD_LOG -eq $TRUE ]]; then
    echo "Upgrade:   $*" | systemd-cat -t aqualinkd -p err
  else
    logger -p local0.error -t aqualinkd "Upgrade:   $*"
  fi
  
  echo "ERROR: $*" 2>/dev/null >> "$OUTPUT"
}


function check_tool() {
  cmd=$1
  if ! command -v "$cmd" &>/dev/null
  then
    log "Command '$cmd' could not be found!"
    return "$FALSE"
  fi

  return "$TRUE"
}

function latest_release_version {
  REL_VERSION=$(curl -fsSL "$REPO/releases/latest" | grep -Po '"tag_name": "\K.*?(?=")')
  if [[ "$REL_VERSION" != "" ]]; then
    return "$TRUE"
  else
    return "$FALSE"
  fi
}

function latest_development_version {
  #DEV_VERSION=$(curl --silent -L "https://raw.githubusercontent.com/sfeakes/AqualinkD/master/version.h" | grep AQUALINKD_VERSION | cut -d '"' -f 2)
  DEV_VERSION=$(curl -fsSL -H "Accept: application/vnd.github.raw" "$REPO/contents/source/version.h" | grep AQUALINKD_VERSION | cut -d '"' -f 2)
  if [[ "$DEV_VERSION" != "" ]]; then
    return "$TRUE"
  else
    return "$FALSE"
  fi
}

function installed_version {
  if [ -f "$INSTALLED_BINARY" ]; then
    if check_tool strings &&
       check_tool grep &&
       check_tool awk &&
       check_tool tr; then
      INSTALLED_VERSION=$(strings "$INSTALLED_BINARY" | grep sw_version | awk -v RS="," -v FS=":" '/sw_version/{print $2;exit;}' | tr -d ' "' )
      log "Current installed version $INSTALLED_VERSION"
    fi
  else
    log "AqualinkD is not installed"
  fi

  if [[ "$INSTALLED_VERSION" != "" ]]; then
    return "$TRUE"
  else
    return "$FALSE"
  fi
}

function check_system_arch {
  ARCH=$(dpkg --print-architecture)

  case $ARCH in 
    arm64 |\
    armhf)
      return "$TRUE"
    ;;
    *)
      logerr "System arch is $ARCH, this is not supported by AqualinkD"
      return "$FALSE";
    ;;
  esac
}


function check_can_upgrade {
  #version=$1
  output=""
  # Check we have needed commands.
  # curl, dpkg, systemctl
  if ! command -v curl &>/dev/null; then output+="Command 'curl' not found, please check it's installed and in path\n"; fi
  if ! command -v dpkg &>/dev/null; then output+="Command 'dpkg' not found, please check it's installed and in path\n"; fi
  if ! command -v systemctl &>/dev/null; then output+="Command 'systemctl' not found, please check it's installed and in path\n"; fi
  
  # Check root is rw
  if mount | grep " / " | grep -q "(ro,"; then
    # check if can remount rw.
    if mount / -o remount,rw &>/dev/null; then
      # can mount RW.
      mount / -o remount,ro &>/dev/null
    else
      output+="Root filesystem is readonly & failed to remount read write, can't upgrade";
    fi
  fi

  # Check we can get the latest version
  if ! latest_release_version; then output+="Couldn't find latest version on github"; fi

   # Check we can get the latest version
  if command -v dpkg &>/dev/null; then
    if ! check_system_arch; then output+="System Architecture not supported!"; fi
  fi

  if [[ "$output" == "" ]] && [[ "$REL_VERSION" != "" ]]; then 
    return "$TRUE"
  else [[ "$output" != "" ]]
    logerr "$output";
    return "$FALSE"
  fi

  return "$TRUE"
}


function download_latest_release {
  mkdir -p "$TEMP_INSTALL"
  tar_url=$(curl -fsSL "$REPO/releases/latest" | grep -Po '"tarball_url": "\K.*?(?=")')
  if [[ "$tar_url" == "" ]]; then return "$FALSE"; fi

  curl -fsSL "$tar_url" | tar xz --strip-components=1 --directory="$TEMP_INSTALL"
  if [ $? -ne 0 ]; then return "$FALSE"; fi

  return "$TRUE";
}

function download_latest_development {
  mkdir -p "$TEMP_INSTALL"
  tar_url="$REPO/tarball/master"
  curl -fsSL "$tar_url" | tar xz --strip-components=1 --directory="$TEMP_INSTALL"
  if [ $? -ne 0 ]; then return "$FALSE"; fi

  return "$TRUE";
}

function download_version {
  tar_url=$(curl -fsSL "$REPO/releases" | awk 'match($0,/.*"tarball_url": "(.*\/tarball\/.*)".*/)' | grep $1\" | awk -F '"' '{print $4}')
  if [[ ! -n "$tar_url" ]]; then
    return $"$FALSE"
  fi

  mkdir -p "$TEMP_INSTALL"
  curl -fsSL "$tar_url" | tar xz --strip-components=1 --directory="$TEMP_INSTALL"
  if [ $? -ne 0 ]; then return "$FALSE"; fi

  return "$TRUE";
}

function get_all_versions {
  curl -fsSL "$REPO/releases" | awk 'match($0,/.*"tarball_url": "(.*\/tarball\/.*)".*/)' | awk -F '/' '{split($NF,a,"\""); print a[1]}'
}

function run_install_script {
  if [ ! -f "$TEMP_INSTALL/release/install.sh" ]; then
    logerr "Can not find install script $TEMP_INSTALL/release/install.sh"
    return "$FALSE"
  fi

  log "Installing AqualinkD $1"

  # Can't run in background as it'll cleanup / delete files before install.
  nohup "$TEMP_INSTALL/release/install.sh" >> "$OUTPUT" 2>&1
  #source "/nas/data/Development/Raspberry/AqualinkD/release/install.sh" &>> "$OUTPUT"
  
  #nohup "/nas/data/Development/Raspberry/AqualinkD/release/install.sh" >> "$OUTPUT" 2>&1 &
  #nohup "$TEMP_INSTALL/release/install.sh" >> "$OUTPUT" 2>&1 &
}

function remove_install {
  curl -fsSL -H "Accept: application/vnd.github.raw+json" "$REPO/contents/install/install.sh" | sudo bash clean
}

function cleanup {
  rm -rf "$TEMP_INSTALL"
}


####################################################
#
#  Main
#  


# See if we are called from curl ot local dir.
# with curl no tty input and script name wil be blank.
if ! tty > /dev/null 2>&1; then
  script=$(basename "$0")
  if [ "$script" == "bash" ] || [ "$script" == "" ]; then 
    #echo "$(basename "$0") Script is likely running from curl"
    # We don't actualy use this, but may in the future to leave it here
    FROM_CURL=$TRUE
  fi
fi

if [[ $EUID -ne 0 ]]; then
   logerr "This script must be run as root" 
   exit 1
fi

#if [ ! -t 0 ]; then
  #Don't use log function here as we will cleanout the file if it exists.
  # Can't use $0 below as this script might be piped into bash from curl
  echo "$SELF - $(date) " 2>/dev/null > "$OUTPUT"
#fi

if check_can_upgrade; then
  installed_version
  if [[ "$INSTALLED_VERSION" != "" ]]; then
    log "Current AqualinkD installation $INSTALLED_VERSION"
    log "System OK to upgrade AqualinkD to $REL_VERSION"
  else
    log "System OK to install AqualinkD $REL_VERSION"
  fi
  
  #exit $TRUE;
else
  logerr "Can not upgrade, Please fix error(s)!"
  exit $FALSE;
fi

case $1 in
  check|checkupgradable)
    # We have already done the check, and returned false at this point, so return true.
    exit $TRUE
  ;;
  development)
    if ! latest_development_version; then logerr "getting development version";exit "$FALSE"; fi
    if ! download_latest_development; then logerr "downloading latest development";exit "$FALSE"; fi
    run_install_script "$REL_VERSION"
    cleanup
  ;;
  # Add Delete / remove / clean.
  clean|delete|remove)
    if ! remove_install; then logerr "Removing install";exit "$FALSE"; fi
    log "Removed install"
  ;;
  list|versions)
    get_all_versions
  ;;
  v*)
    if ! download_version $1; then logerr "downloading version $1";exit "$FALSE"; fi
    run_install_script "$REL_VERSION"
    cleanup
  ;;
  -h|help|h)
    echo "AqualinkD Installation script"
    echo "$SELF               <- download and install latest AqualinkD version"
    echo "$SELF latest        <- download and install latest AqualinkD version"
    echo "$SELF development   <- download and install latest AqualinkD development version"
    echo "$SELF clean         <- Remove AqualinkD"
    echo "$SELF list          <- List available versions to install"
    echo "$SELF v1.0.0        <- install AqualinkD v1.0.0 (use list option to see available versions)"
  ;;
  latest|*)
    if ! download_latest_release; then logerr "downloading latest"; exit "$FALSE"; fi
    run_install_script "$REL_VERSION"
    cleanup
  ;;
esac


exit


# List all versions
# curl -fsSL https://api.github.com/repos/sfeakes/aqualinkd/releases | awk 'match($0,/.*"html_url": "(.*\/releases\/tag\/.*)".*/)'
# curl -fsSL "https://api.github.com/repos/sfeakes/AqualinkD/releases" | awk 'match($0,/.*"tarball_url": "(.*\/tarball\/.*)".*/)' | awk -F '"' '{print $4}'