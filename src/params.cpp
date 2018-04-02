#!/bin/bash -x
echo -e '\e[1;36m'; OPT=0; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
///////////////////////////////////////////////////////////////////////////////
////
/// Optional/named param example
//

//based on https://marcoarena.wordpress.com/2014/12/16/bring-named-parameters-in-modern-cpp/
//TEST_F(SomeDiceGameConfig, JustTwoTurnsGame)
//{
//   auto gameConfig = CreateGameConfig( [](auto& r) {
//       r.NumberOfDice = 5u;
//       r.MaxDiceValue = 6;
//       r.NumberOfTurns = 2u;
//   });
//}

//to see compiled code:
//objdump -CSr params

class other
{
    int m_int;
public:
    explicit other(int i = 0): m_int(i) {}
    ~other() {}
};


//#define ParamDef(name, type, defval)  
//class name  
//{  
//public:  
//    type name;  
//    explicit name(type name = defval): name(name) {}  
//}
#include <string>
#include <iostream> //std::cout, std::flush
#include "ostrfmt.h" //FMT()
//#include "atomic.h" //ATOMIC_MSG()
#include "msgcolors.h" //SrcLine, msg colors
#define MSG(msg)  { std::cout << msg << std::flush; }

class API
{
public: //member vars (caller can set these via ctor params):
//    ParamDef(int, i, 0);
//    ParamDef(bool, b, false);
//    ParamDef(std::string, s, "");
//    ParamDef(other*, o, nullptr);
    int i = 0;
    bool b = false;
    std::string s;
    SrcLine srcline = 0;
private: //member vars (caller *cannot* set these directly):
    other* o = nullptr;
//    {
//    public: //ctors/dtors
//        explicit params() { reset(); }
//        ~params() {}
//    public: //methods
//        void reset() { i = 0; b = false; s = ""; o = nullptr; }
//    };
//    template <typename Params>
//    typedef void (*GetParams)(Params& p); // = 0);
//    using GetParams = void (*GetParams)(typename Params& p); // = 0); //https://stackoverflow.com/questions/19192122/template-declaration-of-typedef-typename-footbar-bar?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
public: //ctors/dtors
//    class CtorParams: public i, public b, public s, public o {};
//    explicit api(GetParams<api> get_params = 0)
    explicit API(void (*get_params)(API& p) = 0)
    {
//        CtorParams params = {i, b, s, o}; //allow caller to set my member vars
        if (get_params) get_params(*this); //set member vars directly
//        std::cout << "ctor: "; show(); std::cout << "\n" <<std::flush;
        MSG(BLUE_MSG << "ctor: " << show() << ENDCOLOR_ATLINE(srcline));
    }
    ~API() { MSG(BLUE_MSG << "dtor: " << show() << ENDCOLOR_ATLINE(srcline)); }
public: //operators
    friend std::ostream& operator<<(std::ostream& ostrm, const API& api) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "{ i = " << api.i << ", b = " << api.b << ", s = '" << api.s << FMT("', o = %p") << api.o << " }";
        return ostrm;
    }
public: //methods
//    class FuncParams: public s, public i, public b;
//    typedef struct { std::string s; int i; bool b; } FuncParams; //use typedef in function cb sig so caller can use auto type in lambda function
    struct FuncParams { std::string s = "none"; int i = 999; bool b = true; SrcLine srcline = SRCLINE; }; //FuncParams() { s = "none"; i = 999; b = true; }}; // FuncParams;
//    struct FuncParams_t: FuncParams { FuncParams my_t; };
//    void func(GetParams<FuncParams> get_params = 0)
    void func(void (*get_params)(struct FuncParams& p) = 0)
    {
        /*static*/ struct FuncParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
        if (get_params) get_params(params);
//        func(params);
//    }
//    void func(FuncParams& params)
//    {
//        std::cout << "func: " << show() << "\n" << std::flush;
//apply func params to member vars:
        s += params.s;
        i += params.i;
        b |= params.b;
//        std::cout << "func: "; show(); std::cout << "\n" <<std::flush;
        MSG(BLUE_MSG << "func: " << show() << ENDCOLOR_ATLINE(params.srcline));
    }
private: //helpers
    API& show() { return *this; } //dummy function for readability
