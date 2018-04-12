//simple msg que
//uses mutex (shared) and cond var
//for ipc, mutex + cond var need to be in shared memory; for threads, just needs to be in heap
//this is designed for synchronous usage; msg que holds 1 int ( can be used as bitmap)
//template<int MAXLEN>
//no-base class to share mutex across all template instances:
//template<int MAXDEPTH>


#ifndef _MSGQUE_H
#define _MSGQUE_H

#include <mutex>
#include <condition_variable>
//for example see http://en.cppreference.com/w/cpp/thread/condition_variable
#include <type_traits> //std::decay<>, std::remove_reference<>, std::remove_pointer<>
#include "shmalloc.h"

#ifndef VOLATILE
 #define VOLATILE
#endif

#ifdef MSGQUE_DEBUG
 #include "atomic.h" //ATOMIC_MSG()
 #include "ostrfmt.h" //FMT()
 #include "elapsed.h" //timestamp()
 #include "msgcolors.h" //SrcLine, msg colors
 #define DEBUG_MSG  ATOMIC_MSG
 #define WANT_DEBUG(stmt)  stmt
#else
 #define DEBUG_MSG(msg)  {} //noop
 #define WANT_DEBUG(stmt)  {} //noop
 #include "msgcolors.h" //SrcLine, msg colors
#endif
#ifndef MSGQUE_DETAILS
 #define MSGQUE_DETAILS  0
#endif

#ifdef IPC_THREAD
 #define IFIPC(stmt)  stmt
 #define malloc(size, shmkey)  ::shmalloc(size, shmkey, SRCLINE)
 #define memsize(ptr)  ::shmsize(ptr)
#else
 #define IFIPC(stmt)  //noop
 #define malloc(size, shmkey)  malloc(size)
 #define memsize(ptr)  sizeof(*ptr)
#endif

//#if 1 //new def; shm-compatible
//usage:
//    ShmMsgQue& msgque = *(ShmMsgQue*)shmalloc(sizeof(ShmMsgQue), shmkey, SRCLINE);
//    if (isParent) new (&msgque) ShmMsgQue(name, SRCLINE); //call ctor to init (parent only)
//    ...
//    if (isParent) { msgque.~ShmMsgQue(); shmfree(&msgque, SRCLINE); }

//#define SINGLE_THREADED  0
//#define MULTI_THREADED  1
//#define MULTI_PROCESS  -1
//template <bool IPC>

//safe for multi-threading (uses mutex):
#ifndef PARAMS
 #define PARAMS  SRCLINE, [](auto& _)
