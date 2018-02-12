//shared memory allocator
//see https://www.ibm.com/developerworks/aix/tutorials/au-memorymanager/index.html
//see also http://anki3d.org/cpp-allocators-for-games/

//CAUTION: “static initialization order fiasco”
//https://isocpp.org/wiki/faq/ctors

//to look at shm:
// ipcs -m 

// https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
// https://stackoverflow.com/questions/4836863/shared-memory-or-mmap-linux-c-c-ipc

//    you identify your shared memory segment with some kind of symbolic name, something like "/myRegion"
//    with shm_open you open a file descriptor on that region
//    with ftruncate you enlarge the segment to the size you need
//    with mmap you map it into your address space

#ifndef _SHMALLOC_H
#define _SHMALLOC_H

//template<int SIZE>
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <cstdlib>
#include <string.h>
//#include <string> //https://stackoverflow.com/questions/6320995/why-i-cannot-cout-a-string
#include <stdexcept>
#include "colors.h"
#include "ostrfmt.h"
#include "elapsed.h"
#include "atomic.h"

#define rdup(num, den)  (((num) + (den) - 1) / (den) * (den))
#define size32_t  uint32_t //don't need huge sizes in shared memory; cut down on wasted bytes


//shared memory segment:
class ShmSeg
{
public: //ctor/dtor
    enum class persist: int {Reuse = 0, NewTemp = -1, NewPerm = +1};
    explicit ShmSeg(size32_t size, persist cre = persist::NewTemp, key_t key = 0): m_ptr(0), m_keep(true)
    {
        if (cre != persist::Reuse) destroy(key); //start fresh
        m_keep = (cre != persist::NewTemp);        
        create(key, size, cre != persist::Reuse);
    }
    ~ShmSeg()
    {
        detach();
        if (!m_keep) destroy(m_key);
    }
public: //getters
    inline int shmkey() const { return m_key; }
    inline void* shmptr() const { return m_ptr; }
    inline size32_t shmsize() const { return m_size; }
private: //data
    key_t m_key;
    void* m_ptr;
    size32_t m_size;
    bool m_keep;
private: //helpers
    void create(key_t key, size32_t size, bool want_new)
    {
        if (!key) key = (rand() << 16) | 0xfeed;
//#define  SHMKEY  ((key_t) 7890) /* base value for shmem key */
//#define  PERMS 0666
//        if (cre != persist::PreExist) destroy(key); //delete first (parent wants clean start)
//        if (!key) key = (rand() << 16) | 0xfeed;
        if ((size < 1) || (size >= 10000000)) throw std::runtime_error("ShmSeg: bad size"); //set reasonable limits
        int shmid = shmget(key, size, 0666 | (want_new? IPC_CREAT | IPC_EXCL: 0)); // | SHM_NORESERVE); //NOTE: clears to 0 upon creation
        ATOMIC(std::cout << CYAN_MSG << "ShmSeg: cre shmget key " << FMT("0x%x") << key << ", size " << size << " => " << FMT("0x%x") << shmid << ENDCOLOR << std::flush);
        if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
        struct shmid_ds info;
        if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
        void* ptr = shmat(shmid, NULL /*system choses adrs*/, 0); //read/write access
        ATOMIC(std::cout << BLUE_MSG << "ShmSeg: shmat id " << FMT("0x%x") << shmid << " => " << FMT("0x%lx") << (long)ptr << ENDCOLOR << std::flush);
        if (ptr == (void*)-1) throw std::runtime_error(std::string(strerror(errno)));
        m_key = key;
        m_ptr = ptr;
        m_size = info.shm_segsz; //size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
    }
    void detach()
    {
        if (!m_ptr) return; //not attached
//  int shmctl(int shmid, int cmd, struct shmid_ds *buf);
        if (shmdt(m_ptr) == -1) throw std::runtime_error(strerror(errno));
        m_ptr = 0; //can't use m_shmptr after this point
        ATOMIC(std::cout << CYAN_MSG << "ShmSeg: dettached " << ENDCOLOR << std::flush); //desc();
    }
    static void destroy(key_t key)
    {
        if (!key) return; //!owner or !exist
        int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
        ATOMIC(std::cout << CYAN_MSG << "ShmSeg: destroy " << FMT("0x%x") << key << " => " << FMT("0x%x") << shmid << ENDCOLOR << std::flush);
        if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
        if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
        throw std::runtime_error(strerror(errno));
    }
};


