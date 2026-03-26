// =============================================================================
// main8.cpp
// Validation manette: layout type PlayStation 3 en rectangles.
// Chaque bouton allume son rectangle. Les sticks affichent aussi leur position.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkGamepadSystem.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Compute/NkIComputeContext.h"
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
    using namespace nkentseu::math;

    struct LoopCtx { 
        float dt = 0; 
        float time = 0; 
        bool running = true;
        bool hasFocus = true;           // État du focus
        bool isMinimized = false;       // État de minimisation
        bool isResizing = false;        // État de redimensionnement
        bool needsResize = false;       // Redimensionnement en attente
        uint32 pendingW = 0; 
        uint32 pendingH = 0;
        
        // Compteurs pour éviter les recréations trop fréquentes
        uint32 resizeAttempts = 0;
        uint32 focusLostFrames = 0;
    };

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
        NkColor offColor;
        NkColor onColor;
    };

    static RectI MakeRect(float centerX, float centerY, float width, float height) {
        RectI r{};
        r.w = static_cast<int32>(width);
        r.h = static_cast<int32>(height);
        r.x = static_cast<int32>(centerX) - r.w / 2;
        r.y = static_cast<int32>(centerY) - r.h / 2;
        return r;
    }

    static void FillRect(NkIGraphicsContext* ctx, const RectI& r, NkColor color) {
        auto* fb = NkNativeContext::GetSoftwareBackBuffer(ctx);

        for (int32 y = r.y; y < r.y + r.h; ++y) {
            for (int32 x = r.x; x < r.x + r.w; ++x) {
                fb->SetPixel(x, y, color.r, color.g, color.b, color.a);
            }
        }
    }

    static void StrokeRect(NkIGraphicsContext* ctx, const RectI& r, NkColor color) {
        auto* fb = NkNativeContext::GetSoftwareBackBuffer(ctx);
        
        for (int32 x = r.x; x < r.x + r.w; ++x) {
            fb->SetPixel(x, r.y, color.r, color.g, color.b, color.a);
            fb->SetPixel(x, r.y + r.h - 1, color.r, color.g, color.b, color.a);
        }
        for (int32 y = r.y; y < r.y + r.h; ++y) {
            fb->SetPixel(r.x, y, color.r, color.g, color.b, color.a);
            fb->SetPixel(r.x + r.w - 1, y, color.r, color.g, color.b, color.a);
        }
    }

    static void DrawStick(
        NkIGraphicsContext* ctx, float cx, float cy, float size,
        float ax, float ay, bool pressed)
    {
        ax = SafeAxis11(ax);
        ay = SafeAxis11(ay);
        const RectI zone = MakeRect(cx, cy, size, size);
        const NkColor zoneColor = pressed ? NkColor(110, 180, 255, 255) : NkColor(55, 55, 70, 255);
        FillRect(ctx, zone, zoneColor);
        StrokeRect(ctx, zone, NkColor(20, 20, 24, 255));

        const float travel = (size * 0.5f) - 8.0f;
        const float kx = ax;
        const float ky = ay;
        const float knobX = cx + kx * travel;
        const float knobY = cy - ky * travel; // Y+ = haut cAAtAA NK.

        const RectI knob = MakeRect(knobX, knobY, 14.0f, 14.0f);
        FillRect(ctx, knob, NkColor(245, 245, 245, 255));
        StrokeRect(ctx, knob, NkColor(30, 30, 30, 255));
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
        NkIGraphicsContext* ctx, uint32 width, uint32 height,
        bool connected, uint32 padIndex)
    {
        if (width == 0 || height == 0) return;
        const float s = math::NkMin(width / 1280.0f, height / 720.0f);
        const float cx = width * 0.5f;
        const float cy = height * 0.54f;

        const RectI body = MakeRect(cx, cy, 860.0f * s, 420.0f * s);
        FillRect(ctx, body, NkColor(26, 26, 36, 255));
        StrokeRect(ctx, body, NkColor(60, 60, 84, 255));

        const NkColor idle = NkColor(78, 78, 92, 255);
        const NkColor outline = NkColor(20, 20, 25, 255);

        const ButtonVisual buttons[] = {
            // D-Pad
            { NkGamepadButton::NK_GP_DPAD_UP,    -270.f, -28.f, 44.f, 34.f, idle, NkColor(120, 200, 255, 255) },
            { NkGamepadButton::NK_GP_DPAD_DOWN,  -270.f,  58.f, 44.f, 34.f, idle, NkColor(120, 200, 255, 255) },
            { NkGamepadButton::NK_GP_DPAD_LEFT,  -318.f,  15.f, 44.f, 34.f, idle, NkColor(120, 200, 255, 255) },
            { NkGamepadButton::NK_GP_DPAD_RIGHT, -222.f,  15.f, 44.f, 34.f, idle, NkColor(120, 200, 255, 255) },

            // Face buttons (PS style)
            { NkGamepadButton::NK_GP_NORTH,  270.f, -28.f, 44.f, 34.f, idle, NkColor(90, 220, 90, 255)  }, // Triangle
            { NkGamepadButton::NK_GP_SOUTH,  270.f,  58.f, 44.f, 34.f, idle, NkColor(80, 150, 255, 255) }, // Cross
            { NkGamepadButton::NK_GP_WEST,   222.f,  15.f, 44.f, 34.f, idle, NkColor(240, 120, 220, 255)}, // Square
            { NkGamepadButton::NK_GP_EAST,   318.f,  15.f, 44.f, 34.f, idle, NkColor(255, 110, 110, 255)}, // Circle

            // Shoulders / triggers
            { NkGamepadButton::NK_GP_LB,        -230.f, -166.f, 120.f, 32.f, idle, NkColor(255, 180, 80, 255) },
            { NkGamepadButton::NK_GP_LT_DIGITAL,-230.f, -210.f, 120.f, 32.f, idle, NkColor(255, 140, 60, 255) },
            { NkGamepadButton::NK_GP_RB,         230.f, -166.f, 120.f, 32.f, idle, NkColor(255, 180, 80, 255) },
            { NkGamepadButton::NK_GP_RT_DIGITAL, 230.f, -210.f, 120.f, 32.f, idle, NkColor(255, 140, 60, 255) },

            // Center buttons
            { NkGamepadButton::NK_GP_BACK,   -72.f,  12.f, 84.f, 30.f, idle, NkColor(180, 180, 180, 255) }, // Select
            { NkGamepadButton::NK_GP_START,   72.f,  12.f, 84.f, 30.f, idle, NkColor(180, 180, 180, 255) }, // Start
            { NkGamepadButton::NK_GP_GUIDE,    0.f, -26.f, 72.f, 30.f, idle, NkColor(120, 255, 140, 255) }, // PS
        };

        for (const ButtonVisual& b : buttons) {
            const bool down = connected && NkGamepads().IsButtonDown(padIndex, b.button);
            RectI r = MakeRect(cx + b.cx * s, cy + b.cy * s, b.w * s, b.h * s);
            FillRect(ctx, r, down ? b.onColor : b.offColor);
            StrokeRect(ctx, r, outline);
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

        DrawStick(ctx, cx - 150.f * s, cy + 145.f * s, 116.f * s, lx, ly, l3);
        DrawStick(ctx, cx + 150.f * s, cy + 145.f * s, 116.f * s, rx, ry, r3);

        // Barres analogiques L2/R2 pour calibration.
        const float barW = 120.f * s;
        const float barH = 10.f * s;
        RectI ltBar = MakeRect(cx - 230.f * s, cy - 246.f * s, barW, barH);
        RectI rtBar = MakeRect(cx + 230.f * s, cy - 246.f * s, barW, barH);
        FillRect(ctx, ltBar, idle);
        FillRect(ctx, rtBar, idle);

        RectI ltFill = ltBar;
        RectI rtFill = rtBar;
        ltFill.w = static_cast<int32>(SafeAxis01(lt) * barW);
        rtFill.w = static_cast<int32>(SafeAxis01(rt) * barW);
        FillRect(ctx, ltFill, NkColor(255, 160, 70, 255));
        FillRect(ctx, rtFill, NkColor(255, 160, 70, 255));
        StrokeRect(ctx, ltBar, outline);
        StrokeRect(ctx, rtBar, outline);

        if (!connected) {
            RectI info = MakeRect(cx, cy + 250.f * s, 360.f * s, 30.f * s);
            FillRect(ctx, info, NkColor(90, 52, 52, 255));
            StrokeRect(ctx, info, outline);
        }
    }

} // namespace

