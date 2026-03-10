#pragma once

// =============================================================================
// NkApplicationEvent.h
// Hiérarchie des événements liés au cycle de vie de l'application.
//
// Catégorie : NK_CAT_APPLICATION
//
// Classes :
//   NkAppEvent              — base abstraite (catégorie APPLICATION)
//     NkAppLaunchEvent      — démarrage de l'application
//     NkAppTickEvent        — tick logique (horloge de la boucle principale)
//     NkAppUpdateEvent      — mise à jour de la logique applicative
//     NkAppRenderEvent      — demande de rendu d'une frame
//     NkAppCloseEvent       — fermeture demandée (peut être annulée)
// =============================================================================

#include "NkEvent.h"
#include "NKContainers/String/NkStringUtils.h"

namespace nkentseu {

    // =========================================================================
    // NkAppEvent — base abstraite pour tous les événements application
    // =========================================================================

    /**
     * @brief Classe de base pour les événements liés à l'application.
     *
     * Catégorie : NK_CAT_APPLICATION
     */
    class NkAppEvent : public NkEvent {
    public:
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_APPLICATION)

    protected:
        explicit NkAppEvent(NkU64 windowId = 0) noexcept : NkEvent(windowId) {}
    };

    // =========================================================================
    // NkAppLaunchEvent — lancement de l'application
    // =========================================================================

    /**
     * @brief Émis une seule fois au démarrage de l'application, avant la boucle
     *        principale.
     *
     * Transporte les arguments de la ligne de commande (argc/argv).
     * Le tableau argv doit rester valide pendant toute la durée du traitement.
     */
    class NkAppLaunchEvent final : public NkAppEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_LAUNCH)

        /**
         * @param argc  Nombre d'arguments CLI.
         * @param argv  Tableau de chaînes (durée de vie >= événement).
         */
        NkAppLaunchEvent(int argc = 0, const char* const* argv = nullptr) noexcept
            : NkAppEvent()
            , mArgc(argc)
            , mArgv(argv)
        {}

        NkEvent*    Clone()    const override { return new NkAppLaunchEvent(*this); }
        NkString ToString() const override {
            return "AppLaunch(argc=" + string::NkToString(mArgc) + ")";
        }

        int                GetArgc() const noexcept { return mArgc; }
        const char* const* GetArgv() const noexcept { return mArgv; }

        /// @brief Retourne l'argument i, ou nullptr si hors limites
        const char* GetArg(int i) const noexcept {
            return (mArgv && i >= 0 && i < mArgc) ? mArgv[i] : nullptr;
        }

    private:
        int                mArgc = 0;
        const char* const* mArgv = nullptr;
    };

    // =========================================================================
    // NkAppTickEvent — tick logique de la boucle principale
    // =========================================================================

    /**
     * @brief Émis à chaque itération de la boucle principale pour alimenter la
     *        logique temporelle (physique, animations, IA...).
     *
     * mDeltaTime est le temps réel écoulé depuis le tick précédent, en secondes.
     * A 60 fps : mDeltaTime ~= 0.01667 s.
     */
    class NkAppTickEvent final : public NkAppEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_TICK)

        /**
         * @param deltaTime  Temps écoulé depuis le dernier tick [secondes].
         * @param totalTime  Temps cumulé depuis le lancement [secondes].
         */
        explicit NkAppTickEvent(NkF64 deltaTime, NkF64 totalTime = 0.0) noexcept
            : NkAppEvent()
            , mDeltaTime(deltaTime)
            , mTotalTime(totalTime)
        {}

        NkEvent*    Clone()    const override { return new NkAppTickEvent(*this); }
        NkString ToString() const override {
            return "AppTick(dt=" + string::NkToString(mDeltaTime)
                 + "s total=" + string::NkToString(mTotalTime) + "s)";
        }

        NkF64 GetDeltaTime() const noexcept { return mDeltaTime; }
        NkF64 GetTotalTime() const noexcept { return mTotalTime; }

        /// @brief FPS instantané (0 si deltaTime est nul)
        NkF64 GetFps() const noexcept {
            return (mDeltaTime > 0.0) ? (1.0 / mDeltaTime) : 0.0;
        }

    private:
        NkF64 mDeltaTime = 0.0;
        NkF64 mTotalTime = 0.0;
    };

    // =========================================================================
    // NkAppUpdateEvent — mise à jour de la logique applicative
    // =========================================================================

    /**
     * @brief Émis pour déclencher les mises à jour applicatives (état du jeu,
     *        UI, IA, réseau...).
     *
     * Peut être émis à pas variable (mFixedStep = false) ou à pas de temps fixe
     * (mFixedStep = true), ce qui permet une simulation déterministe.
     */
    class NkAppUpdateEvent final : public NkAppEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_UPDATE)

        /**
         * @param deltaTime  Durée du pas de simulation [secondes].
         * @param fixedStep  true si la mise à jour se fait à pas fixe.
         */
        explicit NkAppUpdateEvent(NkF64 deltaTime,
                                   bool  fixedStep = false) noexcept
            : NkAppEvent()
            , mDeltaTime(deltaTime)
            , mFixedStep(fixedStep)
        {}

        NkEvent*    Clone()    const override { return new NkAppUpdateEvent(*this); }
        NkString ToString() const override {
            return NkString("AppUpdate(dt=") + string::NkToString(mDeltaTime)
                 + (mFixedStep ? " fixed" : " variable") + ")";
        }

        NkF64 GetDeltaTime() const noexcept { return mDeltaTime; }
        bool  IsFixedStep()  const noexcept { return mFixedStep; }

    private:
        NkF64 mDeltaTime = 0.0;
        bool  mFixedStep  = false;
    };

    // =========================================================================
    // NkAppRenderEvent — demande de rendu d'une frame
    // =========================================================================

    /**
     * @brief Émis pour déclencher le rendu graphique d'une frame.
     *
     * mAlpha est un facteur d'interpolation [0, 1] entre le dernier état simulé
     * et le prochain, utile pour un rendu fluide en mode pas de temps fixe.
     * A 1.0, l'état rendu correspond exactement au dernier état simulé.
     */
    class NkAppRenderEvent final : public NkAppEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_RENDER)

        /**
         * @param alpha       Facteur d'interpolation [0,1].
         * @param frameIndex  Index de la frame courante depuis le lancement.
         */
        explicit NkAppRenderEvent(NkF64 alpha      = 1.0,
                                   NkU64 frameIndex = 0) noexcept
            : NkAppEvent()
            , mAlpha(alpha)
            , mFrameIndex(frameIndex)
        {}

        NkEvent*    Clone()    const override { return new NkAppRenderEvent(*this); }
        NkString ToString() const override {
            return "AppRender(frame=" + string::NkToString(mFrameIndex)
                 + " alpha=" + string::NkToString(mAlpha) + ")";
        }

        NkF64 GetAlpha()      const noexcept { return mAlpha; }
        NkU64 GetFrameIndex() const noexcept { return mFrameIndex; }

    private:
        NkF64 mAlpha      = 1.0;
        NkU64 mFrameIndex = 0;
    };

    // =========================================================================
    // NkAppCloseEvent — fermeture demandée (peut être annulée)
    // =========================================================================

    /**
     * @brief Émis lorsque l'application reçoit une demande de fermeture.
     *
     * Sources typiques : clic sur la croix de la fenêtre principale, signal OS
     * (SIGTERM, WM_QUIT), ou appel programmatique.
     *
     * Un handler peut annuler la fermeture via Cancel() -- par exemple pour
     * afficher une boîte de dialogue "Enregistrer avant de quitter ?".
     * Si IsForced() est true, l'annulation est ignorée.
     *
     * @code
     *   void OnClose(NkEvent& e) {
     *       if (auto* ce = e.As<NkAppCloseEvent>()) {
     *           if (HasUnsavedChanges()) {
     *               ce->Cancel();       // bloque la fermeture
     *               ShowSaveDialog();
     *           }
     *       }
     *   }
     * @endcode
     */
    class NkAppCloseEvent final : public NkAppEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_CLOSE)

        /**
         * @param forced  true = fermeture imposée par l'OS, non annulable.
         */
        explicit NkAppCloseEvent(bool forced = false) noexcept
            : NkAppEvent()
            , mForced(forced)
            , mCancelled(false)
        {}

        NkEvent*    Clone()    const override { return new NkAppCloseEvent(*this); }
        NkString ToString() const override {
            return NkString("AppClose(") + (mForced ? "forced" : "requested")
                 + (mCancelled ? " cancelled" : "") + ")";
        }

        bool IsForced()    const noexcept { return mForced; }
        bool IsCancelled() const noexcept { return mCancelled; }

        /// @brief Annule la fermeture. Sans effet si IsForced() est true.
        void Cancel() noexcept { if (!mForced) mCancelled = true; }

    private:
        bool mForced    = false;
        bool mCancelled = false;
    };

} // namespace nkentseu