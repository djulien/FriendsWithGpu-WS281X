//shared memory allocator
//see https://www.ibm.com/developerworks/aix/tutorials/au-memorymanager/index.html
//see also http://anki3d.org/cpp-allocators-for-games/

//CAUTION: “static initialization order fiasco”
//https://isocpp.org/wiki/faq/ctors

//to look at shm:
// ipcs -m 

// https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
// https://stackoverflow.com/questions/4836863/shared-memory-or-mmap-linux-c-c-ipc

//    you identify your shared memory segment with some kind of symbolic name, something like "/myRegion"
//    with shm_open you open a file descriptor on that region
//    with ftruncate you enlarge the segment to the size you need
//    with mmap you map it into your address space

#ifndef _SHMALLOCATOR_H
#define _SHMALLOCATOR_H

//template<int SIZE>
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <cstdlib>
#include <stdint.h> //uintptr_t
#include <mutex>
#include <condition_variable>
#include <string.h>
//#include <string> //https://stackoverflow.com/questions/6320995/why-i-cannot-cout-a-string
#include <stdexcept>
#include "ostrfmt.h"
#include "elapsed.h"
#include "atomic.h"
#include "shmalloc.h"
#include "colors.h"
//#define ENDCOLOR  ENDCOLOR << std::flush

#define SIZEOF(thing)  (sizeof(thing) / sizeof(thing[0]))
#define divup(num, den)  (((num) + (den) - 1) / (den))
#define rdup(num, den)  (divup(num, den) * (den))
//#define size32_t  uint32_t //don't need huge sizes in shared memory; cut down on wasted bytes


//"perfect forwarding" (typesafe args) to ctor:
//proxy/perfect forwarding info:
// http://www.stroustrup.com/wrapper.pdf
// https://stackoverflow.com/questions/24915818/c-forward-method-calls-to-embed-object-without-inheritance
// https://stackoverflow.com/questions/13800449/c11-how-to-proxy-class-function-having-only-its-name-and-parent-class/13885092#13885092
#define PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    type(ARGS&& ... args): base(std::forward<ARGS>(args) ...)
//special handling for last param:
#define NEAR_PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    type(ARGS&& ... args, key_t key = 0): base(std::forward<ARGS>(args) ..., key)

//make a unique key based on source code location:
//std::string srckey;
#define SRCKEY  ShmHeap::crekey(__FILE__, __LINE__)


#include <unistd.h> //sysconf()
#define PAGE_SIZE  4096 //NOTE: no Userland #define for this, so set it here and then check it at run time; https://stackoverflow.com/questions/37897645/page-size-undeclared-c
#if 0
//void check_page_size()
int main_other(int argc, const char* argv[]);
//template <typename... ARGS >
//int main_other(ARGS&&... args)
//{ if( a<b ) std::bind( std::forward<FN>(fn), std::forward<ARGS>(args)... )() ; }
int main(int argc, const char* argv[])
#define main  main_other
{
    int pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize != PAGE_SIZE) ATOMIC_MSG(RED_MSG << "PAGE_SIZE incorrect: is " << pagesize << ", expected " << PAGE_SIZE << ENDCOLOR)
    else ATOMIC_MSG(GREEN_MSG << "PAGE_SIZE correct: " << pagesize << ENDCOLOR);
    return main(argc, argv);
}
#endif


//minimal memory pool:
//can be used with stl containers, but will only reclaim/dealloc space at highest used address
//NOTE: do not store pointers; addresses can vary between processes
//use shmalloc + placement new to put this in shared memory
//NOTE: no key/symbol mgmt here; use a separate lkup sttr or container
template <int SIZE> //, int PAGESIZE = 0x1000>
class MemPool
{
//    SrcLine m_srcline;
public:
    MemPool(SrcLine srcline = 0): m_storage{2} { debug(srcline); } // { m_storage[0] = 1; } //: m_used(m_storage[0]) { m_used = 0; }
    ~MemPool() { ATOMIC(std::cout << YELLOW_MSG << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); }
public:
    inline size_t used(size_t req = 0) { return (m_storage[0] + req) * sizeof(m_storage[0]); }
    inline size_t avail() { return (SIZEOF(m_storage) - m_storage[0]) * sizeof(m_storage[0]); }
//    inline void save() { m_storage[1] = m_storage[0]; }
//    inline void restore() { m_storage[0] = m_storage[1]; }
    inline size_t nextkey() { return m_storage[0]; }
    void* alloc(size_t count, size_t key, SrcLine srcline = 0) //repeat a previous alloc()
    {
        size_t saved = m_storage[0];
        m_storage[0] = key;
        void* retval = alloc(count, srcline);
        m_storage[0] = saved;
        return retval;
    }
    /*virtual*/ void* alloc(size_t count, SrcLine srcline = 0)
    {
        if (count < 1) return nullptr;
        count = divup(count, sizeof(m_storage[0])); //for alignment
        ATOMIC_MSG(BLUE_MSG << "MemPool: alloc count " << count << ", used(req) " << used(count + 1) << " vs size " << sizeof(m_storage) << ENDCOLOR_ATLINE(srcline));
        if (used(count + 1) > sizeof(m_storage)) enlarge(used(count + 1));
        m_storage[m_storage[0]++] = count;
        void* ptr = &m_storage[m_storage[0]];
        m_storage[0] += count;
        ATOMIC_MSG(YELLOW_MSG << "MemPool: allocated " << count << " octobytes at " << FMT("%p") << ptr << ", used " << used() << ", avail " << avail() << ENDCOLOR);
        return ptr; //malloc(count);
    }
    /*virtual*/ void free(void* addr, SrcLine srcline = 0)
    {
//        free(ptr);
        size_t inx = ((intptr_t)addr - (intptr_t)m_storage) / sizeof(m_storage[0]);
        if ((inx < 1) || (inx >= SIZE)) ATOMIC_MSG(RED_MSG << "inx " << inx << ENDCOLOR);
        if ((inx < 1) || (inx >= SIZE)) throw std::bad_alloc();
        size_t count = m_storage[--inx] + 1;
        ATOMIC_MSG(BLUE_MSG << "MemPool: deallocate count " << count << " at " << FMT("%p") << addr << ", reclaim " << m_storage[0] << " == " << inx << " + " << count << "? " << (m_storage[0] == inx + count) << ENDCOLOR_ATLINE(srcline));
        if (m_storage[0] == inx + count) m_storage[0] -= count; //only reclaim at end of pool (no other addresses will change)
        ATOMIC_MSG(YELLOW_MSG << "MemPool: deallocated " << count << " octobytes at " << FMT("%p") << addr << ", used " << used() << ", avail " << avail() << ENDCOLOR);
    }
    /*virtual*/ void enlarge(size_t count, SrcLine srcline = 0)
    {
        count = rdup(count, PAGE_SIZE / sizeof(m_storage[0]));
        ATOMIC_MSG(RED_MSG << "MemPool: want to enlarge " << count << " octobytes" << ENDCOLOR_ATLINE(srcline));
        throw std::bad_alloc(); //TODO
    }
    static const char* TYPENAME();
//private:
#define EXTRA  2
    void debug(SrcLine srcline = 0) { ATOMIC_MSG(BLUE_MSG << "MemPool: size " << SIZE << " (" << sizeof(*this) << ") = #elements " << (SIZEOF(m_storage) - 1) << "+1, sizeof(storage elements) " << sizeof(m_storage[0]) << ENDCOLOR_ATLINE(srcline)); }
//    typedef struct { size_t count[1]; uint32_t data[0]; } entry;
    size_t m_storage[EXTRA + divup(SIZE, sizeof(size_t))]; //first element = used count
//    size_t& m_used;
};
template<>
const char* MemPool<40>::TYPENAME() { return "MemPool<40>"; }
template<>
const char* MemPool<300>::TYPENAME() { return "MemPool<300>"; }


#include <mutex>
#include <atomic>
#include <iostream> //std::cout

//mutex with lock indicator:
//lock flag is atomic to avoid extra locks
class MutexWithFlag: public std::mutex
{
public:
//use atomic data member rather than a getter so caller doesn't need to lock/unlock each time just to check locked flag
    std::atomic<bool> islocked; //NOTE: mutex.try_lock() is not reliable (spurious failures); use explicit flag instead; see: http://en.cppreference.com/w/cpp/thread/mutex/try_lock
public: //ctor/dtor
    MutexWithFlag(): islocked(false) {}
//    ~MutexWithFlag() { if (islocked) unlock(); }
    ~MutexWithFlag() { ATOMIC_MSG(BLUE_MSG << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); if (islocked) unlock(); }
public: //member functions
    void lock() { debug("lock"); std::mutex::lock(); islocked = true; }
    void unlock() { debug("unlock"); islocked = false; std::mutex::unlock(); }
//    static void unlock(MutexWithFlag* ptr) { ptr->unlock(); } //custom deleter for use with std::unique_ptr
    static const char* TYPENAME();
private:
    void debug(const char* func) { ATOMIC_MSG(YELLOW_MSG << func << ENDCOLOR); }
};
const char* MutexWithFlag::TYPENAME() { return "MutexWithFlag"; }


//mutex mixin class:
//3 ways to wrap parent class methods:
//    void* ptr1 = pool20->alloc(10);
//    void* ptr2 = pool20()().alloc(4);
//    void* ptr3 = pool20.base().base().alloc(1);
// https://stackoverflow.com/questions/4421706/what-are-the-basic-rules-and-idioms-for-operator-overloading?rq=1
//NOTE: operator-> is the only one that is applied recursively until a non-class is returned, and then it needs a pointer

#include <memory> //std::deleter

//used to attach mutex directly to another object type (same memory space)
//optional auto lock/unlock around access to methods
//also wraps all method calls via operator-> with mutex lock/unlock
template <class TYPE, bool AUTO_LOCK = true> //derivation
//class WithMutex //mixin/wrapper
class WithMutex: public TYPE //derivation
{
public: //ctor/dtor
//    typedef TYPE base_type;
    typedef WithMutex<TYPE, AUTO_LOCK> this_type;
//    typedef decltype(*this) this_type;
//    Mutexed(): m_locked(false) {} //mixin
//    bool& islocked;
    PERFECT_FWD2BASE_CTOR(WithMutex, TYPE) {} //std::cout << "with mutex\n"; } //, islocked(m_mutex.islocked /*false*/) {} //derivation
//    PERFECT_FWD2BASE_CTOR(WithMutex, m_wrapped), m_locked(false) {} //wrapped
//    Mutexed(TYPE* ptr):
    ~WithMutex() { ATOMIC_MSG(BLUE_MSG << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); }
//    TYPE* operator->() { return this; } //allow access to parent members (auto-upcast only needed for derivation)
private:
//protected:
//    TYPE m_wrapped;
public:
    MutexWithFlag /*std::mutex*/ m_mutex;
//    std::atomic<bool> m_locked; //NOTE: mutex.try_lock() is not reliable (spurious failures); use explicit flag instead; see: http://en.cppreference.com/w/cpp/thread/mutex/try_lock
public:
//    bool islocked() { return m_locked; } //if (m_mutex.try_lock()) { m_mutex.unlock(); return false; }
//private:
//    void lock() { ATOMIC_MSG(YELLOW_MSG << "lock" << ENDCOLOR); m_mutex.lock(); /*islocked = true*/; }
//    void unlock() { ATOMIC_MSG(YELLOW_MSG << "unlock" << ENDCOLOR); /*islocked = false*/; m_mutex.unlock(); }
private:
#if 1
//helper class to ensure unlock() occurs after member function returns
//TODO: derive from unique_ptr<>
    class unlock_later
    {
        this_type* m_ptr;
    public: //ctor/dtor to wrap lock/unlock
        unlock_later(this_type* ptr): m_ptr(ptr) { /*if (AUTO_LOCK)*/ m_ptr->m_mutex.lock(); }
        ~unlock_later() { /*if (AUTO_LOCK)*/ m_ptr->m_mutex.unlock(); }
    public:
        inline TYPE* operator->() { return m_ptr; } //allow access to wrapped members
//        inline operator TYPE() { return *m_ptr; } //allow access to wrapped members
//        inline TYPE& base() { return *m_ptr; }
//        inline TYPE& operator()() { return *m_ptr; }
    };
#endif
#if 0
    typedef std::unique_ptr<this_type, void(*)(this_type*)> my_ptr_type;
    class unlock_later: public my_ptr_type
    {
    public: //ctor/dtor to wrap lock/unlock
//        PERFECT_FWD2BASE_CTOR(unlock_later, my_type) {} //std::cout << "with mutex\n"; } //, islocked(m_mutex.islocked /*false*/) {} //derivation
        unlock_later(this_type* ptr): my_ptr_type(ptr) { base_type::get()->m_mutex.lock(); }
        ~unlock_later() { base_type::get()->m_mutex.unlock(); }
//    public:
    };
#endif
public: //pointer operator; allows safe multi-process access to shared object's member functions
//nope    TYPE* /*ProxyCaller*/ operator->() { typename WithMutex<TYPE>::scoped_lock lock(m_inner.ptr); return m_inner.ptr; } //ProxyCaller(m_ps.ptr); } //https://stackoverflow.com/questions/22874535/dependent-scope-need-typename-in-front
//    typedef std::unique_ptr<this_type, void(*)(this_type*)> unlock_later; //decltype(&MutexWithFlag::unlock)> unlock_later;
//    typedef std::unique_ptr<TYPE, void(*)(TYPE*)> unlock_later; //decltype(&MutexWithFlag::unlock)> unlock_later;
#if 0
    template<bool NEED_LOCK = AUTO_LOCK>
    inline typename std::enable_if<!NEED_LOCK, TYPE*>::type operator->() { return this; }
    template<bool NEED_LOCK = AUTO_LOCK>
//    inline typename std::enable_if<NEED_LOCK, unlock_later/*&&*/>::type operator->() { m_mutex.lock(); return unlock_later(this, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    inline typename std::enable_if<NEED_LOCK, unlock_later/*&&*/>::type operator->() { return unlock_later(this); } //, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
#endif
    inline unlock_later operator->() { return unlock_later(this); } //, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
//    inline unlock_later/*&&*/ operator->() { m_mutex.lock(); return unlock_later(this, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    static const char* TYPENAME();
#if 0
    inline unlock_later/*&&*/ operator->() { return unlock_later(this); }
//    inline operator TYPE() { return unlock_later(this); }
    inline unlock_later locked() { return unlock_later(this); }
    inline TYPE& nested() { return locked().base(); }
//    inline unlock_later operator()() { return unlock_later(this); }
//    TYPE* operator->() { return this; } //allow access to parent members (auto-upcast only needed for derivation)
#endif
//public:
//    static void lock() { std::cout << "lock\n" << std::flush; }
//    static void unlock() { std::cout << "unlock\n" << std::flush; }
};
template<>
const char* WithMutex<MemPool<40>, true>::TYPENAME() { return "WithMutex<MemPool<40>, true>"; }
template<>
const char* WithMutex<MemPool<300>, true>::TYPENAME() { return "WithMutex<MemPool<300>, true>"; }


