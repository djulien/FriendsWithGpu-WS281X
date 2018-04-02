//shmem keys

#ifndef _SHMKEYS_H
#define _SHMKEYS_H

//shmem keys:
//use consts to avoid the need to pass dynamic keys between procs (avoids rigid child proc protocols)
#define ATOMIC_MSG_SHMKEY  0x100DECAF
#define HEAPPAGE_SHMKEY  0x200DECAF
#define TESTOBJ_SHMKEY  0x300DECAF
#define SHARED_CRITICAL_SHMKEY  0x400DECAF
#define THRIDS_SHMKEY  0x500DECAF

#define SHMKEY1  0x1111beef
#define SHMKEY2  0x2222beef
#define SHMKEY3  0x3333beef

#endif //ndef _SHMKEYS_H

//#ifdef WANT_CLUP_SCRIPT
// ipcrm -M 0x1111beef
// ipcrm -M 0x2222beef
// ipcrm -M 0x3333beef
//#endif

//eof