int nkmain(const nkentseu::NkEntryState& /*state*/) {
    using namespace nkentseu;
    
    NkWindowConfig cfg;
    cfg.title = "Sandbox - Gamepad PS3 Validator";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.centered = true;
    cfg.resizable = true;
    cfg.dropEnabled = false;

    NkWindow window;
    
    if (!window.Create(cfg)) {
        return -2;
    }

    auto desc = NkContextDesc::MakeSoftware(true);
    NkIGraphicsContext* ctx = NkContextFactory::Create(window, desc);
    if (!ctx) { 
        logger.Errorf("[SW] Context failed"); 
        return -1; 
    }

    nkentseu::NkSoftwareFramebuffer* framebuffer = NkNativeContext::GetSoftwareBackBuffer(ctx);

    bool autoPreset = false;
    bool invertStickY = false;
    NkPadProfile selectedProfile = NkPadProfile::AUTO;
    NkArray<bool, NK_MAX_GAMEPADS> presetApplied{};
    RawProbeState rawProbe{};
    
    NkChrono chrono;
    NkElapsedTime elapsed{};
    auto& events = NkEvents();
    auto& hidMapper = events.GetHidMapper();

    // logger.Info("JSON: {{ \"id\": {0} }}", 42);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    constexpr bool kEnableRuntimeHidHooks = true;
#else
    constexpr bool kEnableRuntimeHidHooks = false;
#endif
    if (kEnableRuntimeHidHooks) {
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
            
        events.AddEventCallback<NkHidConnectEvent>(
            [&hidMapper](NkHidConnectEvent* ev) {
                const NkHidDeviceInfo& info = ev->GetInfo();
                hidMapper.SetButtonMap(info.deviceId, 0, 0);
                hidMapper.SetAxisMap(info.deviceId, 0, 0, false, 1.f, 0.08f, 0.f);
                logger.Infof("[HID] CONNECT dev=%llu name=%.127s", static_cast<unsigned long long>(info.deviceId), info.name);
            });
            
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

    constexpr bool kInputDiagLogs = true;
    LoopCtx lc;

    while (lc.running) {
        NkElapsedTime e = chrono.Reset();
        (void)e;

        while (NkEvent* ev = events.PollEvent()) {

            if (ev->Is<NkWindowCloseEvent>()) {
                lc.running = false;
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
                    lc.running = false;
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
                if (!lc.running) break;
            }
            
            // ── Gestion du focus (NOUVEAU) ─────────────────────────────────
            if (auto* focusEv = ev->As<NkWindowFocusLostEvent>()) {
                lc.hasFocus = false;
                lc.focusLostFrames++;
                logger.Debug("[Focus] Window lost focus");
            }
            if (auto* focusEv = ev->As<NkWindowFocusGainedEvent>()) {
                lc.hasFocus = true;
                lc.focusLostFrames = 0;
                logger.Debug("[Focus] Window gained focus");
                
                // Quand on regagne le focus, planifier un resize si nécessaire
                if (!lc.isMinimized) {
                    math::NkVec2u sz = window.GetSize();
                    if (sz.x > 0 && sz.y > 0) {
                        lc.pendingW = sz.x;
                        lc.pendingH = sz.y;
                        lc.needsResize = true;
                    }
                }
            }
            
            // ── Gestion de la minimisation ─────────────────────────────────
            if (ev->Is<NkWindowMinimizeEvent>()) {
                lc.isMinimized = true;
                lc.hasFocus = false;  // La fenêtre perd le focus quand minimisée
                lc.pendingW = lc.pendingH = 0;
                logger.Debug("[Window] Minimized");
            } 
            else if (ev->Is<NkWindowMaximizeEvent>() || ev->Is<NkWindowRestoreEvent>()) {
                lc.isMinimized = false;
                // Sur restauration, la fenêtre va regagner le focus
                // On planifie un resize mais on attend l'événement de focus
                math::NkVec2u sz = window.GetSize();
                if (sz.x > 0 && sz.y > 0) { 
                    lc.pendingW = sz.x; 
                    lc.pendingH = sz.y;
                    lc.needsResize = true;
                }
                logger.Debug("[Window] Restored/Maximized");
            }
            
            // ── Gestion du redimensionnement ──────────────────────────────
            else if (ev->Is<NkWindowResizeBeginEvent>()) {
                lc.isResizing = true;
                lc.pendingW = lc.pendingH = 0;
                lc.resizeAttempts = 0;
                logger.Debug("[Window] Resize begin");
            } 
            else if (ev->Is<NkWindowResizeEndEvent>()) {
                lc.isResizing = false;
                logger.Debug("[Window] Resize end");
            } 
            else if (auto* resize = ev->As<NkWindowResizeEvent>()) {
                if (resize->GetWidth() > 0 && resize->GetHeight() > 0) {
                    lc.pendingW = resize->GetWidth();
                    lc.pendingH = resize->GetHeight();
                    lc.needsResize = true;
                    logger.Debug("[Window] Resize to %ux%u", lc.pendingW, lc.pendingH);
                }
            }
        }

        if (!lc.running || !window.IsOpen()) {
            break;
        }
        
        // 1. Si la fenêtre est minimisée → pas de rendu
        if (lc.isMinimized) {
            NkChrono::SleepMilliseconds(16);
            continue;
        }
        
        // 2. Si la fenêtre n'a pas le focus, on peut réduire la cadence de rendu
        if (!lc.hasFocus) {
            // Optionnel : réduire le framerate quand la fenêtre n'est pas au premier plan
            if (lc.focusLostFrames > 0 && (lc.focusLostFrames % 10) == 0) {
                // Toutes les 10 frames sans focus, on fait une micro-pause
                NkChrono::SleepMilliseconds(1);
            }
        }
        
        // 3. Appliquer le redimensionnement quand approprié
        //    Important : ne pas recréer la swapchain pendant le redimensionnement
        //    et seulement si on a des dimensions valides
        if (!lc.isResizing && lc.needsResize && lc.pendingW > 0 && lc.pendingH > 0) {
            // Limiter le nombre de tentatives pour éviter les boucles infinies
            if (lc.resizeAttempts < 5) {
                if (ctx->OnResize(lc.pendingW, lc.pendingH)) {
                    lc.pendingW = lc.pendingH = 0;
                    lc.needsResize = false;
                    lc.resizeAttempts = 0;
                    // Laisser le temps à la swapchain de se stabiliser
                    NkChrono::SleepMilliseconds(16);
                } else {
                    lc.resizeAttempts++;
                    // Attendre un peu avant de réessayer
                    NkChrono::SleepMilliseconds(16 * lc.resizeAttempts);
                }
            } else {
                // Trop d'échecs, on abandonne pour cette fois
                logger.Warn("[Window] Too many resize failures, resetting");
                lc.needsResize = false;
                lc.resizeAttempts = 0;
                lc.pendingW = lc.pendingH = 0;
            }
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

        if (!ctx->BeginFrame()) {
            // Si BeginFrame échoue, on planifie un resize si nécessaire
            if (lc.pendingW == 0 || lc.pendingH == 0) {
                math::NkVec2u sz = window.GetSize();
                if (sz.x > 0 && sz.y > 0) {
                    lc.pendingW = sz.x;
                    lc.pendingH = sz.y;
                    lc.needsResize = true;
                }
            }
            NkChrono::SleepMilliseconds(16);
            continue;
        }

        constexpr bool kEnablePs3Draw = true;
        if (kEnablePs3Draw) {
            DrawPs3Layout(ctx, window.GetSize().x, window.GetSize().y, connected, pad);
        }
        ctx->EndFrame();
        ctx->Present();

        elapsed = chrono.Elapsed();
        
        if (elapsed.milliseconds < 16) {
            NkChrono::Sleep(16 - elapsed.milliseconds);
        } else {
            NkChrono::YieldThread();
        }
    }

shutdown_sequence:
    const NkElapsedTime shutdownStart = NkChrono::Now();
    auto shutdownMs = [&shutdownStart]() -> long long {
        return static_cast<long long>((NkChrono::Now() - shutdownStart).milliseconds);
    };

    logger.Infof("[SHUTDOWN] begin");
    ctx->Shutdown();
    logger.Infof("[SHUTDOWN] NkContextDestroy +%lldms", shutdownMs());
    window.Close();
    logger.Infof("[SHUTDOWN] window.Close +%lldms", shutdownMs());
    NkContextShutdown();
    logger.Infof("[SHUTDOWN] NkContextShutdown +%lldms", shutdownMs());
    logger.Infof("[SHUTDOWN] runtime shutdown handled by entrypoint +%lldms", shutdownMs());
    return 0;
}