#if 1
//shm object wrapper:
//use operator-> to access wrapped object's methods
//std::shared_ptr<> used to clean up shmem afterwards
//NOTE: use key to share objects across processes
template <typename TYPE> //, typename BASE_TYPE = std::unique_ptr<TYPE, void(*)(TYPE*)>>
class shm_obj//: public TYPE& //: public std::unique_ptr<TYPE, void(*)(TYPE*)> //use smart ptr to clean up afterward
{
//    typedef std::shared_ptr<TYPE /*, shmdeleter<TYPE>*/> base_type; //clup(&this, shmdeleter<TYPE());
//    typedef std::unique_ptr<TYPE, shmdeleter<TYPE>> base_type; //clup(&thisl, shmdeleter<TYPE>());
//    typedef std::unique_ptr<TYPE, void(*)(TYPE*)> base_type;
//    typedef std::unique_ptr<TYPE, void(*)(TYPE*)> base_type; //decltype(&MutexWithFlag::unlock)> unlock_later;
//    base_type m_ptr; //clean up shmem automatically
    std::shared_ptr<TYPE /*, shmdeleter<TYPE>*/> m_ptr; //clean up shmem automtically
public: //ctor/dtor
//equiv to:    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
#define INNER_CREATE(args, key)  m_ptr(new (shmalloc(sizeof(TYPE), key, SRCLINE)) TYPE(args), [](TYPE* ptr) { shmfree(ptr, SRCLINE); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    NEAR_PERFECT_FWD2BASE_CTOR(shm_obj, INNER_CREATE) {} //, m_clup(this, TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) {} //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
#undef INNER_CREATE
public: //operators
//    inline TYPE* operator->() { return base_type::get(); } //allow access to wrapped members; based on http://www.stroustrup.com/wrapper.pdf
//    inline operator TYPE&() { return *m_ptr.get(); }
    inline TYPE& operator->() { return *m_ptr.get(); }
    static const char* TYPENAME();
//private:
//    void debug() { ATOMIC_MSG((*this)->TYPENAME << ENDCOLOR); }
};
template<>
const char* shm_obj<WithMutex<MemPool<40>, true>>::TYPENAME() { return "shm_obj<WithMutex<MemPool<40>, true>>"; }
#endif


#ifdef TEST1_GOOD //shm_obj test, single proc
//#define MEMSIZE  rdup(10, 8)+8 + rdup(4, 8)+8
//WithMutex<MemPool<MEMSIZE>> pool20;
WithMutex<MemPool<rdup(10, 8)+8 + rdup(4, 8)+8>> pool20;
//typedef WithMutex<MemPool<MEMSIZE>> pool_type;
//pool_type pool20;
int main(int argc, const char* argv[])
{
    ATOMIC_MSG(PINK_MSG << "data space:" <<ENDCOLOR);
    void* ptr1 = pool20->alloc(10);
//    void* ptr1 = pool20()()->alloc(10);
//    void* ptr1 = pool20.base().base().alloc(10);
    void* ptr2 = pool20.alloc(4);
    pool20->free(ptr2);
    void* ptr3 = pool20->alloc(1);

    ATOMIC_MSG(PINK_MSG << "stack:" <<ENDCOLOR);
    MemPool<rdup(1, 8)+8 + rdup(1, 8)+8> pool10; //don't need mutex on stack mem (not safe to share it)
    void* ptr4 = pool10.alloc(1);
    void* ptr5 = pool10.alloc(1);
    void* ptr6 = pool10.alloc(0);

    typedef decltype(pool20) pool_type; //use same type as pool20
    ATOMIC_MSG(PINK_MSG << "shmem: actual size " << sizeof(pool_type) << ENDCOLOR);
//    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> shmpool(new (shmalloc(sizeof(pool_type))) pool_type(), shmdeleter<pool_type>());
//    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> shmpool(new (shmalloc(sizeof(pool_type))) pool_type(), shmdeleter<pool_type>());
//    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
//    pool_type shmpool = *shmpool_ptr; //.get();
//#define SHM_DECL(type, var)  type& var = *new (shmalloc(sizeof(type))) type(); std::shared_ptr<type> var##_clup(&var, shmdeleter<type>())
#if 0
    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> clup(&shmpool, shmdeleter<pool_type>());
//OR
    std::unique_ptr<pool_type, shmdeleter<pool_type>> clup(&shmpool, shmdeleter<pool_type>());
#endif
//    SHM_DECL(pool_type, shmpool); //equiv to "pool_type shmpool", but allocates in shmem
    shm_obj<pool_type> shmpool; //put it in shared memory instead of stack
//    ATOMIC_MSG(BLUE_MSG << shmpool.TYPENAME() << ENDCOLOR); //NOTE: don't use -> here (causes recursive lock)
//    shmpool->m_mutex.lock();
//    shmpool->debug();
//    shmpool->m_mutex.unlock();
    void* ptr7 = shmpool->alloc(10);
    void* ptr8 = shmpool->alloc(4); //.alloc(4);
    shmpool->free(ptr8);
    void* ptr9 = shmpool->alloc(1);
//    shmfree(&shmpool);

    return 0;
}
#endif


//typedef shm_obj<WithMutex<MemPool<PAGE_SIZE>>> ShmHeap;
//class ShmHeap;
typedef WithMutex<MemPool<300>> ShmHeap; //bare sttr here, not smart_ptr

//minimal stl allocator (C++11)
//based on example at: https://msdn.microsoft.com/en-us/library/aa985953.aspx
//NOTE: use a separate sttr for shm key/symbol mgmt
template <class TYPE>
struct ShmAllocator  
{  
    typedef TYPE value_type;
    ShmAllocator() noexcept {} //default ctor; not required by STL
    ShmAllocator(ShmHeap* heap): m_heap(heap) {}
    template<class OTHER>
    ShmAllocator(const ShmAllocator<OTHER>& other) noexcept: m_heap(other.m_heap) {} //converting copy ctor (more efficient than C++03)
    template<class OTHER>
    bool operator==(const ShmAllocator<OTHER>& other) const noexcept { return m_heap/*.get()*/ == other.m_heap/*.get()*/; } //true; }
    template<class OTHER>
    bool operator!=(const ShmAllocator<OTHER>& other) const noexcept { return m_heap/*.get()*/ != other.m_heap/*.get()*/; } //false; }
    TYPE* allocate(const size_t count = 1, SrcLine srcline = 0) const { return allocate(count, 0, srcline); }
    TYPE* allocate(const size_t count, key_t key, SrcLine srcline) const
    {
        if (!count) return nullptr;
        if (count > static_cast<size_t>(-1) / sizeof(TYPE)) throw std::bad_array_new_length();
        void* const ptr = m_heap? m_heap->alloc(count * sizeof(TYPE), key, srcline): shmalloc(count * sizeof(TYPE), key, srcline);
        ATOMIC_MSG(YELLOW_MSG << "ShmAllocator: allocated " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << FMT(" bytes for key 0x%lx") << key << " from " << (m_heap? "custom": "heap") << " at " << FMT("%p") << ptr << ENDCOLOR);
        if (!ptr) throw std::bad_alloc();
        return static_cast<TYPE*>(ptr);
    }  
    void deallocate(TYPE* const ptr, size_t count = 1, SrcLine srcline = 0) const noexcept
    {
        ATOMIC_MSG(YELLOW_MSG << "ShmAllocator: deallocate " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << " bytes from " << (m_heap? "custom": "heap") << " at " << FMT("%p") << ptr << ENDCOLOR_ATLINE(srcline));
        if (m_heap) m_heap->free(ptr);
        else shmfree(ptr);
    }
//    std::shared_ptr<ShmHeap> m_heap; //allow heap to be shared between allocators
    ShmHeap* m_heap; //allow heap to be shared between allocators
    static const char* TYPENAME();
};


#if 1 //def WANT_TEST //1 //proxy example, multi-proc
#include "vectorex.h"
#include <unistd.h> //fork()
class TestObj
{
//    std::string m_name;
    char m_name[20]; //store name directly in object so shm object doesn't use char pointer
//    SrcLine m_srcline;
    int m_count;
public:
    explicit TestObj(const char* name, SrcLine srcline = 0): /*m_name(name),*/ m_count(0) { strncpy(m_name, name, sizeof(m_name)); ATOMIC_MSG(CYAN_MSG << FMT("TestObj@%p") << this << " '" << name << "' ctor" << ENDCOLOR_ATLINE(srcline)); }
    TestObj(const TestObj& that): /*m_name(that.m_name),*/ m_count(that.m_count) { strcpy(m_name, that.m_name); ATOMIC_MSG(CYAN_MSG << FMT("TestObj@%p") << this << " '" << m_name << FMT("' copy ctor from %p") << that << ENDCOLOR); }
    ~TestObj() { ATOMIC_MSG(CYAN_MSG << FMT("TestObj@%p") << this << " '" << m_name << "' dtor" << ENDCOLOR); } //only used for debug
public:
    void print() { ATOMIC_MSG(BLUE_MSG << "TestObj.print: (name" << FMT("@%p") << &m_name << FMT(" contents@%p") << m_name/*.c_str()*/ << " '" << m_name << "', count" << FMT("@%p") << &m_count << " " << m_count << ")" << ENDCOLOR); }
    int& inc() { return ++m_count; }
};
template <>
const char* ShmAllocator<TestObj>::TYPENAME() { return "TestObj"; }
template <>
const char* ShmAllocator<std::vector<TestObj, ShmAllocator<TestObj>>>::TYPENAME() { return "std::vector<TestObj>"; }
template<>
const char* WithMutex<TestObj, true>::TYPENAME() { return "WithMutex<TestObj, true>"; }


#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> //fork(), getpid()
#include "ipc.h" //IpcThread(), IpcPipe()
#include "elapsed.h" //timestamp()
//#include <memory> //unique_ptr<>
int main(int argc, const char* argv[])
{
//    ShmKey key(12);
//    std::cout << FMT("0x%lx\n") << key.m_key;
//    key = 0x123;
//    std::cout << FMT("0x%lx\n") << key.m_key;
#if 0 //best way, but doesn't match Node.js fork()
    ShmSeg shm(0, ShmSeg::persist::NewTemp, 300); //key, persistence, size
#else //use pipes to simulate Node.js pass env to child; for example see: https://bytefreaks.net/programming-2/c-programming-2/cc-pass-value-from-parent-to-child-after-fork-via-a-pipe
//    int fd[2];
//	pipe(fd); //create pipe descriptors < fork()
    IpcPipe pipe;
#endif
//    bool owner = fork();
    IpcThread thread(SRCLINE);
//    pipe.direction(owner? 1: 0);
    if (thread.isChild()) sleep(1); //give parent head start
    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent": "child") << " pid " << getpid() << " start" << ENDCOLOR);
#if 1 //simulate Node.js fork()
//    ShmSeg& shm = *std::allocator<ShmSeg>(1); //alloc space but don't init
//    ShmAllocator<ShmSeg> heap_alloc;
//used shared_ptr<> for ref counting *and* to allow skipping ctor in child procs:
    std::shared_ptr<ShmHeap> shmheaptr; //(ShmAllocator<ShmSeg>().allocate()); //alloc space but don't init yet
//    ShmHeap shmheap; //(ShmAllocator<ShmSeg>().allocate()); //alloc space but don't init yet
//    ShmSeg* shmptr = heap_alloc.allocate(1); //alloc space but don't init yet
//    std::unique_ptr<ShmSeg> shm = shmptr;
//    if (owner) shmptr.reset(new /*(shmptr.get())*/ ShmHeap(0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//    if (owner) shmheaptr.reset(new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(), shmdeleter<ShmHeap>()); //[](TYPE* ptr) { shmfree(ptr); }); //0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//#define INNER_CREATE(args)  m_ptr(new (shmalloc(sizeof(TYPE))) TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
//    shmheaptr.reset(thread.isParent()? new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(): (ShmHeap*)shmalloc(sizeof(ShmHeap), pipe.child_rcv(SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //call ctor to init (parent only)
    shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), thread.isParent()? 0: pipe.rcv(SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //call ctor to init (parent only)
    if (thread.isParent()) new (shmheaptr.get()) ShmHeap(); //call ctor to init (parent only)
    if (thread.isParent()) pipe.send(shmkey(shmheaptr.get()), SRCLINE);
    ATOMIC_MSG(BLUE_MSG << FMT("shmheap at %p") << shmheaptr.get() << ENDCOLOR);
//    else shmptr.reset(new /*(shmptr.get())*/ ShmHeap(rcv(fd), ShmSeg::persist::Reuse, 1));
//    else shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), rcv(fd, SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //don't call ctor; Seg::persist::Reuse, 1));
//    std::vector<std::string> args;
//    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
//    if (!owner) new (&shm) ShmSeg(shm.shmkey(), ShmSeg::persist::Reuse, 1); //attach to same shmem seg (child only)
#endif
//parent + child have ref; parent init; parent + child use it; parent + child detach; parent frees

    TestObj bare("berry", SRCLINE);
    bare.inc();
    bare.print();

    WithMutex<TestObj> prot("protected", SRCLINE);
//    ((TestObj)prot).inc();
//    ((TestObj)prot).print();
    prot->inc();
    prot->print();
    
//    ProxyWrapper<Person> person(new Person("testy"));
//can't set ref to rvalue :(    ShmSeg& shm = owner? ShmSeg(0x1234beef, ShmSeg::persist::NewTemp, 300): ShmSeg(0x1234beef, ShmSeg::persist::Reuse, 300); //key, persistence, size
//    if (owner) new (&shm) ShmSeg(0x1234beef, ShmSeg::persist::NewTemp, 300);
//    else new (&shm) ShmSeg(0x1234beef, ShmSeg::persist::Reuse); //key, persistence, size

//    ShmHeap<TestObj> shm_heap(shm_seg);
//    ATOMIC_MSG("shm key " << FMT("0x%lx") << shm.shmkey() << ", size " << shm.shmsize() << ", adrs " << FMT("%p") << shm.shmptr() << "\n");
//    std::set<Example, std::less<Example>, allocator<Example, heap<Example> > > foo;
//    typedef ShmAllocator<TestObj> item_allocator_type;
//    item_allocator_type item_alloc; item_alloc.m_heap = shmptr; //explicitly create so it can be reused in other places (shares state with other allocators)
    ShmAllocator<TestObj> item_alloc(shmheaptr.get()); //item_alloc.m_heap = shmptr; //explicitly create so it can be reused in other places (shares state with other allocators)
//reuse existing shm obj in child proc (bypass ctor):

//    if (owner) shmheaptr.reset(new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(), shmdeleter<ShmHeap>()); //[](TYPE* ptr) { shmfree(ptr); }); //0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//    if (owner) send(fd, shmkey(shmheaptr.get()), SRCLINE);
//    else shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), rcv(fd, SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //don't call ctor; Seg::persist::Reuse, 1));

    key_t svkey = shmheaptr->nextkey();
    TestObj& testobj = *(TestObj*)item_alloc.allocate(1, svkey, SRCLINE); //NOTE: ref avoids copy ctor
    ATOMIC_MSG(BLUE_MSG << "next key " << svkey << FMT(" => adrs %p") << &testobj << ENDCOLOR);
    if (thread.isParent()) new (&testobj) TestObj("testy", SRCLINE);
//    TestObj& testobj = owner? *new (item_alloc.allocate(1, svkey, SRCLINE)) TestObj("testy", SRCLINE): *(TestObj*)item_alloc.allocate(1, svkey, SRCLINE); //NOTE: ref avoids copy ctor
//    shm_ptr<TestObj> testobj("testy", shm_alloc);
    testobj.inc();
    testobj.inc();
    testobj.print();
    ATOMIC_MSG(BLUE_MSG << FMT("&testobj %p") << &testobj << ENDCOLOR);

#if 0
//    typedef std::vector<TestObj, item_allocator_type> list_type;
    typedef std::vector<TestObj, ShmAllocator<TestObj>> list_type;
//    list_type testlist;
//    ShmAllocator<list_type, ShmHeap<list_type>>& list_alloc = item_alloc.rebind<list_type>.other;
//    item_allocator_type::rebind<list_type> list_alloc;
//    typedef typename item_allocator_type::template rebind<list_type>::other list_allocator_type; //http://www.cplusplus.com/forum/general/161946/
//    typedef ShmAllocator<list_type> list_allocator_type;
//    SmhAllocator<list_type, ShmHeap<list_type>> shmalloc;
//    list_allocator_type list_alloc(item_alloc); //list_alloc.m_heap = shmptr; //(item_alloc); //share state with item allocator
    ShmAllocator<list_type> list_alloc(item_alloc); //list_alloc.m_heap = shmptr; //(item_alloc); //share state with item allocator
//    list_type testobj(item_alloc); //stack variable
//    list_type* ptr = list_alloc.allocate(1);
    svkey = shmheaptr->nextkey();
//    list_type& testlist = *new (list_alloc.allocate(1, SRCLINE)) list_type(item_alloc); //(item_alloc); //custom heap variable
    list_type& testlist = *(list_type*)list_alloc.allocate(1, svkey, SRCLINE);
    if (owner) new (&testlist) list_type(item_alloc); //(item_alloc); //custom heap variable
    ATOMIC_MSG(BLUE_MSG << FMT("&list %p") << &testlist << ENDCOLOR);

    testlist.emplace_back("list1");
    testlist.emplace_back("list2");
    testlist[0].inc();
    testlist[0].inc();
    testlist[1].inc();
    testlist[0].print();
    testlist[1].print();
#endif

    std::cout
        << "sizeof(berry) = " << sizeof(bare)
        << ", sizeof(prot) = " << sizeof(prot)
        << ", sizeof(test obj) = " << sizeof(testobj)
//        << ", sizeof(test list) = " << sizeof(testlist)
//        << ", sizeof(shm_ptr<int>) = " << sizeof(shm_ptr<int>)
        << "\n" << std::flush;

    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent (waiting to)": "child") << " exit" << ENDCOLOR);
    if (thread.isParent()) thread.join(SRCLINE); //waitpid(-1, NULL /*&status*/, /*options*/ 0); //NOTE: will block until child state changes
    else shmheaptr.reset((ShmHeap*)0); //don't call dtor for child
    return 0;
}
#endif