//    void show() { std::cout << "i " << i << ", b " << b << ", s '" << s << "', o " << o << std::flush; }
//    friend API& operator<<(std::ostream& strm, const API& api)
//    {
//        return strm << "i = " << api.i << ", b = " << api.b << ", s = '" << api.s << FMT("', o = %p") << api.o;
//    }
//    streambuf* sb );
};


#if 0
void mylambda(api::FuncParams& params)
{
    struct my_params: public std::decay_t<decltype(params)>
    {
//        int i; std::string s;
        my_params() { i = 222; s = "strstrstr"; }
    };
    my_params x;
    std::cout << sizeof(params) << ", " << sizeof(my_params) << "\n" << std::flush;
    new (&params) struct my_params();
}
#endif

//#include <typeinfo>
//#define PARAMS(name)  [](auto& name)
#include <type_traits> //std::decay_t<>
#include <iostream> //std::cout, std::flush
int main(int argc, const char* argv[])
{
    std::cout << "api " << sizeof(API) << ", "
        << "str " << sizeof(std::string) << ", "
        << "int " << sizeof(int) << ", "
        << "bool " << sizeof(bool) << ", "
        << "othr " << sizeof(other*) << ", "
        << "fpm " << sizeof(API::FuncParams) << ", "
        << "\n" << std::flush;

    API A;
//    A.func(PARAMS(p) { p.i = 222; p.s = "strstrstr"; });

//#define PLIST(stmts)  [](auto& p){ std::string& s = p.s; int& i = p.i; bool& b = p.b; stmts; }
//    A.func(PLIST( i = 222; s = "strstrstr"; ));

//kludge: specialize param struct to allow selective setting of named members without using parent struct name:
//use placement new to avoid copy ctor
//see https://stackoverflow.com/questions/12122234/how-to-initialize-a-struct-using-the-c-style-while-using-the-g-compiler/14206226#14206226
//no (returns adrs of temp); use lambda param instead: #define PLIST(stmts)  []() -> api::FuncParams* { struct my_param_list: api::FuncParams { my_param_list() { stmts; }}; return &my_param_list(); }
//NOTE: need c++14 for auto lambda params
//#define PLIST(stmts)  [](auto& p){ struct my_params: /*public api::FuncParams*/ decltype(p) { my_params() { /*stmts*/; std::cout << typeid(p).name() << "\n" << std::flush; }}; new (&p) struct my_params(); }
//#define PLIST(stmts)  [](auto& p){ typedef /*decltype(p)*/ struct api::FuncParams plist; /*std::cout << typeid(p).name() << "\n" << std::flush*/; struct my_params: plist { my_params() { stmts; }}; } //new (&p) struct my_params(); }
#define PLIST1(stmts)  [](auto& params){ struct my_params: API::FuncParams { my_params() { stmts; }}; new (&params) struct my_params(); }
#define PLIST2(stmts)  [](auto& params){ struct my_params: API { my_params() { stmts; }}; new (&params) struct my_params(); }
#define PLIST(stmts)  [](auto params){ struct my_params: public std::decay_t<decltype(params)> { my_params() { stmts; }}; my_params x; std::cout << sizeof(params) << ", " << sizeof(my_params) << "\n" << std::flush; new (&params) struct my_params(); }
    A.func(PLIST2( i = 222; s = "strstrstr"; srcline = SRCLINE; ));
//    A.func(mylambda);
#if 0
    A.func([](auto& params)
    {
        struct my_params: public std::decay_t<decltype(params)>
        {
            my_params() { i = 222; s = "strstrstr"; }
        };
        my_params x;
        std::cout << sizeof(params) << ", " << sizeof(my_params) << "\n" << std::flush;
        new (&params) struct my_params();
    });
    template<typename ... ARGS>
    specialize(ARGS&& ... args)
    {
        new (m_ptr) shm_type(std::forward<ARGS>(args) ...); //, srcline); //pass args to TYPE's ctor (perfect fwding)
    };
#endif
    A.func(PLIST1( i = 333; srcline = SRCLINE; ));

    API B(PLIST2( i = 2; b = true; s = "str"; /*o = new other(4)*/; srcline = SRCLINE; ));
    B.func();
    B.func(PLIST1( i = 55; srcline = SRCLINE; ));

    return 0;
}
//eof