#endif
class MsgQue//Base
{
//protected:
private: //data
//    static std::mutex my_mutex; //assume low usage, share across all signals
    struct //wrap my data in case it needs to be in shared memory
    {
        VOLATILE std::mutex mutex;
        std::condition_variable condvar;
//    int m_msg[MAXLEN], m_count;
//    std::string m_name; //for debug only
        WANT_DEBUG(char name[20]); //store name directly in object so shm object doesn't use char pointer (only for debug)
        /*volatile*/ int msg;
//    } IFIPC(*) m_data; //shmem wrapper
    }* m_ptr;
//    IFIPC(typedef std::remove_pointer<decltype(m_data)> shmtype);
//    typedef std::remove_pointer<decltype(m_data)> shmtype;
public: //ctor params
//    struct CtorParams
//    {
        const char* name = 0;
#ifdef IPC_THREAD //shmem handling info
//        IFIPC(int shmkey = 0); //shmem handling info
        int shmkey = 0; //shmem handling info
//        int extra = 0;
#else
        const int shmkey = 0;
#endif
        bool want_reinit = true;
//        bool debug_free = true;
        SrcLine srcline = 0;
//    };
//    typedef typename std::conditional<AUTO_LOCK, WithMutex<TYPE, AUTO_LOCK>, TYPE>::type shm_type; //see https://stackoverflow.com/questions/17854407/how-to-make-a-conditional-typedef-in-c?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//    shm_type* m_ptr;
//    bool m_want_init, m_debug_free;
public: //ctor/dtor
//    explicit MsgQue(const char* name = 0, VOLATILE std::mutex& mutex = /*std::mutex()*/ shared_mutex): m_msg(0), m_mutex(mutex)
//    explicit MsgQue(const char* name = 0): m_msg(0)
    explicit MsgQue(SrcLine mySrcLine = 0, void (*get_params)(MsgQue&) = 0) //: m_msg(0) //int& i, std::string& s, bool& b, SrcLine& srcline) = 0) //: i(0), b(false), srcline(0), o(nullptr)
    {
//        /*static*/ struct CtorParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
        if (mySrcLine) /*params.*/srcline = mySrcLine;
        if (get_params) get_params(*this); //params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//        /*if (MSGQUE_DETAILS)*/ { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
//        strncpy(m_name, "MsgQue-", sizeof(m_name));
        m_ptr = static_cast<decltype(m_ptr)>(malloc(sizeof(*m_ptr) /*+ Extra*/, shmkey)); //, SrcLine srcline = 0)
        if (!m_ptr) return;
        IFIPC(if (::shmexisted(m_ptr) && !want_reinit) return);
        memset(m_ptr, 0, memsize(m_ptr)); //sizeof(*m_ptr) + EXTRA); //re-init (not needed first time)
//        m_ptr->mutex.std::mutex();
//        m_ptr->locked = false;
//        m_ptr->data.TYPE();
//        m_ptr->WithMutex<TYPE>(std::forward<ARGS>(args) ...);
//        m_ptr->WithMutex<TYPE, AUTO_LOCK>(std::forward<ARGS>(args ...)); //pass args to TYPE's ctor (perfect fwding)
        new (m_ptr) decltype(*m_ptr)::type(); //, srcline); //pass args to TYPE's ctor (perfect fwding)
        WANT_DEBUG(strncpy(m_ptr->name, (name && *name)? name: "(unnamed)", sizeof(m_ptr->name)));
    }
    ~MsgQue()
    {
        if (!m_ptr) return;
        if (m_ptr->msg /*&& MSGQUE_DETAILS*/) DEBUG_MSG(RED_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".dtor: !empty @exit " << FMT("0x%x") << m_ptr->msg << ENDCOLOR_ATLINE(srcline)) //benign, but might be caller bug so complain
        else DEBUG_MSG(GREEN_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".dtor: empty @exit" << ENDCOLOR_ATLINE(srcline));
//        if (m_autodel) delete this;
    }
public: //mem alloc
#if 0
    static void* operator new(size_t size, const char* srcfile, int srcline, bool autodel = false)
    {
        void* ptr = new char[size];
        const char* bp = strrchr(srcfile, '/');
        if (bp) srcfile = bp + 1;
        DEBUG_MSG(YELLOW_MSG << timestamp() << "alloc size " << size << " at adrs " << FMT("0x%x") << ptr << ", autodef? " << autodel << " from " << srcfile << ":" << srcline << ENDCOLOR << std::flush);
//        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
//        m_autodel = autodel; //won't work with static method
//        if (autodel)
//#include <cstdlib>
//void exiting() { std::cout << "Exiting"; }
//    std::atexit(exiting);
        return ptr;
    }
    static void operator delete(void* ptr) noexcept
    {
  //      shmheap.dealloc(ptr);
        delete ptr;
        DEBUG_MSG(YELLOW_MSG << timestamp() << "dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
    }
#endif
public: //getters
    inline VOLATILE std::mutex& mutex() { return m_ptr->mutex; } //in case caller wants to share between instances
public: //operators
    MsgQue* operator->() { return this; } //for compatibility with Shm wrapper operator->
    MsgQue* get() { return this; }
public: //methods
//    MsgQue& clear() { m_msg = 0; return *this; } //fluent
public: //methods
    MsgQue& clear()
    {
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
        m_ptr->msg = 0;
        return *this; //fluent
    }
//    struct SendParams { int msg = 0; bool broadcast = false; SrcLine srcline = SRCLINE; }; //FuncParams() { s = "none"; i = 999; b = true; }}; // FuncParams;
    MsgQue& send(int msg, bool broadcast = false, SrcLine srcline = 0)
//    MsgQue& send(SrcLine mySrcLine = 0, void (*get_params)(struct SendParams&) = 0)
    {
//        /*static*/ struct SendParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//        std::stringstream ssout;
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_ptr->msg & msg) throw "MsgQue.send: msg already queued"; // + tostr(msg) + " already queued");
        m_ptr->msg |= msg; //use bitmask for multiple msgs
        if (!(m_ptr->msg & msg)) throw "MsgQue.send: msg enqueue failed"; // + tostr(msg) + " failed");
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "one") << ", now qued " << FMT("0x%x") << m_ptr->msg << FMT(", adrs %p") << this << ENDCOLOR_ATLINE(srcline));
        if (!broadcast) m_ptr->condvar.notify_one(); //wake main thread
        else m_ptr->condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << FMT("0x%x") << m_ptr->msg << " wanted " << FMT("0x%x") << val << "? " << (m_ptr->msg == val) << ENDCOLOR); return m_ptr->msg == val; }
    bool not_wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << FMT("0x%x") << m_ptr->msg << " !wanted " << FMT("0x%x") << val << "? " << (m_ptr->msg != val) << ENDCOLOR); return m_ptr->msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
