//malloc emulator for shared memory

#ifndef _SHMALLOC_H
#define _SHMALLOC_H

#include <iostream> //stsd::cout, std::flush
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdexcept>
#include "colors.h"
#include "ostrfmt.h"
#include "atomic.h"
#include "elapsed.h" //timestamp()

//to look at shm:
// ipcs -m 
//to delete shm:
// ipcrm -M <key>


//stash some info within shmem block returned to caller:
#define SHM_MAGIC  0xfeedbeef //marker to detcet valid shmem block
typedef struct { int id; key_t key; size_t size; uint32_t marker; } ShmHdr;


//check if shmem ptr is valid:
static ShmHdr* shmptr(void* addr, const char* func)
{
    ShmHdr* ptr = static_cast<ShmHdr*>(addr); --ptr;
    if (ptr->marker == SHM_MAGIC) return ptr;
    char buf[50];
    snprintf(buf, sizeof(buf), "%s: bad shmem pointer %p", func, addr);
    throw std::runtime_error(buf);
}


//allocate memory:
void* shmalloc(size_t size, key_t key = 0, SrcLine srcline = 0)
{
    if ((size < 1) || (size >= 10000000)) throw std::runtime_error("shmalloc: bad size"); //throw std::bad_alloc(); //set reasonable limits
    size += sizeof(ShmHdr);
    if (!key) key = (rand() << 16) | 0xbeef;
    int shmid = shmget(key, size, 0666 | IPC_CREAT); //create if !exist; clears to 0 upon creation
    ATOMIC_MSG(CYAN_MSG << timestamp() << "shmalloc: cre shmget key " << FMT("0x%lx") << key << ", size " << size << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR_ATLINE(srcline));
    if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
    struct shmid_ds shminfo;
    if (shmctl(shmid, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
    ShmHdr* ptr = static_cast<ShmHdr*>(shmat(shmid, NULL /*system choses adrs*/, 0)); //read/write access
    ATOMIC_MSG(BLUE_MSG << timestamp() << "shmalloc: shmat id " << FMT("0x%lx") << shmid << " => " << FMT("%p") << ptr << ", cre by pid " << shminfo.shm_cpid << ", #att " << shminfo.shm_nattch << ENDCOLOR);
    if (ptr == (ShmHdr*)-1) throw std::runtime_error(std::string(strerror(errno)));
    ptr->id = shmid;
    ptr->key = key;
    ptr->size = shminfo.shm_segsz; //size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
    ptr->marker = SHM_MAGIC;
    return ++ptr;
}
void* shmalloc(size_t size, SrcLine srcline = 0) { return shmalloc(size, 0, srcline); }


//get shmem key:
key_t shmkey(void* addr) { return shmptr(addr, "shmkey")->key; }


//get size:
size_t shmsize(void* addr) { return shmptr(addr, "shmsize")->size; }


//dealloc shmem:
void shmfree(void* addr, SrcLine srcline = 0)
{
    ShmHdr* ptr = shmptr(addr, "shmfree");
    ATOMIC_MSG(CYAN_MSG << timestamp() << FMT("shmfree: adrs %p") << addr << FMT(" = ptr %p") << ptr << ENDCOLOR_ATLINE(srcline));
    ShmHdr info = *ptr; //copy info before dettaching
//    struct shmid_ds info;
//    if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
    if (shmdt(ptr) == -1) throw std::runtime_error(strerror(errno));
    ptr = 0; //can't use ptr after this point
//    ATOMIC_MSG(CYAN_MSG << "shmfree: dettached " << ENDCOLOR); //desc();
//    int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
//    if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
//    if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
    struct shmid_ds shminfo;
    if (shmctl(info.id, IPC_STAT, &shminfo) == -1) throw std::runtime_error(strerror(errno));
    if (!shminfo.shm_nattch) //no more procs attached, delete it
        if (shmctl(info.id, IPC_RMID, NULL /*ignored*/)) throw std::runtime_error(strerror(errno));
    ATOMIC_MSG(CYAN_MSG << timestamp() << "shmfree: freed " << FMT("key 0x%lx") << info.key << FMT(", id 0x%lx") << info.id << ", size " << info.size << ", cre pid " << shminfo.shm_cpid << ", #att " << shminfo.shm_nattch << ENDCOLOR_ATLINE(srcline));
}


//std::Deleter wrapper for shmfree:
//based on example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
template <typename TYPE>
struct shmdeleter
{ 
    void operator() (TYPE* ptr) const { shmfree(ptr); }
//    {
//        std::cout << "Call delete from function object...\n";
//        delete p;
//    }
};


#endif //ndef _SHMALLOC_H