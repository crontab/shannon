

#include "except.h"


Exception::~Exception() throw() { }


string EMessage::what() const throw() { return message; }
EMessage::~EMessage() throw()  { }


EInternal::EInternal(int code)
    : EMessage("Internal error #" + itostring(code))  {}
EInternal::EInternal(int code, const string& hint)
    : EMessage("Internal error #" + itostring(code) + " (" + hint + ')')  {}
EInternal::~EInternal() throw()  { }


EDuplicate::EDuplicate(const string& ientry)
    : Exception(), entry(ientry) { }
EDuplicate::~EDuplicate() throw()  { }
string EDuplicate::what() const throw() { return "Duplicate identifier '" + entry + '\''; }


ENotFound::ENotFound(const string& ientry)
    : Exception(), entry(ientry) { }
ENotFound::~ENotFound() throw()  { }
string ENotFound::what() const throw() { return "Unknown identifier '" + entry + '\''; }


ESysError::~ESysError() throw()  { }


string ESysError::what() const throw()
{
    char buf[1024];
    strerror_r(code, buf, sizeof(buf));
    return buf;
}


