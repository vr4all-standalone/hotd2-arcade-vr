/* VR4ALL isolated native gameplay UI sizing; GPL v2 or later. */
#pragma once
#include "cfg/cfg.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <locale>
namespace vr::hud {
inline float size(float value) { return std::isfinite(value) ? std::clamp(value,.5f,2.f) : 1.f; }
inline float step(float value,int dir) { return std::clamp(std::round(size(value)*100.f)+dir*5.f,50.f,200.f)/100.f; }
inline float effective(float saved,bool activeLarge,bool playing,bool service,bool native3D) {
 return activeLarge&&playing&&!service&&native3D?size(saved):1.f;
}
template<class OptionType> class SizeSetting : public OptionType {
public:SizeSetting():OptionType("vr.HudSize",1.f){} using OptionType::operator=;
 void load() override {
  OptionType::load();
  std::istringstream input(config::loadStr("config","vr.HudSize",""));input.imbue(std::locale::classic());float value=1.f;
  const bool valid=bool(input>>value)&&std::isfinite(value)&&value>=.5f&&value<=2.f;input>>std::ws;
  this->set(valid&&input.eof()?value:1.f);
 }
};
// Conservative evidence from this retained frame only. Flat UI, invalid vertices and
// the native overlay plane never establish gameplay geometry; no previous-frame latch.
template<class Context> bool native3DFrame(const Context& ctx,float hudW) {
 if(ctx.isRTT)return false;
 auto list=[&](const auto& polys,unsigned first) {
  for(unsigned n=first;n<polys.size();++n) {
   const auto& p=polys[n];if(p.count<3||p.first+p.count>ctx.verts.size())continue;
   float lo=1e30f,hi=0.f;bool valid=true,overlay=true;
   for(unsigned i=p.first;i<p.first+p.count;++i) {
    const auto& v=ctx.verts[i];
    if(!(v.z>0.f)||!std::isfinite(v.z)||!std::isfinite(v.x)||!std::isfinite(v.y)){valid=false;break;}
    const float w=1.f/v.z;lo=std::min(lo,w);hi=std::max(hi,w);overlay=overlay&&std::abs(w-hudW)<.002f;
   }
   if(valid&&!overlay&&hi-lo>hi*1e-4f)return true;
  }return false;
 };
 return list(ctx.global_param_op,1)||list(ctx.global_param_pt,0)||list(ctx.global_param_tr,0);
}
}
