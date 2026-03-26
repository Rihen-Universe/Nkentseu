// =============================================================================
// NkGamepadSystem.cpp â€” implÃ©mentation faÃ§ade + sÃ©lection backend
//
// CORRECTION : NkEventSystem::Instance() n'existe plus.
// Tout dispatch passe par NkSystem::Events() (NkEventSystem possÃ©dÃ© par NkSystem).
// =============================================================================

#include "NkGamepadSystem.h"
#include "NkGamepadEvent.h"
#include "NkEventSystem.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKMath/NkFunctions.h"

// ---------------------------------------------------------------------------
// SÃ©lection du backend gamepad par plateforme
// ---------------------------------------------------------------------------

namespace nkentseu {

    namespace {
        inline float32 SanitizeAxisValue(float32 v) noexcept {
            return math::NkIsFinite(v) ? v : 0.f;
        }

        inline float32 SanitizeAxisScale(float32 v) noexcept {
            return math::NkIsFinite(v) ? v : 1.f;
        }
    } // namespace

    // Sentinelles pour les accÃ¨s invalides
    NkGamepadSnapshot NkGamepadSystem::sDummySnapshot;
    NkGamepadInfo     NkGamepadSystem::sDummyInfo;

    // -------------------------------------------------------------------------
    // Helper interne â€” seul point d'accÃ¨s Ã  NkEventSystem.
    // NkEventSystem::Instance() n'existe plus â†’ NkSystem::Events().
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Helpers remap
    // -------------------------------------------------------------------------

