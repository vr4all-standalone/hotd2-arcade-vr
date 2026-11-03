"""Compile real HOTD2 Option/INI/menu code; only backing-file I/O is replaced."""
from pathlib import Path
import os,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
BATCH=Path(__file__).resolve().parent
OUT=Path(os.environ.get('HOTD2_WORLD_TEST_OUTPUT','D:/VR4ALL-HOTD-Scale-Build/hotd2/tests'))
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
 policy='#include "rend/vr/world_scale_settings.h"\n' if (ROOT/'core/rend/vr/world_scale_settings.h').exists() else ''
 code='namespace config {\n'+';\n'.join(block(option,s) for s in ('class BaseOption','class Settings','template<typename T, bool PerGameOption = true>'))+';\n'
 code+=line(option,r'^using WorldScaleOption[^\n]+')+'\n'+line(definitions,r'^(?:Option<float>|WorldScaleOption) VrWorldScale[^\n]+')+'\nOption<int> Sentinel("Sentinel",0);\n}'
 menu=source('core/rend/vr/xr_menu.cpp');start=menu.index('case WorldSize:',menu.index('void change('));end=menu.index('case PanelDistance:',start)
 choose='void chooseWorld(int dir){switch(0){case 0:'+menu[start+len('case WorldSize:'):end]+'}}\n'
 (OUT/'under_test.inc').write_text('\n'.join((trim,ini_header,ini,'#include "cfg/cfg.h"',cfg,policy,code,choose)))
 env=dict(os.environ);env['PATH']='D:/msys64/mingw64/bin;'+env.get('PATH','')
 binary=OUT/'world-scale-test.exe'
 subprocess.run(['D:/msys64/mingw64/bin/g++.exe','-std=c++17','-O2','-I'+str(OUT),'-I'+str(ROOT/'core'),str(BATCH/'world-scale-test.cpp'),'-o',str(binary)],check=True,env=env)
 return subprocess.run([str(binary)],env=env).returncode
if __name__=='__main__':sys.exit(main())
