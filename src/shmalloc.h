//shared memory allocator
//see https://www.ibm.com/developerworks/aix/tutorials/au-memorymanager/index.html
//see also http://anki3d.org/cpp-allocators-for-games/

//CAUTION: “static initialization order fiasco”
//https://isocpp.org/wiki/faq/ctors

//to look at shm:
// ipcs -m 
//to delete shm:
// ipcrm -M <key>


// https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
// https://stackoverflow.com/questions/4836863/shared-memory-or-mmap-linux-c-c-ipc
//    you identify your shared memory segment with some kind of symbolic name, something like "/myRegion"
//    with shm_open you open a file descriptor on that region
//    with ftruncate you enlarge the segment to the size you need
//    with mmap you map it into your address space

#ifndef _SHMALLOC_H
#define _SHMALLOC_H

//#include <iostream> //std::cout, std::flush
#include <sys/ipc.h>
#include <sys/shm.h> //shmctl(), shmget(), shmat(), shmdt()
#include <errno.h>
#include <stdexcept> //std::runtime_error
#include <mutex> //std::mutex, lock
#include <atomic> //std::atomic
#include <type_traits> //std::conditional<>
#include <memory> //std::shared_ptr<>


#include "msgcolors.h" //SrcLine, msg colors
#ifdef SHMALLOC_DEBUG //CAUTION: recursive
 #include "atomic.h" //ATOMIC_MSG()
 #include "ostrfmt.h" //FMT()
 #include "elapsed.h" //timestamp()
 #define DEBUG_MSG  ATOMIC_MSG
// #undef SHMALLOC_DEBUG
// #define SHMALLOC_DEBUG  ATOMIC_MSG
#else
 #define DEBUG_MSG(...)  //noop
// #define SHMALLOC_DEBUG(msg)  //noop
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Low-level shamlloc/shmfree (malloc/free emulator)
//

//#include <memory> //std::deleter
//#include "msgcolors.h"
//#define REDMSG(msg)  RED_MSG msg ENDCOLOR_NOLINE


//stash some info within shmem block returned to caller:
#define SHM_MAGIC  0xfeedbeef //marker to detect valid shmem block
typedef struct { int id; key_t key; size_t size; uint32_t marker; } ShmHdr;


//check if shmem ptr is valid:
static ShmHdr* shmptr(void* addr, const char* func)
{
    ShmHdr* ptr = static_cast<ShmHdr*>(addr); --ptr;
    if (ptr->marker == SHM_MAGIC) return ptr;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: bad shmem pointer %p", func, addr);
    throw std::runtime_error(buf);
}


