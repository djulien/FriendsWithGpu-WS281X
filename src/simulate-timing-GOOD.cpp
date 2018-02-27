//from http://en.cppreference.com/w/cpp/thread/condition_variable
//to compile:
//  g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fno-exceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 simulate-timing.cpp -o simulate-timing

#include <iostream>
#include <sstream>
#include <string>
#include <string.h> //strlen
#include <thread>
#include <mutex>
#include <condition_variable>
//#include <unistd.h> //getpid
#include <vector>

#include <chrono>
//#include <thread>
//https://stackoverflow.com/questions/4184468/sleep-for-milliseconds
#define sleep_msec(msec)  std::this_thread::sleep_for(std::chrono::milliseconds(msec))

#define WANT_DETAILS  false //true


template<class TYPE>
class vector_ex: public std::vector<TYPE>
{
public:
    int find(const TYPE& that)
    {
//        for (auto& val: *this) if (val == that) return &that - this;
        for (auto it = this->begin(); it != this->end(); ++it)
            if (*it == that) return it - this->begin();
        return -1;
    }
    std::string join(const char* sep = ",", const char* if_empty = "")
    {
        std::stringstream buf;
        for (auto it = this->begin(); it != this->end(); ++it)
            buf << sep << *it;
//        return this->size()? buf.str().substr(strlen(sep)): buf.str();
        if (!this->size()) buf << sep << if_empty; //"(empty)";
        return buf.str().substr(strlen(sep));
    }
};


std::mutex atomic_mut;
//use macro so stmt can be nested within scoped lock:
#define ATOMIC(stmt)  { std::unique_lock<std::mutex> lock(atomic_mut); stmt; }


#define TOSTR(str)  TOSTR_NESTED(str)
#define TOSTR_NESTED(str)  #str

//ANSI color codes (for console output):
//https://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_COLOR(code)  "\x1b[" code "m"
#define RED_MSG  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_MSG  ANSI_COLOR("1;32")
#define YELLOW_MSG  ANSI_COLOR("1;33")
#define BLUE_MSG  ANSI_COLOR("1;34")
#define MAGENTA_MSG  ANSI_COLOR("1;35")
#define PINK_MSG  MAGENTA_MSG
#define CYAN_MSG  ANSI_COLOR("1;36")
#define GRAY_MSG  ANSI_COLOR("0;37")
//#define ENDCOLOR  ANSI_COLOR("0")
//append the src line# to make debug easier:
#define ENDCOLOR_ATLINE(n)  " &" TOSTR(n) ANSI_COLOR("0") "\n"
#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(__LINE__)


//custom stream manipulator for printf-style formating:
//idea from https://stackoverflow.com/questions/535444/custom-manipulator-for-c-iostream
//and https://stackoverflow.com/questions/11989374/floating-point-format-for-stdostream
//std::ostream& fmt(std::ostream& out, const char* str)
#include <stdio.h> //snprintf
class FMT
{
public:
    explicit FMT(const char* fmt): m_fmt(fmt) {}
private:
    class fmter //actual worker class
    {
    public:
        explicit fmter(std::ostream& strm, const FMT& fmt): m_strm(strm), m_fmt(fmt.m_fmt) {}
//output next object (any type) to stream:
        template<typename TYPE>
        std::ostream& operator<<(const TYPE& value)
        {
//            return m_strm << "FMT(" << m_fmt << "," << value << ")";
            char buf[40]; //enlarge as needed
            snprintf(buf, sizeof(buf), m_fmt, value);
            return m_strm << buf;
        }
    private:
        std::ostream& m_strm;
        const char* m_fmt;
    };
    const char* m_fmt; //save fmt string for inner class
//kludge: return derived stream to allow operator overloading:
    friend FMT::fmter operator<<(std::ostream& strm, const FMT& fmt)
    {
        return FMT::fmter(strm, fmt);
    }
};


//std::chrono::duration<double> elapsed()
double elapsed_msec()
{
    static auto started = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
//    std::cout << "f(42) = " << fibonacci(42) << '\n';
//    auto end = std::chrono::system_clock::now();
//     std::chrono::duration<double> elapsed_seconds = end-start;    
//    long sec = std::chrono::system_clock::now() - started;
    auto now = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
//https://stackoverflow.com/questions/14391327/how-to-get-duration-as-int-millis-and-float-seconds-from-chrono
//http://en.cppreference.com/w/cpp/chrono
//    std::chrono::milliseconds msec = std::chrono::duration_cast<std::chrono::milliseconds>(fs);
//    std::chrono::duration<float> duration = now - started;
//    float msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
#if 0
    typedef std::chrono::milliseconds ms;
    typedef std::chrono::duration<float> fsec;
    fsec fs = now - started;
    ms d = std::chrono::duration_cast<ms>(fs);
    std::cout << fs.count() << "s\n";
    std::cout << d.count() << "ms\n";
    return d.count();
#else
    std::chrono::duration<double, std::milli> elapsed = now - started;
//    std::cout << "Waited " << elapsed.count() << " ms\n";
    return elapsed.count();
#endif
}


std::string timestamp()
{
    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
    float x = 1.2;
    int h = 42;
    ss << FMT("[%4.3f msec] ") << elapsed_msec();
    return ss.str();
}


//busy wait:
//NOTE: used to simulate real CPU work; cen't use blocking wait() function
void busy_msec(int msec)
{
//NO    for (double started = elapsed_msec(); elapsed_msec() < started + msec; ) //NOTE: can't use elapsed time; allows overlapped time slices
    while (msec-- > 0)
        for (volatile int i = 0; i < 300000; ++i); //NOTE: need volatile to prevent optimization; need busy loop to occupy CPU; sleep system calls do not consume CPU time (allows threads to overlap)
}


//int pending = 0;
//std::vector<int> pending;
std::mutex mut;
std::condition_variable main_wake, wker_wake;
#if 0
std::string data;
bool ready = false;
int processed = 0;
//#define THRID  std::hex << std::this_thread::get_id() //https://stackoverflow.com/questions/19255137/how-to-convert-stdthreadid-to-string-in-c
#endif
vector_ex<int> pending;
int frnum = 0;
int main_waker = -1, wker_waker = -1;


//convert to terse int:
int thrid(bool locked = false)
{
//    auto thrid = std::this_thread::get_id();
    if (!locked)
    {
        std::unique_lock<std::mutex> lock(mut);
        return thrid(true);
    }

    static vector_ex<std::thread::id> ids;
//        std::lock_guard<std::mutex> lock(m);
    auto id = std::this_thread::get_id();
    int ofs = ids.find(id);
    if (ofs == -1) { ofs = ids.size(); ids.push_back(id); }
//    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    ss << ofs;
//    return ss.str();
    return ofs;
}


#define WKER_MSG(color, msg)  ATOMIC(std::cout << color << timestamp() << "wker " << myid << " " << msg << ENDCOLOR << std::flush)
#define WKER_DELAY  20
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
 

//class ThreadSafe: public std::basic_ostream<char>
//{
//public:
//    ThreadSafe(std::ostream& strm): std::basic_ostream<char>(strm) {};
//};
#define MAIN_MSG(color, msg)  ATOMIC(std::cout << color << timestamp() << FMT("main[%d]") << frnum << " " << msg << ENDCOLOR << std::flush)
#define NUM_WKERs  4
#define DURATION  10
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

//eof