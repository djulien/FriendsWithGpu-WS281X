#!/bin/bash -x
#g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o simulate-timing  -x c++ - <<//EOF
echo -ne "\n\n\n" > src.cpp; cat <</EOF >> src.cpp; g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o simulate-timing  src.cpp
//simulate multi-threaded (multi-process) timing
//self-compiling c++ file; run this file to compile it; //https://stackoverflow.com/questions/17947800/how-to-compile-code-from-stdin?lq=1
//or, to compile manually:
//  g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 simulate-timing.cpp -o simulate-timing

//GDB in GUI mode: gdb -tui  exe-name
//quick reference to TUI commands: http://beej.us/guide/bggdb/#qref
// layout src   or  split
// info b
// disp var
// printf "%x\n", var


#include <iostream>
//#include <sstream>
//#include <string>
//#include <string.h> //strlen
//#include <thread>
//#include <exception>
//#include <unistd.h> //getpid

#define WANT_DETAILS  true //false //true

#include "colors.h"
#include "ostrfmt.h"
#include "elapsed.h"
#include "vectorex.h"
#include "atomic.h"
//#define SHM_KEY  0x4567feed
#include "msgque.h"
//#define new_SHM(ignore)  new


#include <chrono>
//#include <thread>
//https://stackoverflow.com/questions/4184468/sleep-for-milliseconds
#define sleep_msec(msec)  std::this_thread::sleep_for(std::chrono::milliseconds(msec))


//busy wait:
//NOTE: used to simulate real CPU work; cen't use blocking wait() function
//NOTE: need to calibrate to cpu speed
void busy_msec(int msec)
{
//NO    for (double started = elapsed_msec(); elapsed_msec() < started + msec; ) //NOTE: can't use elapsed time; allows overlapped time slices
    while (msec-- > 0)
        for (volatile int i = 0; i < 300000; ++i); //NOTE: need volatile to prevent optimization; need busy loop to occupy CPU; sleep system calls do not consume CPU time (allows threads to overlap)
}


#define NUM_WKERs  4 //8
#define SHM_KEY  0x4567feed
#include <thread>

#ifdef SHM_KEY //multi-process
 #include "shmalloc.h"
 #include "ipcthread.h"
 #define THREAD  IpcThread
 #define THIS_THREAD  IpcThread

// vector_ex<THREAD::id> size_calc(NUM_WKERs); //dummy var to calculate size of shared memory needed
 struct { vector_ex<THREAD::id> vect; THREAD::id ids[NUM_WKERs]; } size_calc;
 #define SHMLEN  (SHMHDR_LEN + 2 * SHMVAR_LEN(MsgQue) + SHMVAR_LEN(size_calc))
 ShmHeap /*ShmHeapAlloc::*/shmheap(SHMLEN, ShmHeap::persist::NewPerm, 0x4567feed);
//int pending = 0;
//std::vector<int> pending;
//std::mutex mut;
//std::condition_variable main_wake, wker_wake;
//Signal main_wake, wker_wake;
//MsgQue mainq("mainq"), wkerq("wkerq");
//ShmObject<MsgQue> mainq("mainq"), wkerq("wkerq");
//MsgQue& mainq = *new (shmheap.alloc(sizeof(MsgQue), __FILE__, __LINE__)) MsgQue("mainq");
//lamba functions: http://en.cppreference.com/w/cpp/language/lambda
 MsgQue& mainq = SHARED(SRCKEY, MsgQue, MsgQue("mainq", shmheap.mutex()));
 MsgQue& wkerq = SHARED(SRCKEY, MsgQue, MsgQue("wkerq", mainq.mutex()));
//MsgQue& mainq = shared<MsgQue>(SRCKEY, []{ return new shared<MsgQue>("mainq"); });
//MsgQue& mainq = *new_SHM(0) MsgQue("mainq");
//MsgQue& wkerq = *new_SHM(0) MsgQue("wkerq");
//MsgQue& wkerq = *new () MsgQue("wkerq");
//#define new_SHM(...)  new(__FILE__, __LINE__, __VA_ARGS__)

//ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, 0x4567feed);
//ShmMsgQue& mainq = *new ()("mainq"), wkerq("wkerq");
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new_SHM(+j) Complex (i, j); //kludge: force unique key for shmalloc
#else //multi-thread
 #define THREAD  std::thread
 #define THIS_THREAD  std::this_thread
