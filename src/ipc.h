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
#include <type_traits> //std::conditional<>, std::enable_if<>
#include <functional> //std::function<>

#include "srcline.h"
#ifdef IPC_DEBUG
 #include "atomic.h" //ATOMIC_MSG()
 #include "ostrfmt.h" //FMT()
 #include "elapsed.h" //timestamp()
 #include "msgcolors.h"
 #define DEBUG_MSG  ATOMIC_MSG
#else
 #define DEBUG_MSG(msg)  {} //noop
#endif


#ifndef WANT_IPC
 #define WANT_IPC  true //default true, otherwise caller probably wouldn't #include this file
#endif


//ipc msgs:
//NOTE: must be created before fork()
//can be used for in-proc threads, but not needed
class IpcPipe
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
public:
    static IpcPipe& none() { return *(IpcPipe*)0; }
};


#if 0
template <bool, typename = void>
class IpcThread //: public std::thread
{
    std::thread m_thr;
public: //ctor/dtor
    typedef void (*_Callable)(void); //void* data); //TODO: make generic?
//    template<typename _Callable, typename... _Args>
//    explicit IpcThread(_Callable&& entpt, _Args&&... args)
    explicit IpcThread(SrcLine srcline = 0): IpcThread(*new IpcPipe(srcline), srcline) {}
    explicit IpcThread(_Callable/*&*/ entpt, SrcLine srcline = 0): IpcThread(entpt, *new IpcPipe(srcline), srcline) {}


