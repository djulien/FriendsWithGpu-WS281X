#!/bin/bash -x
echo -e '\e[1;36m'; OPT=0; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
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


//from https://stackoverflow.com/questions/8709340/can-the-type-of-a-base-class-be-obtained-from-a-template-type-automatically
template<class BASE, class DERIVED>
BASE base_of(DERIVED BASE::*);


//conditional inheritance base:
template <int, typename = void>
class Data_base
{
public: //member vars
//    typedef decltype(*this) my_type;
    typedef Data_base my_type;
//    void base() {}
//    typedef Data_base base_type; //idea from https://stackoverflow.com/questions/16262338/get-base-class-for-a-type-in-class-hierarchy?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
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
protected: //ctor/dtor
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


//specialization:
template <int MAX_THREADs>
//class Data_base<MAX_THREADs, std::enable_if_t<MAX_THREADs != 0>>: public Data_base<MAX_THREADs>
class Data_base: public std::enable_if_t<MAX_THREADs != 0, Data_base<MAX_THREADs, int>>::type
//class Data_more: public Data_base
//struct MsgQue_multi
{
public: //member vars
//    typedef Data_more my_type;
//    typedef Data_more base_type; //idea from https://stackoverflow.com/questions/16262338/get-base-class-for-a-type-in-class-hierarchy?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    char svar[20];
public: //ctor/dtor
    explicit Data_more(): Data_more(NAMED{ SRCLINE; }) {}
    template <typename CALLBACK>
    explicit Data_more(CALLBACK&& named_params): Data_more(unpack(named_params)) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Data_more& myobj) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    {
        ostrm << "{Data_more: svar " << myobj.svar << ", base " << *(Data_base*)&myobj << " }";
        return ostrm;
    }
protected: //ctor/dtor
    struct CtorParams: Data_base/*<MAX_THREADs>*/::CtorParams
    {
        const char* str = 0;
    };
    explicit Data_more(CtorParams& params): Data_base/*<MAX_THREADs>*/((Data_base::CtorParams)params) /*, svar(params.str)*/ { strncpy(svar, params.str? params.str: "", sizeof(svar)); }
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


template <int MAX_THREADs = 0>
//class MyClass //: public MsgQue_data<MAX_THREADs> //std::conditional<THREADs != 0, MsgQue_multi, MsgQue_base>::type
class MyClass: std::conditional<MAX_THREADs != 0, Data_more, Data_base>::type
{
    bool bvar;
//    typedef typename std::conditional<IPC, pid_t, std::thread*>::type PIDTYPE;
//    MemPtr<MsgQue_data<MAX_THREADs>> m_ptr;
//    Data_base/*<MAX_THREADs>*/ data;
//    typename std::conditional<MAX_THREADs != 0, Data_more, Data_base>::type data;
//public:
#if 0
    struct CtorParams: public Data_base<MAX_THREADs>
    {
        int var1 = 0;
        SrcLine srcline = 0;
    };
#endif
public: //ctor/dtor
//    MyClass() = delete; //don't generate default ctor
    explicit MyClass(): MyClass(NAMED{ SRCLINE; }) { MSG("ctor1"); }
    template <typename CALLBACK>
    explicit MyClass(CALLBACK&& named_params): decltype(my_type)(named_params) { MSG("ctor2"); }
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
    MyClass<0> cx(); //what does this do?

    MyClass<0> c0;
    MSG("c0 " << c0);

    MyClass<1> c1(NAMED{ _.var1 = 11; });
    MSG("c1 " << c1);

    MyClass<-1> c2(NAMED{ _.var1 = 22; SRCLINE; });
    MSG("c2 " << c2);

    return 0;
}
//eof