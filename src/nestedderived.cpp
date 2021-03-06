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


char* strzcpy(char* dest, const char* src, size_t maxlen)
{
    if (maxlen)
    {
        size_t cpylen = std::min(strlen(src), maxlen - 1);
        memcpy(dest, src, cpylen);
        dest[cpylen] = '\0';
    }
    return dest;
}


#if 0 //mostly DRY; BROKEN
//from https://stackoverflow.com/questions/8709340/can-the-type-of-a-base-class-be-obtained-from-a-template-type-automatically
//template<class BASE, class DERIVED>
//BASE base_of(DERIVED BASE::*);
//template <typename>
struct empty_base
{
public: //ctor/dtor
    struct CtorParams {};
//    explicit empty_base() {}
//    empty_base() = default;
//    template <class... Args>
//    constexpr empty_base(Args&&...) {}
//    template <typename ... ARGS> //perfect forward
//    explicit empty_base(ARGS&& ... args) {} //: base(std::forward<ARGS>(args) ...)
    explicit empty_base(const CtorParams& params) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const empty_base& mej) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << ", empty";
        return ostrm;
    }
};

//template <bool expr, typename TYPE>
//using optional_base = std::conditional_t<expr, TYPE, empty_base<TYPE>>;

template <bool = false>
class Base0: public empty_base {};
//specialize for conditional inheritance:
template <>
class Base0<true>: public empty_base
{
    using SUPER = empty_base;
public:
    int ivar;
    SrcLine srcline;
public: //ctor/dtor
    struct CtorParams: public SUPER::CtorParams { int ivar = -1; SrcLine srcline = "here:0"; };
    explicit Base0(const CtorParams& params): SUPER(params), ivar(params.ivar), srcline(params.srcline) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Base0& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", ivar " << me.ivar << ", srcline " << me.srcline << (SUPER)me;
        return ostrm;
    }
};

template <bool = false, bool UP = false>
class Base1: public Base0<UP> {};
//specialize for conditional inheritance:
template <>
class Base1<true, bool UP>: Base0<UP>
{
    using SUPER = Base0<UP>;
public:
    char svar[20];
public: //ctor/dtor
    struct CtorParams: public SUPER::CtorParams { const char* svar = "hello"; };
    explicit Base1(const CtorParams& params): SUPER(params) { strzcpy(svar, params.svar? params.svar: "", sizeof(svar)); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Base1& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", svar " << me.svar << (SUPER)me; //<< static_cast<base_t>(me) << "}";
        return ostrm;
    }
};

template <bool = false, bool UP1 = false, bool UP2 = false>
class Base2: public Base1<UP1, UP2> {};
//specialize for conditional inheritance:
template <>
class Base2<true, bool UP1, bool UP2>: public Base1<UP1, UP2>
{
    using SUPER = Base1<UP1, UP2>;
public:
    bool bvar;
public: //ctor/dtor
    struct CtorParams: public SUPER::CtorParams { bool bvar = true; };
    explicit Base2(const CtorParams& params): SUPER(params), bvar(params.bvar) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Base2& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", bvar " << me.bvar << (SUPER)me; //<< static_cast<base_t>(me) << "}";
        return ostrm;
    }
};

