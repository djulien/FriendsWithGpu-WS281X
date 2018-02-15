//simulate std::thread using a forked process:

#ifndef _IPC_THREAD_H
#define _IPC_THREAD_H



#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> //fork, getpid

class IpcThread
{
public: //ctor/dtor
    typedef void (*_Callable)(void); //void* data); //TODO: make generic?
//    template<typename _Callable, typename... _Args>
//    explicit IpcThread(_Callable&& entpt, _Args&&... args)
    explicit IpcThread(_Callable&& entpt) //, _Args&&... args)
    {
        m_pid = fork();
        const char* proctype = m_pid? "parent": "child";
        ATOMIC(std::cout << YELLOW_MSG << timestamp() << "fork (" << proctype << "): child pid = " << (m_pid? m_pid: getpid()) << ENDCOLOR << std::flush);
        if (m_pid == -1) throw std::runtime_error(strerror(errno)); //fork failed
        if (m_pid) return; //parent
        (*entpt)(/*args*/); //call child main()
        ATOMIC(std::cout << RED_MSG << timestamp() << "child " << getpid() << " exit" << ENDCOLOR << std::flush);
        exit(0); //kludge; don't want to execute remainder of caller
    }
public: //std::thread emulation
    typedef pid_t id;
    static id get_id(void) { return getpid(); }
public: //methods
    void join(void)
    {
        if (!m_pid || (m_pid == -1)) throw std::runtime_error(RED_MSG "join (child): no process to join" ENDCOLOR);
        ATOMIC(std::cout << YELLOW_MSG << timestamp() << "join: wait for pid " << m_pid << ENDCOLOR << std::flush);
        int status;
        waitpid(m_pid, &status, /*options*/ 0); //NOTE: will block until child state changes
    }
private: //data
    pid_t m_pid; //child pid (valid in parent only)
};

#endif //ndef _IPC_THREAD_H