//std::vector extensions

//#define VECTOR_DEBUG


#ifndef _VECTOREX_H
#define _VECTOREX_H

#include <vector>
#include <memory> //std::allocator<>
#include <string> //std::string
#include <sstream> //std::stringstream
#include <string.h> //strlen
#include <thread> //std::this_thread
#include <type_traits> //std::enable_if<>, std::is_same<>
#include <stdarg.h> //v_list, va_start, va_arg, va_end
#include <stdexcept> //std::runtime_error()

#include "srcline.h"
#include "msgcolors.h"
#ifdef VECTOR_DEBUG
 #include "atomic.h"
 #include "elapsed.h" //timestamp()
 #define DEBUG_MSG  ATOMIC_MSG
 #define IF_DEBUG(stmt)  stmt //{ stmt; }
#else
 #define DEBUG_MSG(msg)  {} //noop
 #define IF_DEBUG(stmt)  //noop
#endif
//#ifdef MSGQUE_DETAILS
// #define DETAILS_MSG  DEBUG_MSG
//#else
// #define DETAILS_MSG(stmt)  {} //noop
//#endif

#if 1
#define throwprintf(...)  throw bufprintf(SRCLINE, __VA_ARGS__)
//template<typename ... ARGS>
//const char* bufprintf(ARGS&& ... args, SrcLine srcline = 0)
const char* bufprintf(SrcLine srcline, ...)
{
//    static std::stringstream ss;
    static char buf[250]; //enlarge as needed
//    int needlen = snprintf(buf, sizeof(buf), std::forward<ARGS>(args) ...);
    va_list args;
    va_start(args, srcline);
    const char* fmt = va_arg(args, const char*);
    int needlen = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (needlen > sizeof(buf)) strcpy(buf + sizeof(buf) - 5, " ...");
    DEBUG_MSG(RED_MSG << buf << ENDCOLOR_ATLINE(srcline));
    return buf;
}
#endif


//extended vector:
//adds find() and join() methods
//info about universal refs: https://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers
template<typename TYPE, class ALLOC = std::allocator<TYPE>>
class vector_ex: public std::vector<TYPE, ALLOC>
{
    typedef std::vector<TYPE, ALLOC> base_t;
public: //ctors
//    static std::allocator<TYPE> def_alloc; //default allocator; TODO: is this needed?
//    /*explicit*/ vector_ex() {}
//    explicit vector_ex(std::size_t count, const std::allocator<TYPE>& alloc = def_alloc): std::vector<TYPE>(count, alloc) {}
//    explicit vector_ex(std::size_t count = 0, const ALLOC& alloc = ALLOC()): std::vector<TYPE>(count, alloc) {}
    template<typename ... ARGS>
    explicit vector_ex(ARGS&& ... args): base_t(std::forward<ARGS>(args) ...) {} //perfect fwd
public: //debug
    void reserve(size_t count) { DEBUG_MSG(BLUE_MSG << "reserve " << count << ENDCOLOR); base_t::reserve(count); }
    size_t size() const { DEBUG_MSG(BLUE_MSG << "size " << base_t::size() << ENDCOLOR); return base_t::size(); }
    template <typename TYPE_copy = TYPE> //universal ref (lvalue or rvalue)
    void push_back(/*const*/ TYPE_copy&& that) { DEBUG_MSG(BLUE_MSG << "push back " << that << ENDCOLOR); base_t::push_back(that); }
public: //extensions
//    template <typename TYPE_copy = TYPE> //universal ref
//    int find(/*const*/ TYPE_copy&& that)
    int find(const TYPE& that) const //rvalue ref only
    {
//        for (auto& val: *this) if (val == that) return &that - this;
        for (auto it = this->begin(); it != this->end(); ++it)
            if (*it == that) return it - this->begin();
        return -1;
    }
    std::string join(const char* sep = ",", const char* if_empty = "") const
    {
        std::stringstream buf;
        for (auto it = this->begin(); it != this->end(); ++it)
        {
            buf << sep;
            buf << *it; //some data types don't allow chaining, so do this separately
        }
//        return this->size()? buf.str().substr(strlen(sep)): buf.str();
        if (!this->size()) buf << sep << if_empty; //"(empty)";
        return buf.str().substr(strlen(sep));
    }
//atomic push + find operation (if wrapped with mutex/lock):
    template <typename TYPE_copy = TYPE> //universal ref (lvalue or rvalue)
    int push_and_find(/*const*/ TYPE_copy&& that)
    {
        int retval = /*this->*/size();
        /*this->*/push_back(that);
        if (find(that) != retval) throwprintf("found entry at %d rather than %d", find(that), retval);
        return retval;
    }
    template <typename TYPE_copy = TYPE> //universal ref (lvalue or rvalue)
    int find_or_push(/*const*/ TYPE_copy&& that)
    {
        int retval = /*this->*/find(that);
        if (retval == -1) retval = push_and_find(that);
        return retval;
    }
};
//template<class TYPE>
//std::allocator<TYPE> vector_ex<TYPE>::def_alloc; //default allocator; TODO: is this needed?