// MsgQue& mainq = MsgQue("mainq"); //TODO
// MsgQue& wkerq = MsgQue("wkerq"); //TODO
 MsgQue mainq("mainq"), wkerq("wkerq", mainq.mutex());
//MsgQue& mainq = *new /*(__FILE__, __LINE__, true)*/ MsgQue("mainq");
//MsgQue& wkerq = *new /*(__FILE__, __LINE__, true)*/ MsgQue("wkerq");
#endif


#include <unistd.h> //fork, getpid

//convert thread id to terse int:
int thrid() //bool locked = false)
{
//    auto thrid = std::this_thread::get_id();
//    if (!locked)
//    {
    auto id = THIS_THREAD::get_id(); //std::this_thread::get_id();
    ATOMIC(std::cout << CYAN_MSG << "pid '" << getpid() << FMT("', thread id 0x%lx") << id << ENDCOLOR << std::flush);
//#ifdef SHM_KEY
//    return id;
//#endif
#ifdef SHM_KEY //need shared vector to make thread ids (inx, actually) consistent and unique
    typedef vector_ex<THREAD::id, ShmAllocator<THREAD::id>> vectype;
    static vectype& ids = SHARED(SRCKEY, vectype, vectype);
    std::unique_lock<std::mutex> lock(shmheap.mutex()); //low usage; reuse mutex
#else
    static vector_ex<THREAD::id> ids;
    std::unique_lock<std::mutex> lock(atomic_mut); //low usage; reuse mutex
#endif
//        return thrid(true);
//    }
//        std::lock_guard<std::mutex> lock(m);
    int ofs = ids.find(id);
    if (ofs != -1) throw std::runtime_error(RED_MSG "thrid: duplicate thread id" ENDCOLOR);
    if (ofs == -1) { ofs = ids.size(); ids.push_back(id); }
//    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    ss << ofs;
//    return ss.str();
    return ofs;
}


#if 0
std::string data;
bool ready = false;
int processed = 0;
//#define THRID  std::hex << std::this_thread::get_id() //https://stackoverflow.com/questions/19255137/how-to-convert-stdthreadid-to-string-in-c
#endif
//vector_ex<int> pending;
//int frnum = 0;
//int main_waker = -1, wker_waker = -1;


#define WKER_MSG(color, msg)  ATOMIC(std::cout << color << timestamp() << "wker " << myid << " " << msg << ENDCOLOR << std::flush)
#define WKER_DELAY  20
void wker_main()
{
    int myid = thrid();
    int frnum = 0;
    WKER_MSG(CYAN_MSG, "started, render " << frnum << " for " << WKER_DELAY << " msec");
    for (;;)
    {
        busy_msec(WKER_DELAY); //simulate render()
        if (WANT_DETAILS) WKER_MSG(BLUE_MSG, "rendered " << frnum << ", now notify main");
        mainq.send(1 << myid);

        if (WANT_DETAILS) WKER_MSG(BLUE_MSG, "now wait for next req");
        frnum = wkerq.rcv(&MsgQue::not_wanted, frnum);
        WKER_MSG(BLUE_MSG, "woke with req for " << frnum << ", now render for " << WKER_DELAY << " msec");
        if (frnum < 0) break;
    }
    WKER_MSG(CYAN_MSG, "quit");
}
#if 0 //explicit mutex + cond var
void wker_main()
{
    int myid = thrid();
//    std::thread::id thid = std::this_thread::get_id();
//    std::string thid = std::to_string((long)std::this_thread::get_id());
    WKER_MSG(CYAN_MSG, "started, render " << frnum << " for " << WKER_DELAY << " msec");
    for (;;)
    {
        int seen = frnum;
//        sleep_msec(WKER_DELAY); //simulate render()
        busy_msec(WKER_DELAY); //simulate render()
        if (WANT_DETAILS) WKER_MSG(BLUE_MSG, "rendered " << frnum << ", now notify main");
        { //scope for lock
            std::unique_lock<std::mutex> lock(mut);
//            std::lock_guard<std::mutex> lock(mut);
//            ++pending;
            pending.push_back(myid);
            if (pending.size() == 4) WKER_MSG(BLUE_MSG, "last wker to render");
            main_waker = myid;
//        }
        main_wake.notify_one(); //wake main

        if (WANT_DETAILS) WKER_MSG(BLUE_MSG, "now wait for next req");
//        { //scope for lock
//            std::unique_lock<std::mutex> lock(mut);
            wker_wake.wait(lock, [&myid, &seen]{ if (WANT_DETAILS) WKER_MSG(YELLOW_MSG, "waker " << wker_waker << ", seen " << frnum << "? " << (frnum == seen)); return frnum != seen; }); //NOTE: spurious wakeups can occur
        }
        WKER_MSG(BLUE_MSG, "woke with req for " << frnum << ", now render for " << WKER_DELAY << " msec");
        if (frnum < 0) break;
    }
    WKER_MSG(CYAN_MSG, "quit");
#if 0
    // Wait until main() sends data
    std::unique_lock<std::mutex> lock(mut);
    std::cout << "wker '" << thrid(true) << "' waiting\n";
    cv.wait(lock, []{return ready;});
 
    // after the wait, we own the lock.
    std::cout << "wker '" << thrid(true) << "' woke up\n";
    data += ", wker " + thrid(true);
 
    // Send data back to main()
    ++processed;
//    std::cout << "Worker thread signals data processing completed\n";
 
    // Manual unlocking is done before notifying, to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    cv.notify_one();
#endif
}
#endif 


