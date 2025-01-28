#!/bin/bash
#

PROCESSNAME=aqualinkd
MYPID=`pidof $PROCESSNAME`

if [ $? -ne 0 ]; then
  MYPID=$(pidof "$PROCESSNAME-arm64")
  if [ $? -ne 0 ]; then
    MYPID=$(pidof "$PROCESSNAME-armhf")
  fi
fi

#if [[ $EUID -ne 0 ]]; then
#   echo "This script must be run as root" 
#   exit 1
#fi

top -H -p $MYPID

