#!/bin/bash -x
echo -e '\e[1;36m'; OPT=0; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
///////////////////////////////////////////////////////////////////////////////
////
/// Optional/named param example
//

//conditional member: https://stackoverflow.com/questions/16673169/declaring-member-or-not-depending-on-template-parameter?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//tag dispatch: https://stackoverflow.com/questions/6917079/tag-dispatch-versus-static-methods-on-partially-specialised-classes?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//tag dispatch: https://arne-mertz.de/2016/10/tag-dispatch/
//specialization inheritance: https://stackoverflow.com/questions/347096/how-can-i-get-a-specialized-template-to-use-the-unspecialized-version-of-a-membe?rq=1&utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//specialization inheritance: https://stackoverflow.com/questions/41280944/call-the-unspecialized-template-method-from-a-specialized-template-method

#include <iostream> //std::cout, std::flush, std::ostream
#include <type_traits> //std::conditional<>, std::enable_if<>
#include "srcline.h"
#include "eatch.h"

#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ [&](auto& _)
#endif

#ifndef MSG
 #define MSG(msg)  { std::cout << msg << "\n" << std::flush; }
#endif


//from https://stackoverflow.com/questions/8709340/can-the-type-of-a-base-class-be-obtained-from-a-template-type-automatically
//template<class BASE, class DERIVED>
//BASE base_of(DERIVED BASE::*);
template <typename>
struct empty_base
{
    empty_base() = default;
//    template <class... Args>
//    constexpr empty_base(Args&&...) {}
    template <typename ... ARGS> //perfect forward
    explicit empty_base(ARGS&& ... args) {} //: base(std::forward<ARGS>(args) ...)
};

template <bool expr, typename TYPE>
using optional_base = std::conditional_t<expr, TYPE, empty_base<TYPE>>;

class Base0
{
public:
    int ivar;
    SrcLine srcline;
};
class Base1
{
public:
    char svar[20];
};
class Base2
{
    bool bvar;
};

template <int MAX_THREADs = 0>
class MyClasss:
    public Base0, // unconditional base class
    public optional_base<MAX_THREADs != 0, Base1>,
    public optional_base<MAX_THREADs < 0, Base2>
{
    using B0 = Base 0,
    using B1 = optional_base<MAX_THREADs != 0, Base1>;
    using B2 = optional_base<MAX_THREADs < 0, Base2>;
public:
    struct CtorParams: public B0, public B1, public B2 {};
//    MyClasss() = default;
    MyClasss(const CtorParams& params): B0(params), B1(params), B2(params)
    {
        
    }
};


//tags to control optional features:
//idea from https://arne-mertz.de/2016/10/tag-dispatch/
//see also https://stackoverflow.com/questions/6917079/tag-dispatch-versus-static-methods-on-partially-specialised-classes?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//template <bool> 
//struct NeedSerialization {};


//inheritance root:
//partial template specialization: http://en.cppreference.com/w/cpp/language/partial_specialization
//conditional members: https://codereview.stackexchange.com/questions/101541/optional-base-class-template-to-get-conditional-data-members


#if 0
struct TopType {}; //tag
template <int MAX_THREADs = 0, typename SUPER = void>
class MyClass
{
//    typedef MyClass my_t;
protected:
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& mej) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << ", empty";
        return ostrm;
    }
    struct CtorParams {};
    explicit MyClass(const CtorParams& params) {}
};


//conditional inheritance base:
struct BaseType {}; //tag
template <int MAX_THREADs> //int MAX_THREADs> //, typename = void>
class MyClass<MAX_THREADs, BaseType>: public MyClass<>
{
//    typedef MyClass<BaseType> my_t;
    typedef MyClass<> base_t;
//    typedef empty base_t;
protected:
//public: //member vars
//    typedef decltype(*this) my_type;
//    typedef Data_base my_type;
//    void base() {}
//    typedef Data_base base_type; //idea from https://stackoverflow.com/questions/16262338/get-base-class-for-a-type-in-class-hierarchy?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    int ivar;
    SrcLine srcline;
//public: //ctor/dtor
//    explicit Data(): Data_base(NAMED{ SRCLINE; }) {}
//    template <typename CALLBACK>
//    explicit Data(CALLBACK&& named_params): Data(unpack(named_params)) {}
//public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", ivar " << me.ivar << ", srcline " << me.srcline << static_cast<base_t>(me);
        return ostrm;
    }
//protected: //ctor/dtor
//private: //ctor/dtor
//    template <bool>
    struct CtorParams: base_t::CtorParams
    {
        int ivar = 12345;
        SrcLine srcline = "none:67890";
    };
    explicit MyClass(const CtorParams& params): base_t(static_cast<base_t::CtorParams>(params)), ivar(params.ivar), srcline(params.srcline) {}
