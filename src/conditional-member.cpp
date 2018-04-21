#!/bin/bash -x
echo -e '\e[1;36m'; OPT=3; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)
///////////////////////////////////////////////////////////////////////////////
////
/// Conditional class member var example
//

/* SIMPLEST:
https://stackoverflow.com/questions/47273015/class-template-sfinae-via-inheritance?rq=1
template <typename, typename = void>
  struct s1;
template <typename T>
struct s1<T, std::enable_if_t<std::is_integral<T>::value>> { };
//The following class template, s2, is similar; though it conditionally inherits privately from a trivial base class. What are the differences in functionality between s1 and s2?
struct Base { };
template <typename T>
struct s2 : private std::enable_if_t<std::is_integral<T>::value,Base> { };
*/

/* ALSO GOOD:
https://stackoverflow.com/questions/7389707/inheriting-from-an-enable-ifd-base?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
template <class T, class Enable = void> 
class A { ... };
template <class T>
class A<T, typename enable_if<is_integral<T> >::type> { ... };
template <class T>
class A<T, typename enable_if<is_float<T> >::type> { ... };

https://stackoverflow.com/questions/16358804/c-is-conditional-inheritance-possible?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//here you go with your own class
template<bool UseAdvanced>
class Foo : public std::conditional<UseAdvanced, Advanced, Basic>::type
{
      //your code
};
*/


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


template <bool, typename = void> //typename, typename = void>
struct data_members {};

template <bool INCL> //typename TYPE>
struct data_members<INCL, std::enable_if_t<INCL>>
{
    int some_variable[3];
    void hasit() { }
};

template <bool INCL> //typename TYPE>
struct data_members<INCL, std::enable_if_t<!INCL>>
{
    char some_variable[2];
};

//struct Base { };
//template <bool INCL>
//struct s2 : private std::enable_if_t<std::is_integral<T>::value,Base> { };

/*
template<class>
struct empty_base
{
public:
    empty_base() = default;
    template<class... Args>
    constexpr empty_base(Args&&...) {} //perfect fwd
};
*/

template <bool INCLUDE = true>
class TestEither: public data_members<INCLUDE>
{
public:
    TestEither() {}
    ~TestEither() {}
};


class empty {};
//class addon: public data_members<true> {};
template <bool INCLUDE = true>
class TestAddon: public std::conditional<INCLUDE, data_members<true>, empty>::type
{
public:
    TestAddon() {}
    ~TestAddon() {}
};


#include <iostream> //std::cout, std::flush
//#include "msgcolors.h" //SrcLine, msg colors
#define MSG(msg)  { std::cout << msg << "\n" << std::flush; }


int main(int argc, const char* argv[])
{
    TestEither<true> te;
    TestEither<false> fe;
    TestAddon<true> ta;
    TestAddon<false> fa;

    te.hasit();
    ta.hasit();
//error    fe.hasit();
//error    fa.hasit();
    MSG("size with: " << sizeof(TestEither<true>) << ", " << sizeof(TestAddon<true>) << ", " << sizeof(te) << ", " << sizeof(ta) << ", "
        << "size without " << sizeof(TestEither<false>) << ", " << sizeof(TestAddon<false>) << ", " << sizeof(fe) << ", " << sizeof(fa));

    return 0;
}
//eof