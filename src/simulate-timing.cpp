#!/bin/bash -x
echo -e '\e[1;36m'; OPT=3; BIN=${BASH_SOURCE%.*}; rm -f $BIN; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O$OPT -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++14 -o "$BIN" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)

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

#define NUM_WKERs  +4 //8 //use >0 for in-proc threads, <0 for ipc threads
#define WANT_DETAILS  true //false //true
#define MSGQUE_DEBUG
#define IPC_DEBUG
#define MSGQUE_DETAILS  true

//#define ABS(n)  (((n) < 0)? -(n): (n))


//#include <iostream>
//#include <sstream>
//#include <string>
//#include <string.h> //strlen
//#include <exception>
//#include <unistd.h> //getpid

#define WANT_IPC  (NUM_WKERs < 0)
#if WANT_IPC
 #define IF_IPC(stmt)  { stmt }
#else
 #define IF_IPC(stmt)  {} //noop
#endif
#include "msgcolors.h" //SrcLine, msg colors
#include "ostrfmt.h" //FMT()
#include "vectorex.h"
//#include "shmkeys.h"
//#define SHM_KEY  0x4567feed
//these ones need to come after ipc.h (#def IPC_THREAD):
#include "elapsed.h" //timestamp()
#include "atomic.h" //ATOMIC_MSG()
//#define SHM_KEY  0x4567feed
#include "msgque.h"
//#define new_SHM(ignore)  new
//#if WANT_IPC //NUM_WKERs < 0 //ipc
// #include "ipc.h" //no-needs to come first (to request Ipc versions of other classes)
//#endif
#include "ipc.h"


#include <chrono>
#include <thread>
//https://stackoverflow.com/questions/4184468/sleep-for-milliseconds
#define sleep_msec(msec)  std::this_thread::sleep_for(std::chrono::milliseconds(msec))


//busy wait:
//NOTE: used to simulate real CPU work; cen't use blocking wait() function
//NOTE: need to calibrate to cpu speed
void busy_msec(int msec, SrcLine srcline = 0)
{
//NO    for (double started = elapsed_msec(); elapsed_msec() < started + msec; ) //NOTE: can't use elapsed time; allows overlapped time slices
    ATOMIC_MSG(BLUE_MSG << timestamp() << "busy wait " << msec << " msec" << ENDCOLOR_ATLINE(srcline));
    while (msec-- > 0)
        for (volatile int i = 0; i < 300000; ++i); //NOTE: need volatile to prevent optimization; need busy loop to occupy CPU; sleep system calls do not consume CPU time (allows threads to overlap)
}


//#ifdef SHM_KEY //multi-process
#if 0
#if WANT_IPC //NUM_WKERs < 0 //ipc
// #include "shmalloc.h"
// #include "ipc.h" //needs to come first (to request Ipc versions of other classes)
 #define THREAD  IpcThread
 #define THIS_THREAD  IpcThread

