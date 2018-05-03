#!/bin/bash -x
echo -e '\e[1;36m'; OPT=3; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
///////////////////////////////////////////////////////////////////////////////
////
/// Optional/named param example
//


#include <iostream> //std::cout, std::flush, std::ostream
#include "srcline.h"

#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ [&](auto& _)
#endif

#ifndef MSG
 #define MSG(msg)  { std::cout << msg << "\n" << std::flush; }
#endif


//conditional inheritance base:
template <int, typename = void>
class Data_base
{
public: //member vars
    int ivar;
    SrcLine srcline;
public: //ctor/dtor
    explicit Data_base(): Data_base(NAMED{ SRCLINE; }) {}
    template <typename CALLBACK>
    explicit Data_base(CALLBACK&& named_params): Data_base(unpack(named_params)) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Data_base& myobj) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "{Data_base: ivar " << myobj.ivar << ", srcline " << myobj.srcline << " }";
        return ostrm;
    }
private: //ctor/dtor
    struct CtorParams
    {
        int var1 = 12345;
        SrcLine srcline = "none:67890";
    };
    explicit Data_base(CtorParams& params): ivar(params.var1), srcline(params.srcline) {}
private: //helpers
    template <typename CALLBACK>
    static struct CtorParams& unpack(CALLBACK&& named_params)
    {
        static struct CtorParams ret_params; //need "static" to preserve address after return
        struct CtorParams params; //reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
        auto thunk = [](auto get_params, struct CtorParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(named_params, params);
        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
        return ret_params;
    }
};


#if 0
//specialization:
template <int MAX_THREADs>
class MsgData_base<MAX_THREADs, std::enable_if_t<MAX_THREADs != 0>>: public MsgData_base<
//struct MsgQue_multi
{
    char svar[20];
    struct CtorParams: public CtorParams
    {
        const char* str = 0;
    };
public:
    explicit MsgData_base(): MsgData_base(NAMED{ SRCLINE; }) {}
    template <typename CALLBACK>
    explicit MsgData_base(CALLBACK&& get_params): MsgData_base(get_params)
    {
        struct CtorParams params;
        auto thunk = [](auto get_params, struct FuncParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(get_params, params);
        ivar = params.var1;
    }
};
#endif


template <int MAX_THREADs = 0>
class MyClass //: public MsgQue_data<MAX_THREADs> //std::conditional<THREADs != 0, MsgQue_multi, MsgQue_base>::type
{
    bool bvar;
//    typedef typename std::conditional<IPC, pid_t, std::thread*>::type PIDTYPE;
//    MemPtr<MsgQue_data<MAX_THREADs>> m_ptr;
    Data_base<MAX_THREADs> data;
//public:
#if 0
    struct CtorParams: public Data_base<MAX_THREADs>
    {
        int var1 = 0;
        SrcLine srcline = 0;
    };
#endif
public: //ctor/dtor
    explicit MyClass(): MyClass(NAMED{ SRCLINE; }) { MSG("ctor1"); }
    template <typename CALLBACK>
    explicit MyClass(CALLBACK&& named_params): data(named_params) { MSG("ctor2"); }
#if 0
    {
        struct CtorParams params;
        auto thunk = [](auto get_params, struct FuncParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(get_params, params);
        ivar = params.var1;
        srcline = params.srcline;
    }
#endif
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& myobj) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "{MyClass: data " << myobj.data << " }";
        return ostrm;
    }
};


int main(int argc, const char* argv[])
{
    MyClass<0> c0();
    MSG("c0 " << c0);

    MyClass<1> c1(NAMED{ _.var1 = 11; });
    MSG("c1 " << c1);

    MyClass<-1> c2(NAMED{ _.var1 = 22; SRCLINE; });
    MSG("c2 " << c2);

    return 0;
}
//eof