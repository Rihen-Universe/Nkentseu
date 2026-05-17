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

            /// Appele juste avant destruction.
            virtual void OnExit(AppContext& /*ctx*/) {}
        };

    } // namespace pong
} // namespace nkentseu
