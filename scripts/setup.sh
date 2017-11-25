#!/bin/bash
#set up dependencies and symlinks
#TODO: move dependencies to Makefile or binding.gyp, or find another npm package that installs it
#TODO: figure out a nicer way to handle links
#set -x

HERE="$(dirname "$(readlink -fm "$0")")" #https://stackoverflow.com/questions/20196034/retrieve-parent-directory-of-script
MY_TOP=`git rev-parse --show-toplevel`  #from https://unix.stackexchange.com/questions/6463/find-searching-in-parent-directories-instead-of-subdirectories
#source  "$MY_TOP"/scripts/colors.sh
source  "$HERE"/colors.sh

if [ "x$1" == "pre" ]; then
  echo -e  "${CYAN}installing dependencies ...${NORMAL}"
  sudo  apt-get install  libsdl2-dev
  echo -e  "${GREEN}dependencies installed.${NORMAL}"
elif [ "x$1" == "post" ]; then
  if [ -d "$MY_TOP/node_modules/gpu-friends-ws281x" ]; then
    echo -e  "${GREEN}no symbolic link needed.${NORMAL}"
  else
    echo -e  "${CYAN}making symbolic links ...${NORMAL}"
    ln -snf $MY_TOP/node_modules/gpu-friends-ws281x
    echo -e  "${GREEN}symbolic links created.${NORMAL}"
  fi
else
  echo -e  "${RED}unknown option: '$1'${NORMAL}"
fi

#eof#
