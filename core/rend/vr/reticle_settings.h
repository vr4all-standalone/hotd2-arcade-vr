/* Shared VR4ALL reticle settings; GPL v2 or later. */
#pragma once
#include "cfg/cfg.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <locale>
namespace vr::reticle {
inline float savedFloat(const char* key,float fallback) {
 const std::string text=config::loadStr("config",key,"");
 std::istringstream input(text);input.imbue(std::locale::classic());float value;
 if(!(input>>value)||!std::isfinite(value))return fallback;
 input>>std::ws;return input.eof()?value:fallback;
}
inline int mode(int value) { return value == 2 ? 2 : 1; }
inline int nextMode(int value,int) { return mode(value) == 1 ? 2 : 1; }
inline const char* modeLabel(int value) { return mode(value) == 2 ? "None" : "Custom"; }
inline float minimum(float value) { return std::isfinite(value) ? std::clamp(value,.05f,1.f) : .22f; }
inline float maximum(float value) { return std::isfinite(value) ? std::clamp(value,.25f,3.f) : .40f; }
inline float distance(float value,float fallback=6.5f) { return std::isfinite(value) ? std::clamp(value,.2f,20.f) : std::clamp(fallback,.2f,20.f); }
// Size changes only the artwork offsets. The real world hit never moves.
inline double sizeReference(double target,double panel,float minSize,float shrink,float maxSize=.40f) {
 if(!std::isfinite(panel)||panel<=0)return 0;
 if(!std::isfinite(target)||target<=panel)return panel*maximum(maxSize);
 return target*std::max(double(minimum(minSize)),1./(1.+(target-panel)/distance(shrink)))*maximum(maxSize);
}
template<class OptionType> class ModeSetting : public OptionType {
public:ModeSetting():OptionType("vr.ReticleMode",1){} using OptionType::operator=;
 void load() override {
  OptionType::load();
  std::istringstream input(config::loadStr("config","vr.ReticleMode",""));input.imbue(std::locale::classic());double value=1;
  const bool parsed=bool(input>>value)&&std::isfinite(value);input>>std::ws;
  this->set(parsed&&input.eof()&&value==2.?2:1);
 }
};
template<class OptionType> class MinimumSetting : public OptionType {
public:MinimumSetting():OptionType("vr.ReticleMinSize",.22f){} using OptionType::operator=;
 void load() override {
  OptionType::load();const float value=savedFloat("vr.ReticleMinSize",.22f);
  this->set(value>=.05f&&value<=1.f?value:.22f);
 }
};
template<class OptionType> class MaximumSetting : public OptionType {
public:MaximumSetting():OptionType("vr.ReticleMaxSize",.40f){} using OptionType::operator=;
 void load() override {
  OptionType::load();const float value=savedFloat("vr.ReticleMaxSize",.40f);
  this->set(value>=.25f&&value<=3.f?value:.40f);
 }
};
template<class OptionType> class DistanceSetting : public OptionType {
public:DistanceSetting():OptionType("vr.ReticleShrinkDistance",6.5f){} using OptionType::operator=;
 void load() override {
  OptionType::load();const float value=savedFloat("vr.ReticleShrinkDistance",6.5f);
  // User-calibrated HOTD pair starts at 6.5 m; retain valid explicit tuning.
  this->set(value>=.2f&&value<=20.f?value:6.5f);
 }
};
}
