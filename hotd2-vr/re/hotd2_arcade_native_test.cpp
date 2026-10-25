#include "hotd2_arcade.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using namespace vr::hotd2;
int checks;
void check(bool yes,const char* message){++checks;if(!yes){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
// Independent integer transcription of the arcade's SH4 remapper at 0x06bef0.
int nativeAxis(int raw,int low,int centre,int high,int left,int right){return raw<centre?(raw-centre)*(-left)/(centre-low):(raw-centre)*right/(high-centre);}
int main(){
 std::vector<unsigned char> ram(0x2000000);
 auto r16=[&](unsigned a){std::uint16_t x;std::memcpy(&x,ram.data()+a,2);return x;};
 auto r32=[&](unsigned a){std::uint32_t x;std::memcpy(&x,ram.data()+a,4);return x;};
 auto w16=[&](unsigned a,unsigned x){std::uint16_t n=x;std::memcpy(ram.data()+a,&n,2);};
 auto w32=[&](unsigned a,std::uint32_t x){std::memcpy(ram.data()+a,&x,4);};
 check(!readGun(0,r16,r32).known,"unloaded RAM cannot produce a gun snapshot");
 for(const auto& g:CodeGuards)w16(g.offset,g.word);
 for(const auto& p:InputPatches)w16(p.offset,p.stock);
 for(int p=0;p<2;p++){unsigned b=PlayerBase+p*PlayerStride;w32(b,p);w32(b+4,p?9:5);w32(b+16,p?0:3);w32(b+36,p?0:6);w32(b+52,0);}
 auto g=readGun(0,r16,r32);check(g.known&&g.active&&g.ammo==6&&g.magazine==6&&g.shots==0,"active six-round pistol");
 g=readGun(1,r16,r32);check(g.known&&!g.active,"out player is known but inactive");
 w32(PlayerBase+36,5);w32(PlayerBase+52,1);g=readGun(0,r16,r32);check(g.ammo==5&&g.shots==1,"accepted-shot snapshot");
 w32(PlayerBase+16,0);check(!readGun(0,r16,r32).active,"dead player inactive");w32(PlayerBase+16,3);
 w32(PlayerBase+36,7);check(!readGun(0,r16,r32).known,"impossible magazine rejected");w32(PlayerBase+36,6);
 w32(PlayerBase,1);check(!readGun(0,r16,r32).known,"wrong player block rejected");w32(PlayerBase,0);
 check(!readGun(-1,r16,r32).known&&!readGun(2,r16,r32).known,"invalid player indices rejected");
 w32(ShootingMode,0);check(readScene(r16,r32).known&&readScene(r16,r32).movie,"native story mode");
 w32(ShootingMode,1);check(readScene(r16,r32).known&&!readScene(r16,r32).movie,"native interactive mode");
 w32(ShootingMode,2);check(!readScene(r16,r32).known,"unknown scene mode rejected");
 auto broken=CodeGuards[0];w16(broken.offset,broken.word^1);int writes=0;
 check(!applyInput(r16,[&](unsigned a,unsigned x){++writes;w16(a,x);})&&writes==0,"any unknown code blocks every input write");
 w16(broken.offset,broken.word);
 check(applyInput(r16,w16),"verified input patch applied");
 for(const auto& p:InputPatches)check(r16(p.offset)==p.patched,"15-bit ADC preserved");
 check(applyInput(r16,w16),"input patch idempotent");
 for(int player=0;player<2;player++)for(unsigned i=0;i<6;i++)check(r16(InputBase+player*InputStride+24+i*2)==Calibration[i],"live calibration for both guns");
 for(int x=0;x<640;x++){int raw=(x*65535/639)>>1;int result=nativeAxis(raw,Calibration[0],Calibration[1],Calibration[2],-300,300)+320;check(std::abs(result-x)<=1&&result>=0&&result<640,"whole-width native aim within one pixel");}
 for(int y=0;y<480;y++){int raw=(y*65535/479)>>1;int result=240-nativeAxis(raw,Calibration[3],Calibration[4],Calibration[5],220,-220);check(std::abs(result-y)<=1&&result>=0&&result<480,"whole-height native aim within one pixel");}
 // A reversed native index, unguarded code patch, EEPROM mutation or repeated
 // code-page invalidation must fail these checks. Expectations come from the
 // measured SH4 selector: native 0 selects resource193 (red), 1 resource194 (green).
 for(const auto& guard:BloodColorGuards)w16(guard.offset,guard.word);
 w16(0x0A34AA,0x6430);
 auto beforeBlood=ram;
 writes=0;
 auto bloodWrite=[&](unsigned a,unsigned word){++writes;w16(a,word);};
 check(applyBloodColor(true,r16,bloodWrite),"measured native red blood selector accepted");
 check(r16(0x0A34AA)==0xE400 && writes==1,"red selects native index zero with one code write");
 auto expectedBlood=beforeBlood;expectedBlood[0x0A34AA]=0;expectedBlood[0x0A34AB]=0xE4;
 check(ram==expectedBlood,"red changes only the guarded native selector, no settings or EEPROM bytes");
 check(applyBloodColor(true,r16,bloodWrite)&&writes==1,"red is idempotent without invalidating code every frame");
 check(applyBloodColor(false,r16,bloodWrite)&&r16(0x0A34AA)==0xE401,"green restores native index one");
 writes=0;check(applyBloodColor(false,r16,bloodWrite)&&writes==0,"green is idempotent");
 for(const auto& guard:BloodColorGuards){
  w16(guard.offset,guard.word^1);writes=0;
  check(!applyBloodColor(true,r16,bloodWrite)&&writes==0,"unknown blood code, pointer or resource table blocks writes");
  w16(guard.offset,guard.word);
 }
 w16(0x0A34AA,0xFFFF);writes=0;
 check(!applyBloodColor(true,r16,bloodWrite)&&writes==0,"unknown selector instruction is never overwritten");
 w16(0x0A34AA,0x6430);w16(CodeGuards[0].offset,0);writes=0;
 check(!applyBloodColor(true,r16,bloodWrite)&&writes==0,"different or unloaded arcade revision blocks blood override");
 std::printf("PASS HOTD2 native state/input %d checks\n",checks);
}
