#!/bin/bash
#
# ROOT=/nas/data/Development/Raspberry/gpiocrtl/test-install
#


BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARENT_COMMAND=$(ps -o comm= $PPID)

SERVICE="aqualinkd"

BIN="aqualinkd"
CFG="aqualinkd.conf"
SRV="aqualinkd.service"
DEF="aqualinkd"
MDNS="aqualinkd.service"

BINLocation="/usr/local/bin"
CFGLocation="/etc"
SRVLocation="/etc/systemd/system"
DEFLocation="/etc/default"
WEBLocation="/var/www/aqualinkd/"
MDNSLocation="/etc/avahi/services/"

SOURCEBIN=$BIN

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

if [[ $(mount | grep " / " | grep "(ro,") ]]; then
  echo "Root filesystem is readonly, can't install" 
  exit 1
fi

# Figure out what system we are on and set correct binary.
# If we have been called from make, this is a custom build and install, so ignore check.
if [ "$PARENT_COMMAND" != "make" ] && [ "$1" != "from-make" ] && [ "$1" != "ignorearch" ]; then
  # Use arch or uname -a to get above.
  # dpkg --print-architecture

  # Exit if we can't find systemctl
  command -v dpkg >/dev/null 2>&1 || { echo -e "Can't detect system architecture, Please check path to 'dpkg' or install manually.\n"\
                                               "Or run '$0 ignorearch'" >&2; exit 1; }

  ARCH=$(dpkg --print-architecture)
  BINEXT=""

  case $ARCH in 
    arm64)
      echo "Arch is $ARCH, Using 64bit AqualinkD"
      BINEXT="-arm64"
    ;;
    armhf)
      echo "Arch is $ARCH, Using 32bit AqualinkD"
      BINEXT="-armhf"
    ;;
    *)
      if [ -f $BUILD/$SOURCEBIN-$ARCH ]; then
        echo "Arch $ARCH is not officially supported, but we found a suitable binary"
        BINEXT="-$ARCH"
      else
        echo "Arch $ARCH is unknown, Default to using 32bit HF AqualinkD, you may need to manually try ./release/aqualnkd_arm64"
        BINEXT=""
      fi
    ;;
  esac

  # Need to check BINEXISTS
  if [ -f $BUILD/$SOURCEBIN$BINEXT ]; then
    SOURCEBIN=$BIN$BINEXT
  elif [ -f $BUILD/$SOURCEBIN ]; then
    # Not good
    echo "Can't find correct aqualnkd binary for $ARCH, '$BUILD/$SOURCEBIN$BINEXT' using '$BUILD/$SOURCEBIN' ";
  fi
fi

# Exit if we can't find binary
if [ ! -f $BUILD/$SOURCEBIN ]; then
  echo "Can't find aqualnkd binary `$BUILD/$SOURCEBIN` ";
  exit 1
fi

# Exit if we can't find systemctl
command -v systemctl >/dev/null 2>&1 || { echo "This script needs systemd's systemctl manager, Please check path or install manually" >&2; exit 1; }

# stop service, hide any error, as the service may not be installed yet
systemctl stop $SERVICE > /dev/null 2>&1
SERVICE_EXISTS=$(echo $?)

# Clean everything if requested.
if [ "$1" == "clean" ]; then
  echo "Deleting install"
  systemctl disable $SERVICE > /dev/null 2>&1
  if [ -f $BINLocation/$BIN ]; then
    rm -f $BINLocation/$BIN
  fi
  if [ -f $SRVLocation/$SRV ]; then
    rm -f $SRVLocation/$SRV
  fi
  if [ -f $CFGLocation/$CFG ]; then
    rm -f $CFGLocation/$CFG
  fi
  if [ -f $DEFLocation/$DEF ]; then
    rm -f $DEFLocation/$DEF
  fi
  if [ -d $WEBLocation ]; then
    rm -rf $WEBLocation
  fi
  systemctl daemon-reload
  exit
fi


# Check cron.d options
if [ ! -d "/etc/cron.d" ]; then
  echo "The version of Cron may not support chron.d, if so AqualinkD Scheduler will not work"
  echo "Please check before starting"
else
  if [ -f "/etc/default/cron" ]; then
    CD=$(cat /etc/default/cron | grep -v ^# | grep "\-l")
    if [ -z "$CD" ]; then
      echo "Please enabled cron.d support, if not AqualinkD Scheduler will not work"
      echo "Edit /etc/default/cron and look for the -l option, usually in EXTRA_OPTS"
    fi
  else
    echo "Please make sure the version if Cron supports chron.d, if not the AqualinkD Scheduler will not work"
  fi
fi

# V2.3.9 has kind-a breaking change for config.js, so check existing and rename if needed
#        we added Aux_V? to the button list
if [ -f "$WEBLocation/config.js" ]; then
  # Test is if has AUX_V1 in file
  if ! grep -q Aux_V1 ./config.js; then
    $dateext=`date +%Y%m%d_%H_%M_%S`
    echo "AqualinkD web config is old, making copy to $WEBLocation/config.js.$dateext"
    echo "Please make changes to new version $WEBLocation/config.js"
    mv $WEBLocation/config.js $WEBLocation/config.js.$dateext
  fi
fi



# copy files to locations, but only copy cfg if it doesn;t already exist

cp $BUILD/$SOURCEBIN $BINLocation/$BIN
cp $BUILD/$SRV $SRVLocation/$SRV

if [ -f $CFGLocation/$CFG ]; then
  echo "AqualinkD config exists, did not copy new config, you may need to edit existing! $CFGLocation/$CFG"
else
  cp $BUILD/$CFG $CFGLocation/$CFG
fi

if [ -f $DEFLocation/$DEF ]; then
  echo "AqualinkD defaults exists, did not copy new defaults to $DEFLocation/$DEF"
else
  cp $BUILD/$DEF.defaults $DEFLocation/$DEF
fi

if [ -f $MDNSLocation/$MDNS ]; then
  echo "Avahi/mDNS defaults exists, did not copy new defaults to $MDNSLocation/$MDNS"
else
  if [ -d "$MDNSLocation" ]; then
    cp $BUILD/$MDNS.avahi $MDNSLocation/$MDNS
  else
    echo "Avahi/mDNS may not be installed, not copying $MDNSLocation/$MDNS"
  fi
fi

if [ ! -d "$WEBLocation" ]; then
  mkdir -p $WEBLocation
fi

if [ -f "$WEBLocation/config.js" ]; then
  echo "AqualinkD web config exists, did not copy new config, you may need to edit existing $WEBLocation/config.js "
  rsync -avq --exclude='config.js' $BUILD/../web/* $WEBLocation
else
  cp -r $BUILD/../web/* $WEBLocation
fi


systemctl enable $SERVICE
systemctl daemon-reload

if [ $SERVICE_EXISTS -eq 0 ]; then
  echo "Starting daemon $SERVICE"
  systemctl start $SERVICE
else
  echo "Please edit $CFGLocation/$CFG, then start AqualinkD service"
fi

