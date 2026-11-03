// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
namespace vr::reticle {
struct CustomReticleVertex {float x{},y{},layer{};};
using CustomReticleMesh=std::array<CustomReticleVertex,72>;
// Four separate diagonal bars. The nested geometry shares one player color,
// retaining the outer silhouette and empty center without thin contrast bands.
inline CustomReticleMesh custom_reticle_mesh(){
    CustomReticleMesh result;std::size_t at=0;
    constexpr float widths[]={.14f,.10f,.055f},extensions[]={.045f,.025f,0};
    for(unsigned layer=0;layer<3;layer++)for(float sx:{-1.f,1.f})for(float sy:{-1.f,1.f}){
        const float px=-sy*.70710678118f*widths[layer],py=sx*.70710678118f*widths[layer];
        const float start=.35f-extensions[layer],stop=.86f+extensions[layer];
        const std::array<CustomReticleVertex,4> quad={{{sx*start+px,sy*start+py,float(layer)},
            {sx*stop+px,sy*stop+py,float(layer)},{sx*stop-px,sy*stop-py,float(layer)},
            {sx*start-px,sy*start-py,float(layer)}}};
        for(unsigned index:{0u,1u,2u,0u,2u,3u})result[at++]=quad[index];
    }
    return result;
}
}
