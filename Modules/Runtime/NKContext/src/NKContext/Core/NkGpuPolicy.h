#pragma once
// NkGpuPolicy.h - Cross-platform GPU selection policy utilities
#include "NkContextDesc.h"

namespace nkentseu {

    class NkGpuPolicy final {
    public:
        // Apply process-level hints before creating the graphics context.
        // For OpenGL this is best-effort and platform/driver dependent.
        static void ApplyPreContext(const NkContextDesc& desc);

        // Helper for backend-specific adapter filtering (DXGI/Vulkan PCI IDs).
        static bool MatchesVendorPciId(uint32 pciVendorId, NkGpuVendor vendor);

        static const char* PreferenceName(NkGpuPreference preference);
        static const char* VendorName(NkGpuVendor vendor);
    };

} // namespace nkentseu