//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////



//#define SHM_DECL(type, var)  type& var = *new (shmalloc(sizeof(type))) type(); std::shared_ptr<type> var##_clup(&var, shmdeleter<type>())
//template <typename TYPE>
//class shm_ptr: std::shared_ptr<TYPE> //use smart ptr to clean up afterward
//{
//public: //ctor/dtor
//#define INNER_CREATE(args)  std::shared_ptr<TYPE>(new (shmalloc(sizeof(TYPE))) TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
//    PERFECT_FWD2BASE_CTOR(shm_ptr, INNER_CREATE) {}
//#undef INNER_CREATE
//public: //operators
//    inline TYPE* operator->() { return std::shared_ptr<TYPE>::get(); } //allow access to wrapped members
//};
//#define shm_ptr(type)  std::unique_ptr<type> (new (shmalloc(sizeof(TYPE))) TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr


//shared memory lookup + alloc
//template<type TYPE>
//class ShmObject
//{
//public:
//    explicit ShmObject() {}
//    ~ShmObject() {}
//public:
//};
//MsgQue& mainq = *(MsgQue*)shmlookup(SRCKEY, [] { return new (shmaddrshmalloc(sizeof(MsgQue), key)) MsgQue("mainq"); }
//MsgQue& mainq = *(MsgQue*)shmlookup(SRCKEY, [] (shmaddr) { return new (shmaddr) MsgQue("mainq"); }
//#define SHARED(key, type, ctor)  *(type*)ShmHeapAlloc::shmheap.alloc(key, sizeof(type), true, [] (void* shmaddr) { new (shmaddr) ctor; })

//#define VOID  uintptr_t //void //kludge: avoid compiler warnings with pointer arithmetic


#if 0
template <typename TYPE>
class shm_ptr
{
public: //ctor/dtor
#define INNER_CREATE(args)  m_ptr(new (shmalloc(sizeof(TYPE))) TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    PERFECT_FWD2BASE_CTOR(shm_ptr, INNER_CREATE) {}
#undef INNER_CREATE
public: //operators
    inline TYPE* operator->() { return m_ptr.get(); } //allow access to wrapped members; based on http://www.stroustrup.com/wrapper.pdf
private:
    std::shared_ptr<TYPE> m_ptr; //use smart ptr to clean up afterward
};
#endif
//class shm_ptr
//{
//public: //ctor/dtor
//#define INNER_CREATE(args)  m_var(*new (shmalloc(sizeof(TYPE))) TYPE(args)) //pass ctor args down into m_var ctor
//    PERFECT_FWD2BASE_CTOR(shm_ptr, INNER_CREATE), m_clup(&m_var, shmdeleter<TYPE>()) {}
//#undef INNER_CREATE
//public: //operators
//    inline TYPE* operator->() { return &m_var; } //allow access to wrapped members
//private: //members
//    TYPE& m_var; //= *new (shmalloc(sizeof(TYPE))) TYPE();
//    std::shared_ptr<TYPE> m_clup; //(&var, shmdeleter<TYPE>())
//};


#if 0
class ShmHeap //: public ShmSeg
{
public:
//    PERFECT_FWD2BASE_CTOR(ShmHeap, ShmSeg), m_used(sizeof(m_used)) {}
public:
    inline size_t used() { return m_used; }
    inline size_t avail() { return shmsize() - m_used; }
    void* alloc(size_t count)
    {
//        if (count > max_size()) { throw std::bad_alloc(); }
//        pointer ptr = static_cast<pointer>(::operator new(count * sizeof(type), ::std::nothrow));
        if (m_used + count > shmsize()) throw std::bad_alloc();
        void* ptr = shmptr() + m_used;
        m_used += count;
        ATOMIC_MSG(YELLOW_MSG << "ShmHeap: allocated " << count << " bytes at " << FMT("%p") << ptr << ", " << avail() << " bytes remaining" << ENDCOLOR);
        return ptr;
    }
    void dealloc(void* ptr)
    {
        int count = 1;
        ATOMIC_MSG(YELLOW_MSG << "ShmHeap: deallocate " << count << " bytes at " << FMT("%p") << ptr << ", " << avail() << " bytes remaining" << ENDCOLOR);
//        ::operator delete(ptr);
    }
private:
    size_t m_used;
};
#endif


#if 0
//shmem key:
//define unique type for function signatures
//to look at shm:
// ipcs -m 
class ShmKey
{
public: //debug only
    key_t m_key;
public: //ctor/dtor
//    explicit ShmKey(key_t key = 0): key(key? key: crekey()) {}
    /*explicit*/ ShmKey(const int& key = 0): m_key(key? key: crekey()) { std::cout << "ShmKey ctor " << FMT("0x%lx\n") << m_key; }
//    explicit ShmKey(const ShmKey&& other): m_key((int)other) {} //copy ctor
//    ShmKey(const ShmKey& that) { *this = that; } //copy ctor
    ~ShmKey() {}
public: //operators
    ShmKey& operator=(const int& key) { m_key = key? key: crekey(); return *this; } //conv op; //std::cout << "key assgn " << FMT("0x%lx") << key << FMT(" => 0x%lx") << m_key << "\n"; return *this; } //conv operator
    inline operator int() { return /*(int)*/m_key; }
//    bool operator!() { return key != 0; }
//    inline key_t 
//??    std::ostream& operator<<(const ShmKey& value)
public: //static helpers
    static inline key_t crekey() { int r = (rand() << 16) | 0xfeed; /*std::cout << FMT("rnd key 0x%lx\n") << r*/; return r; }
};
#endif


#if 0
//shared memory segment:
//used to store persistent/shared data across processes
//NO: stores persistent state between ShmAllocator instances
class ShmSeg
{
public: //ctor/dtor
    enum class persist: int {Reuse = 0, NewTemp = -1, NewPerm = +1};
//    ShmSeg(persist cre = persist::NewTemp, size_t size = 0x1000): m_ptr(0), m_keep(true)
    explicit ShmSeg(key_t key = 0, persist cre = persist::NewTemp, size_t size = 0x1000): m_ptr(0), m_keep(true)
    {
        ATOMIC_MSG(CYAN_MSG << "ShmSeg.ctor: key " << FMT("0x%lx") << key << ", persist " << (int)cre << ", size " << size << ENDCOLOR);
        if (cre != persist::Reuse) destroy(key); //start fresh
        m_keep = (cre != persist::NewTemp);        
        create(key, size, cre != persist::Reuse);
//            std::cout << "ctor from " << __FILE__ << ":" << __LINE__ << "\n" << std::flush;
    }
    ~ShmSeg()
    {
        detach();
        if (!m_keep) destroy(m_key);
    }
public: //getters
    inline key_t shmkey() const { return m_key; }
    inline void* shmptr() const { return m_ptr; }
//    inline size32_t shmofs(void* ptr) const { return (VOID*)ptr - (VOID*)m_ptr; } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
//    inline size32_t shmofs(void* ptr) const { return (uintptr_t)ptr - (uintptr_t)m_ptr; } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
    inline size_t shmofs(void* ptr) const { return (uintptr_t)ptr - (uintptr_t)m_ptr; } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
//    inline operator uintptr_t(void* ptr) { return (uintptr_t)(*((void**) a));
    inline size_t shmsize() const { return m_size; }
//    inline size_t shmused() const { return m_used; }
//    inline size_t shmavail() const { return m_size - m_used; }
private: //data
    key_t m_key;
    void* m_ptr;
    size_t m_size; //, m_used;
    bool m_keep;
private: //helpers
    void create(key_t key, size_t size, bool want_new)
    {
        if (!key) key = crekey(); //(rand() << 16) | 0xfeed;
//#define  SHMKEY  ((key_t) 7890) /* base value for shmem key */
//#define  PERMS 0666
//        if (cre != persist::PreExist) destroy(key); //delete first (parent wants clean start)
//        if (!key) key = (rand() << 16) | 0xfeed;
        if ((size < 1) || (size >= 10000000)) throw std::runtime_error("ShmSeg: bad size"); //set reasonable limits
        int shmid = shmget(key, size, 0666 | (want_new? IPC_CREAT | IPC_EXCL: 0)); // | SHM_NORESERVE); //NOTE: clears to 0 upon creation
        ATOMIC_MSG(CYAN_MSG << "ShmSeg: cre shmget key " << FMT("0x%lx") << key << ", size " << size << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR);
        if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
        struct shmid_ds info;
        if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
        void* ptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
        ATOMIC_MSG(BLUE_MSG << "ShmSeg: shmat id " << FMT("0x%lx") << shmid << " => " << FMT("%p") << ptr << ENDCOLOR);
        if (ptr == (void*)-1) throw std::runtime_error(std::string(strerror(errno)));
        m_key = key;
        m_ptr = ptr;
        m_size = info.shm_segsz; //size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
//        m_used = sizeof(size_t);
    }
//    void* alloc(size_t size)
//    {
//        if (shmavail() < size) return 0; //not enough space
//        void* retval = (intptr_t)m_ptr + m_used;
//        m_used += size;
//        return retval;
//    }
    void detach()
    {
        if (!m_ptr) return; //not attached
//  int shmctl(int shmid, int cmd, struct shmid_ds *buf);
        if (shmdt(m_ptr) == -1) throw std::runtime_error(strerror(errno));
        m_ptr = 0; //can't use m_shmptr after this point
        ATOMIC_MSG(CYAN_MSG << "ShmSeg: dettached " << ENDCOLOR); //desc();
    }
    static void destroy(key_t key)
    {
        if (!key) return; //!owner or !exist
        int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
        ATOMIC_MSG(CYAN_MSG << "ShmSeg: destroy " << FMT("key 0x%lx") << key << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR);
        if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
        if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
        throw std::runtime_error(strerror(errno));
    }
public: //custom helpers
    static key_t crekey() { return (rand() << 16) | 0xfeed; }
};
#endif


#if 0 //stl allocator (C++03)
#if 1 //stl-compatible allocator
//shm traits:
//responsible for creating and destroying shm objects (no memory allocate/dealloc)
//based on example from: http://jrruethe.github.io/blog/2015/11/22/allocators/
template <typename TYPE>
class ShmobjTraits
{
public:
    typedef TYPE type;
    template <typename OTHER_TYPE>
    struct rebind { typedef ShmobjTraits<OTHER_TYPE> other; }; //used by stl containers for internal nodes, etc.
public: //ctors
    ShmobjTraits(void) {}
//Copy Constructor:
    template <typename OTHER_TYPE>
    ShmobjTraits(ShmobjTraits<OTHER_TYPE> const& other) {}
public: //operators
//Address of object
    type* address(type& obj) const { return &obj; }
    type const* address(type const& obj) const { return &obj; }
public: //methods
//Construct object:
    void construct(type* ptr, type const& ref) const { new (ptr) type(ref); } //In-place copy construct
//Destroy object:
    void destroy(type* ptr) const { ptr->~type(); }
};


//type-dependent allocator policies:
#define ALLOCATOR_TRAITS(TYPE)  \
    typedef TYPE type; \
    typedef TYPE value_type; \
    typedef TYPE* pointer; \
    typedef TYPE const* const_pointer; \
    typedef TYPE& reference; \
    typedef TYPE const& const_reference; \
    typedef std::size_t size_type; \
    typedef std::ptrdiff_t difference_type

