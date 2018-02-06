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


//atomic operations:
//std::recursive_mutex atomic_mut;
#include <mutex>
std::mutex atomic_mut;
//use macro so stmt can be nested within scoped lock:
#define ATOMIC(stmt)  { std::unique_lock<std::mutex> lock(atomic_mut); stmt; }


////////////////////////////////////////////////////////////////////////////////


// https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
// https://stackoverflow.com/questions/4836863/shared-memory-or-mmap-linux-c-c-ipc

//    you identify your shared memory segment with some kind of symbolic name, something like "/myRegion"
//    with shm_open you open a file descriptor on that region
//    with ftruncate you enlarge the segment to the size you need
//    with mmap you map it into your address space

//template<int SIZE>
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <cstdlib>
#include <string.h>
#define rdup(num, den)  (((num) + (den) - 1) / (den) * (den))
class ShmHeap
{
public:
    ShmHeap(int key): ShmHeap(key, 0, false) {} //child processes
    ShmHeap(int key, size_t size, bool cre): m_key(cre? key: 0), m_ptr(0), m_size(0) //parent process
    {
//        if ((key = ftok("hello.txt", 'R')) == -1) /*Here the file must exist */ 
//        int fd = shm_open(filepath, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
//        if (fd == -1) throw "shm_open failed";
//        if (ftruncate(fd, sizeof(struct region)) == -1)    /* Handle error */;
//        rptr = mmap(NULL, sizeof(struct region), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//        if (rptr == MAP_FAILED) /* Handle error */;
//create shm seg:
//        if (!key) throw "ShmHeap: bad key";
        if (!key) key = (rand() << 16) | 0xfeed;
        if (cre) destroy(); //delete first (parent wants clean start)
    	if ((size < 1) || (size >= 10000000)) throw "ShmHeap: bad size"; //set reasonable limits
//NOTE: size will be rounded up to a multiple of PAGE_SIZE
        int shmid = shmget(key, size, (cre? IPC_CREAT | IPC_EXCL: 0) | 0666); // | SHM_NORESERVE); //NOTE: clears to 0 upon creation
        if (shmid == -1) throw strerror(errno); //failed to create or attach
        void* ptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
        if (ptr == (void*)-1) throw strerror(errno);
        m_key = cre? key: 0;
        m_size = size;
        m_ptr = (hdr*)ptr;
//        if (cre) m_ptr->used += sizeof(m_ptr->used);
        ATOMIC(std::cout << "ShmHeap: attached " << size << " bytes for key " << FMT("0x%x") << m_key << " at " << FMT("0x%x") << m_ptr << ", used = " << used() << ", avail " << available() << "\n" << std::flush);
    }
    ~ShmHeap() { detach(); destroy(); }
public:
    inline int key() { return m_key; }
    inline size_t size() { return m_size; }
    inline size_t available() { return size() - used(); }
    inline size_t used() { return m_ptr->used + sizeof(m_ptr->used); } //account for used space at first location
public:
//generate a unique key from a src location:
//same src location will generate same key, even across processes
    static void crekey(std::string& key, const char* filename, int line)
    {
        const char* bp = strrchr(filename, '/');
        if (!bp) bp = filename;
        key = bp;
        key += ':';
        char buf[10];
        sprintf(buf, "%d", line);
        key += buf; //line;
    }
//NOTE: assumes infrequent allocation (traverses linked list of entries)
    void* alloc(const char* key, int size, int alignment = 4) //assume 32-bit alignment, allow override; https://en.wikipedia.org/wiki/LP64
    {
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
//first check if symbol already exists:
        entry* symptr = &m_ptr->symtab[0];
        for (;;) //check if symbol is already defined
        {
            size_t ofs = (long)symptr - (long)m_ptr; //(void*)symptr - (void*)m_ptr;
            ATOMIC(std::cout << "check for key '" << key << "' ==? symbol '" << symptr->name << "' at ofs " << ofs << "\n" << std::flush);
            if (!strcmp(symptr->name, key)) return &symptr->name[0] + rdup(strlen(symptr->name) + 1, alignment);
            if (!symptr->nextofs) break;
            symptr = &m_ptr->symtab[0] + symptr->nextofs;
        }
        int keylen = rdup(strlen(key) + 1, alignment);
        size = sizeof(symptr->nextofs) + keylen + rdup(size, alignment);
        m_ptr->used = rdup(m_ptr->used, sizeof(symptr->nextofs));
        if (available() < size) return 0; //not enough space
        void* newptr = (void*)m_ptr + used(); //+ sizeof(symptr->nextofs) + keylen;
        symptr->nextofs = (entry*)newptr - &m_ptr->symtab[0];
        newptr += sizeof(symptr->nextofs) + keylen;
        size_t ofs = (long)newptr - (long)m_ptr; //newptr - (void*)m_ptr;
        ATOMIC(std::cout << "alloc key '" << key << "', size " << size << " at ofs " << ofs << ", remaining " << available() << "\n" << std::flush);
        return newptr;
    }
    void destroy() //int key)
    {
        if (!m_key) return; //not owner
        int shmid = shmget(m_key, 1, 0666); //use minimum size in case it changed
        m_key = 0;
        if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
        if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
        throw strerror(errno);
    }
    void detach()
    {
        if (!m_ptr) return;
        if (shmdt(m_ptr) == -1) throw strerror(errno);
        ATOMIC(std::cout << "ShmHeap: dettached " << m_size << " bytes from " << FMT("0x%x") << m_ptr << "\n" << std::flush);
        m_ptr = 0;
    }
private:
    int m_key; //only used for owner
//    void* m_ptr; //ptr to start of shm
    size_t m_size; //total size (bytes)
    typedef struct { size_t nextofs; char name[1]; } entry;
    typedef struct { size_t used; entry symtab[1]; } hdr;
//    inline entry* symtab() { return &m_ptr->symtab[0]; }
    hdr* m_ptr; //ptr to start of shm
};
ShmHeap shmheap(0, 0x1000, true);

