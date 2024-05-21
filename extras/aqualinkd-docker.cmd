#!/bin/bash

CONFDIR=/aquadconf
AQUA_CONF=$CONFDIR/aqualinkd.conf

# Check we have a config directory
if [ -d "$CONFDIR" ]; then

  # Check we have config file, if not copy default
  if [ ! -f "$AQUA_CONF" ]; then
    echo "Warning no aqualinkd.conf in $CONFDIR", using default
    cp /etc/aqualinkd.conf $CONFDIR
  fi

  # Replace local filesystem config with mounted config
  ln -sf "$AQUA_CONF" /etc/aqualinkd.conf

  # If we have a web config, replace the local filesystem with mounted
  if [ -f "$CONFDIR/config.js" ]; then
    ln -sf "$CONFDIR/config.js" /var/www/aqualinkd/config.js
  fi

  # If don't have a cron file, create one
  if [ ! -f "$CONFDIR/aqualinkd.schedule" ]; then
    echo "#***** AUTO GENERATED DO NOT EDIT *****" > "$CONFDIR/aqualinkd.schedule"
  fi

  # link mounted cron file to local filesystem.
  ln -sf "$CONFDIR/aqualinkd.schedule" /etc/cron.d/aqualinkd
  chmod 644 "$CONFDIR/aqualinkd.schedule"
else
  # No conig dir, show warning 
  echo "WARNING no config directory, AqualinkD starting with default config, no changes will be saved"
  AQUA_CONF="/etc/aqualinkd.conf"
fi



# Start cron
service cron start

# Start AqualinkD not in daemon mode
/usr/local/bin/aqualinkd -d -c $AQUA_CONF