//template <typename TYPE>
//struct max_allocations { enum { value = static_cast<std::size_t>(-1) / sizeof(TYPE)}; };
//struct max_allocations { enum { value = /*static_cast<std::size_t>(-1)*/ 10000000 / sizeof(TYPE); }; }; //take it easy on shm

//allocator policy:
//responsible for allocating and deallocating memory
template <typename TYPE> //, int SHM_SIZE = 0x1000, int SHM_KEY = crekey(), int SHM_PERSIST = ShmSeg::persist::NewTemp>
class ShmHeap //: public ShmSeg
{
public:
    ALLOCATOR_TRAITS(TYPE);
    template <typename OTHER_TYPE>
    struct rebind { typedef ShmHeap<OTHER_TYPE> other; };
public: //ctors
    ShmHeap(void): m_shmseg(*new ShmSeg()) {} //default ctor; create new storage
    ShmHeap(ShmSeg& shmseg): m_shmseg(shmseg) {} //reuse existing storage
//    ShmHeap(/*void*/ ShmSeg& shmseg = ShmSeg()): m_shmseg(shmseg) {} //default ctor
    template <typename OTHER_TYPE>
    ShmHeap(ShmHeap<OTHER_TYPE> const& other): m_shmseg(other.m_shmseg) {} //copy ctor; reuse existing storage
//    ShmHeap(ShmHeap<OTHER_TYPE> const& other): m_shmseg(other.m_shmseg) {} //copy ctor
public: //memory mgmt
//allocate memory:
    pointer allocate(size_type count, const_pointer hint = 0)
    {
        if (count > max_size()) { throw std::bad_alloc(); }
        pointer ptr = static_cast<pointer>(::operator new(count * sizeof(type), ::std::nothrow));
        ATOMIC_MSG(YELLOW_MSG << "ShmHeap: allocated " << count << " " << TYPENAME << "(s) * size " << sizeof(type) << " bytes at " << FMT("%p") << ptr << ENDCOLOR);
        return ptr;
    }
//Delete memory:
    void deallocate(pointer ptr, size_type count)
    {
        ATOMIC_MSG(YELLOW_MSG << "ShmHeap: deallocate " << count << " " << TYPENAME << "(s) * size " << sizeof(type) << " bytes at " << FMT("%p") << ptr << ENDCOLOR);
        ::operator delete(ptr);
    }
//max #objects that can be allocated in one call:
    static const int max_bytes = 10000000; //std::size_t>(-1); //take it easy on shm
    static size_type max_size(void) { return max_bytes / sizeof(TYPE); } //max_allocations<TYPE>::value; }
//    size_type max_size(void) const { return m_shmseg.shmsize() / sizeof(TYPE); } //max_allocations<TYPE>::value; }
//TODO
//private: //data
//    typedef struct { size_t used; std::mutex mutex; } ShmHdr;
//    ShmSeg& m_shmseg;
    std::unique_ptr<ShmSeg> m_shmseg;
    static const char* TYPENAME;
};
//TODO: https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c


//allocator:
//#define FORWARD_ALLOCATOR_TRAITS(C)                  \
//typedef typename C::value_type      value_type;      \
//typedef typename C::pointer         pointer;         \
//typedef typename C::const_pointer   const_pointer;   \
//typedef typename C::reference       reference;       \
//typedef typename C::const_reference const_reference; \
//typedef typename C::size_type       size_type;       \
//typedef typename C::difference_type difference_type

//NOTE from http://en.cppreference.com/w/cpp/memory/allocator:
//since c++11 all instances of a given allocator are interchangeable (containers store an allocator instance)
template <typename TYPE, typename PolicyTYPE = ShmHeap<TYPE>, typename TraitsTYPE = ShmobjTraits<TYPE> >
class ShmAllocator: public PolicyTYPE, public TraitsTYPE
{
public:
// Template parameters
//    typedef PolicyT Policy;
//    typedef TraitsT Traits;
//    FORWARD_ALLOCATOR_TRAITS(Policy)
    ALLOCATOR_TRAITS(TYPE);
    template<typename OTHER_TYPE>
    struct rebind { typedef ShmAllocator<OTHER_TYPE, typename PolicyTYPE::template rebind<OTHER_TYPE>::other, typename TraitsTYPE::template rebind<OTHER_TYPE>::other > other; };
public: //ctors
    ShmAllocator(void) {} //default ctor
    template<typename OTHER_TYPE, typename PolicyOTHER, typename TraitsOTHER>
    ShmAllocator(ShmAllocator<OTHER_TYPE, PolicyOTHER, TraitsOTHER> const& other): PolicyTYPE(other), TraitsTYPE(other) {} //copy ctor
};

//see https://www.codeproject.com/Articles/1089905/A-Custom-STL-std-allocator-Replacement-Improves-Pe
//good bkg/perf info: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2271.html

//equivalencies:
//tell stl which allocators are compatible
//NOTE: since ShmAllocator does not actually deallocate memory, all allocators can be considered ==

//allocators != unless specialization says so:
template <typename TYPE, typename PolicyTYPE, typename TraitsTYPE, typename OTHER_TYPE, typename PolicyOTHER, typename TraitsOTHER>
bool operator==(ShmAllocator<TYPE, PolicyTYPE, TraitsTYPE> const& lhs, ShmAllocator<OTHER_TYPE, PolicyOTHER, TraitsOTHER> const& rhs)
{
//    return false;
    return true;
}
template <typename TYPE, typename PolicyTYPE, typename TraitsTYPE, typename OTHER_TYPE, typename PolicyOTHER, typename TraitsOTHER>
bool operator!=(ShmAllocator<TYPE, PolicyTYPE, TraitsTYPE> const& lhs, ShmAllocator<OTHER_TYPE, PolicyOTHER, TraitsOTHER> const& rhs)
{
//    return !(lhs == rhs);
    return false;
}
//allocator != anything else:
template <typename TYPE, typename PolicyTYPE, typename TraitsTYPE, typename OtherAllocator>
bool operator==(ShmAllocator<TYPE, PolicyTYPE, TraitsTYPE> const& lhs, OtherAllocator const& rhs)
{
//    return false;
    return true;
}
template <typename TYPE, typename PolicyTYPE, typename TraitsTYPE, typename OtherAllocator>
bool operator!=(ShmAllocator<TYPE, PolicyTYPE, TraitsTYPE> const& lhs, OtherAllocator const& rhs)
{
//    return !(lhs == rhs);
    return false;
}
//Specialize for heap policy:
template <typename TYPE, typename TraitsTYPE, typename OTHER_TYPE, typename TraitsOTHER>
bool operator==(ShmAllocator<TYPE, ShmHeap<TYPE>, TraitsTYPE> const& lhs, ShmAllocator<OTHER_TYPE, ShmHeap<OTHER_TYPE>, TraitsOTHER> const& rhs)
{
    return true;
}
template <typename TYPE, typename TraitsTYPE, typename OTHER_TYPE, typename TraitsOTHER>
bool operator!=(ShmAllocator<TYPE, ShmHeap<TYPE>, TraitsTYPE> const& lhs, ShmAllocator<OTHER_TYPE, ShmHeap<OTHER_TYPE>, TraitsOTHER> const& rhs)
{
//    return !(lhs == rhs);
    return false;
}
#endif
#endif


#if 0 //example usage
#include <set>
struct Example
{
    int value;
    Example(int v): value(v) {}
    bool operator<(Example const& other) const
    {
        return value < other.value;
    }
};
int main(int argc, char* argv[])
{
    std::set<Example, std::less<Example>, allocator<Example, heap<Example> > > foo;
    foo.insert(Example(3));
    foo.insert(Example(1));
    foo.insert(Example(4));
    foo.insert(Example(2));
    return 0;
}
#endif


#if 0
//allocator mixin class:
//used to attach allocator to another object type (different address spaces)
//#include <memory> //unique_ptr
//template <class TYPE> //derivation
template <typename TYPE, typename PolicyTYPE = ShmHeap<TYPE>, typename TraitsTYPE = ShmobjTraits<TYPE> >
//class WithAllocator //mixin
class WithAllocator: public TYPE //derivation
{
public: //ctor/dtor
    typedef ShmAllocator<TYPE> Allocator;
//    Mutexed(): m_locked(false) {} //mixin
//    PERFECT_FWD2BASE_CTOR(WithMutex, TYPE), m_locked(false) {} //derivation
//#define PERFECT_FWD2BASE_CTOR(type, base)  
    template<typename ... ARGS>
    WithAllocator(ARGS&& ... args, Allocator& alloc = Allocator()): TYPE(std::forward<ARGS>(args) ..., alloc) {}
//    WithAllocator(TYPE& that, Allocator& alloc = Allocator()): m_data(that), m_alloc(&alloc) {} //copy ctor; is this redundant with perfect fwding?
//    WithAllocator(Allocator& alloc = Allocator()): m_alloc(alloc) {}
//    WithAllocator(TYPE& data, Allocator& alloc = Allocator()): m_data(data), m_alloc(&alloc) {} //NOTE: constructs new allocator if not provided
//    Mutexed(TYPE* ptr):
//    ~Mutexed() {}
//    TYPE* operator->() { return this; } //allow access to parent members (auto-upcast only needed for derivation)
public: //operators; NOTE: these are static members
    void* operator new(size_t size, Allocator& alloc = Allocator()) //, const char* filename, int line, int adjust = 0)
    {
        const char* color = (size != sizeof(TYPE))? RED_MSG: YELLOW_MSG;
        ATOMIC_MSG(color << "ShmObj.new: size " << size << " vs. sizeof(" TOSTR(TYPE) << ") " << sizeof(TYPE) << ENDCOLOR);
        TYPE* ptr = alloc.allocate(size);
        ATOMIC_MSG(color << "ShmObj.new: ptr " << FMT("%p") << ptr << ", data ofs " << ((intptr_t)(TYPE*)this - (intptr_t)this) << ", alloc ofs " << ((intptr_t)&m_alloc - (intptr_t)this) << ENDCOLOR);
//        new ((TYPE*)ptr) TYPE() // const { new (ptr) type(ref); } //In-place copy construct
        alloc.construct(ptr, TYPE(alloc)); //call ctor
        new (ptr) (alloc); //set allocator ref
        return ptr;
    }
    void operator delete(void* ptr) noexcept
    {
        Allocator& alloc = ((shmobj*)ptr)->m_alloc.rebind<TYPE>();
        alloc.destroy(ptr);
//        static_cast<ShmObj>(ptr)->m_alloc.deallocate(ptr);
//        ((ShmObj*)ptr)->m_alloc.deallocate(ptr);
//        ATOMIC_MSG(YELLOW_MSG << "dealloc adrs " << FMT("%p") << ptr << " (ofs " << shmheap.shmofs(ptr) << "), deleted but space not reclaimed" << ENDCOLOR);
    }
private:
//protected:
    TYPE& m_data;
    std::unique_ptr<Allocator> m_alloc; //allow sharing and auto-cleanup
public:
    Allocator& alloc() { return *m_alloc; }
    TYPE* operator->() { return &m_data; }
};
#endif


#if 0
//shared memory proxy object:
//wraps a pointer to real data + mutex (different address space)
//passes allocator into object ctor, serializes member access (optional lock/unlock)
//also deallocates when object goes out of scope
//proxy wrapper example: http://www.stroustrup.com/wrapper.pdf
//see also: https://cppguru.wordpress.com/2009/01/14/c-proxy-template/
template <class TYPE, bool AUTOLOCK = true>
class shm_ptr //: public Mutexed<TYPE>
{
    typedef WithMutex<TYPE> ShmType;
    typedef ShmAllocator<ShmType> Allocator;
    union padded //kludge: union makes sizeof() proxy object same as real object (in case caller uses address arithmetic)
    {
//        WithMutex<TYPE>* ptr;
        ShmType* ptr; //ptr to obj in shmem
//        unique_ptr<Allocator> alloc; //allocator for obj; allocator could be shared with other shmem objs
//        WithAllocator<InnerType> shmobj;
//        WithAllocator<TYPE>* ptr;
//        std::unique_ptr<WithMutex<TYPE>> ptr; //unique_ptr will auto dealloc
//        WithAllocator<WithMutex<TYPE>>* ptr;
        TYPE dummy_padding; //NOTE: ctor not called
    public:
//        padded(ShmType* ptr, Allocator& alloc): ptr(ptr), alloc(&alloc) {} //kludge: need ctor to allow init and avoid "deleted function" error
        padded(ShmType* ptr): ptr(ptr) {} //kludge: need ctor to allow init and avoid "deleted function" error
        ~padded() {} //NOTE: this is needed
    } m_inner;
public: //ctor/dtor
//    ProxyWrapper(TYPE* pp): m_ptr(pp) {}
//#define INNER_CREATE(args)  m_inner(new ShmType(args)) //pass ctor args down into m_inner
    template<typename ... ARGS> //use "perfect forwarding" to ctor
//    shm_ptr(ARGS&& ... args, Allocator& alloc = Allocator()): INNER_CREATE(std::forward<ARGS>(args) ..., alloc) {}
//    PERFECT_FWD2BASE_CTOR(shm_ptr, INNER_CREATE) {} //perfectly forwarded ctor
    shm_ptr(ARGS&& ... args, Allocator& alloc = Allocator()): m_inner(new ShmType(std::forward<ARGS>(args) ..., alloc)) {}
//#undef INNER_CREATE
    ~shm_ptr() { delete m_inner.ptr; } //prevent memory leak
private:
//helper class to ensure unlock() occurs after member function returns
    class unlock_later
    {
        WithMutex<TYPE>* m_ptr;
    public: //ctor/dtor to wrap lock/unlock
        unlock_later(WithMutex<TYPE>* ptr): m_ptr(ptr) { if (AUTOLOCK) m_ptr->lock(); }
        ~unlock_later() { if (AUTOLOCK) m_ptr->unlock(); }
    public:
        inline WithMutex<TYPE>* operator->() { return m_ptr; } //allow access to wrapped members
    };
public: //pointer operator; allows safe multi-process access to shared object's members
//nope    TYPE* /*ProxyCaller*/ operator->() { typename WithMutex<TYPE>::scoped_lock lock(m_inner.ptr); return m_inner.ptr; } //ProxyCaller(m_ps.ptr); } //https://stackoverflow.com/questions/22874535/dependent-scope-need-typename-in-front
    inline unlock_later& operator->() { return unlock_later(m_inner.ptr); }
//public:
//    static void lock() { std::cout << "lock\n" << std::flush; }
//    static void unlock() { std::cout << "unlock\n" << std::flush; }
};
#endif


#if 0 //proxy example
//"perfect forwarding" (typesafe args) to ctor:
#define PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    type(ARGS&& ... args): base(std::forward<ARGS>(args) ...)