#if 1 //example std::allocator
//minimal stl allocator (C++11)
//based on example at: https://msdn.microsoft.com/en-us/library/aa985953.aspx
//NOTE: use a separate sttr for shm key/symbol mgmt
template <class TYPE>
struct ExampleAllocator  
{  
    typedef TYPE value_type;
    ExampleAllocator() noexcept {} //default ctor; not required by STL
    template<class OTHER>
    ExampleAllocator(const ExampleAllocator<OTHER>& other) noexcept {} //converting copy ctor (more efficient than C++03)
    template<class OTHER>
    bool operator==(const ExampleAllocator<OTHER>& other) const noexcept { return true; }
    template<class OTHER>
    bool operator!=(const ExampleAllocator<OTHER>& other) const noexcept { return false; }
    TYPE* allocate(const size_t count) const
    {
        if (!count) return nullptr;
        if (count > static_cast<size_t>(-1) / sizeof(TYPE)) throwprintf("bad array new length: %d exceeds %d", count, static_cast<size_t>(-1) / sizeof(TYPE)); //throw std::bad_array_new_length();
        void* ptr = malloc(count);
        DEBUG_MSG(YELLOW_MSG << timestamp() << "ExampleAllocator: allocated " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << FMT(" bytes at %p") << ptr << ENDCOLOR);
        if (!ptr) throw std::bad_alloc();
        return static_cast<TYPE*>(ptr);
    }  
    void deallocate(TYPE* const ptr, size_t count = 1) const noexcept
    {
        DEBUG_MSG(YELLOW_MSG << timestamp() << "ExampleAllocator: deallocate " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << " bytes at " << FMT("%p") << ptr << ENDCOLOR);
        free(ptr);
    }
private:
    static const char* TYPENAME();
};
#endif


