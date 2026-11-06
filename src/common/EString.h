#ifndef COMMON_ESTRING_H
#define COMMON_ESTRING_H

// Wii: EString. One pointer to the characters. Only what the decompiled code uses is
// declared so far.
class EString {
public:
    char *data; // 0x00

    // Declared because the game returns EString through a hidden pointer, which this
    // compiler only does for a class with a copy constructor. Not decompiled yet.
    EString(const EString &other);
    // The PS2 build inlines the destructor into a call to the function at 0x3DE4A8,
    // passing the string and its characters. The name Release is ours.
    ~EString() { Release(data); }
    const char *c_str() const { return data; }
    void Release(char *characters);
};

#endif
