//simple msg que
//uses mutex (shared) and cond var
//for ipc, needs to be in shared memory; for threads, just needs to be in heap
//this is designed for synchronous usage; msg que only holds 1 int (can be used as bitmap)
//template<int MAXLEN>
//no-base class to share mutex across all template instances:
//template<int MAXDEPTH>

#define MSGQUE_DEBUG


#ifndef _MSGQUE_H
#define _MSGQUE_H

#include <mutex>
#include <condition_variable>
#include <unistd.h> //fork, getpid()
#include <thread> //std::this_thread
//for example see http://en.cppreference.com/w/cpp/thread/condition_variable
#include <type_traits> //std::conditional<>, std::decay<>, std::remove_reference<>, std::remove_pointer<>
#include <tr2/type_traits> //std::tr2::direct_bases<>, std::tr2::bases<>
#include <chrono> //std::chrono

#include "shmalloc.h"
#include "srcline.h"
#include "vectorex.h"
#include "elapsed.h" //timestamp()
#include "eatch.h"

#ifdef MSGQUE_DEBUG
 #include "atomic.h"
 #include "msgcolors.h"
 #define DEBUG_MSG  ATOMIC_MSG
 #define IF_DEBUG(stmt)  stmt //{ stmt; }
#else
 #define DEBUG_MSG(msg)  {} //noop
 #define IF_DEBUG(stmt)  //noop
#endif
#ifdef MSG_DETAILS
 #define DETAILS_MSG  DEBUG_MSG
#else
 #define DETAILS_MSG(stmt)  {} //noop
#endif

#ifndef ABS
 #define ABS(n)  (((n) < 0)? -(n): (n))
#endif

#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ [&](auto& _)
#endif

#ifndef WANT_IPC
 #define WANT_IPC  false //default no ipc
#endif

#if WANT_IPC
 #define IF_IPC(stmt)  stmt
#else
 #define IF_IPC(stmt)  //noop
#endif

#ifndef VOLATILE
 #define VOLATILE  //dummy keyword; not needed
#endif


//copy null-terminated string or as space allows:
char* strzcpy(char* dest, const char* src, size_t maxlen)
{
    if (maxlen)
    {
        size_t cpylen = src? std::min(strlen(src), maxlen - 1): 0;
        if (cpylen) memcpy(dest, src, cpylen);
        dest[cpylen] = '\0';
    }
    return dest;
}


//polling needed for single-threaded caller:
//static void sleep_msec(int msec)
//{
//    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
//}


//template<class BASE, class DERIVED>
//BASE base_of(DERIVED BASE::*);


//partial specialization selectors:
#define IsMultiThreaded(num_threads)  ((num_threads) != 0)
#define IsMultiProc(num_threads)  ((num_threads) < 0)


//underlying queue implementation:
//NOTE: caller is responsible for locking if multi-threaded
//see https://stackoverflow.com/questions/11019232/how-can-i-specialize-a-c-template-for-a-range-of-integer-values/11019369
//#define SELECTOR(VAL)  (((VAL) < 0)? -1: ((VAL) > 0)? 1: 0)
//template <int VALUE, int WHICH = SELECTOR(VALUE)>
template <int MAX_THREADs = 0, bool ISMT = IsMultiThreaded(MAX_THREADs), bool ISIPC = IsMultiProc(MAX_THREADs)>
class QueData //: public empty_mixin<QueData> //: public MsgQue_data<MAX_THREADs> //std::conditional<THREADs != 0, MsgQue_multi, MsgQue_base>::type
{
protected: //members
    IF_DEBUG(char m_name[20]); //store name directly in object; can't use char* because mapped address could vary between procs
    /*volatile*/ int m_msg;
    SrcLine m_srcline;
public: //ctor/dtor
//    struct Unpacked {}; //ctor disambiguation tag
    struct CtorParams //: public CtorParams
    {
        IF_DEBUG(const char* name = 0);
        int msg = 0;
        SrcLine srcline = 0;
    };
    explicit QueData(const CtorParams& params): m_srcline(params.srcline), m_msg(params.msg) { IF_DEBUG(strzcpy(m_name, params.name, sizeof(m_name))); }
    ~QueData()
    {
        if (m_msg) DETAILS_MSG(RED_MSG << timestamp() << "QueData-" << m_name << ".dtor: !empty @exit " << FMT("0x%x") << m_msg << ENDCOLOR_ATLINE(m_srcline)) //benign, but might be caller bug so complain
        else DEBUG_MSG(GREEN_MSG << timestamp() << "QueData-" << m_name << ".dtor: empty @exit" << ENDCOLOR_ATLINE(m_srcline));
    }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const QueData& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << "MsgQue<" << MAX_THREADs << ">{" << eatch(2) << IF_DEBUG(", name '" << me.m_name << "'" <<) ", msg " << FMT("0x%x") << me.m_msg << ", srcline " << me.m_srcline << "}";
        ostrm << IF_DEBUG(", name '" << me.m_name << "'" <<) ", msg " << FMT("0x%x") << me.m_msg << ", srcline " << me.m_srcline;
        return ostrm;
    }
    inline QueData* operator->() { return this; } //placeholder for smart pointer in derived class
public: //queue manip methods
    QueData* clear()
    {
//        scoped_lock lock;
//        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
        m_msg = 0;
        return this; //fluent
    }
//    MsgQue& send(SrcLine mySrcLine = 0, void (*get_params)(struct SendParams&) = 0)
    QueData* send(int msg, /*bool broadcast = false,*/ SrcLine srcline = 0)
    {
//        /*static*/ struct SendParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//        std::stringstream ssout;
//        scoped_lock lock;
//        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_msg & msg) throw "QueData.send: msg already queued"; // + tostr(msg) + " already queued");
        m_msg |= msg; //use bitmask for multiple msgs
        if (!(m_msg & msg)) throw "QueData.send: msg enqueue failed"; // + tostr(msg) + " failed");
        /*if (MSGQUE_DETAILS)*/ DETAILS_MSG(BLUE_MSG << timestamp() << "QueData-" << m_name << ".send " << FMT("0x%x") << msg /*<< " to " << (broadcast? "all": "one")*/ << ", now qued " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR_ATLINE(srcline));
//        if (!broadcast) m_ptr->condvar.notify_one(); //wake main thread
//        else m_ptr->condvar.notify_all(); //wake *all* wker threads
        return this; //fluent
    }
    struct SendParams
    {
        int msg = 0; //initial queue contents
//        bool broadcast = false; //send to all listeners when new msg arrives
        SrcLine srcline = 0; //SRCLINE; //debug call stack
     }; //FuncParams() { s = "none"; i = 999; b = true; }}; // FuncParams;
