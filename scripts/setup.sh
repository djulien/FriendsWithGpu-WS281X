#!/bin/bash
#set up dependencies and symlinks
#TODO: move dependencies to Makefile or binding.gyp, or find another npm package that installs it
#TODO: figure out a better way to handle this
#set -x

HERE="$(dirname "$(readlink -fm "$0")")" #https://stackoverflow.com/questions/20196034/retrieve-parent-directory-of-script
#MY_TOP=`git rev-parse --show-toplevel`  #from https://unix.stackexchange.com/questions/6463/find-searching-in-parent-directories-instead-of-subdirectories
#MY_TOP=$HERE/..
NODE_ROOT=`npm root`
DEPS="libsdl2-dev"
TARGET="$NODE_ROOT/gpu-friends-ws281x"
#source  "$MY_TOP"/scripts/colors.sh
source  "$HERE"/colors.sh


#relpath()
#{
#    realpath --relative-to="$1" "$2"
#}
#echo -e "${BLUE}here $HERE${ENDCOLOR}"
#pwd
#echo "root $NODE_ROOT"
#echo `relpath $MY_TOP, $TARGET`


create_symlink()
{
  if [ ! -d "$TARGET" ]; then
    echo -e  "${CYAN}Creating symbolic link ...${ENDCOLOR}"
    ln -s  "$HERE/.."  "$TARGET"
#    ln -snf "$HERE/.." "$TARGET"
    if [ $? != 0 ]; then
      echo -e  "${RED}Can't create symbolic link.${ENDCOLOR}"
    else
      echo -e  "${GREEN}Symbolic link created.${ENDCOLOR}"
    fi
#  else
#    echo -e  "${GREEN}Symbolic link not needed.${ENDCOLOR}"
  fi
}


remove_symlink()
{
  if [ -L "$TARGET" ]; then
    rm  "$TARGET"  #need to remove symlink before npm will install anything (known issue with recent npm)
    if [ $? != 0 ]; then
      echo -e  "${RED}Can't remove symbolic link.${ENDCOLOR}"
    else
      echo -e  "${GREEN}Symbolic link removed.${ENDCOLOR}"
      npm install  #kludge: npm won't run with symlink, so try it again
    fi
  fi
}


install_deps()
{
#  MISSING=0
  for dep in "$DEPS"; do
    echo "checking if dep $dep is installed"
    installed=$(apt list $dep)
    if [[ "$installed" != *"[installed]"* ]]; then
#      MISSING=1
      echo -e  "${CYAN}Installing dependencies ...${ENDCOLOR}"
      sudo  apt-get install  "$DEPS"
      if [ $? != 0 ]; then
        echo -e  "${RED}Can't install dependencies.${ENDCOLOR}"
      else
        echo -e  "${GREEN}Dependencies installed.${ENDCOLOR}"
      fi
      return
    fi
  done
#  if [ "$MISSING" != "0" ]; then
#    echo -e  "${CYAN}Installing dependencies ...${ENDCOLOR}"
#    sudo  apt-get install  "$DEPS"
#    if [ $? != 0 ]; then
#      echo -e  "${RED}Can't install dependencies.${ENDCOLOR}"
#    else
#      echo -e  "${GREEN}Dependencies installed.${ENDCOLOR}"
#    fi
#  else
#    echo -e  "${GREEN}Dependencies already installed.${ENDCOLOR}"
#  fi
}


if [ "x$1" == "xpre" ]; then #pre-install
  remove_symlink
  install_deps
  npm install nan  #kludge: avoid intermittent "Cannot find module 'nan'" errors
elif [ "x$1" == "xpost" ]; then #post-install
  create_symlink
else
  echo -e  "${RED}Unknown option: '$1'${ENDCOLOR}"
fi

#eof
