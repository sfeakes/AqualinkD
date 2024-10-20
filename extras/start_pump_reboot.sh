#!/bin/bash

# Pass time between as #1 and #2 using 24 hour

# example crontab entry to start pump if system boots is between 6am and 11pm
#
# @reboot /path/start_pump_reboot 6 23
#

startAfter=$1
startBefore=$2

SLEEP_BETWEEN_TRIES=5  # 5 seconds
TRIES=5                # 5 tries

# Wait for AqualinkD to come up and connect to panel
sleep 30

hour=$(date +%H)


function turn_on() {
  curl -s -S -o /dev/null http://localhost:80/api/Filter_Pump/set -d value=1 -X PUT --fail
  rtn=$?
  echo $rtn
  return $rtn
}

#echo "Hour=$hour  Ater=$startAfter Before=$startBefore"

# Remember 11:45 is 11, so don't use <= for startBefore
if (($hour >= $startAfter && $hour < $startBefore )); then
  x=1
  while [ $x -le $TRIES ] && [ $(turn_on) -gt 0 ]; do
    sleep $SLEEP_BETWEEN_TRIES
    x=$(( $x + 1 ))
  done
fi