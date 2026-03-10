#pragma once

// =============================================================================
// NkWin32Gamepad.h
// Backend gamepad Win32 — double backend XInput + DirectInput.
//
// Stratégie :
//   1. XInput  — manettes Xbox (360 / One / Series). O(1), simple, limité à 4.
//   2. DirectInput — tout le reste (joysticks, volants, HOTAS, manettes HID
//      génériques). Utilisé uniquement pour les périphériques NON-XInput.
//
// Détection XInput vs DirectInput :
//   On énumère les périphériques DirectInput. Pour chacun, on vérifie si son
//   VID/PID correspond à un contrôleur XInput connu (via le registre ou la
//   liste statique). Si oui, on le saute côté DirectInput : XInput s'en charge.
//
// Slots :
//   [0..3]  → XInput (DWORD 0..3)
//   [4..NK_MAX_GAMEPADS-1] → DirectInput (ordre d'énumération)
//
// Dépendances :
//   xinput.lib, dinput8.lib, dxguid.lib (via #pragma comment)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef DIRECTINPUT_VERSION
#   define DIRECTINPUT_VERSION 0x0800
#endif

#include <windows.h>
#include <xinput.h>
#include <dinput.h>
#include <wbemidl.h>   // pour la détection XInput via WMI (optionnel, voir IsXInputDevice)
#include <oleauto.h>

#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include "NKWindow/Events/NkGamepadEvent.h"
#include "NKWindow/Events/NkGamepadSystem.h"

#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>

namespace nkentseu {

    // =========================================================================
    // Constantes
    // =========================================================================

    /// Nombre de slots XInput (fixé par l'API)
    static constexpr uint32 NK_XINPUT_MAX   = 4u;
    /// Slots DirectInput = total - XInput
    static constexpr uint32 NK_DI_MAX       = NK_MAX_GAMEPADS - NK_XINPUT_MAX;
    /// Deadzone XInput normalisée (SHORT → float)
    static constexpr SHORT NK_XI_DEADZONE_L = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    static constexpr SHORT NK_XI_DEADZONE_R = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
    static constexpr BYTE  NK_XI_TRIGGER_DZ = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

    // =========================================================================
    // Helpers XInput
    // =========================================================================

    static inline float NkXI_NormAxis(SHORT raw) noexcept {
        return raw >= 0
            ? static_cast<float>(raw) / 32767.f
            : static_cast<float>(raw) / 32768.f;
    }

    static inline float NkXI_Deadzone(SHORT raw, SHORT dz) noexcept {
        return (std::abs(raw) < dz) ? 0.f : NkXI_NormAxis(raw);
    }

    static inline float NkXI_Trigger(BYTE raw, BYTE dz) noexcept {
        return (raw < dz) ? 0.f : static_cast<float>(raw - dz) / (255.f - dz);
    }

    // =========================================================================
    // Détection XInput via VID/PID
    //
    // Certains contrôleurs Xbox apparaissent aussi dans l'énumération DirectInput.
    // On les exclut côté DirectInput pour éviter les doublons.
    // Méthode : comparer le GUID de produit DInput avec la liste des VID Xbox
    // connus. Le GUID produit DirectInput encode VID dans les octets [0..1] et
    // PID dans les octets [2..3] (little-endian).
    // =========================================================================

    static bool NkWin32IsXInputVID(WORD vid) noexcept {
        // VIDs connus pour les contrôleurs Xbox / XInput
        static const WORD kXInputVIDs[] = {
            0x045E, // Microsoft
            0x0738, // Mad Catz (XInput)
            0x0E6F, // PDP (XInput)
            0x1532, // Razer (XInput)
            0x24C6, // PowerA (XInput)
            0x1BAD, // Harmonix (XInput)
        };
        for (WORD v : kXInputVIDs)
            if (vid == v) return true;
        return false;
    }