//allocate memory:
void* shmalloc(size_t size, key_t key = 0, SrcLine srcline = 0)
{
    if ((size < 1) || (size >= 10e6)) throw std::runtime_error("shmalloc: bad size"); //throw std::bad_alloc(); //set reasonable limits
    size += sizeof(ShmHdr);
    if (!key) key = (rand() << 16) | 0xbeef;
    int shmid = shmget(key, size, 0666 | IPC_CREAT); //create if !exist; clears to 0 upon creation
    DEBUG_MSG(CYAN_MSG << timestamp() << "shmalloc: cre shmget key " << FMT("0x%lx") << key << ", size " << size << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR_ATLINE(srcline));
    if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
    struct shmid_ds shminfo;
    if (shmctl(shmid, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
    ShmHdr* ptr = static_cast<ShmHdr*>(shmat(shmid, NULL /*system choses adrs*/, 0)); //read/write access
    DEBUG_MSG(BLUE_MSG << timestamp() << "shmalloc: shmat id " << FMT("0x%lx") << shmid << " => " << FMT("%p") << ptr << ", cre by pid " << shminfo.shm_cpid << ", #att " << shminfo.shm_nattch << ENDCOLOR);
    if (ptr == (ShmHdr*)-1) throw std::runtime_error(std::string(strerror(errno)));
    ptr->id = shmid;
    ptr->key = key;
    ptr->size = shminfo.shm_segsz; //size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
    ptr->marker = SHM_MAGIC;
    return ++ptr;
}
void* shmalloc(size_t size, SrcLine srcline = 0) { return shmalloc(size, 0, srcline); }


//get shmem key:
key_t shmkey(void* addr) { return shmptr(addr, "shmkey")->key; }


//get size:
size_t shmsize(void* addr) { return shmptr(addr, "shmsize")->size; }


//dealloc shmem:
void shmfree(void* addr, bool debug_msg, SrcLine srcline = 0)
{
    ShmHdr* ptr = shmptr(addr, "shmfree");
    DEBUG_MSG(CYAN_MSG << timestamp() << FMT("shmfree: adrs %p") << addr << FMT(" = ptr %p") << ptr << ENDCOLOR_ATLINE(srcline));
    ShmHdr info = *ptr; //copy info before dettaching
//    struct shmid_ds info;
//    if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
    if (shmdt(ptr) == -1) throw std::runtime_error(strerror(errno));
    ptr = 0; //CAUTION: can't use ptr after this point
//    DEBUG_MSG(CYAN_MSG << "shmfree: dettached " << ENDCOLOR); //desc();
//    int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
//    if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
//    if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
    struct shmid_ds shminfo;
    if (shmctl(info.id, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
    if (!shminfo.shm_nattch) //no more procs attached, delete it
        if (shmctl(info.id, IPC_RMID, NULL /*ignored*/)) throw std::runtime_error(strerror(errno));
    DEBUG_MSG(CYAN_MSG << timestamp() << "shmfree: freed " << FMT("key 0x%lx") << info.key << FMT(", id 0x%lx") << info.id << ", size " << info.size << ", cre pid " << shminfo.shm_cpid << ", #att " << shminfo.shm_nattch << ENDCOLOR_ATLINE(srcline), debug_msg);
}
void shmfree(void* addr, SrcLine srcline = 0) { shmfree(addr, true, srcline); } //overload


//std::Deleter wrapper for shmfree:
//based on example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
template <typename TYPE>
struct shmdeleter
{ 
    void operator() (TYPE* ptr) const { shmfree(ptr); }
//    {
//        std::cout << "Call delete from function object...\n";
//        delete p;
//    }
};


///////////////////////////////////////////////////////////////////////////////
////
/// Higher level utility/mixin classes
//

//#include <atomic>
//#include "msgcolors.h"
//#include "ostrfmt.h" //FMT()


#define SIZEOF(thing)  (sizeof(thing) / sizeof(thing[0]))
#define divup(num, den)  (((num) + (den) - 1) / (den))
#define rdup(num, den)  (divup(num, den) * (den))
//#define size32_t  uint32_t //don't need huge sizes in shared memory; cut down on wasted bytes

#define MAKE_TYPENAME(type)  template<> const char* type::TYPENAME() { return #type; }


//"perfect forwarding" (typesafe args) to ctor:
//proxy/perfect forwarding info:
// http://www.stroustrup.com/wrapper.pdf
// https://stackoverflow.com/questions/24915818/c-forward-method-calls-to-embed-object-without-inheritance
// https://stackoverflow.com/questions/13800449/c11-how-to-proxy-class-function-having-only-its-name-and-parent-class/13885092#13885092
// http://cpptruths.blogspot.com/2012/06/perfect-forwarding-of-parameter-groups.html
// http://en.cppreference.com/w/cpp/utility/functional/bind
#define PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    explicit type(ARGS&& ... args): base(std::forward<ARGS>(args) ...)
//special handling for last param:

#define NEAR_PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    explicit type(ARGS&& ... args, key_t key = 0): base(std::forward<ARGS>(args) ..., key)


#define PAGE_SIZE  4096 //NOTE: no Userland #define for this, so set it here and then check it at run time; https://stackoverflow.com/questions/37897645/page-size-undeclared-c
#if 0
#include <unistd.h> //sysconf()
//void check_page_size()
int main_other(int argc, const char* argv[]);
//template <typename... ARGS >
//int main_other(ARGS&&... args)
//{ if( a<b ) std::bind( std::forward<FN>(fn), std::forward<ARGS>(args)... )() ; }
int main(int argc, const char* argv[])
#define main  main_other
{
    int pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize != PAGE_SIZE) DEBUG_MSG(RED_MSG << "PAGE_SIZE incorrect: is " << pagesize << ", expected " << PAGE_SIZE << ENDCOLOR)
    else DEBUG_MSG(GREEN_MSG << "PAGE_SIZE correct: " << pagesize << ENDCOLOR);
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
    ~MemPool() { DEBUG_MSG(YELLOW_MSG << timestamp() << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); }
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
        DEBUG_MSG(BLUE_MSG << timestamp() << timestamp() << "MemPool: alloc count " << count << ", used(req) " << used(count + 1) << " vs size " << sizeof(m_storage) << ENDCOLOR_ATLINE(srcline));
        if (used(count + 1) > sizeof(m_storage)) enlarge(used(count + 1));
        m_storage[m_storage[0]++] = count;
        void* ptr = &m_storage[m_storage[0]];
        m_storage[0] += count;
        DEBUG_MSG(YELLOW_MSG << timestamp() << "MemPool: allocated " << count << " octobytes at " << FMT("%p") << ptr << ", used " << used() << ", avail " << avail() << ENDCOLOR);
        return ptr; //malloc(count);
    }
    /*virtual*/ void free(void* addr, SrcLine srcline = 0)
    {
//        free(ptr);
        size_t inx = ((intptr_t)addr - (intptr_t)m_storage) / sizeof(m_storage[0]);
        if ((inx < 1) || (inx >= SIZE)) DEBUG_MSG(RED_MSG << "inx " << inx << ENDCOLOR);
        if ((inx < 1) || (inx >= SIZE)) throw std::bad_alloc();
        size_t count = m_storage[--inx] + 1;
        DEBUG_MSG(BLUE_MSG << timestamp() << "MemPool: deallocate count " << count << " at " << FMT("%p") << addr << ", reclaim " << m_storage[0] << " == " << inx << " + " << count << "? " << (m_storage[0] == inx + count) << ENDCOLOR_ATLINE(srcline));
        if (m_storage[0] == inx + count) m_storage[0] -= count; //only reclaim at end of pool (no other addresses will change)
        DEBUG_MSG(YELLOW_MSG << timestamp() << "MemPool: deallocated " << count << " octobytes at " << FMT("%p") << addr << ", used " << used() << ", avail " << avail() << ENDCOLOR);
    }
    /*virtual*/ void enlarge(size_t count, SrcLine srcline = 0)
    {
        count = rdup(count, PAGE_SIZE / sizeof(m_storage[0]));
        DEBUG_MSG(RED_MSG << timestamp() << "MemPool: want to enlarge " << count << " octobytes" << ENDCOLOR_ATLINE(srcline));
        throw std::bad_alloc(); //TODO
    }
    static const char* TYPENAME();
//private:
#define EXTRA  2
    void debug(SrcLine srcline = 0) { DEBUG_MSG(BLUE_MSG << timestamp() << "MemPool: size " << SIZE << " (" << sizeof(*this) << " actually) = #elements " << (SIZEOF(m_storage) - 1) << "+1, sizeof(units) " << sizeof(m_storage[0]) << ENDCOLOR_ATLINE(srcline)); }
//    typedef struct { size_t count[1]; uint32_t data[0]; } entry;
    size_t m_storage[EXTRA + divup(SIZE, sizeof(size_t))]; //first element = used count
//    size_t& m_used;
#undef EXTRA
};
//NOTE: caller might need to define these:
//template<>
//const char* MemPool<40>::TYPENAME() { return "MemPool<40>"; }


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
    ~MutexWithFlag() { DEBUG_MSG(BLUE_MSG << timestamp() << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); if (islocked) unlock(); }
public: //member functions
    void lock() { debug("lock"); std::mutex::lock(); islocked = true; }
    void unlock() { debug("unlock"); islocked = false; std::mutex::unlock(); }
//    static void unlock(MutexWithFlag* ptr) { ptr->unlock(); } //custom deleter for use with std::unique_ptr
    static const char* TYPENAME();
private:
    void debug(const char* func) { DEBUG_MSG(YELLOW_MSG << timestamp() << func << ENDCOLOR); }
};
const char* MutexWithFlag::TYPENAME() { return "MutexWithFlag"; }


#if 0
//ref count mixin class:
template <class TYPE>
class WithRefCount: public TYPE
{
    int m_count;
public: //ctor/dtor
    PERFECT_FWD2BASE_CTOR(WithRefCount, TYPE), m_count(0) { /*addref()*/; } //std::cout << "with mutex\n"; } //, islocked(m_mutex.islocked /*false*/) {} //derivation
    ~WithRefCount() { /*delref()*/; }
public: //helper class to clean up ref count
    typedef WithRefCount<TYPE> type;
    typedef void (*_Destructor)(type*); //void* data); //TODO: make generic?
    class Scope
    {
        type& m_obj;
        _Destructor& m_clup;
    public: //ctor/dtor
        Scope(type& obj, _Destructor&& clup): m_obj(obj), m_clup(clup) { m_obj.addref(); }
        ~Scope() { if (!m_obj.delref()) m_clup(&m_obj); }
    };
//    std::shared_ptr<type> clup(/*!thread.isParent()? 0:*/ &testobj, [](type* ptr) { DEBUG_MSG("bye " << ptr->numref() << "\n"); if (ptr->dec()) return; ptr->~TestObj(); shmfree(ptr, SRCLINE); });
public:
    int& numref() { return m_count; }
    int& addref() { return ++m_count; }
    int& delref() { /*DEBUG_MSG("dec ref " << m_count - 1 << "\n")*/; return --m_count; }
};
#endif


//mutex mixin class:
//3 ways to wrap parent class methods:
//    void* ptr1 = pool20->alloc(10);
//    void* ptr2 = pool20()().alloc(4);
//    void* ptr3 = pool20.base().base().alloc(1);
// https://stackoverflow.com/questions/4421706/what-are-the-basic-rules-and-idioms-for-operator-overloading?rq=1
//NOTE: operator-> is the only one that is applied recursively until a non-class is returned, and then it needs a pointer


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
    ~WithMutex() { DEBUG_MSG(BLUE_MSG << timestamp() << TYPENAME() << FMT(" dtor on %p") << this << ENDCOLOR); }
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
//    void lock() { DEBUG_MSG(YELLOW_MSG << "lock" << ENDCOLOR); m_mutex.lock(); /*islocked = true*/; }
//    void unlock() { DEBUG_MSG(YELLOW_MSG << "unlock" << ENDCOLOR); /*islocked = false*/; m_mutex.unlock(); }
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
//alternate approach: perfect forwarding without operator->; generates more code
//TODO?    template<typename ... ARGS> \
//    type(ARGS&& ... args): base(std::forward<ARGS>(args) ...)
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
//NOTE: caller might need to define these:
//template<>
//const char* WithMutex<MemPool<40>, true>::TYPENAME() { return "WithMutex<MemPool<40>, true>"; }


#if 0 //use ShmPtr<> instead
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
//    void debug() { DEBUG_MSG((*this)->TYPENAME << ENDCOLOR); }
};
//NOTE: caller might need to define these:
//template<>
//const char* shm_obj<WithMutex<MemPool<40>, true>>::TYPENAME() { return "shm_obj<WithMutex<MemPool<40>, true>>"; }
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// shmem ptr with auto-cleanup:
//

