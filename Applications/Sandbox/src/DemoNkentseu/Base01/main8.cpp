// =============================================================================
// main8.cpp
// Validation manette: layout type PlayStation 3 en rectangles.
// Chaque bouton allume son rectangle. Les sticks affichent aussi leur position.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKContext/Deprecate/NkRendererConfig.h"
#include "NKWindow/Events/NkGamepadSystem.h"
#include "NKTime/NkChrono.h"
#include "NKMath/NKMath.h"
#include "NKMemory/NkUtils.h"
#include "NKContainers/CacheFriendly/NkArray.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#include <emscripten.h>
#endif

#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

// AppData pattern #9: self-registered lambda (no macro).
static nkentseu::NkAppData BuildGamepadPs3AppData() {
    nkentseu::NkAppData d{};
    d.appName = "SandboxGamepadPS3";
    d.appVersion = "1.0.0";
    d.enableEventLogging = true;
    d.enableRendererDebug = false;
    d.enableMultiWindow = false;
    return d;
}

namespace {
const bool gGamepadPs3AppDataRegistered = []() {
    nkentseu::NkSetEntryAppData(BuildGamepadPs3AppData());
    return true;
}();
} // namespace

namespace {
using namespace nkentseu;

static float SafeAxis01(float v) {
    if (!math::NkIsFinite(v)) return 0.f;
    return math::NkClamp(v, 0.0f, 1.0f);
}

static float SafeAxis11(float v) {
    if (!math::NkIsFinite(v)) return 0.f;
    return math::NkClamp(v, -1.0f, 1.0f);
}

struct RectI {
    int32 x;
    int32 y;
    int32 w;
    int32 h;
};

struct ButtonVisual {
    NkGamepadButton button;
    float cx;
    float cy;
    float w;
    float h;
    uint32 offColor;
    uint32 onColor;
};

static RectI MakeRect(float centerX, float centerY, float width, float height) {
    RectI r{};
    r.w = static_cast<int32>(width);
    r.h = static_cast<int32>(height);
    r.x = static_cast<int32>(centerX) - r.w / 2;
    r.y = static_cast<int32>(centerY) - r.h / 2;
    return r;
}

static void FillRect(NkRenderer& renderer, const RectI& r, uint32 color) {
    for (int32 y = r.y; y < r.y + r.h; ++y) {
        for (int32 x = r.x; x < r.x + r.w; ++x) {
            renderer.SetPixel(x, y, color);
        }
    }
}

static void StrokeRect(NkRenderer& renderer, const RectI& r, uint32 color) {
    for (int32 x = r.x; x < r.x + r.w; ++x) {
        renderer.SetPixel(x, r.y, color);
        renderer.SetPixel(x, r.y + r.h - 1, color);
    }
    for (int32 y = r.y; y < r.y + r.h; ++y) {
        renderer.SetPixel(r.x, y, color);
        renderer.SetPixel(r.x + r.w - 1, y, color);
    }
}

static uint32 Dim(uint32 c, float k) {
    uint8 r, g, b, a;
    NkRenderer::UnpackColor(c, r, g, b, a);
    r = static_cast<uint8>(math::NkClamp(static_cast<int>(r * k), 0, 255));
    g = static_cast<uint8>(math::NkClamp(static_cast<int>(g * k), 0, 255));
    b = static_cast<uint8>(math::NkClamp(static_cast<int>(b * k), 0, 255));
    return NkRenderer::PackColor(r, g, b, a);
}

static void DrawStick(
    NkRenderer& renderer, float cx, float cy, float size,
    float ax, float ay, bool pressed)
{
    ax = SafeAxis11(ax);
    ay = SafeAxis11(ay);
    const RectI zone = MakeRect(cx, cy, size, size);
    const uint32 zoneColor = pressed
        ? NkRenderer::PackColor(110, 180, 255, 255)
        : NkRenderer::PackColor(55, 55, 70, 255);
    FillRect(renderer, zone, zoneColor);
    StrokeRect(renderer, zone, NkRenderer::PackColor(20, 20, 24, 255));

    const float travel = (size * 0.5f) - 8.0f;
    const float kx = ax;
    const float ky = ay;
    const float knobX = cx + kx * travel;
    const float knobY = cy - ky * travel; // Y+ = haut cAAtAA NK.

    const RectI knob = MakeRect(knobX, knobY, 14.0f, 14.0f);
    FillRect(renderer, knob, NkRenderer::PackColor(245, 245, 245, 255));
    StrokeRect(renderer, knob, NkRenderer::PackColor(30, 30, 30, 255));
}

static uint32 FindFirstConnectedGamepad() {
    for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
        if (NkGamepads().IsConnected(i)) {
            return i;
        }
    }
    return NK_MAX_GAMEPADS;
}

static bool IsEscapePressed(const NkKeyPressEvent* k) {
    if (!k) return false;
    return k->GetKey() == NkKey::NK_ESCAPE
        || k->GetScancode() == NkScancode::NK_SC_ESCAPE;
}

enum class NkPadProfile : uint32 {
    AUTO = 0,
    XBOX,
    PS4,
    PS3,
    SWITCH_PRO,
    GENERIC_DINPUT,
    COUNT
};

