 
 #!/bin/bash
 #
 # To re-synchronize your lights, 
 # turn the switch on, 
 # then back off, 
 # then wait between 11 and 14 seconds and turn the switch back on. 
 # When the lights come back on, they should enter program #1, and be synchronized.

 # Different way to sync
 # To re-synchronize your lights, start with the lights off and follow the steps below: 
 # 1. Turn lights on.
 # 2. Turn off light for between 11-15 seconds,.
 # 3. Turn lights on.
 
 # Different again
 # https://images-na.ssl-images-amazon.com/images/I/A1aZApuRWQS.pdf

# Manual
# https://hayward-pool-assets.com/assets/documents/pools/pdf/manuals/colorlogic-installation-operation.pdf
#
# Warm up time
# https://www.troublefreepool.com/threads/pool-lights-wont-sync-up.177510/

URL="http://pool/api/Pool%20Light/set"

function delay {
  echo "Sleeping $1 seconds"
  sleep $1
}
function on {
  echo -n "Turning light on. "
  curl -X PUT -d value=1 $URL
  echo ""
}
function off {
  echo -n "Turning light off. "
  curl -X PUT -d value=0 $URL
  echo ""
}

# This is light mode check
function mode_check {
  on
  delay 120
  off
  delay 12

  on
  delay 1
  off
  delay 12

  on
  delay 1
  off
  delay 12

  on
  delay 1
  off
  delay 12

  on

  echo "Lights should be flashing Red and White, power down for at least 1 minute"
}

# This is color recync. or color program.
function resync {
  on
  delay 70   
  off
  delay 12
  on

  echo "Lights should be acting the same color mode"
}

function change_mode {
  off
  delay 12
  on  
  echo "Lights should be acting the same color mode"
}

function mode {
  on
  delay 1
  off
  delay 13
  
  on
  delay 1
  off
  delay 13
  
  on
  delay 1
  off
  delay 13

  on

  echo "Lights should blink red"
}


resync
read -p "Y/N " input
while [ "$input" == "n" ]; do
  change_mode
  read -p "Y/N " input
done
echo "Leave Lights off for at least 2 mins"
off
exit

#mode_check
resync