#include "standalone_game.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
static int checks;
struct CoinArchive {
 std::vector<bool> data; unsigned at=0;
 CoinArchive& operator<<(bool value){data.push_back(value);return *this;}
 CoinArchive& operator>>(bool& value){value=data.at(at++);return *this;}
};
void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
int main(){
 check(vr::standaloneGameError("hotd2")==nullptr,"verified arcade boot accepted");
 check(vr::standaloneGameError("MK-51002")!=nullptr,"Dreamcast USA rejected");
 check(vr::standaloneGameError("MK-5100250")!=nullptr,"Dreamcast PAL rejected");
 check(vr::standaloneGameError("hotd2o")!=nullptr,"unmeasured arcade revision rejected");
 vr::StandaloneArcadeCoins coins;
 check(coins.update(0,true,false)==8,"initial arcade credits cover both players");
 check(coins.update(0,true,false)==0,"polling never repeats inserted coins");
 check(coins.update(0,true,true)==0,"coin insertion never collides with a Start press");
 check(coins.update(0,true,true)==0,"held Start never repeats inserted coins");
 check(coins.update(0,true,false)==4,"released Start replenishes the two-credit game cost");
 check(coins.update(0,true,true)==0,"later Start leaves the coin line alone");
 vr::StandaloneArcadeCoins paid;
 check(paid.update(0,false,true)==0,"coin-operated mode preserved");
 vr::StandaloneArcadeCoins replay;
 CoinArchive beforeSeed;replay.serialize(beforeSeed);
 check(replay.update(0,true,false)==8,"initial forward seed");
 replay.deserialize(beforeSeed);
 check(replay.update(0,true,false)==8,"rollback replays initial seed exactly");
 replay.update(0,true,true);
 CoinArchive beforeRelease;replay.serialize(beforeRelease);
 check(replay.update(0,true,false)==4,"forward release inserts four coins");
 replay.deserialize(beforeRelease);
 check(replay.update(0,true,false)==4,"rollback replays released Start exactly");
 check(replay.update(0,true,false)==0,"restored edge never repeats");
 check(vr::standaloneArcadeMediaError(false,false)!=nullptr,"disc path rejected");
 check(vr::standaloneArcadeMediaError(true,true)!=nullptr,"slave path rejected");
 check(vr::standaloneArcadeSetError("hotd2",true,0xa800000,22)==nullptr,"22 loader entries accepted");
 check(vr::standaloneArcadeSetError("hotd2",true,0xa800000,21)!=nullptr,"missing mirror rejected");
 const std::uint32_t crc[]={0xb23d1a0c,0xb23d1a0c,0x88fb0562,0x5f4dd576,0x3e62fca4,0x6f73a852,0xc7950445,0x5a812247,0x17e9414a,0x5d2d8134,0xeacaf26d,0x1e3686be,0x5ada00a2,0x9eff6247,0x8a80b16a,0x7ae20daf,0xfbb8641b,0x3881ec23,0x66bff6e4,0x8cd2f654,0x6cf6e705,0x495e6265};
 for(unsigned n=0;n<22;++n){
  const std::size_t offset=n<2?n*0x200000ULL:(n-1)*0x800000ULL,length=n<2?0x200000:0x800000;
  check(vr::standaloneArcadeChipMetadataError(n,offset,length,crc[n],true)==nullptr,"correct loader entry");
  check(vr::standaloneArcadeChipMetadataError(n,offset+1,length,crc[n],true)!=nullptr,"wrong offset rejected");
  check(vr::standaloneArcadeChipMetadataError(n,offset,length,crc[n]^1,true)!=nullptr,"wrong revision CRC rejected");
 }
 const char sample[]="123456789";
 check(vr::standaloneArcadeCrc32(sample,9)==0xcbf43926,"CRC32 standard vector");
 std::vector<unsigned char> wrong(0x200000,0);
 check(vr::standaloneArcadeChipError(0,wrong.size(),wrong.size(),wrong.data())!=nullptr,"correctly sized corrupt chip rejected");
 check(vr::standaloneArcadeBiosError("hod2bios",wrong.size(),wrong.size(),wrong.data(),0x9e3bfa1b)!=nullptr,"corrupt BIOS bytes rejected");
 check(vr::standaloneArcadeBiosError("naomi",wrong.size(),wrong.size(),wrong.data(),0xd4895685)!=nullptr,"ordinary NAOMI BIOS rejected");
 std::printf("PASS HOTD2 arcade policy %d checks\n",checks);
}