static constexpr uint32 kTestButtonCount = 54;
static constexpr uint32 kTestAxisCount = 23;
static constexpr uint32 kKnownButtonCount = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
static constexpr uint32 kKnownAxisCount = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
static constexpr uint32 kRawButtonBase = kKnownButtonCount;

static const char* ProfileName(NkPadProfile p) {
    switch (p) {
        case NkPadProfile::AUTO: return "Auto";
        case NkPadProfile::XBOX: return "Xbox";
        case NkPadProfile::PS4: return "PS4/PS5";
        case NkPadProfile::PS3: return "PS3";
        case NkPadProfile::SWITCH_PRO: return "Switch Pro";
        case NkPadProfile::GENERIC_DINPUT: return "Generic DInput";
        default: return "Unknown";
    }
}

static NkString LowerAscii(NkString v) {
    for (char& c : v) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return v;
}

static bool Contains(const NkString& hay, const char* needle) {
    return hay.Find(needle) != NkString::npos;
}

static NkPadProfile DetectPadProfile(const NkGamepadInfo& info) {
    const NkString name = LowerAscii(info.name);

    if (info.type == NkGamepadType::NK_GP_TYPE_XBOX || info.vendorId == 0x045E ||
        Contains(name, "xbox") || Contains(name, "xinput"))
    {
        return NkPadProfile::XBOX;
    }

    if (info.vendorId == 0x054C || Contains(name, "sony") || Contains(name, "playstation") ||
        Contains(name, "dualshock") || Contains(name, "dualsense") || Contains(name, "wireless controller"))
    {
        if (Contains(name, "dualshock 3") || Contains(name, "ps3")) {
            return NkPadProfile::PS3;
        }
        return NkPadProfile::PS4;
    }

    if (info.vendorId == 0x057E || Contains(name, "nintendo") ||
        Contains(name, "switch") || Contains(name, "pro controller"))
    {
        return NkPadProfile::SWITCH_PRO;
    }

    return NkPadProfile::GENERIC_DINPUT;
}

static void DisableAllPadMappings(uint32 padIndex) {
    auto& gp = NkGamepads();
    for (uint32 b = 0; b < NkGamepadSystem::BUTTON_COUNT; ++b) {
        gp.DisableButtonByIndex(padIndex, b);
    }
    for (uint32 a = 0; a < NkGamepadSystem::AXIS_COUNT; ++a) {
        gp.DisableAxisByIndex(padIndex, a);
    }
}

static void MapCommonAxes(uint32 padIndex, bool invertStickY) {
    auto& gp = NkGamepads();
    auto mapAxis = [&](uint32 src, NkGamepadAxis dst, bool invert = false) {
        gp.SetAxisMapByIndex(padIndex, src, dst, invert, 1.f);
    };

    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LX), NkGamepadAxis::NK_GP_AXIS_LX, false);
    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LY), NkGamepadAxis::NK_GP_AXIS_LY, invertStickY);
    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RX), NkGamepadAxis::NK_GP_AXIS_RX, false);
    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RY), NkGamepadAxis::NK_GP_AXIS_RY, invertStickY);
    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT), NkGamepadAxis::NK_GP_AXIS_LT, false);
    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT), NkGamepadAxis::NK_GP_AXIS_RT, false);
    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X), NkGamepadAxis::NK_GP_AXIS_DPAD_X, false);
    mapAxis(static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y), NkGamepadAxis::NK_GP_AXIS_DPAD_Y, false);
}

static void MapXboxLikeLogicalButtons(uint32 padIndex) {
    auto& gp = NkGamepads();
    auto mapBtn = [&](NkGamepadButton btn) {
        gp.SetButtonMapByIndex(padIndex, static_cast<uint32>(btn), btn);
    };

    mapBtn(NkGamepadButton::NK_GP_SOUTH);
    mapBtn(NkGamepadButton::NK_GP_EAST);
    mapBtn(NkGamepadButton::NK_GP_WEST);
    mapBtn(NkGamepadButton::NK_GP_NORTH);
    mapBtn(NkGamepadButton::NK_GP_LB);
    mapBtn(NkGamepadButton::NK_GP_RB);
    mapBtn(NkGamepadButton::NK_GP_LT_DIGITAL);
    mapBtn(NkGamepadButton::NK_GP_RT_DIGITAL);
    mapBtn(NkGamepadButton::NK_GP_LSTICK);
    mapBtn(NkGamepadButton::NK_GP_RSTICK);
    mapBtn(NkGamepadButton::NK_GP_BACK);
    mapBtn(NkGamepadButton::NK_GP_START);
    mapBtn(NkGamepadButton::NK_GP_GUIDE);
    mapBtn(NkGamepadButton::NK_GP_DPAD_UP);
    mapBtn(NkGamepadButton::NK_GP_DPAD_DOWN);
    mapBtn(NkGamepadButton::NK_GP_DPAD_LEFT);
    mapBtn(NkGamepadButton::NK_GP_DPAD_RIGHT);
}

static void MapDInputPhysicalButton(uint32 padIndex, uint32 physicalButton, NkGamepadButton dst) {
    NkGamepads().SetButtonMapByIndex(padIndex, kRawButtonBase + physicalButton, dst);
}

