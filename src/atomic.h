//atomic stmt wrapper:
//wraps a thread- or ipc-safe critical section of code with a mutex lock/unlock

#ifndef _ATOMIC_H
#define _ATOMIC_H

#include <iostream> //std::cout, std::flush
#include <mutex> //std::mutex, std::unique_lock
#include "msgcolors.h" //SrcLine and colors (used with ATOMIC_MSG)
//#include <memory> //std::unique_ptr<>


//atomic operations:
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
 public: //ctor/dtor
//    LockOnce(): base_type(mutex(), std::defer_lock) { std::cout << "should i lock? " << &mutex() << "\n" << std::flush; } // ++recursion(); }
//    ~LockOnce() {} //{ --recursion(); } //unlock when goes out of scope
    LockOnce(): m_lock(mutex(), std::defer_lock) { maybe_lock(); } // ++recursion(); }
    ~LockOnce() {} //{ --recursion(); } //unlock when goes out of scope
 private: //members
    void maybe_lock()
    {
        std::cout << "LockOnce: should i lock? " << !!m_lock.mutex() << "\n" << std::flush;
        if (m_lock.mutex()) m_lock.lock(); //only lock mutex if it is ready
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
    LockOnce(): m_lock(mutex()) {} //{ mutex().lock(); } //: base_type(mutex()) {}
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
        if (inout) std::cout << m_funcname << " in ...\n" << std::flush;
        else std::cout << " ... " << m_funcname << " out\n" << std::flush;
    }
};
//#endif


//use macro so any stmt can be nested within scoped lock:
//CAUTION: might be recursive if IPC_THREAD is turned on for ShmPtr (in shmalloc.h)
//#define ATOMIC(stmt)  { std::unique_lock<std::mutex, std::defer_lock> lock(atomic_mut); stmt; }
//#define ATOMIC(stmt)  { LockOnce lock; stmt; }
//#define ATOMIC(stmt)  { std::unique_ptr<LockOnce> lock(new LockOnce()); stmt; }
#define ATOMIC_1ARG(stmt)  { InOut here("atomic"); LockOnce lock; stmt; }
#define ATOMIC_2ARGS(stmt, check)  { InOut here("atomic"); LockOnce lock; stmt; }

#define ATOMIC_MSG_1ARG(msg)  ATOMIC(std::cout << msg << std::flush)
#define ATOMIC_MSG_2ARGS(msg, check)  ATOMIC(std::cout << msg << std::flush, check)


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
    std::cout << "cre mutex: busy? " << busy << "\n" << std::flush;
    if (busy) return *(std::mutex*)0; //placeholder mutex; NOTE: will segfault if caller tries to use this
//    if (!ready /*state == Started*/) return *(std::mutex*)0; //placeholder mutex; NOTE: will segfault if caller tries to use it
//    state = Started;
    busy = true;
    static ShmPtr<std::mutex, ATOMIC_MSG_SHMKEY, 0, true, false> m_mutex; //ipc mutex (must be in shared memory); static for persistence and defered init to avoid “static initialization order fiasco”
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