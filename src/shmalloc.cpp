#!/bin/bash -x
g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o shmalloc -x c++ - <<//EOF
//shared memory allocator test
//self-compiling c++ file; run this file to compile it; //https://stackoverflow.com/questions/17947800/how-to-compile-code-from-stdin?lq=1
//or, to compile manually:
// g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 shmalloc.cpp -o shmalloc
// gdb ./shmalloc
// bt
// x/80xw 0x7ffff7ff7000


#include "shmalloc.h"
//#define ShmHeap  ShmHeapAlloc::ShmHeap

//define shared memory heap:
//ShmHeap shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);
ShmHeap ShmHeapAlloc::shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);


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