//shared memory heap:
//NOTE: very simplistic, not suitable for intense usage
class ShmHeap: public ShmSeg
{
#define dump(...)  dump(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#define gc(...)  gc(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
public: //ctor/dtor
    explicit ShmHeap(size32_t size, persist cre = persist::NewTemp, key_t key = 0): ShmSeg(size, cre, key) //, scoped_lock::m_mutex(mutex())
    {
        if (cre == persist::Reuse) return; //no need to init shm seg
//these are redundant (smh init to 0 anyway):
//                m_ptr->used = 0;
//                m_ptr->symtab[0].nextofs = 0;
//                m_ptr->symtab[0].name[0] = '\0';
//this one definitely needed:
//                m_ptr->used = (long)&m_ptr->symtab[0] - (long)m_ptr;
//first entry is mutex instead of named storage:
//                m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
        scoped_lock lock(*this);
        new (&mutex()) std::mutex(); //make sure mutex is inited; placement new: https://stackoverflow.com/questions/2494471/c-is-it-possible-to-call-a-constructor-directly-without-new
//                m_shmptr[1].nextofs = 0; //eof marker
        gc(true);
        ATOMIC(std::cout << BLUE_MSG << "sizeof(mutex) = " << sizeof(std::mutex) << ", sizeof(size_t) = " << sizeof(size_t) << ", x/80xw (addr) to examine memory" << ENDCOLOR << std::flush);
//        init_params.init = true;
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
        dump("initial heap:");
        ATOMIC(std::cout << CYAN_MSG << "ShmHeap: attached " << desc() << ENDCOLOR << std::flush);
    }
    ~ShmHeap() {} // (&mutex())->~std::mutex(); //not needed?
private: //data
//symbol table entry:
    typedef struct
    {
        size32_t nextofs;
        union
        {
            char name[1]; //named storage entry (name + storage space)
            std::mutex mutex; //thread/proc sync (first entry only)
        };
    } entry;
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
    inline entry* symtab(size32_t ofs) const { return (entry*)(shmptr() + ofs); } //relative ofs -> ptr
//        inline size32_t symofs(entry* symptr) const { return (long)symptr - (long)m_symtab; }
//        typedef struct { size_t used; std::mutex mutex; entry symtab[1]; } hdr;
//    inline entry* symtab() { return &m_ptr->symtab[0]; }
//        hdr* m_ptr; //ptr to start of shm
//        struct { size32_t nextofs; std::mutex mutex; entry symtab[1]; }* m_ptr; //shm struct ptr
//        struct { std::mutex mutex; entry symtab[1]; }* m_shmptr; //shm struct
//    entry* m_shmptr; //symbol table in shm
//        struct { size_t nextofs; std::mutex mutex; }* m_ptr; //shm struct ptr
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
//    bool m_persist;
public: //getters
    inline std::mutex& mutex() { return symtab(0)->mutex; }
    inline size32_t shmofs(void* ptr) const { return (long)ptr - (long)shmptr(); } //ptr -> relative ofs; //(long)ptr - (long)m_shmptr; }
public: //allocator methods
    class scoped_lock: public std::unique_lock<std::mutex>
    {
    public:
        explicit scoped_lock(ShmHeap& shmheap): std::unique_lock<std::mutex>(shmheap.mutex()) {};
//        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
//    private:
//        static std::mutex& m_mutex;
    };
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
    void* alloc(const char* key, size_t size, int alignment = sizeof(uint32_t)) //assume 32-bit alignment, allow override; https://en.wikipedia.org/wiki/LP64
    {
//        defered_init(); //kludge: defer init until needed (avoids problems with static init)
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
        int keylen = rdup(strlen(key) + 1, alignment); //alignment for var storage
        size = rdup(keylen + size, sizeof(symtab(0)->nextofs)); //alignment for following nextofs
//        std::unique_lock<std::mutex> lock(mutex()); //m_shmptr->mutex);
        scoped_lock lock(*this);
//first check if symbol already exists:
//            entry* symptr = &m_ptr->symtab[0];
//            entry* parent = (long)&m_ptr->nextofs - (long)m_ptr; //symofs = m_ptr->nextofs;
//            entry* parent = symtab(0);
        dump("before alloc:");
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
        entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs);
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
            if (!symptr->nextofs) //eof
            {
//            size = sizeof(symptr->nextofs) + keylen + rdup(size, alignment);
//            m_ptr->used = rdup(m_ptr->used, sizeof(symptr->nextofs));
//            if (available() < size) return 0; //not enough space
//                    long neweof = (long)symptr->name + keylen + rdup(size, alignment);
//alloc new storage:
                entry* nextptr = (entry*)&symptr->name[size]; //alloc space for this var
                size32_t remaining = (long)shmptr() + shmsize() - (long)nextptr->name;
                ATOMIC(std::cout << GREEN_MSG << "alloc key '" << key << "', size " << size << " at ofs " << shmofs(symptr) << ", remaining " << remaining << ENDCOLOR << std::flush);
                if (remaining < 0) return 0; //not enough space
                symptr->nextofs = shmofs(nextptr); //neweof - (long)m_ptr;
                strcpy(symptr->name, key);
                memset(symptr->name + keylen, 0, size - keylen); //clear storage for new var
                nextptr->nextofs = 0; //new eof marker (in case space was reclaimed earlier)
                dump("after alloc:");
                return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
//                    break;
            }
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
//                size_t ofs = (long)symptr - (long)m_ptr; //(void*)symptr - (void*)m_ptr;
            ATOMIC(std::cout << BLUE_MSG << "check for key '" << key << "' ==? symbol '" << symptr->name << "' at ofs " << shmofs(symptr) << ENDCOLOR << std::flush);
            if (!strcmp(symptr->name, key)) return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen); //break; //found