//    MsgQue* send(int msg, /*bool broadcast = false,*/ SrcLine srcline = 0)
    template <typename CALLBACK>
    QueData* send(CALLBACK&& named_params)
    {
        /*static*/ struct SendParams params; //reinit each time; comment out for sticky defaults
//        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
        auto thunk = [](auto get_params, struct SendParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(named_params, params);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
        return send(params.msg, /*params.broadcast,*/ params.srcline);
    }
//rcv filters:
//    typedef bool (*cbFilter)(int);
    bool wanted(int val) { /*if (MSGQUE_DETAILS)*/ DETAILS_MSG(BLUE_MSG << timestamp() << FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { /*if (MSGQUE_DETAILS)*/ DETAILS_MSG(BLUE_MSG << timestamp() << FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
//    struct RcvParams { bool (MsgQue::*filter)(int val) = 0, int operand = 0, bool remove = false, SrcLine srcline = SRCLINE; };
//    template <typename CALLBACK>
    int rcv(bool (QueData::*filter)(int val), int operand = 0, /*bool (*cb)(int)*/ decltype(wanted) cb = 0, bool remove = false, SrcLine srcline = 0)
//    int rcv(SrcLine mySrcLine = 0, void (*get_params)(struct RcvParams&) = 0)
    {
//        /*static*/ struct RcvParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
//        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        /*if (MSGQUE_DETAILS)*/ DETAILS_MSG(BLUE_MSG << timestamp() << "QueData-" << m_ptr->name << ".rcv: op " << FMT("0x%x") << operand << ", msg " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR_ATLINE(srcline));
        while (!(this->*filter)(operand)) cb? cb(): yield(); //m_ptr->condvar.wait(scoped_lock); //ignore spurious wakeups
        int retval = m_msg;
        if (remove) m_msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
    struct RcvParams
    {
        bool (QueData::*filter)(int val); //check for matching queue entry
        int operand = 0; //filter operand
//        bool (*cb)(int) = 0; //additional logic when queue is busy
        decltype(wanted) cb = 0; //additional logic when queue is busy
        bool remove = false; //remove queue entry when received
        SrcLine srcline = 0; //debug call stack
    };
//    MsgQue* send(int msg, /*bool broadcast = false,*/ SrcLine srcline = 0)
    template <typename CALLBACK>
    int rcv(CALLBACK&& named_params)
    {
        /*static*/ struct RcvParams params; //reinit each time; comment out for sticky defaults
//        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
        auto thunk = [](auto get_params, struct SendParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(named_params, params);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
        return rcv(params.filter, params.operand, params.cb, params.remove, params.srcline);
    }
public: //utility methods
    static inline auto thrid() { return getpid(); }
    inline int thinx(decltype(thrid()) id = thrid()) { return 0; }
private:
    static void yield() { sleep_msec(1); }
//polling needed for single-threaded caller:
    static void sleep_msec(int msec) { std::this_thread::sleep_for(std::chrono::milliseconds(msec)); }
};


//multi-threading specialization:
//need mutex + cond var for thread serialization, shmem key for ipc
//use operator-> to auto-lock during methods
//class MultiThreaded: public empty_mixin<MultiThreaded>
template <int MAX_THREADs>
class QueData<MAX_THREADs, true, false>: public QueData<MAX_THREADs, false, false> //SELECTOR(1)>
{
    typedef QueData<MAX_THREADs, false, false> base_t; //SELECTOR(1)>
//no worky    typedef decltype(thrid()) THRID;
//    typedef std::thread::id THRID;
public: //helpers
//    void yield() { m_condvar.wait(scoped_lock); //ignore spurious wakeups
//ipc specialization:
//    static std::enable_if<WANT_IPC, auto> thrid() { return getpid(); }
//in-proc specialization:
    static inline std::enable_if<!WANT_IPC, std::thread::id /*auto*/> thrid() { return std::this_thread::get_id(); }
    typedef decltype(thrid()) THRID;
//private:
protected: //members
    VOLATILE MutexWithFlag /*std::mutex*/ m_mutex;
    std::condition_variable m_condvar;
    PreallocVector<THRID /*, MAX_THREADS*/> m_ids; //list of registered thread ids; no-NOTE: must be last data member
    THRID list_space[ABS(MAX_THREADs)];
public: //ctor/dtor
//    struct Unpacked {}; //ctor disambiguation tag
    struct CtorParams: public base_t::CtorParams {}; //{ int ivar = -1; SrcLine srcline = "here:0"; };
    explicit QueData(const CtorParams& params): base_t(/*(base_t::CtorParams)*/params), m_ids(list_space, ABS(MAX_THREADs)) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const QueData& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", ids(" << me.m_ids.size() << "): " << me.m_ids.join(",") << static_cast<base_t>(me);
        return ostrm;
    }
private:
#if 1
//helper class to ensure unlock() occurs after member function returns
//TODO: derive from unique_ptr<>
    class unlock_after
    {
        QueData* m_ptr;
    public: //ctor/dtor to wrap lock/unlock
        unlock_after(QueData* ptr): m_ptr(ptr) { DEBUG_MSG("lock now"); /*if (AUTO_LOCK)*/ m_ptr->m_mutex.lock(); }
        ~unlock_after() { DEBUG_MSG("unlock later"); /*if (AUTO_LOCK)*/ m_ptr->m_mutex.unlock(); }
    public:
        inline QueData* operator->() { return m_ptr; } //allow access to wrapped members
//        inline operator TYPE() { return *m_ptr; } //allow access to wrapped members
//        inline TYPE& base() { return *m_ptr; }
//        inline TYPE& operator()() { return *m_ptr; }
    };
//pointer operator; allows safe multi-process access to shared object's member functions
    inline unlock_after operator->() { return unlock_after(this); } //, [](this_type* ptr) { ptr->m_mutex.unlock(); }); } //deleter example at: http://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
#endif
public: //methods
    template <typename CALLBACK>
    int rcv(bool (QueData::*filter)(int val), int operand = 0, CALLBACK&& cb = 0, bool remove = false, SrcLine srcline = 0)
    {
        auto waitfor = cb; //[cb, m_condvar&]() { m_condvar.wait(scoped_lock); if (cb) cb(); }; //ignore spurious wakeups
        return base_t::waitfor(filter, operand, waitfor, remove, srcline);
    }
//convert system-defined id into sequentially assigned index:
    int thinx(THRID id = thrid())
    {
//        std::lock_guard<std::mutex> lock(m);
//        std::unique_lock<decltype(m_mutex)> scoped_lock(m_mutex);
//NOTE: use op->() for shm safety with ipc
        int ofs = m_ids.find(id);
        if (ofs != -1) throw std::runtime_error(RED_MSG "thrid: duplicate thread id" ENDCOLOR_NOLINE);
        if (ofs == -1) { ofs = m_ids.size(); m_ids.push_back(id); } //ofs = ids.push_and_find(id);
//    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    ss << ofs;
//    return ss.str();
        ATOMIC_MSG(CYAN_MSG << timestamp() << "pid '" << getpid() << FMT("', thread id 0x%lx") << id << " => thr inx " << ofs << ENDCOLOR);
        return ofs;
    }
};

///*static*/ friend std::ostream& operator<<(std::ostream& ostrm, std::thread::id val) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//{ 
//    ostrm << val;
//    return ostrm;
//}


//multi-process specialization:
template <int MAX_THREADs>
class QueData<MAX_THREADs, true, true>: public QueData<MAX_THREADs, true, false> //SELECTOR(1)>
{
    typedef QueData<MAX_THREADs, true, false> base_t; //SELECTOR(1)>
protected: //members
    int m_shmkey; //shmem handling info
    bool m_want_reinit;
public: //ctor/dtor
//    struct Unpacked {}; //ctor disambiguation tag
    struct CtorParams: public base_t::CtorParams
    {
        int shmkey = 0; //shmem handling info
        bool want_reinit = false; //true;
    };
    explicit QueData(const CtorParams& params): base_t(/*(base_t::CtorParams)*/params), m_shmkey(params.shmkey), m_want_reinit(params.want_reinit) {}
public:
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const QueData& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", shmkey " << me.m_shmkey << ", want_reinit " << me.m_want_reinit << (base_t)me;
        return ostrm;
    }
};


#if 0
//conditional inheritance plumbing:
//from https://stackoverflow.com/questions/8709340/can-the-type-of-a-base-class-be-obtained-from-a-template-type-automatically
//NOTE: compiler supposedly optimizes out empty base classes
template <typename> //kludge: allow multiple definitions
struct empty_mixin
{
public: //ctor/dtor
    struct CtorParams {};
//    explicit empty_base() {}
//    empty_base() = default;
//    template <class... Args>
//    constexpr empty_base(Args&&...) {}
//    template <typename ... ARGS> //perfect forward
//    explicit empty_base(ARGS&& ... args) {} //: base(std::forward<ARGS>(args) ...)
    explicit empty_mixin(const CtorParams& params) {}
public: //operators
//    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const empty_mixin& mej) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//    { 
////        ostrm << ", empty";
//        return ostrm;
//    }
};
template <bool expr, typename TYPE>
using inherit_if = std::conditional_t<expr, TYPE, empty_mixin<TYPE>>;


//template <int MAX_THREADs = 0>
class QueData: public empty_mixin<QueData> //: public MsgQue_data<MAX_THREADs> //std::conditional<THREADs != 0, MsgQue_multi, MsgQue_base>::type
{
protected: //members
    IF_DEBUG(char m_name[20]); //store name directly in object; can't use char* because mapped address could vary between procs
    SrcLine m_srcline;
public: //ctor/dtor
    struct CtorParams //: public CtorParams
    {
        IF_DEBUG(const char* name = 0);
        SrcLine srcline = 0;
    };
    explicit MsgQue(const CtorParams& params): m_srcline(params.srcline) { IF_DEBUG(strzcpy(m_name, params.name, sizeof(m_name))); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MultiProc& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << "MsgQue{" << eatch(2) << IF_DEBUG(", name '" << me.m_name << "'" <<) ", srcline " << me.m_srcline << "}";
        return ostrm;
    }
};


//multi-threading:
//need mutex + cond var for serialization, shmem key for ipc
template <int MAX_THREADs = 0>
class MultiThreaded: public empty_mixin<MultiThreaded>
{
    typedef decltype(thrid()) THRID;
protected: //members
    VOLATILE std::mutex m_mutex;
    std::condition_variable m_condvar;
    PreallocVector<THRID /*, MAX_THREADS*/> m_ids; //list of registered thread ids; no-NOTE: must be last data member
    THRID list_space[ABS(MAX_THREADs)];
public: //ctor/dtor
//    struct CtorParams { int ivar = -1; SrcLine srcline = "here:0"; };
    explicit MultiThreaded(const CtorParams& params): m_ids(list_space, ABS(MAX_THREADs)) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MultiThreaded& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", ids(" << me.m_ids.count << "): " << me.m_ids.join(","); //<< static_cast<base_t>(me) << "}";
        return ostrm;
    }
public: //methods
//convert system-defined id into sequentially assigned index:
    int thinx(THRID id = thrid())
    {
//        std::lock_guard<std::mutex> lock(m);
        std::unique_lock<decltype(m_mutex)> scoped_lock(m_mutex);
//NOTE: use op->() for shm safety with ipc
        int ofs = m_ids.find(id);
        if (ofs != -1) throw std::runtime_error(RED_MSG "thrid: duplicate thread id" ENDCOLOR_NOLINE);
        if (ofs == -1) { ofs = m_ids.size(); m_ids.push_back(id); } //ofs = ids.push_and_find(id);
//    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    ss << ofs;
//    return ss.str();
        ATOMIC_MSG(CYAN_MSG << timestamp() << "pid '" << getpid() << FMT("', thread id 0x%lx") << id << " => thr inx " << ofs << ENDCOLOR);
        return ofs;
    }
public: //helpers
//ipc specialization:
    static std::enable_if<WANT_IPC, auto> thrid() { return getpid(); }
//in-proc specialization:
    static std::enable_if<!WANT_IPC, auto> thrid() { return std::this_thread::get_id(); }
};


//ipc specialization:
//template <int MAX_THREADs = 0>
class MultiProc: public empty_mixin<MultiProc>
{
protected: //members
    int m_shmkey; //shmem handling info
    bool m_want_reinit;
public: //ctor/dtor
    struct CtorParams //: public CtorParams
    {
        int shmkey = 0; //shmem handling info
        bool want_reinit = false; //true;
    };
    explicit MultiProc(const CtorParams& params): m_shmkey(params.shmkey), m_want_reinit(params.want_reinit) {}
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MultiProc& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
        ostrm << ", shmkey " << me.m_shmkey << ", want_reinit " << me.m_want_reinit;
        return ostrm;
    }
};
#endif


//MsgQue ~= shm_ptr<>, inner data ptr, op new + del

template <int MAX_THREADs = 0>
class MsgQue //: public MemPtr<QueData<MAX_THREADs>, WANT_IPC>
//    public QueData, //unconditional base
//    public inherit_if<MAX_THREADs != 0, MultiThreaded<MAX_THREADs>>,
//    public inherit_if<MAX_THREADs < 0, MultiProc>
{
    typedef QueData<MAX_THREADs> ptr_type;
//    using B0 = Base0;
//    using B1 = inherit_if<MAX_THREADs != 0, Base1>;
//    using B2 = inherit_if<MAX_THREADs < 0, Base2>;
//    struct data:
//        public QueData,
//        public inherit_if<MAX_THREADs != 0, MultiThreaded<MAX_THREADs>>,
//        public inherit_if<MAX_THREADs < 0, MultiProc>
//    MemPtr<MsgQue_data<MAX_THREADs>> m_ptr;
//    struct QueData<MAX_THREADs>* m_ptr;
//    MemPtr<QueData<MAX_THREADs>, WANT_IPC> m_ptr;
//    /*volatile*/ int m_msg;
//    typedef typename std::conditional<IPC, pid_t, std::thread*>::type PIDTYPE;
    MemPtr<ptr_type, WANT_IPC> m_ptr;
public: //ctor/dtor
//    MyClass() = delete; //don't generate default ctor
//    struct CtorParams: public B0::CtorParams, public B1::CtorParams, public B2::CtorParams {};
//    MyClass() = default;
    explicit MsgQue(): MsgQue(NAMED{ /*SRCLINE*/; }) { DEBUG_MSG("ctor1"); }
    template <typename CALLBACK> //accept any arg type (only want caller's lambda function)
    explicit MsgQue(CALLBACK&& named_params): MsgQue(unpack(named_params), Unpacked{}) { DEBUG_MSG("ctor2"); }
public: //operators
    /*static*/ friend std::ostream& operator<<(std::ostream& ostrm, const MsgQue& me) //https://stackoverflow.com/questions/2981836/how-can-i-use-cout-myclass?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
    { 
//        ostrm << "MyClass{" << eatch(2) << static_cast<B0>(me) << static_cast<B1>(me) << static_cast<B2>(me) << "}";
        ostrm << FMT("MsgQue{@%p ") << &*me.m_ptr << eatch(2) << *me.m_ptr << "}";
        return ostrm;
    }
    ptr_type* operator->() { return m_ptr; }
private: //helpers
    struct Unpacked {}; //ctor disambiguation tag
    struct CtorParams: public ptr_type::CtorParams {}; //decltype(*this) {};
//    explicit MyClass(const CtorParams& params, Unpacked): B0(static_cast<typename B0::CtorParams>(params)), B1(static_cast<typename B1::CtorParams>(params)), B2(static_cast<typename B2::CtorParams>(params))
//    explicit MyClass(const CtorParams& params, Unpacked): B0((B0::CtorParams)params), B1((B1::CtorParams)params), B2((B2::CtorParams)params)
    explicit MsgQue(const CtorParams& params, Unpacked): m_ptr(NAMED{ _.shmkey = params.shmkey; _.persist = params.persist; _.srcline = params.srcline; })
    {
        if (!m_ptr.existed()) new (m_ptr) /*decltype(*m_ptr)*/ ptr_type(params); //placement new; init after shm alloc but before usage; don't init if already there; 
        DEBUG_MSG("ctor unpacked: " << FMT("%p") << &*m_ptr << " " << *m_ptr);
    }
    template <typename CALLBACK>
    static struct CtorParams& unpack(CALLBACK&& named_params)
    {
        static struct CtorParams params; //need "static" to preserve address after return
//        struct CtorParams params; //reinit each time; comment out for sticky defaults
        new (&params) struct CtorParams; //placement new: reinit each time; comment out for sticky defaults
//        MSG("ctor params: var1 " << params.var1 << ", src line " << params.srcline);
        auto thunk = [](auto get_params, struct CtorParams& params){ get_params(params); }; //NOTE: must be captureless, so wrap it
        thunk(named_params, params);
//        ret_params = params;
//        MSG("ret ctor params: var1 " << ret_params.var1 << ", src line " << ret_params.srcline);
        return params;
    }
};
#if 0 //happens too early (before ctor), so can't pass in ctor params
public: //mem mgmt
    static void* operator new(size_t size, int shmkey = 0, SrcLine srcline = 0)
    {
        void* ptr = memalloc<MAX_THREADs < 0>(size + ABS(MAX_THREADs) * sizeof(decltype(thrid())), shmkey, srcline);
        return ptr;
    }
    static void operator delete(void* ptr)
    {
        /*if (ptr)*/ memfree<MAX_THREADs < 0>(ptr);
    }
public: //ctors/dtors
//    explicit MsgQue(const char* name = 0, VOLATILE std::mutex& mutex = /*std::mutex()*/ shared_mutex): m_msg(0), m_mutex(mutex)
//    explicit MsgQue(const char* name = 0): m_msg(0)
    explicit MsgQue(SrcLine mySrcLine = 0, void (*get_params)(MsgQue&) = 0) //: m_msg(0) //int& i, std::string& s, bool& b, SrcLine& srcline) = 0) //: i(0), b(false), srcline(0), o(nullptr)
    {
        /*static*/ struct CtorParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
        if (mySrcLine) /*params.*/srcline = mySrcLine;
        if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//        /*if (MSGQUE_DETAILS)*/ { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
//        strncpy(m_name, "MsgQue-", sizeof(m_name));
//        new (&m_ptr) MemPtr(sizeof(*m_ptr), params.shmkey, params.srcline);
//        m_ptr = static_cast<decltype(m_ptr)*>(::shmalloc(sizeof(*m_ptr) /*+ Extra*/, shmkey, params.srcline))); //, SrcLine srcline = 0)
        m_ptr = memalloc<WANT_IPC(MAX_THREADs)>(size + ABS(MAX_THREADs) * sizeof(decltype(thrid())), params.shmkey, params.srcline);
        if (!m_ptr) return;
        IFIPC(if (::shmexisted(m_ptr) && !want_reinit) return);
        memset(m_ptr, 0, memsize(m_ptr)); //sizeof(*m_ptr) + EXTRA); //re-init (not needed first time)
//        m_ptr->mutex.std::mutex();
//        m_ptr->locked = false;
//        m_ptr->data.TYPE();
//        m_ptr->WithMutex<TYPE>(std::forward<ARGS>(args) ...);
//        m_ptr->WithMutex<TYPE, AUTO_LOCK>(std::forward<ARGS>(args ...)); //pass args to TYPE's ctor (perfect fwding)
        new (m_ptr) std::decay<decltype(*m_ptr)>(); //, srcline); //pass args to TYPE's ctor (perfect fwding)
//        /*if (!ids->size())*/ m_ptr->ids.reserve(max_threads); //avoid extraneous copy ctors later
        WANT_DEBUG(strncpy(m_ptr->name, (name && *name)? name: "(unnamed)", sizeof(m_ptr->name)));
    }
    ~MsgQue()
    {
        if (!m_ptr) return;
        if (m_ptr->msg /*&& MSGQUE_DETAILS*/) DEBUG_MSG(RED_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".dtor: !empty @exit " << FMT("0x%x") << m_ptr->msg << ENDCOLOR_ATLINE(srcline)) //benign, but might be caller bug so complain
        else DEBUG_MSG(GREEN_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".dtor: empty @exit" << ENDCOLOR_ATLINE(srcline));
//        if (m_autodel) delete this;
    }
#endif


#endif //ndef _MSGQUE_H
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _MSGQUE_H
#define _MSGQUE_H

#include <mutex>
#include <condition_variable>
//for example see http://en.cppreference.com/w/cpp/thread/condition_variable
#include <type_traits> //std::decay<>, std::remove_reference<>, std::remove_pointer<>
#include "shmalloc.h"

#ifndef VOLATILE
 #define VOLATILE
#endif

#ifdef MSGQUE_DEBUG
 #include "atomic.h" //ATOMIC_MSG()
 #include "ostrfmt.h" //FMT()
 #include "elapsed.h" //timestamp()
 #include "msgcolors.h" //SrcLine, msg colors
 #define DEBUG_MSG  ATOMIC_MSG
 #define WANT_DEBUG(stmt)  stmt
#else
 #define DEBUG_MSG(msg)  {} //noop
 #define WANT_DEBUG(stmt)  {} //noop
 #include "msgcolors.h" //still need SrcLine, msg colors
#endif
#ifndef MSGQUE_DETAILS
 #define MSGQUE_DETAILS  0
#endif

/*
#ifdef IPC_THREAD
 #include "ipc.h" //IpcThread; out-of-proc threads)
 #define IFIPC(stmt)  stmt
// #define malloc(size, shmkey)  ::shmalloc(size, shmkey, SRCLINE)
// #define memsize(ptr)  ::shmsize(ptr)
// typedef decltype(IpcThread::get_id()) THRID;
#else
 #include <thread> //std::this_thread; in-proc threads
 #define IFIPC(stmt)  //noop
// #define malloc(size, shmkey)  malloc(size)
// #define memsize(ptr)  sizeof(*ptr)
// typedef decltype(std::this_thread::get_id()) THRID;
#endif
*/

//#if 1 //new def; shm-compatible
//usage:
//    ShmMsgQue& msgque = *(ShmMsgQue*)shmalloc(sizeof(ShmMsgQue), shmkey, SRCLINE);
//    if (isParent) new (&msgque) ShmMsgQue(name, SRCLINE); //call ctor to init (parent only)
//    ...
//    if (isParent) { msgque.~ShmMsgQue(); shmfree(&msgque, SRCLINE); }

//#define SINGLE_THREADED  0
//#define MULTI_THREADED  1
//#define MULTI_PROCESS  -1
//template <bool IPC>

#ifndef NAMED
 #define NAMED  /*SRCLINE,*/ [&](auto& _)
#endif

//define template <bool, typename = void>
class MsgQueBase
{
};

template <bool INCL> //typename TYPE>
struct data_members<INCL, std::enable_if_t<INCL>>
{
    int some_variable[3];
    void hasit() { }
};


//safe for multi-threading (uses mutex):
//template <int MAX_THREADS = 0> //typename THRID>
template <int THREADS = 0> //bool WANT_IPC = false>
class MsgQue: public Shm<THREADS > 0> //std::enable_if<class MsgQueBase
{
public: //static methods
#ifdef IPC_THREAD
    static auto thrid() { return IpcThread::get_id(); }
//    typedef decltype(IpcThread::get_id()) THRID;
    static void* malloc(size_t size, int shmkey = 0, SrcLine srcline = 0) { return ::shmalloc(size, shmkey, srcline); }
    static size_t memsize(void* ptr) { return ::shmsize(ptr); }
#else
    static auto thrid() { return std::this_thread::get_id(); }
// typedef decltype(std::this_thread::get_id()) THRID;
    static void* malloc(size_t size, int shmkey_ignored = 0, SrcLine srcline = 0) { return malloc(size); }
    static size_t memsize(void* ptr) { return sizeof(*ptr); }
#endif
    static auto num_cpu() { return std::thread::hardware_concurrency(); }
public: //non-shared data (ctor params)
//    struct CtorParams
//    {
        const char* name = 0;
#ifdef IPC_THREAD //shmem handling info
//        IFIPC(int shmkey = 0); //shmem handling info
        int shmkey = 0; //shmem handling info
//        int extra = 0;
//        typedef decltype(IpcThread::get_id()) ThreadId;
#else
        const int shmkey = 0;
//        typedef decltype(std::this_thread::get_id()) ThreadId;
#endif
        bool want_reinit = true;
//        int max_threads = 4;
//        bool debug_free = true;
        SrcLine srcline = 0;
//    };
//    typedef typename std::conditional<AUTO_LOCK, WithMutex<TYPE, AUTO_LOCK>, TYPE>::type shm_type; //see https://stackoverflow.com/questions/17854407/how-to-make-a-conditional-typedef-in-c?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
//    shm_type* m_ptr;
//    bool m_want_init, m_debug_free;
//protected:
private: //data
//    static std::mutex my_mutex; //assume low usage, share across all signals
    struct //shared data; wrapped in case it needs to be in shared memory
    {
        VOLATILE std::mutex mutex;
        std::condition_variable condvar;
//    int m_msg[MAXLEN], m_count;
//    std::string m_name; //for debug only
        WANT_DEBUG(char name[20]); //store name directly in object so shm object doesn't use char pointer (only for debug)
        /*volatile*/ int msg;
        PreallocVector<decltype(thrid()) /*, MAX_THREADS*/> ids; //list of registered thread ids; NOTE: must be last
        decltype(thrid()) id_list[num_cpu()];
//    } IFIPC(*) m_data; //shmem wrapper
#ifdef IPC_THREAD
    }* m_ptr;
#else
    } m_ptr[1];
#endif
//    IFIPC(typedef std::remove_pointer<decltype(m_data)> shmtype);
//    typedef std::remove_pointer<decltype(m_data)> shmtype;
public: //ctors/dtors
//    explicit MsgQue(const char* name = 0, VOLATILE std::mutex& mutex = /*std::mutex()*/ shared_mutex): m_msg(0), m_mutex(mutex)
//    explicit MsgQue(const char* name = 0): m_msg(0)
    explicit MsgQue(SrcLine mySrcLine = 0, void (*get_params)(MsgQue&) = 0) //: m_msg(0) //int& i, std::string& s, bool& b, SrcLine& srcline) = 0) //: i(0), b(false), srcline(0), o(nullptr)
    {
//        /*static*/ struct CtorParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
        if (mySrcLine) /*params.*/srcline = mySrcLine;
        if (get_params) get_params(*this); //params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//        /*if (MSGQUE_DETAILS)*/ { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
//        strncpy(m_name, "MsgQue-", sizeof(m_name));
        IFIPC(m_ptr = static_cast<decltype(m_ptr)>(::shmalloc(sizeof(*m_ptr) /*+ Extra*/, shmkey, params.srcline))); //, SrcLine srcline = 0)
        if (!m_ptr) return;
        IFIPC(if (::shmexisted(m_ptr) && !want_reinit) return);
        memset(m_ptr, 0, memsize(m_ptr)); //sizeof(*m_ptr) + EXTRA); //re-init (not needed first time)
//        m_ptr->mutex.std::mutex();
//        m_ptr->locked = false;
//        m_ptr->data.TYPE();
//        m_ptr->WithMutex<TYPE>(std::forward<ARGS>(args) ...);
//        m_ptr->WithMutex<TYPE, AUTO_LOCK>(std::forward<ARGS>(args ...)); //pass args to TYPE's ctor (perfect fwding)
        new (m_ptr) std::decay<decltype(*m_ptr)>(); //, srcline); //pass args to TYPE's ctor (perfect fwding)
//        /*if (!ids->size())*/ m_ptr->ids.reserve(max_threads); //avoid extraneous copy ctors later
        WANT_DEBUG(strncpy(m_ptr->name, (name && *name)? name: "(unnamed)", sizeof(m_ptr->name)));
    }
    ~MsgQue()
    {
        if (!m_ptr) return;
        if (m_ptr->msg /*&& MSGQUE_DETAILS*/) DEBUG_MSG(RED_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".dtor: !empty @exit " << FMT("0x%x") << m_ptr->msg << ENDCOLOR_ATLINE(srcline)) //benign, but might be caller bug so complain
        else DEBUG_MSG(GREEN_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".dtor: empty @exit" << ENDCOLOR_ATLINE(srcline));
//        if (m_autodel) delete this;
    }
public: //mem alloc
#if 0
    static void* operator new(size_t size, const char* srcfile, int srcline, bool autodel = false)
    {
        void* ptr = new char[size];
        const char* bp = strrchr(srcfile, '/');
        if (bp) srcfile = bp + 1;
        DEBUG_MSG(YELLOW_MSG << timestamp() << "alloc size " << size << " at adrs " << FMT("0x%x") << ptr << ", autodef? " << autodel << " from " << srcfile << ":" << srcline << ENDCOLOR << std::flush);
//        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
//        m_autodel = autodel; //won't work with static method
//        if (autodel)
//#include <cstdlib>
//void exiting() { std::cout << "Exiting"; }
//    std::atexit(exiting);
        return ptr;
    }
    static void operator delete(void* ptr) noexcept
    {
  //      shmheap.dealloc(ptr);
        delete ptr;
        DEBUG_MSG(YELLOW_MSG << timestamp() << "dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
    }
#endif
public: //getters
    inline VOLATILE std::mutex& mutex() { return m_ptr->mutex; } //in case caller wants to share between instances
public: //operators
//    MsgQue* operator->() { return this; } //for compatibility with Shm wrapper operator->
//    MsgQue* get() { return this; }
public: //methods
//    MsgQue& clear() { m_msg = 0; return *this; } //fluent
public: //methods
    MsgQue& clear()
    {
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
        m_ptr->msg = 0;
        return *this; //fluent
    }
//    struct SendParams { int msg = 0; bool broadcast = false; SrcLine srcline = SRCLINE; }; //FuncParams() { s = "none"; i = 999; b = true; }}; // FuncParams;
    MsgQue& send(int msg, bool broadcast = false, SrcLine srcline = 0)
//    MsgQue& send(SrcLine mySrcLine = 0, void (*get_params)(struct SendParams&) = 0)
    {
//        /*static*/ struct SendParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
//        std::stringstream ssout;
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_ptr->msg & msg) throw "MsgQue.send: msg already queued"; // + tostr(msg) + " already queued");
        m_ptr->msg |= msg; //use bitmask for multiple msgs
        if (!(m_ptr->msg & msg)) throw "MsgQue.send: msg enqueue failed"; // + tostr(msg) + " failed");
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "one") << ", now qued " << FMT("0x%x") << m_ptr->msg << FMT(", adrs %p") << this << ENDCOLOR_ATLINE(srcline));
        if (!broadcast) m_ptr->condvar.notify_one(); //wake main thread
        else m_ptr->condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << FMT("0x%x") << m_ptr->msg << " wanted " << FMT("0x%x") << val << "? " << (m_ptr->msg == val) << ENDCOLOR); return m_ptr->msg == val; }
    bool not_wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << FMT("0x%x") << m_ptr->msg << " !wanted " << FMT("0x%x") << val << "? " << (m_ptr->msg != val) << ENDCOLOR); return m_ptr->msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
