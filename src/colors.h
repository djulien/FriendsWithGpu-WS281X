#!/bin/bash -x
echo -e '\e[1;36m'; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -DUNIT_TEST -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)


//console colors

#ifndef _COLORS_H
#define _COLORS_H


#define TOSTR(str)  TOSTR_NESTED(str)
#define TOSTR_NESTED(str)  #str

//ANSI color codes (for console output):
//https://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_COLOR(code)  "\x1b[" code "m"
#define RED_MSG  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_MSG  ANSI_COLOR("1;32")
#define YELLOW_MSG  ANSI_COLOR("1;33")
#define BLUE_MSG  ANSI_COLOR("1;34")
#define MAGENTA_MSG  ANSI_COLOR("1;35")
#define PINK_MSG  MAGENTA_MSG
#define CYAN_MSG  ANSI_COLOR("1;36")
#define GRAY_MSG  ANSI_COLOR("0;37")
//#define ENDCOLOR  ANSI_COLOR("0")
//append the src line# to make debug easier:
//#define ENDCOLOR_ATLINE(srcline)  " &" TOSTR(srcline) ANSI_COLOR("0") "\n"
#define ENDCOLOR_ATLINE(srcline)  "  &" << shortsrc(srcline? srcline: SRCLINE) << ANSI_COLOR("0") "\n"
//#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%s) //%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(SRCLINE) //__LINE__)

//typedef struct { int line; } SRCLINE; //allow compiler to distinguish param types, prevent implicit conversion
//typedef int SRCLINE;
//#define _GNU_SOURCE //select GNU version of basename()
#include <stdio.h> //snprintf()
#include <stdlib.h> //atoi()
#include <string.h>

#define SRCLINE  __FILE__ ":" TOSTR(__LINE__)
typedef const char* SrcLine; //allow compiler to distinguish param types, catch implicit conv
SrcLine shortsrc(SrcLine srcline, int line = 0)
{
    static char buf[60]; //static to preserve after return to caller
    const char* bp = strrchr(srcline, '/');
    if (bp) srcline = bp + 1; //drop parent folder name
    const char* endp = strrchr(srcline, '.'); //drop extension
    if (!endp) endp = srcline + strlen(srcline);
    if (!line)
    {
        bp = strrchr(endp, ':');
        if (bp) line = atoi(bp+1);
    }
    snprintf(buf, sizeof(buf), "%.*s:%d", (int)(endp - srcline), srcline, line);
    return buf;
}
#if 0
class SRCLINE
{
    int m_line;
public: //ctor/dtor
    explicit SRCLINE(int line): m_line(line) {}
public: //opeartors
    inline operator bool() { return m_line != 0; }
//    static SRCLINE FromInt(int line)
//    {
//        SRCLINE retval;
//        retval.m_line = line;
//        return retval;
//    }
};
#endif
//#define ENDCOLOR_LINE(line)  FMT(ENDCOLOR_MYLINE) << (line? line: __LINE__) //show caller line# if available

#endif //ndef _COLORS_H


#ifdef UNIT_TEST
//echo -e '\e[1;36m'; g++ -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${1%.*}"; echo -e '\e[0m'

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
#endif //def UNIT_TEST