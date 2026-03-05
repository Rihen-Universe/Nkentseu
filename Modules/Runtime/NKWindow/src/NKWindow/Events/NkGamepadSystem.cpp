// =============================================================================
// NkGamepadSystem.cpp — implémentation façade + sélection backend
//
// CORRECTION : NkEventSystem::Instance() n'existe plus.
// Tout dispatch passe par NkSystem::Events() (NkEventSystem possédé par NkSystem).
// =============================================================================

#include "NkGamepadSystem.h"
#include "NkGamepadEvent.h"
#include "NkEventSystem.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKPlatform/NkPlatformDetect.h"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Sélection du backend gamepad par plateforme
// ---------------------------------------------------------------------------

#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
#   include "NKWindow/Platform/Noop/NkNoopGamepad.h"
    using PlatformGamepad = nkentseu::NkNoopGamepad;

#elif defined(NKENTSEU_PLATFORM_UWP)
#   include "NKWindow/Platform/UWP/NkUWPGamepad.h"
    using PlatformGamepad = nkentseu::NkUWPGamepad;

#elif defined(NKENTSEU_PLATFORM_XBOX)
#   include "NKWindow/Platform/Xbox/NkXboxGamepad.h"
    using PlatformGamepad = nkentseu::NkXboxGamepad;

#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#   include "NKWindow/Platform/Win32/NkWin32Gamepad.h"
    using PlatformGamepad = nkentseu::NkWin32Gamepad;

#elif defined(NKENTSEU_PLATFORM_MACOS)
#   include "NKWindow/Platform/Cocoa/NkCocoaGamepad.h"
    using PlatformGamepad = nkentseu::NkCocoaGamepad;

#elif defined(NKENTSEU_PLATFORM_IOS)
#   include "NKWindow/Platform/UIKit/NkUIKitGamepad.h"
    using PlatformGamepad = nkentseu::NkUIKitGamepad;

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include "NKWindow/Platform/Android/NkAndroidGamepad.h"
    using PlatformGamepad = nkentseu::NkAndroidGamepad;

#elif defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_WINDOWING_XLIB) \
   || defined(NKENTSEU_WINDOWING_WAYLAND)
#   include "NKWindow/Platform/Linux/NkLinuxGamepadBackend.h"
    using PlatformGamepad = nkentseu::NkLinuxGamepad;

#elif defined(NKENTSEU_PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#   include "NKWindow/Platform/WASM/NkWASMGamepad.h"
    using PlatformGamepad = nkentseu::NkWASMGamepad;

#else
#   include "NKWindow/Platform/Noop/NkNoopGamepad.h"
    using PlatformGamepad = nkentseu::NkNoopGamepad;
#endif

namespace nkentseu {

    // Sentinelles pour les accès invalides
    NkGamepadSnapshot NkGamepadSystem::sDummySnapshot;
    NkGamepadInfo     NkGamepadSystem::sDummyInfo;

    // -------------------------------------------------------------------------
    // Helper interne — seul point d'accès à NkEventSystem.
    // NkEventSystem::Instance() n'existe plus → NkSystem::Events().
    // -------------------------------------------------------------------------
    static inline NkEventSystem& EvSys() noexcept {
        return NkSystem::Events();
    }

    // -------------------------------------------------------------------------
    // Helpers remap
    // -------------------------------------------------------------------------

