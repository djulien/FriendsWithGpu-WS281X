//simulate std::thread using a forked process:

#ifndef _IPC_H
#define _IPC_H


#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> //fork, getpid
#include "elapsed.h" //timestamp()

class IpcThread
{
public: //ctor/dtor
    typedef void (*_Callable)(void); //void* data); //TODO: make generic?
//    template<typename _Callable, typename... _Args>
//    explicit IpcThread(_Callable&& entpt, _Args&&... args)
    bool isParent() const { return (m_pid != 0); }
    bool isChild() const { return (m_pid == 0); }
    bool isError() const { return (m_pid == -1); }
    explicit IpcThread(SrcLine srcline = 0)
    {
        m_pid = fork();
        const char* proctype = isParent()? "parent": "child";
        ATOMIC_MSG(YELLOW_MSG << timestamp() << "fork (" << proctype << "): child pid = " << (isParent()? m_pid: getpid()) << ENDCOLOR_ATLINE(srcline));
        if (isError()) throw std::runtime_error(strerror(errno)); //fork failed
    }
    explicit IpcThread(_Callable&& entpt, SrcLine srcline = 0): IpcThread(srcline) //, _Args&&... args)
    {
        if (!isChild()) return; //parent or error
        (*entpt)(/*args*/); //call child main()
        ATOMIC_MSG(RED_MSG << timestamp() << "child " << getpid() << " exit" << ENDCOLOR);
        exit(0); //kludge; don't want to execute remainder of caller
    }
public: //std::thread emulation
    typedef pid_t id;
    static id get_id(void) { return getpid(); }
public: //methods
    void join(SrcLine srcline = 0)
    {
        int status;
        if (!isParent() || isError()) throw std::runtime_error(/*RED_MSG*/ "join (child): no process to join" /*ENDCOLOR*/);
        ATOMIC_MSG(YELLOW_MSG << timestamp() << "join: wait for pid " << m_pid << ENDCOLOR_ATLINE(srcline));
        waitpid(m_pid, &status, /*options*/ 0); //NOTE: will block until child state changes
    }
private: //data
    pid_t m_pid; //child pid (valid in parent only)
};


//ipc msgs:
//NOTE: must be created before fork()
class IpcPipe
{
private: //data members
    int m_fd[2];
//    bool m_open[2];
public: //ctor
    IpcPipe(): m_fd{-1, -1} { pipe(m_fd); } //m_open[0] = m_open[1] = true; } //create pipe descriptors < fork()
    ~IpcPipe() { pipe_close(Child2Parent); pipe_close(Parent2Child); }
public: //methods
    const int Parent2Child = 0, Child2Parent = 1;
//    void direction(int which) { pipe_close(1 - which); } //which one is writer
    void parent_send(int value, SrcLine srcline = 0) { pipe_send(Parent2Child, value, srcline); }
    void child_send(int value, SrcLine srcline = 0) { pipe_send(Child2Parent, value, srcline); }
    int parent_rcv(SrcLine srcline = 0) { return pipe_rcv(Child2Parent, srcline); }
    int child_rcv(SrcLine srcline = 0) { return pipe_rcv(Parent2Child, srcline); }
private: //helpers:
    void pipe_close(int which)
    {
//        if (!m_open[which]) return;
//ATOMIC_MSG("pipe close[" << which << "]" << ENDCOLOR);
        if (m_fd[which] == -1) return;
        close(m_fd[which]);
        m_fd[which] = -1;
    }
    void pipe_send(int which, int value, SrcLine srcline = 0)
    {
//        direction(which);
        pipe_close(1 - which);
//ATOMIC_MSG("pipe write[" << which << "]" << ENDCOLOR);
        write(m_fd[which], &value, sizeof(value));
        ATOMIC_MSG(BLUE_MSG << "parent '" << getpid() << FMT("' send 0x%x") << value << " to child" << ENDCOLOR_ATLINE(srcline));
//        close(m_fd[1]);
    }
    int pipe_rcv(int which, SrcLine srcline = 0)
    {
        int retval;
//        direction(1 - which); //close write side of pipe; child is read-only
        pipe_close(1 - which);
//ATOMIC_MSG("pipe read[" << which << "]" << ENDCOLOR);
        read(m_fd[which], &retval, sizeof(retval)); //NOTE: blocks until data received
        ATOMIC_MSG(BLUE_MSG << "child '" << getpid() << FMT("' rcv 0x%x") << retval << " from parent" << ENDCOLOR_ATLINE(srcline));
//        close(m_fd[0]);
        return retval;
    }
};

#endif //ndef _IPC_H