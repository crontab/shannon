
#include "str.h"


static char* _itobase(large value, char* buf, int base, int& len, bool _signed)
{
    // internal conversion routine: converts the value to a string 
    // at the end of the buffer and returns a pointer to the first
    // character. this is to get rid of copying the string to the 
    // beginning of the buffer, since finally the string is supposed 
    // to be copied to a dynamic string in itostring(). the buffer 
    // must be at least 65 bytes long.

    static char digits[65] = 
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    char* pdigits;
    if (base > 36)
	pdigits = digits;       // start from '.'
    else
	pdigits = digits + 2;   // start from '0'
    
    int i = 64;
    buf[i] = 0;

    bool neg = false;
    ularge v = value;
    if (_signed && base == 10 && value < 0)
    {
        v = -value;
        // since we can't handle the lowest signed 64-bit value, we just
        // return a built-in string.
        if ((large)v < 0)   // the LLONG_MIN negated results in the same value
        {
            len = 20;
            return "-9223372036854775808";
        }
        neg = true;
    }

    do
    {
        buf[--i] = pdigits[unsigned(v % base)];
        v /= base;
    } while (v > 0);

    if (neg)
        buf[--i] = '-';

    len = 64 - i;
    return buf + i;
}


static void _itobase2(string& result, large value, int base, int width, char padchar, bool _signed)
{
    result.clear();

    if (base < 2 || base > 64)
        return;

    char buf[65];   // the longest possible string is when base=2
    int reslen;
    char* p = _itobase(value, buf, base, reslen, _signed);

    if (width > reslen)
    {
        if (padchar == 0)
        {
            // default pad char
            if (base == 10)
                padchar = ' ';
            else if (base > 36)
                padchar = '.';
            else
                padchar = '0';
        }

        bool neg = *p == '-';
        if (neg) { p++; reslen--; }
        width -= reslen;
        memset(result.resize(width), padchar, width);
        result.append(p, reslen);
        if (neg)
            result[0] = '-';
    }
    else 
        result.assign(p, reslen);
}


string itostring(large value, int base, int width, char padchar) 
{
    string result;
    _itobase2(result, value, base, width, padchar, true);
    return result;
}


string itostring(ularge value, int base, int width, char padchar) 
{
    string result;
    _itobase2(result, value, base, width, padchar, false);
    return result;
}


string itostring(int value, int base, int width, char padchar) 
{
    string result;
    _itobase2(result, (large)value, base, width, padchar, true);
    return result;
}


string itostring(unsigned value, int base, int width, char padchar) 
{
    string result;
    _itobase2(result, (ularge)value, base, width, padchar, false);
    return result;
}


string itostring(large v)        { return itostring(v, 10, 0, ' '); }
string itostring(ularge v)       { return itostring(v, 10, 0, ' '); }
string itostring(int v)          { return itostring((large)v, 10, 0, ' '); }
string itostring(unsigned v)     { return itostring((ularge)v, 10, 0, ' '); }


ularge stringtou(const char* str, bool* error, bool* overflow, int base)
{
    *error = false;
    *overflow = false;

    if (str == 0 || *str == 0 || base < 2 || base > 64)
        { *error = true; return 0; }

    const char* p = str;
    ularge result = 0;

    do 
    {
        int c = *p++;

        if (c >= 'a')
        {
            // for the numeration bases that use '.', '/', digits and
            // uppercase letters the letter case is insignificant.
            if (base <= 38)
                c -= 'a' - '9' - 1;
            else  // others use both upper and lower case letters
                c -= ('a' - 'Z' - 1) + ('A' - '9' - 1);
        }
        else if (c > 'Z')
            { *error = true; return 0; }
        else if (c >= 'A')
            c -= 'A' - '9' - 1;
        else if (c > '9')
            { *error = true; return 0; }

        c -= (base > 36) ? '.' : '0';
        if (c < 0 || c >= base)
            { *error = true; return 0; }

        ularge t = result * uint(base);
        if (t / base != result)
            { *overflow = true; return 0; }
        result = t;
        t = result + uint(c);
        if (t < result)
            { *overflow = true; return 0; }
        result = t;

    }
    while (*p != 0);

    return result;
}

