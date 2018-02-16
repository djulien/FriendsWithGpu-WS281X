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
};
//template<class TYPE>
//std::allocator<TYPE> vector_ex<TYPE>::def_alloc; //default allocator; TODO: is this needed?


#endif //ndef _VECTOREX_H