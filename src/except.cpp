

#include "except.h"


Exception::~Exception()  { }


string EMessage::what() const  { return message; }
EMessage::~EMessage()  { }


EDuplicate::EDuplicate(const string& ientry)
    : Exception(), entry(ientry) { }
EDuplicate::~EDuplicate()  { }
string EDuplicate::what() const  { return "Duplicate identifier '" + entry + '\''; }


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


class EInternal: public EMessage
{
public:
    EInternal(int code);
    EInternal(int code, string const& hint);
    virtual ~EInternal();
};

EInternal::EInternal(int code)
    : EMessage("Internal error #" + itostring(code))  {}
EInternal::EInternal(int code, const string& hint)
    : EMessage("Internal error #" + itostring(code) + " (" + hint + ')')  {}
EInternal::~EInternal()  { }


void internal(int code)
{
    throw EInternal(code);
}

void internal(int code, const char* msg)
{
    throw EInternal(code, msg);
}