//    struct RcvParams { bool (MsgQue::*filter)(int val) = 0, int operand = 0, bool remove = false, SrcLine srcline = SRCLINE; };
    int rcv(bool (MsgQue::*filter)(int val), int operand = 0, bool remove = false, SrcLine srcline = 0)
//    int rcv(SrcLine mySrcLine = 0, void (*get_params)(struct RcvParams&) = 0)
    {
//        /*static*/ struct RcvParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
//        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".rcv: op " << FMT("0x%x") << operand << ", msg " << FMT("0x%x") << m_ptr->msg << FMT(", adrs %p") << this << ENDCOLOR_ATLINE(srcline));
        while (!(this->*filter)(operand)) m_ptr->condvar.wait(scoped_lock); //ignore spurious wakeups
        int retval = m_ptr->msg;
        if (remove) m_ptr->msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
#if 0
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
#endif
//protected:
private: //helpers
//can't use nested class on non-static members :(
//    class scoped_lock: public std::unique_lock<std::mutex>
//    {
//    public:
//        /*explicit*/ scoped_lock(): std::unique_lock<std::mutex>(m_mutex) {};
////        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
//    };
//    static const char* tostr(int val)
//    {
//        static char buf[20];
//        snprintf(buf, sizeof(buf), "0x%x", val);
//        return buf;
//    }
};
//#endif


//#ifdef SHM_KEY
// #include "shmalloc.h"
//ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, 0x4567feed);
//#endif

//#ifdef SHM_KEY
//multi-process queue:
//can be accessed by multiple proceses
//class MsgQue: public ShmHeapAlloc //: public MsgQue
//#else
//single process queue:
//can be accessed by multiple threads within same process
//class MsgQue: public MsgQueBase
#if 0 //old def (working)
#define VOLATILE  //volatile //TODO: is this needed for multi-threaded app?
static VOLATILE std::mutex shared_mutex;

class MsgQue//Base
//#endif
{
public: //ctor/dtor
    explicit MsgQue(const char* name = 0, VOLATILE std::mutex& mutex = /*std::mutex()*/ shared_mutex): m_msg(0), m_mutex(mutex)
    {
        /*if (MSGQUE_DETAILS)*/ { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
    }
    ~MsgQue()
    {
        if (m_msg /*&& MSGQUE_DETAILS*/) DEBUG_MSG(RED_MSG << m_name << ".dtor: !empty @exit " << FMT("0x%x") << m_msg << ENDCOLOR) //benign, but might be caller bug so complain
        else DEBUG_MSG(GREEN_MSG << m_name << ".dtor: empty @exit" << ENDCOLOR);
//        if (m_autodel) delete this;
    }
#if 0
    static void* operator new(size_t size, const char* srcfile, int srcline, bool autodel = false)
    {
        void* ptr = new char[size];
        const char* bp = strrchr(srcfile, '/');
        if (bp) srcfile = bp + 1;
        DEBUG_MSG(YELLOW_MSG << "alloc size " << size << " at adrs " << FMT("0x%x") << ptr << ", autodef? " << autodel << " from " << srcfile << ":" << srcline << ENDCOLOR << std::flush);
//        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
//        m_autodel = autodel; //won't work with static method
//        if (autodel)
//#include <cstdlib>
//void exiting() { std::cout << "Exiting"; }
//    std::atexit(exiting);
        return ptr;
    }
    static void operator delete(void* ptr) noexcept
    {
  //      shmheap.dealloc(ptr);
        delete ptr;
        DEBUG_MSG(YELLOW_MSG << "dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
    }
#endif
public: //getters
    inline VOLATILE std::mutex& mutex() { return m_mutex; } //in case caller wants to share between instances
public: //methods
    MsgQue& clear()
    {
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_mutex);
        m_msg = 0;
        return *this; //fluent
    }
    MsgQue& send(int msg, bool broadcast = false)
    {
//        std::stringstream ssout;
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_mutex);
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_msg & msg) throw "MsgQue.send: msg already queued"; // + tostr(msg) + " already queued");
        m_msg |= msg; //use bitmask for multiple msgs
        if (!(m_msg & msg)) throw "MsgQue.send: msg enqueue failed"; // + tostr(msg) + " failed");
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "one") << ", now qued " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
    int rcv(bool (MsgQue::*filter)(int val), int operand = 0, bool remove = false)
    {
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_mutex);
//        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".rcv: op " << FMT("0x%x") << operand << ", msg " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR);
        while (!(this->*filter)(operand)) m_condvar.wait(scoped_lock); //ignore spurious wakeups
        int retval = m_msg;
        if (remove) m_msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