//minimal stl allocator (C++11)
//based on example at: https://msdn.microsoft.com/en-us/library/aa985953.aspx
template <typename TYPE> //, bool CTOR_DTOR = true>
struct FixedAlloc: public ExampleAllocator<TYPE>
{
    typedef ExampleAllocator<TYPE> base_t;
//    typedef TYPE value_type;
//    FixedAlloc() noexcept {} //default ctor; not required by STL
//    template<class OTHER>
//    FixedAlloc(const FixedAlloc<OTHER>& other) noexcept {} //converting copy ctor (more efficient than C++03)
//    template<class OTHER>
    template<typename ... ARGS>
    FixedAlloc(size_t limit = 0, ARGS&& ... args) noexcept: FixedAlloc(m_data.list, limit, std::forward<ARGS>(args) ...) { DEBUG_MSG(BLUE_MSG << "fixed alloc !list " << limit << ENDCOLOR); }
    template<typename ... ARGS>
    FixedAlloc(TYPE* list, size_t limit = 0, ARGS&& ... args) noexcept: base_t(std::forward<ARGS>(args) ...), m_list(list), m_limit(limit), m_alloc(0) { DEBUG_MSG(BLUE_MSG << "fixed alloc list " << limit << ENDCOLOR); }
    template<class OTHER>
    bool operator==(const FixedAlloc<OTHER>& other) const noexcept { return this == &other; } //true; }
    template<class OTHER>
    bool operator!=(const FixedAlloc<OTHER>& other) const noexcept { return this != &other; } //false; }
    TYPE* allocate(const size_t count) //const
    {
        if (!count) return nullptr;
        if (count > /*static_cast<size_t>(-1) / sizeof(TYPE)*/ m_limit) throwprintf("FixedAlloc: bad alloc length %d vs. allowed limit %d", count, m_limit); //throw std::bad_array_new_length();
        if (m_alloc) throwprintf("bad alloc: already allocated"); //std::bad_alloc();
//        void* const ptr = m_heap? m_heap->alloc(count * sizeof(TYPE), key, srcline): shmalloc(count * sizeof(TYPE), key, srcline);
//            void* const ptr = m_heap? memalloc<false>(count * sizeof(TYPE), key, srcline): memalloc<true>(count * sizeof(TYPE), key, srcline);
        DEBUG_MSG(YELLOW_MSG << timestamp() << "FixedAlloc: allocated " << count << " " << TYPENAME() << "(s) at " << FMT("%p") << m_list << ENDCOLOR);
//            if (!ptr) throw std::bad_alloc();
//            return static_cast<TYPE*>(ptr);
//        if (CTOR_DTOR)
//            for (size_t inx = 0; inx < count; ++inx)
//                new (m_list + inx) TYPE(std::forward<ARGS>(args) ...); //placement new; perfect fwding
        m_alloc = count; //only allow 1 alloc
        return m_list;
    }  
    void deallocate(TYPE* const ptr, size_t count = 1) //const noexcept //noop
    {
        if (count != m_alloc) throwprintf("bad partial dealloc: %d of %d", count, m_alloc); //throw std::bad_alloc(); //should be all or nothing
        DEBUG_MSG(YELLOW_MSG << timestamp() << "FixedAlloc: non-deallocate " << count << " " << TYPENAME() << "(s) at " << FMT("%p") << ptr << " (not really)" << ENDCOLOR);
//        if (CTOR_DTOR)
//            for (size_t inx = 0; inx < m_data.count; ++inx)
//                m_list[inx].~TYPE(); //call dtor
        m_alloc = 0;
    }
private:
    TYPE* m_list;
    size_t m_limit, m_alloc;
    struct { /*size_t count*/; TYPE list[0]; } m_data; //NOTE: need struct for alignment/packing; NOTE: must come last (preallocated data follows); //std::max(SIZE, 1)];
    static const char* TYPENAME();
};


//#include "ostrfmt.h"
//#include <iostream> //std::cout, std::flush
//#define DEBUG(msg)  { std::cout << msg << "\n" << std::flush; }

