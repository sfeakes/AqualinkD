#!/bin/sh
while :
do
  wget -O /dev/stdout 'http://aqualink.ip.address/?command=mhstatus' 2>/dev/null
  sync
  sleep 60
done