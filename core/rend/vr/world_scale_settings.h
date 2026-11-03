/* HOTD2 accepted physical world scale and one-time global migration. GPL v2 or later. */
#pragma once
#include "cfg/cfg.h"
#include <algorithm>
#include <cmath>
namespace vr::world_scale {
inline constexpr float Default=0.10f;
inline float step(float value,int direction) {
    if(!std::isfinite(value))value=Default;
    value=std::clamp(value,0.01f,0.2f);
    const int units=direction>0?int(std::floor(value*100+.0001f))+1:int(std::ceil(value*100-.0001f))-1;
    return std::clamp(units,1,20)/100.f;
}
// The menu and grip control already save globally. Old per-game scale entries
// must not resurrect the previous default after this migration.
template<class OptionType> class Setting : public OptionType {
public:
    Setting():OptionType("vr.WorldScale",Default){}
    using OptionType::operator=;
    void load() override {
        OptionType::load();
        if(config::loadInt("config","vr.WorldScaleDefaultVersion",0)>=3)return;
        this->set(Default);
        config::saveFloat("config","vr.WorldScale",Default);
        config::saveInt("config","vr.WorldScaleDefaultVersion",3);
    }
};
}
