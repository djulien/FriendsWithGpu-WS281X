#!/bin/bash -x
g++ -D__FILENAME__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O0 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o shmalloc -x c++ - <<//EOF
#line 4 __FILENAME__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line)

//shared memory allocator test
//self-compiling c++ file; run this file to compile it; //https://stackoverflow.com/questions/17947800/how-to-compile-code-from-stdin?lq=1
//how to get name of bash script: https://stackoverflow.com/questions/192319/how-do-i-know-the-script-file-name-in-a-bash-script
//or, to compile manually:
// g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 shmalloc.cpp -o shmalloc
// gdb ./shmalloc
// bt
// x/80xw 0x7ffff7ff7000


#include "shmalloc.h"

#ifdef JUNK
//#define ShmHeap  ShmHeapAlloc::ShmHeap

//define shared memory heap:
//ShmHeap shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);
ShmHeap ShmHeapAlloc::shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);


//typename ... ARGS>
//unique_ptr<T> factory(ARGS&&... args)
//{
//    return unique_ptr<T>(new T { std::forward<ARGS>(args)... });
//}
//    explicit vector_ex(std::size_t count = 0, const ALLOC& alloc = ALLOC()): std::vector<TYPE>(count, alloc) {}

#include "vectorex.h"
//#include "autoptr.h"
//#include <memory> //unique_ptr
int main()
{
#if 0
    typedef vector_ex<int, ShmAllocator<int>> vectype;
    static vectype& ids = //SHARED(SRCKEY, vectype, vectype);
        *(vectype*)ShmHeapAlloc::shmheap.alloc(SRCKEY, sizeof(vectype), 4 * sizeof(int), true, [] (void* shmaddr) { new (shmaddr) vectype; })
#else
//    ShmObj<vector_ex<int>, 0x5678feed, 4 * sizeof(int)> vect; //, __LINE__> vect;
//pool + "new" pattern from: https://stackoverflow.com/questions/20945439/using-operator-new-and-operator-delete-with-a-custom-memory-pool-allocator
//    ShmAllocator<vector_ex<int>> shmalloc;
//    unique_ptr<ShmObj<vector_ex<int>>> vectp = new (shmalloc) ShmObj<vector_ex<int>>();
    unique_ptr<ShmObj<vector_ex<int>>> vectp = new ShmObj<vector_ex<int>>();
#define vect  (*vectp)
//    ShmPtr<vector_ex<int>> vectp = new vector_ex<int>();
//    ShmObj<vector_ex<int>> vect;
#endif
    std::cout << "&vec " << FMT("%p") << &vect
        << FMT(", shmkey 0x%lx") << vect.allocator.shmkey()
        << ", sizeof(vect) " << sizeof(vect)
//        << ", extra " << vect.extra
        << ", #ents " << vect.size()
//        << ", srcline " << vect.srcline
        << "\n" << std::flush;
    for (int n = 0; n < 2; ++n) vect.emplace_back(100 + n);
    int ofs = vect.find(100);
    if (ofs == -1) throw std::runtime_error(RED_MSG "can't find entry" ENDCOLOR);
    vect.push_back(10);
    vect.push_back(12);
    std::cout << "sizeof(vect) " << sizeof(vect)
        << ", #ents " << vect.size() << "\n" << std::flush;
    vect.push_back(10);
    std::cout << "sizeof(vect) " << sizeof(vect)
        << ", #ents " << vect.size() << "\n" << std::flush;
//    std::unique_lock<std::mutex> lock(shmheap.mutex()); //low usage; reuse mutex
//    std::cout << "hello from " << __FILE__ << ":" << __LINE__ << "\n" << std::flush;
}
//#define main   mainx
#endif

#if 0
class Complex: public ShmHeapAlloc
{
public:
    Complex() {} //needed for custom new()
    Complex (double a, double b): r (a), c (b) {}
private:
    double r; // Real Part
    double c; // Complex Part
public:
};


#define NUM_LOOP  5 //5000
#define NUM_ENTS  5 //1000
int main(int argc, char* argv[]) 
{
    Complex* array[NUM_ENTS];
    std::cout << timestamp() << "start\n" << std::flush;
    for (int i = 0; i < NUM_LOOP; i++)
    {
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new(__FILE__, __LINE__ + j) Complex (i, j); //kludge: force unique key for shmalloc
        for (int j = 0; j < NUM_ENTS; j++) array[j] = new_SHM(+j) Complex (i, j); //kludge: force unique key for shmalloc
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new(shmheap.alloc(sizeof(Complex), __FILE__, __LINE__ + j) Complex (i, j); //kludge: force unique key for shmalloc
        for (int j = 0; j < NUM_ENTS; j++) delete array[j]; //FIFO
//        for (int j = 0; j < NUM_ENTS; j++) delete array[NUM_ENTS-1 - j]; //LIFO
//        for (int j = 0; j < NUM_ENTS; j++) array[j]->~Complex(); //FIFO
    }
    std::cout << timestamp() << "finish\n" << std::flush;
//    std::cout << "#alloc: " << Complex::nalloc << ", #free: " << Complex::nfree << "\n" << std::flush;
    return 0;
}
#endif


//eof
#if 0 //custom new + delete
    void* operator new(size_t size)
    {
        void* ptr = ::new Complex(); //CAUTION: force global new() here (else recursion)
//        void* ptr = malloc(size); //alternate
        ++nalloc;
        return ptr;
    }
    void operator delete(void* ptr)
    {
        ++nfree;
        free(ptr);
    }
    void* operator new (size_t size, const char* filename, int line)
    {
        void* ptr = new char[size];
        cout << "size = " << size << " filename = " << filename << " line = " << line << endl;
        return ptr;
    }
#endif
//    static int nalloc, nfree;
//int Complex::nalloc = 0;
//int Complex::nfree = 0;


#if 0
#include <iostream>

void* operator new(size_t size, const char* filename, int line)
{
    void* ptr = new char[size];
    ++Complex::nalloc;
    if (Complex::nalloc == 1)
        std::cout << "size = " << size << " filename = " << filename << " line = " << line << "\n" << std::flush;
    return ptr;
}
//tag alloc with src location:
//NOTE: cpp avoids recursion so macro names can match actual function names here
#define new  new(__FILE__, __LINE__)
#endif