//vector that uses preallocated memory:
//list memory immediately follows vector unless caller gives another location
//NOTE about alignment: https://stackoverflow.com/questions/15593637/cache-aligned-stack-variables?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
template<typename TYPE, /*bool CTOR_DTOR = true,*/ typename ALLOC = FixedAlloc<TYPE>> //, typename = void> //, int SIZE>
class PreallocVector: public vector_ex<TYPE, ALLOC> //vector_ex<TYPE>::FixedAlloc>
{
    typedef vector_ex<TYPE, ALLOC> base_t; //vector_ex<TYPE>::FixedAlloc> base_t; //PreallocVector::FixedAlloc<TYPE>> base_t;
    static const ALLOC& default_alloc() //kludge: dummy value to select default allocator
    {
        static const ALLOC dummy; //nested to avoid dangling declare later
        return dummy;
    }
//preallocated list:
//    TYPE* m_list;
//    struct { size_t count; TYPE list[0]; } m_data; //NOTE: need struct for alignment/packing; NOTE: must come last (preallocated data follows); //std::max(SIZE, 1)];
public: //ctors
    template<typename ... ARGS>
    explicit PreallocVector(size_t count, ARGS&& ... args, const ALLOC& alloc = default_alloc()): base_t(std::forward<ARGS>(args) ..., (alloc != default_alloc())? alloc: ALLOC(count)) { DEBUG_MSG(BLUE_MSG << "prealloc ctor1" << ENDCOLOR); init(count); } //std::forward<ARGS>(args) ...); }
    template<typename ... ARGS>
    explicit PreallocVector(TYPE* list, size_t count, ARGS&& ... args, const ALLOC& alloc = default_alloc()): base_t(std::forward<ARGS>(args) ..., (alloc != default_alloc())? alloc: ALLOC(list, count)) { DEBUG_MSG(BLUE_MSG << "prealloc ctor2" << ENDCOLOR); init(count); } //std::forward<ARGS>(args) ...); }
    ~PreallocVector() { DEBUG_MSG(BLUE_MSG << "dtor" << ENDCOLOR); clup(); }
private:
//    template<typename ... ARGS>
    void init(size_t count) //ARGS&& ... args)
    {
        /*if (count)*/ base_t::reserve(count); //pre-allocate all storage
//        DEBUG(FMT("&list[0]  = %p") << &m_list[0] << ", sizeof list = " << sizeof(m_list));
//        if (!CTOR_DTOR) return; //caller will handle it
        DEBUG_MSG(CYAN_MSG << timestamp() << "PreallocVector: ctor for " << base_t::capacity() << " " << TYPENAME() << "(s)" << ENDCOLOR);
//        for (size_t inx = 0; inx < base_t::capacity(); ++inx)
//            new (&this[inx]) TYPE(std::forward<ARGS>(args) ...); //placement new; perfect fwding
    }
    void clup()
    {
//        if (!CTOR_DTOR) return; //caller will handle it
        DEBUG_MSG(CYAN_MSG << timestamp() << "PreallocVector: dtor for " << base_t::capacity() << " " << TYPENAME() << "(s)" << ENDCOLOR);
//        for (size_t inx = 0; inx < base_t::capacity(); ++inx)
//            base_t[inx].~TYPE(); //call dtor
    }
//    ALLOC& m_alloc;
    static const char* TYPENAME();
};
#if 0
#pragma message("TODO: derive from above")
template<typename TYPE, bool CTOR_DTOR = true, typename = void> //, int SIZE>
class PreallocVector
{
//public: //members
//    TYPE (&m_list)[];
    TYPE* m_list;
    struct { size_t count; TYPE list[0]; } m_data; //NOTE: need struct for alignment/packing; NOTE: must come last (preallocated data follows); //std::max(SIZE, 1)];
//private: //kludge: std::thread::id no worky with operator<<()
//    typedef std::is_same<std::decay<TYPE>, std::thread::id> isthread;
//    template <typename TYPE_copy = TYPE>
//    static TYPE tostr(TYPE_copy&& val) { return val; }
//    static std::enable_if<!std::is_same<std::decay<TYPE_copy>, std::thread::id>::value, int> tostr(TYPE_copy&& val) { return val; }
//thread::id specialization:
//    template <typename TYPE_copy = TYPE>
//    static std::enable_if<std::is_same<std::decay<TYPE_copy>, std::thread::id>::value, int> tostr(TYPE_copy&& val) { return (int)val; }
//    static TYPE tostr(TYPE val) { return val; }
public: //ctors
    template<typename ... ARGS>
    explicit PreallocVector(TYPE* list, size_t count = 0, ARGS&& ... args): m_data({0}), m_list(list)
    {
//        DEBUG(FMT("&list[0]  = %p") << &m_list[0] << ", sizeof list = " << sizeof(m_list));
        if (!CTOR_DTOR) return; //caller will handle it
        for (size_t inx = 0; inx < m_data.count; ++inx)
            new (m_data.list + inx) TYPE(std::forward<ARGS>(args) ...); //placement new; perfect fwding
    }
    template<typename ... ARGS>
    explicit PreallocVector(size_t count = 0, ARGS&& ... args): PreallocVector(&m_data.list[0], count, std::forward<ARGS>(args) ...) {}
    ~PreallocVector()
    {
        if (!CTOR_DTOR) return; //caller will handle it
        for (size_t inx = 0; inx < m_data.count; ++inx)
            m_list[inx].~TYPE(); //call dtor
    }
public: //methods
    size_t size() const { return m_data.count; }
    void reserve(size_t count)
    {
        if (!CTOR_DTOR) { m_data.count = count; return; } //caller will handle it
        while (m_data.count < count) new (m_list + m_data.count++) TYPE(); //placement new
        while (m_data.count > count) m_list[--m_data.count].~TYPE(); //dtor
    }
    void push_back(const TYPE&& that)
    {
//        if (m_count >= SIZEOF(m_list)) throw std::runtime_error("PreallocVector: size overflow");
        new (m_data.list + m_data.count++) TYPE(that); //placement new, copy ctor
    }
    template<typename ... ARGS>
    void emplace_back(ARGS&& ... args)
    {
//        if (m_count >= SIZEOF(m_list)) throw std::runtime_error("PreallocVector: bad index");
        new (m_data.list + m_data.count++) TYPE(std::forward<ARGS>(args) ...); //placement new; perfect fwding
    }
    TYPE& operator[](int inx) //const
    {
        if ((inx < 0) || (inx >= m_data.count)) throw std::runtime_error("PreallocVector: bad index");
        return m_data.list[inx];
    }
    int find(const TYPE&& that) const
    {
        for (int inx = 0; inx < m_data.count; ++inx)
            if (m_data.list[inx] == that) return inx;
        return -1;
    }
    std::string join(const char* sep = ",", const char* if_empty = "") const
    {
        std::stringstream buf;
        for (int inx = 0; inx < m_data.count; ++inx)
        {
            buf << sep;
            buf << /*tostr*/ m_data.list[inx];
        }
        if (!this->size()) buf << sep << if_empty; //"(empty)";
        return buf.str().substr(strlen(sep));
    }
//atomic push + find:
    int push_and_find(const TYPE&& that)
    {
        int retval = this->size();
//        this->push_back(that); //no worky
        new (m_data.list + m_data.count++) TYPE(that); //placement new, copy ctor
        return retval;
    }
};
//thread::id partial specialization:
//kludge: std::thread::id no worky with operator<<()
//template <bool CTOR_DTOR> //= true>
//class PreallocVector<std::thread::id, CTOR_DTOR>: public PreallocVector<std::thread::id, CTOR_DTOR, char>
//{
//    static int tostr(std::thread::id val) { return (int)val; }
//};
#endif