template <int MAX_THREADs = 0>
//class MyClass: std::conditional<MAX_THREADs < 0, Base2, std::conditional<MAX_THREADs != 0, Base1, Base0>::type>::type
class MyClass: Base2<MAX_THREADs < 0, MAX_THREADs != 0>
{
//    using SUPER = std::conditional<MAX_THREADs < 0, Base2, std::conditional<MAX_THREADs != 0, Base1, Base0>::type>::type;
    using SUPER = Base2<MAX_THREADs < 0, MAX_THREADs != 0>;
public: //ctor/dtor
//    MyClass() = delete; //don't generate default ctor
//    struct CtorParams: public B0::CtorParams, public B1::CtorParams, public B2::CtorParams {};
//    MyClass() = default;
    explicit MyClass(): MyClass(NAMED{ /*SRCLINE*/; }) { MSG("ctor1"); }
    template <typename CALLBACK> //accept any arg type (only want caller's lambda function)
    explicit MyClass(CALLBACK&& named_params): MyClass(unpack(named_params), Unpacked{}) { MSG("ctor2"); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "MyClass{" << eatch(2) << (SUPER)me << "}";
        return ostrm;
    }
private: //helpers
    struct Unpacked {}; //ctor disambiguation tag
    explicit MyClass(const CtorParams& params, Unpacked): SUPER(params)
    {
        MSG("ctor unpacked: " << *this);
    }
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
#endif

#if 1 //slightly WET at final level
//from https://stackoverflow.com/questions/8709340/can-the-type-of-a-base-class-be-obtained-from-a-template-type-automatically
//template<class BASE, class DERIVED>
//BASE base_of(DERIVED BASE::*);
template <typename> //kludge: allow multiple definitions
struct empty_base
{
public: //ctor/dtor
    struct CtorParams {};
//    explicit empty_base() {}
//    empty_base() = default;
//    template <class... Args>
//    constexpr empty_base(Args&&...) {}
//    template <typename ... ARGS> //perfect forward
//    explicit empty_base(ARGS&& ... args) {} //: base(std::forward<ARGS>(args) ...)
    explicit empty_base(const CtorParams& params) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const empty_base& mej) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << ", empty";
        return ostrm;
    }
};

template <bool expr, typename TYPE>
using inherit_if = std::conditional_t<expr, TYPE, empty_base<TYPE>>;

class Base0
{
public:
    int ivar;
    SrcLine srcline;
public: //ctor/dtor
    struct CtorParams { int ivar = -1; SrcLine srcline = "here:0"; };
    explicit Base0(const CtorParams& params): ivar(params.ivar), srcline(params.srcline) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Base0& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", ivar " << me.ivar << ", srcline " << me.srcline; //<< static_cast<base_t>(me) << "}";
        return ostrm;
    }
};
class Base1
{
public:
    char svar[20];
public: //ctor/dtor
    struct CtorParams { const char* svar = "hello"; };
    explicit Base1(const CtorParams& params) { strzcpy(svar, params.svar? params.svar: "", sizeof(svar)); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Base1& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", svar " << me.svar; //<< static_cast<base_t>(me) << "}";
        return ostrm;
    }
};
class Base2
{
    bool bvar;
public: //ctor/dtor
    struct CtorParams { bool bvar = true; };
    explicit Base2(const CtorParams& params): bvar(params.bvar) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const Base2& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", bvar " << me.bvar; //<< static_cast<base_t>(me) << "}";
        return ostrm;
    }
};

