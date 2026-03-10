#pragma once

// =============================================================================
// NkGenericHidMapper.h
// Mapper léger pour relier des indices HID physiques à des indices logiques.
//
// Objectif:
//   - L'utilisateur peut définir un mapping par deviceId.
//   - Le mapping reste générique (pas de dépendance backend).
//   - Utilisable directement depuis les callbacks d'événements HID.
// =============================================================================

#include "NkGenericHidEvent.h"

#include <algorithm>
#include <cmath>

namespace nkentseu {

    inline constexpr NkU32 NK_HID_UNMAPPED = 0xFFFFFFFFu;

    struct NkHidAxisBinding {
        NkU32 logicalAxis = NK_HID_UNMAPPED;
        NkF32 scale       = 1.f;
        NkF32 offset      = 0.f;
        NkF32 deadzone    = 0.f;
        bool  invert      = false;
    };

    class NkGenericHidMapper {
    public:
        void Clear() noexcept { mDevices.Clear(); }

        void ClearDevice(NkU64 deviceId) noexcept {
            mDevices.Erase(deviceId);
        }

        void SetButtonMap(NkU64 deviceId, NkU32 physicalButton, NkU32 logicalButton) {
            mDevices[deviceId].buttonMap[physicalButton] = logicalButton;
        }

        void DisableButton(NkU64 deviceId, NkU32 physicalButton) {
            mDevices[deviceId].buttonMap[physicalButton] = NK_HID_UNMAPPED;
        }

        void SetAxisMap(NkU64 deviceId,
                        NkU32 physicalAxis,
                        NkU32 logicalAxis,
                        bool invert = false,
                        NkF32 scale = 1.f,
                        NkF32 deadzone = 0.f,
                        NkF32 offset = 0.f)
        {
            NkHidAxisBinding b;
            b.logicalAxis = logicalAxis;
            b.invert      = invert;
            b.scale       = scale;
            b.deadzone    = deadzone;
            b.offset      = offset;
            mDevices[deviceId].axisMap[physicalAxis] = b;
        }

        void DisableAxis(NkU64 deviceId, NkU32 physicalAxis) {
            NkHidAxisBinding b;
            b.logicalAxis = NK_HID_UNMAPPED;
            mDevices[deviceId].axisMap[physicalAxis] = b;
        }

        NkU32 ResolveButton(NkU64 deviceId, NkU32 physicalButton) const noexcept {
            const DeviceMapping* dm = FindDevice(deviceId);
            if (!dm) return physicalButton;

            const NkU32* val = dm->buttonMap.Find(physicalButton);
            if (!val) return physicalButton;
            return *val;
        }

        bool ResolveAxis(NkU64 deviceId,
                         NkU32 physicalAxis,
                         NkF32 rawValue,
                         NkU32& outLogicalAxis,
                         NkF32& outValue) const noexcept
        {
            const DeviceMapping* dm = FindDevice(deviceId);
            if (!dm) {
                outLogicalAxis = physicalAxis;
                outValue = std::clamp(rawValue, -1.f, 1.f);
                return true;
            }

            const NkHidAxisBinding* b = dm->axisMap.Find(physicalAxis);
            if (!b) {
                outLogicalAxis = physicalAxis;
                outValue = std::clamp(rawValue, -1.f, 1.f);
                return true;
            }

            if (b->logicalAxis == NK_HID_UNMAPPED) return false;

            NkF32 v = rawValue;
            if (b->invert) v = -v;
            v = (v * b->scale) + b->offset;
            if (std::fabs(v) < b->deadzone) v = 0.f;
            outLogicalAxis = b->logicalAxis;
            outValue = std::clamp(v, -1.f, 1.f);
            return true;
        }

        bool MapButtonEvent(const NkHidButtonEvent& ev,
                            NkU32& outLogicalButton,
                            NkButtonState& outState) const noexcept
        {
            outLogicalButton = ResolveButton(ev.GetDeviceId(), ev.GetButtonIndex());
            if (outLogicalButton == NK_HID_UNMAPPED) return false;
            outState = ev.GetState();
            return true;
        }

        bool MapAxisEvent(const NkHidAxisEvent& ev,
                          NkU32& outLogicalAxis,
                          NkF32& outValue) const noexcept
        {
            return ResolveAxis(ev.GetDeviceId(), ev.GetAxisIndex(), ev.GetValue(),
                               outLogicalAxis, outValue);
        }

    private:
        struct DeviceMapping {
            NkUnorderedMap<NkU32, NkU32>            buttonMap;
            NkUnorderedMap<NkU32, NkHidAxisBinding> axisMap;
        };

        // FindDevice: returns const pointer (nullptr if not found).
        // NkUnorderedMap::Find(key) const → const Value*
        const DeviceMapping* FindDevice(NkU64 deviceId) const noexcept {
            return mDevices.Find(deviceId);
        }

        NkUnorderedMap<NkU64, DeviceMapping> mDevices;
    };

} // namespace nkentseu