static void AddProbeChannels(uint32 padIndex) {
    auto& gp = NkGamepads();
    const uint32 buttonLimit = math::NkMin(kTestButtonCount, NkGamepadSystem::BUTTON_COUNT);
    const uint32 axisLimit = math::NkMin(kTestAxisCount, NkGamepadSystem::AXIS_COUNT);

    for (uint32 b = kKnownButtonCount; b < buttonLimit; ++b) {
        gp.SetButtonMapByIndex(padIndex, b, static_cast<NkGamepadButton>(b));
    }
    for (uint32 a = kKnownAxisCount; a < axisLimit; ++a) {
        gp.SetAxisMapByIndex(padIndex, a, static_cast<NkGamepadAxis>(a), false, 1.f);
    }
}

static void ApplyPadProfileMapping(uint32 padIndex, NkPadProfile profile, bool invertStickY) {
    auto& gp = NkGamepads();
    gp.ClearMapping(padIndex);
    DisableAllPadMappings(padIndex);
    MapCommonAxes(padIndex, invertStickY);

    switch (profile) {
        case NkPadProfile::XBOX:
            MapXboxLikeLogicalButtons(padIndex);
            break;
        case NkPadProfile::PS4:
            MapDInputPhysicalButton(padIndex, 0,  NkGamepadButton::NK_GP_WEST);
            MapDInputPhysicalButton(padIndex, 1,  NkGamepadButton::NK_GP_SOUTH);
            MapDInputPhysicalButton(padIndex, 2,  NkGamepadButton::NK_GP_EAST);
            MapDInputPhysicalButton(padIndex, 3,  NkGamepadButton::NK_GP_NORTH);
            MapDInputPhysicalButton(padIndex, 4,  NkGamepadButton::NK_GP_LB);
            MapDInputPhysicalButton(padIndex, 5,  NkGamepadButton::NK_GP_RB);
            MapDInputPhysicalButton(padIndex, 6,  NkGamepadButton::NK_GP_LT_DIGITAL);
            MapDInputPhysicalButton(padIndex, 7,  NkGamepadButton::NK_GP_RT_DIGITAL);
            MapDInputPhysicalButton(padIndex, 8,  NkGamepadButton::NK_GP_BACK);
            MapDInputPhysicalButton(padIndex, 9,  NkGamepadButton::NK_GP_START);
            MapDInputPhysicalButton(padIndex, 10, NkGamepadButton::NK_GP_LSTICK);
            MapDInputPhysicalButton(padIndex, 11, NkGamepadButton::NK_GP_RSTICK);
            MapDInputPhysicalButton(padIndex, 12, NkGamepadButton::NK_GP_GUIDE);
            MapDInputPhysicalButton(padIndex, 13, NkGamepadButton::NK_GP_TOUCHPAD);
            break;
        case NkPadProfile::PS3:
            MapDInputPhysicalButton(padIndex, 0,  NkGamepadButton::NK_GP_BACK);
            MapDInputPhysicalButton(padIndex, 1,  NkGamepadButton::NK_GP_LSTICK);
            MapDInputPhysicalButton(padIndex, 2,  NkGamepadButton::NK_GP_RSTICK);
            MapDInputPhysicalButton(padIndex, 3,  NkGamepadButton::NK_GP_START);
            MapDInputPhysicalButton(padIndex, 4,  NkGamepadButton::NK_GP_DPAD_UP);
            MapDInputPhysicalButton(padIndex, 5,  NkGamepadButton::NK_GP_DPAD_RIGHT);
            MapDInputPhysicalButton(padIndex, 6,  NkGamepadButton::NK_GP_DPAD_DOWN);
            MapDInputPhysicalButton(padIndex, 7,  NkGamepadButton::NK_GP_DPAD_LEFT);
            MapDInputPhysicalButton(padIndex, 8,  NkGamepadButton::NK_GP_LT_DIGITAL);
            MapDInputPhysicalButton(padIndex, 9,  NkGamepadButton::NK_GP_RT_DIGITAL);
            MapDInputPhysicalButton(padIndex, 10, NkGamepadButton::NK_GP_LB);
            MapDInputPhysicalButton(padIndex, 11, NkGamepadButton::NK_GP_RB);
            MapDInputPhysicalButton(padIndex, 12, NkGamepadButton::NK_GP_NORTH);
            MapDInputPhysicalButton(padIndex, 13, NkGamepadButton::NK_GP_EAST);
            MapDInputPhysicalButton(padIndex, 14, NkGamepadButton::NK_GP_SOUTH);
            MapDInputPhysicalButton(padIndex, 15, NkGamepadButton::NK_GP_WEST);
            MapDInputPhysicalButton(padIndex, 16, NkGamepadButton::NK_GP_GUIDE);
            break;
        case NkPadProfile::SWITCH_PRO:
            MapDInputPhysicalButton(padIndex, 0,  NkGamepadButton::NK_GP_SOUTH);
            MapDInputPhysicalButton(padIndex, 1,  NkGamepadButton::NK_GP_EAST);
            MapDInputPhysicalButton(padIndex, 2,  NkGamepadButton::NK_GP_WEST);
            MapDInputPhysicalButton(padIndex, 3,  NkGamepadButton::NK_GP_NORTH);
            MapDInputPhysicalButton(padIndex, 4,  NkGamepadButton::NK_GP_LB);
            MapDInputPhysicalButton(padIndex, 5,  NkGamepadButton::NK_GP_RB);
            MapDInputPhysicalButton(padIndex, 6,  NkGamepadButton::NK_GP_LT_DIGITAL);
            MapDInputPhysicalButton(padIndex, 7,  NkGamepadButton::NK_GP_RT_DIGITAL);
            MapDInputPhysicalButton(padIndex, 8,  NkGamepadButton::NK_GP_BACK);
            MapDInputPhysicalButton(padIndex, 9,  NkGamepadButton::NK_GP_START);
            MapDInputPhysicalButton(padIndex, 10, NkGamepadButton::NK_GP_LSTICK);
            MapDInputPhysicalButton(padIndex, 11, NkGamepadButton::NK_GP_RSTICK);
            MapDInputPhysicalButton(padIndex, 12, NkGamepadButton::NK_GP_GUIDE);
            MapDInputPhysicalButton(padIndex, 13, NkGamepadButton::NK_GP_CAPTURE);
            break;
        case NkPadProfile::GENERIC_DINPUT:
            MapDInputPhysicalButton(padIndex, 0,  NkGamepadButton::NK_GP_SOUTH);
            MapDInputPhysicalButton(padIndex, 1,  NkGamepadButton::NK_GP_EAST);
            MapDInputPhysicalButton(padIndex, 2,  NkGamepadButton::NK_GP_WEST);
            MapDInputPhysicalButton(padIndex, 3,  NkGamepadButton::NK_GP_NORTH);
            MapDInputPhysicalButton(padIndex, 4,  NkGamepadButton::NK_GP_LB);
            MapDInputPhysicalButton(padIndex, 5,  NkGamepadButton::NK_GP_RB);
            MapDInputPhysicalButton(padIndex, 6,  NkGamepadButton::NK_GP_BACK);
            MapDInputPhysicalButton(padIndex, 7,  NkGamepadButton::NK_GP_START);
            MapDInputPhysicalButton(padIndex, 8,  NkGamepadButton::NK_GP_LSTICK);
            MapDInputPhysicalButton(padIndex, 9,  NkGamepadButton::NK_GP_RSTICK);
            MapDInputPhysicalButton(padIndex, 10, NkGamepadButton::NK_GP_GUIDE);
            break;
        case NkPadProfile::AUTO:
        case NkPadProfile::COUNT:
            break;
    }

    AddProbeChannels(padIndex);
}
struct RawProbeState {
    bool initialized[NK_MAX_GAMEPADS] = {};
    bool buttons[NK_MAX_GAMEPADS][NkGamepadSystem::BUTTON_COUNT] = {};
    float axes[NK_MAX_GAMEPADS][NkGamepadSystem::AXIS_COUNT] = {};
};

