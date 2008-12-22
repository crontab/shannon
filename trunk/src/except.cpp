

#include "except.h"


Exception::~Exception()  { }


string EMessage::what() const  { return message; }
EMessage::~EMessage()  { }


EInternal::EInternal(int code)
    : EMessage("Internal error #" + itostring(code))  {}
EInternal::EInternal(int code, const string& hint)
    : EMessage("Internal error #" + itostring(code) + " (" + hint + ')')  {}
EInternal::~EInternal()  { }


EDuplicate::EDuplicate(const string& ientry)
    : Exception(), entry(ientry) { }
EDuplicate::~EDuplicate()  { }
string EDuplicate::what() const  { return "Duplicate identifier '" + entry + '\''; }


ENotFound::ENotFound(const string& ientry)
    : Exception(), entry(ientry) { }
ENotFound::~ENotFound()  { }
string ENotFound::what() const  { return "Unknown identifier '" + entry + '\''; }


ESysError::~ESysError()  { }


string ESysError::what() const
{
    char buf[1024];
    strerror_r(code, buf, sizeof(buf));
    string result = buf;
    if (!arg.empty())
        result += " (" + arg + ")";
    return "Error: " + result;
}