// vector_ex<THREAD::id> size_calc(NUM_WKERs); //dummy var to calculate size of shared memory needed
// struct { vector_ex<THREAD::id> vect; THREAD::id ids[ABS(NUM_WKERs)]; } size_calc;
// #define SHMLEN  (SHMHDR_LEN + 2 * SHMVAR_LEN(MsgQue) + SHMVAR_LEN(size_calc))
// ShmHeap ShmHeapAlloc::shmheap(SHMLEN, ShmHeap::persist::NewPerm, 0x4567feed);
//int pending = 0;
//std::vector<int> pending;
//std::mutex mut;
//std::condition_variable main_wake, wker_wake;
//Signal main_wake, wker_wake;
//MsgQue mainq("mainq"), wkerq("wkerq");
//ShmObject<MsgQue> mainq("mainq"), wkerq("wkerq");
//MsgQue& mainq = *new (shmheap.alloc(sizeof(MsgQue), __FILE__, __LINE__)) MsgQue("mainq");
//lamba functions: http://en.cppreference.com/w/cpp/language/lambda
// MsgQue& mainq = SHARED(SRCKEY, MsgQue, MsgQue("mainq", ShmHeapAlloc::shmheap.mutex()));
// MsgQue& wkerq = SHARED(SRCKEY, MsgQue, MsgQue("wkerq", mainq.mutex()));
// ShmScope<MsgQue> mscope(SRCLINE, SHMKEY1, "mainq"), wscope(SRCLINE, SHMKEY2, "wkerq"); //, mainq.mutex());
// MsgQue& mainq = mscope.shmobj.data;
// MsgQue& wkerq = wscope.shmobj.data;
//ShmPtr_params settings1(SRCLINE, SHMKEY1, 0, false);
//ShmPtr<MsgQue> mainq("mainq");
/*ShmPtr<MsgQue>*/ MsgQue mainq(PARAMS {_.name = "mainq"; _.shmkey = SHMKEY1; /*_.extra = 0*/; _.want_reinit = false; });
//ShmPtr_params settings2(SRCLINE, SHMKEY2, 0, false);
//ShmPtr<MsgQue> wkerq("wkerq");
/*ShmPtr<MsgQue>*/ MsgQue mainq(PARAMS {_.name = "wkerq"; _.shmkey = SHMKEY2; /*_.extra = 0*/; _.want_reinit = false; });
//MsgQue& mainq = shared<MsgQue>(SRCKEY, []{ return77', t new shared<MsgQue>("mainq"); });
//MsgQue& mainq = *new_SHM(0) MsgQue("mainq");
//MsgQue& wkerq = *new_SHM(0) MsgQue("wkerq");
//MsgQue& wkerq = *new () MsgQue("wkerq");
//#define new_SHM(...)  new(__FILE__, __LINE__, __VA_ARGS__)

//ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, 0x4567feed);
//ShmMsgQue& mainq = *new ()("mainq"), wkerq("wkerq");
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new_SHM(+j) Complex (i, j); //kludge: force unique key for shmalloc
 template <> const char* WithMutex<vector_ex<int, std::allocator<int>>, true>::TYPENAME() { return "WithMutex<vector_ex<int, std::allocator<int>>, true>"; }
#else //multi-thread (single process)
 #include <thread> //std::this_thread
 #define THREAD  std::thread
 #define THIS_THREAD  std::this_thread
// MsgQue& mainq = MsgQue("mainq"); //TODO
// MsgQue& wkerq = MsgQue("wkerq"); //TODO
// MsgQue mainq("mainq"), wkerq("wkerq"); //, mainq.mutex());
 MsgQue mainq(PARAMS {_.name = "mainq"; }), wkerq(PARAMS {_.name = "wkerq"; }), 
