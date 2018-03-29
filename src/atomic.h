//atomic stmt wrapper:
//wraps a thread- or ipc-safe critical section of code with a mutex lock/unlock

#ifndef _ATOMIC_H
#define _ATOMIC_H

#include <mutex> //std::mutex, std::unique_lock
#include "msgcolors.h" //SrcLine and colors (used with ATOMIC_MSG)
//#include <memory> //std::unique_ptr<>

#include <iostream> //std::cout, std::flush
#define MY_DEBUG(msg)  //noop
//#define MY_DEBUG(msg)  std::cout << msg << "\n" << std::flush


//atomic wrapper to iostream messages:
#ifdef IPC_THREAD
// #include "shmkeys.h"
// #include "shmalloc.h"

// ShmPtr<std::mutex, ATOMIC_MSG_SHMKEY, 0, true, false> atomic_mut_ptr; //ipc mutex (must be in shared memory)
// std::mutex& atomic_mut = *atomic_mut_ptr.get();
// std::mutex& atomic_mut; //fwd ref
// std::mutex& get_atomic_mut(); //fwd ref
// #define atomic_mut  get_atomic_mut()
// ShmScope<MsgQue> mscope(SRCLINE, SHMKEY1, "mainq"), wscope(SRCLINE, SHMKEY2, "wkerq"); //, mainq.mutex());
// MsgQue& mainq = mscope.shmobj.data;
// typedef std::unique_lock<std::mutex> base_type;
 class LockOnce //: private base_type
 {
//    static std::mutex m_mutex; //in-process mutex (all threads have same address space)
    std::unique_lock<std::mutex> m_lock;
//    bool m_need_unlock;
 public: //ctor/dtor
//    LockOnce(): base_type(mutex(), std::defer_lock) { std::cout << "should i lock? " << &mutex() << "\n" << std::flush; } // ++recursion(); }
//    ~LockOnce() {} //{ --recursion(); } //unlock when goes out of scope
    LockOnce(bool want_lock = true): m_lock(mutex(), std::defer_lock) { maybe_lock(want_lock); } // ++recursion(); }
    ~LockOnce() { /*if (!m_need_unlock) m_lock.mutex()*/ } //{ --recursion(); } //unlock when goes out of scope
 private: //members
    void maybe_lock(bool want_lock)
    {
        MY_DEBUG("LockOnce: should i lock? " << (want_lock && !!m_lock.mutex()));
        if (want_lock && m_lock.mutex()) m_lock.lock(); //only lock mutex if it is ready
    } 
//    std::mutex& first_lock(std::mutex& mutex) { if (!recursion()++) lock(); } //only lock 1x/process
//    static int& recursion() //use wrapper to avoid trailing static decl at global scope
//    {
//        static int m_recursion = 0;
//        std::cout << "LockOnce: recursion = " << m_recursion << "\n" << std::flush;
//        return m_recursion;
//    }
    static std::mutex& mutex(); //can't define this one until later
 }; 
#else
//std::recursive_mutex atomic_mut;
// std::mutex atomic_mut; //in-process mutex (all threads have same address space)
// typedef std::unique_lock<std::mutex> base_type;
 class LockOnce //: private base_type
 {
//    static std::mutex m_mutex; //in-process mutex (all threads have same address space)
    std::unique_lock<std::mutex> m_lock;
 public: //ctor/dtor
    LockOnce(bool ignored = false): m_lock(mutex()) {} //{ mutex().lock(); } //: base_type(mutex()) {}
    ~LockOnce() {} //mutex().unlock(); }
 private: //members
    static std::mutex& mutex() //use wrapper to avoid trailing static decl at global scope
    {
        static std::mutex m_mutex;
        return m_mutex;
    }
 }; 
// std::mutex LockOnce::m_mutex;
#endif


//#ifdef ATOMIC_DEBUG
class InOut
{
    const char* m_funcname;
public: //ctor/dtor
    InOut(const char* funcname = 0): m_funcname(funcname) { debug(true); }
    ~InOut() { debug(false); }
private: //helpers
    void debug(bool inout)
    {
        if (inout) MY_DEBUG(m_funcname << " in ...");
        else MY_DEBUG(" ... " << m_funcname << " out");
    }
};
//#endif


//handle optional macro params:
//see https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//for stds way to do it without ##: https://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick?noredirect=1&lq=1
#define USE_ARG3(one, two, three, ...)  three

//use macro so any stmt can be nested within scoped lock:
//CAUTION: recursive if IPC_THREAD is turned on with DEBUG for ShmPtr (in shmalloc.h)
//use optional parameter to bypass lock (to avoid recursion deadlock on mutex)
//#define ATOMIC(stmt)  { std::unique_lock<std::mutex, std::defer_lock> lock(atomic_mut); stmt; }
//#define ATOMIC(stmt)  { LockOnce lock; stmt; }
//#define ATOMIC(stmt)  { std::unique_ptr<LockOnce> lock(new LockOnce()); stmt; }
#define ATOMIC_1ARG(stmt)  { InOut here("atomic-1arg"); LockOnce lock; stmt; }
#define ATOMIC_2ARGS(stmt, want_lock)  { InOut here("atomic-2args"); LockOnce lock(want_lock); stmt; }
#define ATOMIC(...)  USE_ARG3(__VA_ARGS__, ATOMIC_2ARGS, ATOMIC_1ARG) (__VA_ARGS__)

