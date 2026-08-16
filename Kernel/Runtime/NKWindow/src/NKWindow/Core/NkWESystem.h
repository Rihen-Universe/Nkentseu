#pragma once

// =============================================================================
// NkWESystem.h
// Point d'entrée global du framework NkWindow.
//   NkInitialise(data);  <- initialise plateforme + event system
//   NkClose();           <- libère tout proprement
//
// NkWESystem est le propriétaire unique de NkEventSystem.
// NkEventSystem n'est plus un singleton — il vit dans NkWESystem.
// NkWESystem gère le registre des fenêtres (assignation des IDs + lookup).
// NkEventSystem ne gère PAS la liste des fenêtres.
// =============================================================================

#include "NkTypes.h"
#include "NKEvent/NkWindowId.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkGamepadSystem.h"
#include "NkSurface.h"

namespace nkentseu {

    class NkWindow;

    // ---------------------------------------------------------------------------
    // NkAppData
    // ---------------------------------------------------------------------------

    struct NkAppData {
            bool enableRendererDebug = false;
            bool enableEventLogging = false;
            NkString appName = "NkApp";
            NkString appVersion = "1.0.0";
            bool enableMultiWindow = true;
            // Minuterie FINE (1 ms) demandée au démarrage, rendue à l'arrêt.
            //
            // ACTIVE PAR DÉFAUT, et c'est le sens de la décision : sans elle,
            // tout `Sleep` de 1 à 12 ms dure ~15,5 ms, et une boucle cadencée au
            // sommeil tourne à 40 img/s là où elle en vise 60 — mesuré le
            // 2026-08-15. Jusque-là l'appel n'existait qu'à UN endroit du moteur
            // (`NkRendererImpl`) : toute application qui n'initialise pas le
            // renderer héritait du défaut SANS QUE RIEN NE LE LUI DISE.
            //
            // REFUSABLE, et ce n'est pas une précaution de style : une minuterie
            // fine paie des réveils, donc de l'énergie — sur portable et sur
            // mobile ce n'est pas neutre, et toutes les applications n'ont pas
            // besoin d'un sommeil précis. **Une décision par défaut qu'on ne
            // peut pas refuser est une décision irréversible déguisée.**
            //
            // Mesures et portée de version : `NkChrono::BeginPreciseTiming`.
            bool enablePreciseTiming = true;
            void *userData = nullptr;
    };

    // ---------------------------------------------------------------------------
    // NkWESystem — cycle de vie global + registre des fenêtres
    // ---------------------------------------------------------------------------

    class NkWESystem {
        public:
            NkWESystem() = default;
            ~NkWESystem() = default;

            NkWESystem(const NkWESystem &) = delete;
            NkWESystem &operator=(const NkWESystem &) = delete;

            static NkWESystem &Instance();

            // --- Cycle de vie ---
            // Point 6 : OleInitialize est appelé ici une seule fois pour tout
            // le processus, avant toute création de fenêtre ou de DropTarget.
            bool Initialise(const NkAppData &data = {});
            void Close();

            bool IsInitialised() const {
                return mInitialised;
            }

            const NkAppData &GetAppData() const {
                return mAppData;
            }

            // --- Accès à NkEventSystem ---
            // NkEventSystem n'est plus un singleton autoproclamé.
            // Tout le code interne et externe passe par ici.
            NkEventSystem &GetEventSystem() {
                return mEventSystem;
            }

            const NkEventSystem &GetEventSystem() const {
                return mEventSystem;
            }

            // Raccourci statique identique à l'ancien NkEvents()
            static NkEventSystem &Events() {
                return Instance().GetEventSystem();
            }

            // --- Accès à NkGamepadSystem (CORRECTION 1 — plus de singleton séparé) ---
            NkGamepadSystem &GetGamepadSystem() {
                return mGamepadSystem;
            }

            const NkGamepadSystem &GetGamepadSystem() const {
                return mGamepadSystem;
            }

            static NkGamepadSystem &Gamepads() {
                return Instance().GetGamepadSystem();
            }

            // --- Registre des fenêtres ---
            NkWindowId RegisterWindow(NkWindow *win);
            void UnregisterWindow(NkWindowId id);
            NkWindow *GetWindow(NkWindowId id) const;

            uint32 GetWindowCount() const {
                return static_cast<uint32>(mWindows.Size());
            }

            NkWindow *GetWindowAt(uint32 index) const;

        private:
            bool mInitialised = false;
            NkAppData mAppData;

            // NkEventSystem possédé par NkWESystem — plus de Meyer's singleton séparé
            NkEventSystem mEventSystem;

            // NkGamepadSystem possédé par NkWESystem (CORRECTION 1 — plus de singleton)
            NkGamepadSystem mGamepadSystem;

            // Registre des fenêtres
            NkUnorderedMap<NkWindowId, NkWindow *> mWindows;
            NkWindowId mNextWindowId = 1;

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
            // Point 6 : flag indiquant si OleInitialize a été appelé par NkWESystem
            bool mOleInitialised = false;
#endif
    };

    // ---------------------------------------------------------------------------
    // Fonctions globales de commodité
    // ---------------------------------------------------------------------------

    inline bool NkInitialise(const NkAppData &data = {}) {
        return NkWESystem::Instance().Initialise(data);
    }

    inline void NkClose() {
        NkWESystem::Instance().Close();
    }

    // Conservé pour compatibilité — délègue à NkWESystem::Events()
    inline NkEventSystem &NkEvents() {
        return NkWESystem::Events();
    }

    // CORRECTION 1 : NkGamepads() délègue à NkWESystem::Gamepads() (plus de singleton)
    inline NkGamepadSystem &NkGamepads() {
        return NkWESystem::Gamepads();
    }

} // namespace nkentseu
