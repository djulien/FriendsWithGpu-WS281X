//console msg:

#ifndef _CONSOLE_MSG_H
#define _CONSOLE_MSG_H

#include <iostream> //std::cout, std::flush
#include "toponly.h"


//#define MSG(msg)  { Thing thing; std::cout << msg << "\n" << std::flush; }
#define USE_ARG3(one, two, three, ...)  three
#define MSG_1ARG(msg)  { TopOnly x; std::cout << msg << "\n" << std::flush; }
#define MSG_2ARGS(msg, want_msg)  { if (want_msg) MSG_1ARG(msg); } // /*if (recursion && nested.isNested()) return*/; MSG_1ARG(msg); }
#define MSG(...)  USE_ARG3(__VA_ARGS__, MSG_2ARGS, MSG_1ARG) (__VA_ARGS__) //accept 1 or 2 macro args


#endif //ndef _CONSOLE_MSG_H