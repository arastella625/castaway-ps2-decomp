#ifndef GAME_UIDB_H
#define GAME_UIDB_H

// Wii: UIDB, the UI database of named integers (UIDBGetInt__4UIDBFPCc, UIDBSetInt__4UIDBFPCci).
class UIDB {
public:
    static int UIDBGetInt(const char *name);
    static void UIDBSetInt(const char *name, int value); // 0x27BD98
};

#endif