//    struct RcvParams { bool (MsgQue::*filter)(int val) = 0, int operand = 0, bool remove = false, SrcLine srcline = SRCLINE; };
    int rcv(bool (MsgQue::*filter)(int val), int operand = 0, bool remove = false, SrcLine srcline = 0)
//    int rcv(SrcLine mySrcLine = 0, void (*get_params)(struct RcvParams&) = 0)
    {
//        /*static*/ struct RcvParams params; // = {"none", 999, true}; //allow caller to set func params without allocating struct; static retains values for next call (CAUTION: shared between instances)
//        if (mySrcLine) params.srcline = mySrcLine;
//        if (get_params) get_params(params); //params.i, params.s, params.b, params.srcline); //NOTE: must match macro signature; //get_params(params);
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
//        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << "MsgQue-" << m_ptr->name << ".rcv: op " << FMT("0x%x") << operand << ", msg " << FMT("0x%x") << m_ptr->msg << FMT(", adrs %p") << this << ENDCOLOR_ATLINE(srcline));
        while (!(this->*filter)(operand)) m_ptr->condvar.wait(scoped_lock); //ignore spurious wakeups
        int retval = m_ptr->msg;
        if (remove) m_ptr->msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
#if 0
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
#endif
public: //methods
//convert thread/procid to terse int:
//#include <unistd.h> //getpid()
//#include "critical.h"
    int thrinx(decltype(thrid())& id) //THRID& id) //bool locked = false)
    {
//        typedef decltype(id) ThreadId;
//        static vectype& ids = SHARED(SRCKEY, vectype, vectype);
//    std::unique_lock<std::mutex> lock(ShmHeapAlloc::shmheap.mutex()); //low usage; reuse mutex
//#else
//    ShmScope<type> scope(SRCLINE, "testobj", SRCLINE); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    type& testobj = scope.shmobj; //ShmObj<TestObj>("testobj", thread, SRCLINE);
//    typedef WithMutex<vector_ex<THREAD::id>> type;
//    static vector_ex<THREAD::id> ids(ABS(NUM_WKERs)); //preallocate space
//    std::unique_lock<std::mutex> lock(atomic_mut); //low usage; reuse mutex
//    ShmScope<type, ABS(NUM_WKERs) + 1> scope(SRCLINE, SHMKEY3); //shm obj wrapper; call dtor when goes out of scope (parent only)
//    static ShmPtr_params settings(SRCLINE, THRIDS_SHMKEY, (ABS(NUM_WKERs) + 1) * sizeof(THREAD::id), false); //don't need auto-lock due to explicit critical section
//    static ShmPtr<type> ids;
//    {
//    CriticalSection<SHARED_CRITICAL_SHMKEY> cs(SRCLINE);
//    if (!ids->size()) ids->reserve(ABS(NUM_WKERs) + 1); //avoid extraneous copy ctors later
//    }
//    ids->reserve(ABS(NUM_WKERs) + 1); //avoid extraneous copy ctors later; only needs to happen 1x; being lazy: just use wrapped method rather than using a critical section with a condition
//    type& ids = scope.shmobj.data; //ShmObj<TestObj>("testobj", thread, SRCLINE);
//#endif
//        return thrid(true);
//    }
//        std::lock_guard<std::mutex> lock(m);
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_ptr->mutex);
//NOTE: use op->() for shm safety with ipc
        int ofs = m_ptr->ids.find(id);
        if (ofs != -1) throw std::runtime_error(RED_MSG "thrid: duplicate thread id" ENDCOLOR_NOLINE);
        if (ofs == -1) { ofs = m_ptr->ids.size(); m_ptr->ids.push_back(id); } //ofs = ids.push_and_find(id);
//    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    ss << ofs;
//    return ss.str();
        ATOMIC_MSG(CYAN_MSG << timestamp() << "pid '" << getpid() << FMT("', thread id 0x%lx") << id << " => thr inx " << ofs << ENDCOLOR);
        return ofs;
    }
