#ifndef COMMON_EASTL_H
#define COMMON_EASTL_H

// The parts of EA's standard library that the decompiled code uses. The member names are
// EASTL's; the bodies are written to match the code the game's compiler inlined.
inline void *operator new(unsigned int, void *place)
{
    return place;
}

extern "C" void *memcpy(void *to, const void *from, unsigned int size);

// EASTL's debug array new, 0x3C0748: the name, flags, debug flags, file and line of the caller.
void *operator new[](unsigned int size, const char *name, int flags, unsigned int debugFlags,
                     const char *file, int line);
extern "C" void *memset(void *to, int value, unsigned int size);

namespace eastl {

class allocator {
public:
    allocator(const char *name = 0) {}
    allocator(const allocator &other) {}

    // The game frees through one out-of-line function that takes no size.
    void deallocate(void *block) { delete[] (char *)block; }
    // Wii: allocate_memory, which calls this with the size, the alignment and an offset.
    void *allocate(unsigned int size, unsigned int alignment, unsigned int offset);
    // EASTL 1.03.00's allocator::allocate(n, flags), inlined where it's used.
    void *allocate(unsigned int size, int flags = 0)
    {
        return ::new ("EASTL", flags, 0, "../../common/EASTL/1.03.00/include/EASTL/allocator.h", 0xd1) char[size];
    }
};

template <class T, class U>
inline void construct_copy(T *place, const U &value)
{
    new (place) T(value);
}

// EASTL's generic_iterator: a pointer wrapped so the algorithms take it by value.
template <class Iterator>
struct generic_iterator {
    Iterator mIterator;

    generic_iterator(const Iterator &x) : mIterator(x) {}
    Iterator base() const { return mIterator; }
    generic_iterator &operator++()
    {
        ++mIterator;
        return *this;
    }
};

template <class Iterator>
inline bool operator!=(const generic_iterator<Iterator> &a, const generic_iterator<Iterator> &b)
{
    return a.mIterator != b.mIterator;
}

template <class InputIterator, class ForwardIterator>
inline ForwardIterator uninitialized_copy_impl(InputIterator first, InputIterator last, ForwardIterator dest)
{
    ForwardIterator currentDest(dest);
    for (; first != last; ++first, ++currentDest)
        construct_copy(currentDest.base(), *first.base());
    return currentDest;
}

// Wii: uninitialized_copy_ptr<First, Last, Result>, through generic iterators as in EASTL.
template <class First, class Last, class Result>
inline Result uninitialized_copy_ptr(First first, Last last, Result result)
{
    const generic_iterator<Result> i(uninitialized_copy_impl(generic_iterator<First>(first),
                                                             generic_iterator<Last>(last),
                                                             generic_iterator<Result>(result)));
    return i.base();
}

template <class T>
inline T *copy_backward(T *first, T *last, T *result)
{
    while (last != first)
        *--result = *--last;
    return result;
}

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

    vector() : mpBegin(0), mpEnd(0), mpCapacity(0) {}
    vector(const Allocator &allocator) : mpBegin(0), mpEnd(0), mpCapacity(0), mAllocator(allocator) {}
    // Wii: VectorBase(n, allocator), then uninitialized_copy_ptr.
    vector(const vector &other) : mAllocator(other.mAllocator)
    {
        const unsigned int n = other.size();
        mpBegin = DoAllocate(n);
        mpEnd = mpBegin;
        mpCapacity = mpBegin + n;
        mpEnd = uninitialized_copy_ptr(other.mpBegin, other.mpEnd, mpBegin);
    }
    T *DoAllocate(unsigned int n)
    {
        if (n)
            return (T *)mAllocator.allocate(n * sizeof(T), 4, 0);
        return 0;
    }
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
    // Out of line: 0x1D1B80 for vector<RouteGoal>.
    vector &operator=(const vector &other);
    void DoInsertValue(T *position, const T &value);

    void pop_back()
    {
        --mpEnd;
        mpEnd->~T();
    }

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

    // One element: the rest move down through memcpy with a byte count, skipped when it was the
    // last (GenericSimCall 0xB, InventoryItems::RemoveItem). Naming position + 1 once is what
    // matches: written twice, GCC folds the byte count into mpEnd - 4 - position.
    T *erase(T *position)
    {
        T *const next = position + 1;
        if (next < mpEnd)
            memcpy(position, next, (char *)mpEnd - (char *)next);
        --mpEnd;
        return position;
    }

    void clear() { erase(mpBegin, mpEnd); }
};

}  // namespace eastl

#endif
