////////////////////////////////////////////////////////////////////////////////
////
/// Safe pointer/wrapper (auto-dealloc resources when they go out of scope)
//

//make debug easier by tagging with name and location:
//#define TRACKED(thing)  thing.newwhere = __LINE__; thing.newname = TOSTR(thing); thing

#if 0 //use std::unique_ptr instead
#ifndef _AUTO_PTR_H
#define _AUTO_PTR_H

//type-safe wrapper for SDL ptrs that cleans up when it goes out of scope:
//std::shared_ptr<> doesn't allow assignment or provide casting; this custom one does
template<typename DataType, typename = void>
class auto_ptr
{
//TODO: private:
public:
    DataType* cast;
public:
//ctor/dtor:
    auto_ptr(DataType* that = NULL): cast(that) {}; // myprintf(22, YELLOW_MSG "assign %s 0x%x" ENDCOLOR, TypeName(that), toint(that)); };
    ~auto_ptr() { this->release(); }
//cast to regular pointer:
//    operator const PtrType*() { return this->cast; }
//TODO; for now, just make ptr public
//conversion:
//NO; lval bypasses op=    operator /*const*/ DataType*&() { return this->cast; } //allow usage as l-value; TODO: would that preserve correct state?
    operator /*const*/ DataType*() { return this->cast; } //r-value only; l-value must use op=
//assignment:
    auto_ptr& operator=(/*const*/ DataType* /*&*/ that)
    {
        if (that != this->cast) //avoid self-assignment
        {
            this->release();
//set new state:
//            myprintf(22, YELLOW_MSG "reassign %s 0x%x" ENDCOLOR, TypeName(that), toint(that));
            this->cast = that; //now i'm responsible for releasing new object (no ref counting)
        }
        return *this; //fluent (chainable)
    }
public:
//nested class for scoped auto-lock:
    class lock //TODO: replace SDL_LOCK and SDL*Locked* with this
    {
		friend class auto_ptr;
    public:
        lock() { myprintf(1, YELLOW_MSG "TODO" ENDCOLOR); };
        ~lock() {};
    };
public:
    void release()
    {
        if (this->cast) Release(this->cast); //overloaded
        this->cast = NULL;
    }
    DataType* keep() //scope-crossing helper
    {
        DataType* retval = this->cast;
        this->cast = NULL; //new owner is responsible for dealloc
        return retval;
    }
};
//#define auto_ptr(type)  auto_ptr<type, TOSTR(type)>
//#define auto_ptr  __SRCLINE__ auto_ptr_noline

#endif //ndef _AUTO_PTR_H
#endif