    /// Retourne true si le GUID produit DirectInput correspond à un device XInput
    static bool NkWin32IsXInputGUID(const GUID& productGuid) noexcept {
        // GUID produit DInput : {VVVVPPPP-...}
        // Les 2 premiers octets du Data1 = PID, les 2 suivants = VID (little-endian)
        WORD vid = static_cast<WORD>((productGuid.Data1 >> 16) & 0x0000FFFF);
        return NkWin32IsXInputVID(vid);
    }

    // =========================================================================
    // NkDIDeviceState — état brut d'un périphérique DirectInput
    // =========================================================================

    struct NkDIDeviceContext {
        IDirectInputDevice8W* device    = nullptr;
        GUID                  guidInstance{};
        GUID                  guidProduct{};
        char                  name[128] = {};
        uint32                 numButtons = 0;
        uint32                 numAxes    = 0;
        bool                  acquired   = false;

        // État brut DIJOYSTATE2
        DIJOYSTATE2 state{};
        DIJOYSTATE2 prevState{};
    };

    // =========================================================================
    // Callback DirectInput : collecte les axes disponibles
    // =========================================================================

    struct NkDIAxisCollector {
        IDirectInputDevice8W* device;
        uint32                 axisCount = 0;
    };

    static BOOL CALLBACK NkDIEnumAxesCallback(const DIDEVICEOBJECTINSTANCEW* doi,
                                               LPVOID pvRef) noexcept
    {
        auto* col = static_cast<NkDIAxisCollector*>(pvRef);
        if (doi->dwType & DIDFT_AXIS) {
            DIPROPRANGE range{};
            range.diph.dwSize       = sizeof(DIPROPRANGE);
            range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            range.diph.dwHow        = DIPH_BYID;
            range.diph.dwObj        = doi->dwType;
            range.lMin              = -32768;
            range.lMax              =  32767;
            col->device->SetProperty(DIPROP_RANGE, &range.diph);
            ++col->axisCount;
        }
        return DIENUM_CONTINUE;
    }

    // =========================================================================
    // Callback DirectInput : énumération des périphériques joystick
    // =========================================================================

    struct NkDIEnumCtx {
        IDirectInput8W*                  di8    = nullptr;
        std::vector<NkDIDeviceContext>*  out    = nullptr;
        uint32                            maxOut = NK_DI_MAX;
    };

