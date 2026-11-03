/*
	The settings menu in the headset (hotd2-vr). See xr_menu.h.

	Copyright 2026 mikermak. This file is part of Flycast and is distributed under the GNU GPL v2 or later.
*/
#include "xr_menu.h"
#include "laser_settings.h"
#include "xr_hands.h"
#include "xr_gun.h"
#include "xr_own.h"
#include "xr_host.h"
#include "xr_panel.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "emulator.h"
#include "log/Log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

namespace vr::xr::menu
{
namespace
{

constexpr float Distance = 1.2f, BelowEyes = 0.1f;
constexpr int Left = 0, Right = 1;

const char *const Labels[Rows] { "Resume", "Recenter", "Gun model", "Gun angle", "Reload", "Two players", "Laser", "Crosshair", "Crosshair min size", "Crosshair max size", "Crosshair shrink distance", "Haptics",
		"World size", "Panel distance", "HUD size", "Wider view", "Blood color", "How to play", "Open Service Menu", "Reset to defaults", "Quit" };
const char *const HapticsNames[4] { "Off", "Light", "Medium", "Strong" };
// vr.Haptics' words, saved by the Haptics row (hapticsLevel() reads them, and 1.2's numbers)
const char *const HapticsWords[4] { "off", "light", "medium", "strong" };
const char *const ReloadNames[2] { "Shoot off-screen", "Point off-screen" };
// Two-player assignment only (p1Hand(): 0 left, 1 right); solo follows either trigger.
const char *const HandsNames[2] { "P1 left / P2 right", "P1 right / P2 left" };
constexpr int GunAngleMax = 30;	// vr.GunAngle's range, degrees either way

glm::mat4 model { 1.f };	// the panel: its middle, facing the player (+z)
enum class Page { Gun, World, System, Game, GunAdvanced, WorldAdvanced };
enum class Dialog { None, Reset, Quit };
enum ExtraRow { Advanced = Rows, GunSize, Cancel, Confirm };
Page page = Page::Gun;
Dialog dialog = Dialog::None;
SettingsHit selected;
SettingsHit hover;

bool advanced() { return page == Page::GunAdvanced || page == Page::WorldAdvanced; }
int tab() { return page == Page::GunAdvanced ? 0 : page == Page::WorldAdvanced ? 1 : int(page); }

void show(Page next)
{
    page = next;
    dialog = Dialog::None;
    selected = { SettingsHit::Kind::Row, 0 };
    hover = {};
}
int activeHand = Right;
bool activeTracked;
bool fresh;
// A Gun angle step turns the guns, and the pointers that leave their muzzles, in the next frame:
// that frame's pointers select nothing (else a stick held on Gun angle goes on stepping another row)
bool gunsTurned;
bool pointing[2];
glm::vec3 pointAt[2];
bool quitting;

// vr.Widen as the game started with it: it widens at the start only, so a change says "(next start)"
bool widenAtStart = true;
bool redBloodAtStart = false;
std::atomic<bool> bloodStateRestored { false };
struct WidenRegistration
{
	WidenRegistration() {
		EventManager::listen(Event::Start, [](Event, void *) { widenAtStart = config::VrWiden; redBloodAtStart = config::VrRedBlood; bloodStateRestored = false; });
		// A snapshot also restores the old effect textures. Only a cold start
		// guarantees the saved choice has reloaded every native blood resource.
		EventManager::listen(Event::LoadState, [](Event, void *) { bloodStateRestored = true; });
	}
} widenRegistration;

// vr.Gun's place in its choices (xr_hands.h gunChoices()): a folder gun by vr.GunFile (4 and over all
// meaning that, its name in any case); any other setting, 1.1's 0, 2 and 3 too, the in-game pistol's, as
// chosenGun() reads them; not there: the first's
int gunAt(const std::vector<GunChoice>& choices)
{
	const int setting = config::VrGun >= OwnGuns ? OwnGuns : 1;
	for (size_t i = 0; i < choices.size(); i++)
		if (choices[i].setting == setting && (choices[i].file == nullptr || own::sameStem(config::VrGunFile.get(), choices[i].file)))
			return (int)i;
	return 0;
}

int widerChoice()
{
    if (!config::VrWiden) return 0;
    return config::VrWidenMode == 2 ? 2 : 1;
}

void selectWider(int choice)
{
    config::VrWidenMode.set(choice);
    config::VrWiden.set(choice != 0);
    config::setAutoSave(false);
    config::saveInt("config", "vr.WidenMode", choice);
    config::saveBool("config", "vr.Widen", config::VrWiden);
    config::setAutoSave(true);
}

bool stepper(int row) {
	return row == GunSize || row == Gun || row == GunAngle || row == Reload || row == Reticle || row == ReticleMinSize || row == ReticleMaxSize || row == ReticleShrinkDistance || row == Haptics || row == WorldSize || row == PanelDistance || row == HudSize;
}
// The rows a stick held left or right steps again and again (Input::repeat)
bool repeats(int row) {
	return row == GunSize || row == GunAngle || row == ReticleMinSize || row == ReticleMaxSize || row == ReticleShrinkDistance || row == WorldSize || row == PanelDistance || row == HudSize;
}
int gunAngle() {
	return std::clamp((int)config::VrGunAngle, -GunAngleMax, GunAngleMax);
}
// (Hands too: either way, its other value)
bool onOff(int row) {
	return row == Hands || row == Laser || row == WiderView || row == BloodColor || row == HowToPlay;
}

std::string value(int row)
{
	char s[32];
	switch (row)
	{
	case Gun:
	{
		// a gun not made yet shows dimmed (only when vr.Gun names it: the row skips it)
		const std::vector<GunChoice> choices = gunChoices();
		const GunChoice& gun = choices[gunAt(choices)];
		return std::string(gun.label) + (!gun.made ? (gun.setting >= 4 ? " (not there)" : config::VrGameHands ? " (not made yet)" : " (game's models off)")
				: !gun.complete ? " (incomplete)" : "");
	}
	case GunAngle:
		// (with a sign, no degree sign: the panel adds it, the log stays ASCII)
		std::snprintf(s, sizeof(s), gunAngle() == 0 ? "%d" : "%+d", gunAngle());
		return s;
	case Reload:
		return ReloadNames[std::clamp((int)config::VrReload, 0, 1)];
	case Hands:
		return HandsNames[twoPlayerP1Hand()];
	case Laser:
		return vr::laser::label(config::VrLaser, config::VrLaserWidth);
	case Reticle:
        return vr::reticle::modeLabel(config::VrReticleMode);
    case ReticleMinSize:
        std::snprintf(s,sizeof(s),"%.0f%%",vr::reticle::minimum(config::VrReticleMinSize)*100.f);return s;
    case ReticleMaxSize:
        std::snprintf(s,sizeof(s),"%.0f%%",vr::reticle::maximum(config::VrReticleMaxSize)*100.f);return s;
    case ReticleShrinkDistance:
        std::snprintf(s,sizeof(s),"%.1f m",vr::reticle::distance(config::VrReticleShrinkDistance));return s;
	case Haptics:
		return HapticsNames[hapticsLevel()];
	case WorldSize:
		std::snprintf(s, sizeof(s), "%.2f", (float)config::VrWorldScale);
		return s;
	case PanelDistance:
		std::snprintf(s, sizeof(s), "%.0f", (float)config::VrHudDepth);
		return s;
	case HudSize:
        std::snprintf(s,sizeof(s),"%.0f%%",vr::hud::size(config::VrHudSize)*100.f);return s;
	case WiderView:
		return std::string(config::VrWiden ? "On" : "Off") + (config::VrWiden != widenAtStart ? " (next start)" : "");
	case BloodColor:
		return std::string(config::VrRedBlood ? "Red" : "Green") + (bloodStateRestored || config::VrRedBlood != redBloodAtStart ? " (next start)" : "");
	case HowToPlay:
		return howToPlayOn() ? "On" : "Off";
	case ServiceMenu:
		return serviceMenuAvailable() ? "" : "Offline / after loading";
	default:
		return "";
	}
}

std::vector<int> visibleRows()
{
    if (dialog != Dialog::None) return { Cancel, Confirm };
    switch (page)
    {
    case Page::Gun: return { Gun, GunAngle, Laser, Reticle, Haptics, Advanced };
    case Page::GunAdvanced: return { GunSize, ReticleMinSize, ReticleMaxSize, ReticleShrinkDistance, Reload };
    case Page::World: return { WiderView, Advanced };
    case Page::WorldAdvanced: return { WorldSize, PanelDistance, HudSize };
    case Page::System: return { Hands, ServiceMenu, HowToPlay, ResetDefaults, Quit };
    case Page::Game: return { BloodColor, ServiceMenu };
    }
    return {};
}

config::Option<float>& gunSize()
{
    return gameGun(chosenGun()) ? config::VrHandScale : config::VrGunScale;
}

SettingsPanel panel(const std::vector<int>& rows)
{
    using Kind = SettingsHit::Kind;
    SettingsPanel p;
    p.game = "The House of the Dead 2";
    p.activeTab = tab();
    p.selectedTab = selected.kind == Kind::Tab ? selected.index : -1;
    p.back = advanced();
    p.backSelected = selected.kind == Kind::Back;
    p.closeSelected = selected.kind == Kind::Close;
    p.dialog = dialog != Dialog::None;
    const char* titles[] { "Gun", "World", "System", "In-game", "Advanced gun settings", "Advanced world settings" };
    p.title = titles[int(page)];
    p.subtitle = page == Page::Game ? "Native operator settings" : "The House of the Dead 2 / VR settings";
    p.footer = page == Page::Game ? "Use the original Service menu to change native settings." :
        "Aim + trigger to select | Stick to adjust | B to go back";
    if (p.dialog)
    {
        p.title = dialog == Dialog::Reset ? "Reset VR settings?" : "Quit to the launcher?";
        p.subtitle = dialog == Dialog::Reset ? "Restore VR preferences. Native settings and saves are preserved." :
            "Leave the current game and return to the launcher.";
    }

    for (int row : rows)
    {
        SettingsRow r;
        r.selected = selected.kind == Kind::Row && selected.index == int(p.rows.size());
        r.stepper = stepper(row);
        if (row == GunSize)
        {
            r.label = "Gun size";
            r.value = std::to_string(int(std::round(gunSize().get() * 100.f))) + "%";
        }
        else if (row == Advanced || row == Cancel || row == Confirm)
        {
            r.label = row == Advanced ? "Advanced" : row == Cancel ? "Cancel" : dialog == Dialog::Reset ? "Reset VR settings" : "Quit";
            r.action = true;
        }
        else
        {
            r.label = row == ServiceMenu && serviceMenuExitAvailable() ? "Exit Service Menu" : Labels[row];
            r.value = value(row);
            r.action = row == ServiceMenu || row == ResetDefaults || row == Quit;
        }
        if (row == GunAngle) r.value += "\xB0";
        if (row == ServiceMenu) r.dim = !serviceMenuAvailable();
        if (row == Hands)
        {
            r.choices = { HandsNames[0], HandsNames[1] };
            r.choice = twoPlayerP1Hand();
        }
        if (row == Gun)
        {
            const auto choices = gunChoices();
            for (const auto& choice : choices)
            {
                r.choices.push_back(choice.label);
                r.choiceEnabled.push_back(choice.made);
            }
            r.choice = choices.empty() ? -1 : gunAt(choices);
        }
        if (row == WiderView)
        {
            r.choices = { "Off", "On", "Fade" };
            r.choice = widerChoice();
            if (bool(config::VrWiden) != widenAtStart) r.note = "Applies next start";
        }
        if (row == Laser) { r.choices = { "Off", "Normal", "5x", "10x" }; r.choice = vr::laser::choice(config::VrLaser, config::VrLaserWidth); }
        if (row == Reticle) { r.choices = { "Custom", "None" }; r.choice = vr::reticle::mode(config::VrReticleMode) - 1; }
        if (row == Haptics) { r.choices = { "Off", "Light", "Medium", "Strong" }; r.choice = hapticsLevel(); }
        if (row == BloodColor)
        {
            r.choices = { "Green", "Red" };
            r.choice = config::VrRedBlood;
            if (bloodStateRestored || config::VrRedBlood != redBloodAtStart) r.note = "Applies next start";
        }
        if (row == HowToPlay) { r.choices = { "Off", "On" }; r.choice = howToPlayOn(); }
        if (!r.choices.empty())
        {
            r.stepper = false;
            r.value.clear();
        }
        p.rows.push_back(std::move(r));
    }
    return p;
}

void selectGun(const GunChoice& choice)
{
    if (!choice.made) return;
    config::VrGun.set(choice.setting);
    config::saveInt("config", "vr.Gun", choice.setting);
    if (choice.file != nullptr)
    {
        config::VrGunFile.set(choice.file);
        config::saveStr("config", "vr.GunFile", choice.file);
    }
    chosenGun();
    gunsTurned = true;
}

void selectLaser(int choice)
{
    config::VrLaser.set(choice != 0);
    if (choice != 0) config::VrLaserWidth.set(vr::laser::savedWidth(choice));
    config::setAutoSave(false);
    config::saveBool("config", "vr.Laser", config::VrLaser);
    config::saveInt("config", "vr.LaserWidth", config::VrLaserWidth);
    config::setAutoSave(true);
}

// A value row one step down (dir -1) or up (+1); an On/Off row the other way either way. Each
// change is saved at once; a change of the knock's strength is felt at once in the hand that made it.
void change(int row, int dir, int hand)
{
	switch (row)
	{
	case Gun:
	{
		// the next one each way, round and round, skipping those not made yet
		const std::vector<GunChoice> choices = gunChoices();
		const int n = (int)choices.size();
		if (n < 2)
			break;	// (locked: another game's own model)
		int at = gunAt(choices);
		for (int i = 0; i < n; i++)
		{
			at = (at + dir + n) % n;
			if (choices[at].made)
				break;
		}
        selectGun(choices[at]);
        return;
    }
    case GunSize:
    {
        auto& size = gunSize();
        size.set(std::clamp(size.get() + dir * .05f, .4f, 1.6f));
        config::saveFloat("config", &size == &config::VrHandScale ? "vr.HandScale" : "vr.GunScale", size);
        gunsTurned = true;
        return;
    }
	case GunAngle:
	{
		// 1 degree a step; at an end nothing (no save, no line)
		const int was = gunAngle();
		const int angle = std::clamp(was + dir, -GunAngleMax, GunAngleMax);
		if (angle == was)
			return;
		config::VrGunAngle.set(angle);
		config::saveInt("config", "vr.GunAngle", angle);
		gunsTurned = true;
		break;
	}
	case Reload:
	{
		const int was = std::clamp((int)config::VrReload, 0, 1);
		const int reload = std::clamp(was + dir, 0, 1);
		if (reload == was)
			return;
		config::VrReload.set(reload);
		config::saveInt("config", "vr.Reload", reload);
		break;
	}
	case Hands:
	{
		// with two players in, the guns swap hands once the menu closes (xr_host.cpp updateLightgun)
		config::VrSwapP1P2.set(!config::VrSwapP1P2);
		config::saveBool("config", "vr.SwapP1P2", config::VrSwapP1P2);
		break;
	}
	case Laser:
        selectLaser(vr::laser::nextChoice(config::VrLaser, config::VrLaserWidth, dir));
        break;
	case Reticle:
        config::VrReticleMode.set(vr::reticle::nextMode(config::VrReticleMode,dir));
        config::saveInt("config","vr.ReticleMode",config::VrReticleMode);break;
    case ReticleMinSize:
    {
        const float value=std::clamp(std::round(vr::reticle::minimum(config::VrReticleMinSize)*100.f)+dir,5.f,100.f)/100.f;
        config::VrReticleMinSize.set(value);config::saveFloat("config","vr.ReticleMinSize",value);break;
    }
    case ReticleMaxSize:
    {
        const float value=std::clamp(std::round(vr::reticle::maximum(config::VrReticleMaxSize)*100.f)+dir*5.f,25.f,300.f)/100.f;
        config::VrReticleMaxSize.set(value);config::saveFloat("config","vr.ReticleMaxSize",value);break;
    }
    case ReticleShrinkDistance:
    {
        const float value=std::clamp(std::round(vr::reticle::distance(config::VrReticleShrinkDistance)*10.f)+dir,2.f,200.f)/10.f;
        config::VrReticleShrinkDistance.set(value);config::saveFloat("config","vr.ReticleShrinkDistance",value);break;
    }
	case Haptics:
	{
		const int was = hapticsLevel();
		const int strength = std::clamp(was + dir, 0, 3);
		if (strength == was)
			return;
		config::VrHaptics.set(HapticsWords[strength]);
		config::saveStr("config", "vr.Haptics", HapticsWords[strength]);
		NOTICE_LOG(INPUT, "XR: menu %s -> %s", Labels[row], value(row).c_str());
		sampleKnock(hand);
		return;
	}
	case WorldSize:
	{
		const float was = config::VrWorldScale;
		const float scale = vr::world_scale::step(was, dir);
		if (scale == was)
			return;
		config::VrWorldScale.set(scale);
		config::saveFloat("config", "vr.WorldScale", scale);
		break;
	}
	case PanelDistance:
	{
		const float was = config::VrHudDepth;
		const float depth = std::clamp(was + 5.f * dir, 15.f, 120.f);
		if (depth == was)
			return;
		config::VrHudDepth.set(depth);
		config::saveFloat("config", "vr.HudDepth", depth);
		break;
	}
	case HudSize:
    {
        const float size=vr::hud::step(config::VrHudSize,dir);
        config::VrHudSize.set(size);config::saveFloat("config","vr.HudSize",size);break;
    }
	case WiderView:
		selectWider(std::clamp(widerChoice() + dir, 0, 2));
		break;
	case BloodColor:
		config::VrRedBlood.set(!config::VrRedBlood);
		config::saveBool("config", "vr.RedBlood", config::VrRedBlood);
		break;
	case HowToPlay:
		// on or off, the menu staying open (closing it turns it off: xr_host.h). The next frame the
		// hands swap gun and controller, and a ray moves by the muzzle's offset: that update learns
		// the rows again, so the selection stays on this row.
		NOTICE_LOG(INPUT, "XR: menu How to play");
		setHowToPlay(!howToPlayOn(), "chosen");
		fresh = true;
		return;
	default:
		return;
	}
	NOTICE_LOG(INPUT, "XR: menu %s -> %s", Labels[row], value(row).c_str());
	if (row == Gun)
		chosenGun();	// (its "XR: gun: ..." line at once)
}

void resetMenuSettings()
{
    // Only VR menu settings are reset. Game data and native operator settings remain intact.
    config::setAutoSave(false);
    config::VrGunScale.reset();
    config::saveFloat("config", "vr.GunScale", config::VrGunScale);
    config::VrHandScale.reset();
    config::saveFloat("config", "vr.HandScale", config::VrHandScale);
    config::VrGun.reset();
    config::saveInt("config", "vr.Gun", config::VrGun.get());
    config::VrGunFile.reset();
    config::saveStr("config", "vr.GunFile", config::VrGunFile.get());
    config::VrGunAngle.reset();
    config::saveInt("config", "vr.GunAngle", config::VrGunAngle.get());
    config::VrReload.reset();
    config::saveInt("config", "vr.Reload", config::VrReload.get());
    config::VrSwapP1P2.reset();
    config::saveBool("config", "vr.SwapP1P2", config::VrSwapP1P2.get());
    config::VrLaserWidth.reset();
    config::saveInt("config", "vr.LaserWidth", config::VrLaserWidth);
    config::VrLaser.reset();
    config::saveBool("config", "vr.Laser", config::VrLaser.get());
    config::VrReticleMode.reset();
    config::saveInt("config", "vr.ReticleMode", config::VrReticleMode.get());
    config::VrReticleMinSize.reset();
    config::saveFloat("config", "vr.ReticleMinSize", config::VrReticleMinSize.get());
    config::VrReticleMaxSize.reset();
    config::saveFloat("config", "vr.ReticleMaxSize", config::VrReticleMaxSize.get());
    config::VrReticleShrinkDistance.reset();
    config::saveFloat("config", "vr.ReticleShrinkDistance", config::VrReticleShrinkDistance.get());
    config::VrHaptics.reset();
    config::saveStr("config", "vr.Haptics", config::VrHaptics.get());
    config::VrWorldScale.reset();
    config::saveFloat("config", "vr.WorldScale", config::VrWorldScale.get());
    config::VrHudDepth.reset();
    config::saveFloat("config", "vr.HudDepth", config::VrHudDepth.get());
    config::VrHudSize.reset();
    config::saveFloat("config", "vr.HudSize", config::VrHudSize.get());
    config::VrWidenMode.reset();
    config::saveInt("config", "vr.WidenMode", config::VrWidenMode);
    config::VrWiden.reset();
    config::saveBool("config", "vr.Widen", config::VrWiden.get());
    config::VrRedBlood.reset();
    config::saveBool("config", "vr.RedBlood", config::VrRedBlood.get());
    config::setAutoSave(true);
}

// A row chosen (A, or a click away from a value row's halves): its action, or + on a value row.
void choose(int row, int hand)
{
	switch (row)
	{
    case Advanced:
        show(page == Page::Gun ? Page::GunAdvanced : Page::WorldAdvanced);
        break;
    case Cancel:
        dialog = Dialog::None;
        selected = { SettingsHit::Kind::Row, 0 };
        break;
    case Confirm:
        if (dialog == Dialog::Quit)
        {
            quitting = true;
            quitToStart();
        }
        else if (dialog == Dialog::Reset)
        {
            resetMenuSettings();
            setHowToPlay(false, "reset to defaults");
            chosenGun();
            gunsTurned = fresh = true;
        }
        dialog = Dialog::None;
        selected = { SettingsHit::Kind::Row, 0 };
        break;
	case ServiceMenu:
        if (openServiceMenu())
        {
            NOTICE_LOG(INPUT, "XR: menu Open Service Menu");
            close();
        }
        break;
    case ResetDefaults:
    case Quit:
        dialog = row == ResetDefaults ? Dialog::Reset : Dialog::Quit;
        selected = { SettingsHit::Kind::Row, 0 };
        break;
	default:
		change(row, +1, hand);
		break;
	}
}

}	// namespace

bool isOpen()
{
	return menuPaused();
}

void place(const glm::mat4& head)
{
	const glm::vec3 eyes(head[3]);
	glm::vec3 forward = -glm::vec3(head[2]);
	// looking straight down (or up), the top of the head (or the chin) is where the face is turned
	if (std::abs(forward.x) + std::abs(forward.z) < 1e-3f)
		forward = glm::vec3(head[1]) * (forward.y < 0.f ? 1.f : -1.f);
	const glm::mat4 turn = glm::rotate(glm::mat4(1.f), std::atan2(-forward.x, -forward.z), glm::vec3(0.f, 1.f, 0.f));
	model = glm::translate(glm::mat4(1.f), eyes + glm::vec3(turn * glm::vec4(0.f, 0.f, -Distance, 0.f)) - glm::vec3(0.f, BelowEyes, 0.f)) * turn;
}

void open(const glm::mat4& head)
{
	place(head);
    show(Page::Gun);
    activeHand = Right;
    activeTracked = false;
    pointing[Left] = pointing[Right] = false;
    fresh = true;
    quitting = false;
    own::scan();
	requestPause(true);
}

void close()
{
	dialog = Dialog::None;
	requestPause(false);
}

void reset()
{
	dialog = Dialog::None;
	quitting = false;
}

void update(const Input& input)
{
    using Kind = SettingsHit::Kind;
    if (!isOpen() || quitting) return;
    if (input.toggle) { close(); return; }
    if (input.back)
    {
        if (dialog != Dialog::None) choose(Cancel, activeHand);
        else if (advanced()) show(Page(tab()));
        else close();
        return;
    }

    const int other = 1 - activeHand;
    const bool transfer = input.ray[other].valid && input.ray[other].click;
    if (transfer)
    {
        activeHand = other;
        hover = {};
    }
    const auto& ray = input.ray[activeHand];
    activeTracked = ray.valid;
    const auto rows = visibleRows();
    const auto p = panel(rows);
    pointing[1 - activeHand] = false;
    pointing[activeHand] = ray.valid && settingsPanelHit(model, p, ray.origin, ray.dir, &pointAt[activeHand]);
    const auto hit = ray.valid ? settingsHit(model, p, ray.origin, ray.dir) : SettingsHit{};
    const auto same = [](const SettingsHit& a, const SettingsHit& b) {
        return a.kind == b.kind && a.index == b.index;
    };
    const bool suppress = fresh || gunsTurned;
    if (!suppress && hit.kind != Kind::None && !same(hit, hover)) selected = hit;
    hover = hit;
    fresh = gunsTurned = false;
    if (transfer) return;

    if (input.up || input.down)
    {
        std::vector<SettingsHit> targets;
        if (!p.dialog)
        {
            targets.push_back({ Kind::Close });
            for (int i = 0; i < 4; ++i) targets.push_back({ Kind::Tab, i });
            if (p.back) targets.push_back({ Kind::Back });
        }
        for (int i = 0; i < int(rows.size()); ++i)
            if (!p.rows[i].dim) targets.push_back({ Kind::Row, i });
        if (!targets.empty())
        {
            auto current = std::find_if(targets.begin(), targets.end(), [&](const SettingsHit& h) { return same(selected, h); });
            const int n = int(targets.size());
            const int index = current == targets.end() ? (input.up ? 0 : n - 1) : int(current - targets.begin());
            selected = targets[(index + (input.up ? -1 : 1) + n) % n];
        }
    }
    const bool click = !suppress && ray.click && hit.kind != Kind::None;
    if (click) selected = hit;
    const bool activate = click || input.select;
    if (activate && selected.kind == Kind::Tab) { show(Page(selected.index)); return; }
    if (activate && selected.kind == Kind::Close) { close(); return; }
    if (activate && selected.kind == Kind::Back) { show(Page(tab())); return; }
    if (selected.kind != Kind::Row || selected.index < 0 || selected.index >= int(rows.size()) || p.rows[selected.index].dim) return;
    const int row = rows[selected.index];
    if (click && hit.choice >= 0)
    {
        switch (row)
        {
        case Gun:
        {
            const auto choices = gunChoices();
            if (hit.choice < int(choices.size())) selectGun(choices[hit.choice]);
            return;
        }
        case Hands:
            if (hit.choice != twoPlayerP1Hand()) change(row, +1, activeHand);
            return;
        case WiderView:
            selectWider(hit.choice);
            return;
        case Laser:
            selectLaser(hit.choice);
            return;
        case Reticle:
            if (hit.choice + 1 != int(config::VrReticleMode)) change(row, +1, activeHand);
            return;
        case Haptics:
            if (hit.choice != hapticsLevel())
            {
                config::VrHaptics.set(HapticsWords[hit.choice]);
                config::saveStr("config", "vr.Haptics", HapticsWords[hit.choice]);
                sampleKnock(activeHand);
            }
            return;
        case BloodColor:
            if (bool(hit.choice) != config::VrRedBlood) change(row, +1, activeHand);
            return;
        case HowToPlay:
            if (bool(hit.choice) != howToPlayOn()) change(row, +1, activeHand);
            return;
        default:
            return;
        }
    }
    if (click && hit.direction) change(row, hit.direction, activeHand);
    else if (activate) choose(row, activeHand);
    else if ((input.left || input.right) && (stepper(row) || onOff(row)) && (!input.repeat || repeats(row)))
        change(row, input.left ? -1 : +1, activeHand);

}

bool pointer(int hand, glm::vec3& at)
{
	if (hand < 0 || hand > 1 || pointerHand() != hand || !pointing[hand])
		return false;
	at = pointAt[hand];
	return true;
}

int pointerHand() { return isOpen() && activeTracked ? activeHand : -1; }

void draw(const glm::mat4& viewProj)
{
    if (!isOpen()) return;
    drawSettingsPanel(viewProj, model, panel(visibleRows()));
    if (pointing[activeHand])
    {
        auto contact = model;
        contact[3] = glm::vec4(pointAt[activeHand] + glm::vec3(model[2]) * .004f, 1.f);
        drawPanelContact(viewProj, contact);
    }
}

}
