#ifndef _TOPONLY_H
#define _TOPONLY_H

//class Nesting
//{
//    static int count = 0;
//public: //ctor/dtor
//    Nesting() { ++count; }
//    ~Nesting() { --count; }
//public: //methods
//    bool isNested() { return (count > 0); }
//};


#include "console.h" //MSG()

//"perfect forwarding" (typesafe args) to ctor:
//proxy/perfect forwarding info:
// http://www.stroustrup.com/wrapper.pdf
// https://stackoverflow.com/questions/24915818/c-forward-method-calls-to-embed-object-without-inheritance
// https://stackoverflow.com/questions/13800449/c11-how-to-proxy-class-function-having-only-its-name-and-parent-class/13885092#13885092
// http://cpptruths.blogspot.com/2012/06/perfect-forwarding-of-parameter-groups.html
// http://en.cppreference.com/w/cpp/utility/functional/bind
#ifndef CTOR_PERFECT_FWD
 #define CTOR_PERFECT_FWD(class, base)  \
    template<typename ... ARGS> \
    explicit class(ARGS&& ... args): base(std::forward<ARGS>(args) ...)
#endif //def CTOR_PERFECT_FWD


///////////////////////////////////////////////////////////////////////////////
////
/// Avoid recursion on nested objects:
//

class TopOnly
{
//    static int count; //= 0;
    bool m_top;
    int& depth(int limit = 0)
    {
        static int count = 0;
        if (limit && (count > limit)) throw std::runtime_error("inf loop?");
        if (count < 0) throw std::runtime_error("nesting underflow");
        return count;
    }
//    void paranoid(int limit = 10) { if (nested() > limit) throw std::runtime_error("inf loop?"); }
public: //ctor/dtor
    bool istop() const { return m_top; }
    TopOnly(int limit = 0) { m_top = !depth(limit)++; MSG("TopOnly ctor", istop()); } //!nested(limit)++); } //if (limit) paranoid(limit); }
    ~TopOnly() { MSG("TopOnly dtor", istop()); --depth(); }
};
//int Thing::count = 0;


#endif //ndef _TOPONLY_H