//protected:
private: //helpers
//can't use nested class on non-static members :(
//    class scoped_lock: public std::unique_lock<std::mutex>
//    {
//    public:
//        /*explicit*/ scoped_lock(): std::unique_lock<std::mutex>(m_mutex) {};
////        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
//    };
//    static const char* tostr(int val)
//    {
//        static char buf[20];
//        snprintf(buf, sizeof(buf), "0x%x", val);
//        return buf;
//    }
};
//#endif
//#define MsgQue  MsgQue<>

//#ifdef SHM_KEY
// #include "shmalloc.h"
//ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, 0x4567feed);
//#endif

//#ifdef SHM_KEY
//multi-process queue:
//can be accessed by multiple proceses
//class MsgQue: public ShmHeapAlloc //: public MsgQue
//#else
//single process queue:
//can be accessed by multiple threads within same process
//class MsgQue: public MsgQueBase
#if 0 //old def (working)
#define VOLATILE  //volatile //TODO: is this needed for multi-threaded app?
static VOLATILE std::mutex shared_mutex;

class MsgQue//Base
//#endif
{
public: //ctor/dtor
    explicit MsgQue(const char* name = 0, VOLATILE std::mutex& mutex = /*std::mutex()*/ shared_mutex): m_msg(0), m_mutex(mutex)
    {
        /*if (MSGQUE_DETAILS)*/ { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
    }
    ~MsgQue()
    {
        if (m_msg /*&& MSGQUE_DETAILS*/) DEBUG_MSG(RED_MSG << m_name << ".dtor: !empty @exit " << FMT("0x%x") << m_msg << ENDCOLOR) //benign, but might be caller bug so complain
        else DEBUG_MSG(GREEN_MSG << m_name << ".dtor: empty @exit" << ENDCOLOR);
//        if (m_autodel) delete this;
    }
#if 0
    static void* operator new(size_t size, const char* srcfile, int srcline, bool autodel = false)
    {
        void* ptr = new char[size];
        const char* bp = strrchr(srcfile, '/');
        if (bp) srcfile = bp + 1;
        DEBUG_MSG(YELLOW_MSG << "alloc size " << size << " at adrs " << FMT("0x%x") << ptr << ", autodef? " << autodel << " from " << srcfile << ":" << srcline << ENDCOLOR << std::flush);
//        if (!ptr) throw std::runtime_error("ShmHeap: alloc failed"); //debug only
//        m_autodel = autodel; //won't work with static method
//        if (autodel)
//#include <cstdlib>
//void exiting() { std::cout << "Exiting"; }
//    std::atexit(exiting);
        return ptr;
    }
    static void operator delete(void* ptr) noexcept
    {
  //      shmheap.dealloc(ptr);
        delete ptr;
        DEBUG_MSG(YELLOW_MSG << "dealloc adrs " << FMT("0x%x") << ptr << ENDCOLOR << std::flush);
    }
#endif
public: //getters
    inline VOLATILE std::mutex& mutex() { return m_mutex; } //in case caller wants to share between instances
public: //methods
    MsgQue& clear()
    {
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_mutex);
        m_msg = 0;
        return *this; //fluent
    }
    MsgQue& send(int msg, bool broadcast = false)
    {
//        std::stringstream ssout;
//        scoped_lock lock;
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_mutex);
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_msg & msg) throw "MsgQue.send: msg already queued"; // + tostr(msg) + " already queued");
        m_msg |= msg; //use bitmask for multiple msgs
        if (!(m_msg & msg)) throw "MsgQue.send: msg enqueue failed"; // + tostr(msg) + " failed");
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "one") << ", now qued " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
    int rcv(bool (MsgQue::*filter)(int val), int operand = 0, bool remove = false)
    {
        std::unique_lock<VOLATILE std::mutex> scoped_lock(m_mutex);
//        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".rcv: op " << FMT("0x%x") << operand << ", msg " << FMT("0x%x") << m_msg << FMT(", adrs %p") << this << ENDCOLOR);
        while (!(this->*filter)(operand)) m_condvar.wait(scoped_lock); //ignore spurious wakeups
        int retval = m_msg;
        if (remove) m_msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
#if 0
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
#endif
//protected:
private: //helpers
//can't use nested class on non-static members :(
//    class scoped_lock: public std::unique_lock<std::mutex>
//    {
//    public:
//        /*explicit*/ scoped_lock(): std::unique_lock<std::mutex>(m_mutex) {};
////        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
//    };
//    static const char* tostr(int val)
//    {
//        static char buf[20];
//        snprintf(buf, sizeof(buf), "0x%x", val);
//        return buf;
//    }
//protected:
private: //data
//    static std::mutex my_mutex; //assume low usage, share across all signals
    VOLATILE std::mutex& m_mutex;
    std::condition_variable m_condvar;
//    int m_msg[MAXLEN], m_count;
    std::string m_name; //for debug only
    /*volatile*/ int m_msg;
};
//std::mutex MsgQue::my_mutex;
#endif


