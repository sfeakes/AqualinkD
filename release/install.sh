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

LOG_SYSTEMD=1   # 1=false in bash, 0=true
REMOUNT_RO=1

TRUE=0
FALSE=1

_logfile=""
_frommake=$FALSE
_ignorearch=$FALSE
_nosystemd=$FALSE

log()
{
  echo "$*"

  if [[ -n "$_logfile" ]]; then
    echo "$*" >> "$_logfile"
  fi

  if [[ $LOG_SYSTEMD -eq 0 ]]; then
    logger -p local0.notice -t aqualinkd.upgrade "Upgrade:   $*"
    # Below is same as above but will only wotrk on journald (leaving it here if we use that rater then file)
    #echo $* | systemd-cat -t aqualinkd_upgrade -p info 
    #echo "$*" >> "$OUTPUT"
  fi
}

printHelp()
{
  echo "$0"
  echo "ignorearch            (don't check OS architecture, install what was made from Makefile)"
  echo "--arch <arch>         (install specific OS architecture - armhf | arm64)"
  echo "--logfile <filename>  (log to file)"
}


#log "Called $0 with $*"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --logfile)
      shift
      _logfile="$1"
      ;;
    --arch | --forcearch)
      shift
      #_forcearch="$1"
      if [[ -n "$1" ]]; then
        _ignorearch=$TRUE
        SOURCEBIN=$BIN-$1
      else
        log "--arch requires parameter eg. ( --arch armhf | --arch arm64 )"
      fi
      ;;
    from-make)
      _frommake=$TRUE
      ;;
    ignorearch)
      _ignorearch=$TRUE
      ;;
    nosystemd)
      _nosystemd=$TRUE
      ;;
    help | -help | --help | -h)
      printHelp
      exit $TRUE;
      ;;
    *)
      echo "Unknown argument: $1"
      printHelp;
      exit $FALSE;
      ;;
  esac
  shift
done

if ! tty > /dev/null 2>&1 || [ "$1" = "syslog" ]; then
  # No stdin, probably called from upgrade script
  LOG_SYSTEMD=0 # Set logger to systemd
fi

if [[ $EUID -ne 0 ]]; then
   log "This script must be run as root" 
   exit 1
fi

if [[ $(mount | grep " / " | grep "(ro,") ]]; then
  if mount / -o remount,rw &>/dev/null; then
      # can mount RW.
      #mount / -o remount,rw &>/dev/null
    log "Root filesystem is readonly, remounted RW"
    REMOUNT_RO=0;
  else
    log "Root filesystem is readonly, can't install" 
    exit 1
  fi
fi

# Figure out what system we are on and set correct binary.
# If we have been called from make, this is a custom build and install, so ignore check.
#if [ "$PARENT_COMMAND" != "make" ] && [ "$1" != "from-make" ] && [ "$1" != "ignorearch" ]; then
if [ "$PARENT_COMMAND" != "make" ] && [ "$_frommake" -eq $FALSE ] && [ "$_ignorearch" -eq $FALSE ]; then
  # Use arch or uname -a to get above.
  # dpkg --print-architecture

  # Exit if we can't find systemctl
  command -v dpkg >/dev/null 2>&1 || { log -e "Can't detect system architecture, Please check path to 'dpkg' or install manually.\n"\
                                               "Or run '$0 ignorearch'" >&2; exit 1; }

  ARCH=$(dpkg --print-architecture)
  BINEXT=""

  case $ARCH in 
    arm64)
      log "Arch is $ARCH, Using 64bit AqualinkD"
      BINEXT="-arm64"
    ;;
    armhf)
      log "Arch is $ARCH, Using 32bit AqualinkD"
      BINEXT="-armhf"
    ;;
    *)
      if [ -f $BUILD/$SOURCEBIN-$ARCH ]; then
        log "Arch $ARCH is not officially supported, but we found a suitable binary"
        BINEXT="-$ARCH"
      else
        log "Arch $ARCH is unknown, Default to using 32bit HF AqualinkD, you may need to manually try ./release/aqualnkd_arm64"
        BINEXT=""
      fi
    ;;
  esac

  # Need to check BINEXISTS
  if [ -f $BUILD/$SOURCEBIN$BINEXT ]; then
    SOURCEBIN=$BIN$BINEXT
  elif [ -f $BUILD/$SOURCEBIN ]; then
    # Not good
    log "Can't find correct aqualnkd binary for $ARCH, '$BUILD/$SOURCEBIN$BINEXT' using '$BUILD/$SOURCEBIN' ";
  fi
fi

# Exit if we can't find binary
if [ ! -f $BUILD/$SOURCEBIN ]; then
  log "Can't find aqualnkd binary `$BUILD/$SOURCEBIN` ";
  exit 1
fi

# Exit if we can't find systemctl
command -v systemctl >/dev/null 2>&1 || { log "This script needs systemd's systemctl manager, Please check path or install manually" >&2; exit 1; }

