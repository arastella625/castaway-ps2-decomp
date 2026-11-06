#ifndef COMMON_EASTL_H
#define COMMON_EASTL_H

// The parts of EA's standard library that the decompiled code uses. The member names are
// EASTL's; the bodies are written to match the code the game's compiler inlined.
inline void *operator new(unsigned int, void *place)
{
    return place;
}

extern "C" void *memcpy(void *to, const void *from, unsigned int size);
extern "C" void *memset(void *to, int value, unsigned int size);

namespace eastl {

class allocator {
public:
    allocator(const char *name = 0) {}
    allocator(const allocator &other) {}

    // The game frees through one out-of-line function that takes no size.
    void deallocate(void *block) { delete[] (char *)block; }
};

template <class InputIterator, class OutputIterator>
inline OutputIterator copy(InputIterator first, InputIterator last, OutputIterator result)
{
    for (; first != last; ++result, ++first)
        *result = *first;
    return result;
}

// Pointers copy as raw memory, through memcpy, even when the length turns out to be zero
// (TreeStack::Reset keeps that call).
template <class T>
inline T **copy(T **first, T **last, T **result)
{
    const unsigned int count = last - first;
    T **const end = (T **)memcpy(result, first, count * sizeof(T *));
    return end + count;
}

template <class T, class Allocator>
class vector {
public:
    T *mpBegin;           // 0x00
    T *mpEnd;             // 0x04
    T *mpCapacity;        // 0x08
    Allocator mAllocator; // 0x0c

    vector(const Allocator &allocator) : mpBegin(0), mpEnd(0), mpCapacity(0), mAllocator(allocator) {}
    ~vector()
    {
        DoDestroyValues(mpBegin, mpEnd);
        if (mpBegin)
            mAllocator.deallocate(mpBegin);
    }

    unsigned int size() const { return mpEnd - mpBegin; }
    T &operator[](unsigned int index) { return mpBegin[index]; }
    T *begin() { return mpBegin; }
    T *end() { return mpEnd; }

    void reserve(unsigned int count);
    void DoInsertValue(T *position, const T &value);

    void pop_back() { --mpEnd; }

    void push_back(const T &value)
    {
        if (mpEnd < mpCapacity)
            new (mpEnd++) T(value);
        else
            DoInsertValue(mpEnd, value);
    }

    // This loop stays in the game's code even for a type with nothing to destroy, because
    // this compiler never removes an empty loop.
    void DoDestroyValues(T *first, T *last)
    {
        for (; first < last; ++first)
            first->~T();
    }

    T *erase(T *first, T *last)
    {
        T *const position = eastl::copy(last, mpEnd, first);
        DoDestroyValues(position, mpEnd);
        mpEnd -= last - first;
        return first;
    }

    void clear() { erase(mpBegin, mpEnd); }
};

}  // namespace eastl

#endif
