#!/bin/bash -x
echo -e '\e[1;36m'; g++ -D__SRCFILE__="\"${BASH_SOURCE##*/}\"" -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 -o "${BASH_SOURCE%.*}" -x c++ - <<//EOF; echo -e '\e[0m'
#line 4 __SRCFILE__ #compensate for shell commands above; NOTE: +1 needed (sets *next* line); add "-E" to above to see raw src

//shared memory allocator test
//self-compiling c++ file; run this file to compile it; //https://stackoverflow.com/questions/17947800/how-to-compile-code-from-stdin?lq=1
//how to get name of bash script: https://stackoverflow.com/questions/192319/how-do-i-know-the-script-file-name-in-a-bash-script
//or, to compile manually:
// g++  -fPIC -pthread -Wall -Wextra -Wno-unused-parameter -m64 -O3 -fno-omit-frame-pointer -fno-rtti -fexceptions  -w -Wall -pedantic -Wvariadic-macros -g -std=c++11 shmalloc.cpp -o shmalloc
// gdb ./shmalloc
// bt
// x/80xw 0x7ffff7ff7000


//#define WANT_TEST1
//#define WANT_TEST2
#define WANT_TEST3
#define SHMALLOC_DEBUG //show shmalloc debug msgs

#include "ipc.h" //put first to request ipc variants; comment out for in-proc multi-threading
#include "atomic.h" //otherwise put this one first so shared mutex will be destroyed last; ATOMIC_MSG()
#include "msgcolors.h" //SrcLine, msg colors
#include "ostrfmt.h" //FMT()
#include "elapsed.h" //timestamp()
#include "shmalloc.h" //MemPool<>, WithMutex<>, ShmAllocator<>, ShmPtr<>
//#include "shmallocator.h"


//little test class for testing shared memory:
class TestObj
{
//    std::string m_name;
    char m_name[20]; //store name directly in object so shm object doesn't use char pointer
//    SrcLine m_srcline;
    int m_count;
public:
    explicit TestObj(const char* name, SrcLine srcline = 0): /*m_name(name),*/ m_count(0) { strncpy(m_name, name, sizeof(m_name)); ATOMIC_MSG(CYAN_MSG << timestamp() << FMT("TestObj@%p") << this << " '" << name << "' ctor" << ENDCOLOR_ATLINE(srcline)); }
    TestObj(const TestObj& that): /*m_name(that.m_name),*/ m_count(that.m_count) { strcpy(m_name, that.m_name); ATOMIC_MSG(CYAN_MSG << timestamp() << FMT("TestObj@%p") << this << " '" << m_name << FMT("' copy ctor from %p") << that << ENDCOLOR); }
    ~TestObj() { ATOMIC_MSG(CYAN_MSG << timestamp() << FMT("TestObj@%p") << this << " '" << m_name << "' dtor" << ENDCOLOR); } //only used for debug
public:
    void print(SrcLine srcline = 0) { ATOMIC_MSG(BLUE_MSG << timestamp() << "TestObj.print: (name" << FMT("@%p") << &m_name /*<< FMT(" contents@%p") << m_name/-*.c_str()*/ << " '" << m_name << "', count" << FMT("@%p") << &m_count << " " << m_count << ")" << ENDCOLOR_ATLINE(srcline)); }
    int& inc() { return ++m_count; }
};


///////////////////////////////////////////////////////////////////////////////
////
/// Test 1
//

#ifdef WANT_TEST1 //shm_obj test, single proc

//#define MEMSIZE  rdup(10, 8)+8 + rdup(4, 8)+8
//WithMutex<MemPool<MEMSIZE>> pool20;
WithMutex<MemPool<rdup(10, 8)+8 + rdup(4, 8)+8>> pool20;
//typedef WithMutex<MemPool<MEMSIZE>> pool_type;
//pool_type pool20;
MAKE_TYPENAME(MemPool<32>)
MAKE_TYPENAME(MemPool<40>)
MAKE_TYPENAME(WithMutex<MemPool<40>>)


