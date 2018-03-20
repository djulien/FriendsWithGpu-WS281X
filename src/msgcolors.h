//#!/bin/bash -x
//echo -e '\e[1;36m'; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -DUNIT_TEST -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
//#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)


//console colors

#ifndef _COLORS_H
#define _COLORS_H

//macro expansion helpers:
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
#define PINK_MSG  MAGENTA_MSG //easier to spell :)
#define CYAN_MSG  ANSI_COLOR("1;36")
#define GRAY_MSG  ANSI_COLOR("0;37")
#define ENDCOLOR_NOLINE  ANSI_COLOR("0")
//append the src line# to make debug easier:
//#define ENDCOLOR_ATLINE(srcline)  " &" TOSTR(srcline) ANSI_COLOR("0") "\n"
#define ENDCOLOR_ATLINE(srcline)  "  &" << shortsrc(srcline, SRCLINE) << ENDCOLOR_NOLINE "\n"
//#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%s) //%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(SRCLINE) //__LINE__)

//typedef struct { int line; } SRCLINE; //allow compiler to distinguish param types, prevent implicit conversion
//typedef int SRCLINE;
//#define _GNU_SOURCE //select GNU version of basename()
#include <stdio.h> //snprintf()
//#include <stdlib.h> //atoi()
#include <string.h>
#include <dirent.h> //opendir(), readdir(), closedir()
//#include <memory> //std::unique_ptr<>
//#include <stdio.h> 


#ifndef SIZEOF
 #define SIZEOF(thing)  (sizeof(thing) / sizeof((thing)[0]))
#endif

//smart ptr wrapper for DIR*:
class Dir
{
public: //ctor/dtor
    Dir(const char* dirname): m_ptr(opendir(dirname)) {}
    ~Dir() { if (m_ptr) closedir(m_ptr); }
public: //operators
    operator bool() { return m_ptr; }
public: //methods:
    struct dirent* next() { return m_ptr? readdir(m_ptr): nullptr; }
private: //data
    DIR* m_ptr;
};


//check for ambiguous base file name:
static bool isunique(const char* folder, const char* basename, const char* ext)
{
//    static std::map<const char*, const char*> exts;
    static struct { char name[32]; int count; } cached[10] = {0}; //base file name + #matching files
    if (!ext) return false; //don't check if no extension
    size_t baselen = std::min<size_t>(ext - basename + 1, sizeof(cached->name)); //ext - basename;

//    if (baselen <= sizeof(cached[0].name))
//first check filename cache:
    auto cacheptr = cached; //NOTE: need initializer so compiler can deduce type
    for (/*auto*/ cacheptr = cached; (cacheptr < cached + SIZEOF(cached)) && cacheptr->name[0]; ++cacheptr)
//    while ((++cacheptr < cached + SIZEOF(cached)) && cacheptr->name[0])
        if (!strncmp(basename, cacheptr->name, baselen)) return (cacheptr->count == 1);
//        else std::cout << "cmp[" << (cacheptr - cached) << "] '" << cacheptr->name << "' vs. " << baselen << ":'" << basename << "'? " << strncmp(basename, cacheptr->name, baselen) << "\n" << std::flush;
//        else if (!cacheptr->name[0]) nextptr = cacheptr;
//next go out and count (could be expensive):
//std::cout << "dir len " << (basename - folder - 1) << ", base len " << (ext - basename) << "\n" << std::flush;
    std::string dirname(folder, (basename != folder)? basename - folder - 1: 0);
//    std::unique_ptr<DIR> dir(opendir(dirname.length()? dirname.c_str(): "."), [](DIR* dirp) { closedir(dirp); });
    Dir dir(dirname.length()? dirname.c_str(): ".");
    if (!dir) return false;
    int count = 0;
    struct dirent* direntp;
    while (direntp = dir.next())
        if (direntp->d_type == DT_REG)
        {
            const char* bp = strrchr(direntp->d_name, '.');
            if (!bp) bp = direntp->d_name + strlen(direntp->d_name);
//            if (bp - direntp->d_name != baselen) continue;
            if (strncmp(direntp->d_name, basename, baselen)) continue;
//std::cout << direntp->d_name << " matches " << baselen << ":" << basename << "\n" << std::flush;
            ++count;
        }
//if there's enough room, cache results for later:
    if (cacheptr < cached + SIZEOF(cached)) { strncpy(cacheptr->name, basename, baselen); cacheptr->count = count; }
//    std::cout << "atline::isunique('" << folder << "', '" << basename << "', '" << ext << "') = [" << (cacheptr - cached) << "] '" << cacheptr->name << "', " << count << "\n" << std::flush;
    return (count == 1);
}


#define SRCLINE  __FILE__ ":" TOSTR(__LINE__)
typedef const char* SrcLine; //allow compiler to distinguish param types, catch implicit conv
SrcLine shortsrc(SrcLine srcline, SrcLine defline) //int line = 0)
{
    static char buf[60]; //static to preserve after return to caller
    if (!srcline) srcline = defline;
    if (!srcline) srcline = SRCLINE;
//std::cout << "raw file " << srcline << "\n" << std::flush;
    const char* svsrc = srcline; //dirname = strdup(srcline);

    const char* bp = strrchr(srcline, '/');
    if (bp) srcline = bp + 1; //drop parent folder name
//    const char* endp = strrchr(srcline, '.'); //drop extension
//    if (!endp) endp = srcline + strlen(srcline);
//    if (!line)
//    {
        bp = strrchr(srcline, ':');
        if (!bp) bp = ":#?"; //endp + strlen(endp);
//        if (bp) line = atoi(bp+1);
//    }
    const char* extp = strrchr(srcline, '.');
    if (!extp || !isunique(svsrc, srcline, extp)) extp = srcline + std::min<size_t>(bp - srcline, strlen(srcline)); //drop extension if unambiguous
    snprintf(buf, sizeof(buf), "%.*s%s", (int)(extp - srcline), srcline, bp); //line);
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