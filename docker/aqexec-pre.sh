#!/bin/bash

# Example file that will start SOCAT before AqualinkD in a docker 
# to support EW-11 WIFI module for RS485 connection
#
# This file should be placed in the config aqualinkd directory defined
# in your docker-compose.yml (or equiv)
#
# MAKE SURE TO CHAGE THE IP BELOW (1.1.1.1)
# aqualinkd.cong should have serial_port=/dev/ttyEW11

echo "Starting SOCAT port binding....."
socat -d -d pty,link=/dev/ttyEW11,raw,ignoreeof TCP4:1.1.1.1:8899,ignoreeof &
sudo docker compose up
echo "Sleeping for SOCAT start....."
sleep 2s