if [ "$_nosystemd" -eq $FALSE ]; then
  # stop service, hide any error, as the service may not be installed yet
  systemctl stop $SERVICE > /dev/null 2>&1
  SERVICE_EXISTS=$(echo $?)
fi

# Clean everything if requested.
if [ "$1" == "clean" ]; then
  log "Deleting install"
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
  if [ -f /etc/cron.d/aqualinkd ]; then
    rm -f /etc/cron.d/aqualinkd
  fi
  if [ -d $WEBLocation ]; then
    rm -rf $WEBLocation
  fi
  systemctl daemon-reload
  exit
fi


# Check cron.d options
if [ ! -d "/etc/cron.d" ]; then
  log "The version of Cron may not support chron.d, if so AqualinkD Scheduler will not work"
  log "Please check before starting"
else
  if [ -f "/etc/default/cron" ]; then
    CD=$(cat /etc/default/cron | grep -v ^# | grep "\-l")
    if [ -z "$CD" ]; then
      log "Please enabled cron.d support, if not AqualinkD Scheduler will not work"
      log "Edit /etc/default/cron and look for the -l option, usually in EXTRA_OPTS"
    fi
  else
    log "Please make sure the version if Cron supports chron.d, if not the AqualinkD Scheduler will not work"
  fi
fi

# V2.3.9 & V2.6.0 has kind-a breaking change for config.js, so check existing and rename if needed
#        we added Aux_V? to the button list
if [ -f "$WEBLocation/config.js" ]; then
  # Test is if has AUX_V1 in file AND "Spa" is in file (Spa_mode changed to Spa)
  # Version 2.6.0 added Chiller as well
  if  ! grep -q '"Aux_V1"' "$WEBLocation/config.js" || 
      ! grep -q '"Spa"' "$WEBLocation/config.js" || 
      ! grep -q '"Chiller"' "$WEBLocation/config.js"; then
    dateext=`date +%Y%m%d_%H_%M_%S`
    log "AqualinkD web config is old, making copy to $WEBLocation/config.js.$dateext"
    log "Please make changes to new version $WEBLocation/config.js"
    mv $WEBLocation/config.js $WEBLocation/config.js.$dateext
  fi
fi



# copy files to locations, but only copy cfg if it doesn;t already exist

cp $BUILD/$SOURCEBIN $BINLocation/$BIN
cp $BUILD/$SRV $SRVLocation/$SRV

if [ -f $CFGLocation/$CFG ]; then
  log "AqualinkD config exists, did not copy new config, you may need to edit existing! $CFGLocation/$CFG"
else
  cp $BUILD/$CFG $CFGLocation/$CFG
fi

if [ -f $DEFLocation/$DEF ]; then
  log "AqualinkD defaults exists, did not copy new defaults to $DEFLocation/$DEF"
else

  cp $BUILD/$DEF.defaults $DEFLocation/$DEF
fi

if [ -f $MDNSLocation/$MDNS ]; then
  log "Avahi/mDNS defaults exists, did not copy new defaults to $MDNSLocation/$MDNS"
else
  if [ -d "$MDNSLocation" ]; then
    cp $BUILD/$MDNS.avahi $MDNSLocation/$MDNS
  else
    log "Avahi/mDNS may not be installed, not copying $MDNSLocation/$MDNS"
  fi
fi

if [ ! -d "$WEBLocation" ]; then
  mkdir -p $WEBLocation
fi

if [ -f "$WEBLocation/config.js" ]; then
  log "AqualinkD web config exists, did not copy new config, you may need to edit existing $WEBLocation/config.js "
  if command -v "rsync" &>/dev/null; then
    rsync -avq --exclude='config.js' $BUILD/../web/* $WEBLocation
  else
    # This isn;t the right way to do it, but seems to work.
    shopt -s extglob
    `cp -r "$BUILD/../web/"!(*config.js) "$WEBLocation"`
    shopt -u extglob
    # Below should work, but doesn't.
    #shopt -s extglob
    #cp -r "$BUILD/../web/"!(*config.js) "$WEBLocation"
    #shopt -u extglob
  fi
else
  cp -r $BUILD/../web/* $WEBLocation
fi

# remount root ro
if [[ $REMOUNT_RO -eq 0 ]]; then
  mount / -o remount,ro &>/dev/null
  log "Root filesystem remounted RO"
fi

if [ "$_nosystemd" -eq $TRUE ]; then
  exit 0
fi

systemctl enable $SERVICE
systemctl daemon-reload

if [ $SERVICE_EXISTS -eq 0 ]; then
  log "Starting daemon $SERVICE"
  systemctl start $SERVICE
else
  log "Please edit $CFGLocation/$CFG, then start AqualinkD service with `sudo systemctl start aqualinkd`"
fi




