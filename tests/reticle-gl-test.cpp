#include <GL/glew.h>
#include <windows.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
static int checks;
static void check(bool ok,const char* message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
namespace config {
// The rendering fixture drives the same numeric storage contract as Option/INI.
int VrReticleMode=1;float VrReticleMinSize=.22f,VrReticleMaxSize=.75f,VrReticleShrinkDistance=6.7f,VrHudDepth=40.f,VrWorldScale=.1f;
bool VrLaser=true;
}
#include "policy.inc"
enum class GunKind {Arcade,Staff};
const char* VertexCompatShader="#version 330 core\n";
const char* PixelCompatShader="#version 330 core\nout vec4 outputColor;\n#define gl_FragColor outputColor\n";
enum {VERTEX_POS_ARRAY,VERTEX_COL_BASE_ARRAY,VERTEX_UV_ARRAY};
struct OpenGlSource {std::string value;OpenGlSource& addSource(const char* s){value+=s;return *this;}std::string generate(){return value;}};
static GLuint gl_CompileAndLink(const char* vs,const char* fs){
 auto shader=[](GLenum kind,const char* s){auto id=glCreateShader(kind);glShaderSource(id,1,&s,nullptr);glCompileShader(id);GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(id,sizeof(log),nullptr,log);std::cerr<<log;std::exit(2);}return id;};
 GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs),p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glBindAttribLocation(p,0,"in_pos");glBindAttribLocation(p,1,"in_base");glBindAttribLocation(p,2,"in_uv");glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);check(ok,"production glow GLSL links");glDeleteShader(v);glDeleteShader(f);return p;
}
static GLuint createVertexArray(){GLuint v;glGenVertexArrays(1,&v);return v;}
static void bindVertexArray(GLuint v){glBindVertexArray(v);}
struct Cache {void UseProgram(GLuint v){glUseProgram(v);}void Enable(GLenum v){glEnable(v);}void Disable(GLenum v){glDisable(v);}void DepthFunc(GLenum v){glDepthFunc(v);}void DepthMask(GLboolean v){glDepthMask(v);}void BlendFunc(GLenum a,GLenum b){glBlendFunc(a,b);}} glcache;
static void logGlErrors(const char*){check(glGetError()==GL_NO_ERROR,"production glow pass has no GL error");}
struct GunView;
static void muzzleNow(const GunView&,glm::vec3&,glm::vec3&,float&);
static bool staffMagic(const GunView&);
struct HandsModel {bool staff;};
static HandsModel* handModel(GunKind kind){static HandsModel model{true};return kind==GunKind::Staff?&model:nullptr;}
namespace theme {const glm::vec3 Accent(.66f,.6f,1.f),Text(.97f,.96f,.94f);}
std::vector<unsigned char> upload;
static void recordedBufferData(GLenum target,GLsizeiptr size,const void* data,GLenum usage){const auto* bytes=static_cast<const unsigned char*>(data);upload.assign(bytes,bytes+size);glBufferData(target,size,data,usage);}
#undef glBufferData
#define glBufferData recordedBufferData
#include "glow.inc"
#undef glBufferData
struct {bool active=false;} serviceControls;
glm::mat4 currentHead(1.f);
static glm::mat4 headPose(){return currentHead;}
#if NINJA_UNIT_POLICY
#include "panel_units.h"
namespace vr {float panelDepth(){return panelDepthInGameUnits(config::VrHudDepth,.01f);}}
#endif
#include "endpoint.inc"
static void muzzleNow(const GunView& gun,glm::vec3& m,glm::vec3& f,float& flash){m=gun.restMuzzle;f=gun.restForward;flash=0;}
static void context(){
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FlycastReticleTest";RegisterClassW(&wc);
 auto w=CreateWindowW(wc.lpszClassName,L"Flycast reticle production raster test",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,wc.hInstance,nullptr);check(w!=nullptr,"hidden test window creates");
 auto dc=GetDC(w);PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof(p);p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=32;p.cDepthBits=24;
 auto format=ChoosePixelFormat(dc,&p);check(format&&SetPixelFormat(dc,format,&p),"OpenGL test pixel format creates");auto rc=wglCreateContext(dc);check(rc&&wglMakeCurrent(dc,rc),"hidden WGL context creates");glewExperimental=GL_TRUE;check(glewInit()==GLEW_OK,"OpenGL entry points load");while(glGetError()!=GL_NO_ERROR){}
}
int main(){
 context();GLuint image,fbo;glGenTextures(1,&image);glBindTexture(GL_TEXTURE_2D,image);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1024,1024,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,image,0);check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"raster framebuffer completes");glViewport(0,0,1024,1024);
 GunView gun;gun.aimDot=true;gun.aimLine=false;gun.aimPoint={0,0,-4};gun.restMuzzle={0,-.5f,0};
 auto vp=glm::perspective(glm::radians(70.f),1.f,.01f,100.f);
 auto render=[&](int mode,glm::vec3 eye=glm::vec3(0),glm::vec3 background=glm::vec3(0)){config::VrReticleMode=mode;glClearColor(background.r,background.g,background.b,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);drawGunGlow(vp*glm::translate(glm::mat4(1),-eye),eye,gun,true);std::vector<unsigned char> pixels(1024*1024*4);glReadPixels(0,0,1024,1024,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
 auto lit=[](const auto& p){int n=0;for(size_t i=0;i<p.size();i+=4)n+=p[i]||p[i+1]||p[i+2];return n;};
 auto pixels=render(1);check(lit(pixels)>8,"Custom reticle draws independently with laser beam disabled");
 config::VrReticleMaxSize=1.5f;auto larger=render(1);check(lit(larger)>lit(pixels)*1.5,"150 percent maximum enlarges actual custom reticle geometry");config::VrReticleMaxSize=.75f;
 bool hollow=true;for(int y=510;y<=513;y++)for(int x=510;x<=513;x++){auto i=(y*1024+x)*4;hollow&=!(pixels[i]||pixels[i+1]||pixels[i+2]);}check(hollow,"Custom reticle keeps a hollow center at the real hit");
 check(lit(render(2))==0,"None suppresses gameplay endpoints");
 gun.player=1;pixels=render(1);unsigned red=0,blue=0;for(size_t i=0;i<pixels.size();i+=4){red+=pixels[i];blue+=pixels[i+2];}check(blue>red*2,"logical P2 custom reticle stays blue");
 unsigned maxRed=0,maxGreen=0;for(size_t i=0;i<pixels.size();i+=4){maxRed=std::max(maxRed,unsigned(pixels[i]));maxGreen=std::max(maxGreen,unsigned(pixels[i+1]));}check(maxRed<=34&&maxGreen<=104,"overlapping custom arms retain exact P2 blue rather than additive cyan");
 auto background=render(2,{},glm::vec3(.8f,.7f,.6f));auto bright=render(1,{},glm::vec3(.8f,.7f,.6f));int solidBlue=0;for(size_t i=0;i<bright.size();i+=4)solidBlue+=bright[i]>=30&&bright[i]<=34&&bright[i+1]>=100&&bright[i+1]<=104&&bright[i+2]==255;check(solidBlue>20,"custom arms keep exact player blue over bright scenery");
 bool scenery=true;for(int y=510;y<=513;y++)for(int x=510;x<=513;x++)for(int c=0;c<3;c++){auto i=(y*1024+x)*4+c;scenery&=bright[i]==background[i];}check(scenery,"hollow center preserves actual bright scene pixels");
 gun.player=0;pixels=render(1);red=blue=0;for(size_t i=0;i<pixels.size();i+=4){red+=pixels[i];blue+=pixels[i+2];}check(red>blue*2,"logical P1 custom reticle stays red after hand transfer");
 gun.menuPointer=true;check(lit(render(2))>8,"None preserves the menu/service endpoint pointer");
 gun.menuPointer=false;gun.servicePointer=true;check(lit(render(2))>8,"None preserves the native service endpoint pointer");gun.servicePointer=false;
 currentHead=glm::rotate(glm::mat4(1),.3f,glm::vec3(0,0,1));gun.reticleHead=currentHead;render(1,{-0.032f,0,0});const auto left=upload;render(1,{.032f,0,0});check(left==upload,"both eye passes upload identical world vertices using shared head orientation");
 gun.reticleHead=glm::mat4(1);gun.reticlePanelDistance=4.f;
 auto width=[](const auto& p){int lo=1024,hi=-1;for(int y=0;y<1024;y++)for(int x=0;x<1024;x++){auto i=(y*1024+x)*4;if(p[i]||p[i+1]||p[i+2]){lo=std::min(lo,x);hi=std::max(hi,x);}}return hi<lo?0:hi-lo+1;};
 const int nearWidth=width(render(1));gun.aimPoint.z=-60;const int farWidth=width(render(1));check(std::abs(farWidth-nearWidth*.22f)<3,"far raster size reaches the22percent perceived floor at the true target depth");
 gun.aimPoint={0,0,-4};gun.aimLine=true;auto noEndpoint=render(2);gun.aimDot=false;check(noEndpoint==render(2),"None removes gameplay endpoint while preserving the independent laser beam");gun.aimLine=false;
 GunView endpoint;config::VrLaser=false;currentHead=glm::mat4(1);updateEndpoint(endpoint,1,true,false,30.f,.1f,{1,2,3},{0,0,-1});check(endpoint.player==1&&endpoint.aimPoint==glm::vec3(1,2,0)&&endpoint.aimDot&&!endpoint.aimLine,"production host keeps logical P2 and actual 3D intersection with beam disabled");
 serviceControls.active=true;updateEndpoint(endpoint,0,true,false,30.f,.1f,{1,2,3},{0,0,-1});check(endpoint.servicePointer&&!endpoint.aimDot,"native service retains previous laser-off endpoint behavior");config::VrLaser=true;updateEndpoint(endpoint,0,true,false,30.f,.1f,{1,2,3},{0,0,-1});check(endpoint.servicePointer&&endpoint.aimDot&&endpoint.aimLine,"native service retains previous laser-on pointers");serviceControls.active=false;
#if MOK_EFFECTS
 gun.menuPointer=false;gun.aimDot=false;gun.kind=GunKind::Staff;gun.shot=1;gun.sinceShot=.04f;gun.restMuzzle={0,0,-1};gun.now=1;check(lit(render(2))>8,"None preserves native MOK staff pulse and golden motes");
#endif
 std::cout<<checks<<" production OpenGL reticle checks passed\n";
}
