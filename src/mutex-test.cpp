//from http://en.cppreference.com/w/cpp/thread/condition_variable
//to compile:
//  g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fno-exceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 mutex-test.cpp -o mutex-test


#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
//#include <unistd.h> //getpid
#include <vector>

#include <chrono>
//#include <thread>
//https://stackoverflow.com/questions/4184468/sleep-for-milliseconds
#define sleep_msec(msec)  std::this_thread::sleep_for(std::chrono::milliseconds(msec))


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
};


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


std::mutex mut;
std::condition_variable cv;
#if 0
std::string data;
bool ready = false;
int processed = 0;
//#define THRID  std::hex << std::this_thread::get_id() //https://stackoverflow.com/questions/19255137/how-to-convert-stdthreadid-to-string-in-c
#endif


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


int pending = 0;
int frnum = 0;

void wker_main()
{
    int myid = thrid();
//    std::thread::id thid = std::this_thread::get_id();
//    std::string thid = std::to_string((long)std::this_thread::get_id());
    std::cout << CYAN_MSG << timestamp() << "wker " << myid << " started" << ENDCOLOR << std::flush;
    for (;;)
    {
        int seen = frnum;
        sleep_msec(20); //simulate render()
        std::cout << BLUE_MSG << timestamp() << "wker " << myid << " rendered " << frnum << ", now notify main" << ENDCOLOR << std::flush;
        { //scope for lock
            std::unique_lock<std::mutex> lock(mut);
//            std::lock_guard<std::mutex> lock(mut);
            ++pending;
//        }
        cv.notify_one();

        std::cout << BLUE_MSG << timestamp() << "wker " << myid << " now wait for next req" << ENDCOLOR << std::flush;
//        { //scope for lock
//            std::unique_lock<std::mutex> lock(mut);
            cv.wait(lock, [&seen]{ return frnum != seen; });
        }
        std::cout << BLUE_MSG << timestamp() << "wker " << myid << " woke with req for " << frnum << ENDCOLOR << std::flush;
        if (frnum < 0) break;
    }
    std::cout << CYAN_MSG << timestamp() << "wker " << myid << " quit" << ENDCOLOR << std::flush;
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
 

int main()
{
//    int val = 42;
//    std::cout << val << " in hex is " << FMT(" 0x%x") << val << "\n";
//    std::cout << timestamp() << "main started\n";
//    sleep_msec(10); //simulate encode()
//    std::cout << timestamp() << "after 10 msec\n";
//    sleep_msec(1000); //simulate encode()
//    std::cout << timestamp() << "after 1 sec\n";
//    return 0;
    std::stringstream stream; // #include <sstream> for this
    stream << 1 << 2 << 3 << "\n";
    std::cout << stream.str();

    std::cout << ((std::stringstream() << 1 << 2 << 3 << "\n").str());

//    std::cout << sizeof(std::thread::id) << "\n";
//    std::string thid = std::to_string((long)std::this_thread::get_id());

//    std::thread worker1(worker_thread);
//    std::thread worker2(worker_thread);
    std::vector<std::thread> wkers;
    std::cout << CYAN_MSG << timestamp() << "main " << thrid() << " launch wkers" << ENDCOLOR << std::flush;
//    pending = 10;
    for (int n = 0; n < 10; ++n) wkers.emplace_back(wker_main);
    std::cout << PINK_MSG << timestamp() << "main launched " << wkers.size() << " wkers" << ENDCOLOR << std::flush;
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
        std::cout << PINK_MSG << timestamp() << FMT("main[%d]") << frnum << " now wait for replies" << ENDCOLOR << std::flush;
        { //scope for lock
            std::unique_lock<std::mutex> lock(mut);
            cv.wait(lock, [&wkers]{ return pending == wkers.size(); }); //wait for all wkers
        }
        std::cout << PINK_MSG << timestamp() << FMT("main[%d]") << frnum << " all wkers ready, now encode()" << ENDCOLOR << std::flush;
        sleep_msec(10); //simulate encode()

        pending = 0;
        if (++frnum >= 10) break;
        std::cout << PINK_MSG << timestamp() << FMT("main[%d]") << frnum << " encoded, now notify wkers" << ENDCOLOR << std::flush;
        cv.notify_one(); //wake wkers
        std::cout << PINK_MSG << timestamp() << FMT("main[%d]") << frnum << " notified, now finish render()" << ENDCOLOR << std::flush;
        sleep_msec(100); //simulate present()

        std::cout << PINK_MSG << timestamp() << FMT("main[%d]") << frnum << " presented" << ENDCOLOR << std::flush;
    }
#endif
    frnum = -1;
    cv.notify_one(); //wake wkers
    std::cout << CYAN_MSG << timestamp() << "main now quit" << ENDCOLOR << std::flush;

//    worker1.join();
//    worker2.join();
    for (auto& w: wkers) w.join();
}

//eof