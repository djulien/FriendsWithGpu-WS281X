//std::vector extensions

#ifndef _VECTOREX_H
#define _VECTOREX_H

#include <vector>
#include <memory> //std::allocator<>
#include <string> //std::string
#include <sstream> //std::stringstream
#include <string.h> //strlen
#include <thread> //std::this_thread
#include <type_traits> //std::enable_if<>, std::is_same<>

//extended vector:
//adds find() and join() methods
template<typename TYPE, class ALLOC = std::allocator<TYPE>>
class vector_ex: public std::vector<TYPE, ALLOC>
{
public: //ctors
//    static std::allocator<TYPE> def_alloc; //default allocator; TODO: is this needed?
//    /*explicit*/ vector_ex() {}
//    explicit vector_ex(std::size_t count, const std::allocator<TYPE>& alloc = def_alloc): std::vector<TYPE>(count, alloc) {}
//    explicit vector_ex(std::size_t count = 0, const ALLOC& alloc = ALLOC()): std::vector<TYPE>(count, alloc) {}
    template<typename ... ARGS>
    explicit vector_ex(ARGS&& ... args): std::vector<TYPE, ALLOC>(std::forward<ARGS>(args) ...) {} //perfect fwd
public: //extensions
    int find(const TYPE&& that)
    {
//        for (auto& val: *this) if (val == that) return &that - this;
        for (auto it = this->begin(); it != this->end(); ++it)
            if (*it == that) return it - this->begin();
        return -1;
    }
    std::string join(const char* sep = ",", const char* if_empty = "")
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
    int push_and_find(const TYPE&& that)
    {
        int retval = this->size();
        this->push_back(that);
        return retval;
    }
};
//template<class TYPE>
//std::allocator<TYPE> vector_ex<TYPE>::def_alloc; //default allocator; TODO: is this needed?


//minimal stl allocator (C++11)
//based on example at: https://msdn.microsoft.com/en-us/library/aa985953.aspx
template <typename TYPE>
struct FixedAlloc
{  
    TYPE* m_list;
    size_t m_limit;
    typedef TYPE value_type;
    FixedAlloc() noexcept {} //default ctor; not required by STL
    template<class OTHER>
    FixedAlloc(const FixedAlloc<OTHER>& other) noexcept {} //converting copy ctor (more efficient than C++03)
    template<class OTHER>
    FixedAlloc(TYPE* list, size_t limit = 0) noexcept: m_list(list), m_limit(limit) {} //converting copy ctor (more efficient than C++03)
    template<class OTHER>
    bool operator==(const FixedAlloc<OTHER>& other) const noexcept { return this == other; } //true; }
    template<class OTHER>
    bool operator!=(const FixedAlloc<OTHER>& other) const noexcept { return this != other; } //false; }
    TYPE* allocate(const size_t count) const
    {
        if (!count) return nullptr;
        if (count > /*static_cast<size_t>(-1) / sizeof(TYPE)*/ m_limit) throw std::bad_array_new_length();
//        void* const ptr = m_heap? m_heap->alloc(count * sizeof(TYPE), key, srcline): shmalloc(count * sizeof(TYPE), key, srcline);
//            void* const ptr = m_heap? memalloc<false>(count * sizeof(TYPE), key, srcline): memalloc<true>(count * sizeof(TYPE), key, srcline);
//            DEBUG_MSG(YELLOW_MSG << timestamp() << "FixedAlloc: allocated " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << FMT(" bytes for key 0x%lx") << key << " from " << (m_heap? "custom": "heap") << " at " << FMT("%p") << ptr << ENDCOLOR);
//            if (!ptr) throw std::bad_alloc();
//            return static_cast<TYPE*>(ptr);
        return m_list;
    }  
    void deallocate(TYPE* const ptr, size_t count = 1) const noexcept {} //noop
};


//#include "ostrfmt.h"
//#include <iostream> //std::cout, std::flush
//#define DEBUG(msg)  { std::cout << msg << "\n" << std::flush; }