//shmem ptr wrapper class:
//like std::safe_ptr<> but for shmem:
//additional params are passed via template to keep ctor signature clean (for perfect fwding)
template <typename TYPE, int KEY = 0, int EXTRA = 0, bool INIT = true, bool AUTO_LOCK = true>
class ShmPtr
{
//    WithMutex<TYPE>* m_ptr;
//    struct ShmContents
//    {
//        int shmid; //need this to dettach/destroy later (key might have been generated)
//        std::mutex mutex; //need a mutex to control access, so include it with shmem object
//        std::atomic<bool> locked; //NOTE: mutex.try_lock() is not reliable (spurious failures); use explicit flag instead; see: http://en.cppreference.com/w/cpp/thread/mutex/try_lock
//        TYPE data; //caller's data (could be larger)
//    }* m_ptr;
    typedef typename std::conditional<AUTO_LOCK, WithMutex<TYPE, AUTO_LOCK>, TYPE>::type shm_type; //see https://stackoverflow.com/questions/17854407/how-to-make-a-conditional-typedef-in-c?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    shm_type* m_ptr;
//    WithMutex<TYPE, AUTO_LOCK>* m_ptr;
//    typedef decltype(*m_ptr) shm_type;
public: //ctor/dtor
//    PERFECT_FWD2BASE_CTOR(ShmPtr, TYPE)
    template<typename ... ARGS>
    explicit ShmPtr(ARGS&& ... args): m_ptr(0), debug_free(true)
    {
        m_ptr = static_cast<shm_type*>(::shmalloc(sizeof(*m_ptr) + EXTRA, KEY)); //, SrcLine srcline = 0)
        if (!INIT || !m_ptr) return;
        memset(m_ptr, 0, sizeof(*m_ptr) + EXTRA); //re-init (not needed first time)
//        m_ptr->mutex.std::mutex();
//        m_ptr->locked = false;
//        m_ptr->data.TYPE();
//        m_ptr->WithMutex<TYPE>(std::forward<ARGS>(args) ...);
//        m_ptr->WithMutex<TYPE, AUTO_LOCK>(std::forward<ARGS>(args ...)); //pass args to TYPE's ctor (perfect fwding)
        new (m_ptr) shm_type(std::forward<ARGS>(args) ...); //, srcline); //pass args to TYPE's ctor (perfect fwding)
     }
    ~ShmPtr()
    {
        if (!INIT || !m_ptr) return;
//        m_ptr->~WithMutex<TYPE>();
        m_ptr->~shm_type(); //call TYPE's dtor
//        std::cout << "good-bye ShmPtr\n" << std::flush;
        ::shmfree(m_ptr, debug_free);
    }
public: //operators
//    TYPE* operator->() { return m_ptr; }
    shm_type* operator->() { return m_ptr; }
//    operator TYPE&() { return *m_ptr; }
    shm_type* get() { return m_ptr; }
public: //info about shmem
    bool debug_free;
    key_t shmkey() { return ::shmkey(m_ptr); }
    size_t shmsize() { return ::shmsize(m_ptr); }
};


