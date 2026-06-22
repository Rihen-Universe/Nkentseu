#pragma once
// =============================================================================
// NogeIntroScene.h — Intro Noge/Rihen Universe (2s, texte centré animé)
// Transition vers MainMenuScene
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu { namespace songoo {

    class NogeIntroScene : public Scene {
    public:
        const char* Name() const noexcept override { return "NogeIntro"; }
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
    private:
        float mTime = 0.f;
        static constexpr float kDuration = 2.0f;
    };

}} // namespace nkentseu::songoo