    explicit IpcPipe(SrcLine srcline = 0) { DEBUG_MSG(BLUE_MSG << timestamp() << "!IpcThread ctor" << ENDCOLOR_ATLINE(srcline)); }
    ~IpcPipe() { DEBUG_MSG(BLUE_MSG << timestamp() << "!IpcThread dtor" << ENDCOLOR); }
public: //methods
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
template <bool IPC = true> //default true, otherwise caller probably wouldn't have #included this file
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
#endif


#if 0
//conditional inheritance base:
template <typename = void>
class IpcThread_data
{
//    SrcLine m_srcline;
//    std::vector<IpcThread*> IpcThread::all;
};

//in-proc (non-ipc) specialization:
template <bool IPC>
class IpcThread_data<std::enable_if_t<!IPC>>
{
//    std::thread m_thr;
public:
//in-proc specialization:
    static std::enable_if<!IPC, auto> thrid() { return std::this_thread::get_id(); }
};


//ipc specialization:
template <bool IPC>
class IpcThread_data<std::enable_if_t<IPC>>
{
//    std::thread m_thr;
//    pid_t m_thr;
public:
//ipc specialization:
    static std::enable_if<IPC, auto> thrid() { return getpid(); }
};
#endif


#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ [&](auto& _)
#endif

template <bool IPC = true> //default true, otherwise caller probably wouldn't #include this file
class IpcThread //: public Thread_data<IPC>
{
//    typedef decltype(thrid()) mypidtype;
    typedef typename std::conditional<IPC, pid_t, std::thread*>::type PIDTYPE;
    std::string child_pid() //kludge: wrap to avoid "operands to ? have different types" error
    {
        std::stringstream ss;
        if (isParent()) ss << m_pid;
        else ss << getpid();
        return ss.str();
    }
public: //ctor/dtor
    typedef /*static*/ void (*Callback)(void); //void* data); //TODO: add generic params?
//    typedef std::function<void()> Callback;
//    template<typename _Callable, typename... _Args>
//    explicit IpcThread(_Callable&& entpt, _Args&&... args)
//positional params:
//    explicit IpcThread(SrcLine srcline = 0): IpcThread(IpcPipe::none() /*new IpcPipe(srcline)*/, srcline) {}
//    explicit IpcThread(Callback/*&*/ entpt): IpcThread(entpt, 0) {}
    explicit IpcThread(Callback/*&*/ entpt, SrcLine srcline = 0): IpcThread(entpt, IpcPipe::none() /*new IpcPipe(srcline)*/, srcline) {}
    explicit IpcThread(IpcPipe& pipe, SrcLine srcline = 0): IpcThread(0, pipe, srcline) {} //m_anychild(false) //: m_pipe(pipe)
//NOTE: all other ctors come thru here
    explicit IpcThread(Callback/*&*/ entpt, IpcPipe& pipe /*= IpcPipe()*/, SrcLine srcline = 0): m_anychild(false) //IpcThread(pipe, srcline) //, _Args&&... args)
    {
        all().push_back(this); //keep track of instances; let caller use global collection
        m_pid = fork(entpt);
        m_pipe.reset(&pipe); //NOTE: must cre pipe < fork
//        std::cout << "thread here: is parent? " << !!m_pid << ", me " << getpid() << "\n" << std::flush;
//        const char* proctype = isChild()? "child": isError()? "error": "parent"; //isParent()? "parent": "child";
//#pragma message("there's a recursion problem here?")
        DEBUG_MSG(YELLOW_MSG << timestamp() << "fork (" << proctype() << "): child pid = " << child_pid() << ENDCOLOR_ATLINE(srcline));
//        std::cout << "thread here2: is parent? " << !!m_pid << ", me " << getpid() << "\n" << std::flush;
        if (isError()) throw std::runtime_error(strerror(errno)); //fork failed
    }
    ~IpcThread()
    {
        if (isParent()) join(SRCLINE); //waitpid(-1, NULL /*&status*/, /*options*/ 0); //NOTE: will block until child state changes
        auto inx = find(all().begin(), all().end(), this);
        if (inx != all().end()) all().erase(inx);
    }
//named param variants:
#if 0
    struct CtorParams
    {
        Callback entpt = 0;
        IpcPipe& pipe = IpcPipe::none();
        SrcLine srcline = 0;
    };
//    explicit IpcThread(SrcLine mySrcLine = 0, void (*get_params)(CtorParams& params) = 0) //void (*get_params)(CtorParams&) = 0)
    explicit IpcThread(/*SrcLine srcline = 0*/): IpcThread(/*srcline,*/ NAMED{ SRCLINE; }) {} //= 0) //void (*get_params)(struct FuncParams&) = 0)
    template <typename CALLBACK>
    explicit IpcThread(/*SrcLine mySrcLine = 0,*/ CALLBACK&& get_params /*= 0*/) //void (*get_params)(API&) = 0) //int& i, std::string& s, bool& b, SrcLine& srcline) = 0) //: i(0), b(false), srcline(0), o(nullptr)
    {
        /*static*/ struct CtorParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params != 0) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
        auto thunk = [](auto arg, struct CtorParams& params){ /*(static_cast<decltype(callback)>(arg))*/ arg(params); }; //NOTE: must be captureless, so wrap it
        thunk(get_params, params);
        new (this) IpcThread(params.entpt, params.pipe, params.srcline); //convert from named params to positional params
    }
//    struct Unpacked {}; //ctor disambiguation tag
//    explicit IpcThread(const CtorParams& params/*, Unpacked*/): IpcThread(params.entpt, params.pipe, params.srcline) {}
#endif
private: //data
    /*std::conditional<IPC, pid_t, std::thread*>*/ PIDTYPE m_pid; //child pid (valid in parent only)
    bool m_anychild;
    std::shared_ptr<IpcPipe> m_pipe;
//broken    static /*auto*/ /*decltype(m_pid)*/ PIDTYPE topid(intptr_t val) { return reinterpret_cast</*decltype(m_pid)*/ PIDTYPE>(val); }
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    static typename std::enable_if<!IPC_copy, /*pid_t*/ PIDTYPE>::type topid(intptr_t val) { return reinterpret_cast</*decltype(m_pid)*/ PIDTYPE>(val); } //in-proc (non-ipc) specialization
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    static typename std::enable_if<IPC_copy, /*pid_t*/ PIDTYPE>::type topid(intptr_t val) { return val; } //ipc (multi-proc) specialization
public: //methods
    bool isParent() const { return !isChild() && !isError(); } //(m_pid != 0); }
    bool isChild() const { return (m_pid == topid(0)); }
    bool isError() const { return (m_pid == topid(-1)); }
    const char* proctype() const { return isChild()? "child": isError()? "error": "parent"; } //isParent()? "parent": "child";
//    key_t ParentKeyGen(SrcLine srcline = 0) { return isParent()? 0: rcv(srcline); } //generate parent key
    void send(int value, SrcLine srcline = 0) { m_pipe->send(value, srcline); }
    int rcv(SrcLine srcline = 0) { return m_pipe->rcv(srcline); }
    void allow_any() { m_anychild = true; } //m_pid = -1; } //allow any child to wake up join()
    void join(SrcLine srcline = 0)
    {
        if (!m_pid) throw std::runtime_error("no thread to join");
        int status;
        /*auto*/ PIDTYPE waitfor = m_anychild? topid(-1): m_pid;
        if (!isParent()/* || isError()*/) throw std::runtime_error(/*RED_MSG*/ "join (!parent): no process to join" /*ENDCOLOR*/);
        DEBUG_MSG(YELLOW_MSG << timestamp() << "join: wait for pid " << waitfor << ENDCOLOR_ATLINE(srcline));
        waitpid(waitfor, &status, /*options*/ 0); //NOTE: will block until child state changes
    }
private: //emulate ipc functions
//    explicit IpcThread(std::enable_if<!IPC, Callback/*&*/> entpt, IpcPipe& pipe /*= IpcPipe()*/, SrcLine srcline = 0)
//    static void myexit() { exit(0); }
    static void noop() {}
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    typename std::enable_if<!IPC_copy, /*pid_t*/ PIDTYPE>::type fork(Callback entpt = 0) //in-proc (non-ipc) specialization
    {
        if (!entpt) throw std::runtime_error("!ipc fork: thread function missing");
        DEBUG_MSG(BLUE_MSG << "mt fork, entpt? " << !!entpt << ENDCOLOR);
        m_pid = new std::thread(entpt? entpt: noop); //myexit); //run_child);
        return m_pid; //parent
    }
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    typename std::enable_if<IPC_copy, /*pid_t*/ PIDTYPE>::type fork(Callback entpt = 0) //ipc (multi-proc) specialization
    {
        DEBUG_MSG(BLUE_MSG << "ipc fork, entpt? " << !!entpt << ENDCOLOR);
        pid_t retval = ::fork();
        if (entpt && (retval == 0)) //child
        {
//        if (!isChild()) return; //parent or error
            DEBUG_MSG(GREEN_MSG << timestamp() << "child " << getpid() << " calling entpt" << ENDCOLOR);
//        (*entpt)(/*args*/); //call child main()
            (*entpt)();
            DEBUG_MSG(RED_MSG << timestamp() << "child " << getpid() << " exit" << ENDCOLOR);
            exit(0); //kludge; don't want to execute remainder of caller
        }
        return retval; //m_pid; //parent
    }
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    typename std::enable_if<!IPC_copy, /*decltype(m_pid)*/ PIDTYPE>::type waitpid(/*decltype(m_pid)*/ PIDTYPE pid, int* stat_loc, int options) //in-proc (non-ipc) specialization
    {
        if (pid == topid(-1)) throw std::runtime_error("!ipc waitpid: wait for any child not implemented");
        m_pid->join();
        PIDTYPE retval = m_pid;
        m_pid = 0;
        return retval; //m_pid;
    }
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    typename std::enable_if<IPC_copy, PIDTYPE>::type waitpid(PIDTYPE pid, int* stat_loc, int options) { return ::waitpid(pid, stat_loc, options); } //ipc (multi-proc) specialization
public: //shared data
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    static typename std::enable_if<IPC_copy, pid_t>::type thrid() { return getpid(); } //ipc (multi-proc) specialization
    template <bool IPC_copy = IPC> //kludge: avoid "dependent scope" error
    static typename std::enable_if<!IPC_copy, std::thread::id>::type thrid() { return std::this_thread::get_id(); } //in-proc (non-ipc) specialization
//    static std::vector<IpcThread*> all();
    static /*std::vector<IpcThread*>&*/ auto& all()
    {
        static std::vector<IpcThread*> m_thrids;
        return m_thrids;
    }
};
//std::vector<IpcThread*> IpcThread::all;


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