    static BOOL CALLBACK NkDIEnumDevicesCallback(const DIDEVICEINSTANCEW* ddi,
                                                   LPVOID pvRef) noexcept
    {
        auto* ctx = static_cast<NkDIEnumCtx*>(pvRef);
        if (!ctx || !ctx->out) return DIENUM_STOP;
        if (ctx->out->size() >= ctx->maxOut) return DIENUM_STOP;

        // Exclure les périphériques XInput (ils sont gérés par le slot XInput)
        if (NkWin32IsXInputGUID(ddi->guidProduct)) return DIENUM_CONTINUE;

        NkDIDeviceContext dc{};
        dc.guidInstance = ddi->guidInstance;
        dc.guidProduct  = ddi->guidProduct;
        WideCharToMultiByte(CP_UTF8, 0,
            ddi->tszProductName, -1,
            dc.name, static_cast<int>(sizeof(dc.name)),
            nullptr, nullptr);

        IDirectInputDevice8W* dev = nullptr;
        HRESULT hr = ctx->di8->CreateDevice(ddi->guidInstance, &dev, nullptr);
        if (FAILED(hr) || !dev) return DIENUM_CONTINUE;

        // Format de données
        hr = dev->SetDataFormat(&c_dfDIJoystick2);
        if (FAILED(hr)) { dev->Release(); return DIENUM_CONTINUE; }

        // Mode coopératif non-exclusif (partage avec d'autres apps)
        hr = dev->SetCooperativeLevel(GetForegroundWindow(),
                                       DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        if (FAILED(hr)) { dev->Release(); return DIENUM_CONTINUE; }

        // Configurer les plages d'axes
        NkDIAxisCollector col{ dev, 0 };
        dev->EnumObjects(NkDIEnumAxesCallback, &col, DIDFT_AXIS);
        dc.numAxes = col.axisCount;

        // Compter les boutons
        DIDEVCAPS caps{};
        caps.dwSize = sizeof(DIDEVCAPS);
        if (SUCCEEDED(dev->GetCapabilities(&caps)))
            dc.numButtons = caps.dwButtons;

        dev->Acquire();
        dc.device   = dev;
        dc.acquired = true;

        ctx->out->push_back(dc);
        return DIENUM_CONTINUE;
    }

    // =========================================================================
    // Mapping DirectInput → NkGamepadSnapshot
    //
    // DIJOYSTATE2 axes :
    //   lX/lY        → stick gauche ([-32768..32767])
    //   lZ           → gâchette gauche (certains périphériques)
    //   lRx/lRy      → stick droit
    //   lRz          → gâchette droite (certains périphériques)
    //   rglSlider[0] → alternative pour LT
    //   rglSlider[1] → alternative pour RT
    //   rgdwPOV[0]   → D-pad (en centièmes de degrés, 0xFFFFFFFF = centré)
    // =========================================================================

    static float NkDI_NormAxis(LONG raw) noexcept {
        // DInput axe dans [-32768, 32767]
        return raw >= 0
            ? static_cast<float>(raw) / 32767.f
            : static_cast<float>(raw) / 32768.f;
    }

    static void NkDI_FillSnapshot(const NkDIDeviceContext& dc,
                                   uint32 slotIndex,
                                   NkGamepadSnapshot& s) noexcept
    {
        const DIJOYSTATE2& st = dc.state;
        const uint32 logicalBtnCount = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
        const uint32 logicalAxisCount = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
        const uint32 rawBtnBase = logicalBtnCount;
        const uint32 rawAxisBase = logicalAxisCount;

        NkGamepadInfo info = s.info;
        s.Clear();
        s.info = info;
        s.connected       = true;
        s.info.index      = slotIndex;
        s.info.type       = NkGamepadType::NK_GP_TYPE_GENERIC;
        s.info.numButtons = dc.numButtons;
        s.info.numAxes    = dc.numAxes;

        // Sticks (X/Y = gauche).
        const float lx = NkDI_NormAxis(st.lX);
        const float ly = NkDI_NormAxis(st.lY);
        float rx = NkDI_NormAxis(st.lRx);
        float ry = NkDI_NormAxis(st.lRy);

        // Certains pads HID exposent le stick droit sur Z/Rz.
        const float z  = NkDI_NormAxis(st.lZ);
        const float rz = NkDI_NormAxis(st.lRz);
        if ((std::fabs(rx) < 0.01f && std::fabs(ry) < 0.01f) &&
            (std::fabs(z) > 0.05f || std::fabs(rz) > 0.05f))
        {
            rx = z;
            ry = rz;
        }

        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LX)] = lx;
        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LY)] = ly;
        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RX)] = rx;
        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RY)] = ry;

        // Gâchettes : privilégier lZ/lRz, fallback sliders si pas d'activité.
        float lt = z;
        float rt = rz;
        if ((std::fabs(lt) < 0.01f && std::fabs(rt) < 0.01f) &&
            (st.rglSlider[0] != 0 || st.rglSlider[1] != 0))
        {
            lt = NkDI_NormAxis(st.rglSlider[0]);
            rt = NkDI_NormAxis(st.rglSlider[1]);
        }
        // Axe centré [-1,+1] -> trigger [0,+1]
        if (lt < 0.f) lt = (lt + 1.f) * 0.5f;
        if (rt < 0.f) rt = (rt + 1.f) * 0.5f;
        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT)] = std::clamp(lt, 0.f, 1.f);
        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT)] = std::clamp(rt, 0.f, 1.f);

        // Canal brut: exposer les boutons physiques DI en indices étendus.
        const uint32 physicalButtonCount =
            std::min<uint32>(dc.numButtons, static_cast<uint32>(sizeof(st.rgbButtons) / sizeof(st.rgbButtons[0])));
        for (uint32 b = 0; b < physicalButtonCount; ++b) {
            const bool down = (st.rgbButtons[b] & 0x80) != 0;
            const uint32 rawIndex = rawBtnBase + b;
            if (rawIndex < NkGamepadSystem::BUTTON_COUNT) {
                s.buttons[rawIndex] = down;
            }
        }

        // Compatibilité: mapping générique DI [0..] -> layout NK depuis SOUTH.
        const uint32 firstBtn = static_cast<uint32>(NkGamepadButton::NK_GP_SOUTH);
        const uint32 mapCount = (logicalBtnCount > firstBtn) ? (logicalBtnCount - firstBtn) : 0u;
        uint32 btnMax = std::min(dc.numButtons, mapCount);
        for (uint32 b = 0; b < btnMax; ++b)
            s.buttons[firstBtn + b] = (st.rgbButtons[b] & 0x80) != 0;

        // D-pad (POV hat) → boutons D-pad discrets
        DWORD pov = st.rgdwPOV[0];
        bool povValid = (pov != 0xFFFFFFFF);
        if (povValid) {
            float deg = static_cast<float>(pov) / 100.f; // centièmes → degrés
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)]    = (deg >= 315.f || deg < 45.f);
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] = (deg >= 45.f  && deg < 135.f);
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)]  = (deg >= 135.f && deg < 225.f);
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)]  = (deg >= 225.f && deg < 315.f);
        } else {
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)]    = false;
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] = false;
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)]  = false;
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)]  = false;
        }

        // Axes D-pad analogiques
        const float dpadX =
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] ? 1.f :
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)]  ? -1.f : 0.f;
        const float dpadY =
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)]   ? 1.f :
            s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)] ? -1.f : 0.f;

        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X)] = dpadX;
        s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y)] = dpadY;

        // Canal brut: axes physiques DI en indices étendus.
        const LONG physicalAxes[] = {
            st.lX, st.lY, st.lZ, st.lRx, st.lRy, st.lRz,
            st.rglSlider[0], st.rglSlider[1]
        };
        const uint32 physicalAxisCount =
            static_cast<uint32>(sizeof(physicalAxes) / sizeof(physicalAxes[0]));

        for (uint32 a = 0; a < physicalAxisCount; ++a) {
            const uint32 rawIndex = rawAxisBase + a;
            if (rawIndex >= NkGamepadSystem::AXIS_COUNT) break;
            s.axes[rawIndex] = NkDI_NormAxis(physicalAxes[a]);
        }

        if (rawAxisBase + physicalAxisCount + 1u < NkGamepadSystem::AXIS_COUNT) {
            s.axes[rawAxisBase + physicalAxisCount] = dpadX;
            s.axes[rawAxisBase + physicalAxisCount + 1u] = dpadY;
        }
    }

    // =========================================================================
    // NkWin32Gamepad — backend complet XInput + DirectInput
    // =========================================================================

    class NkWin32Gamepad final : public NkIGamepad {
    public:
        NkWin32Gamepad()  = default;
        ~NkWin32Gamepad() override { Shutdown(); }

        // =====================================================================
        // Init
        // =====================================================================

        bool Init() override {
            if (mInitialised) return true;

            // Initialiser les snapshots
            for (auto& s : mSnapshots) s.Clear();

            // DirectInput8
            HRESULT hr = DirectInput8Create(
                GetModuleHandleW(nullptr),
                DIRECTINPUT_VERSION,
                IID_IDirectInput8W,
                reinterpret_cast<void**>(&mDI8),
                nullptr);
            if (FAILED(hr)) {
                // DInput non disponible — on fonctionne en XInput seul
                mDI8 = nullptr;
            }

            // Énumérer les périphériques DirectInput (hors XInput)
            if (mDI8) RefreshDIDevices();

            mInitialised = true;
            return true;
        }

        // =====================================================================
        // Shutdown
        // =====================================================================

        void Shutdown() override {
            if (!mInitialised) return;

            // Couper les vibrations XInput
            XINPUT_VIBRATION vz{};
            for (DWORD i = 0; i < NK_XINPUT_MAX; ++i)
                XInputSetState(i, &vz);

            // Libérer les devices DirectInput
            for (auto& dc : mDIDevices) {
                if (dc.device) {
                    dc.device->Unacquire();
                    dc.device->Release();
                    dc.device = nullptr;
                }
            }
            mDIDevices.clear();

            if (mDI8) { mDI8->Release(); mDI8 = nullptr; }

            mInitialised = false;
        }

        // =====================================================================
        // Poll — XInput + DirectInput
        // =====================================================================

        void Poll() override {
            if (!mInitialised) return;
            PollXInput();
            PollDirectInput();
        }

        // =====================================================================
        // GetConnectedCount
        // =====================================================================

        uint32 GetConnectedCount() const override {
            uint32 n = 0;
            for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i)
                if (mSnapshots[i].connected) ++n;
            return n;
        }

        // =====================================================================
        // GetSnapshot
        // =====================================================================

        const NkGamepadSnapshot& GetSnapshot(uint32 idx) const override {
            static NkGamepadSnapshot sDummy;
            return (idx < NK_MAX_GAMEPADS) ? mSnapshots[idx] : sDummy;
        }

        // =====================================================================
        // Rumble
        // XInput : moteurs L/R.
        // DirectInput : Force Feedback — non implémenté ici (complexe, rare).
        // =====================================================================

        void Rumble(uint32 idx,
                    float motorLow, float motorHigh,
                    float /*triggerLeft*/, float /*triggerRight*/,
                    uint32 /*durationMs*/) override
        {
            if (idx < NK_XINPUT_MAX) {
                // Slot XInput
                XINPUT_VIBRATION v{};
                v.wLeftMotorSpeed  = static_cast<WORD>(std::min(motorLow,  1.f) * 65535.f);
                v.wRightMotorSpeed = static_cast<WORD>(std::min(motorHigh, 1.f) * 65535.f);
                XInputSetState(static_cast<DWORD>(idx), &v);
            }
            // DirectInput Force Feedback : ignoré silencieusement
        }

        // =====================================================================
        // GetName
        // =====================================================================

        const char* GetName() const noexcept override {
            return "Win32 (XInput + DirectInput8)";
        }

    private:
        bool                       mInitialised = false;
        IDirectInput8W*            mDI8         = nullptr;

        // Snapshots unifiés [0..3]=XInput, [4..7]=DirectInput
        std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};

        // Contextes DirectInput
        std::vector<NkDIDeviceContext> mDIDevices;

        // =====================================================================
        // PollXInput — slots 0..3
        // =====================================================================

        void PollXInput() {
            for (DWORD i = 0; i < NK_XINPUT_MAX; ++i) {
                XINPUT_STATE xs{};
                bool wasConnected = mSnapshots[i].connected;
                bool isConnected  = (XInputGetState(i, &xs) == ERROR_SUCCESS);

                mSnapshots[i].connected = isConnected;

                if (isConnected) {
                    FillXInputSnapshot(xs.Gamepad, i, mSnapshots[i]);
                    FillXInputBattery(i, mSnapshots[i]);
                    if (!wasConnected)
                        FillXInputInfo(i, mSnapshots[i].info);
                } else {
                    NkGamepadInfo saved = mSnapshots[i].info;
                    mSnapshots[i].Clear();
                    mSnapshots[i].info = saved;
                }
            }
        }

        // =====================================================================
        // FillXInputSnapshot
        // =====================================================================

        static void FillXInputSnapshot(const XINPUT_GAMEPAD& xp,
                                        uint32 idx,
                                        NkGamepadSnapshot& s) noexcept
        {
            using B = NkGamepadButton;
            using A = NkGamepadAxis;

            s.connected  = true;
            s.info.index = idx;

            auto btn = [&](B b, WORD mask) {
                s.buttons[static_cast<uint32>(b)] = (xp.wButtons & mask) != 0;
            };

            btn(B::NK_GP_SOUTH,      XINPUT_GAMEPAD_A);
            btn(B::NK_GP_EAST,       XINPUT_GAMEPAD_B);
            btn(B::NK_GP_WEST,       XINPUT_GAMEPAD_X);
            btn(B::NK_GP_NORTH,      XINPUT_GAMEPAD_Y);
            btn(B::NK_GP_LB,         XINPUT_GAMEPAD_LEFT_SHOULDER);
            btn(B::NK_GP_RB,         XINPUT_GAMEPAD_RIGHT_SHOULDER);
            btn(B::NK_GP_LSTICK,     XINPUT_GAMEPAD_LEFT_THUMB);
            btn(B::NK_GP_RSTICK,     XINPUT_GAMEPAD_RIGHT_THUMB);
            btn(B::NK_GP_BACK,       XINPUT_GAMEPAD_BACK);
            btn(B::NK_GP_START,      XINPUT_GAMEPAD_START);
            btn(B::NK_GP_DPAD_UP,    XINPUT_GAMEPAD_DPAD_UP);
            btn(B::NK_GP_DPAD_DOWN,  XINPUT_GAMEPAD_DPAD_DOWN);
            btn(B::NK_GP_DPAD_LEFT,  XINPUT_GAMEPAD_DPAD_LEFT);
            btn(B::NK_GP_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT);

            float lt = NkXI_Trigger(xp.bLeftTrigger,  NK_XI_TRIGGER_DZ);
            float rt = NkXI_Trigger(xp.bRightTrigger, NK_XI_TRIGGER_DZ);
            s.buttons[static_cast<uint32>(B::NK_GP_LT_DIGITAL)] = (lt > 0.5f);
            s.buttons[static_cast<uint32>(B::NK_GP_RT_DIGITAL)] = (rt > 0.5f);

            auto ax = [&](A a, float v) { s.axes[static_cast<uint32>(a)] = v; };
            ax(A::NK_GP_AXIS_LX, NkXI_Deadzone(xp.sThumbLX, NK_XI_DEADZONE_L));
            ax(A::NK_GP_AXIS_LY, NkXI_Deadzone(xp.sThumbLY, NK_XI_DEADZONE_L));
            ax(A::NK_GP_AXIS_RX, NkXI_Deadzone(xp.sThumbRX, NK_XI_DEADZONE_R));
            ax(A::NK_GP_AXIS_RY, NkXI_Deadzone(xp.sThumbRY, NK_XI_DEADZONE_R));
            ax(A::NK_GP_AXIS_LT, lt);
            ax(A::NK_GP_AXIS_RT, rt);

            // D-pad axes analogiques (pour cohérence avec DInput)
            ax(A::NK_GP_AXIS_DPAD_X,
               s.buttons[static_cast<uint32>(B::NK_GP_DPAD_RIGHT)] ? 1.f :
               s.buttons[static_cast<uint32>(B::NK_GP_DPAD_LEFT)]  ? -1.f : 0.f);
            ax(A::NK_GP_AXIS_DPAD_Y,
               s.buttons[static_cast<uint32>(B::NK_GP_DPAD_UP)]   ? 1.f :
               s.buttons[static_cast<uint32>(B::NK_GP_DPAD_DOWN)] ? -1.f : 0.f);
        }

        static void FillXInputBattery(DWORD idx, NkGamepadSnapshot& s) noexcept {
            XINPUT_BATTERY_INFORMATION bi{};
            if (XInputGetBatteryInformation(idx, BATTERY_DEVTYPE_GAMEPAD, &bi) != ERROR_SUCCESS)
                return;
            s.isCharging = false;
            switch (bi.BatteryLevel) {
                case BATTERY_LEVEL_EMPTY:  s.batteryLevel = 0.00f; break;
                case BATTERY_LEVEL_LOW:    s.batteryLevel = 0.25f; break;
                case BATTERY_LEVEL_MEDIUM: s.batteryLevel = 0.60f; break;
                case BATTERY_LEVEL_FULL:   s.batteryLevel = 1.00f; break;
                default:                   s.batteryLevel = -1.f;  break;
            }
        }

        static void FillXInputInfo(DWORD idx, NkGamepadInfo& info) noexcept {
            info.index      = static_cast<uint32>(idx);
            info.type       = NkGamepadType::NK_GP_TYPE_XBOX;
            info.hasRumble  = true;
            info.hasBattery = true;
            info.numButtons = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
            info.numAxes    = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
            std::snprintf(info.id,   sizeof(info.id),   "XInput#%u", static_cast<unsigned>(idx));
            std::snprintf(info.name, sizeof(info.name), "Xbox Controller %u", static_cast<unsigned>(idx));
        }

        // =====================================================================
        // DirectInput — énumération et poll
        // =====================================================================

        void RefreshDIDevices() {
            if (!mDI8) return;

            // Libérer les anciens devices
            for (auto& dc : mDIDevices) {
                if (dc.device) {
                    dc.device->Unacquire();
                    dc.device->Release();
                }
            }
            mDIDevices.clear();

            NkDIEnumCtx ctx{ mDI8, &mDIDevices, NK_DI_MAX };
            mDI8->EnumDevices(DI8DEVCLASS_GAMECTRL,
                              NkDIEnumDevicesCallback,
                              &ctx,
                              DIEDFL_ATTACHEDONLY);

            // Remplir les infos des snapshots DirectInput
            for (uint32 i = 0; i < static_cast<uint32>(mDIDevices.size()); ++i) {
                uint32 slot = NK_XINPUT_MAX + i;
                auto& dc   = mDIDevices[i];
                auto& info = mSnapshots[slot].info;
                const uint16 vid = static_cast<uint16>((dc.guidProduct.Data1 >> 16) & 0xFFFF);
                const uint16 pid = static_cast<uint16>(dc.guidProduct.Data1 & 0xFFFF);

                info.index      = slot;
                info.type       = NkGamepadType::NK_GP_TYPE_GENERIC;
                info.hasRumble  = false;
                info.numButtons = dc.numButtons;
                info.numAxes    = dc.numAxes;
                info.vendorId   = vid;
                info.productId  = pid;

                if (vid == 0x054C) info.type = NkGamepadType::NK_GP_TYPE_PLAYSTATION;
                else if (vid == 0x045E) info.type = NkGamepadType::NK_GP_TYPE_XBOX;
                else if (vid == 0x057E) info.type = NkGamepadType::NK_GP_TYPE_NINTENDO;

                std::snprintf(info.id,   sizeof(info.id),
                              "DInput#%u-VID_%04X-PID_%04X",
                              i,
                              static_cast<unsigned>(vid),
                              static_cast<unsigned>(pid));
                std::snprintf(info.name, sizeof(info.name), "%s", dc.name);
            }
        }

        void PollDirectInput() {
            if (!mDI8) return;

            for (uint32 i = 0; i < static_cast<uint32>(mDIDevices.size()); ++i) {
                uint32 slot = NK_XINPUT_MAX + i;
                auto& dc   = mDIDevices[i];

                if (!dc.device) {
                    mSnapshots[slot].connected = false;
                    continue;
                }

                // Tentative de réacquisition si perdu
                HRESULT hr = dc.device->Poll();
                if (FAILED(hr)) {
                    hr = dc.device->Acquire();
                    if (FAILED(hr)) {
                        mSnapshots[slot].connected = false;
                        continue;
                    }
                    dc.device->Poll();
                }

                dc.prevState = dc.state;
                hr = dc.device->GetDeviceState(sizeof(DIJOYSTATE2), &dc.state);
                if (FAILED(hr)) {
                    mSnapshots[slot].connected = false;
                    continue;
                }

                NkDI_FillSnapshot(dc, slot, mSnapshots[slot]);
            }

            // Slots DInput non remplis → déconnectés
            for (uint32 i = static_cast<uint32>(mDIDevices.size()); i < NK_DI_MAX; ++i) {
                uint32 slot = NK_XINPUT_MAX + i;
                mSnapshots[slot].connected = false;
            }
        }
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX
