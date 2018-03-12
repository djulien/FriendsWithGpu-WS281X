//simulate std::thread using a forked process:

#ifndef _IPC_H
#define _IPC_H


#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> //fork, getpid
#include "elapsed.h" //timestamp()

//ipc msgs:
//NOTE: must be created before fork()
class IpcPipe
{
private: //data members
    int m_fd[2];
//    bool m_open[2];
public: //ctor
    IpcPipe(SrcLine srcline = 0): m_fd{-1, -1} { ATOMIC_MSG(BLUE_MSG << "IpcPipe ctor" << ENDCOLOR_ATLINE(srcline)); pipe(m_fd); } //m_open[0] = m_open[1] = true; } //create pipe descriptors < fork()
    ~IpcPipe() { pipe_close(ReadEnd); pipe_close(WriteEnd); ATOMIC_MSG(BLUE_MSG << "IpcPipe dtor" << ENDCOLOR); }
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
//ATOMIC_MSG("pipe write[" << which << "]" << ENDCOLOR);
        ssize_t wrlen = write(m_fd[which], &value, sizeof(value));
        ATOMIC_MSG(((wrlen == sizeof(value))? BLUE_MSG: RED_MSG) << "parent '" << getpid() << "' send len " << wrlen << FMT(", value 0x%lx") << value << " to child" << ENDCOLOR_ATLINE(srcline));
        if (wrlen == -1) throw std::runtime_error(strerror(errno)); //write failed
//        close(m_fd[1]);
    }
    int rcv(SrcLine srcline = 0)
    {
        int which = ReadEnd;
        int retval = -1;
//        direction(1 - which); //close write side of pipe; child is read-only
        pipe_close(1 - which);
//ATOMIC_MSG("pipe read[" << which << "]" << ENDCOLOR);
        ssize_t rdlen = read(m_fd[which], &retval, sizeof(retval)); //NOTE: blocks until data received
        ATOMIC_MSG(((rdlen == sizeof(retval))? BLUE_MSG: RED_MSG) << "child '" << getpid() << "' rcv len " << rdlen << FMT(", value 0x%lx") << retval << " from parent" << ENDCOLOR_ATLINE(srcline));
        if (rdlen == -1) throw std::runtime_error(strerror(errno)); //read failed
//        close(m_fd[0]);
        return retval;
    }
private: //helpers:
    void pipe_close(int which)
    {
//        if (!m_open[which]) return;
        if (m_fd[which] == -1) return;
//ATOMIC_MSG("pipe close[" << which << "]" << ENDCOLOR);
        close(m_fd[which]);
        m_fd[which] = -1;
    }
};


class IpcThread
{
public: //ctor/dtor
    typedef void (*_Callable)(void); //void* data); //TODO: make generic?
//    template<typename _Callable, typename... _Args>
//    explicit IpcThread(_Callable&& entpt, _Args&&... args)
    bool isParent() const { return (m_pid != 0); }
    bool isChild() const { return (m_pid == 0); }
    bool isError() const { return (m_pid == -1); }
    explicit IpcThread(SrcLine srcline = 0): IpcThread(*new IpcPipe(srcline), srcline) {}
    explicit IpcThread(IpcPipe& pipe, SrcLine srcline = 0) //: m_pipe(pipe)
    {
        m_pid = fork();
        m_pipe.reset(&pipe); //NOTE: pipe must be cre < fork
        const char* proctype = isParent()? "parent": "child";
        ATOMIC_MSG(YELLOW_MSG << timestamp() << "fork (" << proctype << "): child pid = " << (isParent()? m_pid: getpid()) << ENDCOLOR_ATLINE(srcline));
        if (isError()) throw std::runtime_error(strerror(errno)); //fork failed
    }
    explicit IpcThread(_Callable& entpt, IpcPipe& pipe /*= IpcPipe()*/, SrcLine srcline = 0): IpcThread(pipe, srcline) //, _Args&&... args)
    {
        if (!isChild()) return; //parent or error
        (*entpt)(/*args*/); //call child main()
        ATOMIC_MSG(RED_MSG << timestamp() << "child " << getpid() << " exit" << ENDCOLOR);
        exit(0); //kludge; don't want to execute remainder of caller
    }
    ~IpcThread() { if (isParent()) join(SRCLINE); } //waitpid(-1, NULL /*&status*/, /*options*/ 0); //NOTE: will block until child state changes
public: //std::thread emulation
    typedef pid_t id;
    static id get_id(void) { return getpid(); }
//    IpcPipe& pipe() { return m_pipe; }
public: //methods
    void send(int value, SrcLine srcline = 0) { m_pipe->send(value, srcline); }
    int rcv(SrcLine srcline = 0) { return m_pipe->rcv(srcline); }
    void join(SrcLine srcline = 0)
    {
        int status;
        if (!isParent() || isError()) throw std::runtime_error(/*RED_MSG*/ "join (child): no process to join" /*ENDCOLOR*/);
        ATOMIC_MSG(YELLOW_MSG << timestamp() << "join: wait for pid " << m_pid << ENDCOLOR_ATLINE(srcline));
        waitpid(m_pid, &status, /*options*/ 0); //NOTE: will block until child state changes
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
    std::shared_ptr<IpcPipe> m_pipe;
};


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
//    std::shared_ptr<type> clup(/*!thread.isParent()? 0:*/ &testobj, [](type* ptr) { ATOMIC_MSG("bye " << ptr->numref() << "\n"); if (ptr->dec()) return; ptr->~TestObj(); shmfree(ptr, SRCLINE); });
public:
    int& numref() { return m_count; }
    int& addref() { return ++m_count; }
    int& delref() { /*ATOMIC_MSG("dec ref " << m_count - 1 << "\n")*/; return --m_count; }
};

#endif //ndef _IPC_H