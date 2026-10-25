// SPDX-License-Identifier: GPL-2.0-or-later
// Measured HOTD2 USA arcade cartridge (b23d1a0c); never Dreamcast offsets.
#pragma once
#include <cstdint>
namespace vr::hotd2 {
struct Guard { unsigned offset, word; };
struct Patch { unsigned offset, stock, patched; };
inline constexpr unsigned PlayerBase=0x3D0420, PlayerStride=0x100;
inline constexpr unsigned InputBase=0x3D17CC, InputStride=36, ShootingMode=0x3D0648;
// The native mapper targets X [-300,0,300], Y [220,0,-220]. These live
// calibration values map 15-bit cabinet input over the full 640x480 frame,
// within one pixel including its integer division. The persistent EEPROM is untouched.
inline constexpr std::uint16_t Calibration[]={1024,16384,31791,1365,16384,31464};
inline constexpr Guard CodeGuards[]={
    { 0x096C10, 0x51E9 },
    { 0x096C12, 0x71FF },
    { 0x096C14, 0x1E19 },
    { 0x096C34, 0x53ED },
    { 0x096C36, 0x7301 },
    { 0x096C38, 0x1E3D },
    { 0x025090, 0xD022 },
    { 0x025094, 0x8805 },
    { 0x025098, 0xD021 },
    { 0x02509C, 0x8805 },
    { 0x02511C, 0x0420 },
    { 0x02511E, 0x0C3D },
    { 0x096CBC, 0x0420 },
    { 0x096CBE, 0x0C3D },
    { 0x096CC4, 0x0648 },
    { 0x096CC6, 0x0C3D },
    { 0x096C24, 0x2228 },
    { 0x06BCE0, 0x17CC },
    { 0x06BCE2, 0x0C3D },
    { 0x06BEF6, 0x6351 },
    { 0x06BF46, 0x3478 },
    { 0x06BF50, 0x0437 },
    { 0x06BF76, 0x6703 },
    { 0x06BF84, 0x0417 },
    { 0x0BA54C, 0x30E4 },
    { 0x0BA54E, 0x0C9C },
    { 0x0FB020, 0xFED4 },
    { 0x0FB022, 0x0000 },
    { 0x0FB024, 0x012C },
    { 0x0FB026, 0x00DC },
    { 0x0FB028, 0x0000 },
    { 0x0FB02A, 0xFF24 }
};
// Keep fifteen bits of each gun's JVS ADC instead of discarding the low eight.
// Signed native calibration arithmetic still fits in a 16-bit positive word.
inline constexpr Patch InputPatches[]={
    { 0x0BA4C2, 0x4119, 0x4101 },
    { 0x0BA4CC, 0x4019, 0x4001 },
    { 0x0BA4D4, 0x4019, 0x4001 },
    { 0x0BA4DE, 0x4019, 0x4001 }
};
template<class Read16> bool codeValid(Read16 read16) {
    for(const auto& g:CodeGuards) if(read16(g.offset)!=g.word) return false;
    for(const auto& p:InputPatches) {const unsigned w=read16(p.offset);if(w!=p.stock&&w!=p.patched)return false;}
    return true;
}
template<class Read16,class Write16> bool applyInput(Read16 read16,Write16 write16) {
    if(!codeValid(read16))return false;
    for(const auto& p:InputPatches)if(read16(p.offset)!=p.patched)write16(p.offset,p.patched);
    for(unsigned player=0;player<2;player++)for(unsigned i=0;i<6;i++) {
        const unsigned a=InputBase+player*InputStride+24+i*2;
        if(read16(a)!=Calibration[i])write16(a,Calibration[i]);
    }
    // Native gun mode 2 reads the real JVS ADC and offscreen sensor. Preserve its neighbour.
    const unsigned mode=read16(0x3D10A2);
    if((mode&255)!=2)write16(0x3D10A2,(mode&0xff00)|2);
    return true;
}
struct Gun { bool known=false,active=false;std::uint8_t ammo=0,magazine=6;std::uint32_t shots=0; };
template<class Read16,class Read32> Gun readGun(int player,Read16 read16,Read32 read32) {
    Gun g;
    if(player<0||player>1||!codeValid(read16))return g;
    const unsigned b=PlayerBase+unsigned(player)*PlayerStride;
    const auto phase=read32(b+4),life=read32(b+16),ammo=read32(b+36);
    if(read32(b)!=unsigned(player)||phase>16||life>99||ammo>6)return g;
    g.known=true;g.active=phase==5&&life>0;g.ammo=std::uint8_t(ammo);g.shots=read32(b+52);
    return g;
}
struct Scene {bool known=false,movie=false;};
template<class Read16,class Read32> Scene readScene(Read16 read16,Read32 read32) {
    if(!codeValid(read16))return {};
    const unsigned shooting=read32(ShootingMode);
    return shooting<=1?Scene{true,shooting==0}:Scene{};
}
}