    float32 NkGamepadSystem::ClampAxisForTarget(uint32 logicalAxisIndex, float32 value) noexcept {
        if (logicalAxisIndex >= AXIS_COUNT) return 0.f;
        value = SanitizeAxisValue(value);

        // Les gÃ¢chettes restent dans [0,1], les autres axes dans [-1,1].
        if (logicalAxisIndex == static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT) ||
            logicalAxisIndex == static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT))
        {
            return math::NkClamp(value, 0.f, 1.f);
        }
        return math::NkClamp(value, -1.f, 1.f);
    }

    void NkGamepadSystem::ResetMappingToIdentity(uint32 idx) noexcept {
        if (idx >= NK_MAX_GAMEPADS) return;

        NkRemapProfile& profile = mMappings[idx];
        profile.active = false;

        for (uint32 b = 0; b < BUTTON_COUNT; ++b) {
            profile.buttonMap[b] = b;
        }
        for (uint32 a = 0; a < AXIS_COUNT; ++a) {
            profile.axisMap[a].logicalAxis = a;
            profile.axisMap[a].scale       = 1.f;
            profile.axisMap[a].invert      = false;
        }
    }

    const NkGamepadSnapshot& NkGamepadSystem::ApplyRemap(uint32 idx,
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
        for (uint32 b = 0; b < BUTTON_COUNT; ++b) {
            if (!raw.buttons[b]) continue;

            uint32 dst = profile.buttonMap[b];
            if (dst >= BUTTON_COUNT) continue;
            out.buttons[dst] = true;
        }

        // Axes physiques -> logiques
        for (uint32 a = 0; a < AXIS_COUNT; ++a) {
            const NkAxisRemap& remap = profile.axisMap[a];
            if (remap.logicalAxis >= AXIS_COUNT) continue;

            float32 v = SanitizeAxisValue(raw.axes[a]);
            if (remap.invert) v = -v;
            v *= SanitizeAxisScale(remap.scale);
            v = ClampAxisForTarget(remap.logicalAxis, v);

            float32& dst = out.axes[remap.logicalAxis];
            dst = SanitizeAxisValue(dst);
            if (math::NkFabs(v) > math::NkFabs(dst)) dst = v;
        }

        // CohÃ©rence dpad boutons <-> axes si non fournis explicitement.
        const uint32 dpadX = static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X);
        const uint32 dpadY = static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y);
        const uint32 left  = static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT);
        const uint32 right = static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT);
        const uint32 up    = static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP);
        const uint32 down  = static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN);

        float32 dpadXValue = SanitizeAxisValue(out.axes[dpadX]);
        float32 dpadYValue = SanitizeAxisValue(out.axes[dpadY]);

        if (math::NkFabs(dpadXValue) < 0.001f) {
            dpadXValue = out.buttons[right] ? 1.f : out.buttons[left] ? -1.f : 0.f;
        }
        if (math::NkFabs(dpadYValue) < 0.001f) {
            dpadYValue = out.buttons[up] ? 1.f : out.buttons[down] ? -1.f : 0.f;
        }
        out.axes[dpadX] = dpadXValue;
        out.axes[dpadY] = dpadYValue;

        if (!out.buttons[left]  && dpadXValue < -0.5f) out.buttons[left]  = true;
        if (!out.buttons[right] && dpadXValue >  0.5f) out.buttons[right] = true;
        if (!out.buttons[up]    && dpadYValue >  0.5f) out.buttons[up]    = true;
        if (!out.buttons[down]  && dpadYValue < -0.5f) out.buttons[down]  = true;

        // CohÃ©rence triggers analogiques -> boutons digitaux si absents.
        const uint32 ltA = static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT);
        const uint32 rtA = static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT);
        const uint32 ltB = static_cast<uint32>(NkGamepadButton::NK_GP_LT_DIGITAL);
        const uint32 rtB = static_cast<uint32>(NkGamepadButton::NK_GP_RT_DIGITAL);
        const float32 ltValue = SanitizeAxisValue(out.axes[ltA]);
        const float32 rtValue = SanitizeAxisValue(out.axes[rtA]);
        out.axes[ltA] = ltValue;
        out.axes[rtA] = rtValue;
        if (!out.buttons[ltB]) out.buttons[ltB] = ltValue > 0.5f;
        if (!out.buttons[rtB]) out.buttons[rtB] = rtValue > 0.5f;

        return out;
    }

    void NkGamepadSystem::SyncMappedSnapshot(uint32 idx) noexcept {
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

    bool NkGamepadSystem::Init(memory::NkUniquePtr<NkIGamepad> backend) {
        if (mReady) return true;
        if (!backend) {
            // Aucun backend fourni : le systeme restera inactif.
            // NkSystem::Initialise() fournit toujours le bon backend plateforme.
            return false;
        }
        mBackend = traits::NkMove(backend);
        mReady   = mBackend->Init();
        if (!mMappingPersistence) {
            memory::NkAllocator& allocator = memory::NkGetDefaultAllocator();
            mMappingPersistence = memory::NkUniquePtr<NkIGamepadMappingPersistence>(
                allocator.New<NkTextGamepadMappingPersistence>(),
                memory::NkDefaultDelete<NkIGamepadMappingPersistence>(&allocator));
        }

        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
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
        mBackend.Reset();
        mReady = false;

        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            mRawSnapshot[i].Clear();
            mMappedSnapshot[i].Clear();
            mPrevSnapshot[i].Clear();
            ResetMappingToIdentity(i);
        }
    }

    // -------------------------------------------------------------------------
    // Remapping public API
    // -------------------------------------------------------------------------

    void NkGamepadSystem::ClearMapping(uint32 idx) noexcept {
        if (idx >= NK_MAX_GAMEPADS) return;
        ResetMappingToIdentity(idx);
        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::ClearAllMappings() noexcept {
        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            ResetMappingToIdentity(i);
            SyncMappedSnapshot(i);
        }
    }

    void NkGamepadSystem::SetButtonMapByIndex(uint32 idx,
                                              uint32 physicalButtonIndex,
                                              NkGamepadButton logicalButton) noexcept
    {
        if (idx >= NK_MAX_GAMEPADS || physicalButtonIndex >= BUTTON_COUNT) return;

        NkRemapProfile& profile = mMappings[idx];
        uint32 logicalIndex = static_cast<uint32>(logicalButton);
        if (logicalButton == NkGamepadButton::NK_GP_UNKNOWN) {
            logicalIndex = NK_GAMEPAD_UNMAPPED;
        }
        profile.buttonMap[physicalButtonIndex] =
            (logicalIndex < BUTTON_COUNT) ? logicalIndex : NK_GAMEPAD_UNMAPPED;
        profile.active = true;

        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::SetAxisMapByIndex(uint32 idx,
                                            uint32 physicalAxisIndex,
                                            NkGamepadAxis logicalAxis,
                                            bool invert,
                                            float32 scale) noexcept
    {
        if (idx >= NK_MAX_GAMEPADS || physicalAxisIndex >= AXIS_COUNT) return;

        NkRemapProfile& profile = mMappings[idx];
        uint32 logicalIndex = static_cast<uint32>(logicalAxis);
        if (logicalIndex >= AXIS_COUNT) logicalIndex = NK_GAMEPAD_UNMAPPED;

        profile.axisMap[physicalAxisIndex].logicalAxis = logicalIndex;
        profile.axisMap[physicalAxisIndex].invert      = invert;
        profile.axisMap[physicalAxisIndex].scale       = SanitizeAxisScale(scale);
        profile.active = true;

        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::DisableButtonByIndex(uint32 idx, uint32 physicalButtonIndex) noexcept {
        if (idx >= NK_MAX_GAMEPADS || physicalButtonIndex >= BUTTON_COUNT) return;
        mMappings[idx].buttonMap[physicalButtonIndex] = NK_GAMEPAD_UNMAPPED;
        mMappings[idx].active = true;
        SyncMappedSnapshot(idx);
    }

    void NkGamepadSystem::DisableAxisByIndex(uint32 idx, uint32 physicalAxisIndex) noexcept {
        if (idx >= NK_MAX_GAMEPADS || physicalAxisIndex >= AXIS_COUNT) return;
        mMappings[idx].axisMap[physicalAxisIndex].logicalAxis = NK_GAMEPAD_UNMAPPED;
        mMappings[idx].axisMap[physicalAxisIndex].invert      = false;
        mMappings[idx].axisMap[physicalAxisIndex].scale       = 1.f;
        mMappings[idx].active = true;
        SyncMappedSnapshot(idx);
    }

    const NkGamepadSystem::NkRemapProfile* NkGamepadSystem::GetMapping(uint32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return nullptr;
        return &mMappings[idx];
    }

    // -------------------------------------------------------------------------
    // PollGamepads â€” dÃ©tection deltas boutons/axes + fire callbacks
    // -------------------------------------------------------------------------

    void NkGamepadSystem::PollGamepads() {
        if (!mReady || !mBackend) return;

        mBackend->Poll();

        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            mRawSnapshot[i] = mBackend->GetSnapshot(i);
            const NkGamepadSnapshot& cur  = ApplyRemap(i, mRawSnapshot[i]);
            const NkGamepadSnapshot& prev = mPrevSnapshot[i];

            // Connexion / dÃ©connexion
            if (cur.connected != prev.connected)
                FireConnect(cur.info, cur.connected);

            if (!cur.connected) {
                mPrevSnapshot[i] = cur;
                continue;
            }

            // Boutons (Ã©vÃ©nements limitÃ©s aux boutons connus par l'enum)
            for (uint32 b = 0; b < EVENT_BUTTON_COUNT; ++b) {
                if (cur.buttons[b] != prev.buttons[b]) {
                    FireButton(i, static_cast<NkGamepadButton>(b),
                               cur.buttons[b] ? NkButtonState::NK_PRESSED
                                              : NkButtonState::NK_RELEASED);
                }
            }

            // Axes avec deadzone et epsilon (domaine enum)
            for (uint32 a = 0; a < EVENT_AXIS_COUNT; ++a) {
                float32 v  = ApplyDeadzone(SanitizeAxisValue(cur.axes[a]));
                float32 pv = ApplyDeadzone(SanitizeAxisValue(prev.axes[a]));
                if (math::NkFabs(v - pv) > mAxisEpsilon)
                {
                    FireAxis(i, static_cast<NkGamepadAxis>(a), v, pv);
                }
            }

            mPrevSnapshot[i] = cur;
        }
    }

    // -------------------------------------------------------------------------
    // Polling direct
    // -------------------------------------------------------------------------

    uint32 NkGamepadSystem::GetConnectedCount() const noexcept {
        return (mReady && mBackend) ? mBackend->GetConnectedCount() : 0;
    }

    bool NkGamepadSystem::IsConnected(uint32 idx) const noexcept {
        return idx < NK_MAX_GAMEPADS && GetSnapshot(idx).connected;
    }

    const NkGamepadInfo& NkGamepadSystem::GetInfo(uint32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return sDummyInfo;
        return GetSnapshot(idx).info;
    }

    const NkGamepadSnapshot& NkGamepadSystem::GetSnapshot(uint32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return sDummySnapshot;
        return mMappedSnapshot[idx];
    }

    const NkGamepadSnapshot& NkGamepadSystem::GetRawSnapshot(uint32 idx) const noexcept {
        if (idx >= NK_MAX_GAMEPADS) return sDummySnapshot;
        return mRawSnapshot[idx];
    }

    bool NkGamepadSystem::IsButtonDown(uint32 idx, NkGamepadButton btn) const noexcept {
        return GetSnapshot(idx).IsButtonDown(btn);
    }

    bool NkGamepadSystem::IsButtonDownByIndex(uint32 idx, uint32 btnIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || btnIndex >= BUTTON_COUNT) return false;
        return mMappedSnapshot[idx].buttons[btnIndex];
    }

    float32 NkGamepadSystem::GetAxis(uint32 idx, NkGamepadAxis ax) const noexcept {
        return GetSnapshot(idx).GetAxis(ax);
    }

    float32 NkGamepadSystem::GetAxisByIndex(uint32 idx, uint32 axisIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || axisIndex >= AXIS_COUNT) return 0.f;
        return mMappedSnapshot[idx].axes[axisIndex];
    }

    bool NkGamepadSystem::IsRawButtonDownByIndex(uint32 idx, uint32 btnIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || btnIndex >= BUTTON_COUNT) return false;
        return mRawSnapshot[idx].buttons[btnIndex];
    }

    float32 NkGamepadSystem::GetRawAxisByIndex(uint32 idx, uint32 axisIndex) const noexcept {
        if (idx >= NK_MAX_GAMEPADS || axisIndex >= AXIS_COUNT) return 0.f;
        return mRawSnapshot[idx].axes[axisIndex];
    }

    // -------------------------------------------------------------------------
    // Commandes de sortie
    // -------------------------------------------------------------------------

    void NkGamepadSystem::Rumble(uint32 idx,
                                 float32 motorLow, float32 motorHigh,
                                 float32 triggerLeft, float32 triggerRight,
                                 uint32 durationMs)
    {
        if (!mReady || !mBackend) return;
        mBackend->Rumble(idx, motorLow, motorHigh, triggerLeft, triggerRight, durationMs);

        // Dispatch de l'Ã©vÃ©nement rumble via NkSystem::Events()
        NkGamepadRumbleEvent event(idx, motorLow, motorHigh,
                                   triggerLeft, triggerRight, durationMs);
        if (mEventSystem) mEventSystem->DispatchEvent(event);
    }

    void NkGamepadSystem::SetLEDColor(uint32 idx, uint32 rgba) {
        if (!mReady || !mBackend) return;
        mBackend->SetLEDColor(idx, rgba);
    }

    void NkGamepadSystem::SetMappingPersistence(memory::NkUniquePtr<NkIGamepadMappingPersistence> persistence) {
        mMappingPersistence = traits::NkMove(persistence);
    }

    NkGamepadMappingProfileData NkGamepadSystem::ExportMappingProfile() const {
        NkGamepadMappingProfileData profile;
        profile.version = 1;
        profile.backendName = (mBackend ? mBackend->GetName() : "Unknown");
        profile.slots.Reserve(NK_MAX_GAMEPADS);

        for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
            const NkRemapProfile& src = mMappings[i];
            if (!src.active) continue;

            NkGamepadMappingSlotData dst;
            dst.slotIndex = i;
            dst.active = src.active;
            dst.buttons.Reserve(BUTTON_COUNT);
            dst.axes.Reserve(AXIS_COUNT);

            for (uint32 b = 0; b < BUTTON_COUNT; ++b) {
                NkGamepadButtonMapEntry e;
                e.physicalButton = b;
                e.logicalButton = src.buttonMap[b];
                dst.buttons.PushBack(e);
            }
            for (uint32 a = 0; a < AXIS_COUNT; ++a) {
                NkGamepadAxisMapEntry e;
                e.physicalAxis = a;
                e.logicalAxis = src.axisMap[a].logicalAxis;
                e.scale = src.axisMap[a].scale;
                e.invert = src.axisMap[a].invert;
                dst.axes.PushBack(e);
            }
            profile.slots.PushBack(traits::NkMove(dst));
        }

        return profile;
    }

    bool NkGamepadSystem::ImportMappingProfile(const NkGamepadMappingProfileData& profile,
                                               bool clearExisting,
                                               NkString* outError)
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
                dst.axisMap[a.physicalAxis].scale  = SanitizeAxisScale(a.scale);
                dst.axisMap[a.physicalAxis].invert = a.invert;
            }

            SyncMappedSnapshot(slot.slotIndex);
        }

        if (outError) outError->Clear();
        return true;
    }

    bool NkGamepadSystem::SaveMappingProfile(const NkString& userId,
                                             NkString* outError) const
    {
        if (!mMappingPersistence) {
            if (outError) *outError = "No mapping persistence backend configured.";
            return false;
        }
        const NkGamepadMappingProfileData profile = ExportMappingProfile();
        return mMappingPersistence->Save(userId, profile, outError);
    }

    bool NkGamepadSystem::LoadMappingProfile(const NkString& userId,
                                             bool clearExisting,
                                             NkString* outError)
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
    // Fire helpers â€” tout le dispatch passe par *mEventSystem = NkSystem::Events()
    // -------------------------------------------------------------------------

    void NkGamepadSystem::FireConnect(const NkGamepadInfo& info, bool connected) {
        if (!mEventSystem) { if (mConnectCb) mConnectCb(info, connected); return; }
        if (connected) {
            NkGamepadConnectEvent    ev(info);       mEventSystem->DispatchEvent(ev);
        } else {
            NkGamepadDisconnectEvent ev(info.index); mEventSystem->DispatchEvent(ev);
        }

        // Mettre Ã  jour NkEventState.gamepads en mÃªme temps
        auto& gpState = mEventSystem->GetInputState().GetGamepads();
        if (connected) gpState.OnConnect(info.index, info);
        else           gpState.OnDisconnect(info.index);

        if (mConnectCb) mConnectCb(info, connected);
    }

    void NkGamepadSystem::FireButton(uint32 idx, NkGamepadButton btn, NkButtonState st) {
        if (!mEventSystem) { if (mButtonCb) mButtonCb(idx, btn, st); return; }
        if (btn == NkGamepadButton::NK_GP_UNKNOWN) return;

        if (st == NkButtonState::NK_PRESSED) {
            NkGamepadButtonPressEvent   ev(idx, btn); mEventSystem->DispatchEvent(ev);
        } else {
            NkGamepadButtonReleaseEvent ev(idx, btn); mEventSystem->DispatchEvent(ev);
        }

        // Mettre Ã  jour NkEventState.gamepads
        auto* slot = mEventSystem->GetInputState().GetGamepads().GetSlot(idx);
        if (slot) {
            if (st == NkButtonState::NK_PRESSED) slot->OnButtonPress(btn);
            else                                  slot->OnButtonRelease(btn);
        }

        if (mButtonCb) mButtonCb(idx, btn, st);
    }

    void NkGamepadSystem::FireAxis(uint32 idx, NkGamepadAxis ax,
                                   float32 value, float32 prevValue)
    {
        if (!mEventSystem) { if (mAxisCb) mAxisCb(idx, ax, value); return; }
        value = SanitizeAxisValue(value);
        prevValue = SanitizeAxisValue(prevValue);
        NkGamepadAxisEvent ev(idx, ax, value, prevValue);
        mEventSystem->DispatchEvent(ev);

        // Mettre Ã  jour NkEventState.gamepads
        auto* slot = mEventSystem->GetInputState().GetGamepads().GetSlot(idx);
        if (slot) slot->OnAxisMove(ax, value);

        if (mAxisCb) mAxisCb(idx, ax, value);
    }

} // namespace nkentseu
