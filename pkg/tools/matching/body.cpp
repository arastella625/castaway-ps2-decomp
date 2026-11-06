// Reconstruction of two SimBodyPart methods, used only to compare compiler output.
#define V4(n) virtual void v##n##a(); virtual void v##n##b(); virtual void v##n##c(); virtual void v##n##d();

struct Elem { char pad[0xc]; unsigned int code; char pad2[8]; };

struct GeomVec {
    Elem *p;
    int size() const { return p ? ((int *)p)[-1] : 0; }
};

struct BitFlags {
    unsigned int v;
    int Get(int i) const;
};

class SimBodyPart {
public:
    unsigned int f0;
    BitFlags flags;
    GeomVec *vec;
    void *typeInfo;
    Elem *cur;
    unsigned int f14;
    unsigned char clothing, modelCode, model, style, texture, degradation;
    unsigned short f1e;

    V4(0) V4(1) V4(2)
    virtual void v12();
    virtual void v13();
    virtual int NoClothing();                 // slot 15 sits at vptr+0x78
    V4(3) V4(4) V4(5) V4(6) V4(7) V4(8) V4(9) V4(10)
    virtual unsigned char DefaultClothing(int a);  // slot 48 sits at vptr+0x180

    void SetClothingSetIndex(unsigned char i);
    int GetNumPossibleClothingSets();
    int IsGeneric();
};

int SimBodyPart::IsGeneric()
{
    if (clothing >= vec->size())
        return 0;
    switch (style) {
    case 1: if (cur->code & 2) return 1; break;
    case 2: if (cur->code & 4) return 1; break;
    case 3: if (cur->code & 8) return 1; break;
    case 4: if (cur->code & 0x10) return 1; break;
    case 5: if (cur->code & 0x20) return 1; break;
    }
    return 0;
}

void SimBodyPart::SetClothingSetIndex(unsigned char i)
{
    if (clothing >= vec->size())
        i = DefaultClothing(flags.Get(0));
    clothing = i;
    cur = &vec->p[clothing];
    flags.v = cur->code;
}

int SimBodyPart::GetNumPossibleClothingSets()
{
    if (NoClothing() == 0x7f)
        return 0;
    return (unsigned char)vec->size();
}
