//shared memory allocator
//to compile:
// g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 shmalloc.cpp -o shmalloc
// gdb ./shmalloc
// bt
// x/80xw 0x7ffff7ff7000

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

//to look at shm:
// ipcs -m 

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
//#include <string> //https://stackoverflow.com/questions/6320995/why-i-cannot-cout-a-string
#include <stdexcept>
#define rdup(num, den)  (((num) + (den) - 1) / (den) * (den))
//mixin class for custom allocator:
class ShmHeapAlloc
{
public:
    void* operator new(size_t size, const char* filename, int line)
    {
        std::string key;
        shmheap.crekey(key, filename, line);
        void* ptr = shmheap.alloc(key.c_str(), size);
//    ++Complex::nalloc;
//    if (Complex::nalloc == 1)
        ATOMIC(std::cout << YELLOW_MSG << "alloc size " << size << ", key '" << key << "' at adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
        return ptr;
    }
    void operator delete(void* ptr) noexcept
    {
        shmheap.dealloc(ptr);
        ATOMIC(std::cout << YELLOW_MSG << "dealloc adrs " << FMT("0x%x") << ptr << " deleted but space not reclaimed" << ENDCOLOR << std::flush);
    }
//private: //configurable shm seg
    class ShmHeap
    {
    public: //ctor/dtor
        enum class persist: int {PreExist = 0, NewTemp = +1, NewPerm = -1};
        explicit ShmHeap(int key): ShmHeap(key, 0, persist::PreExist) {} //child processes
        explicit ShmHeap(int key, size_t size, persist cre): m_key(0), m_size(0), m_ptr(0), m_persist(false) //parent process
        {
            init_params = {false, key, size, cre}; //kludge: save info and init later (to avoid segv during static init)
        }
        struct { bool init; int key; size_t size; persist cre; } init_params;
        void defered_init() //kludge: delay init until needed (else segv during static init)
        {
            if (init_params.init) return; //already inited
//kludge: restore ctor params:
            int key = init_params.key;
            size_t size = init_params.size;
            persist cre = init_params.cre;
            ATOMIC(std::cout << BLUE_MSG << "defered_init(key " << FMT("0x%x") << key << ", size " << size << ", cre " << (int)cre << ")" << ENDCOLOR << std::flush);
//        ATOMIC(std::cout << CYAN_MSG << "hello" << ENDCOLOR << std::flush);
//        if ((key = ftok("hello.txt", 'R')) == -1) /*Here the file must exist */ 
//        int fd = shm_open(filepath, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
//        if (fd == -1) throw "shm_open failed";
//        if (ftruncate(fd, sizeof(struct region)) == -1)    /* Handle error */;
//        rptr = mmap(NULL, sizeof(struct region), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//        if (rptr == MAP_FAILED) /* Handle error */;
//create shm seg:
//        if (!key) throw "ShmHeap: bad key";
            if (cre != persist::PreExist) destroy(key); //delete first (parent wants clean start)
            if (!key) key = (rand() << 16) | 0xfeed;
            if ((size < 1) || (size >= 10000000)) throw std::runtime_error("ShmHeap: bad size"); //set reasonable limits
//NOTE: size will be rounded up to a multiple of PAGE_SIZE
            int shmid = shmget(key, size, 0666 | ((cre != persist::PreExist)? IPC_CREAT | IPC_EXCL: 0)); // | SHM_NORESERVE); //NOTE: clears to 0 upon creation
            ATOMIC(std::cout << BLUE_MSG << "shmget key " << FMT("0x%x") << key << ", size " << size << " => " << shmid << ENDCOLOR << std::flush);
            if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
            void* ptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
            ATOMIC(std::cout << BLUE_MSG << "shmat id " << FMT("0x%x") << shmid << " => " << FMT("0x%lx") << (long)ptr << ENDCOLOR << std::flush);
            if (ptr == (void*)-1) throw std::runtime_error(std::string(strerror(errno)));
            m_key = key;
            m_size = size;
            m_ptr = static_cast<decltype(m_ptr)>(ptr);
            m_persist = (cre != persist::NewTemp);
            if (cre != persist::PreExist) //init shm seg
            {
//these are redundant (smh init to 0 anyway):
//                m_ptr->used = 0;
//                m_ptr->symtab[0].nextofs = 0;
//                m_ptr->symtab[0].name[0] = '\0';
                m_ptr->nextofs = (long)&m_ptr->symtab[0] - (long)m_ptr; //eof
//this one definitely needed:
//                m_ptr->used = (long)&m_ptr->symtab[0] - (long)m_ptr;
//this one is probably needed:
                new (&m_ptr->mutex) std::mutex(); //make sure mutex is inited; placement new: https://stackoverflow.com/questions/2494471/c-is-it-possible-to-call-a-constructor-directly-without-new
            }
            init_params.init = true;
//            std::atexit(exiting); //this happens automatically
//            void exiting() { std::cout << "Exiting"; }
//        if (cre) m_ptr->used += sizeof(m_ptr->used);
//std::cout << "here1\n" << std::flush;
//            const char* d = desc().c_str();
//std::cout << "here2\n" << std::flush;
//            std::cout << "here2 desc = " << desc() << "\n" << std::flush;
//            printf("desc = %d:\n", strlen(d));
//            printf("desc = %d:%s\n", strlen(d), d);
//std::cout << desc() << " here3\n" << std::flush;
            ATOMIC(std::cout << CYAN_MSG << "ShmHeap: attached " << desc() << ENDCOLOR << std::flush);
        }
        ~ShmHeap() { detach(); if (!m_persist) destroy(m_key); }
    public: //getters
        inline int key() const { return m_key; }
        inline size_t size() const { return m_size; }
//        inline size_t available() const { return size() - used(); }
//        inline size_t used() const { return m_ptr->used; } // + sizeof(m_ptr->used); } //account for used space at front
    public: //cleanup
        void detach()
        {
            if (!m_ptr) return; //not attached
//  int shmctl(int shmid, int cmd, struct shmid_ds *buf);
            if (shmdt(m_ptr) == -1) throw std::runtime_error(strerror(errno));
            ATOMIC(std::cout << CYAN_MSG << "ShmHeap: dettached " << desc() << ENDCOLOR << std::flush);
            m_ptr = 0;
        }
        static void destroy(int key)
        {
            if (!key) return; //!owner or !exist
            int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
            ATOMIC(std::cout << CYAN_MSG << "ShmHeap: destroy " << FMT("0x%x") << key << " => " << FMT("0x%x") << shmid << ENDCOLOR << std::flush);
            if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
            if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
            throw std::runtime_error(strerror(errno));
        }
    public: //allocator
//generates a unique key from a src location:
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
//allocate named storage:
//NOTE: assumes infrequent allocation (traverses linked list of entries)
        void* alloc(const char* key, int size, int alignment = sizeof(uint32_t)) //assume 32-bit alignment, allow override; https://en.wikipedia.org/wiki/LP64
        {
            defered_init(); //defer init until needed (avoids problems with static init)
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
//first check if symbol already exists:
            int keylen = rdup(strlen(key) + 1, alignment);
            std::unique_lock<std::mutex> lock(m_ptr->mutex);
//check if symbol is already defined:
//            entry* symptr = &m_ptr->symtab[0];
//            entry* parent = (long)&m_ptr->nextofs - (long)m_ptr; //symofs = m_ptr->nextofs;
//            entry* parent = symtab(0);
            entry* symptr = symtab(m_ptr->nextofs);
            for (;;)
            {
                if (!symptr->name[0]) //eof
                {
//            size = sizeof(symptr->nextofs) + keylen + rdup(size, alignment);
//            m_ptr->used = rdup(m_ptr->used, sizeof(symptr->nextofs));
//            if (available() < size) return 0; //not enough space
                    size_t neweof = (long)symptr + keylen + rdup(size, alignment);
                    ATOMIC(std::cout << GREEN_MSG << "alloc key '" << key << "', size " << size << " at ofs " << ((long)symptr - (long)m_ptr) << ", remaining " << ((long)m_ptr + m_size - neweof) << ENDCOLOR << std::flush);
                    if (neweof > (long)m_ptr + m_size) return 0; //not enough space
                    symptr->nextofs = neweof;
                    strcpy(symptr->name, key);
                    break;
                }
                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
//                size_t ofs = (long)symptr - (long)m_ptr; //(void*)symptr - (void*)m_ptr;
                ATOMIC(std::cout << BLUE_MSG << "check for key '" << key << "' ==? symbol '" << symptr->name << "' at ofs " << symofs(symptr) << ENDCOLOR << std::flush);
                if (!strcmp(symptr->name, key)) break; //found
//                parent = symptr;
                symptr = symtab(symptr->nextofs);
//                symptr = (long)&m_ptr->symtab[0] + symptr->nextofs;
            }
            return (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
        }
        void dealloc(void* ptr)
        {
            std::unique_lock<std::mutex> lock(m_ptr->mutex);
//            entry* symptr = (void*)m_ptr + m_ptr->nextofs;
//            size_t symofs = (long)&m_ptr->symtab[0] - (long)m_ptr;
            entry* symptr = symtab(m_ptr->nextofs);
            for (;;)
            {
                if (!symptr->name[0]) break; //eof
                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
                int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
                void* varadrs = (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
                ATOMIC(std::cout << BLUE_MSG << "check if key '" << symptr->name << "' at ofs " << symofs(symptr) << " with adrs " << FMT("0x%x") << varadrs << ".." << FMT("0x%x") << symptr->nextofs << " is at dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
                if ((varadrs <= ptr) && (ptr < (void*)symtab(symptr->nextofs))) //adrs + rdup(keylen, 8))
                {
                    symptr->nextofs = symtab(symptr->nextofs)->nextofs; //remove from chain // = (long)&m_ptr->symtab[0] + symptr->nextofs;
                    return;
                }
                symptr = symtab(symptr->nextofs);
            }
            throw std::runtime_error("ShmHeap: attempt to dealloc unknown address");
        }
    private: //data
        int m_key; //only used for owner
//    void* m_ptr; //ptr to start of shm
        size_t m_size; //total size (bytes)
        typedef struct { size_t nextofs; char name[1]; } entry; //named storage entry
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
        inline entry* symtab(size_t ofs) const { return (entry*)((long)m_ptr + ofs); }
        inline size_t symofs(entry* symptr) const { return (long)symptr - (long)m_ptr; }
//        typedef struct { size_t used; std::mutex mutex; entry symtab[1]; } hdr;
//    inline entry* symtab() { return &m_ptr->symtab[0]; }
//        hdr* m_ptr; //ptr to start of shm
        struct { size_t nextofs; std::mutex mutex; entry symtab[1]; }* m_ptr; //shm struct ptr
//        struct { size_t nextofs; std::mutex mutex; }* m_ptr; //shm struct ptr
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
        bool m_persist;
    private: //helpers
//show shm details (for debug):
        const std::string /*NO &*/ desc() const
//        const char* desc() const
        {
            entry* symptr = symtab(m_ptr->nextofs);
            for (;;)
            {
                if (!symptr->name[0]) break; //eof
                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
                symptr = symtab(symptr->nextofs);
            }
            size_t used = symofs(symptr);
//            static std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
//            ostrm.str(""); ostrm.clear(); //https://stackoverflow.com/questions/5288036/how-to-clear-ostringstream
            std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
            ostrm << m_size << " bytes for key " << FMT("0x%x") << m_key;
            ostrm << " at " << FMT("0x%x") << m_ptr;
            ostrm << ", used = " << used << ", avail " << (m_size - used);
//            std::cout << ostrm.str().c_str() << "\n" << std::flush;
            return ostrm.str(); //.c_str();
        }
    };
    static ShmHeap shmheap; //use static member to reduce clutter in var instances
};
ShmHeapAlloc::ShmHeap ShmHeapAlloc::shmheap(0x4567feed, 0x1000, ShmHeapAlloc::ShmHeap::persist::NewPerm);
//tag allocated var with src location:
//NOTE: cpp avoids recursion so macro names can match actual function names here
//#define new  new(__FILE__, __LINE__)
#define NEW  new(__FILE__, __LINE__)


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


class Complex: public ShmHeapAlloc
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
        for (int j = 0; j < NUM_ENTS; j++) array[j] = new(__FILE__, __LINE__ + j) Complex (i, j);
        for (int j = 0; j < NUM_ENTS; j++) delete array[j];
    }
    std::cout << timestamp() << "finish\n" << std::flush;
//    std::cout << "#alloc: " << Complex::nalloc << ", #free: " << Complex::nfree << "\n" << std::flush;
    return 0;
}

//eof