//class ThreadSafe: public std::basic_ostream<char>
//{
//public:
//    ThreadSafe(std::ostream& strm): std::basic_ostream<char>(strm) {};
//};
#define MAIN_MSG(color, msg)  ATOMIC(std::cout << color << timestamp() << FMT("main[%d]") << frnum << " " << msg << ENDCOLOR << std::flush)
//#define NUM_WKERs  4 //8
#define DURATION  10 //100
#define MAIN_ENCODE_DELAY  10
#define MAIN_PRESENT_DELAY  33 //100
int main()
{
    int myid = thrid();
#if 0 //calibrate
    std::cout << timestamp() << "start\n";
    for (volatile int i = 0; i < 200000; ++i); //NOTE: need volatile to prevent optimization; system calls allow threads to overlap; need real busy wait loop here
    std::cout << timestamp() << "end\n";
    return 0;
#endif
#if 0 //timing test
    MAIN_MSG(PINK_MSG, "start, work for 10 msec");
    busy_msec(10);
    MAIN_MSG(PINK_MSG, "after 10 msec, work for 1 sec");
    busy_msec(1000);
    MAIN_MSG(PINK_MSG, "after 1 sec");
    return 0;
#endif
    int frnum = 0;
    std::vector<THREAD> wkers;
    MAIN_MSG(CYAN_MSG, "thread " << myid << " launch " << NUM_WKERs << " wkers, " << FMT("&mainq = %p") << (long)&mainq << << FMT(", &wkerq = %p") << (long)&wkerq);
//    pending = 10;
    for (int n = 0; n < NUM_WKERs; ++n) wkers.emplace_back(wker_main);
    MAIN_MSG(PINK_MSG, "launched " << wkers.size() << " wkers");
//    for (int fr = 0; fr < 10; ++fr) //duration
    for (;;)
    {
        if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "now wait for replies");
//        const char* status = "on time";
        mainq.rcv(&MsgQue::wanted, ((1 << NUM_WKERs) - 1) << 1, true); //wait for all wkers to respond (blocking)
        MAIN_MSG(PINK_MSG, "all wkers ready, now encode() for " << MAIN_ENCODE_DELAY << " msec");
        busy_msec(MAIN_ENCODE_DELAY); //simulate encode()
        if (++frnum >= DURATION) frnum = -1; //break;
        const char* status = (frnum < 0)? "quit": "frreq";
        /*if (WANT_DETAILS)*/ MAIN_MSG(PINK_MSG, "encoded, now notify wkers (" << status << ") and finish render() for " << MAIN_PRESENT_DELAY << " msec");
        wkerq.clear().send(frnum, true); //wake *all* wkers
//        MAIN_MSG(PINK_MSG, "notified wkers (" << status << "), now finish render() for " << MAIN_PRESENT_DELAY << " msec");
        sleep_msec(MAIN_PRESENT_DELAY); //simulate render present()
        if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "presented");
        if (frnum < 0) break;
    }
    for (auto& w: wkers) w.join(); //    std::vector<std::thread> wkers;
    MAIN_MSG(CYAN_MSG, "quit");
    wkerq.clear(); //leave queue in empty state (benign; avoid warning)
}
#if 0 //explicit mutex + cond var
int main()
{
    int myid = thrid();
#if 0 //calibrate
    std::cout << timestamp() << "start\n";
    for (volatile int i = 0; i < 200000; ++i); //NOTE: need volatile to prevent optimization; system calls allow threads to overlap; need real busy wait loop here
    std::cout << timestamp() << "end\n";
    return 0;
#endif
//    int val = 42;
//    std::cout << val << " in hex is " << FMT(" 0x%x") << val << "\n";
//    std::cout << timestamp() << "main started\n";
#if 0 //timing test
    MAIN_MSG(PINK_MSG, "start, work for 10 msec");
    busy_msec(10);
    MAIN_MSG(PINK_MSG, "after 10 msec, work for 1 sec");
    busy_msec(1000);
    MAIN_MSG(PINK_MSG, "after 1 sec");
    return 0;
#endif
//    std::stringstream stream; // #include <sstream> for this
//    stream << FMT(" <%3d> ") << 1 << 2 << FMT(" (%2.1f) ") << 3. << "\n" << std::flush;
//    std::cout << stream.str();
//    std::cout << (std::stringstream() << 1 << 2 << 3 << "\n");
//    std::cout << FMT(" <%3d> ") << 1 << 2 << FMT(" (%2.1f) ") << 3.1 << "\n" << std::flush;

//    std::cout << sizeof(std::thread::id) << "\n";
//    std::string thid = std::to_string((long)std::this_thread::get_id());

//    std::thread worker1(worker_thread);
//    std::thread worker2(worker_thread);
    std::vector<std::thread> wkers;
    MAIN_MSG(CYAN_MSG, "thread " << myid << " launch " << NUM_WKERs << " wkers");
//    pending = 10;
    for (int n = 0; n < NUM_WKERs; ++n) wkers.emplace_back(wker_main);
    MAIN_MSG(PINK_MSG, "launched " << wkers.size() << " wkers");
#if 0
    data = "data";
    // send data to the worker thread
    {
        std::lock_guard<std::mutex> lock(mut);
        ready = true;
    }
    std::cout << "main '" << thrid(false) << "' notify data ready\n";
    cv.notify_one();
 
    // wait for the worker
    {
        std::unique_lock<std::mutex> lock(mut);
        std::cout << "main '" << thrid(true) << "' wait for reply\n";
        cv.wait(lock, [&wkers]{return processed == wkers.size();});
    }

    std::cout << "main '" << thrid(true) << "' woke up\n";
    std::cout << "main '" << thrid(false) << "', data = " << data << '\n';
#else
//    for (int fr = 0; fr < 10; ++fr) //duration
    for (;;)
    {
        if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "now wait for replies");
        const char* status = "on time";
        { //scope for lock
            std::unique_lock<std::mutex> lock(mut);
            if (pending.size() != wkers.size())
            {
                status = "waited";
                main_wake.wait(lock, [&wkers]{ if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "waker " << main_waker << ", got " << pending.size() << " replies from: " << pending.join(",", "(empty)")); return pending.size() == wkers.size(); }); //wait for all wkers; NOTE: spurious wakeups can occur
            }
        }
        MAIN_MSG(PINK_MSG, "all wkers ready (" << status << "), now encode() for " << MAIN_ENCODE_DELAY << " msec");
//        sleep_msec(MAIN_ENCODE_DELAY); //simulate encode()
        busy_msec(MAIN_ENCODE_DELAY); //simulate encode()

//        pending = 0;
        pending.clear();
        if (++frnum >= DURATION) break;
        if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "encoded, now notify wkers");
        wker_waker = myid;
        wker_wake.notify_all(); //wake *all* wkers
        MAIN_MSG(PINK_MSG, "notified wkers, now finish render() for " << MAIN_PRESENT_DELAY << " msec");
        sleep_msec(MAIN_PRESENT_DELAY); //simulate render present()

        if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "presented");
    }
#endif
    frnum = -1;
    wker_waker = myid;
    wker_wake.notify_all(); //wake *all* wkers
    MAIN_MSG(BLUE_MSG, "send quit req");

//    worker1.join();
//    worker2.join();
    for (auto& w: wkers) w.join();
    MAIN_MSG(CYAN_MSG, "quit");
}
#endif

//eof