static void ProbeRawInput(uint32 padIndex, RawProbeState& probe) {
    if (padIndex >= NK_MAX_GAMEPADS) return;

    const NkGamepadSnapshot& raw = NkGamepads().GetRawSnapshot(padIndex);
    if (!raw.connected) {
        probe.initialized[padIndex] = false;
        memory::NkMemSet(probe.buttons[padIndex], 0, sizeof(probe.buttons[padIndex]));
        memory::NkMemSet(probe.axes[padIndex], 0, sizeof(probe.axes[padIndex]));
        return;
    }

    if (!probe.initialized[padIndex]) {
        for (uint32 b = 0; b < NkGamepadSystem::BUTTON_COUNT; ++b) {
            probe.buttons[padIndex][b] = raw.buttons[b];
        }
        for (uint32 a = 0; a < NkGamepadSystem::AXIS_COUNT; ++a) {
            probe.axes[padIndex][a] = raw.axes[a];
        }
        probe.initialized[padIndex] = true;
        return;
    }

    for (uint32 b = 0; b < NkGamepadSystem::BUTTON_COUNT; ++b) {
        if (raw.buttons[b] == probe.buttons[padIndex][b]) continue;
        const bool wasDown = probe.buttons[padIndex][b];
        probe.buttons[padIndex][b] = raw.buttons[b];
        logger.Infof("[RAW GP%u] button[%u] %s",
            padIndex, b, raw.buttons[b] ? "DOWN" : "UP");

        // Diagnostic demandAA: code bas niveau "non spAAcifiA- par l'API NK.
        // Ici: tout bouton hors enum NkGamepadButton (canal brut AAtendu).
        if (b >= kKnownButtonCount && !wasDown && raw.buttons[b]) {
            logger.Infof("[LOW-LEVEL GP%u] UNKNOWN_BUTTON code=%u (raw_index=%u)",
                padIndex,
                static_cast<unsigned>(b - kRawButtonBase),
                static_cast<unsigned>(b));
        }
    }

    for (uint32 a = 0; a < NkGamepadSystem::AXIS_COUNT; ++a) {
        const float prev = math::NkIsFinite(probe.axes[padIndex][a]) ? probe.axes[padIndex][a] : 0.f;
        float now = raw.axes[a];
        if (!math::NkIsFinite(now)) now = 0.f;
        if (math::NkFabs(now - prev) < 0.20f) continue;
        probe.axes[padIndex][a] = now;
        logger.Infof("[RAW GP%u] axis[%u] %.3f", padIndex, a, now);

        // MAAme diagnostic pour les axes non standard.
        if (a >= kKnownAxisCount) {
            logger.Infof("[LOW-LEVEL GP%u] UNKNOWN_AXIS code=%u (raw_index=%u) value=%.3f",
                padIndex,
                static_cast<unsigned>(a - kKnownAxisCount),
                static_cast<unsigned>(a),
                now);
        }
    }
}

