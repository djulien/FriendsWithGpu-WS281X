//eat next few chars from c++ ostream

#ifndef _EATCH_H
#define _EATCH_H

#include <sstream> //std::stringstream

//custom stream manipulator for printf-style formating:
//idea from https://stackoverflow.com/questions/535444/custom-manipulator-for-c-iostream
//and https://stackoverflow.com/questions/11989374/floating-point-format-for-stdostream
//std::ostream& fmt(std::ostream& out, const char* str)


//eat chars from ostream:
//see http://www.dreamincode.net/forums/topic/175448-custom-output-manipulators-with-arguments/
//std::ostream& eat(std::ostream& os)
//{
//    return os << '\t' << std::setfill(' ') << std::setw(8);
//}
class eatch
{
    int m_count;
public:
    explicit eatch(int count): m_count(count) {}
#if 0
    friend std::ostream& operator<<(std::ostream& ostrm, const eatch& eater) { return eater.eat(ostrm); }
private: //helpers
    std::ostream& eat(std::ostream& ostrm) const
    {
        long pos = ostrm.tellp();
        printf("pos %ld\n", pos);
        if (pos != -1) if (m_count > 0) ostrm.seekp(pos - m_count);
        return ostrm;
    }
#else
private: //helpers
    class eater //actual worker class
    {
        std::ostream& m_strm;
        int m_count;
    public:
//output next object (any type) to stream:
        explicit eater(std::ostream& ostrm, const eatch& eater): m_strm(ostrm), m_count(eater.m_count) {}
        template<typename TYPE>
        std::ostream& operator<<(const TYPE& value)
        {
            std::stringstream ss;
            ss << value;
//            printf("str len %d, skip len %d\n", ss.str().length(), m_count);
//TODO: if m_count > str().length() then carry forward remaining count
            if (m_count > 0) m_strm << ss.str().substr(m_count);
            else m_strm << ss.str();
            return m_strm;
        }
    };
//kludge: return derived stream to allow operator overloading:
    friend eatch::eater operator<<(std::ostream& ostrm, const eatch& eater) { return eatch::eater(ostrm, eater); }
#endif
};


#endif //ndef _EATCH_H


#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

#include "eatch.h"

#ifndef MSG
 #define MSG(msg)  { std::cout << msg << "\n" << std::flush; }
#endif


//int main(int argc, const char* argv[])
void unit_test()
{
    MSG("abcdef" << eatch(2) << "xyzzy" << eatch(-1) << "mnop" << eatch(1) << "bye");
//    return 0;
}

#endif //def WANT_UNIT_TEST