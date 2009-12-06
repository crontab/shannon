#ifndef __BSEARCH_H
#define __BSEARCH_H


template <class Container, class T>
bool bsearch(const Container& cont, const T& t, int& idx) {
    int l, h, i, c;
    l = 0;
    h = cont.size() - 1;
    bool ret = false;
    while (l <= h) 
    {
        i = (l + h) / 2;
        c = cont.compare(i, t);
        if (c < 0)
            l = i + 1;
        else
        {
            h = i - 1;
            if (c == 0)
                ret = true;
        }
    }
    idx = l;
    return ret;
}


#endif