static void DrawPs3Layout(
    NkRenderer& renderer, uint32 width, uint32 height,
    bool connected, uint32 padIndex)
{
    if (width == 0 || height == 0) return;
    const float s = math::NkMin(width / 1280.0f, height / 720.0f);
    const float cx = width * 0.5f;
    const float cy = height * 0.54f;

    const RectI body = MakeRect(cx, cy, 860.0f * s, 420.0f * s);
    FillRect(renderer, body, NkRenderer::PackColor(26, 26, 36, 255));
    StrokeRect(renderer, body, NkRenderer::PackColor(60, 60, 84, 255));

    const uint32 idle = NkRenderer::PackColor(78, 78, 92, 255);
    const uint32 outline = NkRenderer::PackColor(20, 20, 25, 255);

    const ButtonVisual buttons[] = {
        // D-Pad
        { NkGamepadButton::NK_GP_DPAD_UP,    -270.f, -28.f, 44.f, 34.f, idle, NkRenderer::PackColor(120, 200, 255, 255) },
        { NkGamepadButton::NK_GP_DPAD_DOWN,  -270.f,  58.f, 44.f, 34.f, idle, NkRenderer::PackColor(120, 200, 255, 255) },
        { NkGamepadButton::NK_GP_DPAD_LEFT,  -318.f,  15.f, 44.f, 34.f, idle, NkRenderer::PackColor(120, 200, 255, 255) },
        { NkGamepadButton::NK_GP_DPAD_RIGHT, -222.f,  15.f, 44.f, 34.f, idle, NkRenderer::PackColor(120, 200, 255, 255) },

        // Face buttons (PS style)
        { NkGamepadButton::NK_GP_NORTH,  270.f, -28.f, 44.f, 34.f, idle, NkRenderer::PackColor(90, 220, 90, 255)  }, // Triangle
        { NkGamepadButton::NK_GP_SOUTH,  270.f,  58.f, 44.f, 34.f, idle, NkRenderer::PackColor(80, 150, 255, 255) }, // Cross
        { NkGamepadButton::NK_GP_WEST,   222.f,  15.f, 44.f, 34.f, idle, NkRenderer::PackColor(240, 120, 220, 255)}, // Square
        { NkGamepadButton::NK_GP_EAST,   318.f,  15.f, 44.f, 34.f, idle, NkRenderer::PackColor(255, 110, 110, 255)}, // Circle

        // Shoulders / triggers
        { NkGamepadButton::NK_GP_LB,        -230.f, -166.f, 120.f, 32.f, idle, NkRenderer::PackColor(255, 180, 80, 255) },
        { NkGamepadButton::NK_GP_LT_DIGITAL,-230.f, -210.f, 120.f, 32.f, idle, NkRenderer::PackColor(255, 140, 60, 255) },
        { NkGamepadButton::NK_GP_RB,         230.f, -166.f, 120.f, 32.f, idle, NkRenderer::PackColor(255, 180, 80, 255) },
        { NkGamepadButton::NK_GP_RT_DIGITAL, 230.f, -210.f, 120.f, 32.f, idle, NkRenderer::PackColor(255, 140, 60, 255) },

        // Center buttons
        { NkGamepadButton::NK_GP_BACK,   -72.f,  12.f, 84.f, 30.f, idle, NkRenderer::PackColor(180, 180, 180, 255) }, // Select
        { NkGamepadButton::NK_GP_START,   72.f,  12.f, 84.f, 30.f, idle, NkRenderer::PackColor(180, 180, 180, 255) }, // Start
        { NkGamepadButton::NK_GP_GUIDE,    0.f, -26.f, 72.f, 30.f, idle, NkRenderer::PackColor(120, 255, 140, 255) }, // PS
    };

    for (const ButtonVisual& b : buttons) {
        const bool down = connected && NkGamepads().IsButtonDown(padIndex, b.button);
        RectI r = MakeRect(cx + b.cx * s, cy + b.cy * s, b.w * s, b.h * s);
        FillRect(renderer, r, down ? b.onColor : b.offColor);
        StrokeRect(renderer, r, outline);
    }

    float lx = 0.f, ly = 0.f, rx = 0.f, ry = 0.f, lt = 0.f, rt = 0.f;
    bool l3 = false, r3 = false;
    if (connected) {
        lx = SafeAxis11(NkGamepads().GetAxis(padIndex, NkGamepadAxis::NK_GP_AXIS_LX));
        ly = SafeAxis11(NkGamepads().GetAxis(padIndex, NkGamepadAxis::NK_GP_AXIS_LY));
        rx = SafeAxis11(NkGamepads().GetAxis(padIndex, NkGamepadAxis::NK_GP_AXIS_RX));
        ry = SafeAxis11(NkGamepads().GetAxis(padIndex, NkGamepadAxis::NK_GP_AXIS_RY));
        lt = SafeAxis01(NkGamepads().GetAxis(padIndex, NkGamepadAxis::NK_GP_AXIS_LT));
        rt = SafeAxis01(NkGamepads().GetAxis(padIndex, NkGamepadAxis::NK_GP_AXIS_RT));
        l3 = NkGamepads().IsButtonDown(padIndex, NkGamepadButton::NK_GP_LSTICK);
        r3 = NkGamepads().IsButtonDown(padIndex, NkGamepadButton::NK_GP_RSTICK);
    }

    DrawStick(renderer, cx - 150.f * s, cy + 145.f * s, 116.f * s, lx, ly, l3);
    DrawStick(renderer, cx + 150.f * s, cy + 145.f * s, 116.f * s, rx, ry, r3);

    // Barres analogiques L2/R2 pour calibration.
    const float barW = 120.f * s;
    const float barH = 10.f * s;
    RectI ltBar = MakeRect(cx - 230.f * s, cy - 246.f * s, barW, barH);
    RectI rtBar = MakeRect(cx + 230.f * s, cy - 246.f * s, barW, barH);
    FillRect(renderer, ltBar, idle);
    FillRect(renderer, rtBar, idle);

    RectI ltFill = ltBar;
    RectI rtFill = rtBar;
    ltFill.w = static_cast<int32>(SafeAxis01(lt) * barW);
    rtFill.w = static_cast<int32>(SafeAxis01(rt) * barW);
    FillRect(renderer, ltFill, Dim(NkRenderer::PackColor(255, 160, 70, 255), 1.0f));
    FillRect(renderer, rtFill, Dim(NkRenderer::PackColor(255, 160, 70, 255), 1.0f));
    StrokeRect(renderer, ltBar, outline);
    StrokeRect(renderer, rtBar, outline);

    if (!connected) {
        RectI info = MakeRect(cx, cy + 250.f * s, 360.f * s, 30.f * s);
        FillRect(renderer, info, NkRenderer::PackColor(90, 52, 52, 255));
        StrokeRect(renderer, info, outline);
    }
}

} // namespace

