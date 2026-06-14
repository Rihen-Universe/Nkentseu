#pragma once
// =============================================================================
// Scene.h
// -----------------------------------------------------------------------------
// Interface abstraite d'une scene UI / jeu.
// Cycle de vie :
//   OnEnter(ctx)            : appele quand la scene devient active (push/replace)
//   OnUpdate(ctx, dt)       : appele chaque frame avant OnRender
//   OnRender(ctx)           : appele apres OnUpdate
//   OnExit(ctx)             : appele quand la scene est retiree (pop/replace)
//   OnResize(ctx, w, h)     : appele apres redimensionnement window
//
// Une scene peut demander une transition au manager via :
//   ctx.scenes->Replace(new ChildScene());
//   ctx.scenes->Push(new OverlayScene());
//   ctx.scenes->Pop();
// =============================================================================

#include "AppContext.h"
#include "Pong/UI/Theme.h"   // theme::Dark() pour la couleur de fond par defaut

namespace nkentseu
{
    class NkEvent;
}

namespace nkentseu
{
    namespace pong
    {

        // ─────────────────────────────────────────────────────────────────────
        // Scene — interface abstraite.
        // ─────────────────────────────────────────────────────────────────────
        class Scene
        {
        public:
            virtual ~Scene() = default;

            /// Identifiant lisible pour les logs (optionnel).
            virtual const char* Name() const noexcept { return "Scene"; }

            /// Couleur de fond (clear) de la scene. PongApp::Render efface la cible
            /// avec cette couleur AVANT OnRender. Defaut = theme::Dark() ; une scene
            /// peut l'override (ex. RihenIntro -> blanc). Remplace l'ancien
            /// r.Clear(...) par-scene retire lors de la migration NKCanvas.
            virtual math::NkColor BackgroundColor() const { return theme::Dark(); }

            /// Appele quand la scene devient active.
            virtual void OnEnter(AppContext& /*ctx*/) {}

            /// Avance la logique d'un delta-time.
            virtual void OnUpdate(AppContext& /*ctx*/, float /*dt*/) {}

            /// Dessine la scene (entre BeginFrame / EndFrame du contexte GL).
            virtual void OnRender(AppContext& /*ctx*/) {}

            /// Appele apres redimensionnement de la fenetre.
            virtual void OnResize(AppContext& /*ctx*/, int /*w*/, int /*h*/) {}

            /// Appele pour chaque event UI / input (clavier, souris, touch).
            /// Les events systeme (close, resize, focus) sont consommes par
            /// Apps.cpp avant le forwarding — les scenes ne voient donc que
            /// les events significatifs pour leur logique.
            virtual void OnEvent(AppContext& /*ctx*/, NkEvent& /*ev*/) {}

            /// Appele quand l'app perd le focus (Android Hidden, PC focus
            /// lost). La scene doit suspendre les animations et, pour le
            /// gameplay, mettre en pause automatique.
            virtual void OnPause(AppContext& /*ctx*/) {}

            /// Appele quand l'app revient au foreground.
            virtual void OnResume(AppContext& /*ctx*/) {}

            /// Appele quand la scene revient en haut de stack apres qu'une
            /// scene enfant a ete Pop. Permet de re-armer un grace period
            /// anti auto-trigger : si le user click RETOUR sur la scene
            /// enfant et que le bouton de retour de cette scene est au meme
            /// endroit que sur celle-ci, le release/touch-end fuite et
            /// retrigger un Pop ici aussi (cf bug Pong Options > Tutoriel).
            virtual void OnResumedFromChild(AppContext& /*ctx*/) {}

            /// Appele juste avant destruction.
            virtual void OnExit(AppContext& /*ctx*/) {}
        };

    } // namespace pong
} // namespace nkentseu
