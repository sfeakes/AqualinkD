#!/bin/bash

# Example file that will start SOCAT before AqualinkD in a docker 
# to support EW-11 WIFI module for RS485 connection
#
# This file should be placed in the config aqualinkd directory defined
# in your docker-compose.yml (or equiv)
#
# MAKE SURE TO CHAGE THE IP BELOW
#

echo "Starting SOCAT port binding....."
socat -d -d pty,link=/dev/tty.Pool2,raw TCP:192.168.99.248:8899 &
echo "Sleeping for SOCAT start....."
sleep 2s