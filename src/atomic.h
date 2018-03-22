//atomic stmt wrapper:

#ifndef _ATOMIC_H
#define _ATOMIC_H

#include <mutex>
#include <iostream> //std::cout, std::flush

//atomic operations:
#ifdef IPC_THREAD
 #error TBD
 ShmScope<MsgQue> mscope(SRCLINE, SHMKEY1, "mainq"), wscope(SRCLINE, SHMKEY2, "wkerq"); //, mainq.mutex());
 MsgQue& mainq = mscope.shmobj.data;
 
#else
//std::recursive_mutex atomic_mut;
 std::mutex atomic_mut; //in-process mutex (all threads have same address space)
#endif

//use macro so any stmt can be nested within scoped lock:
#define ATOMIC(stmt)  { std::unique_lock<std::mutex> lock(atomic_mut); stmt; }


#define ATOMIC_MSG(msg)  ATOMIC(std::cout << msg << std::flush)

#endif //ndef _ATOMIC_H