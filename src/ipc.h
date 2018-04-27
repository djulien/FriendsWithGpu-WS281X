//simulate std::thread using a forked process:

#ifndef _IPC_H
#define _IPC_H

//#define IPC_THREAD //tell other modules to use ipc threads instead of in-proc threads

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> //fork, getpid()
#include <algorithm> //std::find()
#include <vector> //std::vector<>
#include <stdexcept> //std::runtime_error
#include <memory> //std::shared_ptr<>
#include <thread> //std::this_thread


#ifdef IPC_DEBUG
 #include "atomic.h" //ATOMIC_MSG()
 #include "ostrfmt.h" //FMT()
 #include "elapsed.h" //timestamp()
 #include "msgcolors.h" //SrcLine, msg colors
 #define DEBUG_MSG  ATOMIC_MSG
#else
 #define DEBUG_MSG(msg)  {} //noop
 #include "msgcolors.h" //SrcLine, msg colors
#endif


#ifndef WANT_IPC
 #define WANT_IPC  true
#endif


template <bool, typename = void>
class IpcPipe
{
public: //ctor/dtor
    explicit IpcPipe(SrcLine srcline = 0) { DEBUG_MSG(BLUE_MSG << timestamp() << "!IpcPipe ctor" << ENDCOLOR_ATLINE(srcline)); }
    ~IpcPipe() { DEBUG_MSG(BLUE_MSG << timestamp() << "!IpcPipe dtor" << ENDCOLOR); }
public: //methods
//no    const int Parent2Child = 0, Child2Parent = 1;
    const int ReadEnd = 0, WriteEnd = 1;
    void send(int value, SrcLine srcline = 0) { throw std:runtime_error("no pipe to send"); }
    int rcv(SrcLine srcline = 0) { throw std:runtime_error("no pipe to rcv"); }
};


//ipc msgs:
//NOTE: must be created before fork()
template <bool IPC = true> //default true, otherwise called probably wouldn't have #included this file
class IpcPipe<IPC, std::enable_if_t<IPC>>
{
private: //data members
    int m_fd[2];
//    bool m_open[2];
public: //ctor/dtor
    explicit IpcPipe(SrcLine srcline = 0): m_fd{-1, -1} { DEBUG_MSG(BLUE_MSG << timestamp() << "IpcPipe ctor" << ENDCOLOR_ATLINE(srcline)); pipe(m_fd); } //m_open[0] = m_open[1] = true; } //create pipe descriptors < fork()
    ~IpcPipe() { pipe_close(ReadEnd); pipe_close(WriteEnd); DEBUG_MSG(BLUE_MSG << timestamp() << "IpcPipe dtor" << ENDCOLOR); }
public: //methods
//no    const int Parent2Child = 0, Child2Parent = 1;
    const int ReadEnd = 0, WriteEnd = 1;
//    void direction(int which) { pipe_close(1 - which); } //which one is writer
//    void parent_send(int value, SrcLine srcline = 0) { pipe_send(Parent2Child, value, srcline); }
//    void child_send(int value, SrcLine srcline = 0) { pipe_send(Child2Parent, value, srcline); }
//    int parent_rcv(SrcLine srcline = 0) { return pipe_rcv(Child2Parent, srcline); }
//    int child_rcv(SrcLine srcline = 0) { return pipe_rcv(Parent2Child, srcline); }
    void send(int value, SrcLine srcline = 0)
    {
        int which = WriteEnd;
//        direction(which);
        pipe_close(1 - which);
//DEBUG_MSG("pipe write[" << which << "]" << ENDCOLOR);
        ssize_t wrlen = write(m_fd[which], &value, sizeof(value));
        DEBUG_MSG(((wrlen == sizeof(value))? BLUE_MSG: RED_MSG) << timestamp() << "parent '" << getpid() << "' send len " << wrlen << FMT(", value 0x%lx") << value << " to child" << ENDCOLOR_ATLINE(srcline));
        if (wrlen == -1) throw std::runtime_error(strerror(errno)); //write failed
//        close(m_fd[1]);
    }
    int rcv(SrcLine srcline = 0)
    {
        int which = ReadEnd;
        int retval = -1;
//        direction(1 - which); //close write side of pipe; child is read-only
        pipe_close(1 - which);
//DEBUG_MSG("pipe read[" << which << "]" << ENDCOLOR);
        ssize_t rdlen = read(m_fd[which], &retval, sizeof(retval)); //NOTE: blocks until data received
        DEBUG_MSG(((rdlen == sizeof(retval))? BLUE_MSG: RED_MSG) << timestamp() << "child '" << getpid() << "' rcv len " << rdlen << FMT(", value 0x%lx") << retval << " from parent" << ENDCOLOR_ATLINE(srcline));
        if (rdlen == -1) throw std::runtime_error(strerror(errno)); //read failed
//        close(m_fd[0]);
        return retval;
    }
private: //helpers:
    void pipe_close(int which)
    {
//        if (!m_open[which]) return;
        if (m_fd[which] == -1) return;
//DEBUG_MSG("pipe close[" << which << "]" << ENDCOLOR);
        close(m_fd[which]);
        m_fd[which] = -1;
    }
};


