#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
static int checks;
static void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}}
static bool near(float a,float b){return std::abs(a-b)<1e-5f;}
static void restart(const std::string& text){config::Settings::instance().reset();config::cfgdb=config::IniFile();config::cfgdb.load(text);config::persisted=text;config::writeCount=0;config::Settings::instance().load(false);}
int main(){
 check(HAS_RETICLE_OPTIONS,"production options expose persisted reticle controls");
#if HAS_RETICLE_OPTIONS
 using namespace config;
 restart("[config]\nvr.HudDepth=40\nvr.WorldScale=.11\nvr.WorldScaleDefaultVersion=3\nSentinel=73\nvr.GunAngle=17\n");
 check(int(VrReticleMode)==1&&near(VrReticleMinSize,.22f)&&near(VrReticleShrinkDistance,6.5f),"fresh and missing no-native reticle settings start Custom,22 percent and6.5 m");
 check(near(VrReticleMaxSize,.40f),"fresh reticle maximum starts at40 percent");
 Settings::instance().save();check(near(loadFloat("config","vr.ReticleShrinkDistance"),6.5f),"ordinary settings save persists the6.5 m default");
 check(loadInt("config","Sentinel")==73&&loadInt("config","vr.GunAngle")==17,"default save preserves unrelated settings");
 const auto saved=persisted;restart(saved);check(near(VrReticleShrinkDistance,6.5f)&&writeCount==0,"saved6.5 m default survives restart without migration writes");
 restart("[config]\n");check(near(VrReticleMinSize,.22f)&&near(VrReticleShrinkDistance,6.5f),"empty settings start at22percent and6.5 m for every adapter");
 for(const char* globals:{"vr.HudDepth=.01\nvr.WorldScale=.01\n","vr.HudDepth=100000\nvr.WorldScale=1\n"}){restart(std::string("[config]\n")+globals+"vr.WorldScaleDefaultVersion=3\n");check(near(VrReticleShrinkDistance,6.5f),"missing shrink is6.5 m independently of tiny or huge legacy physical panel");}
 const std::string legacy="[config]\nvr.HudDepth=40\nvr.WorldScale=.11\nvr.WorldScaleDefaultVersion=3\n[game-a]\nconfig.vr.HudDepth=80\nconfig.vr.WorldScale=.2\n";
 restart(legacy);Settings::instance().setGameId("game-a");Settings::instance().load(true);check(near(VrReticleShrinkDistance,6.5f),"legacy per-game physical panels cannot replace the new 6.5 m starting default");
 Settings::instance().save();restart(persisted);check(near(VrReticleShrinkDistance,6.5f),"per-game settings save and restart keep global6.5 m default");
 restart("[config]\n");Settings::instance().setGameId("no-overrides");Settings::instance().load(true);check(near(VrReticleShrinkDistance,6.5f),"a game with no override section also keeps 6.5 m");
 restart(saved);
 check(std::string(Labels[Laser+1])=="Crosshair"&&std::string(Labels[Laser+2])=="Crosshair min size"&&std::string(Labels[Laser+3])=="Crosshair max size"&&std::string(Labels[Laser+4])=="Crosshair shrink distance","actual menu places the three common rows directly after Laser");
 check(reticleValue(ReticleShrinkDistance)=="6.5 m","menu shows the requested6.5 m starting value");
 chooseReticle(Reticle,1);check(int(VrReticleMode)==2&&reticleValue(Reticle)=="None","no-native menu advances Custom to None");
 chooseReticle(Reticle,1);check(int(VrReticleMode)==1&&reticleValue(Reticle)=="Custom","no-native menu wraps directly to Custom");
 chooseReticle(Reticle,-1);check(int(VrReticleMode)==2,"reverse mode selection has only two choices");
 chooseReticle(ReticleMinSize,1);check(near(VrReticleMinSize,.23f)&&reticleValue(ReticleMinSize)=="23%","minimum size changes in percentage point steps");
 chooseReticle(ReticleMaxSize,1);check(near(VrReticleMaxSize,.45f)&&reticleValue(ReticleMaxSize)=="45%","maximum size changes in five-percent steps");
 chooseReticle(ReticleShrinkDistance,1);check(near(VrReticleShrinkDistance,6.6f)&&reticleValue(ReticleShrinkDistance)=="6.6 m","distance changes from6.5 m in tenths of a metre");
 check(stepper(Reticle)&&repeats(ReticleMinSize)&&repeats(ReticleMaxSize)&&repeats(ReticleShrinkDistance),"reticle selection and numeric repeat retain menu interactions");
 const auto edited=persisted;restart(edited);check(int(VrReticleMode)==2&&near(VrReticleMinSize,.23f)&&near(VrReticleShrinkDistance,6.6f),"actual menu edits survive production INI save/load");
 saveFloat("config","vr.HudDepth",100);saveFloat("config","vr.WorldScale",.2f);restart(persisted);check(near(VrReticleShrinkDistance,6.6f),"explicit shrink choice is independent of later world or panel changes");
 Settings::instance().setGameId("game-a");cfgdb.load("[game-a]\nconfig.vr.ReticleMode=1\nconfig.vr.ReticleMinSize=.99\nconfig.vr.ReticleShrinkDistance=19\nconfig.Sentinel=42\n");Settings::instance().load(true);check(int(VrReticleMode)==2&&near(VrReticleMinSize,.23f)&&near(VrReticleShrinkDistance,6.6f),"per-game entries cannot override the explicit global reticle choices");
 for(const char* value:{".2","1.9","6","20"}){restart(std::string("[config]\nvr.ReticleShrinkDistance=")+value+"\nvr.ReticleMode=2\nvr.ReticleMinSize=.37\n[game-a]\nconfig.vr.HudDepth=80\nconfig.vr.WorldScale=.2\n");const float chosen=std::stof(value);Settings::instance().setGameId("game-a");Settings::instance().load(true);check(near(VrReticleShrinkDistance,chosen)&&int(VrReticleMode)==2&&near(VrReticleMinSize,.37f),"valid existing explicit tuned settings persist through game load");}
 for(const char* bad:{"0","3","-1","garbage","2junk","2.5"}){restart(std::string("[config]\nvr.ReticleMode=")+bad+"\nvr.ReticleMinSize=nan\nvr.ReticleShrinkDistance=nan\n");check(int(VrReticleMode)==1&&near(VrReticleMinSize,.22f)&&near(VrReticleShrinkDistance,6.5f),"invalid values fall back to Custom,22percent and6.5 m");}
 for(const char* bad:{"100",".1","-1","6junk","nan"}){restart(std::string("[config]\nvr.ReticleShrinkDistance=")+bad+"\nvr.ReticleMinSize=.01\nvr.HudDepth=80\nvr.WorldScale=.2\n");check(near(VrReticleMinSize,.22f)&&near(VrReticleShrinkDistance,6.5f),"invalid saved ranges and text use22percent and6.5 m rather than panel values");}
 VrReticleMinSize.set(.05f);chooseReticle(ReticleMinSize,-1);check(near(VrReticleMinSize,.05f),"minimum size clamps at5percent");VrReticleMinSize.set(1.f);chooseReticle(ReticleMinSize,1);check(near(VrReticleMinSize,1.f),"minimum size clamps at100percent");
 VrReticleMaxSize.set(.25f);chooseReticle(ReticleMaxSize,-1);check(near(VrReticleMaxSize,.25f),"maximum clamps at25 percent");VrReticleMaxSize.set(3.f);chooseReticle(ReticleMaxSize,1);check(near(VrReticleMaxSize,3.f),"maximum clamps at300 percent");
 VrReticleShrinkDistance.set(.2f);chooseReticle(ReticleShrinkDistance,-1);check(near(VrReticleShrinkDistance,.2f),"distance minimum clamps at0.2m");VrReticleShrinkDistance.set(20.f);chooseReticle(ReticleShrinkDistance,1);check(near(VrReticleShrinkDistance,20.f),"distance maximum clamps at20m");
 check(near(float(vr::reticle::sizeReference(6.,2.,.25f,2.f,1.f)),2.f),"explicit size formula still matches Gunblade at6m");check(near(float(vr::reticle::sizeReference(20.,2.,.25f,2.f,1.f)),5.f),"minimum perceived size clamps far reticles");check(near(float(vr::reticle::sizeReference(1.,2.,.25f,2.f,1.f)),2.f),"near markers preserve their physical panel reference and target");
 check(near(float(vr::reticle::sizeReference(1.,2.,.25f,2.f,1.5f)),3.f),"150 percent maximum enlarges the close-up artwork without moving its target");
 restart("[config]\nvr.ReticleMaxSize=nan\n");check(near(VrReticleMaxSize,.40f),"nonfinite maximum falls back to40 percent");
 for(const char* value:{"0.25","0.75","1","3"}){restart(std::string("[config]\nvr.ReticleMaxSize=")+value+"\n");check(near(VrReticleMaxSize,std::stof(value)),"valid explicit maximum survives new default");Settings::instance().save();restart(persisted);check(near(VrReticleMaxSize,std::stof(value)),"saved explicit maximum survives restart");}
 for(const char* value:{"bad","nan","inf",".24","3.01"}){restart(std::string("[config]\nvr.ReticleMaxSize=")+value+"\n");check(near(VrReticleMaxSize,.40f),"invalid maximum uses40 percent");}
 check(near(float(vr::reticle::sizeReference(1.,2.,.25f,2.)),.8f),"implicit maximum uses captured40 percent");
 check(near(vr::reticle::maximum(NAN),.40f),"rendering invalid maximum uses40 percent");
 for(const char* value:{".05",".22",".25","1"}){restart(std::string("[config]\nvr.ReticleMinSize=")+value+"\n");check(near(VrReticleMinSize,std::stof(value)),"explicit minimum retained after new default");Settings::instance().save();restart(persisted);check(near(VrReticleMinSize,std::stof(value)),"explicit minimum survives restart");}
 for(const char* value:{".2","6","6.7","20"}){restart(std::string("[config]\nvr.ReticleShrinkDistance=")+value+"\n");check(near(VrReticleShrinkDistance,std::stof(value)),"explicit shrink retained after new default");Settings::instance().save();restart(persisted);check(near(VrReticleShrinkDistance,std::stof(value)),"explicit shrink survives restart");}
 std::cout<<checks<<" production reticle Option/INI/menu checks passed\n";
#endif
}