    NkF32 NkGamepadSystem::ClampAxisForTarget(NkU32 logicalAxisIndex, NkF32 value) noexcept {
        if (logicalAxisIndex >= AXIS_COUNT) return 0.f;

        // Les gâchettes restent dans [0,1], les autres axes dans [-1,1].
        if (logicalAxisIndex == static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_LT) ||
            logicalAxisIndex == static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_RT))
        {
            return std::clamp(value, 0.f, 1.f);
        }
        return std::clamp(value, -1.f, 1.f);
    }

    void NkGamepadSystem::ResetMappingToIdentity(NkU32 idx) noexcept {
        if (idx >= NK_MAX_GAMEPADS) return;

        NkRemapProfile& profile = mMappings[idx];
        profile.active = false;

        for (NkU32 b = 0; b < BUTTON_COUNT; ++b) {
            profile.buttonMap[b] = b;
        }
        for (NkU32 a = 0; a < AXIS_COUNT; ++a) {
            profile.axisMap[a].logicalAxis = a;
            profile.axisMap[a].scale       = 1.f;
            profile.axisMap[a].invert      = false;
        }
    }

    const NkGamepadSnapshot& NkGamepadSystem::ApplyRemap(NkU32 idx,
                                                          const NkGamepadSnapshot& raw) noexcept
    {
        NkGamepadSnapshot& out = mMappedSnapshot[idx];

        if (idx >= NK_MAX_GAMEPADS) return sDummySnapshot;

        // Fast path: aucun remap actif -> copie directe.
        if (!mMappings[idx].active) {
            out = raw;
            return out;
        }

        out.Clear();
        out.connected    = raw.connected;
        out.info         = raw.info;
        out.gyroX        = raw.gyroX;
        out.gyroY        = raw.gyroY;
        out.gyroZ        = raw.gyroZ;
        out.accelX       = raw.accelX;
        out.accelY       = raw.accelY;
        out.accelZ       = raw.accelZ;
        out.batteryLevel = raw.batteryLevel;
        out.isCharging   = raw.isCharging;

        if (!raw.connected) return out;

        const NkRemapProfile& profile = mMappings[idx];

        // Boutons physiques -> logiques
        for (NkU32 b = 0; b < BUTTON_COUNT; ++b) {
            if (!raw.buttons[b]) continue;

            NkU32 dst = profile.buttonMap[b];
            if (dst >= BUTTON_COUNT) continue;
            out.buttons[dst] = true;
        }

        // Axes physiques -> logiques
        for (NkU32 a = 0; a < AXIS_COUNT; ++a) {
            const NkAxisRemap& remap = profile.axisMap[a];
            if (remap.logicalAxis >= AXIS_COUNT) continue;

            NkF32 v = raw.axes[a];
            if (remap.invert) v = -v;
            v *= remap.scale;
            v = ClampAxisForTarget(remap.logicalAxis, v);

            NkF32& dst = out.axes[remap.logicalAxis];
            if (std::fabs(v) > std::fabs(dst)) dst = v;
        }

        // Cohérence dpad boutons <-> axes si non fournis explicitement.
        const NkU32 dpadX = static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X);
        const NkU32 dpadY = static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y);
        const NkU32 left  = static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_LEFT);
        const NkU32 right = static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_RIGHT);
        const NkU32 up    = static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_UP);
        const NkU32 down  = static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_DOWN);

        if (std::fabs(out.axes[dpadX]) < 0.001f) {
            out.axes[dpadX] = out.buttons[right] ? 1.f : out.buttons[left] ? -1.f : 0.f;
        }
        if (std::fabs(out.axes[dpadY]) < 0.001f) {
            out.axes[dpadY] = out.buttons[up] ? 1.f : out.buttons[down] ? -1.f : 0.f;
        }
        if (!out.buttons[left]  && out.axes[dpadX] < -0.5f) out.buttons[left]  = true;
        if (!out.buttons[right] && out.axes[dpadX] >  0.5f) out.buttons[right] = true;
        if (!out.buttons[up]    && out.axes[dpadY] >  0.5f) out.buttons[up]    = true;
        if (!out.buttons[down]  && out.axes[dpadY] < -0.5f) out.buttons[down]  = true;

        // Cohérence triggers analogiques -> boutons digitaux si absents.
        const NkU32 ltA = static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_LT);
        const NkU32 rtA = static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_RT);
        const NkU32 ltB = static_cast<NkU32>(NkGamepadButton::NK_GP_LT_DIGITAL);
        const NkU32 rtB = static_cast<NkU32>(NkGamepadButton::NK_GP_RT_DIGITAL);
        if (!out.buttons[ltB]) out.buttons[ltB] = out.axes[ltA] > 0.5f;
        if (!out.buttons[rtB]) out.buttons[rtB] = out.axes[rtA] > 0.5f;

        return out;
    }

    void NkGamepadSystem::SyncMappedSnapshot(NkU32 idx) noexcept {
        if (idx >= NK_MAX_GAMEPADS) return;

        if (!mReady || !mBackend) {
            mRawSnapshot[idx].Clear();
            mMappedSnapshot[idx].Clear();
            mPrevSnapshot[idx].Clear();
            return;
        }

        mRawSnapshot[idx] = mBackend->GetSnapshot(idx);
        ApplyRemap(idx, mRawSnapshot[idx]);
        mPrevSnapshot[idx] = mMappedSnapshot[idx];
    }

    // -------------------------------------------------------------------------
    // Init / Shutdown
    // -------------------------------------------------------------------------

    bool NkGamepadSystem::Init(std::unique_ptr<NkIGamepad> backend) {
        if (mReady) return true;
        if (!backend) backend = std::make_unique<PlatformGamepad>();
        mBackend = std::move(backend);
        mReady   = mBackend->Init();
        if (!mMappingPersistence) {
            mMappingPersistence = std::make_unique<NkTextGamepadMappingPersistence>();
        }

        for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            mRawSnapshot[i].Clear();
            mMappedSnapshot[i].Clear();
            mPrevSnapshot[i].Clear();
            ResetMappingToIdentity(i);
        }
        return mReady;
    }

    void NkGamepadSystem::Shutdown() {
        if (!mReady) return;
        if (mBackend) mBackend->Shutdown();
        mBackend.reset();
        mReady = false;

        for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            mRawSnapshot[i].Clear();
            mMappedSnapshot[i].Clear();
            mPrevSnapshot[i].Clear();
            ResetMappingToIdentity(i);
        }
    }

    // -------------------------------------------------------------------------
    // Remapping public API
    // -------------------------------------------------------------------------

    void NkGamepadSystem::ClearMapping(NkU32 idx) noexcept {
        if (idx >= NK_MAX_GAMEPADS) return;
        ResetMappingToIdentity(idx);
        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::ClearAllMappings() noexcept {
        for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            ResetMappingToIdentity(i);
            SyncMappedSnapshot(i);
        }
    }

    void NkGamepadSystem::SetButtonMapByIndex(NkU32 idx,
                                              NkU32 physicalButtonIndex,
                                              NkGamepadButton logicalButton) noexcept
    {
        if (idx >= NK_MAX_GAMEPADS || physicalButtonIndex >= BUTTON_COUNT) return;

        NkRemapProfile& profile = mMappings[idx];
        NkU32 logicalIndex = static_cast<NkU32>(logicalButton);
        if (logicalButton == NkGamepadButton::NK_GP_UNKNOWN) {
            logicalIndex = NK_GAMEPAD_UNMAPPED;
        }
        profile.buttonMap[physicalButtonIndex] =
            (logicalIndex < BUTTON_COUNT) ? logicalIndex : NK_GAMEPAD_UNMAPPED;
        profile.active = true;

        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::SetAxisMapByIndex(NkU32 idx,
                                            NkU32 physicalAxisIndex,
                                            NkGamepadAxis logicalAxis,
                                            bool invert,
                                            NkF32 scale) noexcept
    {
        if (idx >= NK_MAX_GAMEPADS || physicalAxisIndex >= AXIS_COUNT) return;

        NkRemapProfile& profile = mMappings[idx];
        NkU32 logicalIndex = static_cast<NkU32>(logicalAxis);
        if (logicalIndex >= AXIS_COUNT) logicalIndex = NK_GAMEPAD_UNMAPPED;

        profile.axisMap[physicalAxisIndex].logicalAxis = logicalIndex;
        profile.axisMap[physicalAxisIndex].invert      = invert;
        profile.axisMap[physicalAxisIndex].scale       = scale;
        profile.active = true;

        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::DisableButtonByIndex(NkU32 idx, NkU32 physicalButtonIndex) noexcept {
        if (idx >= NK_MAX_GAMEPADS || physicalButtonIndex >= BUTTON_COUNT) return;
        mMappings[idx].buttonMap[physicalButtonIndex] = NK_GAMEPAD_UNMAPPED;
        mMappings[idx].active = true;
        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::DisableAxisByIndex(NkU32 idx, NkU32 physicalAxisIndex) noexcept {
        if (idx >= NK_MAX_GAMEPADS || physicalAxisIndex >= AXIS_COUNT) return;
        mMappings[idx].axisMap[physicalAxisIndex].logicalAxis = NK_GAMEPAD_UNMAPPED;
        mMappings[idx].axisMap[physicalAxisIndex].invert      = false;
        mMappings[idx].axisMap[physicalAxisIndex].scale       = 1.f;
        mMappings[idx].active = true;
        SyncMappedSnapshot(idx);
    }

    const NkGamepadSystem::NkRemapProfile* NkGamepadSystem::GetMapping(NkU32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return nullptr;
        return &mMappings[idx];
    }

    // -------------------------------------------------------------------------
    // PollGamepads — détection deltas boutons/axes + fire callbacks
    // -------------------------------------------------------------------------

    void NkGamepadSystem::PollGamepads() {
        if (!mReady || !mBackend) return;
        mBackend->Poll();

        for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            mRawSnapshot[i] = mBackend->GetSnapshot(i);
            const NkGamepadSnapshot& cur  = ApplyRemap(i, mRawSnapshot[i]);
            const NkGamepadSnapshot& prev = mPrevSnapshot[i];

            // Connexion / déconnexion
            if (cur.connected != prev.connected)
                FireConnect(cur.info, cur.connected);

            if (!cur.connected) {
                mPrevSnapshot[i] = cur;
                continue;
            }

            // Boutons (événements limités aux boutons connus par l'enum)
            for (NkU32 b = 0; b < EVENT_BUTTON_COUNT; ++b) {
                if (cur.buttons[b] != prev.buttons[b]) {
                    FireButton(i, static_cast<NkGamepadButton>(b),
                               cur.buttons[b] ? NkButtonState::NK_PRESSED
                                              : NkButtonState::NK_RELEASED);
                }
            }

            // Axes avec deadzone et epsilon (domaine enum)
            for (NkU32 a = 0; a < EVENT_AXIS_COUNT; ++a) {
                NkF32 v  = ApplyDeadzone(cur.axes[a]);
                NkF32 pv = ApplyDeadzone(prev.axes[a]);
                if (std::abs(v - pv) > mAxisEpsilon)
                    FireAxis(i, static_cast<NkGamepadAxis>(a), v, pv);
            }

            mPrevSnapshot[i] = cur;
        }
    }

    // -------------------------------------------------------------------------
    // Polling direct
    // -------------------------------------------------------------------------

    NkU32 NkGamepadSystem::GetConnectedCount() const noexcept {
        return (mReady && mBackend) ? mBackend->GetConnectedCount() : 0;
    }

    bool NkGamepadSystem::IsConnected(NkU32 idx) const noexcept {
        return idx < NK_MAX_GAMEPADS && GetSnapshot(idx).connected;
    }

    const NkGamepadInfo& NkGamepadSystem::GetInfo(NkU32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return sDummyInfo;
        return GetSnapshot(idx).info;
    }

    const NkGamepadSnapshot& NkGamepadSystem::GetSnapshot(NkU32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return sDummySnapshot;
        return mMappedSnapshot[idx];
    }

    const NkGamepadSnapshot& NkGamepadSystem::GetRawSnapshot(NkU32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return sDummySnapshot;
        return mRawSnapshot[idx];
    }

    bool NkGamepadSystem::IsButtonDown(NkU32 idx, NkGamepadButton btn) const noexcept {
        return GetSnapshot(idx).IsButtonDown(btn);
    }

    bool NkGamepadSystem::IsButtonDownByIndex(NkU32 idx, NkU32 btnIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || btnIndex >= BUTTON_COUNT) return false;
        return mMappedSnapshot[idx].buttons[btnIndex];
    }

    NkF32 NkGamepadSystem::GetAxis(NkU32 idx, NkGamepadAxis ax) const noexcept {
        return GetSnapshot(idx).GetAxis(ax);
    }

    NkF32 NkGamepadSystem::GetAxisByIndex(NkU32 idx, NkU32 axisIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || axisIndex >= AXIS_COUNT) return 0.f;
        return mMappedSnapshot[idx].axes[axisIndex];
    }

    bool NkGamepadSystem::IsRawButtonDownByIndex(NkU32 idx, NkU32 btnIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || btnIndex >= BUTTON_COUNT) return false;
        return mRawSnapshot[idx].buttons[btnIndex];
    }

    NkF32 NkGamepadSystem::GetRawAxisByIndex(NkU32 idx, NkU32 axisIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || axisIndex >= AXIS_COUNT) return 0.f;
        return mRawSnapshot[idx].axes[axisIndex];
    }

    // -------------------------------------------------------------------------
    // Commandes de sortie
    // -------------------------------------------------------------------------

    void NkGamepadSystem::Rumble(NkU32 idx,
                                 NkF32 motorLow, NkF32 motorHigh,
                                 NkF32 triggerLeft, NkF32 triggerRight,
                                 NkU32 durationMs)
    {
        if (!mReady || !mBackend) return;
        mBackend->Rumble(idx, motorLow, motorHigh, triggerLeft, triggerRight, durationMs);

        // Dispatch de l'événement rumble via NkSystem::Events()
        NkGamepadRumbleEvent event(idx, motorLow, motorHigh,
                                   triggerLeft, triggerRight, durationMs);
        EvSys().DispatchEvent(event);
    }

    void NkGamepadSystem::SetLEDColor(NkU32 idx, NkU32 rgba) {
        if (!mReady || !mBackend) return;
        mBackend->SetLEDColor(idx, rgba);
    }

    void NkGamepadSystem::SetMappingPersistence(std::unique_ptr<NkIGamepadMappingPersistence> persistence) {
        mMappingPersistence = std::move(persistence);
    }

    NkGamepadMappingProfileData NkGamepadSystem::ExportMappingProfile() const {
        NkGamepadMappingProfileData profile;
        profile.version = 1;
        profile.backendName = (mBackend ? mBackend->GetName() : "Unknown");
        profile.slots.reserve(NK_MAX_GAMEPADS);

        for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            const NkRemapProfile& src = mMappings[i];
            if (!src.active) continue;

            NkGamepadMappingSlotData dst;
            dst.slotIndex = i;
            dst.active = src.active;
            dst.buttons.reserve(BUTTON_COUNT);
            dst.axes.reserve(AXIS_COUNT);

            for (NkU32 b = 0; b < BUTTON_COUNT; ++b) {
                NkGamepadButtonMapEntry e;
                e.physicalButton = b;
                e.logicalButton = src.buttonMap[b];
                dst.buttons.push_back(e);
            }
            for (NkU32 a = 0; a < AXIS_COUNT; ++a) {
                NkGamepadAxisMapEntry e;
                e.physicalAxis = a;
                e.logicalAxis = src.axisMap[a].logicalAxis;
                e.scale = src.axisMap[a].scale;
                e.invert = src.axisMap[a].invert;
                dst.axes.push_back(e);
            }
            profile.slots.push_back(std::move(dst));
        }

        return profile;
    }

    bool NkGamepadSystem::ImportMappingProfile(const NkGamepadMappingProfileData& profile,
                                               bool clearExisting,
                                               std::string* outError)
    {
        if (clearExisting) {
            ClearAllMappings();
        }

        for (const NkGamepadMappingSlotData& slot : profile.slots) {
            if (slot.slotIndex >= NK_MAX_GAMEPADS) continue;

            if (!slot.active) {
                ClearMapping(slot.slotIndex);
                continue;
            }

            ResetMappingToIdentity(slot.slotIndex);
            NkRemapProfile& dst = mMappings[slot.slotIndex];
            dst.active = true;

            for (const NkGamepadButtonMapEntry& b : slot.buttons) {
                if (b.physicalButton >= BUTTON_COUNT) continue;
                dst.buttonMap[b.physicalButton] = (b.logicalButton < BUTTON_COUNT)
                    ? b.logicalButton
                    : NK_GAMEPAD_UNMAPPED;
            }

            for (const NkGamepadAxisMapEntry& a : slot.axes) {
                if (a.physicalAxis >= AXIS_COUNT) continue;
                dst.axisMap[a.physicalAxis].logicalAxis =
                    (a.logicalAxis < AXIS_COUNT) ? a.logicalAxis : NK_GAMEPAD_UNMAPPED;
                dst.axisMap[a.physicalAxis].scale  = a.scale;
                dst.axisMap[a.physicalAxis].invert = a.invert;
            }

            SyncMappedSnapshot(slot.slotIndex);
        }

        if (outError) outError->clear();
        return true;
    }

    bool NkGamepadSystem::SaveMappingProfile(const std::string& userId,
                                             std::string* outError) const
    {
        if (!mMappingPersistence) {
            if (outError) *outError = "No mapping persistence backend configured.";
            return false;
        }
        const NkGamepadMappingProfileData profile = ExportMappingProfile();
        return mMappingPersistence->Save(userId, profile, outError);
    }

    bool NkGamepadSystem::LoadMappingProfile(const std::string& userId,
                                             bool clearExisting,
                                             std::string* outError)
    {
        if (!mMappingPersistence) {
            if (outError) *outError = "No mapping persistence backend configured.";
            return false;
        }

        NkGamepadMappingProfileData profile;
        if (!mMappingPersistence->Load(userId, profile, outError)) {
            return false;
        }
        return ImportMappingProfile(profile, clearExisting, outError);
    }

    // -------------------------------------------------------------------------
    // Fire helpers — tout le dispatch passe par EvSys() = NkSystem::Events()
    // -------------------------------------------------------------------------

    void NkGamepadSystem::FireConnect(const NkGamepadInfo& info, bool connected) {
        if (connected) {
            NkGamepadConnectEvent    ev(info);       EvSys().DispatchEvent(ev);
        } else {
            NkGamepadDisconnectEvent ev(info.index); EvSys().DispatchEvent(ev);
        }

        // Mettre à jour NkEventState.gamepads en même temps
        auto& gpState = EvSys().GetInputState().GetGamepads();
        if (connected) gpState.OnConnect(info.index, info);
        else           gpState.OnDisconnect(info.index);

        if (mConnectCb) mConnectCb(info, connected);
    }

    void NkGamepadSystem::FireButton(NkU32 idx, NkGamepadButton btn, NkButtonState st) {
        if (btn == NkGamepadButton::NK_GP_UNKNOWN) return;

        if (st == NkButtonState::NK_PRESSED) {
            NkGamepadButtonPressEvent   ev(idx, btn); EvSys().DispatchEvent(ev);
        } else {
            NkGamepadButtonReleaseEvent ev(idx, btn); EvSys().DispatchEvent(ev);
        }

        // Mettre à jour NkEventState.gamepads
        auto* slot = EvSys().GetInputState().GetGamepads().GetSlot(idx);
        if (slot) {
            if (st == NkButtonState::NK_PRESSED) slot->OnButtonPress(btn);
            else                                  slot->OnButtonRelease(btn);
        }

        if (mButtonCb) mButtonCb(idx, btn, st);
    }

    void NkGamepadSystem::FireAxis(NkU32 idx, NkGamepadAxis ax,
                                   NkF32 value, NkF32 prevValue)
    {
        NkGamepadAxisEvent ev(idx, ax, value, prevValue);
        EvSys().DispatchEvent(ev);

        // Mettre à jour NkEventState.gamepads
        auto* slot = EvSys().GetInputState().GetGamepads().GetSlot(idx);
        if (slot) slot->OnAxisMove(ax, value);

        if (mAxisCb) mAxisCb(idx, ax, value);
    }

} // namespace nkentseu
