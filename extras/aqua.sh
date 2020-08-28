#!/bin/bash
#
#  Run using
#  curl -s https://raw.githubusercontent.com/sfeakes/AqualinkD/master/extras/aqua.sh | bash -s <script parms> --
#
#  list latest release version (This is zip install)
#  curl --silent "https://api.github.com/repos/sfeakes/AqualinkD/releases/latest" | grep -Po '"tag_name": "\K.*?(?=")'
#
#  list latest dev version
#   curl --silent "https://raw.githubusercontent.com/sfeakes/AqualinkD/master/version.h" | grep AQUALINKD_VERSION | cut -d '"' -f 2
#


NAME="AqualinkD"
SOURCE_LOCATION="/extras/aqua.sh"

HOMEDIR=~
eval HOMEDIR=$HOMEDIR

BASE="$HOMEDIR/software/"
AQUA="$BASE/AqualinkD"
AQUA_GIT="$BASE/AqualinkD/.git"

echoerr() { printf "%s\n" "$*" >&2; }

function print_usage {
  echoerr "No arguments supplied, please use use:-"
  echoerr "$0 release      <- Download & install latest $NAME stable release."
  echoerr "$0 latest       <- Download & install latest $NAME version. (development release)"
  echoerr "$0 force_latest <- Force latest option (don't run any version checks)"
  echoerr "$0 install      <- Install to local filesystem, (config files will not be overwritten)"
  echoerr "$0 clean        <- Remove everything, including configuration files"
  echoerr "$0 <option> downloadonly  <- Download using one of above options, but don't unstall"
  echoerr ""
}

function upgrade_self {
  #echo "NOT UPGRADING SELF, MAKE SURE TO EDIT BEFORE COMMIT"
  #return;
  LOC=$(realpath $0)
  if [ "$LOC" == "/nas/data/Development/Raspberry/AqualinkD/extras/aqua.sh" ]; then
    echo "NOT UPGRADING SELF, (Development environment)"
    return
  elif [ "$0" == "bash" ] || "$0" == "$AQUA/$SOURCE_LOCATION" ]; then
    return
  fi

  if [ -f "$AQUA/$SOURCE_LOCATION" ]; then
    cp "$AQUA/$SOURCE_LOCATION" $0
  fi
}

function git_install {
  sudo apt-get install git
}

function local_version {
  if [ -f $AQUA/version.h ]; then
    echo `cat $AQUA/version.h | grep AQUALINKD_VERSION | cut -d '"' -f 2 | tr '\n' ' '`
  else
    echo 0
  fi
}

function git_release_version {
  echo `curl --silent "https://api.github.com/repos/sfeakes/AqualinkD/releases/latest" | grep -Po '"tag_name": "\K.*?(?=")'`
}

function get_development_version {
  echo `curl --silent "https://raw.githubusercontent.com/sfeakes/AqualinkD/master/version.h" | grep AQUALINKD_VERSION | cut -d '"' -f 2`
}

function remove {
  #cd ~

  if [ -d "$AQUA" ]; then
    if [ -L "$AQUA" ]; then
      unlink "$AQUA"
    else
      rm -rf "$AQUA"
    fi
  fi
}

function isvalid_gitinstall {
  #cd "$AQUA"
  isgit=$(git --git-dir "$AQUA_GIT" status 2>&1)
  return $?
}

function development_version_download {
  echo "Upgrading $NAME from $(local_version) to $(get_development_version)"
  if isvalid_gitinstall; then
    update 
  else
    force_dev_download
  fi
}

function force_dev_download {
  remove
  make-base

  #cd $BASE
  git clone "https://github.com/sfeakes/AqualinkD.git" "$AQUA"
}

function update {
  #cd ~
  #cd $AQUA
  git --git-dir "$AQUA_GIT" pull
}

function clean {
  #cd ~
  sudo "$AQUA/release/install.sh" "clean"
  remove
}

function install {
  if [ ! -f "$AQUA/release/install.sh" ]; then
    echo "ERROR Can not find install script, please download from git using either :-"
    echo "$0 release"
    echo "$0 latest"
    exit 1
  fi
  echo "Installing $NAME"
  #cd ~
  sudo "$AQUA/release/install.sh"
}

function make-base {
  if [ ! -d "$BASE" ]; then
    mkdir -p $BASE
  fi
  if [ ! -d "$BASE" ]; then
    echoerr "Error Can't create $BASE"; 
    exit 1; 
  fi
}

function release_version_download {
  version=$(local_version)
  git_version=$(git_release_version)

  if [ "v$version" != "$git_version" ]; then
    echo "Installing $NAME $git_version"
    
    make-base
    #cd $BASE
    # This is correct way, but tar is made with crap dir structure
    #tar_url=$(curl --silent "https://api.github.com/repos/sfeakes/AqualinkD/releases/latest" | grep -Po '"tarball_url": "\K.*?(?=")')
    #curl --silent -L "$tar_url" | tar xz
    # Use this tar instead, it's correct dir structure
    curl --silent -L "https://github.com/sfeakes/AqualinkD/archive/$git_version.tar.gz" | tar xz -C "$BASE"

    ver=$(echo $git_version  | sed 's/^[^0-9]*//' )
    
    if [ -d "$AQUA" ]; then
      if [ -d "$AQUA-$version" ]; then
        mv "$AQUA" "$AQUA-$RANDOM"
      else
        mv "$AQUA" "$AQUA-$version"
      fi
    fi
    if [ -L "$AQUA" ]; then
      unlink "$AQUA"
    fi

    ln -s "$BASE/AqualinkD-$ver" "$AQUA"
  else
    echo "Local $NAME version $version is latest, not downloading"
  fi
}

function check_git_installed {
  # Check git is installed
  command -v git >/dev/null 2>&1 || { echoerr "git is not installed.  Installing"; git_install; }
  command -v git >/dev/null 2>&1 || { echoerr "git is not installed.  Aborting"; exit 1; }
}

function check_curl_installed {
  command -v curl >/dev/null 2>&1 || { echoerr "curl is not installed.  Installing"; exit 1; }
}
# Check something was passed
if [[ $# -eq 0 ]]; then
    print_usage
    exit
fi

# Pass command line
if [ "$1" == "release" ]; then
  check_curl_installed
  release_version_download 
elif [ "$1" == "latest" ]; then
  check_curl_installed
  check_git_installed
  development_version_download
elif [ "$1" == "force_latest" ]; then
  check_curl_installed
  check_git_installed
  force_dev_download
elif [ "$1" == "install" ]; then
  echo -n ""
#  install 
elif [ "$1" == "clean" ]; then
  clean
  exit
else
  print_usage
  exit;
fi

if [ "$2" == "downloadonly" ]; then
  echo "No install, download only"
else
  install
fi

echo "Installed "`cat $AQUA/version.h | cut -d '"' -f 2 | tr '\n' ' '`

upgrade_self
#cd $CWD

echo ""
echo "Please make sure to read release notes https://github.com/sfeakes/AqualinkD/blob/master/README.md#release"

if [ "$0" == "bash" ]; then
  # Was probably run from curl
  echo -n "Source directory " 
  echo $AQUA | tr -s '/' '/'
  echo -n "To run this script in the future, "
  echo $AQUA/$SOURCE_LOCATION | tr -s '/' '/'
fi
