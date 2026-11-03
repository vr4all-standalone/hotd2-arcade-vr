// Actual Option/Settings/INI/menu code is generated into under_test.inc.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#define WARN_LOG(...) ((void)0)
#define stricmp _stricmp
namespace hostfs {class File;}
#include "under_test.inc"
static int checks=0;
static void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}}
static bool near(float a,float b){return std::abs(a-b)<1e-6f;}
static void restart(const std::string& text){
 config::Settings::instance().reset();config::cfgdb=config::IniFile();config::cfgdb.load(text);
 config::persisted=text;config::writeCount=0;config::Settings::instance().load(false);
}
int main(){
 config::Settings::instance().reset();check(near(config::VrWorldScale,.10f),"fresh/reset world default must be0.10");
 for(const char* value:{"0.102","0.105","0.115","0.2","0.005","invalid"}){
  restart(std::string("[config]\nvr.WorldScale=")+value+"\nvr.WorldScaleDefaultVersion=2\nSentinel=73\nvr.GunAngle=17\nROMPath=owned.zip\n[game-a]\nconfig.vr.WorldScale=0.19\nconfig.Sentinel=19\n");
  check(near(config::VrWorldScale,.10f),"legacy world scale must adopt0.10 once");
  check(config::loadInt("config","vr.WorldScaleDefaultVersion",0)==3,"world migration marker must persist");
  check(near(config::loadFloat("config","vr.WorldScale"),.10f),"new world scale must persist immediately");
  check(config::loadInt("config","Sentinel")==73&&config::loadInt("config","vr.GunAngle")==17,"migration must preserve other settings");
  check(config::loadStr("config","ROMPath")=="owned.zip","migration must preserve media paths");
  const auto saved=config::persisted;restart(saved);
  check(config::writeCount==0&&near(config::VrWorldScale,.10f),"restart must not repeat migration");
  config::Settings::instance().setGameId("game-a");config::Settings::instance().load(true);
  check(near(config::VrWorldScale,.10f)&&int(config::Sentinel)==19,"per-game load must preserve global world scale and unrelated game options");
  chooseWorld(1);check(near(config::VrWorldScale,.11f),"0.10 must step directly to0.11");
  chooseWorld(1);check(near(config::VrWorldScale,.12f),"0.11 must step directly to0.12");
  chooseWorld(-1);check(near(config::VrWorldScale,.11f),"reverse menu must use0.01 steps");
  const auto later=config::persisted;restart(later);
  check(near(config::VrWorldScale,.11f)&&config::writeCount==0,"later manual choices must persist without reset");
 }
 restart("[config]\nvr.WorldScale=0.15\nvr.WorldScaleDefaultVersion=4\n");
 check(near(config::VrWorldScale,.15f)&&config::writeCount==0,"future marker must be preserved");
 config::VrWorldScale.set(.01f);chooseWorld(-1);check(near(config::VrWorldScale,.01f),"minimum must clamp");
 config::VrWorldScale.set(.2f);chooseWorld(1);check(near(config::VrWorldScale,.2f),"maximum must clamp");
 config::VrWorldScale.set(.114617f);chooseWorld(-1);check(near(config::VrWorldScale,.110f),"off-grid value must step to exact grid");
 config::VrWorldScale.set(.01f);
 for(int i=2;i<=20;i++){chooseWorld(1);check(near(config::VrWorldScale,i/100.f),"every forward menu stop must be0.01");}
 for(int i=19;i>=1;i--){chooseWorld(-1);check(near(config::VrWorldScale,i/100.f),"every reverse menu stop must be0.01");}
 config::VrWorldScale.set(.102f);chooseWorld(-1);check(near(config::VrWorldScale,.10f),"removed0.102 must step down to0.10");
 config::VrWorldScale.set(.102f);chooseWorld(1);check(near(config::VrWorldScale,.11f),"removed0.102 must step up to0.11");
 std::cout<<checks<<" production world-default/migration/INI/menu checks passed\n";
}
