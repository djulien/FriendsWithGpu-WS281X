//critical section mutex:

#ifndef _CRITICAL_H
#define _CRITICAL_H

#ifdef IPC_THREAD //multi-proc
 #include "shmalloc.h"
 #include "msgcolors.h" //SRCLINE
#endif

#ifdef CRITICAL_DEBUG
 #include "atomic.h" //ATOMIC_MSG()
 #include "ostrfmt.h" //FMT()
 #include "elapsed.h" //timestamp()
 #include "msgcolors.h" //SrcLine, msg colors
 #define DEBUG_MSG  ATOMIC_MSG
#else
 #define DEBUG_MSG(msg)  //noop
 #include "msgcolors.h" //SrcLine, msg colors
#endif


//use template to allow caller to override sharing of mutex btween critical sections:
template <int SHMKEY = 0>
class CriticalSection
{
//    static std::mutex m_mutex; //in-process mutex (all threads have same address space)
    std::unique_lock<std::mutex> m_lock;
public: //ctor/dtor
    CriticalSection(/*int shmkey = 0*/ SrcLine srcline = 0): m_lock(mutex(/*shmkey*/)) { DEBUG_MSG(BLUE_MSG << timestamp() << FMT("critical %p in ...") << &mutex() << ENDCOLOR_ATLINE(srcline)); } //{ mutex().lock(); } //: base_type(mutex()) {}
    ~CriticalSection() { DEBUG_MSG(BLUE_MSG << timestamp() << FMT("... critical %p out") << &mutex() << ENDCOLOR); } //mutex().unlock(); }
private: //members
    static std::mutex& mutex(/*int shmkey = 0*/) //use wrapper to avoid trailing static decl at global scope
    {
#ifdef IPC_THREAD //multi-proc
        static ShmPtr_params override(SRCLINE, SHMKEY);
        static ShmPtr<std::mutex, false> m_mutex; //ipc mutex (must be in shared memory); static for persistence and defered init to avoid “static initialization order fiasco”
        return *m_mutex.get();
#else //multi-thread (single process)
        static std::mutex m_mutex;
        return m_mutex;
#endif
    }
}; 
// std::mutex LockOnce::m_mutex;


#undef DEBUG_MSG
#endif //ndef _CRITICAL_H