#ifndef __EXCEPT_H
#define __EXCEPT_H


#include "str.h"


class Exception
{
public:
    virtual ~Exception();
    virtual string what() const = 0;
};


class EMessage: public Exception
{
    string message;
public:
    EMessage(const string& imessage): Exception(), message(imessage)  { }
    virtual ~EMessage();
    virtual string what() const;
};


class EDuplicate: public Exception
{
    string entry;
public:
    EDuplicate(const string& ientry);
    virtual ~EDuplicate();
    virtual string what() const;
    const string& getEntry() const  { return entry; }
};


class ESysError: public Exception
{
    int code;
    string arg;
public:
    ESysError(int icode): Exception(), code(icode)  { }
    ESysError(int icode, const string& iArg)
            : Exception(), code(icode), arg(iArg)  { }
    virtual ~ESysError();
    virtual string what() const;
};


void internal(int code);


#endif
