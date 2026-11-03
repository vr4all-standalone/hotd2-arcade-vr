// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>

namespace vr::laser {
inline int choice(bool enabled, int savedWidth) {
    if (!enabled) return 0;
    return savedWidth == 3 ? 2 : savedWidth == 4 ? 3 : 1;
}
inline int savedWidth(int choice) {
    return choice == 2 ? 3 : choice == 3 ? 4 : 1;
}
inline int nextChoice(bool enabled, int value, int direction) {
    return std::clamp(choice(enabled, value) + direction, 0, 3);
}
inline const char* label(bool enabled, int value) {
    constexpr const char* labels[] = { "Off", "Normal", "5x", "10x" };
    return labels[choice(enabled, value)];
}
inline constexpr float PointerHalfWidth = .0008f;
inline float halfWidth(int value) {
    const float multiplier = value == 3 ? 5.f : value == 4 ? 10.f : 2.f;
    return PointerHalfWidth * multiplier;
}
}
