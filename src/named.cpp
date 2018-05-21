#!/bin/bash -x
echo -e '\e[1;36m'; OPT=3; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
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

//NOTE: default is g++ 5.4 on ubuntu 16.04
//NO-NOTE: needs g++-6:
//to see which compiler(s) installed:  ls /usr/bin | grep g[c+][c+]
//to change default compiler version:  sudo update-alternatives --config gcc
//https://askubuntu.com/questions/466651/how-do-i-use-the-latest-gcc-on-ubuntu
// gcc -v; g++ -v
// sudo add-apt-repository ppa:ubuntu-toolchain-r/test
// sudo apt update
// sudo apt-get install gcc-6 g++-6
// sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-6
// gcc -v; g++ -v


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

//void hello() { MSG("hello"); }

//#define NAMED(stmts)  [](auto& p){ srcline = SRCLINE; stmts; }
#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ /*&*/ [&](auto& _)
#endif

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
//    explicit API(void (*get_params)(API& p) = 0) //: i(0), b(false), srcline(0), o(nullptr)
//====
//    PERFECT_FWD2BASE_CTOR(ShmPtr, TYPE)
//    template<typename ... ARGS>
//    explicit ShmPtr(ARGS&& ... args): ShmPtr_params(SRCLINE /*"preserve base"*/, ShmKey, Extra, WantReInit, DebugFree), m_ptr(0), m_want_init(WantReInit), m_debug_free(DebugFree) //kludge: preserve ShmPtr_params
//        new (m_ptr) shm_type(std::forward<ARGS>(args) ...); //, srcline); //pass args to TYPE's ctor (perfect fwding)
//====
//#define API_CTOR  (int& i, std::string& s, bool& b, SrcLine& srcline)
//    explicit API(void (*get_params)(int& i, std::string& s, bool& b, SrcLine& srcline) = 0) //: i(0), b(false), srcline(0), o(nullptr)
//    explicit API(SrcLine mySrcLine = 0, void (*get_params)(API&) = 0) //int& i, std::string& s, bool& b, SrcLine& srcline) = 0) //: i(0), b(false), srcline(0), o(nullptr)
    explicit API(/*SrcLine srcline = 0*/): API(/*srcline,*/ NAMED{}) {} //= 0) //void (*get_params)(struct FuncParams&) = 0)
    template <typename CALLBACK>
    explicit API(/*SrcLine mySrcLine = 0,*/ CALLBACK&& get_params /*= 0*/) //void (*get_params)(API&) = 0) //int& i, std::string& s, bool& b, SrcLine& srcline) = 0) //: i(0), b(false), srcline(0), o(nullptr)
