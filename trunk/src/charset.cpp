

#include "charset.h"


static unsigned char lbitmask[8] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};
static unsigned char rbitmask[8] = {0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

const char charsetesc = '~';


void charset::include(int min, int max)
{
    if (uchar(min) > uchar(max))
        return;
    int lidx = uchar(min) / 8;
    int ridx = uchar(max) / 8;
    uchar lbits = lbitmask[uchar(min) % 8];
    uchar rbits = rbitmask[uchar(max) % 8];

    if (lidx == ridx) 
    {
        data[lidx] |= lbits & rbits;
    }
    else 
    {
        data[lidx] |= lbits;
        for (int i = lidx + 1; i < ridx; i++)
            data[i] = -1;
        data[ridx] |= rbits;
    }
}


static unsigned hex4(unsigned c) 
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= '0' && c <= '9')
        return c - '0';
    return 0;    
}


static unsigned parsechar(const char*& p) 
{
    unsigned ret = *p;
    if (ret == unsigned(charsetesc))
    {
        p++;
        ret = *p;
        if ((ret >= '0' && ret <= '9') || (ret >= 'a' && ret <= 'f') || (ret >= 'A' && ret <= 'F'))
        {
            ret = hex4(ret);
            p++;
            if (*p != 0)
                ret = (ret << 4) | hex4(*p);
        }
    }
    return ret;
}


void charset::assign(const char* p) 
{
    if (*p == '*' && *(p + 1) == 0)
        fill();
    else 
    {
        clear();
        for (; *p != 0; p++) {
            uchar left = parsechar(p);
            if (*(p + 1) == '-')
            {
                p += 2;
                uchar right = parsechar(p);
                include(left, right);
            }
            else
                include(left);
        }
    }
}


bool charset::empty() const
{
    for(int i = 0; i < WORDS; i++) 
        if (*((unsigned*)(data) + i) != 0)
            return false;
    return true;
}


void charset::unite(const charset& s) 
{
    for(int i = 0; i < WORDS; i++) 
        *((unsigned*)(data) + i) |= *((unsigned*)(s.data) + i);
}


void charset::subtract(const charset& s) 
{
    for(int i = 0; i < WORDS; i++) 
        *((unsigned*)(data) + i) &= ~(*((unsigned*)(s.data) + i));
}


void charset::intersect(const charset& s) 
{
    for(int i = 0; i < WORDS; i++) 
        *((unsigned*)(data) + i) &= *((unsigned*)(s.data) + i);
}


void charset::invert() 
{
    for(int i = 0; i < WORDS; i++) 
        *((unsigned*)(data) + i) = ~(*((unsigned*)(data) + i));
}


bool charset::le(const charset& s) const 
{
    for (int i = 0; i < WORDS; i++) 
    {
        int w1 = *((unsigned*)(data) + i);
        int w2 = *((unsigned*)(s.data) + i);
        if ((w2 | w1) != w2)
            return false;
    }
    return true;
}