#if 0
#include "shmalloc.h"
//ShmHeap ShmHeapAlloc::shmheap(100, ShmHeap::persist::NewPerm, 0x4567feed);

//multi-process queue:
//can be accessed by multiple proceses
class ShmMsgQue: public ShmHeapAlloc //: public MsgQue
{
public:
    explicit ShmMsgQue(const char* name = 0): m_msg(0)
    {
        if (MSGQUE_DETAILS) { m_name = "MsgQue-"; m_name += (name && *name)? name: "(unnamed)"; }
    }
    ~ShmMsgQue()
    {
        if (m_msg && MSGQUE_DETAILS) DEBUG_MSG(RED_MSG << m_name << ".dtor: !empty" << FMT("0x%x") << m_msg << ENDCOLOR); //benign, but might be caller bug so complain
    }
public:
    ShmMsgQue& clear()
    {
        m_msg = 0;
        return *this; //fluent
    }
    ShmMsgQue& send(int msg, bool broadcast = false)
    {
//        std::stringstream ssout;
        scoped_lock lock;
//        if (m_count >= MAXLEN) throw new Error("MsgQue.send: queue full (" MAXLEN ")");
        if (m_msg & msg) throw "MsgQue.send: msg already queued"; // + tostr(msg) + " already queued");
        m_msg |= msg; //use bitmask for multiple msgs
        if (!(m_msg & msg)) throw "MsgQue.send: msg enqueue failed"; // + tostr(msg) + " failed");
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".send " << FMT("0x%x") << msg << " to " << (broadcast? "all": "1") << ", now qued " << FMT("0x%x") << m_msg << ENDCOLOR);
        if (!broadcast) m_condvar.notify_one(); //wake main thread
        else m_condvar.notify_all(); //wake *all* wker threads
        return *this; //fluent
    }
