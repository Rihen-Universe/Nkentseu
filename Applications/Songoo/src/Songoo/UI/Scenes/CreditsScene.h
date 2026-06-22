#pragma once
// =============================================================================
// CreditsScene.h — Crédits Songo'o (défilement vertical + bandes kente)
// Logique IDENTIQUE à l'original (même textes, même vitesse de scroll)
// =============================================================================

#include "Songoo/UI/Scene.h"
namespace nkentseu { namespace songoo {
    class CreditsScene : public Scene {
    public:
        const char* Name() const noexcept override { return "Credits"; }
        void OnEnter (AppContext& ctx) override;
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
        void OnEvent (AppContext& ctx, NkEvent& ev) override;
        void OnExit  (AppContext& ctx) override;
    private:
        float mScrollY = 0.f;
        float mTime    = 0.f;
    };
}} // namespace nkentseu::songoo
