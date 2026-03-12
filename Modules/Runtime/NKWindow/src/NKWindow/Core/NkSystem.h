#pragma once

// =============================================================================
// NkSystem.h
// Point d'entrÃ©e global du framework NkWindow.
//   NkInitialise(data);  <- initialise plateforme + event system
//   NkClose();           <- libÃ¨re tout proprement
//
// NkSystem est le propriÃ©taire unique de NkEventSystem.
// NkEventSystem n'est plus un singleton â€” il vit dans NkSystem.
// NkSystem gÃ¨re le registre des fenÃªtres (assignation des IDs + lookup).
// NkEventSystem ne gÃ¨re PAS la liste des fenÃªtres.
// =============================================================================

#include "NkTypes.h"
#include "NkWindowId.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkGamepadSystem.h"
#include "NkSurface.h"

namespace nkentseu {

    class NkWindow;

    // ---------------------------------------------------------------------------
    // NkAppData
    // ---------------------------------------------------------------------------

    struct NkAppData {
        bool            enableRendererDebug = false;
        bool            enableEventLogging  = false;
        NkString        appName             = "NkApp";
        NkString        appVersion          = "1.0.0";
        bool            enableMultiWindow   = true;
        void*           userData            = nullptr;
    };

    // ---------------------------------------------------------------------------
    // NkSystem â€” cycle de vie global + registre des fenÃªtres
    // ---------------------------------------------------------------------------

    class NkSystem {
        public:
            NkSystem()  = default;
            ~NkSystem() = default;

            NkSystem(const NkSystem&)            = delete;
            NkSystem& operator=(const NkSystem&) = delete;

            static NkSystem& Instance();

            // --- Cycle de vie ---
            // Point 6 : OleInitialize est appelÃ© ici une seule fois pour tout
            // le processus, avant toute crÃ©ation de fenÃªtre ou de DropTarget.
            bool Initialise(const NkAppData& data = {});
            void Close();

            bool             IsInitialised() const { return mInitialised; }
            const NkAppData& GetAppData()    const { return mAppData; }

            // --- AccÃ¨s Ã  NkEventSystem ---
            // NkEventSystem n'est plus un singleton autoproclamÃ©.
            // Tout le code interne et externe passe par ici.
            NkEventSystem&       GetEventSystem()       { return mEventSystem; }
            const NkEventSystem& GetEventSystem() const { return mEventSystem; }

            // Raccourci statique identique Ã  l'ancien NkEvents()
            static NkEventSystem& Events() { return Instance().GetEventSystem(); }

            // --- AccÃ¨s Ã  NkGamepadSystem (CORRECTION 1 â€” plus de singleton sÃ©parÃ©) ---
            NkGamepadSystem&       GetGamepadSystem()       { return mGamepadSystem; }
            const NkGamepadSystem& GetGamepadSystem() const { return mGamepadSystem; }
            static NkGamepadSystem& Gamepads() { return Instance().GetGamepadSystem(); }

            // --- Registre des fenÃªtres ---
            NkWindowId RegisterWindow(NkWindow* win);
            void       UnregisterWindow(NkWindowId id);
            NkWindow*  GetWindow(NkWindowId id) const;
            uint32      GetWindowCount() const { return static_cast<uint32>(mWindows.Size()); }
            NkWindow*  GetWindowAt(uint32 index) const;

        private:
            bool          mInitialised = false;
            NkAppData     mAppData;

            // NkEventSystem possÃ©dÃ© par NkSystem â€” plus de Meyer's singleton sÃ©parÃ©
            NkEventSystem    mEventSystem;

            // NkGamepadSystem possÃ©dÃ© par NkSystem (CORRECTION 1 â€” plus de singleton)
            NkGamepadSystem  mGamepadSystem;

            // Registre des fenÃªtres
            NkUnorderedMap<NkWindowId, NkWindow*> mWindows;
            NkWindowId                                 mNextWindowId = 1;

        #if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
            // Point 6 : flag indiquant si OleInitialize a Ã©tÃ© appelÃ© par NkSystem
            bool mOleInitialised = false;
        #endif
    };

    // ---------------------------------------------------------------------------
    // Fonctions globales de commoditÃ©
    // ---------------------------------------------------------------------------

    inline bool NkInitialise(const NkAppData& data = {}) {
        return NkSystem::Instance().Initialise(data);
    }

    inline void NkClose() {
        NkSystem::Instance().Close();
    }

    // ConservÃ© pour compatibilitÃ© â€” dÃ©lÃ¨gue Ã  NkSystem::Events()
    inline NkEventSystem& NkEvents() {
        return NkSystem::Events();
    }

    // CORRECTION 1 : NkGamepads() dÃ©lÃ¨gue Ã  NkSystem::Gamepads() (plus de singleton)
    inline NkGamepadSystem& NkGamepads() {
        return NkSystem::Gamepads();
    }

} // namespace nkentseu