///////////////////////////////////////////////////////////////////////////////
////
/// STL-compatible shm allocator
//

//typedef shm_obj<WithMutex<MemPool<PAGE_SIZE>>> ShmHeap;
//class ShmHeap;

//minimal stl allocator (C++11)
//based on example at: https://msdn.microsoft.com/en-us/library/aa985953.aspx
//NOTE: use a separate sttr for shm key/symbol mgmt
template <class TYPE, int HEAP_SIZE = 300>
struct ShmAllocator  
{  
    typedef TYPE value_type;
    typedef WithMutex<MemPool<HEAP_SIZE>> ShmHeap; //bare sttr here, not smart_ptr;
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
        DEBUG_MSG(YELLOW_MSG << timestamp() << "ShmAllocator: allocated " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << FMT(" bytes for key 0x%lx") << key << " from " << (m_heap? "custom": "heap") << " at " << FMT("%p") << ptr << ENDCOLOR);
        if (!ptr) throw std::bad_alloc();
        return static_cast<TYPE*>(ptr);
    }  
    void deallocate(TYPE* const ptr, size_t count = 1, SrcLine srcline = 0) const noexcept
    {
        DEBUG_MSG(YELLOW_MSG << timestamp() << "ShmAllocator: deallocate " << count << " " << TYPENAME() << "(s) * " << sizeof(TYPE) << " bytes from " << (m_heap? "custom": "heap") << " at " << FMT("%p") << ptr << ENDCOLOR_ATLINE(srcline));
        if (m_heap) m_heap->free(ptr);
        else shmfree(ptr);
    }