int nkmain(const nkentseu::NkEntryState& /*state*/) {
    using namespace nkentseu;

    logger.Named("SandboxGamepadPS3");
#if defined(_DEBUG) || defined(DEBUG) || defined(NKENTSEU_DEBUG)
    logger.Level(NkLogLevel::NK_DEBUG);
#else
    logger.Level(NkLogLevel::NK_INFO);
#endif
    uint32 bootStep = 0;
    auto bootLog = [&bootStep](const char* message) {
        const unsigned step = static_cast<unsigned>(++bootStep);
        logger.Infof("[BOOTSTEP %u] %s", step, message);
    };
    bootLog("logger.Named done");
    
    bootLog("runtime already initialized by entrypoint");

    NkContextInit();
    NkContextConfig contextConfig{};
    contextConfig.api = NkRendererApi::NK_SOFTWARE;
    contextConfig.vsync = true;
    contextConfig.debug = false;
    NkContextSetHints(contextConfig);
    contextConfig = NkContextGetHints();

    NkWindowConfig cfg;
    cfg.title = "Sandbox - Gamepad PS3 Validator";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.centered = true;
    cfg.resizable = true;
    cfg.dropEnabled = false;

    NkContextApplyWindowHints(cfg);
    NkWindow window(cfg);
    bootLog("NkWindow constructed");
    if (!window.IsOpen()) {
        bootLog("window.IsOpen == false");
        NkContextShutdown();
        return -2;
    }
    bootLog("window.IsOpen == true");

    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        bootLog("NkContextCreate failed");
        window.Close();
        NkContextShutdown();
        return -3;
    }

    NkRenderer renderer;
    NkRendererConfig rcfg;
    rcfg.api = NkRendererApi::NK_SOFTWARE;
    rcfg.autoResizeFramebuffer = true;
    
    if (!renderer.Create(window, rcfg)) {
        bootLog("renderer.Create failed");
        NkContextDestroy(context);
        window.Close();
        NkContextShutdown();
        return -4;
    }
    bootLog("renderer.Create done");

    bool running = true;
    bool autoPreset = false;
    bool invertStickY = false;
    NkPadProfile selectedProfile = NkPadProfile::AUTO;
    NkArray<bool, NK_MAX_GAMEPADS> presetApplied{};
    RawProbeState rawProbe{};
    
    NkChrono chrono;
    NkElapsedTime elapsed{};
    auto& events = NkEvents();
    auto& hidMapper = events.GetHidMapper();
    bootLog("NkEvents + hidMapper ready");

    // logger.Info("JSON: {{ \"id\": {0} }}", 42);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    constexpr bool kEnableRuntimeHidHooks = true;
#else
    constexpr bool kEnableRuntimeHidHooks = false;
