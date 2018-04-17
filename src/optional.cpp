#!/bin/bash -x
echo -e '\e[1;36m'; OPT=3; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
///////////////////////////////////////////////////////////////////////////////
////
/// Optional class member var example
//

//based on https://stackoverflow.com/questions/25492589/can-i-use-sfinae-to-selectively-define-a-member-variable-in-a-template-class?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
/*
template<typename T, typename Enable = void>
class base_class;

// my favourite type :D
template<typename T>
class base_class<T, std::enable_if_t<std::is_same<T, myFavouriteType>::value>>{
    public:
        int some_variable;
};

// not my favourite type :(
template<typename T>
class base_class<T, std::enable_if_t<!std::is_same<T, myFavouriteType>::value>>{
    public:
        // no variable
};

template<typename T>
class derived_class: public base_class<T>{
    public:
        // do stuff
};
*/

//no-based on https://codereview.stackexchange.com/questions/101541/optional-base-class-template-to-get-conditional-data-members?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
/*
namespace xstd {
namespace block_adl {

template<class>
struct empty_base
{
        empty_base() = default;

        template<class... Args>
        constexpr empty_base(Args&&...) {}
};

template<bool Condition, class T>
using optional_base = std::conditional_t<Condition, T, empty_base<T>>;

}       // namespace block_adl

// deriving from empty_base or optional_base will not make xstd an associated namespace
// this prevents ADL from finding overloads for e.g. begin/end/swap from namespace xstd
using block_adl::empty_base;
using block_adl::optional_base;

}       // namespace xstd

template<class T>
class Test
:
    public Base0, // unconditional base class
    public xstd::optional_base<Trait1_v<T>, Base1>,
    public xstd::optional_base<Trait2_v<T>, Base2>
{
    using B1 = xstd::optional_base<Trait1_v<T>, Base1>;
    using B2 = xstd::optional_base<Trait2_v<T>, Base2>;
public:
    Test() = default;

    Test(Arg0 const& a0, Arg1 const& a1, Arg2 const& a2)
    :
        Base0(a0),
        B1(a1),
        B2(a2)
    {}    
};
*/

#include <type_traits> //std::conditional<>, std::enable_if<>

//template<bool INCL> //= void>
//class base_class
//{
//};
template<typename Enable = void>
class base_class;

//std::enable_if<!SHARED_inner, void*>::type s
//    typename std::enable_if<SHARED_inner, void>::type shmfree(void* addr, bool debug_msg, SrcLine srcline = 0)
template<bool INCL>
class base_class<std::enable_if_t<INCL>>
{
public:
    int some_variable;
};

template<bool INCL>
class base_class<std::enable_if_t<!INCL>>
{
public:
    // no variable
};

template<bool INCL = true>
class Test: public base_class<INCL>
{
    int x;
public:
    // do stuff
};


/*
template<class>
struct empty_base
{
public:
    empty_base() = default;
    template<class... Args>
    constexpr empty_base(Args&&...) {} //perfect fwd
};

template <bool INCLUDE = true>
class Test: private optional_base<INCLUDE>
{
public:
    Test() {}
    ~Test() {}
};
*/



#include <iostream> //std::cout, std::flush
//#include "msgcolors.h" //SrcLine, msg colors
//#define MSG(msg)  { std::cout << msg << std::flush; }


int main(int argc, const char* argv[])
{
    Test<true> t;
    Test<false> f;

    std::cout << "size with " << sizeof(Test<true>)
        << " = " << sizeof(t) << ", "
        << "size without " << sizeof(Test<false>)
        << " = " << sizeof(f)
        << "\n" << std::flush;

    return 0;
}
//eof