//rcv filters:
    bool wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " wanted " << FMT("0x%x") << val << "? " << (m_msg == val) << ENDCOLOR); return m_msg == val; }
    bool not_wanted(int val) { if (MSGQUE_DETAILS) DEBUG_MSG(FMT("0x%x") << m_msg << " !wanted " << FMT("0x%x") << val << "? " << (m_msg != val) << ENDCOLOR); return m_msg != val; }
//    bool any(int ignored) { return true; } //don't use this (doesn't work due to spurious wakeups)
    int rcv(bool (ShmMsgQue::*filter)(int val), int operand = 0, bool remove = false)
    {
//        std::unique_lock<std::mutex> lock(mutex);
        scoped_lock lock;
//        m_condvar.wait(scoped_lock());
        if (MSGQUE_DETAILS) DEBUG_MSG(BLUE_MSG << timestamp() << m_name << ".rcv: " << FMT("0x%x") << m_msg << ENDCOLOR);
        while (!(this->*filter)(operand)) m_condvar.wait(lock); //ignore spurious wakeups
        int retval = m_msg;
        if (remove) m_msg = 0;
        return retval;
#if 0
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-rcv 0x%x 0x%x, pending %d" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size());
        while (!this->pending.size()) //NOTE: need loop in order to handle "spurious wakeups"
        {
            if (/*!cond ||*/ !OK(SDL_CondWait(cond, mutex))) exc(RED_MSG "Wait for signal 0x%x:(0x%x,0x%x) failed" ENDCOLOR, toint(this), toint(mutex.cast), toint(cond.cast)); //throw SDL_Exception("SDL_CondWait");
            if (!this->pending.size()) err(YELLOW_MSG "Ignoring spurious wakeup" ENDCOLOR); //paranoid
        }
        void* data = pending.back(); //signal already happened
//        myprintf(33, "here-rcv got 0x%x" ENDCOLOR, toint(data));
        pending.pop_back();
        myprintf(30, BLUE_MSG "rcved[%d] 0x%x from signal 0x%x" ENDCOLOR, this->pending.size(), toint(data), toint(this));
        return data;
#endif
    }
