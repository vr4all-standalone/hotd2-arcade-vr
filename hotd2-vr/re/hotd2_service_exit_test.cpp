#include "hotd2_arcade.h"
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
using namespace vr::hotd2;
static int checks;
static void check(bool yes,const char* message) { ++checks; if(!yes) {std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);} }
int main() {
    std::vector<unsigned char> ram(0x2000000);
    auto read16=[&](unsigned a) {std::uint16_t x;std::memcpy(&x,ram.data()+a,2);return x;};
    auto read32=[&](unsigned a) {std::uint32_t x;std::memcpy(&x,ram.data()+a,4);return x;};
    auto write16=[&](unsigned a,unsigned x) {std::uint16_t v=x;std::memcpy(ram.data()+a,&v,2);};
    auto write32=[&](unsigned a,std::uint32_t x) {std::memcpy(ram.data()+a,&x,4);};
    for(const auto& guard:CodeGuards)write16(guard.offset,guard.word);
    for(const auto& patch:InputPatches)write16(patch.offset,patch.stock);
    // Measured USA cartridge dispatcher: mov.l @r2,r0, with r2=0c3d07b8.
    write16(0x020078,0xD23D);write16(0x02007A,0x6022);
    write16(0x020170,0x07B8);write16(0x020172,0x0C3D);
    ServiceExitObserver observer;
    // Real System/Game Test entry/exit trace from the owned cartridge. Each submenu
    // re-enters native dispatch state 2 and returns to 1; only System EXIT uses 3.
    for(unsigned state:{0u,1u,0u,2u,1u,0u,2u,1u,2u,1u}) {
        write32(0x3D07B8,state);
        check(!observer.poll(read16,read32),"native test/submenu navigation keeps Service controls");
    }
    write32(0x3D07B8,3);
    check(observer.poll(read16,read32),"actual native System EXIT reports a game restart");
    check(!observer.poll(read16,read32),"held restart state reports once");
    write16(0x02007A,0);
    check(!observer.poll(read16,read32),"unknown dispatcher cannot report a restart");
    write16(0x02007A,0x6022);
    check(!observer.poll(read16,read32),"temporary invalid snapshot does not duplicate a restart");
    write32(0x3D07B8,0xffffffff);
    check(!observer.poll(read16,read32),"unknown native state cannot report or rearm a restart");
    write32(0x3D07B8,3);
    check(!observer.poll(read16,read32),"unknown native state does not duplicate the previous restart");
    write32(0x3D07B8,0);check(!observer.poll(read16,read32),"native startup rearms the observer");
    write32(0x3D07B8,3);check(observer.poll(read16,read32),"a later native System EXIT reports again");
    observer.reset();
    write16(CodeGuards[0].offset,0);
    check(!observer.poll(read16,read32),"different or unloaded ROM blocks a restart");
    write16(CodeGuards[0].offset,CodeGuards[0].word);
    check(observer.poll(read16,read32),"new emulator session can observe an in-progress native restart");
    std::printf("PASS HOTD2 native Service exit %d checks\n",checks);
}
