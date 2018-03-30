//shmem keys

#ifndef _SHMKEYS_H
#define _SHMKEYS_H

//shmem keys:
//use consts to avoid the need to pass dynamic keys between procs (avoids rigid child proc protocols)
#define ATOMIC_MSG_SHMKEY  0x100DECAF
#define HEAPPAGE_SHMKEY  0x200DECAF
#define TESTOBJ_SHMKEY  0x300DECAF
#define SHARED_CRITICAL_SHMKEY  0x400DECAF

#endif //ndef _SHMKEYS_H

//eof