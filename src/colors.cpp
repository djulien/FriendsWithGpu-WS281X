#!./self-compile.sh
//#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)

// #!/bin/bash -x
// echo -e '\e[1;36m'; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
// #line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)

#include <iostream>
#include "colors.h"

void func(int a, SrcLine srcline = 0)
{
    std::cout << BLUE_MSG << "hello " << a << " from" << ENDCOLOR;
    std::cout << CYAN_MSG << "hello " << a << " from" << ENDCOLOR_ATLINE(srcline);
}


int main(int argc, const char* argv[])
{
    std::cout << BLUE_MSG << "start" << ENDCOLOR;
    func(1);
    func(2, SRCLINE);
    return 0;
}