#define ATOMIC_MSG_1ARG(msg)  ATOMIC(std::cout << msg << std::flush)
#define ATOMIC_MSG_2ARGS(msg, want_lock)  ATOMIC(std::cout << msg << std::flush, want_lock)
#define ATOMIC_MSG(...)  USE_ARG3(__VA_ARGS__, ATOMIC_MSG_2ARGS, ATOMIC_MSG_1ARG) (__VA_ARGS__)


#ifdef IPC_THREAD
 #include "shmkeys.h"
 #include "shmalloc.h"
//this one uses ATOMIC_MSG macro (recursive), so put it later
std::mutex& LockOnce::mutex() //use wrapper to avoid trailing static decl at global scope
{
//CAUTION: recursive; need to satisfy unique_lock<> on deeper levels without mutex ready yet
//        static std::mutex m_mutex;
//    static bool ready = false; //enum { None, Started, Ready } state = None;
    static bool busy = false; //detect recursion
    MY_DEBUG("cre mutex: busy? " << busy);
    if (busy) return *(std::mutex*)0; //placeholder mutex; NOTE: will segfault if caller tries to use this
//    if (!ready /*state == Started*/) return *(std::mutex*)0; //placeholder mutex; NOTE: will segfault if caller tries to use it
//    state = Started;
    busy = true;
    static ShmPtr_params override(SRCLINE, ATOMIC_MSG_SHMKEY, 0, true, false); //don't lock debug msgs after shmfree() (shared mutex is gone)
    static ShmPtr<std::mutex, /*ATOMIC_MSG_SHMKEY, 0, true,*/ false> m_mutex; //ipc mutex (must be in shared memory); static for persistence and defered init to avoid “static initialization order fiasco”
//    m_mutex.debug_free = false; //don't lock debug msgs after shmfree() (shared mutex is gone)
//    static std::mutex nested_mutex;
//    state = Ready;
    busy = false;
    return *m_mutex.get(); //return real shared mutex; //(state == Ready)? *m_mutex.get(): *(std::mutex*)0; //nested_mutex;
}

//class ordering
//{
//public: //ctor/dtor
//    ordering() { ATOMIC_MSG(BLUE_MSG << "first atomic msg" << ENDCOLOR); }
//    ~ordering() { ATOMIC_MSG(BLUE_MSG << "last atomic msg" << ENDCOLOR); }
//};
//ordering dummy; //kludge: force creation of global mutex before other usage so it will be destroyed last
#endif


#if 0 //def IPC_THREAD //NOTE: need to put these *after* ATOMIC_MSG def
class InOut
{
    const char* m_funcname;
    typedef void (*callback)(bool in); //void*);
//    struct { callback cb; void* arg; } m_cb;
    callback m_cb;
public: //ctor/dtor
//    template<typename ... ARGS>
//    InOut(const char* funcname, void (*cb)(std::forward<ARGS>(args ...)), ARGS&& ... args): m_func(func), m_cb(cb) { std::cout << m_func << " in ...\n" << std::flush; }
    InOut(const char* funcname, callback cb = 0 /*, void* cbarg = 0*/): m_funcname(funcname), m_cb(cb) //{cb, cbarg})
    {
        if (m_funcname) std::cout << m_funcname << " in ...\n" << std::flush;
        if (cb) cb(true); //cbarg);
    }
    ~InOut()
    {
//        if (m_cb.cb) m_cb.cbm_cb.arg);
        if (m_cb) m_cb(false);
        if (m_funcname) std::cout << " ... " << m_funcname << " out\n" << std::flush;
    }
};

// #error TBD
 #include "shmkeys.h"
 #include "shmalloc.h"
// ShmPtr<std::mutex, ATOMIC_MSG_SHMKEY, 0, true, false> atomic_mut_ptr; //ipc mutex (must be in shared memory)
// std::mutex& atomic_mut = *atomic_mut_ptr.get();
std::mutex& get_atomic_mut()
{
//    static std::unique_ptr<char> recursion;
    static int recursion = 0; //CAUTION: nested ATOMIC within ShmPtr (viz shmalloc)
    InOut here("get_atomic_mut", [recursion](bool in) { recursion += in? 1: -1; });
//    static std::mutex inner_mutex;
//    if (recursion > 1) return inner_mutex;
    static ShmPtr<std::mutex, ATOMIC_MSG_SHMKEY, 0, true, false> atomic_mut_ptr; //ipc mutex (must be in shared memory); static for persistence and defered init to avoid “static initialization order fiasco”
    return *atomic_mut_ptr.get();
}
#endif //def IPC_THREAD


#endif //ndef _ATOMIC_H