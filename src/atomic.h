//atomic stmt wrapper:
//wraps a thread- or ipc-safe critical section of code with a mutex lock/unlock

#ifndef _ATOMIC_H
#define _ATOMIC_H

#include <iostream> //std::cout, std::flush
#include <mutex> //std::mutex, std::unique_lock
#include "msgcolors.h" //SrcLine and colors (used with ATOMIC_MSG)


//atomic operations:
#ifdef IPC_THREAD
// #error TBD
// #include "shmkeys.h"
// #include "shmalloc.h"

// ShmPtr<std::mutex, ATOMIC_MSG_SHMKEY, 0, true, false> atomic_mut_ptr; //ipc mutex (must be in shared memory)
// std::mutex& atomic_mut = *atomic_mut_ptr.get();
// std::mutex& atomic_mut; //fwd ref
 std::mutex& get_atomic_mut(); //fwd ref
 #define atomic_mut  get_atomic_mut()
// ShmScope<MsgQue> mscope(SRCLINE, SHMKEY1, "mainq"), wscope(SRCLINE, SHMKEY2, "wkerq"); //, mainq.mutex());
// MsgQue& mainq = mscope.shmobj.data;
#else
//std::recursive_mutex atomic_mut;
 std::mutex atomic_mut; //in-process mutex (all threads have same address space)
#endif

//use macro so any stmt can be nested within scoped lock:
#define ATOMIC(stmt)  { std::unique_lock<std::mutex> lock(atomic_mut); stmt; }

#define ATOMIC_MSG(msg)  ATOMIC(std::cout << msg << std::flush)


#ifdef IPC_THREAD //NOTE: need to put these *after* ATOMIC_MSG def
// #error TBD
 #include "shmkeys.h"
 #include "shmalloc.h"
 ShmPtr<std::mutex, ATOMIC_MSG_SHMKEY, 0, true, false> atomic_mut_ptr; //ipc mutex (must be in shared memory)
// std::mutex& atomic_mut = *atomic_mut_ptr.get();
 std::mutex& get_atomic_mut() { return *atomic_mut_ptr.get(); }
#endif //def IPC_THREAD


#endif //ndef _ATOMIC_H