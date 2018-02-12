//atomic wrapper:

#ifndef _ATOMIC_H
#define _ATOMIC_H

//atomic operations:
//std::recursive_mutex atomic_mut;
#include <mutex>
std::mutex atomic_mut;
//use macro so stmt can be nested within scoped lock:
#define ATOMIC(stmt)  { std::unique_lock<std::mutex> lock(atomic_mut); stmt; }

#endif //ndef _ATOMIC_H