#if 0
    void wake(void* msg = NULL)
    {
        auto_ptr<SDL_LockedMutex> lock_HERE(mutex.cast); //SDL_LOCK(mutex));
        myprintf(33, "here-send 0x%x 0x%x, pending %d, msg 0x%x" ENDCOLOR, toint(mutex.cast), toint(cond.cast), this->pending.size(), toint(msg));
        this->pending.push_back(msg); //remember signal happened in case receiver is not listening yet
        if (/*!cond ||*/ !OK(SDL_CondSignal(cond))) exc(RED_MSG "Send signal 0x%x failed" ENDCOLOR, toint(this)); //throw SDL_Exception("SDL_CondSignal");
//        myprintf(33, "here-sent 0x%x" ENDCOLOR, toint(msg));
        myprintf(30, BLUE_MSG "sent[%d] 0x%x to signal 0x%x" ENDCOLOR, this->pending.size(), toint(msg), toint(this));
    }
#endif
//protected:
private:
    class scoped_lock: public std::unique_lock<std::mutex>
    {
    public:
        explicit scoped_lock(): std::unique_lock<std::mutex>(m_mutex) {};
//        ~scoped_lock() { ATOMIC(cout << "unlock\n"); }
    };
//    static const char* tostr(int val)
//    {
//        static char buf[20];
//        snprintf(buf, sizeof(buf), "0x%x", val);
//        return buf;
//    }
//protected:
private:
    static std::mutex m_mutex; //assume low usage, share across all signals
    std::condition_variable m_condvar;