#endif //ndef _VECTOREX_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit tests:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion
#include <iostream> //std::cout, std::flush
//#include "ostrfmt.h" //FMT()
#include "atomic.h"
#include "msgcolors.h"
#define DEBUG(msg)  ATOMIC_MSG(msg) //{ std::cout << msg << "\n" << std::flush; }
#define NO_DEBUG(msg)  {}

class IntObj
{
    int m_value;
public: //ctor/dtor
    /*explicit*/ IntObj(int val = 0): m_value(val) { NO_DEBUG("int ctor " << m_value); }
    /*explicit*/ IntObj(const IntObj& that): m_value(that.m_value) { NO_DEBUG("init cctor " << m_value); }
    ~IntObj() { NO_DEBUG("  int dtor " << m_value); }
public: //operators
    operator int() { return m_value; }
    bool operator==(const IntObj& that) const { return that.m_value == this->m_value; }
//    std::ostream& operator<<(const IntObj& that)
    friend std::ostream& operator<<(std::ostream& ostrm, const IntObj& val)
    {
        ostrm << val.m_value;
        return ostrm;
    }
};

template<>
const char* ExampleAllocator<IntObj>::TYPENAME() { return "ExampleAllocator<IntObj>"; }
template<>
const char* FixedAlloc<IntObj>::TYPENAME() { return "FixedAlloc<IntObj>"; }
template<>
const char* PreallocVector<IntObj, FixedAlloc<IntObj>>::TYPENAME() { return "PreallocVector<IntObj, FixedAlloc<IntObj>>"; }


