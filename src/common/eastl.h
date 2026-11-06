#ifndef COMMON_EASTL_H
#define COMMON_EASTL_H

// The parts of EA's standard library that the decompiled code uses. The member names are
// EASTL's; the bodies are written to match the code the game's compiler inlined.
namespace eastl {

class allocator {
};

template <class InputIterator, class OutputIterator>
inline OutputIterator copy(InputIterator first, InputIterator last, OutputIterator result)
{
    for (; first != last; ++result, ++first)
        *result = *first;
    return result;
}

template <class T, class Allocator>
class vector {
public:
    T *mpBegin;    // 0x00
    T *mpEnd;      // 0x04
    T *mpCapacity; // 0x08

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
