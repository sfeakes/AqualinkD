#!/bin/bash
#

PROCESSNAME=aqualinkd
MYPID=`pidof $PROCESSNAME`

#if [[ $EUID -ne 0 ]]; then
#   echo "This script must be run as root" 
#   exit 1
#fi

top -H -p $MYPID

