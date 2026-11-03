"""Compile actual Option/Settings/INI and reticle menu case code for an adapter."""
from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(sys.argv[1]);OUT=Path(sys.argv[2]);HERE=Path(__file__).parent
def source(name):return (ROOT/name).read_text(encoding='utf-8-sig')
def block(text,signature):
 start=text.index(signature);begin=text.index('{',start);end=begin+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
def line(text,pattern):
 match=re.search(pattern,text,re.M);return match[0] if match else ''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 option,definitions=source('core/cfg/option.h'),source('core/cfg/option.cpp')
 ini_header=source('core/cfg/ini.h').replace('#include "oslib/storage.h"','').replace('#pragma once','')
 ini=re.sub(r'^#include[^\n]*\n','',source('core/cfg/ini.cpp'),flags=re.M)
 for sig in ('void IniFile::load(hostfs::File *file','void IniFile::save(hostfs::File *file'):ini=ini.replace(block(ini,sig),'')
 trim=line(source('core/stdclass.cpp'),r'^const std::string defaultWs[^\n]+')+'\n'+block(source('core/stdclass.h'),'static inline std::string trim_ws(')
 cfg='namespace config {static IniFile cfgdb;static bool autoSaving=true;std::string persisted;int writeCount=0;void saveFile(){cfgdb.save(persisted);++writeCount;}void autoSave(){if(autoSaving)saveFile();}\n'
 for sig in ('void saveStr(','std::string loadStr(','void saveInt(','int loadInt(','void saveBool(','bool loadBool(','void setTransient(','bool isTransient(','bool hasSection(','void deleteSection(','void deleteEntry(','void setAutoSave(','void saveFloat(','float loadFloat(','std::vector<std::string> getEntries('):cfg+=block(source('core/cfg/cfg.cpp'),sig)+'\n'
 cfg+='}'
 has='VrReticleMode' in definitions
 code='#define HAS_RETICLE_OPTIONS '+str(int(has))+'\n'
 code+='namespace config {\n'+';\n'.join(block(option,s) for s in ('class BaseOption','class Settings','template<typename T, bool PerGameOption = true>'))+';\n'
 if 'using WorldScaleOption' in option:code+=line(option,r'^using WorldScaleOption[^\n]+')+'\n'
 for name in ('VrHudDepth','VrWorldScale'):code+=line(definitions,r'^\w+(?:<float>)? '+name+r'[^\n]+')+'\n'
 for name in ('ReticleModeOption','ReticleMinSizeOption','ReticleMaxSizeOption','ReticleDistanceOption'):
  code+=line(option,r'^using '+name+r'[^\n]+')+'\n'
 for name in ('VrReticleMode','VrReticleMinSize','VrReticleMaxSize','VrReticleShrinkDistance'):
  code+=line(definitions,r'^\w+ '+name+r'[^\n]+')+'\n'
 code+='Option<int> Sentinel("Sentinel",0);\n'
 code+='\n}'
 menu=source('core/rend/vr/xr_menu.cpp')
 enum=line(source('core/rend/vr/xr_menu.h'),r'^enum Row[^\n]+')
 labels=line(menu,r'^const char \*const Labels[^\n]+')
 # Labels span two physical lines in these adapters.
 at=menu.index('const char *const Labels');labels=menu[at:menu.index(';',at)+1]
 choose=''
 if has:
  start=menu.index('case Reticle:',menu.index('void change('));end=menu.index('case Haptics:',start)
  choose='void chooseReticle(int row,int dir){switch(row){'+menu[start:end]+'default:break;}}\n'
  start=menu.index('case Reticle:',menu.index('std::string value('));end=menu.index('case Haptics:',start)
  choose+='std::string reticleValue(int row){char s[32];switch(row){'+menu[start:end]+'default:return "";}}\n'
  choose+=block(menu,'bool stepper(')+'\n'+block(menu,'bool repeats(')
 policy=('#include "world_scale_settings.h"\n' if (ROOT/'core/rend/vr/world_scale_settings.h').exists() else '')+('#include "reticle_settings.h"\n' if has else '')
 (OUT/'under_test.inc').write_text('\n'.join((trim,ini_header,ini,'#include "cfg/cfg.h"',cfg,policy,code,enum,labels,line(menu,r'^enum ExtraRow[^\n]+'),choose)))
 env=dict(os.environ);env['PATH']='D:/msys64/mingw64/bin;'+env.get('PATH','')
 binary=OUT/'reticle-settings-test.exe'
 subprocess.run(['D:/msys64/mingw64/bin/g++.exe','-std=c++17','-O2','-I'+str(OUT),'-I'+str(ROOT/'core'),'-I'+str(ROOT/'core/rend/vr'),str(HERE/'reticle-settings-test.cpp'),'-o',str(binary)],check=True,env=env)
 return subprocess.run([str(binary)],env=env).returncode
if __name__=='__main__':sys.exit(main())
