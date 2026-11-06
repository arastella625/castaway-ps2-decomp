#ifndef COMMON_STRINGFUNCTIONS_H
#define COMMON_STRINGFUNCTIONS_H

// Wii: Sprintf__FPcPCce, from base/core. The return type is a guess.
int Sprintf(char *buffer, const char *format, ...);

// The C library's, at 0x47CA6C and 0x47CD78.
extern "C" unsigned int strlen(const char *text);
extern "C" char *strncpy(char *to, const char *from, unsigned int count);

#endif