//    std::shared_ptr<ShmHeap> m_heap; //allow heap to be shared between allocators
    ShmHeap* m_heap; //allow heap to be shared between allocators
    static const char* TYPENAME();
};


///////////////////////////////////////////////////////////////////////////////
////
/// Junk or obsolete code
//

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
        DEBUG_MSG(CYAN_MSG << "ShmSeg.ctor: key " << FMT("0x%lx") << key << ", persist " << (int)cre << ", size " << size << ENDCOLOR);
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
        DEBUG_MSG(CYAN_MSG << "ShmSeg: cre shmget key " << FMT("0x%lx") << key << ", size " << size << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR);
        if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
        struct shmid_ds info;
        if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
        void* ptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
        DEBUG_MSG(BLUE_MSG << "ShmSeg: shmat id " << FMT("0x%lx") << shmid << " => " << FMT("%p") << ptr << ENDCOLOR);
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
        DEBUG_MSG(CYAN_MSG << "ShmSeg: dettached " << ENDCOLOR); //desc();
    }
    static void destroy(key_t key)
    {
        if (!key) return; //!owner or !exist
        int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
        DEBUG_MSG(CYAN_MSG << "ShmSeg: destroy " << FMT("key 0x%lx") << key << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR);
        if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
        if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
        throw std::runtime_error(strerror(errno));
    }
public: //custom helpers
    static key_t crekey() { return (rand() << 16) | 0xfeed; }
};
#endif


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
        DEBUG_MSG(YELLOW_MSG << "ShmHeap: allocated " << count << " bytes at " << FMT("%p") << ptr << ", " << avail() << " bytes remaining" << ENDCOLOR);
        return ptr;
    }
    void dealloc(void* ptr)
    {
        int count = 1;
        DEBUG_MSG(YELLOW_MSG << "ShmHeap: deallocate " << count << " bytes at " << FMT("%p") << ptr << ", " << avail() << " bytes remaining" << ENDCOLOR);
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

#undef DEBUG_MSG
#endif //ndef _SHMALLOC_H