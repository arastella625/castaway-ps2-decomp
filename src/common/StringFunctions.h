#ifndef COMMON_STRINGFUNCTIONS_H
#define COMMON_STRINGFUNCTIONS_H

// Wii: Sprintf__FPcPCce, from base/core. The return type is a guess.
int Sprintf(char *buffer, const char *format, ...);

// The C library's, at 0x47CA6C.
extern "C" unsigned int strlen(const char *text);

#endif
