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
//#define ShmHeap  ShmHeapAlloc::ShmHeap

//define shared memory heap:
//ShmHeap shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);
ShmHeap ShmHeapAlloc::shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);


#include <cstddef> //ptrdiff_t
template<typename TYPE, long SHMKEY = 0, int EXTRA = 0> //, int SRCLINE>
class ShmObj: public TYPE
{
public: //ctors/dtors
    template<typename ... ARGS> //"perfect forwarding"
    ShmObj(ARGS&& ... args): TYPE(std::forward<ARGS>(args) ...), shmkey(SHMKEY? SHMKEY: mkkey()), extra(EXTRA) {} //, srcline(SRCLINE) {}
public: //data members
    const key_t shmkey;
    const int extra; //, srcline;
public: //custom std::allocator
#if 0
    class Allocator
    {
    public: //helper types
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;
        typedef TYPE* pointer;
        typedef const TYPE* const_pointer;
        typedef TYPE& reference;
        typedef const TYPE& const_reference;
        typedef TYPE value_type;
    public: //ctor/dtor
        Allocator() {}
        Allocator(const Allocator&) {}
    public: //member functions
        pointer allocate(size_type count, const void* hint = 0)
        {
            TYPE* ptr = (TYPE*)malloc(count * sizeof(TYPE)); //TODO: replace
            std::cout << RED_MSG << "custom allocated adrs " << FMT("%p") << ptr << ENDCOLOR << std::flush;
            return ptr;
        }
        void deallocate(void* ptr, size_type count)
        {
            if (!ptr) return;
            free(ptr); //TODO: replace
            std::cout << RED_MSG << "custom deallocated adrs " << FMT("%p") << ptr << ENDCOLOR << std::flush;
        }
        pointer address(reference x) const { return &x; }
        const_pointer address(const_reference x) const { return &x; }
        Allocator/*<TYPE>*/& operator=(const Allocator&) { return *this; }
        void construct(pointer ptr, const TYPE& val) { new ((TYPE*)ptr) TYPE(val); }
        void destroy(pointer ptr) { ptr->~TYPE(); }
        size_type max_size() const { return size_t(-1); }
        template <class U>
        struct rebind { typedef Allocator<U> other; };
        template <class U>
        Allocator(const Allocator<U>&) {}
        template <class U>
        Allocator& operator=(const Allocator<U>&) { return *this; }
    };
#else
    class ShmAllocator//: public std::allocator<TYPE>
    {
    public: //type defs
        typedef TYPE value_type;
        typedef TYPE* pointer;
        typedef const TYPE* const_pointer;
        typedef TYPE& reference;
        typedef const TYPE& const_reference;
        typedef std::size_t size_type;
//??    typedef off_t offset_type;
        typedef std::ptrdiff_t difference_type;
    public: //ctor/dtor; nops - allocator has no state
//    mmap_allocator() throw(): std::allocator<T>(), filename(""), offset(0), access_mode(DEFAULT_STL_ALLOCATOR),	map_whole_file(false), allow_remap(false), bypass_file_pool(false), private_file(), keep_forever(false) {}
        ShmAllocator() throw()/*: std::allocator<TYPE>()*/ { fprintf(stderr, "Hello allocator!\n"); }
//    mmap_allocator(const std::allocator<T> &a) throw():	std::allocator<T>(a), filename(""),	offset(0), access_mode(DEFAULT_STL_ALLOCATOR), map_whole_file(false), allow_remap(false), bypass_file_pool(false), private_file(), keep_forever(false) {}
        ShmAllocator(const ShmAllocator& that) throw()/*: std::allocator<TYPE>(that)*/ {}
        template <class OTHER_TYPE, long OTHER_SHMKEY, int OTHER_EXTRA>
        ShmAllocator(const ShmAllocator<OTHER_TYPE, OTHER_SHMKEY, OTHER_EXTRA>& that) throw()/*: std::allocator<TYPE>(that)*/ {}
        ~ShmAllocator() throw() {}
    public: //methods
//rebind allocator to another type:
        template <class OTHER_TYPE, long OTHER_SHMKEY, int OTHER_EXTRA>
        struct rebind { typedef ShmAllocator<OTHER_TYPE, OTHER_SHMKEY, OTHER_EXTRA> other; };
//get value address:
        pointer address (reference value) const { return &value; }
        const_pointer address (const_reference value) const { return &value; }
//max #elements that can be allocated:
        size_type max_size() const throw() { return std::numeric_limits<std::size_t>::max() / sizeof(TYPE); }
//allocate but don't initialize:
        pointer allocate(size_type count, const void* hint = 0)
        {
//        void *the_pointer;
//        if (get_verbosity() > 0) fprintf(stderr, "Alloc %zd bytes.\n", num *sizeof(TYPE));
//        if (access_mode == DEFAULT_STL_ALLOCATOR) return std::allocator<TYPE>::allocate(num, hint);
//        if (num == 0) return NULL;
//        if (bypass_file_pool) the_pointer = private_file.open_and_mmap_file(filename, access_mode, offset, n*sizeof(T), map_whole_file, allow_remap);
//        else the_pointer = the_pool.mmap_file(filename, access_mode, offset, n*sizeof(T), map_whole_file, allow_remap);
//        if (the_pointer == NULL) throw(mmap_allocator_exception("Couldn't mmap file, mmap_file returned NULL"));
//        if (get_verbosity() > 0) fprintf(stderr, "pointer = %p\n", the_pointer);
//        return (pointer)the_pointer;
//print message and allocate memory with global new:
//        return std::allocator<TYPE>::allocate(n, hint);
            pointer ptr = (pointer)(::operator new(count * sizeof(TYPE)));
            ATOMIC(std::cout << YELLOW_MSG << "ShmAllocator: allocated " << count << " element(s)"
                        << " of size " << sizeof(TYPE) << FMT(" at %p") << (long)ptr << ENDCOLOR << std::flush);
            return ptr;
        }
//init elements of allocated storage ptr with value:
        void construct (pointer ptr, const TYPE& value)
        {
            new ((void*)ptr) TYPE(value); //init memory with placement new
        }
//destroy elements of initialized storage ptr:
        void destroy (pointer ptr)
        {
            ptr->~TYPE(); //call dtor
        }
//deallocate storage ptr of deleted elements:
        void deallocate (pointer ptr, size_type count)
        {
//        fprintf(stderr, "Dealloc %d bytes (%p).\n", num * sizeof(TYPE), ptr);
//        if (get_verbosity() > 0) fprintf(stderr, "Dealloc %zd bytes (%p).\n", num *sizeof(TYPE), ptr);
//        if (access_mode == DEFAULT_STL_ALLOCATOR) return std::allocator<T>::deallocate(ptr, num);
//        if (num == 0) return;
//        if (bypass_file_pool) private_file.munmap_and_close_file();
//        else if (!keep_forever) the_pool.munmap_file(filename, access_mode, offset, num *sizeof(TYPE));
//print message and deallocate memory with global delete:
            std::cerr << "deallocate " << count << " element(s)"
                        << " of size " << sizeof(TYPE)
                        << " at: " << (void*)ptr << std::endl;
//        return std::allocator<TYPE>::deallocate(p, n);
            ::operator delete((void*)ptr);
        }
//private:
//		friend class mmappable_vector<TYPE, mmap_allocator<TYPE> >;
    };
#endif
private: //data
    static key_t mkkey() { return (rand() << 16) | 0xfeed; }
};
//typename ... ARGS>
//unique_ptr<T> factory(ARGS&&... args)
//{
//    return unique_ptr<T>(new T { std::forward<ARGS>(args)... });
//}
//    explicit vector_ex(std::size_t count = 0, const ALLOC& alloc = ALLOC()): std::vector<TYPE>(count, alloc) {}

#include "vectorex.h"
int main()
{
#if 0
    typedef vector_ex<int, ShmAllocator<int>> vectype;
    static vectype& ids = //SHARED(SRCKEY, vectype, vectype);
        *(vectype*)ShmHeapAlloc::shmheap.alloc(SRCKEY, sizeof(vectype), 4 * sizeof(int), true, [] (void* shmaddr) { new (shmaddr) vectype; })
#else
    ShmObj<vector_ex<int>, 0x5678feed, 4 * sizeof(int)> vect; //, __LINE__> vect;
#endif
    std::cout << "&vec " << FMT("%p") << &vect
        << FMT(", shmkey 0x%lx") << vect.shmkey
        << ", sizeof(vect) " << sizeof(vect)
        << ", extra " << vect.extra
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
#define main   mainx


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