//vector that uses preallocated memory:
//list memory immediately follows vector unless caller gives another location
//NOTE about alignment: https://stackoverflow.com/questions/15593637/cache-aligned-stack-variables?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
template<typename TYPE, bool CTOR_DTOR = true, typename ALLOC = FixedAlloc<TYPE>> //, typename = void> //, int SIZE>
class PreallocVector: public vector_ex<TYPE, ALLOC> //vector_ex<TYPE>::FixedAlloc>
{
    typedef vector_ex<TYPE, ALLOC> base_t; //vector_ex<TYPE>::FixedAlloc> base_t; //PreallocVector::FixedAlloc<TYPE>> base_t;
//preallocated list:
    TYPE* m_list;
    struct { size_t count; TYPE list[0]; } m_data; //NOTE: need struct for alignment/packing; NOTE: must come last (preallocated data follows); //std::max(SIZE, 1)];
public: //ctors
    template<typename ... ARGS>
    explicit PreallocVector(size_t count = 0, ARGS&& ... args): PreallocVector(&m_data.list[0], count, std::forward<ARGS>(args) ...) {}
    template<typename ... ARGS>
    explicit PreallocVector(TYPE* list, size_t count = 0, ARGS&& ... args): base_t(count, std::forward<ARGS>(args) ..., ALLOC(list, count)), m_data({0}), m_list(list)
    {
//        DEBUG(FMT("&list[0]  = %p") << &m_list[0] << ", sizeof list = " << sizeof(m_list));
        if (!CTOR_DTOR) return; //caller will handle it
        for (size_t inx = 0; inx < m_data.count; ++inx)
            new (m_data.list + inx) TYPE(std::forward<ARGS>(args) ...); //placement new; perfect fwding
    }
    ~PreallocVector()
    {
        if (!CTOR_DTOR) return; //caller will handle it
        for (size_t inx = 0; inx < m_data.count; ++inx)
            m_list[inx].~TYPE(); //call dtor
    }
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
#define DEBUG(msg)  { std::cout << msg << "\n" << std::flush; }
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

template<typename TYPE>
void test1(TYPE& vec)
{
    DEBUG("initial: " << vec.join(", ", "(empty)") << ", sizeof: " << sizeof(vec));
    size_t num = vec.size();
    vec.push_back(3);
    vec.push_back(1);
    vec.push_back(4);
    vec.reserve(8);
    int ofs = vec.push_and_find(-2);
    DEBUG("1 at: " << vec.find(1) << ", 5 at: " << vec.find(5) << ", new one[" << ofs << "]: " << vec[ofs]);
    DEBUG("all " << num + 3 << ": " << vec.join(", "));
}
    
//int main(int argc, const char* argv[])
void unit_test()
{
//    typedef int vectype;
    typedef IntObj vectype;
    vector_ex<vectype> vec1;
    test1(vec1);

//    struct thing2 { PreallocVector<int, false> vec2; int values2[10]; thing2(): vec2(3, 1), values2({11, 22, 33, 44, 55, 66, 77, 88, 99, 1010}) {}; } thing2;
    vectype values2[10] = {11, 22, 33, 44, 55, 66, 77, 88, 99, 1010};
    PreallocVector<vectype, false> vec2(values2, 3, 1);
//    DEBUG(FMT("&list2[0] = %p") << &values2[0]);
    test1(vec2);

//    struct thing3 { PreallocVector<int, true> vec3; int values3[10]; thing3(): vec3(3, 1), values3({11, 22, 33, 44, 55, 66, 77, 88, 99, 1010}) {}; } thing3;
    vectype values3[10] = {11, 22, 33, 44, 55, 66, 77, 88, 99, 1010};
    PreallocVector<vectype, true> vec3(values3, 3, 1);
//    DEBUG(FMT("&list3[0] = %p") << &values3[0]);
    test1(vec3);

//    return 0;
}
#endif //def WANT_UNIT_TEST
//eof