//    int m_msg[MAXLEN], m_count;
    std::string m_name; //for debug only
    /*volatile*/ int m_msg;
};
std::mutex ShmMsgQue::m_mutex;
#endif


//#undef malloc
#undef IF_DEBUG
#undef DEBUG_MSG
#endif //ndef _MSGQUE_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit tests:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion
#include <iostream> //std::cout, std::flush
#include <chrono> //std::chrono
#include "msgcolors.h"
#include "elapsed.h" //timestamp()

#ifndef MSG
 #define MSG(msg)  { std::cout << timestamp() << msg << std::flush; }
#endif


void sleep_msec(int msec)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}


#include "shmkeys.h"
MsgQue<1> testq(NAMED{ _.name = "testq"; IF_IPC(_.shmkey = SHMKEY1); SRCLINE; }); ///*_.extra = 0*/; _.want_reinit = false; });
 

void child_proc()
{
    sleep_msec(500);
    MSG("child start, send");
    testq->send(NAMED{ _.msg = 1; SRCLINE; });
    int qe = testq->rcv(NAMED{ SRCLINE; });
    MSG(FMT("child rcv got 0x%x") << qe);
    MSG("child exit");
}


//int main(int argc, const char* argv[])
void unit_test()
{
    std::thread child(child_proc);
    MSG("main start, rcv");
    int qe = testq->rcv(NAMED{ SRCLINE; });
    MSG(FMT("main rcv got 0x%x") << qe);

    sleep_msec(500);
    MSG("main send 1");
    testq->send(NAMED{ _.msg = 1; SRCLINE; });
    MSG("main join");
    child.join();
    MSG("done");

//    return 0;
}
#endif //def WANT_UNIT_TEST
//eof