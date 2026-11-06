#ifndef COMMON_VECTOR_H
#define COMMON_VECTOR_H

#include "common/types.h"

// Wii: VECTOR<T>. One pointer to the elements; the element count is the int stored
// just before the first element, so an empty vector is a null pointer.
template <class T>
struct VECTOR {
    T *data;

    int size() const { return data ? ((int *)data)[-1] : 0; }
    T &operator[](int index) const { return data[index]; }
};

#endif