#endif
    if (kEnableRuntimeHidHooks) {
        bootLog("before SetConnectCallback");
        
        NkGamepads().SetConnectCallback(
            [&presetApplied](const NkGamepadInfo& info, bool connected) {
                logger.Infof("[Gamepad] %s #%u (%.127s) vid=0x%04X pid=0x%04X type=%u",
                    connected ? "CONNECTED" : "DISCONNECTED",
                    info.index, info.name,
                    static_cast<unsigned>(info.vendorId),
                    static_cast<unsigned>(info.productId),
                    static_cast<unsigned>(info.type));

                if (info.index < NK_MAX_GAMEPADS && !connected) {
                    presetApplied[info.index] = false;
                }
            });
        bootLog("after SetConnectCallback");
            
        // Example of generic HID integration:
        // - map by deviceId on connect
        // - resolve logical indices from raw events
        bootLog("before AddEventCallback<NkHidConnectEvent>");
        events.AddEventCallback<NkHidConnectEvent>(
            [&hidMapper](NkHidConnectEvent* ev) {
                const NkHidDeviceInfo& info = ev->GetInfo();
                hidMapper.SetButtonMap(info.deviceId, 0, 0);
                hidMapper.SetAxisMap(info.deviceId, 0, 0, false, 1.f, 0.08f, 0.f);
                logger.Infof("[HID] CONNECT dev=%llu name=%.127s", static_cast<unsigned long long>(info.deviceId), info.name);
            });
        bootLog("after AddEventCallback<NkHidConnectEvent>");
            
        bootLog("before AddEventCallback<NkHidButtonPressEvent>");
        events.AddEventCallback<NkHidButtonPressEvent>(
            [&hidMapper](NkHidButtonPressEvent* ev) {
                uint32 logical = NK_HID_UNMAPPED;
                NkButtonState state = NkButtonState::NK_RELEASED;
                if (hidMapper.MapButtonEvent(*ev, logical, state)) {
                    logger.Infof("[HID] BUTTON dev=%llu phys=%u -> logical=%u",
                        static_cast<unsigned long long>(ev->GetDeviceId()),
                        ev->GetButtonIndex(), logical);
                }
            });
        bootLog("after AddEventCallback<NkHidButtonPressEvent>");
        
        bootLog("before AddEventCallback<NkHidAxisEvent>");
        events.AddEventCallback<NkHidAxisEvent>(
            [&hidMapper](NkHidAxisEvent* ev) {
                uint32 logicalAxis = NK_HID_UNMAPPED;
                float32 mappedValue = 0.f;
                if (hidMapper.MapAxisEvent(*ev, logicalAxis, mappedValue)) {
                    logger.Infof("[HID] AXIS dev=%llu phys=%u -> logical=%u val=%.3f",
                        static_cast<unsigned long long>(ev->GetDeviceId()),
                        ev->GetAxisIndex(), logicalAxis, mappedValue);
                }
            });
        bootLog("after AddEventCallback<NkHidAxisEvent>");
    }

    logger.Info("[PS3 Validator]");
    logger.Info("  F5: apply current profile remap");
    logger.Info("  F6: clear remap (identity)");
    logger.Info("  F7: toggle stick Y inversion");
    logger.Info("  F8: cycle profile (Auto/Xbox/PS4/PS3/Switch/Generic)");
    logger.Info("  ESC: quit");
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    logger.Info("  Web tip: click canvas, then press any gamepad button once");
#endif
    bootLog("entering main loop");

    constexpr bool kMinimalCloseDiag = false;
    constexpr bool kInputDiagLogs = true;
    if (kMinimalCloseDiag) {
        while (running && window.IsOpen()) {
            NkElapsedTime e = chrono.Reset();
            (void)e;

            while (NkEvent* ev = events.PollEvent()) {
                if (ev->Is<NkWindowCloseEvent>()) {
                    running = false;
                    break;
                }
                if constexpr (kInputDiagLogs) {
                    if (ev->Is<NkMouseEnterEvent>()) logger.Info("[MOUSE] enter");
                    if (ev->Is<NkMouseLeaveEvent>()) logger.Info("[MOUSE] leave");
                    if (auto* mb = ev->As<NkMouseButtonPressEvent>()) {
                        logger.Infof("[MOUSE] press btn=%s x=%d y=%d",
                            NkMouseButtonToString(mb->GetButton()),
                            mb->GetX(), mb->GetY());
                    }
                    if (auto* mb = ev->As<NkMouseButtonReleaseEvent>()) {
                        logger.Infof("[MOUSE] release btn=%s x=%d y=%d",
                            NkMouseButtonToString(mb->GetButton()),
                            mb->GetX(), mb->GetY());
                    }
                }
                if (auto* k = ev->As<NkKeyPressEvent>()) {
                    if (IsEscapePressed(k)) {
                        running = false;
                        break;
                    }
                }
                if (auto* r = ev->As<NkWindowResizeEvent>()) {
                    renderer.Resize(r->GetWidth(), r->GetHeight());
                }
            }

            if (!running || !window.IsOpen()) {
                break;
            }

            renderer.BeginFrame(NkRenderer::PackColor(14, 16, 22, 255));
            renderer.EndFrame();
            renderer.Present();

            elapsed = chrono.Elapsed();
            if (elapsed.milliseconds < 16) {
                NkChrono::Sleep(16 - elapsed.milliseconds);
            } else {
                NkChrono::YieldThread();
            }
        }

        goto shutdown_sequence;
    }

    while (running && window.IsOpen()) {
        NkElapsedTime e = chrono.Reset();
        (void)e;

        while (NkEvent* ev = events.PollEvent()) {

            if (ev->Is<NkWindowCloseEvent>()) {
                running = false;
                break;
            }
            if constexpr (kInputDiagLogs) {
                if (ev->Is<NkMouseEnterEvent>()) logger.Info("[MOUSE] enter");
                if (ev->Is<NkMouseLeaveEvent>()) logger.Info("[MOUSE] leave");
                if (auto* mb = ev->As<NkMouseButtonPressEvent>()) {
                    logger.Infof("[MOUSE] press btn=%s x=%d y=%d",
                        NkMouseButtonToString(mb->GetButton()),
                        mb->GetX(), mb->GetY());
                }
                if (auto* mb = ev->As<NkMouseButtonReleaseEvent>()) {
                    logger.Infof("[MOUSE] release btn=%s x=%d y=%d",
                        NkMouseButtonToString(mb->GetButton()),
                        mb->GetX(), mb->GetY());
                }
            }
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                if (IsEscapePressed(k)) {
                    running = false;
                    break;
                }
                switch (k->GetKey()) {
                    case NkKey::NK_F5:
                        autoPreset = true;
                        presetApplied.fill(false);
                        logger.Infof("[Mapping] profile=%s enabled", ProfileName(selectedProfile));
                        break;
                    case NkKey::NK_F6:
                        autoPreset = false;
                        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
                            NkGamepads().ClearMapping(i);
                            presetApplied[i] = false;
                        }
                        logger.Info("[Mapping] identity mapping restored");
                        break;
                    case NkKey::NK_F7:
                        invertStickY = !invertStickY;
                        presetApplied.fill(false);
                        logger.Infof("[Mapping] invert Y = %s", invertStickY ? "ON" : "OFF");
                        break;
                    case NkKey::NK_F8: {
                        uint32 next = static_cast<uint32>(selectedProfile) + 1u;
                        if (next >= static_cast<uint32>(NkPadProfile::COUNT)) next = 0u;
                        selectedProfile = static_cast<NkPadProfile>(next);
                        autoPreset = true;
                        presetApplied.fill(false);
                        logger.Infof("[Mapping] profile switched to %s", ProfileName(selectedProfile));
                        break;
                    }
                    default:
                        break;
                }
                if (!running) break;
            }
            if (auto* r = ev->As<NkWindowResizeEvent>()) {
                renderer.Resize(r->GetWidth(), r->GetHeight());
            }
        }

        if (!running || !window.IsOpen()) {
            break;
        }

        if (autoPreset) {
            for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
                if (!NkGamepads().IsConnected(i) || presetApplied[i]) continue;
                const NkGamepadInfo& info = NkGamepads().GetInfo(i);
                NkPadProfile resolved = selectedProfile;
                if (resolved == NkPadProfile::AUTO) {
                    resolved = DetectPadProfile(info);
                }

                ApplyPadProfileMapping(i, resolved, invertStickY);
                presetApplied[i] = true;
                logger.Infof("[Mapping] profile=%s applied on slot %u (%s)",
                    ProfileName(resolved), i, info.name);
            }
        }

        const uint32 pad = FindFirstConnectedGamepad();
        const bool connected = (pad < NK_MAX_GAMEPADS);
        constexpr bool kEnableRawProbe = true;
        if (kEnableRawProbe && connected) {
            ProbeRawInput(pad, rawProbe);
        }

        renderer.BeginFrame(NkRenderer::PackColor(14, 16, 22, 255));
        const NkFramebufferInfo& fb = renderer.GetFramebufferInfo();
        const uint32 w = fb.width ? fb.width : window.GetSize().x;
        const uint32 h = fb.height ? fb.height : window.GetSize().y;
        constexpr bool kEnablePs3Draw = true;
        if (kEnablePs3Draw) {
            DrawPs3Layout(renderer, w, h, connected, pad);
        }
        renderer.EndFrame();
        renderer.Present();

        elapsed = chrono.Elapsed();
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        // On Web, return control to the browser each frame so the canvas can present.
        emscripten_sleep(0);
#else
        if (elapsed.milliseconds < 16) {
            NkChrono::Sleep(16 - elapsed.milliseconds);
        } else {
            NkChrono::YieldThread();
        }
#endif
    }

shutdown_sequence:
    const NkElapsedTime shutdownStart = NkChrono::Now();
    auto shutdownMs = [&shutdownStart]() -> long long {
        return static_cast<long long>((NkChrono::Now() - shutdownStart).milliseconds);
    };

    logger.Infof("[SHUTDOWN] begin");
    renderer.Shutdown();
    logger.Infof("[SHUTDOWN] renderer.Shutdown +%lldms", shutdownMs());
    NkContextDestroy(context);
    logger.Infof("[SHUTDOWN] NkContextDestroy +%lldms", shutdownMs());
    window.Close();
    logger.Infof("[SHUTDOWN] window.Close +%lldms", shutdownMs());
    NkContextShutdown();
    logger.Infof("[SHUTDOWN] NkContextShutdown +%lldms", shutdownMs());
    logger.Infof("[SHUTDOWN] runtime shutdown handled by entrypoint +%lldms", shutdownMs());
    return 0;
}