void operator delete(void* ptr) noexcept
{
    ATOMIC(std::cout << "dealloc adrs " << FMT("0x%x") << ptr << " (ignored)\n" << std::flush);
}
void* operator new(size_t size, const char* filename, int line)
{
    std::string key;
    shmheap.crekey(key, filename, line);
    void* ptr = shmheap.alloc(key.c_str(), size);
//    ++Complex::nalloc;
//    if (Complex::nalloc == 1)
    ATOMIC(std::cout << "alloc size " << size << ", key '" << key << "' at adrs " << FMT("0x%x") << ptr << "\n" << std::flush);
    return ptr;
}
//tag alloc with src location:
//NOTE: cpp avoids recursion so macro names can match actual function names here
#define new  new(__FILE__, __LINE__)


//overload new + delete operators:
//can be global or class-specific
//NOTE: these are static members (no "this")
//void* operator new(size_t size); 
//void operator delete(void* pointerToDelete); 
//-OR-
//void* operator new(size_t size, MemoryManager& memMgr); 
//void operator delete(void* pointerToDelete, MemoryManager& memMgr);

// https://www.geeksforgeeks.org/overloading-new-delete-operator-c/
// https://stackoverflow.com/questions/583003/overloading-new-delete

#if 0
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
#endif


class Complex 
{
public:
    Complex() {} //needed for custom new()
    Complex (double a, double b): r (a), c (b) {}
private:
    double r; // Real Part
    double c; // Complex Part
public:
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
};
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


#define NUM_LOOP  5 //5000
#define NUM_ENTS  5 //1000
int main(int argc, char* argv[]) 
{
    Complex* array[NUM_ENTS];
    std::cout << timestamp() << "start\n" << std::flush;
    for (int i = 0; i < NUM_LOOP; i++)
    {
        for (int j = 0; j < NUM_ENTS; j++) array[j] = new Complex (i, j);
        for (int j = 0; j < NUM_ENTS; j++) delete array[j];
    }
    std::cout << timestamp() << "finish\n" << std::flush;
//    std::cout << "#alloc: " << Complex::nalloc << ", #free: " << Complex::nfree << "\n" << std::flush;
    return 0;
}

//eof