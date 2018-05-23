//flow control using std::mutex and std::condition_variable

//#define SIGNAL_DEBUG


#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <mutex> //std::mutex, std::unique_lock<>
#include <condition_variable>
#include <unistd.h> //fork, getpid()
#include <thread> //std::this_thread
#include <stdexcept> //std::runtime_error()
//for example see http://en.cppreference.com/w/cpp/thread/condition_variable
//#include <type_traits> //std::conditional<>, std::decay<>, std::remove_reference<>, std::remove_pointer<>
//#include <tr2/type_traits> //std::tr2::direct_bases<>, std::tr2::bases<>
#include <chrono> //std::chrono
//#include <string> //std::stol()
#include "msgcolors.h"

#ifndef VOLATILE
 #define VOLATILE  //dummy keyword; not needed
#endif

#ifdef SIGNAL_DEBUG
 #include "atomic.h"
 #define DEBUG_MSG  ATOMIC_MSG
#else
 #define DEBUG_MSG(msg)  {} //noop
#endif


//flow control:
class Signal
{
    VOLATILE std::mutex m_mutex;
    std::condition_variable m_condvar;
    std::unique_lock</*VOLATILE std::mutex*/ decltype(m_mutex)>* m_lock; //needed for wait on cond var; otherwise used as a flag
//        typedef /*typename*/ /*std::decay<decltype(m_lock)>::type*/ std::decay<decltype(*m_lock)> lock_type;
public: //ctor/dtor
    Signal(): m_lock(0) { DEBUG_MSG(BLUE_MSG << "Signal ctor" << ENDCOLOR); }
    ~Signal() { if (islocked()) unlock(); DEBUG_MSG(BLUE_MSG << "Signal dtor" << ENDCOLOR); }
public: //members
    inline bool islocked() const { return m_lock; }
    void lock()
    {
//no: okay for mult lock reqs; that's why it's needed        if (islocked()) throw std::runtime_error(RED_MSG "already locked" ENDCOLOR_NOLINE);
//            m_lock = new lock_type(m_mutex); //std::unique_lock<VOLATILE std::mutex>(ptr->m_mutex);
//no worky; compiler bug?            m_lock = new std::decay<decltype(*m_lock)>(m_mutex);
        DEBUG_MSG(BLUE_MSG << "Signal: attempt lock, is locked? " << islocked() << ENDCOLOR);
        auto new_lock = new std::unique_lock<VOLATILE std::mutex>(m_mutex);
        if (islocked()) throw std::runtime_error(RED_MSG "already locked" ENDCOLOR_NOLINE); //bug
        m_lock = new_lock;
    }
    void wait(int poll = 0)
    {
        DEBUG_MSG(BLUE_MSG << "Signal: wait, is locked? " << islocked() << ", poll? " << poll << ENDCOLOR);
        if (/*!poll ||*/ islocked()) { decltype(m_lock) sv_lock = m_lock; m_lock = 0; m_condvar.wait(*sv_lock); m_lock = sv_lock; } //scoped_lock);
        else sleep_msec(std::max(poll, 1)); //cooperative multi-tasking (caller must update queue)
    }
    void notify(bool broadcast = false)
    {
        DEBUG_MSG(BLUE_MSG << "Signal: notify, broadcast? " << broadcast << ", is locked? " << islocked() << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
    }
    void unlock()
    {
        DEBUG_MSG(BLUE_MSG << "Signal: attempt unlock, is locked? " << islocked() << ENDCOLOR);
        if (!islocked()) throw std::runtime_error(RED_MSG "not locked" ENDCOLOR_NOLINE);
        auto sv_lock = m_lock;
        m_lock = 0; //update while still locked
        delete sv_lock;
    }
//private:
public: //helpers
//    static void yield() { sleep_msec(1); }
//polling needed for single-threaded caller:
    static void sleep_msec(int msec) { std::this_thread::sleep_for(std::chrono::milliseconds(msec)); }
};


#if 0
//send or wait for a signal (cond + mutex):
//uses FIFO of messages to compensate for SDL_cond limitations
class Signal
{
private:
    std::vector<void*> pending; //SDL discards signal if nobody waiting (CondWait must occur before CondSignal), so remember it
    auto_ptr<SDL_cond> cond;
//TODO: use smart_ptr with ref count
    static auto_ptr<SDL_mutex> mutex; //assume low usage, share across all signals
    static int count;
//NO-doesn't make sense to send-then-rcv immediately, and rcv-then-send needs processing in between:
//    enum Direction {SendOnly = 1, RcvOnly = 2, Both = 3};
public:
    Signal() //: cond(NULL)
    {
        myprintf(22+10, BLUE_MSG "sig ctor: count %d, m 0x%x, c 0x%x" ENDCOLOR, count, toint(mutex.cast), toint(cond.cast));
        if (!count++)
            if (!(mutex = SDL_CreateMutex())) exc(RED_MSG "Can't create signal mutex" ENDCOLOR); //throw SDL_Exception("SDL_CreateMutex");
        if (!(cond = SDL_CreateCond())) exc(RED_MSG "Can't create signal cond" ENDCOLOR); //throw SDL_Exception("SDL_CreateCond");
        myprintf(22+10, YELLOW_MSG "signal 0x%x has m 0x%x, c 0x%x" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast));
    }
    ~Signal() { if (!--count) mutex = NULL; }
public:
    void* wait()
    {
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
    }
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
};
auto_ptr<SDL_mutex> Signal::mutex;
int Signal::count = 0;
#endif

#undef DEBUG_MSG
#endif //ndef _SIGNAL_H


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


//int main(int argc, const char* argv[])
void unit_test()
{
//TODO

//    return 0;
}
#endif //def WANT_UNIT_TEST
//eof