template <bool, typename = void>
class IpcThread: public std::thread
{
public: //ctor/dtor
    explicit IpcPipe(SrcLine srcline = 0) { DEBUG_MSG(BLUE_MSG << timestamp() << "!IpcThread ctor" << ENDCOLOR_ATLINE(srcline)); }
    ~IpcPipe() { DEBUG_MSG(BLUE_MSG << timestamp() << "!IpcThread dtor" << ENDCOLOR); }
public: //methods
    typedef void (*_Callable)(void); //void* data); //TODO: make generic?
//    template<typename _Callable, typename... _Args>
//    explicit IpcThread(_Callable&& entpt, _Args&&... args)
    explicit IpcThread(SrcLine srcline = 0): IpcThread(*new IpcPipe(srcline), srcline) {}
    explicit IpcThread(_Callable/*&*/ entpt, SrcLine srcline = 0): IpcThread(entpt, *new IpcPipe(srcline), srcline) {}
    explicit IpcThread(IpcPipe& pipe, SrcLine srcline = 0): m_anychild(false) //: m_pipe(pipe)
    {
        all.push_back(this); //keep track of instances; let caller use global collection
        m_pid = fork();
        m_pipe.reset(&pipe); //NOTE: must cre pipe < fork
//        std::cout << "thread here: is parent? " << !!m_pid << ", me " << getpid() << "\n" << std::flush;
//        const char* proctype = isChild()? "child": isError()? "error": "parent"; //isParent()? "parent": "child";
#pragma message("there's a recursion problem here")
        DEBUG_MSG(YELLOW_MSG << timestamp() << "fork (" << proctype() << "): child pid = " << (isParent()? m_pid: getpid()) << ENDCOLOR_ATLINE(srcline));
//        std::cout << "thread here2: is parent? " << !!m_pid << ", me " << getpid() << "\n" << std::flush;
        if (isError()) throw std::runtime_error(strerror(errno)); //fork failed
    }
    explicit IpcThread(_Callable/*&*/ entpt, IpcPipe& pipe /*= IpcPipe()*/, SrcLine srcline = 0): IpcThread(pipe, srcline) //, _Args&&... args)
    {
        if (!isChild()) return; //parent or error
        DEBUG_MSG(GREEN_MSG << timestamp() << "child " << getpid() << " calling entpt" << ENDCOLOR);
        (*entpt)(/*args*/); //call child main()
        DEBUG_MSG(RED_MSG << timestamp() << "child " << getpid() << " exit" << ENDCOLOR);
        exit(0); //kludge; don't want to execute remainder of caller
    }
    ~IpcThread()
    {
        if (isParent()) join(SRCLINE); //waitpid(-1, NULL /*&status*/, /*options*/ 0); //NOTE: will block until child state changes
        auto inx = find(all.begin(), all.end(), this);
        if (inx != all.end()) all.erase(inx);
    }
public: //std::thread emulation
    typedef pid_t id;
    static id get_id(void) { return getpid(); }
//    IpcPipe& pipe() { return m_pipe; }
public: //methods
    bool isParent() const { return !isChild() && !isError(); } //(m_pid != 0); }
    bool isChild() const { return (m_pid == 0); }
    bool isError() const { return (m_pid == -1); }
    const char* proctype() { return isChild()? "child": isError()? "error": "parent"; } //isParent()? "parent": "child";
//    key_t ParentKeyGen(SrcLine srcline = 0) { return isParent()? 0: rcv(srcline); } //generate parent key
    void send(int value, SrcLine srcline = 0) { m_pipe->send(value, srcline); }
    int rcv(SrcLine srcline = 0) { return m_pipe->rcv(srcline); }
    void allow_any() { m_anychild = true; } //m_pid = -1; } //allow any child to wake up join()
    void join(SrcLine srcline = 0)
    {
        int status, waitfor = m_anychild? -1: m_pid;
        if (!isParent()/* || isError()*/) throw std::runtime_error(/*RED_MSG*/ "join (!parent): no process to join" /*ENDCOLOR*/);
        DEBUG_MSG(YELLOW_MSG << timestamp() << "join: wait for pid " << waitfor << ENDCOLOR_ATLINE(srcline));
        waitpid(waitfor, &status, /*options*/ 0); //NOTE: will block until child state changes
    }

//no    const int Parent2Child = 0, Child2Parent = 1;
    const int ReadEnd = 0, WriteEnd = 1;
    void send(int value, SrcLine srcline = 0) { throw std:runtime_error("no pipe to send"); }
    int rcv(SrcLine srcline = 0) { throw std:runtime_error("no pipe to rcv"); }
};


