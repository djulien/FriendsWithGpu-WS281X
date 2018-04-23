#!/bin/bash -x
echo -e '\e[1;36m'; OPT=3; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
///////////////////////////////////////////////////////////////////////////////
////
/// Recursive #include example
//

#include <iostream> //std::cout, std::flush


//#define MSG(msg)  { Thing thing; std::cout << msg << "\n" << std::flush; }
#define USE_ARG3(one, two, three, ...)  three
#define MSG_1ARG(msg)  { Thing x; std::cout << msg << std::flush; }
#define MSG_2ARGS(msg, bypass)  { if (!(bypass)) MSG_1ARG(msg); } // /*if (recursion && nested.isNested()) return*/; MSG_1ARG(msg); }
#define MSG(...)  USE_ARG3(__VA_ARGS__, MSG_2ARGS, MSG_1ARG) (__VA_ARGS__) //accept 1 or 2 macro args

//class Nesting
//{
//    static int count = 0;
//public: //ctor/dtor
//    Nesting() { ++count; }
//    ~Nesting() { --count; }
//public: //methods
//    bool isNested() { return (count > 0); }
//};

class Thing
{
//    static int nested = 0;
    int& nested() { static int count = 0; return count; }
public: //ctor/dtor
    Thing() { MSG("thing ctor", nested()++); }
    ~Thing() { MSG("thing dtor", --nested()); }
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