template<typename TYPE>
void test1(TYPE& vec, bool want_reserve = true)
{
    DEBUG(BLUE_MSG << "initial: " << vec.join(", ", "(empty)") << ", sizeof: " << sizeof(vec) << ENDCOLOR);
    size_t num = vec.size();
    vec.push_back(3);
    vec.push_back(1);
    vec.push_back(4);
    if (want_reserve) vec.reserve(8);
    int ofs = vec.push_and_find(-2);
    DEBUG("1 at: " << vec.find(1) << ", 5 at: " << vec.find(5) << ", new one[" << ofs << "]: " << vec[ofs] << ENDCOLOR);
    DEBUG("all " << num + 4 << ": " << vec.join(", ") << ENDCOLOR);
}


//int main(int argc, const char* argv[])
void unit_test()
{
    DEBUG(PINK_MSG << "vect<IntObj>" << ENDCOLOR);
//    typedef int vectype;
    typedef IntObj vectype;
    vector_ex<vectype> vec1;
    test1(vec1);

    DEBUG(PINK_MSG << "vect<IntObj,ExAlloc>" << ENDCOLOR);
//    struct thing2 { PreallocVector<int, false> vec2; int values2[10]; thing2(): vec2(3, 1), values2({11, 22, 33, 44, 55, 66, 77, 88, 99, 1010}) {}; } thing2;
//    vectype values2[10] = {11, 22, 33, 44, 55, 66, 77, 88, 99, 1010};
//    /*PreallocVector<vectype, false>*/ test_type vec2(values2, 3, 1);
    vector_ex<vectype, ExampleAllocator<IntObj>> vec2;
//    DEBUG(FMT("&list2[0] = %p") << &values2[0]);
    test1(vec2);

    DEBUG(PINK_MSG << "PreallocVect<IntObj>" << ENDCOLOR);
//    typedef vector_ex<vectype, ExampleAllocator<vectype>> test_type;
    typedef PreallocVector<vectype/*, false*/> test_type;
//    struct thing3 { PreallocVector<int, true> vec3; int values3[10]; thing3(): vec3(3, 1), values3({11, 22, 33, 44, 55, 66, 77, 88, 99, 1010}) {}; } thing3;
    vectype values3[10] = {11, 22, 33, 44, 55, 66, 77, 88, 99, 1010};
    /*PreallocVector<vectype, true>*/ test_type vec3(values3, 3+1+1); //, /*std::initializer_list<IntObj>{1, 2}*/ 2);
//    DEBUG(FMT("&list3[0] = %p") << &values3[0]);
    test1(vec3, false);

    DEBUG(PINK_MSG << "PreallocVect<IntObj>" << ENDCOLOR);
    typedef PreallocVector<vectype/*, false*/> test_type;
    struct { test_type vec4/*(3+1+1)*/; vectype storage[5]; } x{test_type(3+1+1)};
    test1(x.vec4, false);

//    return 0;
}
#endif //def WANT_UNIT_TEST
//eof