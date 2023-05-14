#!/bin/bash

#SELF=${0##*/}
SELF=${0}
ME=`whoami`
CWD=`pwd`
GIT_HOST="tiger"

echo -n "Did you remember to 'make' and 'make slog' ? :"
read answer
if [ "$answer" = "n" ]; then
  exit 0
fi

if ! [ "$HOSTNAME" = "$GIT_HOST" ]; then
  ssh $ME@$GIT_HOST 'cd '"$CWD"'; '"$SELF"';'
else

  command -v git >/dev/null 2>&1 || { echo >&2 "GIT is not installed.  Aborting."; exit 1; }

  VER=$(cat version.h | grep AQUALINKD_VERSION | cut -d\" -f2)

  git tag v$VER

  echo "Version set to $VER"

  echo -n "Upload to github ? (y or n):"
  read answer
  if [ "$answer" = "y" ]; then
    git add --all
    if [ $? -ne 0 ]; then
      echo "Error from running 'git add --all'"
      exit 1
    fi

    git commit -m "Version $VER"
    if [ $? -ne 0 ]; then
      echo "Error from running 'git commit -m \"Version $VER\"'"
      exit 1
    fi

    git push
  fi
fi



