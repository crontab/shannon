
#include <stdio.h>

#include "common.h"


Exception::Exception(const string& msg): message(msg)  { }
Exception::~Exception()  { }


EDuplicate::EDuplicate(const string& ientry)
    : Exception("Duplicate identifier '" + ientry + '\''), entry(ientry)  { }
EDuplicate::~EDuplicate()  { }


static string sysErrorStr(int code, const string& arg)
{
    // For some reason strerror_r() returns garbage on my 64-bit Ubuntu. That's unfortunately
    // not the only strange thing about this computer and OS. Could be me, could be hardware
    // or could be Linux. Or all.
//    char buf[1024];
//    strerror_r(code, buf, sizeof(buf));
    string result = strerror(code);
    if (!arg.empty())
        result += " (" + arg + ")";
    return result;
}


ESysError::ESysError(int code, const string& arg)
    : Exception(sysErrorStr(code, arg))  { }


struct EInternal: public Exception
{
public:
    EInternal(int code);
    EInternal(int code, string const& hint);
};

EInternal::EInternal(int code)
    : Exception("Internal error #" + itostring(code))  {}
EInternal::EInternal(int code, const string& hint)
    : Exception("Internal error #" + itostring(code) + " (" + hint + ')')  {}


void internal(int code)
{
    throw EInternal(code);
}

void internal(int code, const char* msg)
{
    throw EInternal(code, msg);
}