//MsgQue& mainq = *new /*(__FILE__, __LINE__, true)*/ MsgQue("mainq");
//MsgQue& wkerq = *new /*(__FILE__, __LINE__, true)*/ MsgQue("wkerq");
// #ifndef IPC_THREAD
//dummy object wrapper (emulates shmem)
#if 0
template <typename TYPE, int NUM_INST = 1>
class ShmScope
{
    typedef struct { int count; TYPE data; } type; //count #ctors, #dtors (ref count)
public: //ctor/dtor
//    PERFECT_FWD2BASE_CTOR(shared, SHOBJ_TYPE&) {}
//#define thread  IpcThread::all[0]
    template<typename ... ARGS>
    explicit ShmScope(/*IpcThread& thread,*/ SrcLine srcline = 0, ARGS&& ... args): shmobj(*(type*)malloc_once(sizeof(type))) //, thread->isParent()? 0: thread->rcv(srcline), srcline))//, m_isparent(thread->isParent())
    {
//        ATOMIC_MSG(BLUE_MSG << timestamp() << (m_isparent? "parent": "child") << " scope ctor" << ENDCOLOR_ATLINE(srcline));
        ATOMIC_MSG(BLUE_MSG << timestamp() << "scope ctor# " << shmobj.count << "/" << (2 * NUM_INST) << FMT(", &obj %p") << & shmobj << ENDCOLOR_ATLINE(srcline));
//        if (!m_isparent) return;
        if (shmobj.count++) return; //not first (parent)
        new (&shmobj.data) TYPE(std::forward<ARGS>(args) ...); //, srcline); //call ctor to init (parent only)
//        thread->send(shmkey(&shmobj), srcline); //send shmkey to child (parent only)
    }
//#undef thread
    ~ShmScope()
    {
//        ATOMIC_MSG(BLUE_MSG << timestamp() << (m_isparent? "parent": "child") << " scope dtor" << ENDCOLOR);
        ATOMIC_MSG(BLUE_MSG << timestamp() << "scope dtor# " << shmobj.count << "/" << (2 * NUM_INST) << ENDCOLOR);
//        if (!m_isparent) return;
        if (shmobj.count++ < 2 * NUM_INST) return; //not last (could be parent or child)
        shmobj.data.~TYPE();
        free(&shmobj);
    }
public: //wrapped object (ref to shared obj)
    type& shmobj;
private:
//always return same address (common to all threads):
    void* malloc_once(size_t size)
    {
        static void* ptr = safe_memset(malloc(size), 0, size); //wrapped to avoid needing separate template<> decl later
//        if (ptr && first) { memset(ptr, 0, size); first = false; } //new shmem is zeroed, so clear heap mem as well
        return ptr;
    }
    void* safe_memset(void* ptr, int val, size_t count)
    {
        if (ptr) memset(ptr, val, count);
        return ptr;
    }
//    bool m_isparent;
//    bool isparent() { return (std::this_thread::get_id() == MAIN_THREAD_ID); } //from https://stackoverflow.com/questions/33869852/how-to-find-out-if-we-are-running-in-main-thread
//    static std::thread::id MAIN_THREAD_ID;
//    static TYPE shared_obj;
};
#endif
//template <> std::thread::id ShmScope::MAIN_THREAD_ID = std::this_thread::get_id();
//template <>  std::thread::id ShmScope::MAIN_THREAD_ID = std::this_thread::get_id();
template<> const char* WithMutex<vector_ex<std::thread::id>>::TYPENAME() { return "WithMutex<vector_ex<std::thread::id>>"; }
// #endif //def IPC_THREAD
#endif
#endif


#if 0
//convert thread/procid to terse int:
#include <unistd.h> //getpid()
#include "critical.h"
int thrid() //bool locked = false)
{
//    auto thrid = std::this_thread::get_id();
//    if (!locked)
//    {
    auto id = THIS_THREAD::get_id(); //std::this_thread::get_id();
//    ATOMIC_MSG(CYAN_MSG << timestamp() << "pid '" << getpid() << FMT("', thread id 0x%lx") << id << ENDCOLOR);
//#ifdef SHM_KEY
//    return id;
//#endif
//#ifdef SHM_KEY //need shared vector to make thread ids (inx, actually) consistent and unique
//    typedef vector_ex<THREAD::id, ShmAllocator<THREAD::id>> vectype;
//    static vectype& ids = SHARED(SRCKEY, vectype, vectype);
//    std::unique_lock<std::mutex> lock(ShmHeapAlloc::shmheap.mutex()); //low usage; reuse mutex
//#else
//    ShmScope<type> scope(SRCLINE, "testobj", SRCLINE); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    type& testobj = scope.shmobj; //ShmObj<TestObj>("testobj", thread, SRCLINE);
//    typedef WithMutex<vector_ex<THREAD::id>> type;
    typedef vector_ex<THREAD::id> type; //TODO: use decltype(id)
//    static vector_ex<THREAD::id> ids(ABS(NUM_WKERs)); //preallocate space
//    std::unique_lock<std::mutex> lock(atomic_mut); //low usage; reuse mutex
//    ShmScope<type, ABS(NUM_WKERs) + 1> scope(SRCLINE, SHMKEY3); //shm obj wrapper; call dtor when goes out of scope (parent only)
    static ShmPtr_params settings(SRCLINE, THRIDS_SHMKEY, (ABS(NUM_WKERs) + 1) * sizeof(THREAD::id), false); //don't need auto-lock due to explicit critical section
    static ShmPtr<type> ids;
//    {
    CriticalSection<SHARED_CRITICAL_SHMKEY> cs(SRCLINE);
    if (!ids->size()) ids->reserve(ABS(NUM_WKERs) + 1); //avoid extraneous copy ctors later
//    }
//    ids->reserve(ABS(NUM_WKERs) + 1); //avoid extraneous copy ctors later; only needs to happen 1x; being lazy: just use wrapped method rather than using a critical section with a condition
//    type& ids = scope.shmobj.data; //ShmObj<TestObj>("testobj", thread, SRCLINE);
//#endif
//        return thrid(true);
//    }
//        std::lock_guard<std::mutex> lock(m);
//NOTE: use op->() for shm safety with ipc
    int ofs = ids->find(id);
    if (ofs != -1) throw std::runtime_error(RED_MSG "thrid: duplicate thread id" ENDCOLOR_NOLINE);
    if (ofs == -1) { ofs = ids->size(); ids->push_back(id); } //ofs = ids.push_and_find(id);
//    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    ss << ofs;
//    return ss.str();
    ATOMIC_MSG(CYAN_MSG << timestamp() << "pid '" << getpid() << FMT("', thread id 0x%lx") << id << " => thr inx " << ofs << ENDCOLOR);
    return ofs;
}
#endif