//mutex mixin class:
//used to attach mutex to another object type
#include <mutex>
#include <atomic>
#include <iostream>
template <class TYPE> //derivation
//class Mutexed //mixin
class WithMutex: public TYPE //derivation
{
public: //ctor/dtor
//    Mutexed(): m_locked(false) {} //mixin
    PERFECT_FWD2BASE_CTOR(WithMutex, TYPE), m_locked(false) {} //derivation
//    Mutexed(TYPE* ptr):
//    ~Mutexed() {}
    TYPE* operator->() { return this; } //allow access to parent members (auto-upcast only needed for derivation)
public: //helper class
//    class scoped_lock: public std::unique_lock<std::mutex>
    class scoped_lock
    {
//        std::mutex& m_mutex;
        WithMutex* m_parent; //NOTE: need this because inner class has no access to outer instance data
    public:
//        scoped_lock(std::mutex& mutex): m_mutex(mutex) { m_mutex.lock(); }
//        ~scoped_lock() { m_mutex.unlock(); }
        scoped_lock(WithMutex* parent): m_parent(parent) { m_parent->lock(); }
        ~scoped_lock() { m_parent->unlock(); }
    };
//#define scoped_lock()  scoped_lock slock(m_mutex)
//#define scoped_lock()  scoped_lock slock(this) //kludge: link inner class to outer instance data; NOTE: cpp not recursive here
private:
//protected:
    std::mutex m_mutex;
    std::atomic<bool> m_locked; //NOTE: mutex.try_lock() is not reliable (spurious failures); use explicit flag instead; see: http://en.cppreference.com/w/cpp/thread/mutex/try_lock
private:
    void lock() { std::cout << "lock\n" << std::flush; m_mutex.lock(); m_locked = true; }
    void unlock() { std::cout << "unlock\n" << std::flush; m_locked = false; m_mutex.unlock(); }
public:
    bool islocked() { return m_locked; } //if (m_mutex.try_lock()) { m_mutex.unlock(); return false; }
};


//shared memory proxy object:
//wraps a pointer to real data + mutex
//proxy wrapper example: http://www.stroustrup.com/wrapper.pdf
//see also: https://cppguru.wordpress.com/2009/01/14/c-proxy-template/
template <class TYPE>
class shm_ptr //: public Mutexed<TYPE>
{
    union padded //kludge: union makes sizeof() proxy object same as real object
    {
//        Mutexed<TYPE>* ptr;
        WithMutex<TYPE>* ptr;
        TYPE dummy_padding;
    public:
        padded(WithMutex<TYPE>* ptr): ptr(ptr) {} //kludge: need ctor to allow init and avoid "deleted function" error
        ~padded() {} //NOTE: this is needed
    } m_inner;
//    class ProxyCaller
//    {
//        TYPE* m_ptr;
//    public:
//        ProxyCaller(TYPE* ptr): m_ptr(ptr) {}
//        ~ProxyCaller() { unlock(); }
//        TYPE* operator->() { return m_ptr; }
//    };
//    class scoped
//    {
//    public:
//        scoped() { std::cout << "lock\n" << std::flush; }
//        ~scoped() { std::cout << "unlock\n" << std::flush; }
//    };
public:
//    ProxyWrapper(TYPE* pp): m_ptr(pp) {}
//    template<typename ... ARGS> //use "perfect forwarding" to ctor
//    shm_ptr(ARGS&& ... args): m_inner(new TYPE(std::forward<ARGS>(args) ...)) {}
#define INNER_CREATE(args)  m_inner(new WithMutex<TYPE>(args)) //pass ctor args down into m_inner
    PERFECT_FWD2BASE_CTOR(shm_ptr, INNER_CREATE) {} //perfectly forwarded ctor
#undef INNER_CREATE
    TYPE* /*ProxyCaller*/ operator->() { typename WithMutex<TYPE>::scoped_lock lock(m_inner.ptr); return m_inner.ptr; } //ProxyCaller(m_ps.ptr); } //https://stackoverflow.com/questions/22874535/dependent-scope-need-typename-in-front
public:
//    static void lock() { std::cout << "lock\n" << std::flush; }
//    static void unlock() { std::cout << "unlock\n" << std::flush; }
};

class TestObj
{
    std::string m_name;
    int m_count;
public:
    TestObj(const char* name): m_name(name), m_count(0) {}
//not needed    ~TestObj() {}
    void print() { std::cout << "TestObj(name '" << m_name << "', count " << m_count << ")\n" << std::flush; }
    int& inc() { return ++m_count; }
};

int main()
{
    TestObj bare("berry");
    bare.inc();
    bare.print();

//    ProxyWrapper<Person> person(new Person("testy"));
    shm_ptr<TestObj> testobj("testy");
    testobj->inc();
    testobj->inc();
    testobj->print();

    std::cout << "sizeof(berry) = " << sizeof(bare) << ", sizeof(test) = " << sizeof(testobj) << "\n" << std::flush;
    return 0;
}
#endif
#if 0 //proxy example
//proxy wrapper: http://www.stroustrup.com/wrapper.pdf
//https://cppguru.wordpress.com/2009/01/14/c-proxy-template/
#include <iostream>

class TestObj
{
    std::string m_name;
    int m_count;
public:
    TestObj(const char* name): m_name(name), m_count(0) {}
//not needed    ~TestObj() {}
    void print() { std::cout << "TestObj(name '" << m_name << "', count " << m_count << ")\n" << std::flush; }
    int& inc() { return ++m_count; }
};

template <class TYPE>
class shm_ptr
{
    union preserve_sizeof //kludge: make sizeof() proxy object same as real object
    {
        TYPE* ptr;
        TYPE data;
    public:
        preserve_sizeof(TYPE* ptr): ptr(ptr) {} //kludge: need ctor to allow init and avoid "deleted function" error
        ~preserve_sizeof() {}
    } m_ps;
    class ProxyCaller
    {
        TYPE* m_ptr;
    public:
        ProxyCaller(TYPE* ptr): m_ptr(ptr) {}
        ~ProxyCaller() { unlock(); }
        TYPE* operator->() { return m_ptr; }
    };
public:
//    ProxyWrapper(TYPE* pp): m_ptr(pp) {}
    template<typename ... ARGS> //use "perfect forwarding" to ctor
    shm_ptr(ARGS&& ... args): m_ps(new TYPE(std::forward<ARGS>(args) ...)) {}
    ProxyCaller operator->() { lock(); return ProxyCaller(m_ps.ptr); }
public:
    static void lock() { std::cout << "lock\n" << std::flush; }
    static void unlock() { std::cout << "unlock\n" << std::flush; }
};

int main()
{
    TestObj bare("berry");
    bare.inc();
    bare.print();

//    ProxyWrapper<Person> person(new Person("testy"));
    shm_ptr<TestObj> testobj("testy");
    testobj->inc();
    testobj->inc();
    testobj->print();

    std::cout << "sizeof(berry) = " << sizeof(bare) << ", sizeof(test) = " << sizeof(testobj) << "\n" << std::flush;
    return 0;
}
#endif
#if 0
//example usage:
#include <set>
struct Example
{
    int value;
    Example(int v): value(v) {}
    bool operator<(Example const& other) const { return value < other.value; }
};
int main(int argc, char* argv[])
{
    std::set<Example, std::less<Example>, ShmAllocator<Example, ShmHeap<Example> > > foo;
    foo.insert(Example(3));
    foo.insert(Example(1));
    foo.insert(Example(4));
    foo.insert(Example(2));
    return 0;
}
#endif
#if 0
//example from: http://en.cppreference.com/w/cpp/memory/allocator
#include <memory>
#include <iostream>
#include <string>
int main()
{
    std::allocator<int> a1;   // default allocator for ints
    int* a = a1.allocate(1);  // space for one int
    a1.construct(a, 7);       // construct the int
    std::cout << a[0] << '\n';
    a1.deallocate(a, 1);      // deallocate space for one int
 
    // default allocator for strings
    std::allocator<std::string> a2;
 
    // same, but obtained by rebinding from the type of a1
    decltype(a1)::rebind<std::string>::other a2_1;
 
    // same, but obtained by rebinding from the type of a1 via allocator_traits
    std::allocator_traits<decltype(a1)>::rebind_alloc<std::string> a2_2;
 
    std::string* s = a2.allocate(2); // space for 2 strings
 
    a2.construct(s, "foo");
    a2.construct(s + 1, "bar");
 
    std::cout << s[0] << ' ' << s[1] << '\n';
 
    a2.destroy(s);
    a2.destroy(s + 1);
    a2.deallocate(s, 2);
}
#endif


#if 0
//proxy wrapper: http://www.stroustrup.com/wrapper.pdf
//shared memory object mixin/wrapper:
//idea for ptr wrapper from: https://stackoverflow.com/questions/20945439/using-operator-new-and-operator-delete-with-a-custom-memory-pool-allocator
template<typename TYPE> //, long SHMKEY = 0, int EXTRA = 0> //, int SRCLINE>
class shmobj //: public TYPE
{
public: //data members
//NOTE: storage is split between local heap and shared memory
    TYPE& m_data; //shmem
    typedef ShmAllocator<TYPE> Allocator;
    Allocator& m_alloc; //local heap
public: //ctors/dtors
//    typedef struct { Allocator& alloc; TYPE data; } wrapped;
    template<typename ... ARGS> //use "perfect forwarding" on ctor
    shmobj(ARGS&& ... args, Allocator& alloc = Allocator()): m_data(*new TYPE(std::forward<ARGS>(args) ...)), m_alloc(alloc) {} //, srcline(SRCLINE) {}
public: //operators; NOTE: these are static members
    void* operator new(size_t size, Allocator& alloc = Allocator()) //, const char* filename, int line, int adjust = 0)
    {
        const char* color = (size != sizeof(TYPE))? RED_MSG: YELLOW_MSG;
        ATOMIC_MSG(color << "ShmObj.new: size " << size << " vs. sizeof(" TOSTR(TYPE) << ") " << sizeof(TYPE) << ENDCOLOR);
        TYPE* ptr = alloc.allocate(size);
        ATOMIC_MSG(color << "ShmObj.new: ptr " << FMT("%p") << ptr << ", data ofs " << ((intptr_t)(TYPE*)this - (intptr_t)this) << ", alloc ofs " << ((intptr_t)&m_alloc - (intptr_t)this) << ENDCOLOR);
//        new ((TYPE*)ptr) TYPE() // const { new (ptr) type(ref); } //In-place copy construct
        alloc.construct((TYPE*)ptr, TYPE(alloc)); //call ctor
        new (&ptr->m_alloc) (alloc); //set allocator ref
        return ptr;
    }
    void operator delete(void* ptr) noexcept
    {
        Allocator& alloc = ((shmobj*)ptr)->m_alloc.rebind<TYPE>();
        alloc.destroy(ptr);
//        static_cast<ShmObj>(ptr)->m_alloc.deallocate(ptr);
//        ((ShmObj*)ptr)->m_alloc.deallocate(ptr);
//        ATOMIC_MSG(YELLOW_MSG << "dealloc adrs " << FMT("%p") << ptr << " (ofs " << shmheap.shmofs(ptr) << "), deleted but space not reclaimed" << ENDCOLOR);
    }
//private: //data
//    typedef struct { ALLOC& alloc; TYPE data; } wrapper;
//    Allocator& m_alloc; //not stored in shmem
//public: //for debug only
//    Allocator& alloc() { return m_alloc; }
};
#endif


#if 0
#include <memory> //unique_ptr
template<typename TYPE> //, long SHMKEY = 0, int EXTRA = 0> //, int SRCLINE>
class ShmObj: public *unique_ptr<TYPE>
{
public: //ctors/dtors
    template<typename ... ARGS> //use "perfect forwarding" on ctor
    ShmObj(ARGS&& ... args): unique_ptr<TYPE>(new TYPE(std::forward<ARGS>(args) ...)) {} //, srcline(SRCLINE) {}
};

    std::forward<ARGS>(args) ...): unique_ptr<TYPE>(this), m_alloc(0) {} //, srcline(SRCLINE) {}
public: //operators; NOTE: these are static members
    typedef ShmAllocator<TYPE> Allocator;
    void* operator new(size_t size, Allocator& alloc = Allocator()) //, const char* filename, int line, int adjust = 0)
    {
//        if (adjust) line += adjust;
//        const char* key = shmheap.crekey(filename, line); //create unique key
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
//TODO?  alignof(void*); //http://www.boost.org/doc/libs/1_59_0/doc/html/align/vocabulary.html
//alignas(Foo) unsigned char memory[sizeof(Foo)];
//Foo* p = ::new((void*)memory) Foo();
        if (size != sizeof(ShmObj)) ATOMIC_MSG(YELLOW_MSG << "ShmObj.new: [WARNING] size " << size << " != sizeof(ShmObj<" TOSTR(TYPE) << ">) " << sizeof(ShmObj) << ENDCOLOR);
        ShmObj* ptr = alloc.allocate(size);
        alloc.construct((TYPE*)ptr, TYPE()); // const { new (ptr) type(ref); } //In-place copy construct
        ptr->m_alloc = &alloc;
//    ++Complex::nalloc;
//    if (Complex::nalloc == 1)
//        ATOMIC_MSG(YELLOW_MSG << "alloc size " << size << ", key " << strlen(key) << ":'" << key << "' at adrs " << FMT("%p") << ptr << ENDCOLOR);
//        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
        return (TYPE*)ptr;
    }
#endif


#if 0 //example proxy wrapper
//proxy wrapper: http://www.stroustrup.com/wrapper.pdf
//https://cppguru.wordpress.com/2009/01/14/c-proxy-template/
#include <iostream>

class Person
{
    std::string mName;
public:
    Person(std::string name): mName(name) {}
    void printName() { std::cout << mName << std::endl; }
};

template <class TYPE>
class ProxyWrapper
{
    TYPE* m_ptr;
    class ProxyCaller
    {
        TYPE* m_ptr;
    public:
        ProxyCaller(TYPE* pp): m_ptr(pp) {}
        ~ProxyCaller() { suffix(); }
        TYPE* operator->() { return m_ptr; }
    };
public:
//    ProxyWrapper(TYPE* pp): m_ptr(pp) {}
    template<typename ... ARGS> //use "perfect forwarding" on ctor
    ProxyWrapper(ARGS&& ... args): m_ptr(new TYPE(std::forward<ARGS>(args) ...)) {}
    ProxyCaller operator->() { prefix(); return ProxyCaller(m_ptr); }
public:
    static void prefix() { std::cout << "prefix\n"; }
    static void suffix() { std::cout << "suffix\n"; }
};

int main()
{
    Person bare("berry");
    bare.printName();

//    ProxyWrapper<Person> person(new Person("testy"));
    ProxyWrapper<Person> person("testy");
    person->printName();
//should print:
// prefix
// test
// suffix
    return 0;
}
#endif


