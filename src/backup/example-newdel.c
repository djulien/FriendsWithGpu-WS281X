//examples from https://www.daniweb.com/programming/software-development/code/252173/c-operator-new-gets-memory-in-shared-memory


// sets data in shared memory...see below for gets data from shared memory
// rather than carrying pointers to various shared
// memory segments, encapsulate!
// Avoid the mismash of "C" programming with pointers lying
// all around!
//
// class Segment overloads operator new to acquire its storage
// in shared memory rather than on the heap.
// How could the client tell?
// the main program looks normal as if the memory
// were allocated on the heap.
// try unix commands "ipcs" and "ipcrm"
// this program works under kernel FC5 2.6.16-1.2133_FC5
// compile via: g++ -o NewSharedMem NewSharedMem.cpp
// run via: NewSharedMem
#include <cstddef> // Size_t
#include <fstream>
#include <iostream>
#include <new>
#include <sys/ipc.h>
#include <sys/shm.h>
#define  SHMKEY  ((key_t) 7890) /* base value for shmem key */
#define  PERMS 0666
using namespace std;
ofstream out("Segment.out");
class Segment
{
static void* voidPtr;
    int iii[1024];
public:
    int get(int i);
    void set(int i, int value);
    Segment() { cout << "Segment()\n"; }
    ~Segment() { cout << "~Segment() ... "; }
// operator new and delete are automatically static
    void* operator new(size_t) throw(bad_alloc);
    void operator delete(void*);
};
int Segment::get(int i)
{
// protect critical shared memory
// pthread_mutex_lock
    return iii[i];
// pthread_mutex_unlock
}
void Segment::set(int i, int value)
{
// protect critical shared memory
// pthread_mutex_lock
    iii[i] = value;
// pthread_mutex_unlock
}
void* Segment::operator new(size_t sz) throw(bad_alloc)
{
    cout<<"sz = "<<sz<<endl;
    int id = shmget(SHMKEY , sz,
        PERMS | IPC_CREAT) ;
    cout<<"id = "<<id<<endl;
    if(id<0) throw bad_alloc();
    voidPtr = (void *) shmat(id, (void *) 0, 0);
    cout<<"voidPtr = "<<hex<<voidPtr<<" sz = "<<sz<<endl;
    if(voidPtr == (void*)0xffffffff) throw bad_alloc();
    cout<<"operator new: "<<sz<<" Bytes"<<endl;
    void* m = voidPtr;
    return m;
}
void* Segment::voidPtr = 0;
void Segment::operator delete(void* m)
{
    cout<<"operator delete"<<endl;
    if (shmdt(voidPtr) < 0)
    {
        cout<<"server: can't detach shared memory "<< hex<<voidPtr<<endl;
        return;
    }
    cout<<"server: success! detached shared memory "<< hex<<voidPtr<<endl;
}
int main()
{
    Segment* psegment;
    try
    {
        psegment = new Segment;
    }
    catch(...)
    {
        cout<<"bad mem alloc"<<endl;
        return -1;
    }
    for(int i = 0; i<1024; i++)
        psegment->set(i,i*2);
int i;
cin>>i;
    cout<<hex<<psegment<<endl;
    delete psegment;
    return -1;
}


// gets data from shared memory
// rather than carrying pointers to various shared
// memory segments, encapsulate!
// Avoid the mismash of "C" programming with pointers lying
// all around!
//
// class Segment overloads operator new to acquire its storage
// in shared memory rather than on the heap.
// How could the client tell?
// the main program looks normal as if the memory
// were allocated on the heap.
// try unix commands "ipcs" and "ipcrm"
// this program works under kernel FC5 2.6.16-1.2133_FC5
// compile via: g++ -o NewSharedMem NewSharedMem.cpp
// run via: NewSharedMem
#include <cstddef> // Size_t
#include <fstream>
#include <iostream>
#include <new>
#include <sys/ipc.h>
#include <sys/shm.h>
#define  SHMKEY  ((key_t) 7890) /* base value for shmem key */
#define  PERMS 0666
using namespace std;
ofstream out("Segment.out");
class Segment
{
static void* voidPtr;
    int iii[1024];
public:
    int get(int i);
    void set(int i, int value);
    Segment() { cout << "Segment()\n"; }
    ~Segment() { cout << "~Segment() ... "; }
// operator new and delete are automatically static
    void* operator new(size_t) throw(bad_alloc);
    void operator delete(void*);
};
int Segment::get(int i)
{
// protect critical shared memory
// pthread_mutex_lock
    return iii[i];
// pthread_mutex_unlock
}
void Segment::set(int i, int value)
{
// protect critical shared memory
// pthread_mutex_lock
    iii[i] = value;
// pthread_mutex_unlock
}
void* Segment::operator new(size_t sz) throw(bad_alloc)
{
    cout<<"sz = "<<sz<<endl;
    int id = shmget(SHMKEY , sz,
        PERMS | IPC_CREAT) ;
    cout<<"id = "<<id<<endl;
    if(id<0) throw bad_alloc();
    voidPtr = (void *) shmat(id, (void *) 0, 0);
    cout<<"voidPtr = "<<hex<<voidPtr<<" sz = "<<sz<<endl;
    if(voidPtr == (void*)0xffffffff) throw bad_alloc();
    cout<<"operator new: "<<sz<<" Bytes"<<endl;
    void* m = voidPtr;
    return m;
}
void* Segment::voidPtr = 0;
void Segment::operator delete(void* m)
{
    cout<<"operator delete"<<endl;
    if (shmdt(voidPtr) < 0)
    {
        cout<<"server: can't detach shared memory "<< hex<<voidPtr<<endl;
        return;
    }
    cout<<"server: success! detached shared memory "<< hex<<voidPtr<<endl;
}
int main()
{
    Segment* psegment;
    try
    {
        psegment = new Segment;
    }
    catch(...)
    {
        cout<<"bad mem alloc"<<endl;
        return -1;
    }
//    for(int i = 0; i<1024; i++)
//        psegment->set(i,i*2);
    for(int i=0; i<24; i++)
        cout<<dec<<psegment->get(i)<<endl;
    cout<<hex<<psegment<<endl;
    delete psegment;
    return -1;
}

