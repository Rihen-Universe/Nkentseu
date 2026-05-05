#pragma once
// =============================================================================
// Nkentseu/Core/NkProfiler.h — Profiler de performance
// =============================================================================
// CORRECTION : Ce fichier était incorrectement placé dans NkPrefab.h.
// Le profiler est un composant de la couche Core, pas des Prefabs.
//
// Usage :
//   NK_PROFILE_SCOPE("MovementSystem.Execute");
//   NK_PROFILE_SAMPLE("Raycast.Cast");
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    class NkProfiler {
        public:
            enum class MarkerType : uint8 {
                Frame, System, Query, Script, Custom
            };

            struct Marker {
                MarkerType  type      = MarkerType::Custom;
                const char* name      = "";
                uint64      startTime = 0;
                uint64      endTime   = 0;
                uint32      frameIndex = 0;
                uint32      threadId  = 0;
                bool        isValid   = false;
            };

            struct Stats {
                uint32 sampleCount = 0;
                uint64 minTime     = UINT64_MAX;
                uint64 maxTime     = 0;
                uint64 totalTime   = 0;
                [[nodiscard]] uint64 AvgTime() const noexcept {
                    return (sampleCount > 0) ? (totalTime / sampleCount) : 0;
                }
            };

            [[nodiscard]] static NkProfiler& Global() noexcept {
                static NkProfiler instance;
                return instance;
            }

            void Begin(const char* name, MarkerType type = MarkerType::Custom) noexcept;
            void End(const char* name) noexcept;
            void Sample(const char* name, MarkerType type = MarkerType::Custom) noexcept {
                Begin(name, type); End(name);
            }

            void NextFrame() noexcept { ++mCurrentFrame; }
            [[nodiscard]] Stats GetStats(const char* name) const noexcept;
            [[nodiscard]] bool  ExportToJSON(char* buffer, uint32 bufSize) const noexcept;
            void Reset() noexcept;

        private:
            uint32 mCurrentFrame = 0;
            NkUnorderedMap<NkString, Stats> mStats;

            [[nodiscard]] static uint64 GetTimestampNs() noexcept;
            [[nodiscard]] static uint32 GetCurrentThreadId() noexcept;
    };

} // namespace nkentseu

// Macros utilitaires
#define NK_PROFILE_SCOPE(name) \
    ::nkentseu::NkProfiler::Global().Begin((name)); \
    struct _NkProfilerEnd_##__LINE__ { \
        const char* _n; \
        _NkProfilerEnd_##__LINE__(const char* n) : _n(n) {} \
        ~_NkProfilerEnd_##__LINE__() noexcept { ::nkentseu::NkProfiler::Global().End(_n); } \
    } _profilerEnd_##__LINE__ { (name) }

#define NK_PROFILE_SAMPLE(name) \
    ::nkentseu::NkProfiler::Global().Sample((name))
