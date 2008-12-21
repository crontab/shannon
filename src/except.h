#ifndef __EXCEPT_H
#define __EXCEPT_H


#include "str.h"


class Exception
{
public:
    virtual ~Exception() throw();
    virtual string what() const throw() = 0;
};


class EMessage: public Exception
{
    string message;
public:
    EMessage(const string& imessage): Exception(), message(imessage)  { }
    virtual ~EMessage() throw();
    virtual string what() const throw();
};


class EInternal: public EMessage
{
public:
    EInternal(int code);
    EInternal(int code, string const& hint);
    virtual ~EInternal() throw();
};


class EDuplicate: public Exception
{
    string entry;
public:
    EDuplicate(const string& ientry);
    virtual ~EDuplicate() throw();
    virtual string what() const throw();
    const string& getEntry() const throw() { return entry; }
};


class ENotFound: public Exception
{
    string entry;
public:
    ENotFound(const string& ientry);
    virtual ~ENotFound() throw();
    virtual string what() const throw();
    const string& getEntry() const throw() { return entry; }
};


class ESysError: public Exception
{
    int code;
    string arg;
public:
    ESysError(int icode): Exception(), code(icode)  { }
    ESysError(int icode, const string& iArg)
            : Exception(), code(icode), arg(iArg)  { }
    virtual ~ESysError() throw();
    virtual string what() const throw();
};


#endif

