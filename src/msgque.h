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

#define VOLATILE  //volatile //TODO: is this needed for multi-threaded app?
static VOLATILE std::mutex shared_mutex;


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
class MsgQue//Base
//#endif
{
public: //ctor/dtor
    explicit MsgQue(const char* name = 0, VOLATILE std::mutex& mutex = /*std::mutex()*/ shared_mutex): m_msg(0), m_mutex(mutex)
    {
        /*if (WANT_DETAILS)*/ { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
    }
    ~MsgQue()
    {
        if (m_msg /*&& WANT_DETAILS*/) ATOMIC(std::cout << RED_MSG << m_name << ".dtor: !empty @exit " << FMT("0x%x") << m_msg << ENDCOLOR) //benign, but might be caller bug so complain
        else ATOMIC(std::cout << GREEN_MSG << m_name << ".dtor: empty @exit" << ENDCOLOR);
//        if (m_autodel) delete this;
    }
#if 0
    static void* operator new(size_t size, const char* srcfile, int srcline, bool autodel = false)
    {
        void* ptr = new char[size];
        const char* bp = strrchr(srcfile, '/');
        if (bp) srcfile = bp + 1;
        ATOMIC(std::cout << YELLOW_MSG << "alloc size " << size << " at adrs " << FMT("0x%x") << ptr << ", autodef? " << autodel << " from " << srcfile << ":" << srcline << ENDCOLOR << std::flush);
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
        ATOMIC(std::cout << YELLOW_MSG << "dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
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
        if (WANT_DETAILS) ATOMIC(std::cout << BLUE_MSG << timestamp() << m_name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "one") << ", now qued " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (WANT_DETAILS) ATOMIC(std::cout << FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { if (WANT_DETAILS) ATOMIC(std::cout << FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
    int rcv(bool (MsgQue::*filter)(int val), int operand = 0, bool remove = false)
    {
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_mutex);
//        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (WANT_DETAILS) ATOMIC(std::cout << BLUE_MSG << timestamp() << m_name << ".rcv: op " << FMT("0x%x") << operand << ", msg " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR);
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
        if (WANT_DETAILS) { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
    }
    ~ShmMsgQue()
    {
        if (m_msg && WANT_DETAILS) ATOMIC(std::cout << RED_MSG << m_name << ".dtor: !empty" << FMT("0x%x") << m_msg << ENDCOLOR); //benign, but might be caller bug so complain
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
        if (WANT_DETAILS) ATOMIC(std::cout << BLUE_MSG << timestamp() << m_name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "1") << ", now qued " << FMT("0x%x") << m_msg << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (WANT_DETAILS) ATOMIC(std::cout << FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { if (WANT_DETAILS) ATOMIC(std::cout << FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
    int rcv(bool (ShmMsgQue::*filter)(int val), int operand = 0, bool remove = false)
    {
//        std::unique_lock<std::mutex> lock(mutex);
        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (WANT_DETAILS) ATOMIC(std::cout << BLUE_MSG << timestamp() << m_name << ".rcv: " << FMT("0x%x") << m_msg << ENDCOLOR);
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


#endif //ndef _MSGQUE_H