template <int MAX_THREADs = 0>
class MyClass:
    public Base0, //unconditional base
    public inherit_if<MAX_THREADs != 0, Base1>,
    public inherit_if<MAX_THREADs < 0, Base2>
{
    using B0 = Base0;
    using B1 = inherit_if<MAX_THREADs != 0, Base1>;
    using B2 = inherit_if<MAX_THREADs < 0, Base2>;
public: //ctor/dtor
//    MyClass() = delete; //don't generate default ctor
    struct CtorParams: public B0::CtorParams, public B1::CtorParams, public B2::CtorParams {};
//    MyClass() = default;
    explicit MyClass(): MyClass(NAMED{ /*SRCLINE*/; }) { MSG("ctor1"); }
    template <typename CALLBACK> //accept any arg type (only want caller's lambda function)
    explicit MyClass(CALLBACK&& named_params): MyClass(unpack(named_params), Unpacked{}) { MSG("ctor2"); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MyClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "MyClass{" << eatch(2) << static_cast<B0>(me) << static_cast<B1>(me) << static_cast<B2>(me) << "}";
        return ostrm;
    }
private: //helpers
    struct Unpacked {}; //ctor disambiguation tag
    explicit MyClass(const CtorParams& params, Unpacked): B0(static_cast<typename B0::CtorParams>(params)), B1(static_cast<typename B1::CtorParams>(params)), B2(static_cast<typename B2::CtorParams>(params))
//    explicit MyClass(const CtorParams& params, Unpacked): B0((B0::CtorParams)params), B1((B1::CtorParams)params), B2((B2::CtorParams)params)
    {
        MSG("ctor unpacked: " << *this);
    }
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
#endif


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
class MyClass: public MyClass<MAX_THREADs, std::enable_if<MAX_THREADs == 0, BaseType>>::type {};
template <int MAX_THREADs>
class MyClass: public MyClass<MAX_THREADs, std::enable_if<MAX_THREADs < 0, MyType>>::type {};
template <int MAX_THREADs>
class MyClass: public MyClass<MAX_THREADs, std::enable_if<MAX_THREADs > 0, MultiType>>::type {};
#endif


//partial specialization selector:
//see https://stackoverflow.com/questions/11019232/how-can-i-specialize-a-c-template-for-a-range-of-integer-values/11019369
//#define SELECTOR(VAL)  (((VAL) < 0)? -1: ((VAL) > 0)? 1: 0)
//template <int VALUE, int WHICH = SELECTOR(VALUE)>
#define IsMultiThreaded(nthreads)  ((nthreads) != 0)
#define IsMultiProc(nthreads)  ((nthreads) < 0)
template <int VALUE = 0, bool ISMT = IsMultiThreaded(VALUE), bool ISIPC = IsMultiProc(VALUE)>
class OtherClass
{
public:
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const OtherClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "OtherClass<" << VALUE << ">"; //", " << ISMT << ", " << ISIPC << ">";
        return ostrm;
    }
};

//specializations:
template <int VAL>
class OtherClass<VAL, true, false>: public OtherClass<VAL, false, false> //SELECTOR(1)>
{
public:
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const OtherClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << "OtherClass<" << VAL << ", true, false>";
        ostrm << (OtherClass<VAL, false, false>)me << ", true, false";
        return ostrm;
    }
};
template <int VAL>
class OtherClass<VAL, true, true>: public OtherClass<VAL, false, true>  //SELECTOR(-1)>
{
public:
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const OtherClass& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << "OtherClass<" << VAL << ", true, true>";
        ostrm << (OtherClass<VAL, false, true>)me << ", true, true";
        return ostrm;
    }
};


// primary template handles false
template<unsigned SIZE, bool IsBig = (SIZE > 256)>
class circular_buffer {
   unsigned char buffer[SIZE];
   unsigned int head; // index
   unsigned int tail; // index
public:
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const circular_buffer& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "cb<" << SIZE << ">";
        return ostrm;
    }
};

// specialization for true
template<unsigned SIZE>
class circular_buffer<SIZE, true> {
   unsigned char buffer[SIZE];
   unsigned char head; // index
   unsigned char tail; // index
public:
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const circular_buffer& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "CB<" << SIZE << ">";
        return ostrm;
    }
};


int main(int argc, const char* argv[])
{
    MyClass<> cx(); //what does this do?
    MSG("cx " << cx);
    MyClass<0> c0;
    MSG("c0 " << c0);
    MyClass<1> c1(NAMED{ _.ivar = 11; });
    MSG("c1 " << c1);
    MyClass<-1> c2(NAMED{ _.ivar = 22; _.bvar = false; _.svar = "bye"; SRCLINE; });
    MSG("c2 " << c2);

    OtherClass<> xx;
    OtherClass<0> x0;
    OtherClass<1> x1;
    OtherClass<-1> xn1;
    MSG("xx " << xx << ", x0 " << x0 << ", x1" << x1 << ", xn1 " << xn1);

    circular_buffer<8> cb8;
    circular_buffer<1024> cb1k;
    MSG("cb8 " << cb8 << ", cb1k " << cb1k);

    return 0;
}
//eof