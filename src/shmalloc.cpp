//shared memory allocator
//to compile:
// g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 shmalloc.cpp -o shmalloc

//see https://www.ibm.com/developerworks/aix/tutorials/au-memorymanager/index.html
//see also http://anki3d.org/cpp-allocators-for-games/


//custom stream manipulator for printf-style formating:
//idea from https://stackoverflow.com/questions/535444/custom-manipulator-for-c-iostream
//and https://stackoverflow.com/questions/11989374/floating-point-format-for-stdostream
//std::ostream& fmt(std::ostream& out, const char* str)
#include <sstream>
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
            char buf[20]; //enlarge as needed
            int needlen = snprintf(buf, sizeof(buf), m_fmt, value);
            if (needlen < sizeof(buf)) return m_strm << buf; //fits ok
            char* bigger = new char[needlen + 1];
            snprintf(bigger, needlen, m_fmt, value);
            m_strm << bigger;
            delete bigger;
            return m_strm;
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
#include <chrono>
double elapsed_msec()
{
    static auto started = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
    auto now = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
//https://stackoverflow.com/questions/14391327/how-to-get-duration-as-int-millis-and-float-seconds-from-chrono
//http://en.cppreference.com/w/cpp/chrono
    std::chrono::duration<double, std::milli> elapsed = now - started;
    return elapsed.count();
}


#include <sstream>
std::string timestamp()
{
    std::stringstream ss;
    ss << FMT("[%4.3f msec] ") << elapsed_msec();
    return ss.str();
}


////////////////////////////////////////////////////////////////////////////////

//overload new + delete operators:
//NOTE: these act like static members (no "this")
//void* operator new(size_t size); 
//void   operator delete(void* pointerToDelete); 
//-OR-
//void* operator new(size_t size, MemoryManager& memMgr); 
//void   operator delete(void* pointerToDelete, MemoryManager& memMgr);


class IMemoryManager 
{
public:
    virtual void* allocate(size_t) = 0;
    virtual void free(void*) = 0;
};
class MemoryManager: public IMemoryManager
{
public: 
    MemoryManager( );
    virtual ~MemoryManager( );
    virtual void* allocate(size_t);
    virtual void free(void*);
};


MemoryManager gMemoryManager; // Memory Manager, global variable
void* operator new(size_t size)
{
    return gMemoryManager.allocate(size);
}
void* operator new[](size_t size)
{
    return gMemoryManager.allocate(size);
}
void operator delete(void* pointerToDelete)
{
    gMemoryManager.free(pointerToDelete);
}
void operator delete[](void* arrayToDelete)
{
    gMemoryManager.free(arrayToDelete);
}



class Complex 
{
public:
    Complex (double a, double b): r (a), c (b) {}
private:
    double r; // Real Part
    double c; // Complex Part
};


#include <iostream>

int main(int argc, char* argv[]) 
{
    Complex* array[1000];
    std::cout << timestamp() << "start\n" << std::flush;
    for (int i = 0; i <  5000; i++)
    {
        for (int j = 0; j < 1000; j++)
            array[j] = new Complex (i, j);
        for (int j = 0; j < 1000; j++)
            delete array[j];
    }
    std::cout << timestamp() << "finish\n" << std::flush;
    return 0;
}

//eof