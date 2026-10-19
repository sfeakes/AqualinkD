#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_build=$(mktemp -d /tmp/iaqtouch-test.XXXXXX)
trap 'rm -f "$test_build/iaqtouch-timeout"; rmdir "$test_build"' EXIT
${CC:-cc} -O2 -g -ffunction-sections -fdata-sections \
  ${TEST_CFLAGS:-} -I source tests/iaqtouch_timeout.c source/iaqtouch.c \
  -Wl,--gc-sections -pthread -o "$test_build/iaqtouch-timeout"
"$test_build/iaqtouch-timeout"