#if 0
std::string data;
bool ready = false;
int processed = 0;
//#define THRID  std::hex << std::this_thread::get_id() //https://stackoverflow.com/questions/19255137/how-to-convert-stdthreadid-to-string-in-c
#endif
//vector_ex<int> pending;
//int frnum = 0;
//int main_waker = -1, wker_waker = -1;


//#if WANT_IPC //NUM_WKERs < 0 //ipc
#include "shmkeys.h"
MsgQue<NUM_WKERs> mainq(NAMED{ _.name = "mainq"; IF_IPC(_.shmkey = SHMKEY1); SRCLINE; }); ///*_.extra = 0*/; _.want_reinit = false; });
MsgQue<NUM_WKERs> wkerq(NAMED{ _.name = "wkerq"; IF_IPC(_.shmkey = SHMKEY2); SRCLINE; }); ///*_.extra = 0*/; _.want_reinit = false; });
//#else
// MsgQue<NUM_WKERs> mainq(NAMED {_.name = "mainq"; SRCLINE; });
// MsgQue<NUM_WKERs> wkerq(NAMED {_.name = "wkerq"; SRCLINE; });
//#endif


#define WKER_MSG(color, msg)  ATOMIC_MSG(color << timestamp() << "wker " << myinx << " " << msg << ENDCOLOR)
#define WKER_DELAY  20
void wker_main()
{
    sleep(2); //give parent head start
    int myinx = mainq->thrinx(); //MsgQue<NUM_WKERs>::thrinx();
    int frnum = 0;
    WKER_MSG(CYAN_MSG, "started, render " << frnum << " for " << WKER_DELAY << " msec");
    for (;;)
    {
        busy_msec(WKER_DELAY, SRCLINE); //simulate render()
        if (WANT_DETAILS) WKER_MSG(BLUE_MSG, "rendered frnum " << frnum << ", now notify main");
        mainq->send(NAMED{ _.msg = 1 << myinx; SRCLINE; });

        if (WANT_DETAILS) WKER_MSG(BLUE_MSG, "now wait for next req");
        frnum = wkerq->rcv(NAMED{ /*_.unwanted = ~(_.wanted = frnum)*/ _.wanted = ~frnum; SRCLINE; }); //&MsgQue::not_wanted, frnum);
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


//#include "ipc.h" //no-needs to come first (to request Ipc versions of other classes)

//class ThreadSafe: public std::basic_ostream<char>
//{
//public:
//    ThreadSafe(std::ostream& strm): std::basic_ostream<char>(strm) {};
//};
#define MAIN_MSG(color, msg)  ATOMIC_MSG(color << timestamp() << FMT("main[%d]") << frnum << " " << msg << ENDCOLOR)
//#define NUM_WKERs  4 //8
#define DURATION  10 //100
#define MAIN_ENCODE_DELAY  10
#define MAIN_PRESENT_DELAY  33 //100
int main()
{
//    int myid = thrid();
    int myinx = mainq->thrinx();
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
    std::vector<IpcThread<WANT_IPC>> wkers;
    MAIN_MSG(CYAN_MSG, "thread " << myinx << " launch " << NUM_WKERs << " wkers, " << FMT("&mainq = %p") << &mainq << FMT(", &wkerq = %p") << &wkerq);
//    pending = 10;
//http://www.bogotobogo.com/cplusplus/C11/3_C11_Threading_Lambda_Functions.php
//https://stackoverflow.com/questions/41063112/problems-creating-vector-of-threads-in-a-loop?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//https://codereview.stackexchange.com/questions/32817/multithreading-c-loop
    for (int n = 0; n < ABS(NUM_WKERs); ++n)
    {
        IpcThread<WANT_IPC> wker(wker_main);
        wkers.push_back(std::move(wker)); //threads must be moved, not copied; http://thispointer.com/c11-how-to-create-vector-of-thread-objects/
//        MAIN_MSG(BLUE_MSG, "launch wker " << n << "/" << wkers.size());
//broken        wkers.emplace_back(wker_main); //NOTE: sends obj out of scope and calls dtor!
//        MAIN_MSG(BLUE_MSG, "launched wker " << n << "/" << wkers.size());
    }
    MAIN_MSG(PINK_MSG, "launched " << wkers.size() << " wkers");
//    for (int fr = 0; fr < 10; ++fr) //duration
    for (;;)
    {
        if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "now wait for replies");
//        const char* status = "on time";
        mainq->rcv(NAMED{ _.wanted = ((1 << ABS(NUM_WKERs)) - 1) << 1; _.remove = true; SRCLINE; }); //wait for all wkers to respond (blocking)
        MAIN_MSG(PINK_MSG, "all wkers ready, now encode() for " << MAIN_ENCODE_DELAY << " msec");
        busy_msec(MAIN_ENCODE_DELAY, SRCLINE); //simulate encode()
        if (++frnum >= DURATION) frnum = -1; //break;
        const char* status = (frnum < 0)? "quit": "frreq";
        /*if (WANT_DETAILS)*/ MAIN_MSG(PINK_MSG, "encoded, now notify wkers (" << status << ") and finish render() for " << MAIN_PRESENT_DELAY << " msec");
        wkerq->clear()->send(NAMED{ _.msg = frnum; _.broadcast = true; SRCLINE; }); //wake *all* wkers
//        MAIN_MSG(PINK_MSG, "notified wkers (" << status << "), now finish render() for " << MAIN_PRESENT_DELAY << " msec");
        sleep_msec(MAIN_PRESENT_DELAY); //simulate render present()
        if (WANT_DETAILS) MAIN_MSG(PINK_MSG, "presented");
        if (frnum < 0) break;
    }
    for (auto& w: wkers) w.join(); //    std::vector<std::thread> wkers;
    MAIN_MSG(CYAN_MSG, "quit");
    wkerq->clear(); //leave queue in empty state (benign; avoid warning)
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

//EOF
set +x
#if [ $? -eq 0 ]; then
if [ -f $BIN ]; then
    echo -e "\e[1;32m compiled OK, now run it ...\e[0m"
    #clean up shmem objects before running:
    #set +x
    echo -e '\e[1;33m'; awk '/#define/{ if (NF > 2) print "ipcrm -vM " $3; }' < shmkeys.h | bash |& grep -v "invalid key" | awk '/removing shared memory segment/{ print gensub(/^(.*) '\''([0-9]+)'\''\s*$/, "\\1 0x\\2", "g", $0); }'; echo -e '\e[0m'
    $BIN
else
    echo -e "\e[1;31m compile FAILED \e[0m"
fi
#eof