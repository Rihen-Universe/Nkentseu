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
#include "NKMath/NkFunctions.h"

#include <algorithm>
#include <cmath>

namespace nkentseu {

    inline constexpr uint32 NK_HID_UNMAPPED = 0xFFFFFFFFu;

    struct NkHidAxisBinding {
        uint32 logicalAxis = NK_HID_UNMAPPED;
        float32 scale       = 1.f;
        float32 offset      = 0.f;
        float32 deadzone    = 0.f;
        bool  invert      = false;
    };

    class NkGenericHidMapper {
    public:
        void Clear() noexcept { mDevices.Clear(); }

        void ClearDevice(uint64 deviceId) noexcept {
            mDevices.Erase(deviceId);
        }

        void SetButtonMap(uint64 deviceId, uint32 physicalButton, uint32 logicalButton) {
            mDevices[deviceId].buttonMap[physicalButton] = logicalButton;
        }

        void DisableButton(uint64 deviceId, uint32 physicalButton) {
            mDevices[deviceId].buttonMap[physicalButton] = NK_HID_UNMAPPED;
        }

        void SetAxisMap(uint64 deviceId,
                        uint32 physicalAxis,
                        uint32 logicalAxis,
                        bool invert = false,
                        float32 scale = 1.f,
                        float32 deadzone = 0.f,
                        float32 offset = 0.f)
        {
            NkHidAxisBinding b;
            b.logicalAxis = logicalAxis;
            b.invert      = invert;
            b.scale       = scale;
            b.deadzone    = deadzone;
            b.offset      = offset;
            mDevices[deviceId].axisMap[physicalAxis] = b;
        }

        void DisableAxis(uint64 deviceId, uint32 physicalAxis) {
            NkHidAxisBinding b;
            b.logicalAxis = NK_HID_UNMAPPED;
            mDevices[deviceId].axisMap[physicalAxis] = b;
        }

        uint32 ResolveButton(uint64 deviceId, uint32 physicalButton) const noexcept {
            const DeviceMapping* dm = FindDevice(deviceId);
            if (!dm) return physicalButton;

            const uint32* val = dm->buttonMap.Find(physicalButton);
            if (!val) return physicalButton;
            return *val;
        }

        bool ResolveAxis(uint64 deviceId,
                         uint32 physicalAxis,
                         float32 rawValue,
                         uint32& outLogicalAxis,
                         float32& outValue) const noexcept
        {
            const DeviceMapping* dm = FindDevice(deviceId);
            if (!dm) {
                outLogicalAxis = physicalAxis;
                outValue = math::NkClamp(rawValue, -1.f, 1.f);
                return true;
            }

            const NkHidAxisBinding* b = dm->axisMap.Find(physicalAxis);
            if (!b) {
                outLogicalAxis = physicalAxis;
                outValue = math::NkClamp(rawValue, -1.f, 1.f);
                return true;
            }

            if (b->logicalAxis == NK_HID_UNMAPPED) return false;

            float32 v = rawValue;
            if (b->invert) v = -v;
            v = (v * b->scale) + b->offset;
            if (math::NkFabs(v) < b->deadzone) v = 0.f;
            outLogicalAxis = b->logicalAxis;
            outValue = math::NkClamp(v, -1.f, 1.f);
            return true;
        }

        bool MapButtonEvent(const NkHidButtonEvent& ev,
                            uint32& outLogicalButton,
                            NkButtonState& outState) const noexcept
        {
            outLogicalButton = ResolveButton(ev.GetDeviceId(), ev.GetButtonIndex());
            if (outLogicalButton == NK_HID_UNMAPPED) return false;
            outState = ev.GetState();
            return true;
        }

        bool MapAxisEvent(const NkHidAxisEvent& ev,
                          uint32& outLogicalAxis,
                          float32& outValue) const noexcept
        {
            return ResolveAxis(ev.GetDeviceId(), ev.GetAxisIndex(), ev.GetValue(),
                               outLogicalAxis, outValue);
        }

    private:
        struct DeviceMapping {
            NkUnorderedMap<uint32, uint32>            buttonMap;
            NkUnorderedMap<uint32, NkHidAxisBinding> axisMap;
        };

        // FindDevice: returns const pointer (nullptr if not found).
        // NkUnorderedMap::Find(key) const → const Value*
        const DeviceMapping* FindDevice(uint64 deviceId) const noexcept {
            return mDevices.Find(deviceId);
        }

        NkUnorderedMap<uint64, DeviceMapping> mDevices;
    };

} // namespace nkentseu