#if 0
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
#endif
//protected:
private: //helpers
//can't use nested class on non-static members :(
//    class scoped_lock: public std::unique_lock<std::mutex>
//    {
//    public:
//        /*explicit*/ scoped_lock(): std::unique_lock<std::mutex>(m_mutex) {};
////        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
//    };
//    static const char* tostr(int val)
//    {
//        static char buf[20];
//        snprintf(buf, sizeof(buf), "0x%x", val);
//        return buf;
//    }
//protected:
private: //data
//    static std::mutex my_mutex; //assume low usage, share across all signals
    VOLATILE std::mutex& m_mutex;
    std::condition_variable m_condvar;
//    int m_msg[MAXLEN], m_count;
    std::string m_name; //for debug only
    /*volatile*/ int m_msg;
};
//std::mutex MsgQue::my_mutex;
#endif


#if 0
#include "shmalloc.h"
//ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, 0x4567feed);

//multi-process queue:
//can be accessed by multiple proceses
class ShmMsgQue: public ShmHeapAlloc //: public MsgQue
{
public:
    explicit ShmMsgQue(const char* name = 0): m_msg(0)
    {
        if (MSGQUE_DETAILS) { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
    }
    ~ShmMsgQue()
    {
        if (m_msg && MSGQUE_DETAILS) DEBUG_MSG(RED_MSG << m_name << ".dtor: !empty" << FMT("0x%x") << m_msg << ENDCOLOR); //benign, but might be caller bug so complain
    }
public:
    ShmMsgQue& clear()
    {
        m_msg = 0;
        return *this; //fluent
    }
    ShmMsgQue& send(int msg, bool broadcast = false)
    {
//        std::stringstream ssout;
        scoped_lock lock;
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_msg & msg) throw "MsgQue.send: msg already queued"; // + tostr(msg) + " already queued");
        m_msg |= msg; //use bitmask for multiple msgs
        if (!(m_msg & msg)) throw "MsgQue.send: msg enqueue failed"; // + tostr(msg) + " failed");
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "1") << ", now qued " << FMT("0x%x") << m_msg << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
    int rcv(bool (ShmMsgQue::*filter)(int val), int operand = 0, bool remove = false)
    {
//        std::unique_lock<std::mutex> lock(mutex);
        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".rcv: " << FMT("0x%x") << m_msg << ENDCOLOR);
        while (!(this->*filter)(operand)) m_condvar.wait(lock); //ignore spurious wakeups
        int retval = m_msg;
        if (remove) m_msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
#if 0
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
#endif
//protected:
private:
    class scoped_lock: public std::unique_lock<std::mutex>
    {
    public:
        explicit scoped_lock(): std::unique_lock<std::mutex>(m_mutex) {};
//        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
    };
//    static const char* tostr(int val)
//    {
//        static char buf[20];
//        snprintf(buf, sizeof(buf), "0x%x", val);
//        return buf;
//    }
//protected:
private:
    static std::mutex m_mutex; //assume low usage, share across all signals
    std::condition_variable m_condvar;
//    int m_msg[MAXLEN], m_count;
    std::string m_name; //for debug only
    /*volatile*/ int m_msg;
};
std::mutex ShmMsgQue::m_mutex;
#endif


#undef malloc
#undef WANT_DEBUG
#undef DEBUG_MSG
#endif //ndef _MSGQUE_H