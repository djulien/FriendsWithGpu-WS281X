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
echo -e "${BLUE}here $HERE${NORMAL}"
#pwd
echo "root $NODE_ROOT"
#echo `relpath $MY_TOP, $TARGET`

if [ "x$1" == "xpre" ]; then
  echo -e  "${CYAN}installing dependencies ...${NORMAL}"
  sudo  apt-get install  "$DEPS"
  echo -e  "${GREEN}dependencies installed.${NORMAL}"
elif [ "x$1" == "xpost" ]; then
  if [ -d "$TARGET" ]; then
    echo -e  "${GREEN}no symbolic link needed.${NORMAL}"
  else
    echo -e  "${CYAN}making symbolic link ...${NORMAL}"
    echo ln -s  "$HERE/.."  "$TARGET"
#    ln -snf "$HERE/.." "$TARGET"
    echo -e  "${GREEN}symbolic links created.${NORMAL}"
  fi
else
  echo -e  "${RED}unknown option: '$1'${NORMAL}"
fi

#eof#
