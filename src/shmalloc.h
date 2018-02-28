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
void* shmalloc(size_t size, key_t key = 0, SRCLINE srcline = 0)
{
    if ((size < 1) || (size >= 10000000)) throw std::runtime_error("shmalloc: bad size"); //throw std::bad_alloc(); //set reasonable limits
    size += sizeof(ShmHdr);
    if (!key) key = (rand() << 16) | 0xbeef;
    int shmid = shmget(key, size, 0666 | IPC_CREAT); //create if !exist; clears to 0 upon creation
    ATOMIC(std::cout << CYAN_MSG << "shmalloc: cre shmget key " << FMT("0x%lx") << key << ", size " << size << " => " << FMT("id 0x%lx") << shmid << ENDCOLOR_LINE(srcline) << std::flush);
    if (shmid == -1) throw std::runtime_error(std::string(strerror(errno))); //failed to create or attach
    struct shmid_ds info;
    if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
    ShmHdr* ptr = static_cast<ShmHdr*>(shmat(shmid, NULL /*system choses adrs*/, 0)); //read/write access
    ATOMIC(std::cout << BLUE_MSG << "shmalloc: shmat id " << FMT("0x%lx") << shmid << " => " << FMT("%p") << ptr << ENDCOLOR << std::flush);
    if (ptr == (ShmHdr*)-1) throw std::runtime_error(std::string(strerror(errno)));
    ptr->id = shmid;
    ptr->key = key;
    ptr->size = info.shm_segsz; //size; //NOTE: size will be rounded up to a multiple of PAGE_SIZE, so get actual size
    ptr->marker = SHM_MAGIC;
    return ++ptr;
}


//get shmem key:
key_t shmkey(void* addr) { return shmptr(addr, "shmkey")->key; }


//get size:
size_t shmsize(void* addr) { return shmptr(addr, "shmsize")->size; }


//dealloc shmem:
void shmfree(void* addr, SRCLINE srcline = 0)
{
    ShmHdr* ptr = shmptr(addr, "shmfree");
    ShmHdr info = *ptr; //copy info before dettaching
//    struct shmid_ds info;
//    if (shmctl(shmid, IPC_STAT, &info) == -1) throw std::runtime_error(strerror(errno));
    if (shmdt(ptr) == -1) throw std::runtime_error(strerror(errno));
    ptr = 0; //can't use ptr after this point
//    ATOMIC(std::cout << CYAN_MSG << "shmfree: dettached " << ENDCOLOR << std::flush); //desc();
//    int shmid = shmget(key, 1, 0666); //use minimum size in case it changed
//    if ((shmid != -1) && !shmctl(shmid, IPC_RMID, NULL /*ignored*/)) return; //successfully deleted
//    if ((shmid == -1) && (errno == ENOENT)) return; //didn't exist
    if (shmctl(info.id, IPC_RMID, NULL /*ignored*/)) throw std::runtime_error(strerror(errno));
    ATOMIC(std::cout << CYAN_MSG << "shmfree: freed " << FMT("key 0x%lx") << info.key << FMT(", id 0x%lx") << info.id << ", size " << info.size << ENDCOLOR_LINE(srcline) << std::flush);
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