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
#define ENDCOLOR_ATLINE(srcline)  " &" << shortname(srcline? srcline: SRCLINE) << ANSI_COLOR("0") "\n"
//#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%s) //%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(SRCLINE) //__LINE__)

//typedef struct { int line; } SRCLINE; //allow compiler to distinguish param types, prevent implicit conversion
//typedef int SRCLINE;
//#define _GNU_SOURCE //select GNU version of basename()
//#include <string.h>

#define SRCLINE  __FILE__ ":" TOSTR(__LINE__)
typedef const char* SrcLine;
const char* shortname(const char* srcline)
{
    strchr, strrchr, etc
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