#!/bin/bash
#
# /*
# * Copyright (c) 2017 Shaun Feakes - All rights reserved
# *
# * You may use redistribute and/or modify this code under the terms of
# * the GNU General Public License version 2 as published by the 
# * Free Software Foundation. For the terms of this license, 
# * see <http://www.gnu.org/licenses/>.
# *
# * You are free to use this software under the terms of the GNU General
# * Public License, but WITHOUT ANY WARRANTY; without even the implied
# * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# * See the GNU General Public License for more details.
# *
# *  https://github.com/sfeakes/aqualinkd
# */

MAX_TEMP=78
WARNING_TEMP=72

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

echomsg() { if [ -t 1 ]; then echo "$@" 1>&2; fi; }

reportThermal() {
  if ((($1)!=0)) ; then
    echomsg "ERROR : $2"
  else
    echomsg "$2 : OK"
  fi
}

TEMP=$(awk '{printf("%.1f\n",$1/1e3)}' /sys/class/thermal/thermal_zone0/temp 2>/dev/null )

if [ $? -ne 0 ]; then
  echomsg "Error: getting CPU temperature"
else
  echomsg "CPU temperature: $TEMP"
  if (( $(awk 'BEGIN {print ("'$TEMP'" >= "'$MAX_TEMP'")}') )); then
    echomsg "Error: Over Temperature $TEMP"
  elif (( $(awk 'BEGIN {print ("'$TEMP'" >= "'$WARNING_TEMP'")}') )); then
    echomsg "Warning: Temperature $TEMP"
  fi
fi

VOLTS=$(vcgencmd measure_volts | grep -Eo '[0-9]+([.][0-9]+)?')
echomsg "CPU Voltage: $VOLTS"

#Get Status, extract hex
STATUS=$(vcgencmd get_throttled)
if [[ "$STATUS" == *'Command not registered'* ]]; then
  echomsg "Error: Update Pi to enable 'vcgencmd get_throttled' command"
  exit;
fi
STATUS=${STATUS#*=}

if [ $? -ne 0 ]; then
  echomsg "Error getting vcgencmd information"
  exit 1;
fi

reportThermal "$STATUS&UNDERVOLTED" "Undervolt" undervolt
reportThermal "$STATUS&HAS_UNDERVOLTED" "Undervolt history" has_undervolt

reportThermal "$STATUS&THROTTLED" "Throttled" throttled
reportThermal "$STATUS&HAS_THROTTLED" "Throttled history" has_throttled

reportThermal "$STATUS&CAPPED" "Frequency Capped" freqcap
reportThermal "$STATUS&HAS_CAPPED" "Frequency Capped history" has_freqcap
