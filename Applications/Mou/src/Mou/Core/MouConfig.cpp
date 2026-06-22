// =============================================================================
// Core/MouConfig.cpp
// =============================================================================
#include "MouConfig.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include <cstdio>

namespace mou {

    namespace settings {
        nkentseu::float32 musicVolume = 0.7f;
        nkentseu::float32 sfxVolume   = 0.9f;
        bool              muted       = false;

        nkentseu::float32 EffectiveMusic() noexcept { return muted ? 0.f : musicVolume; }
        nkentseu::float32 EffectiveSfx()   noexcept { return muted ? 0.f : sfxVolume; }

        static const char* kPath = "settings.cfg";

        void Save() noexcept {
            char buf[128];
            const int n = std::snprintf(buf, sizeof(buf), "music %.3f\nsfx %.3f\nmuted %d\n",
                                        (double)musicVolume, (double)sfxVolume, muted ? 1 : 0);
            if (n <= 0) return;
            nkentseu::NkFile f;
            if (f.Open(kPath, nkentseu::NkFileMode::NK_WRITE)) {
                f.Write(buf, (nkentseu::usize)n);
                f.Close();
            }
        }

        void Load() noexcept {
            nkentseu::NkFile f;
            if (!f.Open(kPath, nkentseu::NkFileMode::NK_READ)) return;   // pas encore de réglages -> défauts
            char buf[256];
            const nkentseu::usize sz = (nkentseu::usize)f.Size();
            const nkentseu::usize rd = f.Read(buf, sz < sizeof(buf) - 1 ? sz : sizeof(buf) - 1);
            f.Close();
            buf[rd] = 0;
            float m = musicVolume, s = sfxVolume; int mu = muted ? 1 : 0;
            std::sscanf(buf, "music %f sfx %f muted %d", &m, &s, &mu);
            musicVolume = (m < 0.f) ? 0.f : (m > 1.f ? 1.f : m);
            sfxVolume   = (s < 0.f) ? 0.f : (s > 1.f ? 1.f : s);
            muted = (mu != 0);
        }
    }  // namespace settings

    namespace memory {
        nkentseu::memory::NkAllocator* gDefaultAllocator  = nullptr;
        nkentseu::memory::NkAllocator* gResourceAllocator = nullptr;
        nkentseu::memory::NkAllocator* gScratchAllocator  = nullptr;

        void InitializeAllocators() noexcept {
            gDefaultAllocator  = &nkentseu::memory::NkGetDefaultAllocator();
            gResourceAllocator = &nkentseu::memory::NkGetDefaultAllocator();
            gScratchAllocator  = &nkentseu::memory::NkGetDefaultAllocator();
            MOU_LOG_INFO("Allocators Mu initialises");
        }

        void ShutdownAllocators() noexcept {
            gDefaultAllocator  = nullptr;
            gResourceAllocator = nullptr;
            gScratchAllocator  = nullptr;
            MOU_LOG_INFO("Allocators Mu liberes");
        }
    }  // namespace memory

    // Init paresseuse : GetLogger() cree un NkLogger("Mu") par defaut au 1er appel.
    // (Ne PAS initialiser via la macro `logger` ici : elle contient __func__, invalide
    // hors d'une fonction -> -Wpredefined-identifier-outside-function.)
    static nkentseu::NkLogger* gLogger = nullptr;

    nkentseu::NkLogger& GetLogger() noexcept {
        if (!gLogger) {
            static nkentseu::NkLogger defaultLogger("Mu");
            gLogger = &defaultLogger;
        }
        return *gLogger;
    }

    namespace globals {
        nkentseu::NkString gDataPath = "";
    }  // namespace globals

}  // namespace mou