///////////////////////////////////////////////////////////////////////////////
////
/// Unit tests:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion
#include <iostream> //std::cout, std::flush
#include <chrono> //std::chrono
#include <thread>
#include "ostrfmt.h" //FMT()
#include "msgcolors.h"
#include "elapsed.h" //timestamp()

#ifndef MSG
 #define MSG(msg)  { std::cout << timestamp() << msg << std::flush; }
#endif

typedef void (*cb)(void);


void sleep_msec(int msec)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}
void child_1sec() { MSG("child start 1 sec\n"); sleep_msec(1000); MSG("child end 1 sec\n"); } // simulate expensive operation
void sleep_1sec() { sleep_msec(1000); } // simulate expensive operation


//from http://en.cppreference.com/w/cpp/thread/thread/join
void test_native_thread()
{
    MSG(PINK_MSG << "test native thread" << ENDCOLOR);
    MSG("starting first helper...\n");
    std::thread helper1(child_1sec);
    MSG("starting second helper...\n");
    std::thread helper2(child_1sec);
    MSG("waiting for helpers to finish...\n");
    helper1.join();
    helper2.join();
    MSG("done!\n");
}


//single process, non-/inline execution:
void test_single(cb cb = 0)
{
    MSG(PINK_MSG << "test single " << (cb? "non": "in-") << "line" << ENDCOLOR);
    IpcThread<false> thr1(cb);
    const char* color = thr1.isParent()? PINK_MSG: BLUE_MSG;
    if (thr1.isChild()) sleep_1sec(); //give parent head start

    MSG(color << timestamp() << thr1.proctype() << " start" << ENDCOLOR);
    sleep_msec(500);
    MSG(color << timestamp() << thr1.proctype() << " finish" << ENDCOLOR);
    thr1.join();
    MSG(CYAN_MSG << timestamp() << "quit" << ENDCOLOR);
}


//multi-process, non-/inline execution:
void test_multi(cb cb = 0)
{
    MSG(PINK_MSG << "test multi " << (cb? "non": "in-") << "line" << ENDCOLOR);
//    IpcThread<true> thread(cb);
    IpcThread<true> thread(NAMED{ _.entpt = cb; SRCLINE; });
    const char* color = thread.isParent()? PINK_MSG: BLUE_MSG;
    if (thread.isChild()) sleep_1sec(); //give parent head start

    MSG(color << timestamp() << thread.proctype() << " start" << ENDCOLOR);
    sleep_msec(500);
    MSG(color << timestamp() << thread.proctype() << " finish" << ENDCOLOR);
    if (thread.isParent()) thread.join();
    else exit(0);
    MSG(CYAN_MSG << timestamp() << "quit " << thread.proctype() << ENDCOLOR);
}


void thread_proc() { MSG(BLUE_MSG << timestamp() << "thread proc" << ENDCOLOR); }

//int main(int argc, const char* argv[])
void unit_test()
{
//    lambda_test();
    test_native_thread();
//    test_single(); //not supported
    test_single(thread_proc);
    test_multi();
    test_multi(thread_proc);

//    return 0;
}
#endif //def WANT_UNIT_TEST
//eof