//ipc msgs:
//NOTE: must be created before fork()
template <bool IPC = true> //default true, otherwise called probably wouldn't have #included this file
class IpcThread<IPC, std::enable_if_t<IPC>>
{
public: //ctor/dtor
    typedef void (*_Callable)(void); //void* data); //TODO: make generic?
//    template<typename _Callable, typename... _Args>
//    explicit IpcThread(_Callable&& entpt, _Args&&... args)
    explicit IpcThread(SrcLine srcline = 0): IpcThread(*new IpcPipe(srcline), srcline) {}
    explicit IpcThread(_Callable/*&*/ entpt, SrcLine srcline = 0): IpcThread(entpt, *new IpcPipe(srcline), srcline) {}
    explicit IpcThread(IpcPipe& pipe, SrcLine srcline = 0): m_anychild(false) //: m_pipe(pipe)
    {
        all.push_back(this); //keep track of instances; let caller use global collection
        m_pid = fork();
        m_pipe.reset(&pipe); //NOTE: must cre pipe < fork
//        std::cout << "thread here: is parent? " << !!m_pid << ", me " << getpid() << "\n" << std::flush;
//        const char* proctype = isChild()? "child": isError()? "error": "parent"; //isParent()? "parent": "child";
#pragma message("there's a recursion problem here")
        DEBUG_MSG(YELLOW_MSG << timestamp() << "fork (" << proctype() << "): child pid = " << (isParent()? m_pid: getpid()) << ENDCOLOR_ATLINE(srcline));
//        std::cout << "thread here2: is parent? " << !!m_pid << ", me " << getpid() << "\n" << std::flush;
        if (isError()) throw std::runtime_error(strerror(errno)); //fork failed
    }
    explicit IpcThread(_Callable/*&*/ entpt, IpcPipe& pipe /*= IpcPipe()*/, SrcLine srcline = 0): IpcThread(pipe, srcline) //, _Args&&... args)
    {
        if (!isChild()) return; //parent or error
        DEBUG_MSG(GREEN_MSG << timestamp() << "child " << getpid() << " calling entpt" << ENDCOLOR);
        (*entpt)(/*args*/); //call child main()
        DEBUG_MSG(RED_MSG << timestamp() << "child " << getpid() << " exit" << ENDCOLOR);
        exit(0); //kludge; don't want to execute remainder of caller
    }
    ~IpcThread()
    {
        if (isParent()) join(SRCLINE); //waitpid(-1, NULL /*&status*/, /*options*/ 0); //NOTE: will block until child state changes
        auto inx = find(all.begin(), all.end(), this);
        if (inx != all.end()) all.erase(inx);
    }
public: //std::thread emulation
    typedef pid_t id;
    static id get_id(void) { return getpid(); }
//    IpcPipe& pipe() { return m_pipe; }
public: //methods
    bool isParent() const { return !isChild() && !isError(); } //(m_pid != 0); }
    bool isChild() const { return (m_pid == 0); }
    bool isError() const { return (m_pid == -1); }
    const char* proctype() { return isChild()? "child": isError()? "error": "parent"; } //isParent()? "parent": "child";
//    key_t ParentKeyGen(SrcLine srcline = 0) { return isParent()? 0: rcv(srcline); } //generate parent key
    void send(int value, SrcLine srcline = 0) { m_pipe->send(value, srcline); }
    int rcv(SrcLine srcline = 0) { return m_pipe->rcv(srcline); }
    void allow_any() { m_anychild = true; } //m_pid = -1; } //allow any child to wake up join()
    void join(SrcLine srcline = 0)
    {
        int status, waitfor = m_anychild? -1: m_pid;
        if (!isParent()/* || isError()*/) throw std::runtime_error(/*RED_MSG*/ "join (!parent): no process to join" /*ENDCOLOR*/);
        DEBUG_MSG(YELLOW_MSG << timestamp() << "join: wait for pid " << waitfor << ENDCOLOR_ATLINE(srcline));
        waitpid(waitfor, &status, /*options*/ 0); //NOTE: will block until child state changes
    }
#if 0
public: //nested class shm wrapper
    template<typename SHOBJ_TYPE>
    class shared: public SHOBJ_TYPE&
    {
    public:
//        PERFECT_FWD2BASE_CTOR(shared, SHOBJ_TYPE&) {}
        template<typename ... ARGS>
        shared(ARGS&& ... args): SHOBJ_TYPE&(*(SHOBJ_TYPE*)shmalloc(sizeof(SHOBJ_TYPE), std::forward<ARGS>(args) ...) {}
    TestObj& testobj = *(TestObj*)shmalloc(sizeof(TestObj), thread.isParent()? 0: thread.rcv(SRCLINE), SRCLINE);
    if (thread.isParent()) thread.send(shmkey(new (&testobj) TestObj("testobj", SRCLINE)), SRCLINE); //call ctor to init, shared shmkey with child (parent only)
    thread.shared<TestObj> testobj("testobj", SRCLINE);
    };
#endif
private: //data
    pid_t m_pid; //child pid (valid in parent only)
    bool m_anychild;
    std::shared_ptr<IpcPipe> m_pipe;
public:
    static std::vector<IpcThread*> all;
};
std::vector<IpcThread*> IpcThread::all;


///////////////////////////////////////////////////////////////////////////////
////
/// Scoped shmem wrapper
//

#if 0 //circular dependency; use ShmPtr<> instead
#include "shmalloc.h"

//shm object wrapper:
//deallocates when parent (owner) ipc thread goes out of scope
//usage:
//    ShmScope<type> scope(SRCLINE, "testobj", SRCLINE); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    type& testobj = scope.shmobj; //ShmObj<TestObj>("testobj", thread, SRCLINE);
template <typename TYPE, int NUM_INST = 1>
class ShmScope
{
    typedef struct { int count; TYPE data; } type; //count #ctors, #dtors (ref count)
public: //ctor/dtor
//    TestObj& testobj = *(TestObj*)shmalloc(sizeof(TestObj), thread.isParent()? 0: pipe.rcv(SRCLINE), SRCLINE);
//    if (thread.isParent()) { testobj.~TestObj(); shmfree(&testobj, SRCLINE); } //only parent will destroy obj
//    PERFECT_FWD2BASE_CTOR(shared, SHOBJ_TYPE&) {}
#define thread  IpcThread::all[0]
#define has_threads  IpcThread::all.size()
    template<typename ... ARGS>
    explicit ShmScope(/*IpcThread& thread,*/ SrcLine srcline = 0, key_t shmkey = 0, ARGS&& ... args): shmobj(*(type*)::shmalloc(sizeof(type), shmkey? shmkey: (!has_threads || thread->isParent())? 0: thread->rcv(srcline), srcline))//, m_isparent(thread->isParent())
    {
//        DEBUG_MSG(BLUE_MSG << timestamp() << (m_isparent? "parent": "child") << " scope ctor" << ENDCOLOR_ATLINE(srcline));
        DEBUG_MSG(BLUE_MSG << timestamp() << "scope ctor# " << shmobj.count << "/" << (2 * NUM_INST) << ENDCOLOR_ATLINE(srcline));
//        if (!m_isparent) return;
        if (shmobj.count++) return; //not first (parent)
        new (&shmobj.data) TYPE(std::forward<ARGS>(args) ...); //, srcline); //call ctor to init (parent only)
        if (!shmkey && has_threads) thread->send(::shmkey(&shmobj), srcline); //send shmkey to child (parent only)
    }
#undef has_threads
#undef thread
    ~ShmScope()
    {
//        DEBUG_MSG(BLUE_MSG << timestamp() << (m_isparent? "parent": "child") << " scope dtor" << ENDCOLOR);
        DEBUG_MSG(BLUE_MSG << timestamp() << "scope dtor# " << shmobj.count << "/" << (2 * NUM_INST) << ENDCOLOR);
//        if (!m_isparent) return;
        if (shmobj.count++ < 2 * NUM_INST) return; //not last (could be parent or child)
        shmobj.data.~TYPE();
        ::shmfree(&shmobj, SRCLINE);
    }
public: //wrapped object (ref to shared obj)
    type& shmobj;
private:
//    bool m_isparent;
};
#endif


#if 0
#define SEMI_PERFECT_FWD2BASE_CTOR(type, base)  \
    template<typename ... ARGS> \
    explicit type(ARGS&& ... args, IpcThread& thr, SrcLine srcline = 0): base(std::forward<ARGS>(args) ..., srcline)

//parent-owned mixin class:
template <class TYPE>
class ParentOwned: public TYPE
{
public: //ctor/dtor
#define CAPTURE_THREAD(__VA_ARGS__, IpcThread& thr, SrcLine srcline = 0)
#define INJECT_THREAD
    PERFECT_FWD2BASE_CTOR(ParentOwned, TYPE), m_count(0) { /*addref()*/; } //std::cout << "with mutex\n"; } //, islocked(m_mutex.islocked /*false*/) {} //derivation
    ~WithRefCount() { /*delref()*/; }
public: //helper class to clean up ref count
    typedef WithRefCount<TYPE> type;
    typedef void (*_Destructor)(type*); //void* data); //TODO: make generic?
    class Scope
    {
        type& m_obj;
        _Destructor& m_clup;
    public: //ctor/dtor
        Scope(type& obj, _Destructor&& clup): m_obj(obj), m_clup(clup) { m_obj.addref(); }
        ~Scope() { if (!m_obj.delref()) m_clup(&m_obj); }
    };
//    std::shared_ptr<type> clup(/*!thread.isParent()? 0:*/ &testobj, [](type* ptr) { DEBUG_MSG("bye " << ptr->numref() << "\n"); if (ptr->dec()) return; ptr->~TestObj(); shmfree(ptr, SRCLINE); });
public:
    int& numref() { return m_count; }
    int& addref() { return ++m_count; }
    int& delref() { /*DEBUG_MSG("dec ref " << m_count - 1 << "\n")*/; return --m_count; }
};
#endif

#undef DEBUG_MSG
#endif //ndef _IPC_H