#!/bin/bash -x
echo -e '\e[1;36m'; g++ -E -D__THISFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __THISFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line); add "-E" to above to see pre-processed src

//optional macro parameters test
//see https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//for std way to do it without ##: https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick?noredirect=1&lq=1

#include <iostream> //std::cout, std::flush

enum { plain = 0, bold = 1, italic = 2 };

void PrintString(const char* message, int size, int style)
{
    std::cout << "msg '" << message << "', size " << size << ", style " << style << "\n" << std::flush;
}

// The multiple macros that you would need anyway [as per: Crazy Eddie]
#define xPRINT_STRING_0_ARGS()              PrintString("(no message)", 0, 0)
#define xPRINT_STRING_1_ARGS(message)              PrintString(message, 0, 0)
#define xPRINT_STRING_2_ARGS(message, size)        PrintString(message, size, 0)
#define xPRINT_STRING_3_ARGS(message, size, style) PrintString(message, size, style)
//#define PRINT_STRING_4_ARGS(message, size, style, ...) PrintString(message, size, style)

#define xGET_ARG4(x, arg1, arg2, arg3, arg4, ...)  arg4
// The interim macro that simply strips the excess and ends up with the required macro
//#define XXX_X(x,A,B,C,D,FUNC, ...)  FUNC  

//#define PRINT_STRING_MACRO_CHOOSER(...)  GET_4TH_ARG(__VA_ARGS__, PRINT_STRING_3_ARGS, PRINT_STRING_2_ARGS, PRINT_STRING_1_ARGS, )
//#define PRINT_STRING(...)  PRINT_STRING_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
//no worky: #define VA_ARGS(...) , ##__VA_ARGS__
#define NUM(...)   NUM_INNER(__VA_ARGS__), 
#define PRINT_STRING(...)  GET_ARG4("kludge " __VA_ARGS__, PRINT_STRING_3_ARGS, PRINT_STRING_2_ARGS, PRINT_STRING_1_ARGS, PRINT_STRING_0_ARGS) (__VA_ARGS__)
//#define PRINT_STRING(...)  GET_ARG4(__VA_OPT__(,) __VA_ARGS__, PRINT_STRING_3_ARGS, PRINT_STRING_2_ARGS, PRINT_STRING_1_ARGS, PRINT_STRING_0_ARGS) (__VA_ARGS__)


//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wvariadic-macros"
//#define FOO(A, ...) foo(A, ##__VA_ARGS__)
//#pragma GCC diagnostic pop

// The macro that the programmer uses 
//#define XXX(...)                    XXX_X(,##__VA_ARGS__,\x
//                                          XXX_4(__VA_ARGS__),\x
//                                          XXX_3(__VA_ARGS__),\x
//                                          XXX_2(__VA_ARGS__),\x
//                                          XXX_1(__VA_ARGS__),\x
//                                          XXX_0(__VA_ARGS__)\x
//                                         ) 



#if 0
/* expands to the first argument */
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

#define BAR(...) printf(FIRST(__VA_ARGS__) "\n" REST(__VA_ARGS__))

/*
 * if there's only one argument, expands to nothing.  if there is more
 * than one argument, expands to a comma followed by everything but
 * the first argument.  only supports up to 9 arguments but can be
 * trivially expanded.
 */
#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...) \
    SELECT_10TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_10TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10

int main(int argc, char *argv[])
{
    BAR("first test");
    BAR("second test: %s", "a string");
    return 0;
}
#endif


int main(int argc, char * const argv[])
{
    PRINT_STRING();
    PRINT_STRING("Hello World!");
    PRINT_STRING("Hello World!", 18);
    PRINT_STRING("Hello World!", 18, bold);

    return 0;
}




//eof