//                parent = symptr;
//                symptr = symtab(symptr->nextofs);
            parent = symptr;
//                symptr = (long)&m_ptr->symtab[0] + symptr->nextofs;
        }
//            return (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
    }
    void dealloc(void* ptr)
    {
//        std::unique_lock<std::mutex> lock(mutex()); //m_shmptr->mutex);
        scoped_lock lock(*this);
//            entry* symptr = (void*)m_ptr + m_ptr->nextofs;
//            size_t symofs = (long)&m_ptr->symtab[0] - (long)m_ptr;
        dump("before dealloc(0x%x):", shmofs(ptr));
        entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
            if (!symptr->nextofs) break; //eof
            int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
            void* varadrs = (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
            ATOMIC(std::cout << BLUE_MSG << "check '" << symptr->name << FMT("' ofs 0x%x") << shmofs(symptr) << " adrs " << FMT("0x%x") << varadrs << ".." << FMT("0x%x") << symptr->nextofs << " ~= dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
            if ((varadrs <= ptr) && (ptr < (void*)symtab(symptr->nextofs))) //adrs + rdup(keylen, 8))
            {
//                    symptr->nextofs = symtab(symptr->nextofs)->nextofs; //remove from chain // = (long)&m_ptr->symtab[0] + symptr->nextofs;
                parent->nextofs = symptr->nextofs; //remove from chain
//                    if (!symtab(m_ptr->nextofs)->name[0]) gc(true); //all vars deallocated
//                    if (m_shmptr->next)
//                    if (!symtab(symptr->nextofs)->nextofs &&
//                    if (!symtab(0)->nextofs)
//                    if ((parent == m_shmptr) && !parent->nextofs m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
                if ((parent == symtab(0)) && !symtab(symptr->nextofs)->nextofs) gc(false); //garbage collect
//                    if (!symtab(m_shmptr->nextofs)->nextofs) m_shmptr->nextofs = shmofs(&m_shmptr[1]); //gc
                dump("after dealloc(0x%x):", shmofs(ptr));
                return;
            }
//                symptr = symtab(symptr->nextofs);
            parent = symptr;
        }
        throw std::runtime_error("ShmHeap: attempt to dealloc unknown address");
    }
private: //helpers; NOTE: mutex must be owned before using these
#undef dump
#undef gc
//garbage collector:
    void gc(int line, bool init) //kludge: need extra param here for __VA_ARGS__
    {
        ATOMIC(std::cout << YELLOW_MSG << "gc(" << init << ")" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//        if (init)
//        {
//            m_ptr[0].nextofs = sizeof(m_ptr[0]); //symofs(m_ptr->symtab); //[0] - (long)m_ptr;
//            m_ptr->symtab[0].name[0] = '\0'; //eof marker
//            m_shmptr->symtab[0].nextofs = 0; //eof marker
        entry* entries = symtab(0);
        entries[0].nextofs = shmofs(&entries[1]);
        entries[1].nextofs = 0; //eof marker
//        }
    }
//show shm details (for debug):
    const std::string /*NO &*/ desc() const
//        const char* desc() const
    {
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //symtab(m_ptr->nextofs);
        entry* parent = symtab(0); //m_shmptr;
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
            if (!symptr->nextofs) break; //eof
//                symptr = symtab(symptr->nextofs);
            parent = symptr;
        }
        size32_t used = shmofs(parent); //symptr);
//            static std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
//            ostrm.str(""); ostrm.clear(); //https://stackoverflow.com/questions/5288036/how-to-clear-ostringstream
        std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
        ostrm << shmsize() << " bytes for shm key " << FMT("0x%x") << shmkey();
        ostrm << " at " << FMT("0x%x") << shmptr();
        ostrm << ", used = " << used << ", avail " << (shmsize() - used);
//            std::cout << ostrm.str().c_str() << "\n" << std::flush;
        return ostrm.str(); //.c_str();
    }
    void dump(int line, const char* desc, long val = 0)
    {
        ATOMIC(std::cout << BLUE_MSG << "shm " << FMT(desc) << val << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//            entry* symptr = symtab(0); //symtab(m_ptr->nextofs);
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
        entry* parent = symtab(0); //m_shmptr;
//        ATOMIC(std::cout << BLUE_MSG << "parent ofs " << FMT("0x%x") << shmofs(parent) << FMT(", 0x%x") << shmofs(&parent[1]) << ENDCOLOR << std::flush);
        for (;;)
        {
            entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
            if (!symptr->nextofs) //eof
            {
                ATOMIC(std::cout << BLUE_MSG << FMT("[0x%x]") << shmofs(symptr) << " eof" << ENDCOLOR << std::flush);
                return;
            }
            int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
            ATOMIC(std::cout << BLUE_MSG << FMT("[0x%x]") << shmofs(symptr) << " next ofs " << FMT("0x%x") << symptr->nextofs << ", name " << strlen(symptr->name) << ":'" << symptr->name << "', adrs " << FMT("0x%x") << shmofs(symptr->name + keylen) << "..+8" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//                symptr = symtab(symptr->nextofs);
            parent = symptr;
        }
    }
//    static ShmHeap shmheap; //use static member to reduce clutter in var instances
//    std::mutex sm;
//no worky    std::mutex& scoped_lock::m_mutex = sm; //mutex();
};
//std::mutex& ShmHeap::scoped_lock::m_mutex = mutex();


//mixin class for custom allocator:
class ShmHeapAlloc
{
public:
    void* operator new(size_t size, const char* filename, int line, int adjust = 0)
    {
        std::string key;
        if (adjust) line += adjust;
        shmheap.crekey(key, filename, line); //create unique key
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
//TODO?  alignof(void*); //http://www.boost.org/doc/libs/1_59_0/doc/html/align/vocabulary.html
//alignas(Foo) unsigned char memory[sizeof(Foo)];
//Foo* p = ::new((void*)memory) Foo();
        void* ptr = shmheap.alloc(key.c_str(), size);
//    ++Complex::nalloc;
//    if (Complex::nalloc == 1)
        ATOMIC(std::cout << YELLOW_MSG << "alloc size " << size << ", key " << key.length() << ":'" << key << "' at adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
        return ptr;
    }
    void operator delete(void* ptr) noexcept
    {
        shmheap.dealloc(ptr);
        ATOMIC(std::cout << YELLOW_MSG << "dealloc adrs " << FMT("0x%x") << ptr << " (ofs " << shmheap.shmofs(ptr) << "), deleted but space not reclaimed" << ENDCOLOR << std::flush);
    }
private:
    static ShmHeap shmheap;
};

//tag allocated var with src location:
//NOTE: cpp avoids recursion so macro names can match actual function names here
//#define new  new(__FILE__, __LINE__)
//#define NEW  new(__FILE__, __LINE__)
#define new_SHM(...)  new(__FILE__, __LINE__, __VA_ARGS__)


#ifdef SHM_KEY
ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, SHM_KEY); //0x4567feed);
#endif


#ifdef OLD_ONE
//mixin class for custom allocator:
class ShmHeapAlloc
{
public:
    void* operator new(size_t size, const char* filename, int line, int adjust)
    {
        std::string key;
        if (adjust) line += adjust;
        shmheap.crekey(key, filename, line); //create unique key
        void* ptr = shmheap.alloc(key.c_str(), size);
//    ++Complex::nalloc;
//    if (Complex::nalloc == 1)
        ATOMIC(std::cout << YELLOW_MSG << "alloc size " << size << ", key " << key.length() << ":'" << key << "' at adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
        return ptr;
    }
    void operator delete(void* ptr) noexcept
    {
        shmheap.dealloc(ptr);
        ATOMIC(std::cout << YELLOW_MSG << "dealloc adrs " << FMT("0x%x") << ptr << " (ofs " << shmheap.shmofs(ptr) << "), deleted but space not reclaimed" << ENDCOLOR << std::flush);
    }
//private: //configurable shm seg
    class ShmHeap
    {
#define dump(...)  dump(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#define gc(...)  gc(__LINE__, __VA_ARGS__) //incl line# in debug; https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
    public: //ctor/dtor
        enum class persist: int {PreExist = 0, NewTemp = +1, NewPerm = -1};
        explicit ShmHeap(int key): ShmHeap(key, 0, persist::PreExist) {} //child processes
        explicit ShmHeap(int key, size32_t size, persist cre): m_key(0), m_size(0), m_shmptr(0), m_persist(false) //parent process
        {
            init_params = {false, key, size, cre}; //kludge: save info and init later (to avoid segv during static init)
        }
    private:
        struct { bool init; int key; size32_t size; persist cre; } init_params;
        void defered_init() //kludge: delay init until needed (else segv during static init)
        {
            if (init_params.init) return; //already inited
//kludge: restore ctor params:
            int key = init_params.key;
            size32_t size = init_params.size;
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
            m_shmptr = static_cast<decltype(m_shmptr)>(ptr);
            m_persist = (cre != persist::NewTemp);
            if (cre != persist::PreExist) //init shm seg
            {
//these are redundant (smh init to 0 anyway):
//                m_ptr->used = 0;
//                m_ptr->symtab[0].nextofs = 0;
//                m_ptr->symtab[0].name[0] = '\0';
//this one definitely needed:
//                m_ptr->used = (long)&m_ptr->symtab[0] - (long)m_ptr;
//first entry is mutex instead of named storage:
//                m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
                new (&m_shmptr[0].mutex) std::mutex(); //make sure mutex is inited; placement new: https://stackoverflow.com/questions/2494471/c-is-it-possible-to-call-a-constructor-directly-without-new
//                m_shmptr[1].nextofs = 0; //eof marker
                gc(true);
                ATOMIC(std::cout << BLUE_MSG << "sizeof(mutex) = " << sizeof(std::mutex) << ", sizeof(size_t) = " << sizeof(size_t) << ", x/80xw (addr) to examine memory" << ENDCOLOR << std::flush);
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
    public:
        ~ShmHeap() { detach(); if (!m_persist) destroy(m_key); }
    public: //getters
        inline int key() const { return m_key; }
        inline size32_t size() const { return m_size; }
//        inline size_t available() const { return size() - used(); }
//        inline size_t used() const { return m_ptr->used; } // + sizeof(m_ptr->used); } //account for used space at front
    public: //cleanup
        void detach()
        {
            if (!m_shmptr) return; //not attached
//  int shmctl(int shmid, int cmd, struct shmid_ds *buf);
            if (shmdt(m_shmptr) == -1) throw std::runtime_error(strerror(errno));
            m_shmptr = 0; //can't use m_shmptr after this point
            ATOMIC(std::cout << CYAN_MSG << "ShmHeap: dettached " << ENDCOLOR << std::flush); //desc();
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
            defered_init(); //kludge: defer init until needed (avoids problems with static init)
//more info about alignment: https://en.wikipedia.org/wiki/Data_structure_alignment
            int keylen = rdup(strlen(key) + 1, alignment); //alignment for var storage
            size = rdup(keylen + size, sizeof(m_shmptr->nextofs)); //alignment for following nextofs
//            std::unique_lock<std::mutex> lock(m_shmptr->mutex);
            scoped_lock lock;
//first check if symbol already exists:
//            entry* symptr = &m_ptr->symtab[0];
//            entry* parent = (long)&m_ptr->nextofs - (long)m_ptr; //symofs = m_ptr->nextofs;
//            entry* parent = symtab(0);
            dump("before alloc:");
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
            entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs);
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
                if (!symptr->nextofs) //eof
                {
//            size = sizeof(symptr->nextofs) + keylen + rdup(size, alignment);
//            m_ptr->used = rdup(m_ptr->used, sizeof(symptr->nextofs));
//            if (available() < size) return 0; //not enough space
//                    long neweof = (long)symptr->name + keylen + rdup(size, alignment);
//alloc new storage:
                    entry* nextptr = (entry*)&symptr->name[size]; //alloc space for this var
                    size32_t remaining = (long)m_shmptr + m_size - (long)nextptr->name;
                    ATOMIC(std::cout << GREEN_MSG << "alloc key '" << key << "', size " << size << " at ofs " << shmofs(symptr) << ", remaining " << remaining << ENDCOLOR << std::flush);
                    if (remaining < 0) return 0; //not enough space
                    symptr->nextofs = shmofs(nextptr); //neweof - (long)m_ptr;
                    strcpy(symptr->name, key);
                    memset(symptr->name + keylen, 0, size - keylen); //clear storage for new var
                    nextptr->nextofs = 0; //new eof marker (in case space was reclaimed earlier)
                    dump("after alloc:");
                    return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
//                    break;
                }
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
//                size_t ofs = (long)symptr - (long)m_ptr; //(void*)symptr - (void*)m_ptr;
                ATOMIC(std::cout << BLUE_MSG << "check for key '" << key << "' ==? symbol '" << symptr->name << "' at ofs " << shmofs(symptr) << ENDCOLOR << std::flush);
                if (!strcmp(symptr->name, key)) return symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen); //break; //found
//                parent = symptr;
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
//                symptr = (long)&m_ptr->symtab[0] + symptr->nextofs;
            }
//            return (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
        }
        void dealloc(void* ptr)
        {
//            std::unique_lock<std::mutex> lock(m_shmptr->mutex);
            scoped_lock lock;
//            entry* symptr = (void*)m_ptr + m_ptr->nextofs;
//            size_t symofs = (long)&m_ptr->symtab[0] - (long)m_ptr;
            dump("before dealloc(0x%x):", shmofs(ptr));
            entry* parent = symtab(0); //m_shmptr;
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
                if (!symptr->nextofs) break; //eof
                int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
                void* varadrs = (void*)symptr->name + keylen; //static_cast<void*>((long)symptr->name + keylen);
                ATOMIC(std::cout << BLUE_MSG << "check '" << symptr->name << FMT("' ofs 0x%x") << shmofs(symptr) << " adrs " << FMT("0x%x") << varadrs << ".." << FMT("0x%x") << symptr->nextofs << " ~= dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
                if ((varadrs <= ptr) && (ptr < (void*)symtab(symptr->nextofs))) //adrs + rdup(keylen, 8))
                {
//                    symptr->nextofs = symtab(symptr->nextofs)->nextofs; //remove from chain // = (long)&m_ptr->symtab[0] + symptr->nextofs;
                    parent->nextofs = symptr->nextofs; //remove from chain
//                    if (!symtab(m_ptr->nextofs)->name[0]) gc(true); //all vars deallocated
//                    if (m_shmptr->next)
//                    if (!symtab(symptr->nextofs)->nextofs &&
//                    if (!symtab(0)->nextofs)
//                    if ((parent == m_shmptr) && !parent->nextofs m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
                    if ((parent == symtab(0)) && !symtab(symptr->nextofs)->nextofs) gc(false); //garbage collect
//                    if (!symtab(m_shmptr->nextofs)->nextofs) m_shmptr->nextofs = shmofs(&m_shmptr[1]); //gc
                    dump("after dealloc(0x%x):", shmofs(ptr));
                    return;
                }
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
            }
            throw std::runtime_error("ShmHeap: attempt to dealloc unknown address");
        }
    private: //data
        int m_key; //only used for owner
//    void* m_ptr; //ptr to start of shm
        size32_t m_size; //total size (bytes)
        typedef struct
        {
            size32_t nextofs;
            union
            {
                char name[1]; //named storage entry (name + storage space)
                std::mutex mutex; //thread/proc sync
            };
        } entry;
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
        inline entry* symtab(size32_t ofs) const { return (entry*)((long)m_shmptr + ofs); }
//        inline size32_t symofs(entry* symptr) const { return (long)symptr - (long)m_symtab; }
//        typedef struct { size_t used; std::mutex mutex; entry symtab[1]; } hdr;
//    inline entry* symtab() { return &m_ptr->symtab[0]; }
//        hdr* m_ptr; //ptr to start of shm
//        struct { size32_t nextofs; std::mutex mutex; entry symtab[1]; }* m_ptr; //shm struct ptr
//        struct { std::mutex mutex; entry symtab[1]; }* m_shmptr; //shm struct
        entry* m_shmptr; //symbol table in shm
//        struct { size_t nextofs; std::mutex mutex; }* m_ptr; //shm struct ptr
//        inline entry* symtab(size_t ofs) { return ofs? (void*)m_ptr + ofs: 0; }
        bool m_persist;
    public:
        inline size32_t shmofs(void* ptr) const { return (long)ptr - (long)m_shmptr; }
    private: //helpers
#undef dump
#undef gc
    class scoped_lock: public std::unique_lock<std::mutex>
    {
    public:
        explicit scoped_lock(): std::unique_lock<std::mutex>(m_shmptr->mutex) {};
//        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
    };
//garbage collector:
    void gc(int line, bool init) //kludge: need extra param here for __VA_ARGS__
    {
        ATOMIC(std::cout << YELLOW_MSG << "gc(" << init << ")" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//        if (init)
//        {
//            m_ptr[0].nextofs = sizeof(m_ptr[0]); //symofs(m_ptr->symtab); //[0] - (long)m_ptr;
//            m_ptr->symtab[0].name[0] = '\0'; //eof marker
//            m_shmptr->symtab[0].nextofs = 0; //eof marker
        m_shmptr[0].nextofs = shmofs(&m_shmptr[1]);
        m_shmptr[1].nextofs = 0; //eof marker
//        }
    }
//show shm details (for debug):
        const std::string /*NO &*/ desc() const
//        const char* desc() const
        {
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //symtab(m_ptr->nextofs);
            entry* parent = symtab(0); //m_shmptr;
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) break; //eof
//                if (!symptr->nextofs) throw std::runtime_error("ShmHeap: symbol chain broken");
                if (!symptr->nextofs) break; //eof
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
            }
            size32_t used = shmofs(parent); //symptr);
//            static std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
//            ostrm.str(""); ostrm.clear(); //https://stackoverflow.com/questions/5288036/how-to-clear-ostringstream
            std::ostringstream ostrm; //use static to preserve ret val after return; only one instance expected so okay to reuse
            ostrm << m_size << " bytes for shm key " << FMT("0x%x") << m_key;
            ostrm << " at " << FMT("0x%x") << m_shmptr;
            ostrm << ", used = " << used << ", avail " << (m_size - used);
//            std::cout << ostrm.str().c_str() << "\n" << std::flush;
            return ostrm.str(); //.c_str();
        }
        void dump(int line, const char* desc, long val = 0)
        {
            ATOMIC(std::cout << BLUE_MSG << "shm " << FMT(desc) << val << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//            entry* symptr = symtab(0); //symtab(m_ptr->nextofs);
//            entry* symptr = symtab(m_shmptr->nextofs); //symtab(0); //shmptr->symtab; //symtab(m_ptr->nextofs);
            entry* parent = symtab(0); //m_shmptr;
            for (;;)
            {
                entry* symptr = symtab(parent->nextofs);
//                if (!symptr->name[0]) //eof
                if (!symptr->nextofs) //eof
                {
                    ATOMIC(std::cout << BLUE_MSG << FMT("[0x%x]") << shmofs(symptr) << " eof" << ENDCOLOR << std::flush);
                    return;
                }
                int keylen = strlen(symptr->name) + 1; //NOTE: unknown alignment
                ATOMIC(std::cout << BLUE_MSG << FMT("[0x%x]") << shmofs(symptr) << " next ofs " << FMT("0x%x") << symptr->nextofs << ", name " << strlen(symptr->name) << ":'" << symptr->name << "', adrs " << FMT("0x%x") << shmofs(symptr->name + keylen) << "..+8" << FMT(ENDCOLOR_MYLINE) << line << std::flush);
//                symptr = symtab(symptr->nextofs);
                parent = symptr;
            }
        }
    };
    static ShmHeap shmheap; //use static member to reduce clutter in var instances
};
#endif


////////////////////////////////////////////////////////////////////////////////

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

#endif //ndef _SHMALLOC_H