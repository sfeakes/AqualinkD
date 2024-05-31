#!/bin/bash
#
# Script to build arm64 & amd64 containers that are published to docker.io
#

IMAGE=aqualinkd

if [ $# -eq 0 ]
  then
    # Below is safer, but not supported on all platforms.
    #VERSION=$(curl --silent "https://api.github.com/repos/sfeakes/AqualinkD/releases/latest" | grep -Po '"tag_name": "[^0-9|v|V]*\K.*?(?=")')
    VERSION=$(curl --silent "https://api.github.com/repos/sfeakes/AqualinkD/releases/latest" | grep  "tag_name" | awk -F'"' '$0=$4')
  else
    VERSION=$1
fi

URL="https://github.com/sfeakes/AqualinkD/archive/refs/tags/"$VERSION".tar.gz"
URL2="https://github.com/sfeakes/AqualinkD/archive/refs/tags/v"$VERSION".tar.gz"
URL3="https://github.com/sfeakes/AqualinkD/archive/refs/tags/V"$VERSION".tar.gz"
#BURL="https://github.com/sfeakes/AqualinkD/archive/refs/heads/"$VERSION".tar.gz"

# Check version is accurate before running docker build

if ! curl --output /dev/null --silent --location --head --fail "$URL"; then
  # Check if version tag has wrong case
  if curl --output /dev/null --silent --location --head --fail "$URL2"; then
    VERSION=v$VERSION
  else
    # Check if it's a branch
    if curl --output /dev/null --silent --location --head --fail "$URL3"; then   
      VERSION=V$VERSION
    else
      echo "ERROR Can't build Docker container for $IMAGE $VERSION"
      echo -e "Neither Version or Branch URLs:- \n $URL \n $URL2 \n $URL3"
      exit 1
    fi
  fi
fi

echo "Building Docker container for $IMAGE using branch $VERSION"
docker buildx build --platform=linux/amd64,linux/arm64 \
                    --file Dockerfile.buildx \
                    -t docker.io/sfeakes/${IMAGE}:${VERSION} \
                    -t docker.io/sfeakes/${IMAGE}:latest \
                    --build-arg AQUALINKD_VERSION=${VERSION} \
                    --push .