#if 0
//custom std::allocator (~heap):
//compatible with stl containers
#include <cstddef> //ptrdiff_t
template<typename TYPE, long SHMKEY = 0, int EXTRA = 0> //, int SRCLINE>
class ShmAllocator//: public std::allocator<TYPE>
{
public: //helper types (required by std::allocator interface)
    typedef TYPE value_type;
    typedef TYPE* pointer;
    typedef const TYPE* const_pointer;
    typedef TYPE& reference;
    typedef const TYPE& const_reference;
    typedef std::size_t size_type;
//??    typedef off_t offset_type;
    typedef std::ptrdiff_t difference_type;
public: //ctor/dtor; NOTE: allocator should be stateless
//    mmap_allocator() throw(): std::allocator<T>(), filename(""), offset(0), access_mode(DEFAULT_STL_ALLOCATOR),	map_whole_file(false), allow_remap(false), bypass_file_pool(false), private_file(), keep_forever(false) {}
    ShmAllocator() throw()/*: std::allocator<TYPE>()*/ { fprintf(stderr, "Hello allocator!\n"); }
//    mmap_allocator(const std::allocator<T> &a) throw():	std::allocator<T>(a), filename(""),	offset(0), access_mode(DEFAULT_STL_ALLOCATOR), map_whole_file(false), allow_remap(false), bypass_file_pool(false), private_file(), keep_forever(false) {}
    ShmAllocator(const ShmAllocator& that) throw()/*: std::allocator<TYPE>(that)*/ {}
    template <class OTHER_TYPE, long OTHER_SHMKEY, int OTHER_EXTRA>
    ShmAllocator(const ShmAllocator/*<OTHER_TYPE, OTHER_SHMKEY, OTHER_EXTRA>*/& that) throw()/*: std::allocator<TYPE>(that)*/ {}
    ~ShmAllocator() throw() {}
public: //operators
    template <class OTHER_TYPE, long OTHER_SHMKEY, int OTHER_EXTRA>
    ShmAllocator/*<TYPE>*/& operator=(const ShmAllocator/*<OTHER_TYPE, OTHER_SHMKEY, OTHER_EXTRA>*/&) { return *this; } //TODO: is this needed?
public: //member functions
//rebind allocator to another type:
    template <class OTHER_TYPE, long OTHER_SHMKEY, int OTHER_EXTRA>
    struct rebind { typedef ShmAllocator/*<OTHER_TYPE, OTHER_SHMKEY, OTHER_EXTRA>*/ other; };
//get value address:
    pointer address(reference value) const { return &value; } //TODO: is this needed?
    const_pointer address(const_reference value) const { return &value; } //TODO: is this needed?
//max #elements that can be allocated:
//        size_type max_size() const { return size_t(-1); }
    size_type max_size() const throw() { return std::numeric_limits<std::size_t>::max() / sizeof(TYPE); } //TODO: is this needed?
//allocate but don't initialize:
    pointer allocate(size_type count, const void* hint = 0)
    {
//        void *the_pointer;
//        if (get_verbosity() > 0) fprintf(stderr, "Alloc %zd bytes.\n", num *sizeof(TYPE));
//        if (access_mode == DEFAULT_STL_ALLOCATOR) return std::allocator<TYPE>::allocate(num, hint);
//        if (num == 0) return NULL;
//        if (bypass_file_pool) the_pointer = private_file.open_and_mmap_file(filename, access_mode, offset, n*sizeof(T), map_whole_file, allow_remap);
//        else the_pointer = the_pool.mmap_file(filename, access_mode, offset, n*sizeof(T), map_whole_file, allow_remap);
//        if (the_pointer == NULL) throw(mmap_allocator_exception("Couldn't mmap file, mmap_file returned NULL"));
//        if (get_verbosity() > 0) fprintf(stderr, "pointer = %p\n", the_pointer);
//        return (pointer)the_pointer;
//print message and allocate memory with global new:
//        return std::allocator<TYPE>::allocate(n, hint);
//            TYPE* ptr = (TYPE*)malloc(count * sizeof(TYPE)); //TODO: replace
//            std::cout << RED_MSG << "custom allocated adrs " << FMT("%p") << ptr << ENDCOLOR;
        pointer ptr = (pointer)(::operator new(count * sizeof(TYPE)));
        ATOMIC_MSG(YELLOW_MSG << "ShmAllocator: allocated " << count << " element(s)"
                    << " of size " << sizeof(TYPE) << FMT(" at %p") << (long)ptr << ENDCOLOR);
        return ptr;
    }
//init elements of allocated storage ptr with value:
    void construct(pointer ptr, const TYPE& value) //TODO: is this needed?
    {
        new ((void*)ptr) TYPE(value); //init memory with placement new
    }
//destroy elements of initialized storage ptr:
    void destroy (pointer ptr) //TODO: is this needed?
    {
        ptr->~TYPE(); //call dtor
    }
//deallocate storage ptr of deleted elements:
    void deallocate (pointer ptr, size_type count)
    {
        if (!ptr) return;
//        fprintf(stderr, "Dealloc %d bytes (%p).\n", num * sizeof(TYPE), ptr);
//        if (get_verbosity() > 0) fprintf(stderr, "Dealloc %zd bytes (%p).\n", num *sizeof(TYPE), ptr);
//        if (access_mode == DEFAULT_STL_ALLOCATOR) return std::allocator<T>::deallocate(ptr, num);
//        if (num == 0) return;
//        if (bypass_file_pool) private_file.munmap_and_close_file();
//        else if (!keep_forever) the_pool.munmap_file(filename, access_mode, offset, num *sizeof(TYPE));
//print message and deallocate memory with global delete:
//            std::cout << RED_MSG << "custom deallocated adrs " << FMT("%p") << ptr << ENDCOLOR;
        std::cout << "deallocate " << count << " element(s)"
                    << " of size " << sizeof(TYPE)
                    << " at: " << (void*)ptr << std::endl;
//        return std::allocator<TYPE>::deallocate(p, n);
//            free(ptr); //TODO: replace
        ::operator delete((void*)ptr);
    }
//private:
//		friend class mmappable_vector<TYPE, mmap_allocator<TYPE> >;
public: //helpers
    static key_t crekey() { return (rand() << 16) | 0xfeed; }
};
#endif
#if 0
//allocator for stl objects:
//based on example at https://stackoverflow.com/questions/826569/compelling-examples-of-custom-c-allocators
//more info at: http://www.cplusplus.com/forum/general/130920/
//example at: http://www.josuttis.com/cppcode/myalloc.hpp.html
//more info at: https://www.codeproject.com/Articles/4795/C-Standard-Allocator-An-Introduction-and-Implement
//namespace mmap_allocator_namespace
template <typename TYPE>
class ShmAllocator//: public std::allocator<TYPE>
{
public: //type defs
    typedef TYPE value_type;
    typedef TYPE* pointer;
    typedef const TYPE* const_pointer;
    typedef TYPE& reference;
    typedef const TYPE& const_reference;
    typedef std::size_t size_type;
//??    typedef off_t offset_type;
    typedef std::ptrdiff_t difference_type;
public: //ctor/dtor; nops - allocator has no state
//    mmap_allocator() throw(): std::allocator<T>(), filename(""), offset(0), access_mode(DEFAULT_STL_ALLOCATOR),	map_whole_file(false), allow_remap(false), bypass_file_pool(false), private_file(), keep_forever(false) {}
    ShmAllocator() throw()/*: std::allocator<TYPE>()*/ { fprintf(stderr, "Hello allocator!\n"); }
//    mmap_allocator(const std::allocator<T> &a) throw():	std::allocator<T>(a), filename(""),	offset(0), access_mode(DEFAULT_STL_ALLOCATOR), map_whole_file(false), allow_remap(false), bypass_file_pool(false), private_file(), keep_forever(false) {}
    ShmAllocator(const ShmAllocator& that) throw()/*: std::allocator<TYPE>(that)*/ {}
    template <class OTHER>
    ShmAllocator(const ShmAllocator<OTHER>& that) throw()/*: std::allocator<TYPE>(that)*/ {}
    ~ShmAllocator() throw() {}
public: //methods
//rebind allocator to another type:
    template <class OTHER>
    struct rebind { typedef ShmAllocator<OTHER> other; };
//get value address:
    pointer address (reference value) const { return &value; }
    const_pointer address (const_reference value) const { return &value; }
//max #elements that can be allocated:
    size_type max_size() const throw() { return std::numeric_limits<std::size_t>::max() / sizeof(TYPE); }
//allocate but don't initialize:
    pointer allocate(size_type num, const void* hint = 0)
    {
//        void *the_pointer;
//        if (get_verbosity() > 0) fprintf(stderr, "Alloc %zd bytes.\n", num *sizeof(TYPE));
//        if (access_mode == DEFAULT_STL_ALLOCATOR) return std::allocator<TYPE>::allocate(num, hint);
//        if (num == 0) return NULL;
//        if (bypass_file_pool) the_pointer = private_file.open_and_mmap_file(filename, access_mode, offset, n*sizeof(T), map_whole_file, allow_remap);
//        else the_pointer = the_pool.mmap_file(filename, access_mode, offset, n*sizeof(T), map_whole_file, allow_remap);
//        if (the_pointer == NULL) throw(mmap_allocator_exception("Couldn't mmap file, mmap_file returned NULL"));
//        if (get_verbosity() > 0) fprintf(stderr, "pointer = %p\n", the_pointer);
//        return (pointer)the_pointer;
//print message and allocate memory with global new:
//        return std::allocator<TYPE>::allocate(n, hint);
        pointer ptr = (pointer)(::operator new(num * sizeof(TYPE)));
        ATOMIC_MSG(YELLOW_MSG << "ShmAllocator: allocated " << num << " element(s)"
                    << " of size " << sizeof(TYPE) << FMT(" at %p") << (long)ptr << ENDCOLOR);
        return ptr;
    }
//init elements of allocated storage ptr with value:
    void construct (pointer ptr, const TYPE& value)
    {
        new ((void*)ptr) TYPE(value); //init memory with placement new
    }
//destroy elements of initialized storage ptr:
    void destroy (pointer ptr)
    {
        ptr->~TYPE(); //call dtor
    }
//deallocate storage ptr of deleted elements:
    void deallocate (pointer ptr, size_type num)
    {
//        fprintf(stderr, "Dealloc %d bytes (%p).\n", num * sizeof(TYPE), ptr);
//        if (get_verbosity() > 0) fprintf(stderr, "Dealloc %zd bytes (%p).\n", num *sizeof(TYPE), ptr);
//        if (access_mode == DEFAULT_STL_ALLOCATOR) return std::allocator<T>::deallocate(ptr, num);
//        if (num == 0) return;
//        if (bypass_file_pool) private_file.munmap_and_close_file();
//        else if (!keep_forever) the_pool.munmap_file(filename, access_mode, offset, num *sizeof(TYPE));
//print message and deallocate memory with global delete:
        std::cerr << "deallocate " << num << " element(s)"
                    << " of size " << sizeof(TYPE)
                    << " at: " << (void*)ptr << std::endl;
//        return std::allocator<TYPE>::deallocate(p, n);
        ::operator delete((void*)ptr);
    }
//private:
//		friend class mmappable_vector<TYPE, mmap_allocator<TYPE> >;
};
//all specializations of this allocator are interchangeable:
template <class T1, class T2>
bool operator== (const ShmAllocator<T1>&, const ShmAllocator<T2>&) throw() { return true; }
template <class T1, class T2>
bool operator!= (const ShmAllocator<T1>&, const ShmAllocator<T2>&) throw() { return false; }
#endif
#ifdef OLD_ONE
//shared memory heap:
//NOTE: very simplistic, not suitable for intense usage
class ShmHeap: public ShmSeg
{
#define dump(...)  dump(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#define gc(...)  gc(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
public: //ctor/dtor
    explicit ShmHeap(size_t size, persist cre = persist::NewTemp, key_t key = 0): ShmSeg(/*hdrlen() +*/ size, cre, key) //, scoped_lock::m_mutex(mutex())
    {
        if (cre == persist::Reuse) return; //no need to init shm seg
//these are redundant (smh init to 0 anyway):
//                m_ptr->used = 0;
//                m_ptr->symtab[0].nextofs = 0;
//                m_ptr->symtab[0].name[0] = '\0';
//this one definitely needed:
//                m_ptr->used = (long)&m_ptr->symtab[0] - (long)m_ptr;
//first entry is mutex instead of named storage:
//                m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
        scoped_lock lock(*this);
        new (&mutex()) std::mutex(); //make sure mutex is inited; placement new: https://stackoverflow.com/questions/2494471/c-is-it-possible-to-call-a-constructor-directly-without-new
//                m_shmptr[1].nextofs = 0; //eof marker
        gc(true);
        ATOMIC_MSG(BLUE_MSG << "sizeof(mutex) = " << sizeof(std::mutex) 
            << ", sizeof(cond_var) = " << sizeof(std::condition_variable)
            << ", sizeof(size_t) = " << sizeof(size_t)
            << ", sizeof(void*) = " << sizeof(void*)
            << ", sizeof(uintptr_t) = " << sizeof(uintptr_t)
            << ", sizeof(entry) = " << sizeof(entry)
            << ", hdr len (overhead) = " << hdrlen()
            << /*", x/80xw (addr) to examine memory" <<*/ ENDCOLOR);
//        init_params.init = true;
//            std::atexit(exiting); //this happens automatically
//            void exiting() { std::cout << "Exiting"; }
//        if (cre) m_ptr->used += sizeof(m_ptr->used);
//std::cout << "here1\n" << std::flush;
//            const char* d = desc().c_str();
//std::cout << "here2\n" << std::flush;
//            std::cout << "here2 desc = " << desc() << "\n" << std::flush;
//            printf("desc = %d:\n", strlen(d));
//            printf("desc = %d:%s\n", strlen(d), d);
//std::cout << desc() << " here3\n" << std::flush;
        dump("initial heap:");
        ATOMIC_MSG(CYAN_MSG << "ShmHeap: attached " << desc() << ENDCOLOR);
    }
    ~ShmHeap() {} // (&mutex())->~std::mutex(); //not needed?
private: //data
//symbol table entry:
    typedef struct
    {
        size_t nextofs; //NOTE: only needs to be 16 bits, but compiler will align on 8-byte boundary
        union
        {
            char name[1]; //named storage entry (name + storage space)
            std::mutex mutex; //thread/proc sync (first entry only)
        };
    } entry;
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
    inline entry* symtab(size_t ofs) const { return (entry*)(shmptr() + ofs); } //relative ofs -> ptr
    inline size_t hdrlen() const { return shmofs(symtab(0)[1].name); }
//        inline size32_t symofs(entry* symptr) const { return (long)symptr - (long)m_symtab; }
//        typedef struct { size_t used; std::mutex mutex; entry symtab[1]; } hdr;
//    inline entry* symtab() { return &m_ptr->symtab[0]; }
//        hdr* m_ptr; //ptr to start of shm
//        struct { size32_t nextofs; std::mutex mutex; entry symtab[1]; }* m_ptr; //shm struct ptr
//        struct { std::mutex mutex; entry symtab[1]; }* m_shmptr; //shm struct
//    entry* m_shmptr; //symbol table in shm
//        struct { size_t nextofs; std::mutex mutex; }* m_ptr; //shm struct ptr
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
//    bool m_persist;
public: //getters
    inline std::mutex& mutex() { return symtab(0)->mutex; }
//    inline size32_t shmofs(VOID* ptr) const { return ptr - shmptr(); } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
public: //allocator methods
//generate unique key from src location:
//same src location will generate same key, even across processes
    static const char* crekey(/*std::string& key,*/ const char* filename, int line)
    {
        static std::string srckey; //NOTE: must persist after return; not thread safe (must be used immediately)
        const char* bp = strrchr(filename, '/');
        if (!bp) bp = filename;
        srckey = bp;
        srckey += ':';
        char buf[10];
        sprintf(buf, "%d", line);
        srckey += buf; //line;
        return srckey.c_str();
    }
//protect critical section: used to synchronize (serialize) threads/processes
    class scoped_lock: public std::unique_lock<std::mutex>
    {
    public:
        explicit scoped_lock(ShmHeap& shmheap): std::unique_lock<std::mutex>(shmheap.mutex()) {};
//        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
//    private:
//        static std::mutex& m_mutex;
    };
//allocate named storage:
//creates new shm object if needed, else shares existing object
//NOTE: assumes infrequent allocation (traverses linked list of entries)
    void* alloc(const char* key, size_t size, size_t extra = 0, bool want_throw = false, void (*ctor)(void* newptr) = 0, int alignment = sizeof(uintptr_t)) //NO-assume 32-bit alignment, allow override; https://en.wikipedia.org/wiki/LP64
    {
//        defered_init(); //kludge: defer init until needed (avoids problems with static init)
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
        int keylen = rdup(strlen(key) + 1, alignment); //alignment for var storage
        size = rdup(keylen + size, sizeof(symtab(0)->nextofs)); //alignment for following nextofs
        extra = rdup(extra, sizeof(symtab(0)->nextofs)); //alignment for following nextofs
//        std::unique_lock<std::mutex> lock(mutex()); //m_shmptr->mutex);
        scoped_lock lock(*this);
//first check if symbol already exists:
//            entry* symptr = &m_ptr->symtab[0];
//            entry* parent = (long)&m_ptr->nextofs - (long)m_ptr; //symofs = m_ptr->nextofs;
//            entry* parent = symtab(0);
        dump("before alloc:");
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
        entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs);
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
            if (!symptr->nextofs) //eof
            {
//            size = sizeof(symptr->nextofs) + keylen + rdup(size, alignment);
//            m_ptr->used = rdup(m_ptr->used, sizeof(symptr->nextofs));
//            if (available() < size) return 0; //not enough space
//                    long neweof = (long)symptr->name + keylen + rdup(size, alignment);
//alloc new storage:
                entry* nextptr = (entry*)(symptr->name + size + extra); //alloc space for this var
                long remaining = (uintptr_t)shmptr() + shmsize() - (uintptr_t)nextptr->name; //NOTE: must be signed
//                ATOMIC_MSG(FMT("ptr %p") << shmptr() << FMT(" + size %zu") << shmsize() << FMT(" - name* %p") << (uintptr_t)nextptr->name << " = remaining " << remaining << ENDCOLOR);
                const char* color = (remaining < 0)? RED_MSG: GREEN_MSG;
                ATOMIC_MSG(color << "alloc key '" << key << "', size " << size << "+" << extra << " extra +" << (shmofs(nextptr->name) - shmofs(nextptr)) << " ovhr at ofs " << shmofs(symptr) << ", remaining " << remaining << ENDCOLOR);
                if (remaining < 0) //not enough space
                    if (want_throw) throw std::runtime_error(RED_MSG "ShmHeap: alloc failed (insufficient space)" ENDCOLOR);
                    else return 0;
                symptr->nextofs = shmofs(nextptr); //neweof - (long)m_ptr;
                strcpy(symptr->name, key);
                memset(symptr->name + keylen, 0, size + extra - keylen); //clear storage for new var
                nextptr->nextofs = 0; //new eof marker (in case space was reclaimed earlier)
                ATOMIC_MSG(RED_MSG << "TODO: alloc: tell object's allocator about " << extra << " extra bytes" << ENDCOLOR);
                dump("after alloc:");
                if (ctor) (*ctor)(symptr->name + keylen); //initialize new object
                return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
//                    break;
            }
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
//                size_t ofs = (long)symptr - (long)m_ptr; //(void*)symptr - (void*)m_ptr;
            ATOMIC_MSG(BLUE_MSG << "check for key '" << key << "' ==? symbol '" << symptr->name << "' at ofs " << shmofs(symptr) << ENDCOLOR);
            if (!strcmp(symptr->name, key)) return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen); //break; //found
