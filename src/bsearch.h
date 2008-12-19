#ifndef __BSEARCH_H
#define __BSEARCH_H

//
// His Majesty, Binary Search. Normally generates around 100 bytes of code
// instead of tons and tons of mess you get with STL, specifically with map
// and friends.
//


template <class Container, class T>
bool bsearch(const Container& cont, const T& t, int count, int& idx) {
    int l, h, i, c;
    l = 0;
    h = count - 1;
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
