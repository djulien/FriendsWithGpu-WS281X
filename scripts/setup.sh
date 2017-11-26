#!/bin/bash
#set up dependencies and symlinks
#TODO: move dependencies to Makefile or binding.gyp, or find another npm package that installs it
#TODO: figure out a nicer way to handle links
#set -x

HERE="$(dirname "$(readlink -fm "$0")")" #https://stackoverflow.com/questions/20196034/retrieve-parent-directory-of-script
#MY_TOP=`git rev-parse --show-toplevel`  #from https://unix.stackexchange.com/questions/6463/find-searching-in-parent-directories-instead-of-subdirectories
#MY_TOP=$HERE/..
NODE_ROOT=`npm root`
#source  "$MY_TOP"/scripts/colors.sh
source  "$HERE"/colors.sh

relpath()
{
    realpath --relative-to="$1" "$2"
}

DEPS="libsdl2-dev"
TARGET="$NODE_ROOT/gpu-friends-ws281x"
#echo -e "${BLUE}here $HERE${NORMAL}"
#pwd
#echo "root $NODE_ROOT"
#echo `relpath $MY_TOP, $TARGET`

if [ "x$1" == "xpre" ]; then
  MISSING=0
  for dep in "$DEPS"; do
    echo "is dep $dep installed?"
    installed=$(apt list $dep)
    if [[ "$installed" != *"[installed]"* ]]; then
      MISSING=1
    fi
  done
  if [ "$MISSING" == "0" ]; then
    echo -e  "${GREEN}Dependencies already installed.${NORMAL}"
  else
    echo -e  "${CYAN}Installing dependencies ...${NORMAL}"
    sudo  apt-get install  "$DEPS"
    if [ $? != 0 ]; then
      echo -e  "${RED}Can't install dependencies.${NORMAL}"
    else
      echo -e  "${GREEN}Dependencies installed.${NORMAL}"
    fi
  fi
elif [ "x$1" == "xpost" ]; then
  if [ -d "$TARGET" ]; then
    echo -e  "${GREEN}Symbolic link not needed.${NORMAL}"
  else
    echo -e  "${CYAN}Creating symbolic link ...${NORMAL}"
    ln -s  "$HERE/.."  "$TARGET"
#    ln -snf "$HERE/.." "$TARGET"
    if [ $? != 0 ]; then
      echo -e  "${RED}Can't create symbolic link.${NORMAL}"
    else
      echo -e  "${GREEN}Symbolic link created.${NORMAL}"
    fi
  fi
else
  echo -e  "${RED}Unknown option: '$1'${NORMAL}"
fi

#eof#
