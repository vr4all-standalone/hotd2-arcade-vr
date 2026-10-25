// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <array>

namespace vr {
// Evaluate loaded media metadata, never a path or emulated RAM. Ordinary Flycast and
// regression builds keep their normal game selection when the option is absent/OFF.
inline const char *standaloneGameError(std::string_view gameId)
{
#if defined(VR_STANDALONE_HOTD2ARCADE) && VR_STANDALONE_HOTD2ARCADE
    if (gameId != "hotd2")
        return "HOTD2 Arcade VR supports the measured USA hotd2.zip cartridge and HOTD2-specific hod2bios.zip BIOS. Dreamcast media and other arcade revisions are unsupported.";
#endif
    return nullptr;
}
// Cartridge gates are separate from the shared boot-title check above. The raw MAME
// cartridge bytes establish the measured revision before any emulated code can run.
inline constexpr std::array<std::uint32_t, 22> standaloneArcadeChipCrcs = {
    0xb23d1a0c, 0xb23d1a0c, 0x88fb0562, 0x5f4dd576, 0x3e62fca4, 0x6f73a852, 0xc7950445, 0x5a812247, 0x17e9414a, 0x5d2d8134, 0xeacaf26d, 0x1e3686be, 0x5ada00a2, 0x9eff6247, 0x8a80b16a, 0x7ae20daf, 0xfbb8641b, 0x3881ec23, 0x66bff6e4, 0x8cd2f654, 0x6cf6e705, 0x495e6265
};

// HOTD2 ignores the usual NAOMI free-play EEPROM setting. Model actual coin
// edges: an initial credit bank, then the game cost after Start is released. Re-filling the counter on
// every poll would repeatedly feed credits to the game's own coin accounting.
struct StandaloneArcadeCoins {
    bool seeded = false, startWas = false;
    template<class Archive> void serialize(Archive& archive) const { archive << seeded << startWas; }
    template<class Archive> void deserialize(Archive& archive) { archive >> seeded >> startWas; }
    int update(int count, bool freePlay, bool start) {
#if defined(VR_STANDALONE_HOTD2ARCADE) && VR_STANDALONE_HOTD2ARCADE
        if (freePlay && (!seeded || (!start && startWas))) {
            if (count <= 0x3ff7) count += seeded ? 4 : 8;
            seeded = true;
        }
#endif
        startWas = start;
        return count;
    }
};

inline const char *standaloneArcadeMediaError(bool archive, bool slave)
{
#if defined(VR_STANDALONE_HOTD2ARCADE) && VR_STANDALONE_HOTD2ARCADE
    if (!archive || slave)
        return standaloneGameError("");
#endif
    return nullptr;
}

inline const char *standaloneArcadeSetError(std::string_view set, bool m2, std::size_t size, unsigned chips)
{
#if defined(VR_STANDALONE_HOTD2ARCADE) && VR_STANDALONE_HOTD2ARCADE
    if (set != "hotd2" || !m2 || size != 0xa800000 || chips != standaloneArcadeChipCrcs.size())
        return standaloneGameError("");
#endif
    return nullptr;
}

inline const char *standaloneArcadeChipMetadataError(unsigned chip, std::size_t offset,
        std::size_t length, std::uint32_t crc, bool normal)
{
#if defined(VR_STANDALONE_HOTD2ARCADE) && VR_STANDALONE_HOTD2ARCADE
    if (chip >= standaloneArcadeChipCrcs.size() || !normal || offset != (chip < 2 ? chip * 0x200000ULL : (chip - 1) * 0x800000ULL)
            || length != (chip < 2 ? 0x200000U : 0x800000U) || crc != standaloneArcadeChipCrcs[chip])
        return standaloneGameError("");
#endif
    return nullptr;
}

inline std::uint32_t standaloneArcadeCrc32(const void *data, std::size_t length)
{
    static const auto table = [] {
        std::array<std::uint32_t, 256> result{};
        for (unsigned n = 0; n < result.size(); ++n) {
            std::uint32_t crc = n;
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
            result[n] = crc;
        }
        return result;
    }();
    const auto *bytes = static_cast<const unsigned char *>(data);
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t n = 0; n < length; ++n)
        crc = table[(crc ^ bytes[n]) & 0xff] ^ (crc >> 8);
    return ~crc;
}

// Shared by initial BIOS selection and subsequent region-driven reloads.
inline const char *standaloneArcadeBiosError(std::string_view bios, std::size_t length,
        std::size_t bytesRead, const void *data, std::uint32_t expectedCrc)
{
#if defined(VR_STANDALONE_HOTD2ARCADE) && VR_STANDALONE_HOTD2ARCADE
    if (bios != "hod2bios" || length != 0x200000 || bytesRead != length || data == nullptr
            || (expectedCrc != 0x065f8500 && expectedCrc != 0x9e3bfa1b && expectedCrc != 0xd99e5b9b)
            || standaloneArcadeCrc32(data, bytesRead) != expectedCrc)
        return "HOTD2 Arcade VR needs a valid 2 MiB HOTD2 BIOS in hod2bios.zip or inside hotd2.zip. An ordinary NAOMI BIOS is not supported.";
#endif
    return nullptr;
}

inline const char *standaloneArcadeChipError(unsigned chip, std::size_t archiveLength,
        std::size_t bytesRead, const void *data)
{
#if defined(VR_STANDALONE_HOTD2ARCADE) && VR_STANDALONE_HOTD2ARCADE
    const std::size_t expectedLength = chip < 2 ? 0x200000U : 0x800000U;
    if (chip >= standaloneArcadeChipCrcs.size() || archiveLength != expectedLength
            || bytesRead != expectedLength || data == nullptr
            || standaloneArcadeCrc32(data, bytesRead) != standaloneArcadeChipCrcs[chip])
        return standaloneGameError("");
#endif
    return nullptr;
}
}