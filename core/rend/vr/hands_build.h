/*
	Making the agent's hands and pistol (hands.bin) from one render pass of the game itself
	(hotd2-vr): the C++ twin of hotd2-vr/assets/rip_hands.py, so the headset makes them from
	the player's own copy of the game, with no PC and nothing of the game in the app. The
	same for the hero's staff in The Maze of the Kings (staff-mok.bin), in the same format,
	and Confidential Mission's guns (gun-cm-*.bin), gun only.

	Standalone (glm, the standard library and platform cache-file replacement), so it can be checked against the Python
	tool on the PC with the same rip.

	Copyright 2026 mikermak. This file is part of Flycast and is distributed under the GNU GPL v2 or later.
*/
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vr::hands
{

// The game over scene: the textures (VRAM addresses) of the agent's parts, per version of
// the game (vr_reproject.cpp's profiles). The pistol is in his right hand, the left one is open.
struct Parts
{
	uint32_t hands[2];
	uint32_t cuff;
	uint32_t gun;
	bool has(uint32_t tex) const { return tex != 0 && (tex == hands[0] || tex == hands[1] || tex == cuff || tex == gun); }
};

// The hero's staff in The Maze of the Kings: the textures (VRAM addresses) of the rod and of
// the hero's body, which his gloves are part of, per scene he is seen in (the attract demo,
// the story's intro). He holds the rod in his right fist.
struct StaffParts
{
	struct Scene { uint32_t rod, body; } scenes[2];
	bool has(uint32_t tex) const {
		for (const Scene& s : scenes)
			if (tex != 0 && (tex == s.rod || tex == s.body))
				return true;
		return false;
	}
	bool isRod(uint32_t tex) const { return tex != 0 && (tex == scenes[0].rod || tex == scenes[1].rod); }
};
// A whole rod: its head piece (42 polygons) and its shaft (24)
constexpr uint32_t StaffRodPolys = 66;

// A gun the game draws by itself, with no hand on it (Confidential Mission's: the agents' pistol
// in the stage 1 intro, the assault rifle in the attract demo), per game's profile
// (vr_reproject.cpp), made into a gun-only model.
struct GunTarget
{
	const char *name;			// "agents' pistol"
	const char *file;			// "gun-cm-pistol.bin", in the data directory
	uint32_t textures[4];		// its textures (VRAM addresses); 0: unused
	uint16_t wholePolys;		// its polygons when all of it is there
	uint16_t minPolys;			// a capture needs at least this many
	float lengthM;				// the built model's length along the barrel, metres
	bool merge;					// merge captures across frames, in the gun's own space (buildGun)
	// Merged, all of it is there (and so flagged) when its polygons number wholePolys, so many
	// of each of its textures (texturePolys, as textures[]); its body (textures[0]) has at most
	// maxOpenEdges edges with one polygon on them; and the body's extents along its own axes,
	// longest first, are within extentsOff of the ratio extents. 0: not checked.
	uint16_t texturePolys[4];
	uint16_t maxOpenEdges;
	float extents[3];
	float extentsOff;
	// Caught in the attract demo with no presses from boot, by this many seconds of game (0: not):
	// the run's first part while its file is missing, before Start leaves the demo. Its file
	// missing alone starts no run, and it's watched for in play as well (hands_rip.cpp).
	float attractBy;
	// Optional: select one spatially joined instance when a pass draws several copies.
	// Distance between corners in game units; 0 keeps the existing whole-pass selection.
	float instanceJoinDistance = 0.f;
	// One model split between opaque and translucent lists (HOTD2 pistol: 28 + 2).
	bool acrossLists = false;

	bool used() const { return file != nullptr && file[0] != 0; }
	bool has(uint32_t tex) const {
		for (uint32_t t : textures)
			if (t != 0 && t == tex)
				return true;
		return false;
	}
};

// What buildGun made of the captures, for the log.
struct GunReport
{
	uint32_t captures = 0;		// given
	uint32_t reference = 0;		// the one the others were fitted onto (the most of the body)...
	uint32_t referencePolys = 0;	// ...and its polygons
	uint32_t merged = 0;		// fitted (that one too)
	uint32_t dropped = 0;		// that didn't fit: under 8 corners shared, or rms over 1 % of the body's length
	double rms = 0.0;			// the worst fit kept, of the body's length
	uint32_t polys = 0, texturePolys[4] {};	// the model's polygons, and by texture
	uint32_t openEdges = 0;		// the body's
	double extents[3] {};		// the body's along its own axes, longest first, that one 1
	// How its front and back were told apart (hands_build.cpp gunAxes), of its length: the mean of
	// its lowest fifth's corners behind the corners' middle (the grip's bottom is behind it); its
	// front's and back's heights (the corners within a tenth of the length of each end). Under 0.05
	// behind, the thinner end is the muzzle instead (byEnds).
	double lowBehind = 0.0, endHeights[2] {};
	bool byEnds = false;
	std::string incomplete;		// the first check that failed; empty: all of it is there
};

// HND1 version 2's flags
enum ModelFlags : uint32_t { ModelGunOnly = 1, ModelComplete = 2 };

// One polygon (a triangle strip) of a render pass, as the TA got it: vertices in framebuffer
// pixels, z = 1/W.
struct RipVertex
{
	float x, y, z, u, v;
	uint8_t col[4];
};
struct RipPoly
{
	uint32_t isp, tsp, tcw, pcw;
	std::vector<RipVertex> v;
	// VRAM address of the texture, 0 for none
	uint32_t texture() const { return (pcw >> 3) & 1 ? (tcw & 0x1FFFFF) << 3 : 0; }
};
struct Rip
{
	uint32_t fbWidth, fbHeight;
	float focalX, focalY;		// the game's focal length, framebuffer pixels
	std::vector<RipPoly> polys;
};
struct Image
{
	uint32_t width = 0, height = 0;
	std::vector<uint8_t> rgba;
};

// The top mip level of a PowerVR texture, from VRAM (linear, as Flycast keeps it) and the
// palette RAM. False for formats not handled (YUV, bump, planar VQ) or out of VRAM.
bool decodeTexture(const uint8_t *vram, size_t vramSize, const uint32_t *palette, uint32_t palCtrl,
		uint32_t tcw, uint32_t tsp, Image& image);

// Lupin's generated gun cache, bounded before allocation and checked against this target.
// Only complete HND1 v2 gun-only payloads are reusable; .part is never a fallback input.
constexpr size_t MaxGunCacheBytes = 16 * 1024 * 1024;
bool validateGunCache(const std::vector<uint8_t>& data, const GunTarget& target);
bool loadGunCache(const std::string& path, const GunTarget& target);
bool saveGunCache(const std::vector<uint8_t>& data, const std::string& path, const GunTarget& target);

// Where the textures come from: tcw and the tsp's size bits.
using TextureSource = std::function<bool(uint32_t tcw, uint32_t tsp, Image& image)>;

// The pistol, the hand around its grip and the open hand, as the bytes of hands.bin (the
// format is in xr_hands.cpp). False with a reason when the pass doesn't have them.
bool build(const Rip& rip, const Parts& parts, const TextureSource& textures, std::vector<uint8_t>& out, std::string& error);

// The hero's staff, his right fist around it and his left glove as the open hand, in the
// same format: the rod is the frame, there is no slide (it travels 0), the muzzle is the tip
// of its head. Shortened (the default), the shaft behind the fist is cut down to a hand's
// width and its tail piece put back on there: about 1 m in all instead of 1.4. False with a
// reason when the pass doesn't show all of it.
bool buildStaff(const Rip& rip, const StaffParts& parts, const TextureSource& textures, std::vector<uint8_t>& out,
		std::string& error, bool shortened = true);

// A gun alone, from its polygons in one or more passes (each capture holds the target's polygons
// of one pass; hands_rip.cpp picks them), as HND1 version 2, flagged gun only: no hands, no
// slide (it travels 0). Gun space as for the pistol (barrel along -z, found by its longest
// edges; +y away from the grip), the origin at the top of the grip, in the middle, the length
// along the barrel the target's lengthM, the muzzle the middle of its front face. Without
// merge (target.merge) the capture with the most of its polygons is the model; merged, the
// captures come together in the eye space of the one with the most of its body: each other
// one fitted onto it (Kabsch on the corners both have, by texture and texture coordinates, but
// those any capture shows in two places), and each of its polygons not there yet added, drawn at
// the welded corners. *complete: all of it was there (wholePolys; merged, the target's other
// checks too, on what went into the model), and so flagged; report->incomplete says what wasn't.
// re/hands_build_test.cpp tests the merge on synthetic captures.
// False with a reason when the captures don't have enough of it or its shape doesn't come out
// as a gun.
bool buildGun(const std::vector<Rip>& captures, const GunTarget& target, const TextureSource& textures,
		std::vector<uint8_t>& out, std::string& error, bool *complete, GunReport *report = nullptr);

}
