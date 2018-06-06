#!/bin/bash
#
# ROOT=/nas/data/Development/Raspberry/gpiocrtl/test-install
#


BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SERVICE="aquarited"

BIN="aquarited"
CFG="aquarited.conf"
SRV="aquarited.service"
DEF="aquarited"
ARC="aquarited_cache.sh"

BINLocation="/usr/local/bin"
CFGLocation="/etc"
SRVLocation="/etc/systemd/system"
DEFLocation="/etc/default"
#WEBLocation="/var/www/aquarited/"
#MDNSLocation="/etc/avahi/services/"

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

if [[ $(mount | grep " / " | grep "(ro,") ]]; then
  echo "Root filesystem is readonly, can't install" 
  exit 1
fi

# Exit if we can't find systemctl
command -v systemctl >/dev/null 2>&1 || { echo "This script needs systemd's systemctl manager, Please check path or install manually" >&2; exit 1; }

# stop service, hide any error, as the service may not be installed yet
systemctl stop $SERVICE > /dev/null 2>&1
SERVICE_EXISTS=$(echo $?)

# mount / rw since stopping deamon will set root_ro
if [[ $(mount | grep " / " | grep "(ro,") ]]; then
  mount / -o remount,rw
fi
# copy files to locations, but only copy cfg if it doesn;t already exist

cp $BUILD/$BIN $BINLocation/$BIN
cp $BUILD/$SRV $SRVLocation/$SRV
cp $BUILD/$ARC $BINLocation/$ARC

if [ -f $CFGLocation/$CFG ]; then
  echo "Config exists, did not copy new config, you may need to edit existing! $CFGLocation/$CFG"
else
  cp $BUILD/$CFG $CFGLocation/$CFG
fi

if [ -f $DEFLocation/$DEF ]; then
  echo "Defaults exists, did not copy new defaults to $DEFLocation/$DEF"
else
  cp $BUILD/$DEF.defaults $DEFLocation/$DEF
fi



#if [ -f $MDNSLocation/$MDNS ]; then
#  echo "Avahi/mDNS defaults exists, did not copy new defaults to $MDNSLocation/$MDNS"
#else
#  if [ -d "$MDNSLocation" ]; then
#    cp $BUILD/$MDNS.avahi $MDNSLocation/$MDNS
#  else
#    echo "Avahi/mDNS may not be installed, not copying $MDNSLocation/$MDNS"
#  fi
#fi
#
#if [ ! -d "$WEBLocation" ]; then
#  mkdir -p $WEBLocation
#fi
#
#cp -r $BUILD/../web/* $WEBLocation

systemctl enable $SERVICE
systemctl daemon-reload

if [ $SERVICE_EXISTS -eq 0 ]; then
  echo "Starting daemon $SERVICE"
  systemctl start $SERVICE
fi

