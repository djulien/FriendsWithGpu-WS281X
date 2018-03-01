#!/bin/bash -x
#GCC="g++ -D__SRCFILE__=\"${BASH_SOURCE##*/}\" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o \"${BASH_SOURCE%.*}\" -x c++ -"
GCC="g++ -D__SRCFILE__=\"${BASH_SOURCE##*/}\" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -x c++ -"
echo -e '\e[1;36m'; $GCC -E <<//EOF | grep -v "^echo" | grep -v "^GCC=" | $GCC -o "${BASH_SOURCE%.*}"; echo -e '\e[0m'
#line 6 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)


#include "unit.h"

#include <iostream>
//#include "unit.h"

int main(int argc, const char* argv[])
{
    std::cout << "hello, started\n" << std::flush;
    int x = func(10);
    std::cout << "result = " << x << "\n" << std::flush;
    return 0;
}
