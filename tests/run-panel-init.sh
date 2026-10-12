#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -D_GNU_SOURCE -DAQ_PDA -ffunction-sections -fdata-sections -Isource \
  tests/panel_init.c source/aq_panel.c source/utils.c source/rs_msg_utils.c source/config.c \
  -Wl,--gc-sections -pthread -lm -o "$tmp/panel-init"
"$tmp/panel-init"
