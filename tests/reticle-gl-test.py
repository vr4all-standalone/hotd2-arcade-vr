"""Run the actual Flycast glow shader/GL pass in a hidden WGL context.

Only model lookup and shader source plumbing are supplied by the fixture; reticle
geometry, draw policy, GL uploads and rasterization are production code.
"""
from pathlib import Path
import os, re, subprocess, sys
ROOT=Path(sys.argv[1]); OUT=Path(sys.argv[2]); HERE=Path(__file__).parent
def source(name): return (ROOT/name).read_text(encoding='utf-8-sig')
def block(text,signature):
 start=text.index(signature); begin=text.index('{',start); end=begin+1; depth=1
 while depth: depth+=(text[end]=='{')-(text[end]=='}'); end+=1
 return text[start:end]
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 g=source('core/rend/vr/xr_gun.cpp'); h=source('core/rend/vr/xr_gun.h')
 types=re.search(r'GLuint glowVbo, glowVao;',g)[0]+'\n'+g[g.index('struct GlowVertex'):g.index('// A cheap deterministic random number')]
 funcs='\n'.join(block(g,s) for s in ('float shotRandom(', 'bool initGlow()', 'void drawGlow(', 'void billboard(', 'void flame(', 'void drawGlowShape('))
 # A future production custom geometry function is included as a whole.
 for sig in ('void customReticle(',):
  if sig in g: funcs+='\n'+block(g,sig)
 if 'static void addStaffMagic(' in g:
  funcs+='\n'+block(g,'static bool staffMagic(')+'\n'+block(g,'static void addStaffMagic(')
 funcs+='\n'+block(g,'void drawGunGlow(')
 (OUT/'glow.inc').write_text(block(h,'struct GunView')+';\n'+types+'\n'+funcs)
 host=source('core/rend/vr/xr_host.cpp');start=host.index('view.player = g.player;');end=host.index(';',host.index('view.aimPoint =',start))+1
 (OUT/'endpoint.inc').write_text('void updateEndpoint(GunView& view,int player,bool onScreen,bool aimOutside,float dist,float s,glm::vec3 o,glm::vec3 d){struct {int player;}g{player};'+host[start:end]+'}\n')
 includes=''
 for name in ('reticle_settings.h','custom_reticle.h'):
  if (ROOT/'core/rend/vr'/name).exists(): includes+='#include "'+name+'"\n'
 (OUT/'policy.inc').write_text(includes+'\n#define MOK_EFFECTS '+str(int('static void addStaffMagic(' in g))+'\n#define NINJA_UNIT_POLICY '+str(int(ROOT.name=='ninja-assault-vr'))+'\n')
 env=dict(os.environ); env['PATH']='D:/msys64/mingw64/bin;'+env.get('PATH','')
 binary=OUT/'reticle-gl-test.exe'
 cmd=['D:/msys64/mingw64/bin/g++.exe','-std=c++17','-O2','-I'+str(OUT),'-I'+str(ROOT/'core'),'-I'+str(ROOT/'core/deps/glm'),'-I'+str(ROOT/'core/rend/vr'),str(HERE/'reticle-gl-test.cpp'),'-o',str(binary),'-lglew32','-lopengl32','-lgdi32','-luser32']
 subprocess.run(cmd,env=env,check=True)
 return subprocess.run([str(binary)],env=env).returncode
if __name__=='__main__': sys.exit(main())