//    template <>
//    struct CtorParams<true>: CtorParams<false>
//    {
//        const char* str = 0;
//    };
//    explicit Data(CtorParams& params): ivar(params.var1), srcline(params.srcline) {}
//private: //helpers
//    template <typename CALLBACK>
//    static struct CtorParams& unpack(CALLBACK&& named_params)
//    {
//        static struct CtorParams params; //need "static" to preserve address after return
////        struct CtorParams params; //reinit each time; comment out for sticky defaults
//        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
////        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
//        auto thunk = [](auto get_params, struct CtorParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
//        thunk(named_params, params);
////        ret_params = params;
////        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
//        return params;
//    }
};


//specialization:
struct MultiType {}; //tag
template <int MAX_THREADs>
class MyClass<MAX_THREADs, MultiType>: public MyClass<BaseType>
{
//    typedef MyClass<MultiType> my_t;
    typedef MyClass<BaseType> base_t;
protected:
//public: //member vars
    char svar[20];
//public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", svar " << me.svar << static_cast<base_t>(me);
        return ostrm;
    }
//protected: //ctor/dtor
    struct CtorParams: base_t::CtorParams
    {
        const char* svar = 0;
    };
    explicit MyClass(const CtorParams& params): base_t(static_cast<base_t::CtorParams>(params)) { strncpy(svar, params.svar? params.svar: "", sizeof(svar)); }
};
#if 0
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
#endif


struct MyType {}; //tag
//template <int MAX_THREADs = 0>
template <int MAX_THREADs>
//class MyClass //: public MsgQue_data<MAX_THREADs> //std::conditional<THREADs != 0, MsgQue_multi, MsgQue_base>::type
class MyClass<MAX_THREADs, MyType>: public MyClass<std::conditional<MAX_THREADs < 0, MultiType, std::conditional<MAX_THREADs > 0, BaseType, TopType>::type>::type> //Data<MAX_THREADs != 0> //: std::conditional<MAX_THREADs != 0, Data_more, Data_base>::type
{
//    typedef MyClass<MyType> my_t;
    typedef MyClass<MultiType> base_t;
//    typedef MyClass<MAX_THREADs != 0> base_t;
    struct Unpacked {}; //ctor disamguation tag
protected:
//public: //member vars
    bool bvar;
//    typedef typename std::conditional<IPC, pid_t, std::thread*>::type PIDTYPE;
//    MemPtr<MsgQue_data<MAX_THREADs>> m_ptr;
//    Data<MAX_THREADs != 0> data;
//    typename std::conditional<MAX_THREADs != 0, Data_more, Data_base>::type data;
//public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "MyClass{" << eatch(2) << ", bvar " << me.bvar << static_cast<base_t>(me) << "}";
        return ostrm;
    }
//public:
    struct CtorParams: base_t::CtorParams
    {
        bool bvar = true;
    };
    explicit MyClass(const CtorParams& params, Unpacked): base_t(static_cast<typename base_t::CtorParams>(params)), bvar(params.bvar) {}
public: //ctor/dtor
//    MyClass() = delete; //don't generate default ctor
    explicit MyClass(): MyClass(NAMED{ /*SRCLINE*/; }) { MSG("ctor1"); }
    template <typename CALLBACK> //accept any arg type (caller's lambda function)
    explicit MyClass(CALLBACK&& named_params): MyClass(unpack(named_params), Unpacked{}) { MSG("ctor2"); }
#if 0
    {
        struct CtorParams params;
        auto thunk = [](auto get_params, struct FuncParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(get_params, params);
        ivar = params.var1;
        srcline = params.srcline;
    }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& myobj) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "{MyClass: data " << myobj.data << " }";
        return ostrm;
    }
#endif
private: //helpers
    template <typename CALLBACK>
    static struct CtorParams& unpack(CALLBACK&& named_params)
    {
        static struct CtorParams params; //need "static" to preserve address after return
//        struct CtorParams params; //reinit each time; comment out for sticky defaults
        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
        auto thunk = [](auto get_params, struct CtorParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(named_params, params);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
        return params;
    }
};

template <int MAX_THREADs = 0>
class MyClasss: public MyClass<MAX_THREADs, std::enable_if<MAX_THREADs == 0, BaseType>>::type {};
template <int MAX_THREADs>
class MyClasss: public MyClass<MAX_THREADs, std::enable_if<MAX_THREADs < 0, MyType>>::type {};
template <int MAX_THREADs>
class MyClasss: public MyClass<MAX_THREADs, std::enable_if<MAX_THREADs > 0, MultiType>>::type {};
#endif


int main(int argc, const char* argv[])
{
    MyClasss<> cx(); //what does this do?

    MyClasss<0> c0;
    MSG("c0 " << c0);

    MyClasss<1> c1(NAMED{ _.ivar = 11; });
    MSG("c1 " << c1);

    MyClasss<-1> c2(NAMED{ _.ivar = 22; _.bvar = false; _.svar = "hello"; SRCLINE; });
    MSG("c2 " << c2);

    return 0;
}
//eof