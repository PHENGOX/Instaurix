#pragma once
#include <cstdint>

class InputState {
public:
    enum Button : uint32_t { Left=1, Right=2, Up=4, Down=8, Quit=16 };
    static constexpr uint32_t Mask=31;
    uint32_t buttons() const { return held; }
    void clear() { held=0; }
    bool key(uint32_t virtualKey,bool down) {
        uint32_t bit=0;
        switch(virtualKey) {
        case 0x25:bit=Left;break; case 0x27:bit=Right;break;
        case 0x26:bit=Up;break; case 0x28:bit=Down;break;
        case 0x1B:bit=Quit;break; default:return false;
        }
        if(down) held|=bit; else held&=~bit;
        return true;
    }
private:
    uint32_t held=0;
};