//                parent = symptr;
//                symptr = symtab(symptr->nextofs);
            parent = symptr;
//                symptr = (long)&m_ptr->symtab[0] + symptr->nextofs;
        }
//            return (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
    }
    void dealloc(void* ptr)
    {
//        std::unique_lock<std::mutex> lock(mutex()); //m_shmptr->mutex);
        scoped_lock lock(*this);
//            entry* symptr = (void*)m_ptr + m_ptr->nextofs;
//            size_t symofs = (long)&m_ptr->symtab[0] - (long)m_ptr;
        dump("before dealloc(0x%zx):", shmofs(ptr));
        entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
            if (!symptr->nextofs) break; //eof
            int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
            uintptr_t varadrs = (uintptr_t)(symptr->name + keylen); //static_cast<void*>((long)symptr->name + keylen);
            ATOMIC_MSG(BLUE_MSG << "check '" << symptr->name << FMT("' ofs 0x%lx") << shmofs(symptr) << " adrs " << FMT("%p") << varadrs << ".." << FMT("0x%lx") << symptr->nextofs << " ~= dealloc adrs " << FMT("%p") << ptr << ENDCOLOR);
            if ((varadrs <= (uintptr_t)ptr) && ((uintptr_t)ptr < (uintptr_t)symtab(symptr->nextofs))) //adrs + rdup(keylen, 8))
            {
//                    symptr->nextofs = symtab(symptr->nextofs)->nextofs; //remove from chain // = (long)&m_ptr->symtab[0] + symptr->nextofs;
                parent->nextofs = symptr->nextofs; //remove from chain
//                    if (!symtab(m_ptr->nextofs)->name[0]) gc(true); //all vars deallocated
//                    if (m_shmptr->next)
//                    if (!symtab(symptr->nextofs)->nextofs &&
//                    if (!symtab(0)->nextofs)
//                    if ((parent == m_shmptr) && !parent->nextofs m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
                if ((parent == symtab(0)) && !symtab(symptr->nextofs)->nextofs) gc(false); //garbage collect
//                    if (!symtab(m_shmptr->nextofs)->nextofs) m_shmptr->nextofs = shmofs(&m_shmptr[1]); //gc
                dump("after dealloc(0x%zx):", shmofs(ptr));
                return;
            }
//                symptr = symtab(symptr->nextofs);
            parent = symptr;
        }
        throw std::runtime_error("ShmHeap: attempt to dealloc unknown address");
    }
private: //helpers; NOTE: mutex must be owned before using these
#undef dump
#undef gc
//garbage collector:
    void gc(int line, bool init) //kludge: need extra param here for __VA_ARGS__
    {
        ATOMIC_MSG(YELLOW_MSG << "gc(" << init << ")" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//        if (init)
//        {
//            m_ptr[0].nextofs = sizeof(m_ptr[0]); //symofs(m_ptr->symtab); //[0] - (long)m_ptr;
//            m_ptr->symtab[0].name[0] = '\0'; //eof marker
//            m_shmptr->symtab[0].nextofs = 0; //eof marker
        entry* entries = symtab(0);
//        entry* e0 = &entries[0]; entry* e1 = &entries[1];
//        ATOMIC_MSG(FMT("shmptr = %p") << shmptr()
//            << FMT(", &ent[0] = %p") << e0
//            << FMT(" ofs 0x%lx") << shmofs(&entries[0])
//            << FMT(", &ent[1] = %p") << e1
//            << FMT(" ofs 0x%lx") << shmofs(&entries[1])
//            << ENDCOLOR);
        entries[0].nextofs = shmofs(&entries[1]);
        entries[1].nextofs = 0; //eof marker
//        }
    }
//show shm details (for debug):
    const std::string /*NO &*/ desc() const
//        const char* desc() const
    {
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //symtab(m_ptr->nextofs);
        entry* parent = symtab(0); //m_shmptr;
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
            parent = symptr;
            if (!symptr->nextofs) break; //eof
//                symptr = symtab(symptr->nextofs);
        }
        size_t used = shmofs(parent->name); //symptr);
//            static std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
//            ostrm.str(""); ostrm.clear(); //https://stackoverflow.com/questions/5288036/how-to-clear-ostringstream
        std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
        ostrm << shmsize() << " bytes"
            << " for shm key " << FMT("0x%lx") << shmkey()
            << " at " << FMT("%p") << shmptr()
            << ", used = " << used << ", avail " << (shmsize() - used);
//            std::cout << ostrm.str().c_str() << "\n" << std::flush;
        return ostrm.str(); //.c_str();
    }
    void dump(int line, const char* desc, size_t val = 0)
    {
        ATOMIC_MSG(BLUE_MSG << "shm " << FMT(desc) << val << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//            entry* symptr = symtab(0); //symtab(m_ptr->nextofs);
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
        entry* parent = symtab(0); //m_shmptr;
//        ATOMIC_MSG(BLUE_MSG << "parent ofs " << FMT("0x%x") << shmofs(parent) << FMT(", 0x%x") << shmofs(&parent[1]) << ENDCOLOR);
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
            if (!symptr->nextofs) //eof
            {
                size_t used = shmofs(symptr->name), remaining = shmsize() - used;
                ATOMIC_MSG(BLUE_MSG << FMT("[0x%lx]") << shmofs(symptr) << " eof, " << used << " used (" << remaining << " remaining)" << ENDCOLOR);
                return;
            }
            int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
            ATOMIC_MSG(BLUE_MSG << FMT("[0x%lx]") << shmofs(symptr) << " next ofs " << FMT("0x%lx") << symptr->nextofs << ", name " << strlen(symptr->name) << ":'" << symptr->name << "', adrs " << FMT("0x%lx") << shmofs(symptr->name + keylen) << "..+8" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//                symptr = symtab(symptr->nextofs);
            parent = symptr;
        }
    }
//    static ShmHeap shmheap; //use static member to reduce clutter in var instances
//    std::mutex sm;
//no worky    std::mutex& scoped_lock::m_mutex = sm; //mutex();
};
//std::mutex& ShmHeap::scoped_lock::m_mutex = mutex();


//mixin class for custom allocator:
class ShmHeapAlloc
{
public:
    void* operator new(size_t size, const char* filename, int line, int adjust = 0)
    {
//        std::string key;
        if (adjust) line += adjust;
        const char* key = shmheap.crekey(filename, line); //create unique key
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
//TODO?  alignof(void*); //http://www.boost.org/doc/libs/1_59_0/doc/html/align/vocabulary.html
//alignas(Foo) unsigned char memory[sizeof(Foo)];
//Foo* p = ::new((void*)memory) Foo();
        void* ptr = shmheap.alloc(key, size);
//    ++Complex::nalloc;
//    if (Complex::nalloc == 1)
        ATOMIC_MSG(YELLOW_MSG << "alloc size " << size << ", key " << strlen(key) << ":'" << key << "' at adrs " << FMT("%p") << ptr << ENDCOLOR);
        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
        return ptr;
    }
    void operator delete(void* ptr) noexcept
    {
        shmheap.dealloc(ptr);
        ATOMIC_MSG(YELLOW_MSG << "dealloc adrs " << FMT("%p") << ptr << " (ofs " << shmheap.shmofs(ptr) << "), deleted but space not reclaimed" << ENDCOLOR);
    }
//private:
public:
    static ShmHeap shmheap;
};

//tag allocated var with src location:
//NOTE: cpp avoids recursion so macro names can match actual function names here
//#define new  new(__FILE__, __LINE__)
//#define NEW  new(__FILE__, __LINE__)
#define new_SHM(...)  new(__FILE__, __LINE__, __VA_ARGS__)

//allow caller to calculate size of shm needed:
#define SHMVAR_LEN(thing)  (sizeof(thing) + 16 + 8)
#define SHMHDR_LEN  56


//#ifdef SHM_KEY
//ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, SHM_KEY); //0x4567feed);
//#endif
#endif