//    explicit API(void (*get_params) API_CTOR = 0) //: i(0), b(false), srcline(0), o(nullptr)
//#define ctor(stmts)  [](int& i, std::string& s, bool& b, SrcLine& srcline) stmts
    {
//        CtorParams params = {i, b, s, o}; //allow caller to set my member vars
//        if (mySrcLine) srcline = mySrcLine;
//        if (get_params != (CALLBACK&&)0) //get named params from caller
//        {
//            auto callback = *get_params;
//            auto thunk = [](void* arg, API& params){ if (arg) (*static_cast<decltype(callback)*>(arg))(params); }; //NOTE: must be captureless, so wrap it
        auto thunk = [](auto get_params, API& params){ /*(*static_cast<decltype(callback)*>(arg))*/ get_params(params); }; //NOTE: must be captureless, so wrap it
//            get_params(*this); //(i, s, b, srcline); //(*this); //set member vars directly
        thunk(get_params, *this); //get named params from caller
//        }
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
//    /*struct*/ class FuncParams { public: std::string s; int i; bool b; SrcLine srcline; FuncParams(): s("none"), i(999), b(true), srcline(SRCLINE) {} }; //FuncParams() { s = "none"; i = 999; b = true; }}; // FuncParams;
    struct FuncParams { std::string s = "none"; int i = 999; bool b = true; SrcLine srcline = SRCLINE; }; //FuncParams() { s = "none"; i = 999; b = true; }}; // FuncParams;
//    typedef struct { std::string s /*= "none"*/; int i /*= 999*/; bool b /*= true*/; SrcLine srcline /*= 0*/; } FuncParams; //() { s = "none"; i = 999; b = true; }}; // FuncParams;
//    struct FuncParams_t: FuncParams { FuncParams my_t; };
//    void func(GetParams<FuncParams> get_params = 0)
//    void func(void (*get_params)(struct FuncParams& p) = 0)
//#define API_FUNC  (int& i, std::string& s, bool& b, SrcLine& srcline)
//    void func(void (*get_params)(int& i, std::string& s, bool& b, SrcLine& srcline) = 0)
//    void func(SrcLine mySrcLine = 0, void (*get_params)(struct FuncParams&) = 0)
    void func(/*SrcLine srcline = 0*/) { func(/*srcline,*/ NAMED{}); } //= 0) //void (*get_params)(struct FuncParams&) = 0)
    template <typename CALLBACK>
    void func(/*SrcLine mySrcLine = 0,*/ CALLBACK&& get_params /*= 0*/) //void (*get_params)(struct FuncParams&) = 0)
    {
        /*static*/ struct FuncParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params != (CALLBACK&&)0) //get named params from caller
//        {
//        decltype(callback_arg) callback = callback_arg;
//            auto callback = *get_params;
//            auto thunk = [](void* arg, struct FuncParams& params){ if (arg) (*static_cast<decltype(callback)*>(arg))(params); }; //NOTE: must be captureless, so wrap it
        auto thunk = [](auto get_params, struct FuncParams& params){ /*(static_cast<decltype(callback)>(arg))*/ get_params(params); }; //NOTE: must be captureless, so wrap it
//        do_something(thunk, callback);
//            get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
        thunk(get_params, params); //get named params from caller
//        }
//    void func(void (*get_params) API_FUNC = 0)
//#define func(stmts)  func([](int& i, std::string& s, bool& b, SrcLine& srcline) { stmts })
//        hello();
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


struct Unpacked {}; //ctor disambiguation tag
template <typename UNPACKED, typename CALLBACK>
static UNPACKED& unpack(UNPACKED& params, CALLBACK&& named_params)
{
//        static struct CtorParams params; //need "static" to preserve address after return
//        struct CtorParams params; //reinit each time; comment out for sticky defaults
//        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
    auto thunk = [](auto get_params, UNPACKED& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
//        MSG(BLUE_MSG << "get params ..." << ENDCOLOR);
    thunk(named_params, params);
//        MSG(BLUE_MSG << "... got params" << ENDCOLOR);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
    MSG(BLUE_MSG << "unpack ret" << ENDCOLOR);
    return params;
}


class API2
{
    class Nested
    {
        int m_i;
        std::string m_s;
        SrcLine m_srcline;
    public: //ctor/dtor
        explicit Nested(int i = 0, std::string s = "hello", SrcLine srcline = 0): m_i(i), m_s(s), m_srcline(srcline) { MSG(CYAN_MSG << "Nested ctor" << ENDCOLOR_ATLINE(m_srcline)); }
        ~Nested() { MSG(CYAN_MSG << "Nested dtor" << ENDCOLOR_ATLINE(m_srcline)); }
    public: //operators
        /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Nested& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        { 
            ostrm << "i " << me.m_i << ", s '" << me.m_s << "', srcline " << shortsrc(me.m_srcline, SRCLINE);
            return ostrm;
        }
    public: //methods
        void thing(bool b = false, SrcLine srcline = 0) { MSG(BLUE_MSG << "Nested::thing: b " << b << ", " << *this << ENDCOLOR_ATLINE(srcline)); }
    public: //named variants
            struct CtorParams //NOTE: needs to be exposed for ctor chaining
            {
                int i = 56;
                std::string s = "78";
                SrcLine srcline = 0;
            };
        template <typename CALLBACK>
        explicit Nested(CALLBACK&& named_params): Nested(unpack(temp_params, named_params), Unpacked{}) {}

        template <typename CALLBACK>
        void thing(CALLBACK&& named_params)
        {
            struct ThingParams
            {
                bool b = true;
                SrcLine srcline = 0; //SRCLINE; //debug call stack
            };
            /*static*/ struct ThingParams params; //reinit each time; comment out for sticky defaults
            auto thunk = [](auto get_params, struct ThingParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
            thunk(named_params, params);
            thing(params.b, params.srcline);
        }
private:
        struct CtorParams temp_params; //need a place to unpack params (instance-specific)
        explicit Nested(const CtorParams& params, Unpacked): Nested(params.i, params.s, params.srcline) {}
    };
    bool m_b;
    int m_i;
    Nested m_n;
    SrcLine m_srcline;
public: //ctor/dtor
    explicit API2(bool b = false, int i = -1, std::string s = "huh?", SrcLine srcline = 0): m_b(b), m_i(i), m_n(i, s, srcline), m_srcline(srcline) { MSG(CYAN_MSG << "API2 ctor" << ENDCOLOR_ATLINE(m_srcline)); }
    ~API2() { MSG(CYAN_MSG << "API2 dtor" << ENDCOLOR_ATLINE(m_srcline)); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const API2& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
//        std::cout << "srcline = " << FMT("%p") << me.m_srcline << "\n" << std::flush;
        ostrm << "b " << me.m_b << ", " << me.m_i << ", srcline " << shortsrc(me.m_srcline, SRCLINE) << ", nested {" << me.m_n << "}";
        return ostrm;
    }
public: //methods
    void func(int i = 2, SrcLine srcline = 0) { MSG(BLUE_MSG << "API2::func: i " << i << ENDCOLOR_ATLINE(srcline)); m_n.thing(i != 2, srcline); }
public: //named variants
        struct CtorParams: public Nested::CtorParams //NOTE: needs to be exposed for ctor chaining
        {
            bool b = false;
            int i = 1234;
            SrcLine srcline = 0;
//            CtorParams() { MSG("CtorParams ctor\n"); }
//            ~CtorParams() { MSG("CtorParams dtor\n"); }
        };
    template <typename CALLBACK>
    explicit API2(CALLBACK&& named_params): API2(unpack(temp_params, named_params), Unpacked{}) {}
#if 0
    template <typename CALLBACK>
    static struct CtorParams& unpack(struct CtorParams& params, CALLBACK&& named_params)
    {
//        static struct CtorParams params; //need "static" to preserve address after return
//        struct CtorParams params; //reinit each time; comment out for sticky defaults
//        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
        auto thunk = [](auto get_params, struct CtorParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
//        MSG(BLUE_MSG << "get params ..." << ENDCOLOR);
        thunk(named_params, params);
//        MSG(BLUE_MSG << "... got params" << ENDCOLOR);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
        MSG(BLUE_MSG << "unpack ret" << ENDCOLOR);
        return params;
    }
#endif
#if 0
    template <typename CALLBACK>
    explicit API2(CALLBACK&& named_params)
    {
        struct CtorParams params;
        auto thunk = [](auto get_params, struct CtorParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        MSG(BLUE_MSG << "get params ..." << ENDCOLOR);
        thunk(named_params, params);
        MSG(BLUE_MSG << "... got params" << ENDCOLOR);
//no        API2(params.b, params.i, params.srcline); //NOTE: this creates a temp, not a ctor chain!
        MSG(BLUE_MSG << "ctor ret" << ENDCOLOR);
    }
#endif
    template <typename CALLBACK>
    void func(CALLBACK&& named_params)
    {
        struct FuncParams
        {
            bool i = 22;
            SrcLine srcline = 0; //SRCLINE; //debug call stack
        };
        /*static*/ struct FuncParams params; //reinit each time; comment out for sticky defaults
        auto thunk = [](auto get_params, struct FuncParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(named_params, params);
        func(params.i, params.srcline);
    }
private:
    explicit API2(const CtorParams& params, Unpacked): API2(params.b, params.i, params.s, params.srcline) {}
    struct CtorParams temp_params; //need a place to unpack params (instance-specific)
};


#if 0 //too complicated
//see https://stackoverflow.com/questions/28746744/passing-lambda-as-function-pointer
#include<type_traits>
#include<utility>
template<typename Callable>
union storage
{
    storage() {}
    std::decay_t<Callable> callable;
};
template<int, typename Callable, typename Ret, typename... Args>
auto fnptr_(Callable&& c, Ret (*)(Args...))
{
    static bool used = false;
    static storage<Callable> s;
    using type = decltype(s.callable);
    if (used) s.callable.~type();
    new (&s.callable) type(std::forward<Callable>(c));
    used = true;
    return [](Args... args) -> Ret { return Ret(s.callable(args...)); };
}
template<typename Fn, int N = 0, typename Callable>
Fn* fnptr(Callable&& c)
{
    return fnptr_<N>(std::forward<Callable>(c), (Fn*)nullptr);
}
void foo(void (*fn)()) { fn(); }
void lambda_test()
{
    int i = 42;
    auto fn = fnptr<void()>([i]{std::cout << i;});
    foo(fn);  // compiles!
}
#endif
#if 1 //better
//see http://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html
void do_something(void(*callback)(void*), void* callback_arg) { callback(callback_arg); }
template <typename = void>
class Something2
{
//    static auto thunk = [](void* arg){ (*static_cast<decltype(callback)*>(arg))(); }; //NOTE: thunk must be captureless
//#define THUNK(cb)  [](void* arg){ (*static_cast<decltype(cb)*>(arg))(); } //NOTE: must be captureless, so wrap it
public:
    template <typename THUNK, typename CALLBACK>
    static void do_something(THUNK&& callback, CALLBACK&& callback_arg) { callback(callback_arg); }

    template <typename CALLBACK>
    static void do_something2(CALLBACK&& callback_arg)
    {
//        decltype(callback_arg) callback = callback_arg;
        auto callback = *callback_arg;
        auto thunk = [](void* arg){ (*static_cast<decltype(callback)*>(arg))(); }; //NOTE: must be captureless, so wrap it
        thunk(callback_arg);
//        do_something(thunk, callback);
    }
};
//template <typename TYPE>
//auto thunk2 = [](TYPE&& arg){ (*arg)(); }; //NOTE: thunk must be captureless
void lambda_test()
{
    MSG(PINK_MSG << "lamba test" << ENDCOLOR);
    int num_callbacks = 0;
//pass the lambda function as callback arg and provide captureless thunk as callback function pointer:
    auto callback = [&](){ MSG("callback called " << ++num_callbacks << " times\n"); };

    auto thunk = [](void* arg){ (*static_cast<decltype(callback)*>(arg))(); }; //NOTE: thunk must be captureless
    do_something(thunk, &callback);
    do_something(thunk, &callback);
//    do_something(thunk, &callback);
//equiv to above using static members:
    Something2<> s;
    s.do_something(thunk, &callback);
    s.do_something(thunk, &callback);
//    s.do_something(thunk, &callback);
//equiv to above using buried thunk:
    s.do_something2(&callback);
    s.do_something2(&callback);
//    s.do_something2(&callback);
}
#endif


//#include <typeinfo>
//#define NAMED(name)  [&](auto& name)
#include <type_traits> //std::decay<>, std::remove_reference<>
#include <iostream> //std::cout, std::flush
int main(int argc, const char* argv[])
{
    lambda_test();

    std::cout << "api " << sizeof(API) << ", "
        << "str " << sizeof(std::string) << ", "
        << "int " << sizeof(int) << ", "
        << "bool " << sizeof(bool) << ", "
        << "othr " << sizeof(other*) << ", "
        << "fpm " << sizeof(API::FuncParams) << ", "
        << "\n" << std::flush;

    MSG(PINK_MSG << "API1 test" << ENDCOLOR);
    { //scope
    API A(NAMED{ SRCLINE; });
//    A.func(PARAMS(p) { p.i = 222; p.s = "strstrstr"; });

//#define PARAMS(stmts)  [](auto& p){ std::string& s = p.s; int& i = p.i; bool& b = p.b; stmts; }
//    A.func(PARAMS( i = 222; s = "strstrstr"; ));

//kludge: specialize param struct to allow selective setting of named members without using parent struct name:
//use placement new to avoid copy ctor
//see https://stackoverflow.com/questions/12122234/how-to-initialize-a-struct-using-the-c-style-while-using-the-g-compiler/14206226#14206226
//no (returns adrs of temp); use lambda param instead: #define PARAMS(stmts)  []() -> api::FuncParams* { struct my_param_list: api::FuncParams { my_param_list() { stmts; }}; return &my_param_list(); }
//NOTE: need c++14 for auto lambda params
//NOTE: needs g++-6 for inheritance from lambda param + std::decay_t
//#define PLIST(stmts)  [](auto& p){ struct my_params: /*public api::FuncParams*/ decltype(p) { my_params() { /*stmts*/; std::cout << typeid(p).name() << "\n" << std::flush; }}; new (&p) struct my_params(); }
//#define PLIST(stmts)  [](auto& p){ typedef /*decltype(p)*/ struct api::FuncParams plist; /*std::cout << typeid(p).name() << "\n" << std::flush*/; struct my_params: plist { my_params() { stmts; }}; } //new (&p) struct my_params(); }
//#define TYPEOF(plist)  API::FuncParams //std::decay_t<decltype(plist)>
//#define TYPEOF(plist)  base_type //std::remove_reference<decltype(plist)> //decltype(plist) //std::decay_t<decltype(plist)> //API::FuncParams
//#define TYPEOF(plist)  std::decay<decltype(plist)>::type //API::FuncParams
//#define PLIST1(stmts)  [](auto& params){ struct my_params: API::FuncParams { my_params() { stmts; }}; new (&params) struct my_params(); }
//#define PLIST2(stmts)  [](auto& params){ struct my_params: API { my_params() { stmts; }}; new (&params) struct my_params(); }
//#define PLIST(stmts)  [](auto& params) { class my_params: public std::decay<decltype(params)>::type { public: my_params() { this->srcline = SRCLINE; stmts; } }; /*debug()*/; new (&params) my_params(); } //my_params x; std::cout << sizeof(params) << ", " << sizeof(my_params) << "\n" << std::flush; new (&params) struct my_params(); }
//NO WORKY FOR DEPENDENT CLASS MEMBERS: #define PLIST(stmts)  [](auto& params) { struct my_params: std::decay<decltype(params)>::type { my_params() { this->srcline = SRCLINE; stmts; } }; /*debug()*/; new (&params) my_params(); } //my_params x; std::cout << sizeof(params) << ", " << sizeof(my_params) << "\n" << std::flush; new (&params) struct my_params(); }
//#define PLIST(stmts)  [](auto& params){ struct wrap { template<typename TYPE> using base_type = typename std::remove_reference<TYPE>::type; struct my_params: public TYPEOF(params) { my_params() { /*srcline = SRCLINE; stmts*/; }}}; debug(); new (&params) struct my_params(); } //my_params x; std::cout << sizeof(params) << ", " << sizeof(my_params) << "\n" << std::flush; new (&params) struct my_params(); }
//#define debug()  { std::cout << "plist size: " << sizeof(params) << ", " << sizeof(my_params) << "\n" << std::flush; }
//    A.func(PLIST( this->i = 222; this->s = "strstrstr"; ));
//#define func(stmts)  [](int& i, std::string& s, bool& b, other*& o, SrcLine& srcline) stmts
//    A.func([] API_FUNC { i = 222; s = "strstrstr"; });
//    A.func([] API_FUNC { i = 222; s = "strstrstr"; });
    std::string my_str3 = "strstrstr";
    A.func(NAMED{ _.i = 222; _.s = my_str3; SRCLINE; });
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
//    A.func([] API_FUNC { i = 333; });
    A.func(NAMED{ _.i = 333; /*SRCLINE*/; });
    }
    std::cout << "----\n" << std::flush;

    { //scope
//API X(stmt)  =>  API X(ctor(stmt));
//    API B([] API_CTOR { i = 2; b = true; s = "str"; /*o = new other(4)*/; });
    std::string my_str = "str";
    API B(NAMED{ _.i = 2; _.b = true; _.s = my_str; /*_.o = new other(4)*/; SRCLINE; });
    B.func(NAMED{ SRCLINE; });
//    B.func([] API_FUNC { i = 55; });
    B.func(NAMED{ _.i = 55; SRCLINE; });
//    B.func(SRCLINE, &(auto x = [&](auto& _){ _.i = 55; }));
//#define NAMED  SRCLINE, auto cb = [&](auto& _), &cb
    }

    MSG(PINK_MSG << "API2 test" << ENDCOLOR);
    { //scope
    API2 x;
    x.func();
    x.func(56);
    MSG(BLUE_MSG << "x: " << x << ENDCOLOR);
    }
    std::cout << "----\n" << std::flush;

    { //scope
    API2 y(true, 33, "here", SRCLINE);
    y.func(10);
    MSG(BLUE_MSG << "y: " << y << ENDCOLOR);
    }
    std::cout << "----\n" << std::flush;

    { //scope
    API2 z(NAMED{ _.i = 44; _.b = true; SRCLINE; });
    z.func(100);
    MSG(BLUE_MSG << "z: " << z << ENDCOLOR);
    }

    return 0;
}
//eof