int main(int argc, const char* argv[])
{
    ATOMIC_MSG(PINK_MSG << "data space:" <<ENDCOLOR);
    void* ptr1 = pool20->alloc(10);
//    void* ptr1 = pool20()()->alloc(10);
//    void* ptr1 = pool20.base().base().alloc(10);
    void* ptr2 = pool20.alloc(4);
    pool20->free(ptr2);
    void* ptr3 = pool20->alloc(1);

    ATOMIC_MSG(PINK_MSG << "stack:" <<ENDCOLOR);
    MemPool<rdup(1, 8)+8 + rdup(1, 8)+8> pool10; //don't need mutex on stack mem (not safe to share it)
    void* ptr4 = pool10.alloc(1);
    void* ptr5 = pool10.alloc(1);
    void* ptr6 = pool10.alloc(0);

    typedef decltype(pool20) pool_type; //use same type as pool20
    ATOMIC_MSG(PINK_MSG << "shmem: actual size " << sizeof(pool_type) << ENDCOLOR);
//    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> shmpool(new (shmalloc(sizeof(pool_type))) pool_type(), shmdeleter<pool_type>());
//    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> shmpool(new (shmalloc(sizeof(pool_type))) pool_type(), shmdeleter<pool_type>());
//    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
//    pool_type shmpool = *shmpool_ptr; //.get();
//#define SHM_DECL(type, var)  type& var = *new (shmalloc(sizeof(type))) type(); std::shared_ptr<type> var##_clup(&var, shmdeleter<type>())
#if 0
    pool_type& shmpool = *new (shmalloc(sizeof(pool_type))) pool_type();
    std::shared_ptr<pool_type /*, shmdeleter<pool_type>*/> clup(&shmpool, shmdeleter<pool_type>());
//OR
    std::unique_ptr<pool_type, shmdeleter<pool_type>> clup(&shmpool, shmdeleter<pool_type>());
#endif
//    SHM_DECL(pool_type, shmpool); //equiv to "pool_type shmpool", but allocates in shmem
    shm_obj<pool_type> shmpool; //put it in shared memory instead of stack
//    ATOMIC_MSG(BLUE_MSG << shmpool.TYPENAME() << ENDCOLOR); //NOTE: don't use -> here (causes recursive lock)
//    shmpool->m_mutex.lock();
//    shmpool->debug();
//    shmpool->m_mutex.unlock();
    void* ptr7 = shmpool->alloc(10);
    void* ptr8 = shmpool->alloc(4); //.alloc(4);
    shmpool->free(ptr8);
    void* ptr9 = shmpool->alloc(1);
//    shmfree(&shmpool);

    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Test 2
//

#ifdef WANT_TEST2 //proxy example, multi-proc; BROKEN
//#include "vectorex.h"
//#include <unistd.h> //fork()
//#include "ipc.h"
#ifndef IPC_THREAD
 #error "uncomment ipc.h near top for this test"
#endif

//template <>
//const char* ShmAllocator<TestObj>::TYPENAME() { return "TestObj"; }
//template <>
//const char* ShmAllocator<std::vector<TestObj, ShmAllocator<TestObj>>>::TYPENAME() { return "std::vector<TestObj>"; }
//template<>
//const char* WithMutex<TestObj, true>::TYPENAME() { return "WithMutex<TestObj, true>"; }

#define COMMA ,  //kludge: macros don't like commas within args; from https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
MAKE_TYPENAME(WithMutex<TestObj COMMA true>)
MAKE_TYPENAME(ShmAllocator<TestObj>)
MAKE_TYPENAME(WithMutex<MemPool<300> COMMA true>)
MAKE_TYPENAME(MemPool<300>)

//typedef shm_obj<WithMutex<MemPool<PAGE_SIZE>>> ShmHeap;
//template <typename TYPE, int KEY = 0, int EXTRA = 0, bool INIT = true, bool AUTO_LOCK = true>
#include "shmkeys.h"
//#include "shmalloc.h"


//#include <sys/types.h>
//#include <sys/wait.h>
//#include <unistd.h> //fork(), getpid()
//#include "ipc.h" //IpcThread(), IpcPipe()
//#include "elapsed.h" //timestamp()
//#include <memory> //unique_ptr<>
int main(int argc, const char* argv[])
{
//    ShmKey key(12);
//    std::cout << FMT("0x%lx\n") << key.m_key;
//    key = 0x123;
//    std::cout << FMT("0x%lx\n") << key.m_key;
#if 0 //best way, but doesn't match Node.js fork()
    ShmSeg shm(0, ShmSeg::persist::NewTemp, 300); //key, persistence, size
#else //use pipes to simulate Node.js pass env to child; for example see: https://bytefreaks.net/programming-2/c-programming-2/cc-pass-value-from-parent-to-child-after-fork-via-a-pipe
//    int fd[2];
//	pipe(fd); //create pipe descriptors < fork()
    IpcPipe pipe;
#endif
//    bool owner = fork();
    IpcThread thread(SRCLINE);
//    pipe.direction(owner? 1: 0);
    if (thread.isChild()) sleep(1); //give parent head start
    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent": "child") << " pid " << getpid() << " start" << ENDCOLOR);
#if 1 //simulate Node.js fork()
//    ShmSeg& shm = *std::allocator<ShmSeg>(1); //alloc space but don't init
//    ShmAllocator<ShmSeg> heap_alloc;
//used shared_ptr<> for ref counting *and* to allow skipping ctor in child procs:
//    std::shared_ptr<ShmHeap> shmheaptr; //(ShmAllocator<ShmSeg>().allocate()); //alloc space but don't init yet
//    typedef ShmPtr<WithMutex<MemPool<PAGE_SIZE>>, HEAPPAGE_SHMKEY> ShmHeap;
    typedef ShmPtr<MemPool<PAGE_SIZE>, HEAPPAGE_SHMKEY> ShmHeap;
//    typedef ShmAllocator<TestObj, PAGE_SIZE> ShmHeap;
    ShmHeap shmheaptr;
//    ShmHeap shmheap; //(ShmAllocator<ShmSeg>().allocate()); //alloc space but don't init yet
//    ShmSeg* shmptr = heap_alloc.allocate(1); //alloc space but don't init yet
//    std::unique_ptr<ShmSeg> shm = shmptr;
//    if (owner) shmptr.reset(new /*(shmptr.get())*/ ShmHeap(0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//    if (owner) shmheaptr.reset(new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(), shmdeleter<ShmHeap>()); //[](TYPE* ptr) { shmfree(ptr); }); //0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//#define INNER_CREATE(args)  m_ptr(new (shmalloc(sizeof(TYPE))) TYPE(args), [](TYPE* ptr) { shmfree(ptr); }) //pass ctor args down into m_var ctor; deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
//    shmheaptr.reset(thread.isParent()? new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(): (ShmHeap*)shmalloc(sizeof(ShmHeap), pipe.child_rcv(SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //call ctor to init (parent only)
//    shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), thread.isParent()? 0: pipe.rcv(SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //call ctor to init (parent only)
//    if (thread.isParent()) new (shmheaptr.get()) ShmHeap(); //call ctor to init (parent only)
//    if (thread.isParent()) pipe.send(shmkey(shmheaptr.get()), SRCLINE);
    ATOMIC_MSG(BLUE_MSG << timestamp() << FMT("shmheap at %p") << shmheaptr.get() << ENDCOLOR);
//    else shmptr.reset(new /*(shmptr.get())*/ ShmHeap(rcv(fd), ShmSeg::persist::Reuse, 1));
//    else shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), rcv(fd, SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //don't call ctor; Seg::persist::Reuse, 1));
//    std::vector<std::string> args;
//    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
//    if (!owner) new (&shm) ShmSeg(shm.shmkey(), ShmSeg::persist::Reuse, 1); //attach to same shmem seg (child only)
#endif
//parent + child have ref; parent init; parent + child use it; parent + child detach; parent frees

    TestObj bare("berry", SRCLINE);
    bare.inc();
    bare.print(SRCLINE);

    WithMutex<TestObj> prot("protected", SRCLINE);
//    ((TestObj)prot).inc();
//    ((TestObj)prot).print();
    prot->inc(); //NOTE: must use operator-> to get auto-lock wrapping
    prot->print(SRCLINE);
    
//    ProxyWrapper<Person> person(new Person("testy"));
//can't set ref to rvalue :(    ShmSeg& shm = owner? ShmSeg(0x1234beef, ShmSeg::persist::NewTemp, 300): ShmSeg(0x1234beef, ShmSeg::persist::Reuse, 300); //key, persistence, size
//    if (owner) new (&shm) ShmSeg(0x1234beef, ShmSeg::persist::NewTemp, 300);
//    else new (&shm) ShmSeg(0x1234beef, ShmSeg::persist::Reuse); //key, persistence, size

//    ShmHeap<TestObj> shm_heap(shm_seg);
//    ATOMIC_MSG("shm key " << FMT("0x%lx") << shm.shmkey() << ", size " << shm.shmsize() << ", adrs " << FMT("%p") << shm.shmptr() << "\n");
//    std::set<Example, std::less<Example>, allocator<Example, heap<Example> > > foo;
//    typedef ShmAllocator<TestObj> item_allocator_type;
//    item_allocator_type item_alloc; item_alloc.m_heap = shmptr; //explicitly create so it can be reused in other places (shares state with other allocators)
    ShmAllocator<TestObj> item_alloc(shmheaptr.get()); //item_alloc.m_heap = shmptr; //explicitly create so it can be reused in other places (shares state with other allocators)
//reuse existing shm obj in child proc (bypass ctor):

//    if (owner) shmheaptr.reset(new (shmalloc(sizeof(ShmHeap), SRCLINE)) ShmHeap(), shmdeleter<ShmHeap>()); //[](TYPE* ptr) { shmfree(ptr); }); //0x123beef, ShmSeg::persist::NewTemp, 300)); //call ctor to init (parent only)
//    if (owner) send(fd, shmkey(shmheaptr.get()), SRCLINE);
//    else shmheaptr.reset((ShmHeap*)shmalloc(sizeof(ShmHeap), rcv(fd, SRCLINE), SRCLINE), shmdeleter<ShmHeap>()); //don't call ctor; Seg::persist::Reuse, 1));

    key_t svkey = shmheaptr->nextkey();
    TestObj& testobj = *(TestObj*)item_alloc.allocate(1, svkey, SRCLINE); //NOTE: ref avoids copy ctor
    ATOMIC_MSG(BLUE_MSG << timestamp() << "next key " << svkey << FMT(" => adrs %p") << &testobj << ENDCOLOR);
    if (thread.isParent()) new (&testobj) TestObj("testy", SRCLINE);
//    TestObj& testobj = owner? *new (item_alloc.allocate(1, svkey, SRCLINE)) TestObj("testy", SRCLINE): *(TestObj*)item_alloc.allocate(1, svkey, SRCLINE); //NOTE: ref avoids copy ctor
//    shm_ptr<TestObj> testobj("testy", shm_alloc);
    testobj.inc();
    testobj.inc();
    testobj.print(SRCLINE);
    ATOMIC_MSG(BLUE_MSG << timestamp() << FMT("&testobj %p") << &testobj << ENDCOLOR);

#if 0
//    typedef std::vector<TestObj, item_allocator_type> list_type;
    typedef std::vector<TestObj, ShmAllocator<TestObj>> list_type;
//    list_type testlist;
//    ShmAllocator<list_type, ShmHeap<list_type>>& list_alloc = item_alloc.rebind<list_type>.other;
//    item_allocator_type::rebind<list_type> list_alloc;
//    typedef typename item_allocator_type::template rebind<list_type>::other list_allocator_type; //http://www.cplusplus.com/forum/general/161946/
//    typedef ShmAllocator<list_type> list_allocator_type;
//    SmhAllocator<list_type, ShmHeap<list_type>> shmalloc;
//    list_allocator_type list_alloc(item_alloc); //list_alloc.m_heap = shmptr; //(item_alloc); //share state with item allocator
    ShmAllocator<list_type> list_alloc(item_alloc); //list_alloc.m_heap = shmptr; //(item_alloc); //share state with item allocator
//    list_type testobj(item_alloc); //stack variable
//    list_type* ptr = list_alloc.allocate(1);
    svkey = shmheaptr->nextkey();
//    list_type& testlist = *new (list_alloc.allocate(1, SRCLINE)) list_type(item_alloc); //(item_alloc); //custom heap variable
    list_type& testlist = *(list_type*)list_alloc.allocate(1, svkey, SRCLINE);
    if (owner) new (&testlist) list_type(item_alloc); //(item_alloc); //custom heap variable
    ATOMIC_MSG(BLUE_MSG << FMT("&list %p") << &testlist << ENDCOLOR);

    testlist.emplace_back("list1");
    testlist.emplace_back("list2");
    testlist[0].inc();
    testlist[0].inc();
    testlist[1].inc();
    testlist[0].print();
    testlist[1].print();
#endif

    ATOMIC_MSG(BLUE_MSG
        << timestamp()
        << "sizeof(berry) = " << sizeof(bare)
        << ", sizeof(prot) = " << sizeof(prot)
        << ", sizeof(test obj) = " << sizeof(testobj)
//        << ", sizeof(test list) = " << sizeof(testlist)
//        << ", sizeof(shm_ptr<int>) = " << sizeof(shm_ptr<int>)
        << ENDCOLOR);

    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent (waiting to)": "child") << " exit" << ENDCOLOR);
    if (thread.isParent()) thread.join(SRCLINE); //waitpid(-1, NULL /*&status*/, /*options*/ 0); //NOTE: will block until child state changes
    else shmheaptr.reset((ShmHeap*)0); //don't call dtor for child
    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Test 3
//

#ifdef WANT_TEST3 //generic shm usage pattern
#ifndef IPC_THREAD
 #error "uncomment ipc.h near top for this test"
#endif
//usage:
//    ShmMsgQue& msgque = *(ShmMsgQue*)shmalloc(sizeof(ShmMsgQue), shmkey, SRCLINE);
//    if (isParent) new (&msgque) ShmMsgQue(name, SRCLINE); //call ctor to init (parent only)
//    ...
//    if (isParent) { msgque.~ShmMsgQue(); shmfree(&msgque, SRCLINE); }

#include "vectorex.h"
#include "shmkeys.h"
//#include <unistd.h> //fork()
//#include "ipc.h" //IpcThread(), IpcPipe()
//#include <memory> //unique_ptr<>

//template <>
//const char* ShmAllocator<TestObj>::TYPENAME() { return "TestObj"; }
//template <>
//const char* ShmAllocator<std::vector<TestObj, ShmAllocator<TestObj>>>::TYPENAME() { return "std::vector<TestObj>"; }
//template<>
//const char* WithMutex<TestObj, true>::TYPENAME() { return "WithMutex<TestObj, true>"; }
#define COMMA ,  //kludge: macros don't like commas within args; from https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
MAKE_TYPENAME(WithMutex<TestObj COMMA true>)

//template<typename TYPE>
//TYPE& shmobj(typedef TYPE& Ref; //kludge: C++ doesn't like "&" on derivation line

int main(int argc, const char* argv[])
{
//    IpcPipe pipe; //create pipe descriptors < fork()
    IpcThread thread(SRCLINE);
//    pipe.direction(owner? 1: 0);
    if (thread.isChild()) sleep(1); //run child after parent
    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent": "child") << " pid " << getpid() << " start" << ENDCOLOR);

#if 0 //private object
    TestObj testobj("testobj", SRCLINE);
#elif 0 //explicitly shared object
//    typedef WithRefCount<TestObj> type; //NOTE: assumes parent + child access overlap
//    typedef ShmObj<TestObj> type;
    TestObj& testobj = *(TestObj*)shmalloc(sizeof(TestObj), thread.ParentKeyGen(SRCLINE), SRCLINE);
    if (thread.isParent()) thread.send(shmkey(new (&testobj) TestObj("testobj", SRCLINE)), SRCLINE); //call ctor to init, shared shmkey with child (parent only)
//    else testobj.inc();
//    std::shared_ptr<type> clup(/*!thread.isParent()? 0:*/ &testobj, [](type* ptr) { ATOMIC_MSG("bye " << ptr->numref() << "\n"); if (ptr->dec()) return; ptr->~TestObj(); shmfree(ptr, SRCLINE); });
//    type::Scope clup(testobj, thread, [](type* ptr) { ATOMIC_MSG("bye\n"); ptr->~type(); shmfree(ptr, SRCLINE); });
    std::shared_ptr<TestObj> clup(/*!thread.isParent()? 0:*/ &testobj, [thread](TestObj* ptr) { if (!thread.isParent()) return; ATOMIC_MSG("bye\n"); ptr->~TestObj(); shmfree(ptr, SRCLINE); });
#else //automatically shared object
    typedef WithMutex<TestObj> type;
//    ShmScope<type> scope(thread, SRCLINE, "testobj", SRCLINE); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    ShmScope<type, 2> scope(SRCLINE, "testobj", SRCLINE); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    type& testobj = scope.shmobj.data; //ShmObj<TestObj>("testobj", thread, SRCLINE);
parent: INIT true
child: INIT false
    ShmPtr<TestObj, TESTOBJ_SHMKEY, 0, false> testobj("testobj", SRCLINE);
//    thread.shared<TestObj> testobj("testobj", SRCLINE);
//    Shmobj<TestObj> testobj("testobj", thread, SRCLINE);

//call ctor to init, shared shmkey with child (parent only), destroy later:
//#define SHARED(ctor, thr)  \
//    *(TestObj*)shmalloc(sizeof(TestObj), thr.isParent()? 0: thr.rcv(SRCLINE), SRCLINE); \
//    if (thr.isParent()) thr.send(shmkey(new (&testobj) ctor), SRCLINE); \
//    std::shared_ptr<TestObj> dealloc(&testobj, [](TestObj* ptr){ ptr->~TestObj(); shmfree(ptr, SRCLINE); }
//    TestObj& testobj = SHARED(TestObj("shmobj", SRCLINE), thread);
//    IpcShared<TestObj> TestObj("shmobj", SRCLINE), thread);
#endif
    testobj->inc(); //testobj.inc();
    testobj->print(SRCLINE); //testobj.print();

    ATOMIC_MSG(PINK_MSG << timestamp() << (thread.isParent()? "parent (waiting to)": "child") << " exit" << ENDCOLOR);
    if (thread.isParent()) thread.join(SRCLINE); //don't let parent go out of scope before child (parent calls testobj dtor, not child); NOTE: blocks until child state changes
//    if (thread.isParent()) { testobj.~TestObj(); shmfree(&testobj, SRCLINE); } //only parent will destroy obj
    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
////
/// Test 4
//

#ifdef WANT_TEST4 //generic shm usage pattern
//#include <memory> //unique_ptr<>

#define COMMA ,  //kludge: macros don't like commas within args; from https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
MAKE_TYPENAME(WithMutex<TestObj COMMA true>)

int main(int argc, const char* argv[])
{
    ATOMIC_MSG(BLUE_MSG << "start" << ENDCOLOR);
    ShmPtr<TestObj, 0x4444beef, 100, false> objptr("shmobj", SRCLINE);
    objptr->inc();
    objptr->inc();
    objptr->inc();
    objptr->print();
//    objptr->lock();

    ATOMIC_MSG(BLUE_MSG << "finish" << ENDCOLOR);
    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////

#ifdef JUNK
//#define ShmHeap  ShmHeapAlloc::ShmHeap

//define shared memory heap:
//ShmHeap shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);
ShmHeap ShmHeapAlloc::shmheap(0x1000, ShmHeap::persist::NewPerm, 0x4567feed);


//typename ... ARGS>
//unique_ptr<T> factory(ARGS&&... args)
//{
//    return unique_ptr<T>(new T { std::forward<ARGS>(args)... });
//}
//    explicit vector_ex(std::size_t count = 0, const ALLOC& alloc = ALLOC()): std::vector<TYPE>(count, alloc) {}

#include "vectorex.h"
//#include "autoptr.h"
//#include <memory> //unique_ptr
int main()
{
#if 0
    typedef vector_ex<int, ShmAllocator<int>> vectype;
    static vectype& ids = //SHARED(SRCKEY, vectype, vectype);
        *(vectype*)ShmHeapAlloc::shmheap.alloc(SRCKEY, sizeof(vectype), 4 * sizeof(int), true, [] (void* shmaddr) { new (shmaddr) vectype; })
#else
//    ShmObj<vector_ex<int>, 0x5678feed, 4 * sizeof(int)> vect; //, __LINE__> vect;
//pool + "new" pattern from: https://stackoverflow.com/questions/20945439/using-operator-new-and-operator-delete-with-a-custom-memory-pool-allocator
//    ShmAllocator<vector_ex<int>> shmalloc;
//    unique_ptr<ShmObj<vector_ex<int>>> vectp = new (shmalloc) ShmObj<vector_ex<int>>();
    unique_ptr<ShmObj<vector_ex<int>>> vectp = new ShmObj<vector_ex<int>>();
#define vect  (*vectp)
//    ShmPtr<vector_ex<int>> vectp = new vector_ex<int>();
//    ShmObj<vector_ex<int>> vect;
#endif
    std::cout << "&vec " << FMT("%p") << &vect
        << FMT(", shmkey 0x%lx") << vect.allocator.shmkey()
        << ", sizeof(vect) " << sizeof(vect)
//        << ", extra " << vect.extra
        << ", #ents " << vect.size()
//        << ", srcline " << vect.srcline
        << "\n" << std::flush;
    for (int n = 0; n < 2; ++n) vect.emplace_back(100 + n);
    int ofs = vect.find(100);
    if (ofs == -1) throw std::runtime_error(RED_MSG "can't find entry" ENDCOLOR);
    vect.push_back(10);
    vect.push_back(12);
    std::cout << "sizeof(vect) " << sizeof(vect)
        << ", #ents " << vect.size() << "\n" << std::flush;
    vect.push_back(10);
    std::cout << "sizeof(vect) " << sizeof(vect)
        << ", #ents " << vect.size() << "\n" << std::flush;
//    std::unique_lock<std::mutex> lock(shmheap.mutex()); //low usage; reuse mutex
//    std::cout << "hello from " << __FILE__ << ":" << __LINE__ << "\n" << std::flush;
}
//#define main   mainx
#endif

#if 0
class Complex: public ShmHeapAlloc
{
public:
    Complex() {} //needed for custom new()
    Complex (double a, double b): r (a), c (b) {}
private:
    double r; // Real Part
    double c; // Complex Part
public:
};


#define NUM_LOOP  5 //5000
#define NUM_ENTS  5 //1000
int main(int argc, char* argv[]) 
{
    Complex* array[NUM_ENTS];
    std::cout << timestamp() << "start\n" << std::flush;
    for (int i = 0; i < NUM_LOOP; i++)
    {
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new(__FILE__, __LINE__ + j) Complex (i, j); //kludge: force unique key for shmalloc
        for (int j = 0; j < NUM_ENTS; j++) array[j] = new_SHM(+j) Complex (i, j); //kludge: force unique key for shmalloc
//        for (int j = 0; j < NUM_ENTS; j++) array[j] = new(shmheap.alloc(sizeof(Complex), __FILE__, __LINE__ + j) Complex (i, j); //kludge: force unique key for shmalloc
        for (int j = 0; j < NUM_ENTS; j++) delete array[j]; //FIFO
//        for (int j = 0; j < NUM_ENTS; j++) delete array[NUM_ENTS-1 - j]; //LIFO
//        for (int j = 0; j < NUM_ENTS; j++) array[j]->~Complex(); //FIFO
    }
    std::cout << timestamp() << "finish\n" << std::flush;
//    std::cout << "#alloc: " << Complex::nalloc << ", #free: " << Complex::nfree << "\n" << std::flush;
    return 0;
}
#endif


//eof
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