#ifdef OLD_ONE2
//mixin class for custom allocator:
class ShmHeapAlloc
{
public:
    void* operator new(size_t size, const char* filename, int line, int adjust)
    {
        std::string key;
        if (adjust) line += adjust;
        shmheap.crekey(key, filename, line); //create unique key
        void* ptr = shmheap.alloc(key.c_str(), size);
//    ++Complex::nalloc;
//    if (Complex::nalloc == 1)
        ATOMIC_MSG(YELLOW_MSG << "alloc size " << size << ", key " << key.length() << ":'" << key << "' at adrs " << FMT("0x%x") << ptr << ENDCOLOR);
        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
        return ptr;
    }
    void operator delete(void* ptr) noexcept
    {
        shmheap.dealloc(ptr);
        ATOMIC_MSG(YELLOW_MSG << "dealloc adrs " << FMT("0x%x") << ptr << " (ofs " << shmheap.shmofs(ptr) << "), deleted but space not reclaimed" << ENDCOLOR);
    }
//private: //configurable shm seg
    class ShmHeap
    {
#define dump(...)  dump(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#define gc(...)  gc(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
    public: //ctor/dtor
        enum class persist: int {PreExist = 0, NewTemp = +1, NewPerm = -1};
        explicit ShmHeap(int key): ShmHeap(key, 0, persist::PreExist) {} //child processes
        /*explicit*/ ShmHeap(int key, size32_t size, persist cre): m_key(0), m_size(0), m_shmptr(0), m_persist(false) //parent process
        {
            init_params = {false, key, size, cre}; //kludge: save info and init later (to avoid segv during static init)
        }
    private:
        struct { bool init; int key; size32_t size; persist cre; } init_params;
        void defered_init() //kludge: delay init until needed (else segv during static init)
        {
            if (init_params.init) return; //already inited
//kludge: restore ctor params:
            int key = init_params.key;
            size32_t size = init_params.size;
            persist cre = init_params.cre;
            ATOMIC_MSG(BLUE_MSG << "defered_init(key " << FMT("0x%x") << key << ", size " << size << ", cre " << (int)cre << ")" << ENDCOLOR);
//        ATOMIC_MSG(CYAN_MSG << "hello" << ENDCOLOR);
//        if ((key = ftok("hello.txt", 'R')) == -1) /*Here the file must exist */ 
//        int fd = shm_open(filepath, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
//        if (fd == -1) throw "shm_open failed";
//        if (ftruncate(fd, sizeof(struct region)) == -1)    /* Handle error */;
//        rptr = mmap(NULL, sizeof(struct region), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//        if (rptr == MAP_FAILED) /* Handle error */;
//create shm seg:
//        if (!key) throw "ShmHeap: bad key";
            if (cre != persist::PreExist) destroy(key); //delete first (parent wants clean start)
            if (!key) key = (rand() << 16) | 0xfeed;
            if ((size < 1) || (size >= 10000000)) throw std::runtime_error("ShmHeap: bad size"); //set reasonable limits
//NOTE: size will be rounded up to a multiple of PAGE_SIZE
            int shmid = shmget(key, size, 0666 | ((cre != persist::PreExist)? IPC_CREAT | IPC_EXCL: 0)); // | SHM_NORESERVE); //NOTE: clears to 0 upon creation
            ATOMIC_MSG(BLUE_MSG << "shmget key " << FMT("0x%x") << key << ", size " << size << " => " << shmid << ENDCOLOR);
            if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
            void* ptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
            ATOMIC_MSG(BLUE_MSG << "shmat id " << FMT("0x%x") << shmid << " => " << FMT("0x%lx") << (long)ptr << ENDCOLOR);
            if (ptr == (void*)-1) throw std::runtime_error(std::string(strerror(errno)));
            m_key = key;
            m_size = size;
            m_shmptr = static_cast<decltype(m_shmptr)>(ptr);
            m_persist = (cre != persist::NewTemp);
            if (cre != persist::PreExist) //init shm seg
            {
//these are redundant (smh init to 0 anyway):
//                m_ptr->used = 0;
//                m_ptr->symtab[0].nextofs = 0;
//                m_ptr->symtab[0].name[0] = '\0';
//this one definitely needed:
//                m_ptr->used = (long)&m_ptr->symtab[0] - (long)m_ptr;
//first entry is mutex instead of named storage:
//                m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
                new (&m_shmptr[0].mutex) std::mutex(); //make sure mutex is inited; placement new: https://stackoverflow.com/questions/2494471/c-is-it-possible-to-call-a-constructor-directly-without-new
//                m_shmptr[1].nextofs = 0; //eof marker
                gc(true);
                ATOMIC_MSG(BLUE_MSG << "sizeof(mutex) = " << sizeof(std::mutex) << ", sizeof(size_t) = " << sizeof(size_t) << ", x/80xw (addr) to examine memory" << ENDCOLOR);
            }
            init_params.init = true;
//            std::atexit(exiting); //this happens automatically
//            void exiting() { std::cout << "Exiting"; }
//        if (cre) m_ptr->used += sizeof(m_ptr->used);
//std::cout << "here1\n" << std::flush;
//            const char* d = desc().c_str();
//std::cout << "here2\n" << std::flush;
//            std::cout << "here2 desc = " << desc() << "\n" << std::flush;
//            printf("desc = %d:\n", strlen(d));
//            printf("desc = %d:%s\n", strlen(d), d);
//std::cout << desc() << " here3\n" << std::flush;
            ATOMIC_MSG(CYAN_MSG << "ShmHeap: attached " << desc() << ENDCOLOR);
        }
    public:
        ~ShmHeap() { detach(); if (!m_persist) destroy(m_key); }
    public: //getters
        inline int key() const { return m_key; }
        inline size32_t size() const { return m_size; }
//        inline size_t available() const { return size() - used(); }
//        inline size_t used() const { return m_ptr->used; } // + sizeof(m_ptr->used); } //account for used space at front
    public: //cleanup
        void detach()
        {
            if (!m_shmptr) return; //not attached
//  int shmctl(int shmid, int cmd, struct shmid_ds *buf);
            if (shmdt(m_shmptr) == -1) throw std::runtime_error(strerror(errno));
            m_shmptr = 0; //can't use m_shmptr after this point
            ATOMIC_MSG(CYAN_MSG << "ShmHeap: dettached " << ENDCOLOR); //desc();
        }
        static void destroy(int key)
        {
            if (!key) return; //!owner or !exist
            int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
            ATOMIC_MSG(CYAN_MSG << "ShmHeap: destroy " << FMT("0x%x") << key << " => " << FMT("0x%x") << shmid << ENDCOLOR);
            if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
            if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
            throw std::runtime_error(strerror(errno));
        }
    public: //allocator
//generates a unique key from a src location:
//same src location will generate same key, even across processes
        static void crekey(std::string& key, const char* filename, int line)
        {
            const char* bp = strrchr(filename, '/');
            if (!bp) bp = filename;
            key = bp;
            key += ':';
            char buf[10];
            sprintf(buf, "%d", line);
            key += buf; //line;
        }
//allocate named storage:
//NOTE: assumes infrequent allocation (traverses linked list of entries)
        void* alloc(const char* key, int size, int alignment = sizeof(uint32_t)) //assume 32-bit alignment, allow override; https://en.wikipedia.org/wiki/LP64
        {
            defered_init(); //kludge: defer init until needed (avoids problems with static init)
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
            int keylen = rdup(strlen(key) + 1, alignment); //alignment for var storage
            size = rdup(keylen + size, sizeof(m_shmptr->nextofs)); //alignment for following nextofs
//            std::unique_lock<std::mutex> lock(m_shmptr->mutex);
            scoped_lock lock;
//first check if symbol already exists:
//            entry* symptr = &m_ptr->symtab[0];
//            entry* parent = (long)&m_ptr->nextofs - (long)m_ptr; //symofs = m_ptr->nextofs;
//            entry* parent = symtab(0);
            dump("before alloc:");
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
            entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs);
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
                if (!symptr->nextofs) //eof
                {
//            size = sizeof(symptr->nextofs) + keylen + rdup(size, alignment);
//            m_ptr->used = rdup(m_ptr->used, sizeof(symptr->nextofs));
//            if (available() < size) return 0; //not enough space
//                    long neweof = (long)symptr->name + keylen + rdup(size, alignment);
//alloc new storage:
                    entry* nextptr = (entry*)&symptr->name[size]; //alloc space for this var
                    size32_t remaining = (long)m_shmptr + m_size - (long)nextptr->name;
                    ATOMIC_MSG(GREEN_MSG << "alloc key '" << key << "', size " << size << " at ofs " << shmofs(symptr) << ", remaining " << remaining << ENDCOLOR);
                    if (remaining < 0) return 0; //not enough space
                    symptr->nextofs = shmofs(nextptr); //neweof - (long)m_ptr;
                    strcpy(symptr->name, key);
                    memset(symptr->name + keylen, 0, size - keylen); //clear storage for new var
                    nextptr->nextofs = 0; //new eof marker (in case space was reclaimed earlier)
                    dump("after alloc:");
                    return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
//                    break;
                }
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
//                size_t ofs = (long)symptr - (long)m_ptr; //(void*)symptr - (void*)m_ptr;
                ATOMIC_MSG(BLUE_MSG << "check for key '" << key << "' ==? symbol '" << symptr->name << "' at ofs " << shmofs(symptr) << ENDCOLOR);
                if (!strcmp(symptr->name, key)) return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen); //break; //found
//                parent = symptr;
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
//                symptr = (long)&m_ptr->symtab[0] + symptr->nextofs;
            }
//            return (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
        }
        void dealloc(void* ptr)
        {
//            std::unique_lock<std::mutex> lock(m_shmptr->mutex);
            scoped_lock lock;
//            entry* symptr = (void*)m_ptr + m_ptr->nextofs;
//            size_t symofs = (long)&m_ptr->symtab[0] - (long)m_ptr;
            dump("before dealloc(0x%x):", shmofs(ptr));
            entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
                if (!symptr->nextofs) break; //eof
                int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
                void* varadrs = (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
                ATOMIC_MSG(BLUE_MSG << "check '" << symptr->name << FMT("' ofs 0x%x") << shmofs(symptr) << " adrs " << FMT("0x%x") << varadrs << ".." << FMT("0x%x") << symptr->nextofs << " ~= dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR);
                if ((varadrs <= ptr) && (ptr < (void*)symtab(symptr->nextofs))) //adrs + rdup(keylen, 8))
                {
//                    symptr->nextofs = symtab(symptr->nextofs)->nextofs; //remove from chain // = (long)&m_ptr->symtab[0] + symptr->nextofs;
                    parent->nextofs = symptr->nextofs; //remove from chain
//                    if (!symtab(m_ptr->nextofs)->name[0]) gc(true); //all vars deallocated
//                    if (m_shmptr->next)
//                    if (!symtab(symptr->nextofs)->nextofs &&
//                    if (!symtab(0)->nextofs)
//                    if ((parent == m_shmptr) && !parent->nextofs m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
                    if ((parent == symtab(0)) && !symtab(symptr->nextofs)->nextofs) gc(false); //garbage collect
//                    if (!symtab(m_shmptr->nextofs)->nextofs) m_shmptr->nextofs = shmofs(&m_shmptr[1]); //gc
                    dump("after dealloc(0x%x):", shmofs(ptr));
                    return;
                }
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
            }
            throw std::runtime_error("ShmHeap: attempt to dealloc unknown address");
        }
    private: //data
        int m_key; //only used for owner
//    void* m_ptr; //ptr to start of shm
        size32_t m_size; //total size (bytes)
        typedef struct
        {
            size32_t nextofs;
            union
            {
                char name[1]; //named storage entry (name + storage space)
                std::mutex mutex; //thread/proc sync
            };
        } entry;
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
        inline entry* symtab(size32_t ofs) const { return (entry*)((long)m_shmptr + ofs); }
//        inline size32_t symofs(entry* symptr) const { return (long)symptr - (long)m_symtab; }
//        typedef struct { size_t used; std::mutex mutex; entry symtab[1]; } hdr;
//    inline entry* symtab() { return &m_ptr->symtab[0]; }
//        hdr* m_ptr; //ptr to start of shm
//        struct { size32_t nextofs; std::mutex mutex; entry symtab[1]; }* m_ptr; //shm struct ptr
//        struct { std::mutex mutex; entry symtab[1]; }* m_shmptr; //shm struct
        entry* m_shmptr; //symbol table in shm
//        struct { size_t nextofs; std::mutex mutex; }* m_ptr; //shm struct ptr
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
        bool m_persist;
    public:
        inline size32_t shmofs(void* ptr) const { return (long)ptr - (long)m_shmptr; }
    private: //helpers
#undef dump
#undef gc
    class scoped_lock: public std::unique_lock<std::mutex>
    {
    public:
        /*explicit*/ scoped_lock(): std::unique_lock<std::mutex>(m_shmptr->mutex) {};
//        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
    };
//garbage collector:
    void gc(int line, bool init) //kludge: need extra param here for __VA_ARGS__
    {
        ATOMIC_MSG(YELLOW_MSG << "gc(" << init << ")" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//        if (init)
//        {
//            m_ptr[0].nextofs = sizeof(m_ptr[0]); //symofs(m_ptr->symtab); //[0] - (long)m_ptr;
//            m_ptr->symtab[0].name[0] = '\0'; //eof marker
//            m_shmptr->symtab[0].nextofs = 0; //eof marker
        m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
        m_shmptr[1].nextofs = 0; //eof marker
//        }
    }
//show shm details (for debug):
        const std::string /*NO &*/ desc() const
//        const char* desc() const
        {
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //symtab(m_ptr->nextofs);
            entry* parent = symtab(0); //m_shmptr;
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
                if (!symptr->nextofs) break; //eof
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
            }
            size32_t used = shmofs(parent); //symptr);
//            static std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
//            ostrm.str(""); ostrm.clear(); //https://stackoverflow.com/questions/5288036/how-to-clear-ostringstream
            std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
            ostrm << m_size << " bytes for shm key " << FMT("0x%x") << m_key;
            ostrm << " at " << FMT("0x%x") << m_shmptr;
            ostrm << ", used = " << used << ", avail " << (m_size - used);
//            std::cout << ostrm.str().c_str() << "\n" << std::flush;
            return ostrm.str(); //.c_str();
        }
        void dump(int line, const char* desc, long val = 0)
        {
            ATOMIC_MSG(BLUE_MSG << "shm " << FMT(desc) << val << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//            entry* symptr = symtab(0); //symtab(m_ptr->nextofs);
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
            entry* parent = symtab(0); //m_shmptr;
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
                if (!symptr->nextofs) //eof
                {
                    ATOMIC_MSG(BLUE_MSG << FMT("[0x%x]") << shmofs(symptr) << " eof" << ENDCOLOR);
                    return;
                }
                int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
                ATOMIC_MSG(BLUE_MSG << FMT("[0x%x]") << shmofs(symptr) << " next ofs " << FMT("0x%x") << symptr->nextofs << ", name " << strlen(symptr->name) << ":'" << symptr->name << "', adrs " << FMT("0x%x") << shmofs(symptr->name + keylen) << "..+8" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
            }
        }
    };
    static ShmHeap shmheap; //use static member to reduce clutter in var instances
};
#endif


////////////////////////////////////////////////////////////////////////////////

//overload new + delete operators:
//can be global or class-specific
//NOTE: these are static members (no "this")
//void* operator new(size_t size); 
//void operator delete(void* pointerToDelete); 
//-OR-
//void* operator new(size_t size, MemoryManager& memMgr); 
//void operator delete(void* pointerToDelete, MemoryManager& memMgr);

// https://www.geeksforgeeks.org/overloading-new-delete-operator-c/
// https://stackoverflow.com/questions/583003/overloading-new-delete

#if 0
class IMemoryManager 
{
public:
    virtual void* allocate(size_t) = 0;
    virtual void free(void*) = 0;
};
class MemoryManager: public IMemoryManager
{
public: 
    MemoryManager( );
    virtual ~MemoryManager( );
    virtual void* allocate(size_t);
    virtual void free(void*);
};
MemoryManager gMemoryManager; // Memory Manager, global variable
void* operator new(size_t size)
{
    return gMemoryManager.allocate(size);
}
void* operator new[](size_t size)
{
    return gMemoryManager.allocate(size);
}
void operator delete(void* pointerToDelete)
{
    gMemoryManager.free(pointerToDelete);
}
void operator delete[](void* arrayToDelete)
{
    gMemoryManager.free(arrayToDelete);
}
#endif

#endif //ndef _SHMALLOCATOR_H