#!/bin/bash
#send files to RPi
#set -x

HERE="$(dirname "$(readlink -fm "$0")")" #https://stackoverflow.com/questions/20196034/retrieve-parent-directory-of-script
#MY_TOP=`git rev-parse --show-toplevel`  #from https://unix.stackexchange.com/questions/6463/find-searching-in-parent-directories-instead-of-subdirectories
#source  "$MY_TOP"/scripts/colors.sh
source  ./colors.sh

#echo "i am at $HERE"
#echo use script "$HERE"/getcfg.js
#echo -e "${BLUE}running $HERE/getcfg.js${NORMAL}"
echo -e "${BLUE}setting vars${NORMAL}"
"$HERE"/getcfg.js
eval $("$HERE"/getcfg.js)

if [ $# -lt 1 ]; then
    echo -e "${RED}no files to transfer?${NORMAL}"
else
    for file in "$@"
    do
        echo -e "${BLUE}xfr ${CYAN}$file ${BLUE}at `date`${NORMAL}"
#        echo sshpass -p $RPi_pass scp "$file" $RPi_user@$RPi_addr:$RPi_folder
        sshpass -p $RPi_pass scp "$file" $RPi_user@$RPi_addr:$RPi_folder
    done
    echo -e "${GREEN}$# files xfred.${NORMAL}"
fi

#eof#
