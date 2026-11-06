#ifndef COMMON_ESTRING_H
#define COMMON_ESTRING_H

// Wii: EString. One pointer to the characters; an empty string points at the shared
// EString::m_null. Only what the decompiled code uses is declared so far.
//
// The inline bodies below are the Wii build's, which keeps each one as a small function
// (__ct__7EStringFv, __ct__7EStringFPCc, __ct__7EStringFRC7EString, __dt__7EStringFv).
// The PS2 build inlines them, so only the helpers they call have addresses.
class EString {
public:
    char *data; // 0x00

    EString() { SetToNull(); }
    EString(const char *text) { MakeCopy(text); }
    // Writes the number as text. `return 0;` from a function that returns EString picks
    // this constructor, so it gives the string "0", not an empty one.
    EString(int number)
    {
        SetToNull();
        Convert(number);
    }
    // The copy constructor is why the game returns EString through a hidden pointer: this
    // compiler only does that for a class that has one.
    EString(const EString &other) { MakeCopy(other); }
    ~EString() { Deallocate(data); }

    // Out of line in the PS2 game, at 0x141AF0, although c_str is inlined.
    operator const char *() const;
    const char *c_str() const { return data; }

    void Format(const char *format, ...);
    void MakeLegalSymbolName();
    void Convert(int number);

    void SetToNull();                  // points data at m_null
    void MakeCopy(const char *text);   // allocates a copy; null or empty text gives m_null
    void Deallocate(char *characters); // frees the buffer unless it is m_null
};

#endif
