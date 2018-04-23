#!/bin/bash -x
echo -e '\e[1;36m'; OPT=0; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
///////////////////////////////////////////////////////////////////////////////
////
/// Recursive #include example
//

#include <stdexcept> //std::runtime_error


#include "toponly.h"


class Thing: public TopOnly
{
public: //ctor/dtor
    CTOR_PERFECT_FWD(Thing, TopOnly) { MSG("Thing: top? " << istop())}
    ~Thing() { MSG("~Thing: top? " << istop()); }
};


int main(int argc, const char* argv[])
{
    MSG("test1");
    MSG("test2", 0);
    MSG("test3", 1);
    Thing x;

    return 0;
}
//eof