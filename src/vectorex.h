//std::vector extensions

#ifndef _VECTOREX_H
#define _VECTOREX_H

#include <vector>
#include <memory> //std::allocator
#include <string.h> //strlen

//extended vector:
//adds find() and join() methods
template<class TYPE, class ALLOC = std::allocator<TYPE>>
class vector_ex: public std::vector<TYPE>
{
public: //ctors
//    static std::allocator<TYPE> def_alloc; //default allocator; TODO: is this needed?
//    /*explicit*/ vector_ex() {}
//    explicit vector_ex(std::size_t count, const std::allocator<TYPE>& alloc = def_alloc): std::vector<TYPE>(count, alloc) {}
    explicit vector_ex(std::size_t count = 0, const ALLOC& alloc = ALLOC()): std::vector<TYPE>(count, alloc) {}
public: //extensions
    int find(const TYPE& that)
    {
//        for (auto& val: *this) if (val == that) return &that - this;
        for (auto it = this->begin(); it != this->end(); ++it)
            if (*it == that) return it - this->begin();
        return -1;
    }
    std::string join(const char* sep = ",", const char* if_empty = "")
    {
        std::stringstream buf;
        for (auto it = this->begin(); it != this->end(); ++it)
            buf << sep << *it;
//        return this->size()? buf.str().substr(strlen(sep)): buf.str();
        if (!this->size()) buf << sep << if_empty; //"(empty)";
        return buf.str().substr(strlen(sep));
    }
//atomic push + find:
    int push_and_find(const TYPE& that)
    {
        int retval = this->size();
        this->push_back(that);
        return retval;
    }
};
//template<class TYPE>
//std::allocator<TYPE> vector_ex<TYPE>::def_alloc; //default allocator; TODO: is this needed?

template<class TYPE, int SIZE>
class PreallocVector
{
//public: //members
    TYPE m_list[std::max(SIZE, 1)];
    size_t m_count;
public: //ctors
    explicit PreallocVector(): m_count(0) {}
    ~PreallocVector() {}
public: //methods
    size_t size() { return m_count; }
    void push_back(TYPE& that)
    {
        if (m_count >= SIZEOF(m_list)) throw std::runtime_error("PreallocVector: size overflow");
        new (m_list + m_count++) TYPE(that); //placement new, copy ctor
    }
    template<typename ... ARGS> \
    void emplace_back(ARGS&& ... args)
    {
        if (m_count >= SIZEOF(m_list)) throw std::runtime_error("PreallocVector: bad index");
        new (m_list + m_count++) TYPE(std::forward<ARGS>(args) ...); //placement new; perfect fwding
    }
    TYPE& operator[](int inx)
    {
        if ((inx < 0) || (m_count >= SIZEOF(m_list))) throw std::runtime_error("PreallocVector: bad index");
        return m_list[inx];
    }
    int find(const TYPE& that)
    {
        for (int inx = 0; inx < SIZEOF(m_list); ++inx)
            if (m_list[inx] == that) return inx;
        return -1;
    }
    std::string join(const char* sep = ",", const char* if_empty = "")
    {
        std::stringstream buf;
        for (int inx = 0; inx < SIZEOF(m_list); ++inx)
            buf << sep << m_list[inx];
        if (!this->size()) buf << sep << if_empty; //"(empty)";
        return buf.str().substr(strlen(sep));
    }
//atomic push + find:
    int push_and_find(const TYPE& that)
    {
        int retval = this->size();
        this->push_back(that);
        return retval;
    }
};

#endif //ndef _VECTOREX_H