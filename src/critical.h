//critical section mutex:

#define CRITICAL_DEBUG
#define WANT_IPC  false

#ifndef _CRITICAL_H
#define _CRITICAL_H

#include <mutex> // std::mutex, std::unique_lock<>


//#ifdef IPC_THREAD //multi-proc
// #include "shmalloc.h"
// #include "msgcolors.h" //SRCLINE
//#endif

#ifndef WANT_IPC
 #define WANT_IPC  true //default true, otherwise caller probably wouldn't #include this file
#endif


#include "srcline.h"
#ifdef CRITICAL_DEBUG
 #include "atomic.h" //ATOMIC_MSG()
 #include "ostrfmt.h" //FMT()
 #include "elapsed.h" //timestamp()
 #include "msgcolors.h" //msg colors
 #define DEBUG_MSG  ATOMIC_MSG
#else
 #define DEBUG_MSG(msg)  {} //noop
#endif


//use template to allow caller to override sharing of mutex btween critical sections:
template <bool IPC = false> //int SHMKEY = 0>
class CriticalSection
{
//    static std::mutex m_mutex; //in-process mutex (all threads have same address space)
    std::unique_lock<std::mutex> m_lock;
public: //ctor/dtor
    CriticalSection(/*int shmkey = 0*/ SrcLine srcline = 0): m_lock(mutex(/*shmkey*/)) { DEBUG_MSG(BLUE_MSG << timestamp() << FMT("critical %p in ...") << &mutex() << ENDCOLOR_ATLINE(srcline)); } //{ mutex().lock(); } //: base_type(mutex()) {}
    ~CriticalSection() { DEBUG_MSG(BLUE_MSG << timestamp() << FMT("... critical %p out") << &mutex() << ENDCOLOR); } //mutex().unlock(); }
private: //members
//local memory (non-ipc) specialization:
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    static typename std::enable_if<!IPC_copy, std::mutex&>::type mutex() //use wrapper to avoid trailing static decl at global scope
    {
        static std::mutex m_mutex;
        return m_mutex;
    }
//shared memory (ipc) specialization:
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
//    static std::mutex& mutex(/*int shmkey = 0*/) //use wrapper to avoid trailing static decl at global scope
    static typename std::enable_if<IPC_copy, std::mutex&>::type mutex() //use wrapper to avoid trailing static decl at global scope
    {
        throw std::runtime_error("not implented");
//        static ShmPtr_params override(SRCLINE, SHMKEY);
//        static ShmPtr<std::mutex, false> m_mutex; //ipc mutex (must be in shared memory); static for persistence and defered init to avoid “static initialization order fiasco”
//        return *m_mutex.get();
        static std::mutex m_mutex;
        return m_mutex;
    }
}; 
// std::mutex LockOnce::m_mutex;


#undef DEBUG_MSG
#endif //ndef _CRITICAL_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit tests:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion
#include <iostream> //std::cout, std::flush
#include <chrono> //std::chrono
#include <thread>
#include "ostrfmt.h" //FMT()
#include "msgcolors.h"
#include "elapsed.h" //timestamp()
#include "ipc.h"

#ifndef MSG
 #define MSG(msg)  { std::cout << timestamp() << msg << std::flush; }
#endif

typedef void (*cb)(void);


void sleep_msec(int msec)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}


void thread_proc() { MSG(BLUE_MSG << timestamp() << "thread proc" << ENDCOLOR); }

//int main(int argc, const char* argv[])
void unit_test()
{
    MSG(PINK_MSG << "test cs" << ENDCOLOR);
//    IpcThread<true> thread(cb);
    IpcThread<true> thread(NAMED{ _.entpt = thread_proc; SRCLINE; });
    const char* color = thread.isParent()? PINK_MSG: BLUE_MSG;
    if (thread.isChild()) sleep_msec(1000); //give parent head start

    {
        CriticalSection<WANT_IPC> cs;
        MSG(color << timestamp() << thread.proctype() << " start" << ENDCOLOR);
        sleep_msec(500);
        MSG(color << timestamp() << thread.proctype() << " finish" << ENDCOLOR);
    }
    if (thread.isParent()) thread.join();
    else exit(0);
    MSG(CYAN_MSG << timestamp() << "quit " << thread.proctype() << ENDCOLOR);

//    return 0;
}
#endif //def WANT_UNIT_TEST
//eof